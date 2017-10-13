#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <iostream>
#include <signal.h>

#include "socket_lib.hpp"
#include "client_lib.hpp"
#include "threadpool.hpp"

#define PIPE_READ 0
#define PIPE_WRITE 1

constexpr int NUM = 16;
constexpr int ENUM = NUM * 2 + 1;
constexpr int MAX_FN_BUFFER_SIZE = 256; // FN: filename

void closeClient(cfd_t cfd, ClientMap* clients) {
  close(cfd);
  clients->erase(cfd);
}


void add_epoll_read_event(int efd, int fd, struct epoll_event *ep_event) {
  ep_event->data.fd = fd;
  ep_event->events = EPOLLIN | EPOLLET;
  int s = epoll_ctl(efd, EPOLL_CTL_ADD, fd, ep_event);
  if (s == -1) {
    perror("epoll_ctl");
    exit(EXIT_FAILURE);
  }
}


int main (int argc, char *argv[]) {
  int sfd, s;
  int efd;
  ClientMap clients;
  char fn_buffer[MAX_FN_BUFFER_SIZE];
  int pipes[NUM][2];

  if (argc != 3) {
    fprintf(stderr, "Usage: %s [ip] [port]\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // handle SIGPIPE from send
  // if the socket has been closed by either side, the process calling send()
  // will get the signal SIGPIPE. (Unless send() was called with the
  // MSG_NOSIGNAL flag.)
  struct sigaction new_action;
  new_action.sa_handler = SIG_IGN;
  sigemptyset(&new_action.sa_mask);
  new_action.sa_flags = 0;
  sigaction(SIGPIPE, &new_action, NULL);

  sfd = create_and_bind(argv[1], argv[2]);
  if (sfd == -1) {
    exit(EXIT_FAILURE);
  }

  s = make_socket_non_blocking(sfd);
  if (s == -1) {
    exit(EXIT_FAILURE);
  }

  s = listen(sfd, SOMAXCONN);
  if (s == -1) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  efd = epoll_create1(0);
  if (efd == -1) {
    perror("epoll_create");
    exit(EXIT_FAILURE);
  }

  struct epoll_event socket_event;
  add_epoll_read_event(efd, sfd, &socket_event);


  // create NUM pipes and NUM threads
  // add in pipes in monitoring events
  ThreadPool<NUM> thread_pool;
  auto input_pipes = thread_pool.inputPipes();

  for (int i = 0; i < NUM; i++) {
    struct epoll_event pipe_read_event;
    add_epoll_read_event(efd, input_pipes[i], &pipe_read_event);
  }

  /* Buffer where events are returned */
  struct epoll_event *events = (epoll_event*) calloc(ENUM, sizeof socket_event);

  /* The event loop */
  while (1) {
    int n = epoll_wait(efd, events,ENUM, -1);

    for (int i = 0; i < n; i++) {
      int fd = events[i].data.fd;

      if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
        /* An error has occured on this fd, or the socket is not
           ready for reading (why were we notified then?) */
        fprintf(stderr, "epoll error\n");

        // TODO: better failure tolerance. e.g., I don't want to close a pipe
        close(events[i].data.fd);
        continue;
      } 
      
      if (sfd == fd) {
        /* We have a notification on the listening socket, which
           means one or more incoming connections. */
        // and add to events
        accept_clients(sfd, &clients, efd);
        continue;
      } 
      
      // test if fd is one of the clients
      auto iter = clients.find(fd);
      if (iter != clients.end() && (events[i].events & EPOLLIN)) {
        // read filename
        cfd_t cfd = fd;
        int fn_len = readFilename(cfd, fn_buffer, sizeof(fn_buffer));
        if (fn_len == 0) {
          // remote has closed
          closeClient(cfd, &clients);
          continue;
        }

        printf("client %d's filename is %s\n", cfd, fn_buffer);

        ClientState& client = iter->second;
        auto job = [cfd, fn_buffer, &client](pfd_t pipe_out) {
          client.size  = readFile(fn_buffer, client.buffer);
          cfd_t cfd_to_write = cfd;
          ssize_t count = write(pipe_out, reinterpret_cast<void*>(&cfd_to_write), sizeof(cfd));
          if (count <= 0) {
            perror("error writing to pipe\n");
            close(pipe_out);
            close(cfd);
          }
        };
        thread_pool.addTask(job);
        // TODO: use one of the threads in the thread pool to read from file
        // and save the data into iter->second.buffer and write cfd into pipe
        continue;
      }

      if (iter == clients.end()) {
        // fd is from pipe and and some thread has finished reading file content
        cfd_t cfd;
        int count = read(fd /*pipe fd*/, reinterpret_cast<void*>(&cfd), sizeof(cfd));
        auto citer = clients.find(cfd);
        assert(citer != clients.end());
        struct epoll_event event;
        event.data.fd = cfd;
        event.events = EPOLLOUT;  // does LT/ET really matter in sending mode?
        s = epoll_ctl(efd, EPOLL_CTL_MOD, cfd, &event);
        if (s == -1) {
          perror("epoll_ctl for write");
          closeClient(cfd, &clients);
        }
        continue;
      }

      if (iter != clients.end() && events[i].events & EPOLLOUT) {
        // ready to send
        cfd_t cfd = fd;
        bool success = sendData(cfd, &(iter->second));
        if (success /* finished sending */ || 
            errno != EAGAIN && errno != EWOULDBLOCK /* real error occurs */) {
          closeClient(cfd, &clients);
        }
        continue;
      }
    }
  }

  free(events);
  close(sfd);

  return EXIT_SUCCESS;
}

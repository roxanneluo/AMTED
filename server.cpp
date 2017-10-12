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

#include "socket_lib.hpp"
#include "client_lib.hpp"

constexpr int NUM = 16;
constexpr int ENUM = NUM+1;
constexpr int MAX_FN_BUFFER_SIZE = 256; // FN: filename

void closeClient(cfd_t cfd, ClientMap* clients) {
  close(cfd);
  clients->erase(cfd);
}

int main (int argc, char *argv[])
{
  int sfd, s;
  int efd;
  ClientMap clients;
  char fn_buffer[MAX_FN_BUFFER_SIZE];

  if (argc != 3)
    {
      fprintf (stderr, "Usage: %s [ip] [port]\n", argv[0]);
      exit (EXIT_FAILURE);
    }

  sfd = create_and_bind (argv[1], argv[2]);
  if (sfd == -1)
    abort ();

  s = make_socket_non_blocking (sfd);
  if (s == -1)
    abort ();

  s = listen (sfd, SOMAXCONN);
  if (s == -1)
    {
      perror ("listen");
      abort ();
    }

  efd = epoll_create1 (0);
  if (efd == -1)
    {
      perror ("epoll_create");
      abort ();
    }

  struct epoll_event socket_event;
  socket_event.data.fd = sfd;
  socket_event.events = EPOLLIN | EPOLLET;
  s = epoll_ctl (efd, EPOLL_CTL_ADD, sfd, &socket_event);
  if (s == -1)
    {
      perror ("epoll_ctl");
      abort ();
    }

  // TODO
  // create NUM pipes and NUM threads
  // add in pipes in monitoring events

  /* Buffer where events are returned */
  struct epoll_event *events = (epoll_event*)calloc (ENUM, sizeof socket_event);

  /* The event loop */
  while (1) {
    int n = epoll_wait (efd, events,ENUM, -1);
    for (int i = 0; i < n; i++) {
      int fd = events[i].data.fd;
      if ((events[i].events & EPOLLERR) || 
          (events[i].events & EPOLLHUP)) {
          /* An error has occured on this fd, or the socket is not
             ready for reading (why were we notified then?) */
          fprintf (stderr, "epoll error\n");
        // TODO: better failure tolerance. e.g., I don't want to close a pipe
          close (events[i].data.fd);
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
      if (iter != clients.end() && events[i].events & EPOLLIN) {
        // read filename
        cfd_t cfd = fd;
        int fn_len = readFilename(cfd, fn_buffer, sizeof(fn_buffer));
        if (fn_len == 0) {
          // remote has closed
          closeClient(cfd, &clients);
          continue;
        }

        printf("client %d's filename is %s\n", cfd, fn_buffer);

        // TODO: use one of the threads in the thread pool to read from file
        // and save the data into iter->second.buffer and write cfd into pipe
        continue;
      }

      if (iter == clients.end()) {
        // fd is from pipe and and some thread has finished reading file content
        cfd_t cfd;
        int count = read(fd /*pipe fd*/, reinterpret_cast<char*>(&cfd), sizeof(cfd));
        auto citer = clients.find(cfd);
        assert(citer != clients.end());
        struct epoll_event event;
        event.data.fd = cfd;
        event.events = EPOLLOUT;  // does LT/ET really matter in sending mode?
        s = epoll_ctl (efd, EPOLL_CTL_MOD, cfd, &event);
        if (s == -1) {
          perror("epoll_ctl for write");
          closeClient(cfd, &clients);
        }
        continue;
      }

      // TODO: handle SIGPIPE from send
      // if the socket has been closed by either side, the process calling send()
      // will get the signal SIGPIPE. (Unless send() was called with the
      // MSG_NOSIGNAL flag.)
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

  free (events);

  close (sfd);

  return EXIT_SUCCESS;
}

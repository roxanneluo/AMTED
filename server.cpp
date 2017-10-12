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
#include "socket_lib.hpp"
#include "client_lib.hpp"

constexpr int NUM = 16;
constexpr int ENUM = NUM+1;

int main (int argc, char *argv[])
{
  int sfd, s;
  int efd;
  struct epoll_event event;
  struct epoll_event *events;

  ClientMap clients;

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

  event.data.fd = sfd;
  event.events = EPOLLIN | EPOLLET;
  s = epoll_ctl (efd, EPOLL_CTL_ADD, sfd, &event);
  if (s == -1)
    {
      perror ("epoll_ctl");
      abort ();
    }

  // TODO
  // create NUM pipes and NUM threads
  // add in pipes in monitoring events

  /* Buffer where events are returned */
  events = (epoll_event*)calloc (ENUM, sizeof event);

  /* The event loop */
  while (1) {
    int n, i;

    n = epoll_wait (efd, events,ENUM, -1);
    for (i = 0; i < n; i++) {
      if ((events[i].events & EPOLLERR) || 
          (events[i].events & EPOLLHUP)) {
                /* An error has occured on this fd, or the socket is not
                   ready for reading (why were we notified then?) */
          fprintf (stderr, "epoll error\n");
          close (events[i].data.fd);
          continue;
      } else if (sfd == events[i].data.fd) {
        /* We have a notification on the listening socket, which
           means one or more incoming connections. */
        // and add to events
        accept_clients(sfd, &clients, efd);
        continue;
      } else {
        /* We have data on the fd waiting to be read. Read and
           display it. We must read whatever data is available
           completely, as we are running in edge-triggered mode
           and won't get a notification again for the same
           data. */
        int done = 0;

        while (1)
          {
            ssize_t count;
            char buf[512];

            count = read (events[i].data.fd, buf, sizeof buf);
            if (count == -1)
              {
                /* If errno == EAGAIN, that means we have read all
                   data. So go back to the main loop. */
                if (errno != EAGAIN)
                  {
                    perror ("read");
                    done = 1;
                  }
                break;
              }
            else if (count == 0)
              {
                /* End of file. The remote has closed the
                   connection. */
                done = 1;
                break;
              }

            /* Write the buffer to standard output */
            s = write (1, buf, count);
            if (s == -1)
              {
                perror ("write");
                abort ();
              }
          }

        if (done)
          {
            printf ("Closed connection on descriptor %d\n",
                    events[i].data.fd);

            /* Closing the descriptor will make epoll remove it
               from the set of descriptors which are monitored. */
            close (events[i].data.fd);
          }
      }
    }
  }

  free (events);

  close (sfd);

  return EXIT_SUCCESS;
}

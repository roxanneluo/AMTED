#ifndef __SOCKET_LIB__
#define __SOCKET_LIB__

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

#include "client_lib.hpp"

using sfd_t = int; // socket file descriptor
using efd_t = int;

int make_socket_non_blocking (int sfd)
{
  int flags, s;

  flags = fcntl (sfd, F_GETFL, 0);
  if (flags == -1)
    {
      perror ("fcntl");
      return -1;
    }

  flags |= O_NONBLOCK;
  s = fcntl (sfd, F_SETFL, flags);
  if (s == -1)
    {
      perror ("fcntl");
      return -1;
    }

  return 0;
}

int create_and_bind (const char* ip, const char *port)
{
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int s, sfd;

  memset (&hints, 0, sizeof (struct addrinfo));
  hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
  hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
  //hints.ai_flags = AI_PASSIVE;     /* All interfaces */

  s = getaddrinfo (ip, port, &hints, &result);
  if (s != 0)
    {
      fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
      return -1;
    }

  for (rp = result; rp != NULL; rp = rp->ai_next)
    {
      sfd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (sfd == -1)
        continue;

      s = bind (sfd, rp->ai_addr, rp->ai_addrlen);
      if (s == 0)
        {
          /* We managed to bind successfully! */
          break;
        }

      close (sfd);
    }

  if (rp == NULL)
    {
      fprintf (stderr, "Could not bind\n");
      return -1;
    }

  freeaddrinfo (result);

  return sfd;
}

bool accept_clients(sfd_t sfd, ClientMap* clients, efd_t efd) {

  while (1)
  {
    struct sockaddr in_addr;
    socklen_t in_len;
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    int s;

    in_len = sizeof in_addr;
    cfd_t cfd = accept (sfd, &in_addr, &in_len);
    if (cfd == -1)
      {
        if ((errno == EAGAIN) ||
            (errno == EWOULDBLOCK))
          {
            /* We have processed all incoming
               connections. */
            return true;
          }
        else
          {
            perror ("accept");
            return false;
          }
      }

    s = getnameinfo (&in_addr, in_len,
                     hbuf, sizeof hbuf,
                     sbuf, sizeof sbuf,
                     NI_NUMERICHOST | NI_NUMERICSERV);
    if (s == 0)
    {
      printf("Accepted connection on descriptor %d "
             "(host=%s, port=%s)\n", cfd, hbuf, sbuf);
    }

    /* Make the incoming socket non-blocking and add it to the
       list of fds to monitor. */
    s = make_socket_non_blocking (cfd);
    if (s == -1)
      abort (); //TODO: handle failure better?

    // add to clients
    (*clients)[cfd] = ClientState();

    struct epoll_event event;
    event.data.fd = cfd;
    event.events = EPOLLIN | EPOLLET;
    s = epoll_ctl (efd, EPOLL_CTL_ADD, cfd, &event);
    if (s == -1)
      {
        perror ("epoll_ctl");
        abort ();
      }
  }
}


#endif


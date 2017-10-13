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
#include <cassert>

#include "client_lib.hpp"

using sfd_t = int; // socket file descriptor
using efd_t = int;
using cfd_t = int;

int make_socket_non_blocking(int sfd) {
  int flags, status;
  flags = fcntl(sfd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl");
    return -1;
  }
  flags |= O_NONBLOCK;
  status = fcntl(sfd, F_SETFL, flags);
  if (status == -1) {
    perror("fcntl");
    return -1;
  }
  return 0;
}


int create_and_bind(const char* ip, const char *port) {
  struct addrinfo hints;
  struct addrinfo *result, *p;
  int ret;
  int sfd;  // Server socket file descriptor

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;      // IPv4 and IPv6
  hints.ai_socktype = SOCK_STREAM;  // TCP socket

  ret = getaddrinfo(ip, port, &hints, &result);
  if (ret != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
    return -1;
  }

  for (p = result; p != NULL; p = p->ai_next) {
    sfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sfd == -1) {
      continue;
    }
    ret = bind(sfd, p->ai_addr, p->ai_addrlen);
    if (ret == 0) {
      break;
    }
  }
  if (p == NULL) {
    fprintf(stderr, "Could not bind\n");
    return -1;
  }

  freeaddrinfo(result);
  return sfd;
}


bool accept_clients(sfd_t sfd, ClientMap* clients, efd_t efd) {
  struct sockaddr in_addr;
  socklen_t in_len;
  char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
  int status;

  while (1) {
    in_len = sizeof in_addr;
    cfd_t cfd = accept(sfd, &in_addr, &in_len);

    if (cfd == -1) {
      if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
        // We have processed all incoming connections.
        return true;
      } else {
        perror ("accept");
        return false;
      }
    }

    status = getnameinfo (&in_addr, in_len,
        hbuf, sizeof hbuf,
        sbuf, sizeof sbuf,
        NI_NUMERICHOST | NI_NUMERICSERV);
    if (status == 0) {
      printf("Accepted connection\n");
    }

    // Make the incoming socket non-blocking and add it to the
    // list of fds to monitor.

    status = make_socket_non_blocking(cfd);
    if (status == -1) {
      exit(EXIT_FAILURE);
    }

    // add to clients
    (*clients)[cfd] = ClientState();

    struct epoll_event event;
    event.data.fd = cfd;
    event.events = EPOLLIN | EPOLLET;
    ssize_t s = epoll_ctl(efd, EPOLL_CTL_ADD, cfd, &event);
    if (s == -1) {
      perror("epoll_ctl");
      exit(EXIT_FAILURE);
    }
  }
}


int readFilename(cfd_t cfd, char* buffer, int max_size) {
  int len = 0;
  while (1) {
    ssize_t count = recv(cfd, buffer + len, sizeof max_size, 0);

    if (count == 0) {
      /* End of file. The remote has closed the
         connection. */
      return 0;
    }

    if (count == -1) {
      /* If errno == EAGAIN, that means we have read all
         data. So go back to the main loop. */
      if (errno != EAGAIN)
      {
        perror ("read filename");
        return 0;
      }
      return len;
    }
    len += count;
  }
}


// return succeeded in writing out all the data or not
bool sendData(cfd_t cfd, ClientState* client) {
  while (1) {
    ssize_t count = send(cfd, client->buffer + client->write_offset,
        client->size - client->write_offset, 0);
    printf("sending %d bytes\n", count);
    if (count == -1 && errno != EAGAIN && errno != EWOULDBLOCK) return false;

    client->write_offset += count;
    if (count == -1) return false;
    if (client->write_offset >= client->size)
      return true;
  }
}

size_t readFile(const char* filename, char* buffer) {
  printf("reading file\n");
  size_t fileSize = 0;
  if (FILE *fp = fopen(filename, "r")) {
    size_t len;
    while (1) {
      len = fread(buffer + fileSize, 1, sizeof(buffer), fp);
      if (len > 0) {
        fileSize += len;
      } else {
        break;
      }
    }
    fclose(fp);
    printf("file content: %s\n", buffer);
    return fileSize;
  }

  perror("fopen");
  return -1;
}

#endif


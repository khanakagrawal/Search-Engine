/*
 * Copyright ©2024 Hannah C. Tang.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Autumn Quarter 2024 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <stdio.h>       // for snprintf()
#include <unistd.h>      // for close(), fcntl()
#include <sys/types.h>   // for socket(), getaddrinfo(), etc.
#include <sys/socket.h>  // for socket(), getaddrinfo(), etc.
#include <arpa/inet.h>   // for inet_ntop()
#include <netdb.h>       // for getaddrinfo()
#include <errno.h>       // for errno, used by strerror()
#include <string.h>      // for memset, strerror()
#include <iostream>      // for std::cerr, etc.

#include "./ServerSocket.h"

extern "C" {
  #include "libhw1/CSE333.h"
}

using std::string;

static const int kHNameLen = 1024;

namespace hw4 {

ServerSocket::ServerSocket(uint16_t port) {
  port_ = port;
  listen_sock_fd_ = -1;
}

ServerSocket::~ServerSocket() {
  // Close the listening socket if it's not zero.  The rest of this
  // class will make sure to zero out the socket if it is closed
  // elsewhere.
  if (listen_sock_fd_ != -1)
    close(listen_sock_fd_);
  listen_sock_fd_ = -1;
}

bool ServerSocket::BindAndListen(int ai_family, int *const listen_fd) {
  // Use "getaddrinfo," "socket," "bind," and "listen" to
  // create a listening socket on port port_.  Return the
  // listening socket through the output parameter "listen_fd"
  // and set the ServerSocket data member "listen_sock_fd_"

  // STEP 1:
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = ai_family;
  hints.ai_socktype = SOCK_STREAM;  // stream
  hints.ai_flags = AI_PASSIVE;      // use wildcard "in6addr_any" address
  hints.ai_flags |= AI_V4MAPPED;    // use v4-mapped v6 if no v6 found
  hints.ai_protocol = IPPROTO_TCP;  // tcp protocol
  hints.ai_canonname = nullptr;
  hints.ai_addr = nullptr;
  hints.ai_next = nullptr;

  // Use argv[1] as the string representation of our portnumber to
  // pass in to getaddrinfo().  getaddrinfo() returns a list of
  // address structures via the output parameter "result".
  struct addrinfo *result;
  std::string port = std::to_string(port_);

  int res = getaddrinfo(nullptr, port.c_str(), &hints, &result);

  // Did addrinfo() fail?
  if (res != 0) {
    std::cerr << "getaddrinfo() failed: ";
    std::cerr << gai_strerror(res) << std::endl;
    return false;
  }

  // Loop through the returned address structures until we are able
  // to create a socket and bind to one.  The address structures are
  // linked in a list through the "ai_next" field of result.
  for (struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
    listen_sock_fd_ = socket(rp->ai_family,
                       rp->ai_socktype,
                       rp->ai_protocol);
    if (listen_sock_fd_ == -1) {
      // Creating this socket failed.  So, loop to the next returned
      // result and try again.
      std::cerr << "socket() failed: " << strerror(errno) << std::endl;
      listen_sock_fd_ = -1;
      continue;
    }

    // Configure the socket; we're setting a socket "option."  In
    // particular, we set "SO_REUSEADDR", which tells the TCP stack
    // so make the port we bind to available again as soon as we
    // exit, rather than waiting for a few tens of seconds to recycle it.
    int optval = 1;
    setsockopt(listen_sock_fd_, SOL_SOCKET, SO_REUSEADDR,
               &optval, sizeof(optval));

    // Try binding the socket to the address and port number returned
    // by getaddrinfo().
    if (bind(listen_sock_fd_, rp->ai_addr, rp->ai_addrlen) == 0) {
      // Bind worked!
      *listen_fd = listen_sock_fd_;
      sock_family_ = rp->ai_family;
      break;
    }

    // The bind failed.  Close the socket, then loop back around and
    // try the next address/port returned by getaddrinfo().
    close(listen_sock_fd_);
    listen_sock_fd_ = -1;
  }

  // Free the structure returned by getaddrinfo().
  freeaddrinfo(result);

  // If we failed to bind, return failure.
  if (listen_sock_fd_ == -1)
    return false;

  // Success. Tell the OS that we want this to be a listening socket.
  if (listen(listen_sock_fd_, SOMAXCONN) != 0) {
    std::cerr << "Failed to mark socket as listening: ";
    std::cerr << strerror(errno) << std::endl;
    close(listen_sock_fd_);
    return false;
  }

  return true;
}

bool ServerSocket::Accept(int *const accepted_fd,
                          string *const client_addr,
                          uint16_t *const client_port,
                          string *const client_dns_name,
                          string *const server_addr,
                          string *const server_dns_name) const {
  // Accept a new connection on the listening socket listen_sock_fd_.
  // (Block until a new connection arrives.)  Return the newly accepted
  // socket, as well as information about both ends of the new connection,
  // through the various output parameters.

  // STEP 2:
  struct sockaddr_storage caddr;
  socklen_t caddr_len = sizeof(caddr);
  struct sockaddr *c_address = reinterpret_cast<struct sockaddr *>(&caddr);
  int client_fd = -1;
  while (1) {
    client_fd = accept(listen_sock_fd_,
                       c_address,
                       &caddr_len);
    if (client_fd < 0) {
      if ((errno == EINTR) || (errno == EAGAIN)) {
        continue;
      }
      std::cerr << "Failure on accept: " << strerror(errno) << std::endl;
      return false;
    } else {
      break;
    }
  }

  // start putting in return params
  *accepted_fd = client_fd;

  // get client address and port num based on which type of address
  if (c_address->sa_family == AF_INET) {
    // it's an IPv4 address
    struct sockaddr_in *ipv4_addr = reinterpret_cast<struct sockaddr_in *>(
        c_address);

    char a[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ipv4_addr->sin_addr), a, INET_ADDRSTRLEN);

    *client_addr = std::string(a);
    *client_port = htons(ipv4_addr->sin_port);
  } else {
    // it's an IPv6 address
    struct sockaddr_in6 *in6 = reinterpret_cast<struct sockaddr_in6 *>(
      c_address);

    char a[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &(in6->sin6_addr), a, INET6_ADDRSTRLEN);

    *client_addr = std::string(a);
    *client_port = htons(in6->sin6_port);
  }

  // DNS lookup to get client's DNS name
  char hname[kHNameLen];
  hname[0] = '\0';
  Verify333(getnameinfo(c_address, caddr_len, hname, kHNameLen, nullptr, 0, 0)
             == 0);
  *client_dns_name = std::string(hname);

  if (sock_family_ == AF_INET) {
    // The server is using an IPv4 address.
    struct sockaddr_in srvr;
    socklen_t srvrlen = sizeof(srvr);
    char addrbuf[INET_ADDRSTRLEN];
    getsockname(client_fd,
                reinterpret_cast<struct sockaddr *>(&srvr),
                &srvrlen);
    inet_ntop(AF_INET, &srvr.sin_addr, addrbuf, INET_ADDRSTRLEN);
    *server_addr = std::string(addrbuf);

    // Get the server's dns name
    getnameinfo(reinterpret_cast<const struct sockaddr *>(&srvr),
                srvrlen, hname, kHNameLen, nullptr, 0, 0);
    *server_dns_name = std::string(hname);
  } else {
    // The server is using an IPv6 address.
    struct sockaddr_in6 srvr;
    socklen_t srvrlen = sizeof(srvr);
    char addrbuf[INET6_ADDRSTRLEN];
    getsockname(client_fd,
                reinterpret_cast<struct sockaddr *>(&srvr),
                &srvrlen);
    inet_ntop(AF_INET6, &srvr.sin6_addr, addrbuf, INET6_ADDRSTRLEN);
    *server_addr = std::string(addrbuf);

    // Get the server's dns name
    getnameinfo(reinterpret_cast<const struct sockaddr *>(&srvr),
                srvrlen, hname, kHNameLen, nullptr, 0, 0);
    *server_dns_name = std::string(hname);
  }

  return true;
}

}  // namespace hw4

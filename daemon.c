#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "packet.h"
#include "socket.h"

#define BACKLOG 3

ssize_t start(int port) {
  int server_fd, new_socket;
  struct sockaddr_in address;
  int addrlen = sizeof(address);
  zynq_packet_t packet;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket");
    goto fail;
  }

  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt))) {
    perror("setsockopt");
    goto fail;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind");
    goto fail;
  }

  if (listen(server_fd, BACKLOG) < 0) {
    perror("listen");
    goto fail;
  }

  // client accept loop
  while (true) {
    memset(&packet, 0, sizeof(packet));
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                             (socklen_t *)&addrlen)) < 0) {
      perror("accept");
      continue;
    }
    char addr_str[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &address.sin_addr, addr_str, INET_ADDRSTRLEN)) {
      perror("inet_ntop");
      goto client_fail;
    }
    printf("accepted new client from %s:%d\n", addr_str,
           ntohs(address.sin_port));
    // client process loop
    while (true) {
      SOCK_READ(new_socket, &packet, sizeof(packet), client_fail)
      switch (packet.type) {
      case READ: {
        printf("READ %p %ld\n", (void *)packet.rw.addr, packet.rw.len);
        /* replace this with a pointer from mmap-ed /dev/mem */
        uint8_t *buf = malloc(packet.rw.len);
        memset(buf, 0, packet.rw.len);
        SOCK_WRITE(new_socket, buf, packet.rw.len, client_fail)
        free(buf);
        break;
      }
      case WRITE: {
        printf("WRITE %p %ld\n", (void *)packet.rw.addr, packet.rw.len);
        /* replace this with a pointer from mmap-ed /dev/mem */
        uint8_t *buf = malloc(packet.rw.len);
        memset(buf, 0, packet.rw.len);
        SOCK_READ(new_socket, buf, packet.rw.len, client_fail)
        free(buf);

        /* ack after successfully written */
        uint32_t ack = ACK;
        SOCK_WRITE(new_socket, &ack, sizeof(ack), client_fail)
        break;
      }
      case EXECUTE: {
        printf("EXECUTE %p %p\n", (void *)packet.exec.addr,
               (void *)packet.exec.stop);

        /* ack after breakpoint */
        uint32_t ack = ACK;
        SOCK_WRITE(new_socket, &ack, sizeof(ack), client_fail)
        break;
      }
      default:
        fprintf(stderr, "unknown packet type 0x%x, dropping client\n",
                (int)packet.type);
        goto client_fail;
      }
    }

    // the above loop should not exit
  client_fail:
    close(new_socket);
    printf("client disconnected.\n");
  }

  close(server_fd);

  return 0;
fail:
  return -1;
}

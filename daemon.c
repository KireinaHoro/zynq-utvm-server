#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "daemon.h"
#include "monitor.h"
#include "packet.h"
#include "socket.h"
#include "util.h"

#define BACKLOG 3

ssize_t start(int port) {
  int server_fd, new_socket;
  struct sockaddr_in address;
  int addrlen = sizeof(address);
  zynq_packet_t packet;

  // open /dev/mem
  // we have hardware coherency via HPC set up, no need for O_SYNC
  int fd = open("/dev/mem", O_RDWR);
  if (fd < 0) {
    perror("open");
    goto fail;
  }
  uint8_t *addr =
      mmap(0, RISCV_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, RISCV_OFFSET);
  if (addr == MAP_FAILED) {
    perror("mmap");
    goto fail_after_open;
  }

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket");
    goto fail_after_mmap;
  }

  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt))) {
    perror("setsockopt");
    goto fail_after_mmap;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind");
    goto fail_after_mmap;
  }

  if (listen(server_fd, BACKLOG) < 0) {
    perror("listen");
    goto fail_after_mmap;
  }

  printf("listening for incoming connection on port %d\n", port);
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
    int tty_fd = -1;
    // client process loop
    while (true) {
      SOCK_READ(new_socket, &packet, sizeof(packet), client_fail)
      switch (packet.type) {
      case READ: {
        // printf("READ %p %ld\n", (void *)packet.rw.addr, packet.rw.len);
        if (packet.rw.addr < RISCV_OFFSET ||
            packet.rw.addr >= RISCV_OFFSET + RISCV_SIZE) {
          fprintf(stderr, "invalid address 0x%lx\n", packet.rw.addr);
          goto client_fail;
        }
        flush(packet.rw.addr, packet.rw.len, tty_fd);
        uint8_t *buf = packet.rw.addr - RISCV_OFFSET + addr;
        SOCK_WRITE(new_socket, buf, packet.rw.len, client_fail)
        // hexdump(buf, packet.rw.len);
        break;
      }
      case WRITE: {
        // printf("WRITE %p %ld\n", (void *)packet.rw.addr, packet.rw.len);
        if (packet.rw.addr < RISCV_OFFSET ||
            packet.rw.addr >= RISCV_OFFSET + RISCV_SIZE) {
          fprintf(stderr, "invalid address 0x%lx\n", packet.rw.addr);
          goto client_fail;
        }
        uint8_t *buf = packet.rw.addr - RISCV_OFFSET + addr;
        SOCK_READ(new_socket, buf, packet.rw.len, client_fail)
        // hexdump(buf, packet.rw.len);

        /* ack after successfully written */
        uint32_t ack = ACK;
        SOCK_WRITE(new_socket, &ack, sizeof(ack), client_fail)
        break;
      }
      case EXECUTE: {
        printf("EXECUTE %p %p\n", (void *)packet.exec.addr,
               (void *)packet.exec.stop);
        // initialize monitor
        if (tty_fd >= 0) {
          // close old fd first, last used in READ
          close(tty_fd);
        }
        tty_fd = initialize(addr);
        if (tty_fd < 0) {
          goto fail_after_mmap;
        }

        uint64_t sepc;
        if ((sepc = execute(packet.exec.addr, packet.exec.stop, tty_fd,
                            new_socket)) < 0) {
          close(tty_fd);
          goto client_fail;
        }

        // printf("execute stopped at 0x%lx\n", sepc);

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
    printf("client %s:%d disconnected\n", addr_str, ntohs(address.sin_port));
  }

  close(server_fd);
  munmap(addr, RISCV_SIZE);
  close(fd);
  return 0;
fail_after_mmap:
  munmap(addr, RISCV_SIZE);
fail_after_open:
  close(fd);
fail:
  return -1;
}

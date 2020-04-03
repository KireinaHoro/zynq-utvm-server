#ifndef _SOCKET_H_
#define _SOCKET_H_

#define SOCK_READ(sock, buf, len, label)                                       \
  {                                                                            \
    int bytes_read, total_read = 0;                                            \
    while (total_read < (len)) {                                               \
      bytes_read =                                                             \
          read((sock), (uint8_t *)(buf) + total_read, (len)-total_read);       \
      if (bytes_read <= 0) {                                                   \
        if (bytes_read < 0) {                                                  \
          perror("read");                                                      \
        }                                                                      \
        goto label;                                                            \
      }                                                                        \
      total_read += bytes_read;                                                \
    }                                                                          \
  }

#define SOCK_WRITE(sock, buf, len, label)                                      \
  {                                                                            \
    int bytes_write, total_write = 0;                                          \
    while (total_write < (len)) {                                              \
      bytes_write =                                                            \
          write((sock), (uint8_t *)(buf) + total_write, (len)-total_write);    \
      if (bytes_write <= 0) {                                                  \
        if (bytes_write < 0) {                                                 \
          perror("write");                                                     \
        }                                                                      \
        goto label;                                                            \
      }                                                                        \
      total_write += bytes_write;                                              \
    }                                                                          \
  }

#endif // _SOCKET_H_

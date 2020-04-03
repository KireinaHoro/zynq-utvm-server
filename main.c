#include <stdio.h>
#include <stdlib.h>

#include "daemon.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  start(atoi(argv[1]));
  exit(EXIT_SUCCESS);
}

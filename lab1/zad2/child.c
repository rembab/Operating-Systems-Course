#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc < 2)
    exit(0);

  int M = atoi(argv[1]);

  for (int j = 0; j < M; j++) {
    printf("Potomek (%d)\n", getpid());

    usleep(250000);
  }

  return 0;
}

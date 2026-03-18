#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define M 1

int zmiennaGlobalna = 0;

int main(int argc, char *argv[]) {
  if (argc < 2)
    exit(0);

  int N = atoi(argv[1]);

  for (int i = 0; i < N; i++) {
    if (fork() == 0) {
      zmiennaGlobalna++;
      for (int j = 0; j < M; j++) {
        usleep(250000);
        printf("Potomek (%d)\n", getpid());
      }
      exit(0);
    }
  }

  int status = 0;
  while (wait(&status) > 0) {
  }
  printf("Rodzic (%d) zmiennaGlobalna=%d\n", getpid(), zmiennaGlobalna);
  return 0;
}

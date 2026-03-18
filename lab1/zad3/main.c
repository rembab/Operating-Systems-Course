#include "definitions.h"

int main(int argc, char *argv[]) {
  if (argc < 3)
    exit(0);

  int N = atoi(argv[1]);
  int M = atoi(argv[2]);

  char command[256];
  snprintf(command, sizeof(command), "./child %d", M);

  unlink(OUTPUT_FILE);

  for (int j = 0; j < N; j++) {
    if (fork() == 0) {
      system(command);
      exit(0);
    }
  }

  int status;
  while (wait(&status) > 0)
    ;
  printf("Rodzic %d\n", getpid());
  return 0;
}

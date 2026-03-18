#include "definitions.h"

int main(int argc, char *argv[]) {
  if (argc < 2)
    exit(0);

  int M = atoi(argv[1]);

  FILE *file = fopen(OUTPUT_FILE, "a");

  if (file == NULL) {
    perror("Error opening file");
    exit(1);
  }

  int fd = fileno(file);
  flock(fd, LOCK_EX);

  for (int j = 0; j < M; j++) {
    fprintf(file, "Potomek (%d)\n", getpid());
    fflush(file);

    usleep(250000);
  }

  flock(fd, LOCK_UN);
  fclose(file);

  return 0;
}

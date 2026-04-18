#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc < 2)
    exit(0);

  int reaction = 0;

  if (strcmp(argv[1], "default") == 0)
    reaction = 0;
  if (strcmp(argv[1], "ignore") == 0)
    reaction = 1;
  if (strcmp(argv[1], "mask") == 0)
    reaction = 2;
  if (strcmp(argv[1], "handle") == 0)
    reaction = 3;

  printf("Launching child, desired reaction id: %d\n", reaction);

  int fork_result = fork();
  if (fork_result == 0) {
    printf("meow\n");
    write(1, "meow\n", 6);
    fflush(stdout);
    // execl("./child", "child");
  } else {
    printf("woof\n");
    union sigval val;
    val.sival_int = reaction;
    sigqueue(fork_result, SIGUSR2, val);
  }
  return 0;
}

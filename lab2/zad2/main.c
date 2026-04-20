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

  int fork_result = fork();
  if (fork_result == 0) {
    execl("./child", "child", NULL);
  } else {
    union sigval val;
    val.sival_int = reaction;
    printf("Proces macierzysty %d\n", getpid());
    printf("Uruchamiam proces potomny o id: %d\n", fork_result);
    printf("Oczekiwane id reakcji: %d\n", reaction);
    sleep(1);
    sigqueue(fork_result, SIGUSR2, val);
    sleep(19);
  }
  return 0;
}

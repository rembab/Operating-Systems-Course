#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void sig_default() { signal(SIGUSR1, SIG_DFL); }

void sig_ignore() { signal(SIGUSR1, SIG_IGN); }

void sig_mask() {
  sigset_t msk;
  sigemptyset(&msk);
  sigaddset(&msk, SIGUSR1);
  sigset_t old;
  sigprocmask(SIG_BLOCK, &msk, &old);
}
void sig_unblock() {
  sigset_t msk;
  sigemptyset(&msk);
  sigaddset(&msk, SIGUSR1);
  sigset_t old;
  sigprocmask(SIG_UNBLOCK, &msk, &old);
}
void handler(int sig_no) {
  printf("Wywołano handler dla sygnału %d\n", sig_no);
}

void sig_handle() { signal(SIGUSR1, handler); }

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Brak argumentu default/ignore/mask/handle\n");
    exit(0);
  }
  if (strcmp(argv[1], "default") == 0)
    sig_default();
  if (strcmp(argv[1], "ignore") == 0)
    sig_ignore();
  if (strcmp(argv[1], "mask") == 0)
    sig_mask();
  if (strcmp(argv[1], "handle") == 0)
    sig_handle();

  printf("%d rozpoczyna pętlę\n", getpid());
  for (int i = 1; i <= 20; i++) {
    printf("%d\n", i);
    if (i == 5 || i == 15) {
      printf("Wysyłam sygnał USR1\n");
      raise(SIGUSR1);
    }
    if (i == 10) {
      sigset_t pending_sigs;
      sigemptyset(&pending_sigs);
      sigpending(&pending_sigs);
      if (sigismember(&pending_sigs, SIGUSR1)) {
        printf("Odblokowywuję USR1\n");
        sig_unblock();
      }
    }
    sleep(1);
  }
  printf("Pętla została wykonana w całości\n");
  return 0;
}

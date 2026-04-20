#include <bits/types/siginfo_t.h>
#include <bits/types/sigset_t.h>
#include <signal.h>
#include <stdio.h>
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

void set_sig1_reaction(int sig, siginfo_t *info, void *ucontext) {
  int reaction = info->si_value.sival_int;
  if (reaction == 0)
    sig_default();
  if (reaction == 1)
    sig_ignore();
  if (reaction == 2)
    sig_mask();
  if (reaction == 3)
    sig_handle();
}

int main(int argc, char *argv[]) {
  struct sigaction act;
  act.sa_sigaction = set_sig1_reaction;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_SIGINFO;
  sigaction(SIGUSR2, &act, NULL);

  printf("%d rozpoczyna pętlę\n", getpid());
  for (int i = 1; i <= 20; i++) {
    sleep(1);
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
  }
  printf("Pętla została wykonana w całości\n");
  return 0;
}

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
  sigprocmask(SIG_BLOCK, &msk, &old);
}
void handler(int sig_no) {
  printf("A signal? In my process? How peculiar! Moreover tis would seem it is "
         "numbered %d!\n",
         sig_no);
}

void sig_handle() { signal(SIGUSR1, handler); }

void set_sig1_reaction(int reaction) {
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

  signal(SIGUSR2, set_sig1_reaction);

  printf("I am beggining loop de loop in the child process of pid: %d\n",
         getpid());

  for (int i = 0; i < 20; i++) {
    printf("%d\n", i);
    if (i == 5 || i == 15) {
      printf("I must signal myself posthaste\n");
      raise(SIGUSR1);
    }
    if (i == 10) {
      sigset_t pending_sigs;
      sigpending(&pending_sigs);
      if (sigismember(&pending_sigs, SIGUSR1)) {
        printf("Good golly! T'would seem my precious signal has been blocked! "
               "I must inquire my trusty sig_unblock immediately!\n");
        sig_unblock();
      }
    }
    sleep(1);
  }
  return 0;
}

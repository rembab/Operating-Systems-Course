
#include "sig_lib.h"
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

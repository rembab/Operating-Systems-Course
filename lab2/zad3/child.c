#include <bits/types/siginfo_t.h>
#include <bits/types/sigset_t.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef DYNAMIC

#include <dlfcn.h>

void (*sig_default)(void) = NULL;
void (*sig_ignore)(void) = NULL;
void (*sig_mask)(void) = NULL;
void (*sig_unblock)(void) = NULL;
void (*sig_handle)(void) = NULL;

#else

#include "sig_lib.h"

#endif

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
#ifdef DYNAMIC
  dlerror();

  printf("Wczytuję bibliotekę dynamicznie\n");
  void *handle = dlopen("./sig_lib.so", RTLD_LAZY);

  if (!handle) {
    fprintf(stderr, "Błąd wczytywania biblioteki dynamicznej: %s\n", dlerror());
  }

  sig_default = (void (*)())dlsym(handle, "sig_default");
  sig_mask = (void (*)())dlsym(handle, "sig_mask");
  sig_unblock = (void (*)())dlsym(handle, "sig_unblock");
  sig_ignore = (void (*)())dlsym(handle, "sig_ignore");
  sig_handle = (void (*)())dlsym(handle, "sig_handle");

  if (dlerror() != NULL) {
    fprintf(stderr, "Błąd wczytywania funkcji biblioteki dynamicznej: %s\n",
            dlerror());
  }
#else

  printf("Wczytuję bibliotekę statyczną/współdzieloną\n");
#endif

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
#ifdef DYNAMIC

  dlclose(handle);
#endif
  printf("Pętla została wykonana w całości\n");
  return 0;
}


#include "sig_lib.h"
void handler(int sig_no) {
  printf("Wywołano handler dla sygnału %d\n", sig_no);
}

void sig_handle() { signal(SIGUSR1, handler); }

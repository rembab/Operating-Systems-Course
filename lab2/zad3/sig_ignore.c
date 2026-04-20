
#include "sig_lib.h"
void sig_ignore() { signal(SIGUSR1, SIG_IGN); }

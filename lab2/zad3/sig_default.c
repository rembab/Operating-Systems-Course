
#include "sig_lib.h"
void sig_default() { signal(SIGUSR1, SIG_DFL); }

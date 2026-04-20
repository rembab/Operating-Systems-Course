#ifndef SIG_LIB
#define SIG_LIB

#include <signal.h>
#include <stdio.h>

void sig_default(void);
void sig_handle(void);
void sig_ignore(void);
void sig_mask(void);
void sig_unblock(void);
#endif

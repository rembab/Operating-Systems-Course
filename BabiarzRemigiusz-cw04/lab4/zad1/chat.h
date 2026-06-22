#ifndef CHAT_H
#define CHAT_H

#include <fcntl.h>
#include <mqueue.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SERVER_QUEUE "/chat_server"
#define MAX_CLIENTS 1024
#define MAX_MSG_LEN 256

typedef struct {
  int client_id;
  char text[MAX_MSG_LEN];
} Msg;

#endif

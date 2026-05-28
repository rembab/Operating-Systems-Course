#include "chat.h"
#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

mqd_t make_client_queue(char *queue_name) {
  struct mq_attr attr;
  attr.mq_flags = 0;
  attr.mq_maxmsg = 10;
  attr.mq_msgsize = sizeof(Msg);
  attr.mq_curmsgs = 0;

  return mq_open(queue_name, O_CREAT | O_RDONLY, 0644, &attr);
}

int main() {
  char queue_name[MAX_MSG_LEN];
  snprintf(queue_name, MAX_MSG_LEN, "/chat_client_%d", getpid());

  mqd_t client_queue = make_client_queue(queue_name);
  if (client_queue == -1) {
    printf("Error creating client queue\n");
    exit(1);
  }

  mqd_t server_queue = mq_open(SERVER_QUEUE, O_WRONLY);
  if (server_queue == -1) {
    printf("Error opening server queue\n");
    mq_close(client_queue);
    mq_unlink(queue_name);
    exit(1);
  }

  Msg msg;
  msg.client_id = -1;
  strncpy(msg.text, queue_name, MAX_MSG_LEN);

  if (mq_send(server_queue, (char *)&msg, sizeof(Msg), 0) == -1) {
    printf("Error sending id request to server\n");
    exit(1);
  }

  if (mq_receive(client_queue, (char *)&msg, sizeof(Msg), 0) == -1) {
    printf("Error receiving id from server\n");
    exit(1);
  }

  int id = msg.client_id;
  printf("Connected to server. Assigned id: %d\n", id);

  int fork_result = fork();
  if (fork_result == 0) {
    while (1) {
      if (mq_receive(client_queue, (char *)&msg, sizeof(Msg), 0) != -1) {
        printf("Client %d: %s\n", msg.client_id, msg.text);
      }
    }
  } else {
    printf("Type q to disconnect\n");
    msg.client_id = id;

    while (fgets(msg.text, MAX_MSG_LEN, stdin)) {
      if (mq_send(server_queue, (char *)&msg, sizeof(Msg), 0) == -1) {

        printf("Error sending message\n");
      }
      if (strncmp(msg.text, "q", 1) == 0) {
        break;
      }
    }

    kill(fork_result, SIGKILL);

    mq_close(client_queue);
    mq_close(server_queue);
    mq_unlink(queue_name);
    printf("Disconnected\n");
  }
}

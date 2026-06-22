#include "chat.h"
#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>

mqd_t make_server_queue() {
  struct mq_attr attr;
  attr.mq_flags = 0;
  attr.mq_maxmsg = 10;
  attr.mq_msgsize = sizeof(Msg);
  attr.mq_curmsgs = 0;

  mq_unlink(SERVER_QUEUE);

  return mq_open(SERVER_QUEUE, O_CREAT | O_RDONLY, 0644, &attr);
}

int main() {
  mqd_t clients[MAX_CLIENTS];
  int next_client_id = 0;

  mqd_t server_queue = make_server_queue();
  if (server_queue == -1) {
    perror("Error opening server queue");
    exit(1);
  }

  printf("Server started\n");

  Msg msg;
  while (1) {
    if (mq_receive(server_queue, (char *)&msg, sizeof(Msg), 0) == -1) {
      printf("Error receiving server queue message");
      continue;
    }

    if (msg.client_id == -1) {
      // register new client
      if (next_client_id == MAX_CLIENTS) {
        printf("Reached max clients");
        continue;
      }
      mqd_t client_queue = mq_open(msg.text, O_WRONLY);

      if (client_queue != -1) {
        clients[next_client_id] = client_queue;
        Msg reply;
        reply.client_id = next_client_id;
        mq_send(client_queue, (char *)&reply, sizeof(Msg), 0);
        printf("New client. Assigned id: %d", next_client_id);
        next_client_id++;
      } else {
        printf("Error creating new client queue");
      }
    } else {
      // broadcast
      for (int i = 0; i < next_client_id; i++) {
        if (i != msg.client_id)
          mq_send(clients[i], (char *)&msg, sizeof(Msg), 0);
      }
    }
  }
  mq_close(server_queue);
  mq_unlink(SERVER_QUEUE);
  return 0;
}

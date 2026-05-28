#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MSG_LEN 10

void handle_sigint(int sig) {}

typedef struct {
  int count;
  int capacity;
  char buffer[][MSG_LEN + 1];
} shared_data_t;

char *random_msg() {
  char *msg = malloc((MSG_LEN + 1) * sizeof(char));
  for (int i = 0; i < MSG_LEN; i++) {
    msg[i] = rand() % 26 + 'a';
  }
  msg[MSG_LEN] = '\0';
  return msg;
}

void producer_loop(shared_data_t *shared, sem_t *mutex, sem_t *empty,
                   sem_t *full) {
  srand(time(NULL) ^ getpid());

  while (1) {
    sem_wait(empty);
    sem_wait(mutex);

    char *msg = random_msg();

    strncpy(shared->buffer[shared->count], msg, MSG_LEN + 1);
    printf("%d wrote: %s\n", getpid(), shared->buffer[shared->count]);

    shared->count++;

    free(msg);
    sem_post(mutex);
    sem_post(full);
  }
}

void consumer_loop(shared_data_t *shared, sem_t *mutex, sem_t *empty,
                   sem_t *full) {
  while (1) {
    sem_wait(full);
    sem_wait(mutex);

    char msg[MSG_LEN + 1];
    strncpy(msg, shared->buffer[0], MSG_LEN + 1);

    for (int j = 0; j < shared->count - 1; j++) {
      strncpy(shared->buffer[j], shared->buffer[j + 1], MSG_LEN + 1);
    }

    shared->count--;

    sem_post(mutex);
    sem_post(empty);

    for (int i = 0; i < MSG_LEN; i++) {
      printf("%d read: %c\n", getpid(), msg[i]);
      usleep(300000);
    }
  }
}

int main(int argc, char **argv) {
  if (argc < 4) {
    printf("Usage: %s <N_producers> <M_consumers> <K_capacity>\n", argv[0]);
    exit(1);
  }

  signal(SIGINT, handle_sigint);

  int N = atoi(argv[1]);
  int M = atoi(argv[2]);
  int K = atoi(argv[3]);

  shm_unlink("/msg_queue_shm");
  int shm = shm_open("/msg_queue_shm", O_CREAT | O_RDWR, 0666);
  if (shm == -1) {
    perror("shm_open failed");
    exit(1);
  }

  size_t shm_size = sizeof(shared_data_t) + (K * (MSG_LEN + 1) * sizeof(char));
  if (ftruncate(shm, shm_size) == -1) {
    perror("ftruncate failed");
    exit(1);
  }

  shared_data_t *shared =
      mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
  if (shared == MAP_FAILED) {
    perror("mmap failed");
    exit(1);
  }

  shared->count = 0;
  shared->capacity = K;

  sem_unlink("/mutex_sem");
  sem_unlink("/empty_sem");
  sem_unlink("/full_sem");

  sem_t *mutex_sem = sem_open("/mutex_sem", O_CREAT, 0666, 1);
  sem_t *empty_sem = sem_open("/empty_sem", O_CREAT, 0666, K);
  sem_t *full_sem = sem_open("/full_sem", O_CREAT, 0666, 0);

  for (int i = 0; i < N; i++) {
    if (fork() == 0) {
      signal(SIGINT, SIG_DFL);
      producer_loop(shared, mutex_sem, empty_sem, full_sem);
      exit(0);
    }
  }

  for (int i = 0; i < M; i++) {
    if (fork() == 0) {
      signal(SIGINT, SIG_DFL);
      consumer_loop(shared, mutex_sem, empty_sem, full_sem);
      exit(0);
    }
  }
  while (wait(NULL) > 0)
    ;

  sem_close(mutex_sem);
  sem_close(empty_sem);
  sem_close(full_sem);
  sem_unlink("/mutex_sem");
  sem_unlink("/empty_sem");
  sem_unlink("/full_sem");

  munmap(shared, shm_size);
  shm_unlink("/msg_queue_shm");
  return 0;
}

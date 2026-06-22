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

#define MAX_K 4096
#define MSG_LEN 10

#define MUTEX_SEM "/mutex_sem"
#define FULL_SEM "/full_sem"
#define EMPTY_SEM "/empty_sem"
#define SHM_NAME "/msg_queue_shm"

void handle_sigint(int sig) { killpg(0, SIGTERM); }

typedef struct {
  int normal_count;
  char normal_queue[MAX_K][MSG_LEN + 1];

  int priority_count;
  char priority_queue[MAX_K][MSG_LEN + 1];
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

    if (rand() % 100 < 30) {
      strncpy(shared->priority_queue[shared->priority_count], msg, MSG_LEN + 1);
      printf("%d wrote PRIO: %s\n", getpid(),
             shared->priority_queue[shared->priority_count]);
      shared->priority_count++;
    } else {
      strncpy(shared->normal_queue[shared->normal_count], msg, MSG_LEN + 1);
      printf("%d wrote NORM: %s\n", getpid(),
             shared->normal_queue[shared->normal_count]);
      shared->normal_count++;
    }

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

    if (shared->priority_count > 0) {
      strncpy(msg, shared->priority_queue[0], MSG_LEN + 1);
      for (int j = 0; j < shared->priority_count - 1; j++) {
        strncpy(shared->priority_queue[j], shared->priority_queue[j + 1],
                MSG_LEN + 1);
      }
      shared->priority_count--;
      printf("%d read PRIO\n", getpid());
    } else {
      strncpy(msg, shared->normal_queue[0], MSG_LEN + 1);
      for (int j = 0; j < shared->normal_count - 1; j++) {
        strncpy(shared->normal_queue[j], shared->normal_queue[j + 1],
                MSG_LEN + 1);
      }
      shared->normal_count--;
      printf("%d read NORM\n", getpid());
    }

    sem_post(mutex);
    sem_post(empty);

    for (int i = 0; i < MSG_LEN; i++) {
      printf("%d read: %c\n", getpid(), msg[i]);
      usleep(300000);
    }
  }
}

void manager_loop(shared_data_t *shared, sem_t *mutex) {
  while (1) {
    sleep(5);

    sem_wait(mutex);

    printf("QUEUE COUNTS: NORM: %d, PRIO: %d\n", shared->normal_count,
           shared->priority_count);

    if (shared->normal_count > 0) {
      strncpy(shared->priority_queue[shared->priority_count],
              shared->normal_queue[0], MSG_LEN + 1);
      shared->priority_count++;

      for (int i = 0; i < shared->normal_count - 1; i++) {
        strncpy(shared->normal_queue[i], shared->normal_queue[i + 1],
                MSG_LEN + 1);
      }
      shared->normal_count--;
    }

    sem_post(mutex);
  }
}

int main(int argc, char **argv) {
  if (argc < 4) {
    printf(
        "Provide number of producers, number of consumers, queue capacity\n");
    exit(1);
  }

  signal(SIGINT, handle_sigint);

  int N = atoi(argv[1]);
  int M = atoi(argv[2]);
  int K = atoi(argv[3]);

  if (N <= 0 || M <= 0 || K <= 0 || K >= 1024) {
  }

  shm_unlink(SHM_NAME);
  int shm = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
  if (shm == -1) {
    perror("shm_open failed");
    exit(1);
  }

  size_t shm_size = sizeof(shared_data_t);

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

  shared->normal_count = 0;
  shared->priority_count = 0;

  sem_unlink(MUTEX_SEM);
  sem_unlink(EMPTY_SEM);
  sem_unlink(FULL_SEM);

  sem_t *mutex_sem = sem_open(MUTEX_SEM, O_CREAT, 0666, 1);
  sem_t *empty_sem = sem_open(EMPTY_SEM, O_CREAT, 0666, K);
  sem_t *full_sem = sem_open(FULL_SEM, O_CREAT, 0666, 0);

  if (fork() == 0) {
    signal(SIGINT, SIG_DFL);
    manager_loop(shared, mutex_sem);
    exit(0);
  }

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
  sem_unlink(MUTEX_SEM);
  sem_unlink(EMPTY_SEM);
  sem_unlink(FULL_SEM);

  munmap(shared, shm_size);
  shm_unlink(SHM_NAME);
  return 0;
}

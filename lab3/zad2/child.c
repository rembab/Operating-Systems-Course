#include <bits/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MAIN_TO_CHILD "/tmp/main_to_child"
#define CHILD_TO_MAIN "/tmp/child_to_main"

double f(double x) { return 4.0 / (x * x + 1.0); }

double riemann(double start, double end, double step) {
  double sum = 0.0;
  for (double x = start; x < end; x += step) {
    double current_step = step;
    if (x + step > end)
      current_step = end - x;
    sum += f(x) * current_step;
  }
  return sum;
}

int main() {

  int receiver = open(MAIN_TO_CHILD, O_RDONLY);

  int k;
  read(receiver, &k, sizeof(int));

  double rectangle_size;
  read(receiver, &rectangle_size, sizeof(double));

  double start_riemann;
  read(receiver, &start_riemann, sizeof(double));

  double end_riemann;
  read(receiver, &end_riemann, sizeof(double));
  close(receiver);

  int (*pipes)[2] = malloc(k * sizeof(*pipes));
  pid_t fork_result = 0;

  for (int i = 0; i < k; i++) {
    pipe(pipes[i]);
    fork_result = fork();
    if (fork_result == 0) {
      close(pipes[i][0]);
      double start = i * ((end_riemann - start_riemann) / k) + start_riemann;
      double end =
          (i + 1) * ((end_riemann - start_riemann) / k) + start_riemann;

      double interval_result = riemann(start, end, rectangle_size);
      write(pipes[i][1], &interval_result, sizeof(double));
      close(pipes[i][1]);
      exit(0);

    } else {
      close(pipes[i][1]);
    }
  }

  double total = 0;
  for (int i = 0; i < k; i++) {
    double interval_result;
    read(pipes[i][0], &interval_result, sizeof(double));
    total += interval_result;
    close(pipes[i][0]);
  }

  int sender = open(CHILD_TO_MAIN, O_WRONLY);
  write(sender, &total, sizeof(double));
  close(sender);

  free(pipes);
}

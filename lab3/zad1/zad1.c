#include <bits/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

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

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Wrong number of arguments. Provide k and n values");
    return 2;
  }
  double rectangle_size = atof(argv[1]);
  int n = atoi(argv[2]);

  for (int k = 1; k <= n; k++) {
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    int (*pipes)[2] = malloc(k * sizeof(*pipes));
    pid_t fork_result = 0;

    for (int i = 0; i < k; i++) {
      pipe(pipes[i]);
      fork_result = fork();
      if (fork_result == 0) {
        close(pipes[i][0]);
        double start = i * (1.0 / k);
        double end = (i + 1) * (1.0 / k);

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
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    int elapsed_ns = (end_time.tv_sec - start_time.tv_sec) * 1e9 +
                     (end_time.tv_nsec - start_time.tv_nsec);
    printf("k = %2d \t Wynik = %.10f \t Czas (ns) = %d \n", k, total,
           elapsed_ns);

    free(pipes);
  }
}

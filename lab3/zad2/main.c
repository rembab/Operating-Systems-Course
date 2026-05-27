#include <bits/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
double f(double x) { return 4.0 / (x * x + 1.0); }

#define MAIN_TO_CHILD "/tmp/main_to_child"
#define CHILD_TO_MAIN "/tmp/child_to_main"

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Zła liczba argumentów. Podaj n oraz rozmiar prostokąta.\n");
    return 2;
  }

  unlink(MAIN_TO_CHILD);
  unlink(CHILD_TO_MAIN);
  mkfifo(MAIN_TO_CHILD, 0666);
  mkfifo(CHILD_TO_MAIN, 0666);

  if (fork() == 0) {
    execl("./child", "./child", NULL);
    exit(0);
  }

  double start = 0, end = 0;

  while (start >= end) {
    printf("Podaj początek przedziału \n");
    scanf("%lf", &start);
    printf("Podaj koniec przedziału \n");
    scanf("%lf", &end);
  }
  int n = atoi(argv[1]);
  double rectangle_size = atof(argv[2]);

  int sender = open(MAIN_TO_CHILD, O_WRONLY);
  write(sender, &n, sizeof(int));
  write(sender, &rectangle_size, sizeof(double));
  write(sender, &start, sizeof(double));
  write(sender, &end, sizeof(double));
  close(sender);

  double result;
  int receiver = open(CHILD_TO_MAIN, O_RDONLY);
  read(receiver, &result, sizeof(double));
  close(receiver);
  printf("Wynik obliczeń: %f\n", result);
  unlink(MAIN_TO_CHILD);
  unlink(CHILD_TO_MAIN);
}

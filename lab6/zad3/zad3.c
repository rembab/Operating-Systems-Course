#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_FRAME_DIFF 20.0
#define BUFFER_SIZE 16

#define LEFT_FREQUENCY 20
#define RIGHT_FREQUENCY 25
#define STATE_FREQUENCY 10

typedef struct {
  int id;
  struct timespec timestamp;
} Frame;

typedef struct {
  int id;
  Frame left_frame;
  Frame right_frame;
} StereoPair;

typedef struct {
  double x, y, theta;
  struct timespec timestamp;
} RobotState;

// circular FIFO buffer for camera frames
typedef struct {
  Frame data[BUFFER_SIZE];
  int head;
  int tail;
  int count;
  pthread_mutex_t lock;
  pthread_cond_t not_empty;
} frame_buffer_t;

// circular FIFO buffer for stereo pairs
typedef struct {
  StereoPair data[BUFFER_SIZE];
  int head;
  int tail;
  int count;
  pthread_mutex_t lock;
  pthread_cond_t not_empty;
} stereo_buffer_t;

// circular FIFO buffer for robot states
typedef struct {
  RobotState data[BUFFER_SIZE];
  int head;
  int tail;
  int count;
  pthread_mutex_t lock;
  pthread_cond_t not_empty;
} state_buffer_t;

volatile sig_atomic_t keep_running = 1;

frame_buffer_t left_cam_buf;
frame_buffer_t right_cam_buf;
stereo_buffer_t sync_buf;
state_buffer_t robot_state_buf;

// atomic counters watched by the watchdog and stats_thread
atomic_int left_count = 0;
atomic_int right_count = 0;
atomic_int stereo_count = 0;
atomic_int state_count = 0;
atomic_int deadline_misses = 0;

void sleep_ms(int ms) {
  struct timespec ts = {ms / 1000, (ms % 1000) * 1000000};
  clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
}

struct timespec get_current_time() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return ts;
}

// t1 - t2 in ms
double get_time_diff_ms(struct timespec t1, struct timespec t2) {
  double ms = (t1.tv_sec - t2.tv_sec) * 1000.0;
  ms += (t1.tv_nsec - t2.tv_nsec) / 1000000.0;
  return ms;
}

// initialize a frame buffer
void init_frame_buffer(frame_buffer_t *b) {
  b->head = 0;
  b->tail = 0;
  b->count = 0;
  pthread_mutex_init(&b->lock, NULL);
  pthread_cond_init(&b->not_empty, NULL);
}

// put a frame in the buffer, if it is full we drop the oldest one
void push_frame(frame_buffer_t *b, Frame f) {
  pthread_mutex_lock(&b->lock);
  if (b->count == BUFFER_SIZE) {
    b->tail = (b->tail + 1) % BUFFER_SIZE;
    b->count--;
  }
  b->data[b->head] = f;
  b->head = (b->head + 1) % BUFFER_SIZE;
  b->count++;
  pthread_cond_signal(&b->not_empty);
  pthread_mutex_unlock(&b->lock);
}

// take a frame from the buffer, waits until there is one
// returns 0 if we are shutting down
int pop_frame(frame_buffer_t *b, Frame *f) {
  pthread_mutex_lock(&b->lock);
  while (b->count == 0 && keep_running) {
    pthread_cond_wait(&b->not_empty, &b->lock);
  }
  if (b->count == 0) {
    pthread_mutex_unlock(&b->lock);
    return 0;
  }
  *f = b->data[b->tail];
  b->tail = (b->tail + 1) % BUFFER_SIZE;
  b->count--;
  pthread_mutex_unlock(&b->lock);
  return 1;
}

// initialize a stereo buffer
void init_stereo_buffer(stereo_buffer_t *b) {
  b->head = 0;
  b->tail = 0;
  b->count = 0;
  pthread_mutex_init(&b->lock, NULL);
  pthread_cond_init(&b->not_empty, NULL);
}

void push_stereo(stereo_buffer_t *b, StereoPair sp) {
  pthread_mutex_lock(&b->lock);
  if (b->count == BUFFER_SIZE) {
    b->tail = (b->tail + 1) % BUFFER_SIZE;
    b->count--;
  }
  b->data[b->head] = sp;
  b->head = (b->head + 1) % BUFFER_SIZE;
  b->count++;
  pthread_cond_signal(&b->not_empty);
  pthread_mutex_unlock(&b->lock);
}

int pop_stereo(stereo_buffer_t *b, StereoPair *sp) {
  pthread_mutex_lock(&b->lock);
  while (b->count == 0 && keep_running) {
    pthread_cond_wait(&b->not_empty, &b->lock);
  }
  if (b->count == 0) {
    pthread_mutex_unlock(&b->lock);
    return 0;
  }
  *sp = b->data[b->tail];
  b->tail = (b->tail + 1) % BUFFER_SIZE;
  b->count--;
  pthread_mutex_unlock(&b->lock);
  return 1;
}

// initialize a robot state buffer
void init_state_buffer(state_buffer_t *b) {
  b->head = 0;
  b->tail = 0;
  b->count = 0;
  pthread_mutex_init(&b->lock, NULL);
  pthread_cond_init(&b->not_empty, NULL);
}

void push_state(state_buffer_t *b, RobotState s) {
  pthread_mutex_lock(&b->lock);
  if (b->count == BUFFER_SIZE) {
    b->tail = (b->tail + 1) % BUFFER_SIZE;
    b->count--;
  }
  b->data[b->head] = s;
  b->head = (b->head + 1) % BUFFER_SIZE;
  b->count++;
  pthread_cond_signal(&b->not_empty);
  pthread_mutex_unlock(&b->lock);
}

int pop_state(state_buffer_t *b, RobotState *s) {
  pthread_mutex_lock(&b->lock);
  while (b->count == 0 && keep_running) {
    pthread_cond_wait(&b->not_empty, &b->lock);
  }
  if (b->count == 0) {
    pthread_mutex_unlock(&b->lock);
    return 0;
  }
  *s = b->data[b->tail];
  b->tail = (b->tail + 1) % BUFFER_SIZE;
  b->count--;
  pthread_mutex_unlock(&b->lock);
  return 1;
}

// wake up the waiting threads so they can see that we want to stop
void wake_everyone() {
  pthread_cond_broadcast(&left_cam_buf.not_empty);
  pthread_cond_broadcast(&right_cam_buf.not_empty);
  pthread_cond_broadcast(&sync_buf.not_empty);
  pthread_cond_broadcast(&robot_state_buf.not_empty);
}

// handler for safe exit
void handle_sigint(int sig) { keep_running = 0; }

// 25 hz, left camera
void *left_camera_thread(void *arg) {
  int frame_id = 0;
  while (keep_running) {
    Frame f = {++frame_id, get_current_time()};
    push_frame(&left_cam_buf, f);
    left_count++;
    sleep_ms(1000 / LEFT_FREQUENCY);
  }
  return NULL;
}

// 20 hz, right camera
void *right_camera_thread(void *arg) {
  int frame_id = 0;
  while (keep_running) {
    Frame f = {++frame_id, get_current_time()};
    push_frame(&right_cam_buf, f);
    right_count++;
    sleep_ms(1000 / RIGHT_FREQUENCY);
  }
  return NULL;
}

// takes one frame from each camera and makes a stereo pair if the
// timestamps are close enough
void *sync_thread(void *arg) {
  Frame left, right;
  int has_left = 0, has_right = 0;
  int curr_id = 0;

  while (keep_running) {
    if (!has_left) {
      has_left = pop_frame(&left_cam_buf, &left);
    }
    if (!has_right) {
      has_right = pop_frame(&right_cam_buf, &right);
    }

    if (has_left && has_right) {
      double diff = get_time_diff_ms(left.timestamp, right.timestamp);

      if (diff >= -MAX_FRAME_DIFF && diff <= MAX_FRAME_DIFF) {
        StereoPair sp = {curr_id, left, right};
        push_stereo(&sync_buf, sp);
        has_left = 0;
        has_right = 0;
        stereo_count++;
      } else if (diff > MAX_FRAME_DIFF) {
        has_right = 0;
      } else {
        has_left = 0;
      }
      curr_id++;
    }
  }
  return NULL;
}

// 10 hz, writes the stereo pair to two files
void *image_writer_thread(void *arg) {
  StereoPair sp;
  while (keep_running) {
    if (pop_stereo(&sync_buf, &sp)) {
      char filename_l[128], filename_r[128];

      sprintf(filename_l, "images/%04d_left.jpg", sp.id);
      sprintf(filename_r, "images/%04d_right.jpg", sp.id);

      FILE *fl = fopen(filename_l, "w");
      if (fl) {
        fprintf(fl, "Left cam: %ld.%09ld\nFrame id: %d\n",
                sp.left_frame.timestamp.tv_sec, sp.left_frame.timestamp.tv_nsec,
                sp.left_frame.id);
        fclose(fl);
      }
      FILE *fr = fopen(filename_r, "w");
      if (fr) {
        fprintf(fr, "Right cam: %ld.%09ld\nFrame id: %d\n",
                sp.right_frame.timestamp.tv_sec,
                sp.right_frame.timestamp.tv_nsec, sp.right_frame.id);
        fclose(fr);
      }

      printf("Saved stereo pair: ID: %04d L:%04d R:%04d\n", sp.id,
             sp.left_frame.id, sp.right_frame.id);

      sleep_ms(100);
    }
  }
  return NULL;
}

// 100 hz, robot movement simulation, also checks its own deadline
void *robot_state_thread(void *arg) {
  double x = 10.0, y = 0.0, th = 0.0;
  struct timespec last = get_current_time();

  while (keep_running) {
    struct timespec now = get_current_time();
    if (get_time_diff_ms(now, last) > STATE_FREQUENCY * 1.2) {
      deadline_misses++;
    }
    last = now;

    RobotState state = {x, y, th, now};
    x += -0.01;
    y += 0.01;
    th += 0.005;

    push_state(&robot_state_buf, state);
    state_count++;

    sleep_ms(10);
  }
  return NULL;
}

// 10 hz, writes the robot state to a log file
void *logger_thread(void *arg) {
  (void)arg;
  RobotState state;
  FILE *log_file = fopen("state_log.txt", "w");
  while (keep_running) {
    if (pop_state(&robot_state_buf, &state)) {
      if (log_file) {
        fprintf(log_file, "T: %ld.%09ld | X: %.2f, Y: %.2f, Rot: %.2f\n",
                state.timestamp.tv_sec, state.timestamp.tv_nsec, state.x,
                state.y, state.theta);
        fflush(log_file);
      }
      sleep_ms(100);
    }
  }
  if (log_file)
    fclose(log_file);
  return NULL;
}

// prints some statistics every 3 seconds
void *stats_thread(void *arg) {
  int prev_left = 0, prev_right = 0, prev_state = 0;
  while (keep_running) {
    sleep_ms(3000);
    if (!keep_running)
      break;

    int l_count = left_count;
    int r_count = right_count;
    int ster_count = stereo_count;
    int stat_count = state_count;

    printf("\nStats:\n");
    printf("Left frames: %d (%.1f Hz)\n", l_count, (l_count - prev_left) / 3.0);
    printf("Right frames: %d (%.1f Hz)\n", r_count,
           (r_count - prev_right) / 3.0);
    printf("Stereo pairs: %d\n", ster_count);
    printf("Robot states: %d (%.1f Hz)\n\n", stat_count,
           (stat_count - prev_state) / 3.0);

    prev_left = l_count;
    prev_right = r_count;
    prev_state = stat_count;
  }
  return NULL;
}

// every second checks if the cameras and robot run fast enough
void *watchdog_thread(void *arg) {
  int prev_left = 0, prev_right = 0, prev_state = 0, prev_miss = 0;
  while (keep_running) {
    sleep_ms(1000);
    if (!keep_running)
      break;

    int l = left_count, r = right_count, s = state_count, m = deadline_misses;

    if (l - prev_left < LEFT_FREQUENCY * 0.8)
      printf("[WATCHDOG] LEFT CAMERA SLOW\n");
    if (r - prev_right < RIGHT_FREQUENCY * 0.8)
      printf("[WATCHDOG] RIGHT CAMERA SLOW\n");
    if (s - prev_state < STATE_FREQUENCY * 0.8)
      printf("[WATCHDOG] ROBOT STATE SLOW\n");
    if (m > prev_miss)
      printf("[WATCHDOG] ROBOT DEADLINE MISSED\n");

    prev_left = l;
    prev_right = r;
    prev_state = s;
    prev_miss = m;
  }
  return NULL;
}

// writes the final report file
void write_report(int seconds) {
  FILE *f = fopen("report.txt", "w");
  if (!f)
    return;
  fprintf(f, "Runtime: %d s\n", seconds);
  fprintf(f, "Left frames: %d\n", (int)left_count);
  fprintf(f, "Right frames: %d\n", (int)right_count);
  fprintf(f, "Stereo pairs: %d\n", (int)stereo_count);
  fprintf(f, "Robot states: %d\n", (int)state_count);
  fprintf(f, "Deadline misses: %d\n", (int)deadline_misses);
  fclose(f);
  printf("Report saved to ./report.txt\n");
}

void set_priority(pthread_t t, int prio) {
  struct sched_param sp;
  sp.sched_priority = prio;
  if (pthread_setschedparam(t, SCHED_FIFO, &sp) != 0) {
    printf("Could not set priority %d\n", prio);
  }
}

int main(int argc, char **argv) {
  int run_seconds = 20;
  if (argc > 1) {
    run_seconds = atoi(argv[1]);
  }

  signal(SIGINT, handle_sigint);

  system("mkdir -p images");

  init_frame_buffer(&left_cam_buf);
  init_frame_buffer(&right_cam_buf);
  init_stereo_buffer(&sync_buf);
  init_state_buffer(&robot_state_buf);

  pthread_t t_cam_l, t_cam_r, t_sync, t_writer, t_robot, t_logger, t_watchdog,
      t_stats;

  printf("Running for %ds (CTRL+C to stop)\n", run_seconds);

  time_t start = time(NULL);

  pthread_create(&t_cam_l, NULL, left_camera_thread, NULL);
  pthread_create(&t_cam_r, NULL, right_camera_thread, NULL);
  pthread_create(&t_sync, NULL, sync_thread, NULL);
  pthread_create(&t_writer, NULL, image_writer_thread, NULL);
  pthread_create(&t_robot, NULL, robot_state_thread, NULL);
  pthread_create(&t_logger, NULL, logger_thread, NULL);
  pthread_create(&t_watchdog, NULL, watchdog_thread, NULL);
  pthread_create(&t_stats, NULL, stats_thread, NULL);

  set_priority(t_robot, 80);
  set_priority(t_watchdog, 70);

  for (int i = 0; i < run_seconds && keep_running; i++) {
    sleep_ms(1000);
  }

  printf("\nCleanup...\n");
  keep_running = 0;
  wake_everyone();

  pthread_join(t_cam_l, NULL);
  pthread_join(t_cam_r, NULL);
  pthread_join(t_sync, NULL);
  pthread_join(t_writer, NULL);
  pthread_join(t_robot, NULL);
  pthread_join(t_logger, NULL);
  pthread_join(t_watchdog, NULL);
  pthread_join(t_stats, NULL);

  write_report((int)(time(NULL) - start));

  printf("Finished. Images saved to ./images/\n");
  return 0;
}

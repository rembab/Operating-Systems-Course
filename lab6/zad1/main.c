#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_FRAME_DIFF 20.0

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

typedef struct {
  Frame data;
  pthread_mutex_t lock;
  sem_t sem_full;
  sem_t sem_empty;
} FrameBox;

typedef struct {
  StereoPair data;
  pthread_mutex_t lock;
  sem_t sem_full;
  sem_t sem_empty;
} StereoBox;

typedef struct {
  RobotState data;
  pthread_mutex_t lock;
  sem_t sem_full;
  sem_t sem_empty;
} StateBox;

volatile sig_atomic_t keep_running = 1;

FrameBox left_cam_box;
FrameBox right_cam_box;
StereoBox sync_box;
StateBox robot_state_box;

void sleep_ms(int ms) {
  struct timespec ts = {ms / 1000, (ms % 1000) * 1000000};
  clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
}

struct timespec get_current_time() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return ts;
}

struct timespec get_100ms_later() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_nsec += 100000000;
  if (ts.tv_nsec >= 1000000000) {
    ts.tv_sec++;
    ts.tv_nsec -= 1000000000;
  }
  return ts;
}

// t1 - t2 in ms
double get_time_diff_ms(struct timespec t1, struct timespec t2) {
  double ms = (t1.tv_sec - t2.tv_sec) * 1000.0;
  ms += (t1.tv_nsec - t2.tv_nsec) / 1000000.0;
  return ms;
}

// initialize a frame box
void init_frame_box(FrameBox *box) {
  pthread_mutex_init(&box->lock, NULL);
  sem_init(&box->sem_full, 0, 0);
  sem_init(&box->sem_empty, 0, 1);
}

// pushes a frame into a frame box, is non blocking
// if box is full, replaces data
void push_frame(FrameBox *box, Frame f) {
  // push new frame data into the box
  pthread_mutex_lock(&box->lock);
  box->data = f;
  pthread_mutex_unlock(&box->lock);

  // if the box is currently empty, signal it has been filled
  if (sem_trywait(&box->sem_empty) == 0) {
    sem_post(&box->sem_full);
  }
}

// removes a frame from a box, returns the frame into the *f pointer
// 1 on success, 0 on fail
// blocking
int pop_frame(FrameBox *box, Frame *f) {
  struct timespec ts = get_100ms_later();
  if (sem_timedwait(&box->sem_full, &ts) == 0) {
    pthread_mutex_lock(&box->lock);
    *f = box->data;
    pthread_mutex_unlock(&box->lock);
    sem_post(&box->sem_empty);
    return 1;
  }
  return 0;
}

// initialize a stereo box
void init_stereo_box(StereoBox *box) {
  pthread_mutex_init(&box->lock, NULL);
  sem_init(&box->sem_full, 0, 0);
  sem_init(&box->sem_empty, 0, 1);
}

// push a stereo frame to a box
// non blocking, on non empty replaces data
void push_stereo(StereoBox *box, StereoPair sp) {
  pthread_mutex_lock(&box->lock);
  box->data = sp;
  pthread_mutex_unlock(&box->lock);

  if (sem_trywait(&box->sem_empty) == 0) {
    sem_post(&box->sem_full);
  }
}

// removes a stereo frame from a box
// blocking
int pop_stereo(StereoBox *box, StereoPair *sp) {
  struct timespec ts = get_100ms_later();
  if (sem_timedwait(&box->sem_full, &ts) == 0) {
    pthread_mutex_lock(&box->lock);
    *sp = box->data;
    pthread_mutex_unlock(&box->lock);
    sem_post(&box->sem_empty);
    return 1;
  }
  return 0;
}

// initialize a robot state box
void init_state_box(StateBox *box) {
  pthread_mutex_init(&box->lock, NULL);
  sem_init(&box->sem_full, 0, 0);
  sem_init(&box->sem_empty, 0, 1);
}

// push a robot state log into a state box
// non blocking
void push_state(StateBox *box, RobotState s) {
  pthread_mutex_lock(&box->lock);
  box->data = s;
  pthread_mutex_unlock(&box->lock);

  if (sem_trywait(&box->sem_empty) == 0) {
    sem_post(&box->sem_full);
  }
}

// remove a robot state log from a box
// blocking
int pop_state(StateBox *box, RobotState *s) {
  struct timespec ts = get_100ms_later();
  if (sem_timedwait(&box->sem_full, &ts) == 0) {
    pthread_mutex_lock(&box->lock);
    *s = box->data;
    pthread_mutex_unlock(&box->lock);
    sem_post(&box->sem_empty);
    return 1;
  }
  return 0;
}

// handler for safe exit
void handle_sigint(int sig) { keep_running = 0; }

// 25 hz
// pushes frames asynchronously. If last frame wasnt read yet, it dies
void *left_camera_thread(void *arg) {
  int frame_id = 0;
  while (keep_running) {
    Frame f = {++frame_id, get_current_time()};
    push_frame(&left_cam_box, f);
    sleep_ms(40);
  }
  return NULL;
}

// 20 hz
// pushes frames asynchronously. If last frame wasnt read yet, it dies
void *right_camera_thread(void *arg) {
  int frame_id = 0;
  while (keep_running) {
    Frame f = {++frame_id, get_current_time()};
    push_frame(&right_cam_box, f);
    sleep_ms(50);
  }
  return NULL;
}

// fetches left and right frame
// if difference is too high kills the older frame and fetches a new one
// if not, saves to stereo box
void *sync_thread(void *arg) {
  Frame left, right;
  int has_left = 0, has_right = 0;
  int curr_id = 0;

  while (keep_running) {
    if (!has_left) {
      has_left = pop_frame(&left_cam_box, &left);
    }
    if (!has_right) {
      has_right = pop_frame(&right_cam_box, &right);
    }

    if (has_left && has_right) {
      double diff = get_time_diff_ms(left.timestamp, right.timestamp);

      if (diff >= -MAX_FRAME_DIFF && diff <= MAX_FRAME_DIFF) {
        StereoPair sp = {curr_id, left, right};
        push_stereo(&sync_box, sp);
        has_left = 0;
        has_right = 0;
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

// 10 hz
// takes from stereo box and writes a image log
void *image_writer_thread(void *arg) {
  StereoPair sp;
  while (keep_running) {
    if (pop_stereo(&sync_box, &sp)) {
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

      double diff =
          get_time_diff_ms(sp.left_frame.timestamp, sp.right_frame.timestamp);
      double abs_diff = diff >= 0 ? diff : -diff;

      printf("Saved stereo pair: ID: %04d L:%04d & R:%04d Diff: %.2f ms\n",
             sp.id, sp.left_frame.id, sp.right_frame.id, abs_diff);

      sleep_ms(100);
    }
  }
  return NULL;
}

// 100 hz
// robot movement simulation. Saves coords, rotation and timestamp to state box
void *robot_state_thread(void *arg) {
  double x = 10.0, y = 0.0, th = 0.0;
  while (keep_running) {
    RobotState state = {x, y, th, get_current_time()};
    x += -0.01;
    y += 0.01;
    th += 0.005;

    push_state(&robot_state_box, state);
    sleep_ms(10);
  }
  return NULL;
}

// 10hz
// pops from state box saving the newest robot state log
void *logger_thread(void *arg) {
  (void)arg;
  RobotState state;
  FILE *log_file = fopen("state_log.txt", "w");
  while (keep_running) {
    if (pop_state(&robot_state_box, &state)) {
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

int main() {
  signal(SIGINT, handle_sigint);

  system("mkdir -p images");

  init_frame_box(&left_cam_box);
  init_frame_box(&right_cam_box);
  init_stereo_box(&sync_box);
  init_state_box(&robot_state_box);

  pthread_t t_cam_l, t_cam_r, t_sync, t_writer, t_robot, t_logger;

  printf("Running for 20s\n");

  pthread_create(&t_cam_l, NULL, left_camera_thread, NULL);
  pthread_create(&t_cam_r, NULL, right_camera_thread, NULL);
  pthread_create(&t_sync, NULL, sync_thread, NULL);
  pthread_create(&t_writer, NULL, image_writer_thread, NULL);
  pthread_create(&t_robot, NULL, robot_state_thread, NULL);
  pthread_create(&t_logger, NULL, logger_thread, NULL);

  for (int i = 0; i < 40 && keep_running; i++) {
    sleep_ms(500);
  }

  printf("\nCleanup...\n");
  keep_running = 0;

  pthread_join(t_cam_l, NULL);
  pthread_join(t_cam_r, NULL);
  pthread_join(t_sync, NULL);
  pthread_join(t_writer, NULL);
  pthread_join(t_robot, NULL);
  pthread_join(t_logger, NULL);

  pthread_mutex_destroy(&left_cam_box.lock);
  sem_destroy(&left_cam_box.sem_full);
  sem_destroy(&left_cam_box.sem_empty);

  pthread_mutex_destroy(&right_cam_box.lock);
  sem_destroy(&right_cam_box.sem_full);
  sem_destroy(&right_cam_box.sem_empty);

  pthread_mutex_destroy(&sync_box.lock);
  sem_destroy(&sync_box.sem_full);
  sem_destroy(&sync_box.sem_empty);

  pthread_mutex_destroy(&robot_state_box.lock);
  sem_destroy(&robot_state_box.sem_full);
  sem_destroy(&robot_state_box.sem_empty);

  printf("Finished. Images saved to ./images/\n");
  return 0;
}

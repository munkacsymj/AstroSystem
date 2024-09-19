#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>

int fd;

void *main_thread(void *arg) {
  char buffer[12];
  while(1) {
    fprintf(stderr, "thread: starting read()\n");
    int ret = read(fd, buffer, 1);
    if (ret > 0) {
      fprintf(stderr, "read() returned %d bytes: %s\n",
	      ret, buffer);
    } else if (ret == 0) {
      fprintf(stderr, "read() returned 0.\n");
    } else {
      fprintf(stderr, "read() error. ");
      perror("read(): ");
    }
  }
  return nullptr;
}

int main(int argc, char **argv) {
  fd = open("/dev/ttyUSB1", O_RDWR);
  if (fd < 0) {
    perror("Cannot open link to CFW.\n");
    exit(-1);
  }

  ////////////////
  // Start the reader thread
  ////////////////
  pthread_t thread;
  int ptid = pthread_create(&thread,
			    nullptr, // attr
			    main_thread,
			    nullptr); // arg to main_thread()

  if (ptid < 0) {
    perror("pthread_create: ");
    exit(-1);
  }
  
  ////////////////
  // Now send some messages
  ////////////////
 int ret = write(fd, "NOW", 3);
 fprintf(stderr, "write() returned %d, expecting 3.\n", ret);

  fprintf(stderr, "starting 30 second sleep.\n");
  sleep(30);

 sleep(2);

 fprintf(stderr, "sending '1'\n");
 ret = write(fd, "1", 1);

 sleep(10);

 fprintf(stderr, "sending '1'\n");
 ret = write(fd, "1", 1);

 sleep(10);

 fprintf(stderr, "sending 'NOW'\n");
 ret = write(fd, "NOW", 3);
 fprintf(stderr, "write() returned %d, expecting 3.\n", ret);

 sleep(2);

 fprintf(stderr, "sending '1'\n");
 ret = write(fd, "1", 1);

 sleep(10);

 fprintf(stderr, "sending 'VRS'\n");
 ret = write(fd, "VRS", 3);
 fprintf(stderr, "write() returned %d, expecting 3.\n", ret);

 sleep(2);

 fprintf(stderr, "sending '2'\n");
 ret = write(fd, "2", 1);

 sleep(10);

  return 0;
}

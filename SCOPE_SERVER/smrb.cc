#include "smrb.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>		// link with -pthread
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>		// exit()
#include <stdio.h>
#include <unistd.h>		// ftruncate()
#include <sys/types.h>
#include <time.h>		// time()

#define NUM_POINTS 256

SMRB::SMRB(SMRB_Initialization_t startup_mode) {
  // Attach to the shared memory area if it already exists; if it
  // doesn't exist, create it. Permissions are set so that all
  // processes must share a common effective user ID.
  int fd;
  smrb_size = sizeof(SMRB_Header_t) + sizeof(DataPoint_t)*NUM_POINTS + 1000;
  
  if (startup_mode == SMRB_INIT_STARTUP) {
    fd = shm_open("/Focuser_SMRB", O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd < 0) {
      perror("Unable to connect to shared memory:");
      exit(-2);
    }

    ftruncate(fd, smrb_size);
  } else {
    fd = shm_open("/Focuser_SMRB", O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0) {
      perror("Unable to connect to shared memory:");
      exit(-2);
    }
  }    

  void *smrb_start = mmap(nullptr,                // no requested address
			  smrb_size,
			  PROT_READ | PROT_WRITE, // access
			  MAP_SHARED,             // shared across processes
			  fd, 0);
  if (smrb_start == nullptr) {
    perror("mmap() failed:");
    exit(-2);
  }

  header = (SMRB_Header_t *) smrb_start;
  // align ring to 8-byte boundary
  ring = (DataPoint_t *) ((uintptr_t)((char *)smrb_start+7) & ~(uintptr_t)0x08);

  int res = close(fd);

  if (startup_mode == SMRB_INIT_STARTUP) {
    
    //********************************
    //        Protection semaphore
    //********************************
    res = sem_init(&header->write_protect_semaphore,
		   1, // shared across processes
		   1); // initialize to unlocked state

    if (res) {
      perror("sem_init(): initialization error: ");
      exit(-2);
    }

    //********************************
    //        Ring setup
    //********************************
    header->num_ring_points = NUM_POINTS;
    header->ring_start = header->ring_next = 0;
  }
}
  
void
SMRB::deep_shutdown(void) {
  int val = shm_unlink("/Focuser_SMRB");

  if (val) {
    perror("Error attempting to unlink SMRB ");
  }

}

SMRB::~SMRB(void) {
  ;
}

unsigned int
SMRB::NumPoints(void) {
  if (header->ring_next < header->ring_start) {
    return header->num_ring_points - (header->ring_start - header->ring_next);
  } else {
    return header->ring_next - header->ring_start;
  }
}

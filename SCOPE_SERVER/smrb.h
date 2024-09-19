/* This may look like C code, but it is really -*-c++-*- */

#ifndef _SMRB_H
#define _SMRB_H

#include <semaphore.h>
#include <stdint.h>

//****************************************************************
//    Class Shared Memory Ring Buffer (SMRB)
// Basic underlying assumption: the shared memory region will be
// mapped to different addresses in each of the different
// executables that attach to the shared memory region.
//****************************************************************
struct SMRB_Header_t {
  unsigned num_ring_points;
  sem_t write_protect_semaphore;
  int ring_start;		// semaphore-protected
  int ring_next;		// semaphore-protected
};

struct DataPoint_t {
  unsigned char data;
};

enum SMRB_Initialization_t {
			    SMRB_INIT_STARTUP,
			    SMRB_INIT_NORMAL,
};

class SMRB {
 public:
  SMRB(SMRB_Initialization_t startup_mode);
  ~SMRB(void);

  // "i" should be incremented with
  // i = (i+1) % num_ring_points;
  DataPoint_t &Get(unsigned int i);

  // returns the number of points currently contained in the ring.
  unsigned int NumPoints(void);

  // Should only be called by the launcher application; this will shut
  // down and deallocate the shared memory
  void deep_shutdown(void);

  time_t RefTime(void) { return header->ref_time; }

 private:
  void *smrb_start;		// start of shared mem in this process
  SMRB_Header_t *header;
  DataPoint_t *ring;	  // This is the first valid point in the ring

  int ring_start; // index of first valid entry
  int ring_next;  // index of next valid valid

  size_t smrb_size;
};

#endif

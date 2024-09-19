#include "arduino_serial_lib.h"
#include <stdio.h>
#include <stdlib.h>		/* exit() */
#include <unistd.h>		/* sleep(), getopt() */
#include <pthread.h>
#include "focus.h"
#include "prb.h"

//#define TEST_MODE

int focus_fd = -1;
static int &c14focuser_fd = focus_fd;

static const char *devname = "/dev/serial/by-id/usb-FTDI_FT231X_USB_UART_DN0402M3-if00-port0";

void InitFocuser(void) {
  c14focuser_fd = serialport_init(devname, 9600);

  if (c14focuser_fd < 0) {
    perror("Cannot open /dev/ttyUSB0 for C14Focuser:");
    return;
  }
}

// Integer status for cc_status field below
#define COMMAND_OK 0
#define COMMAND_ILLFORMED 1
#define COMMAND_MOTORERR 2
#define COMMAND_NONE 3

// Integer values for cc_command below
#define MSG_MOVE 0
#define MSG_QUERY 1
#define MSG_DEVICE_QUERY 2
#define MSG_CURPOS 3
#define MSG_DEVICEID 4

struct Command {
  int cc_command;
  int cc_seq;
  int cc_byte1;
  long cc_word;
  int cc_status;
} command_in, command_out;

#define NODATA -3

// PREFIX codes
#define PREFIX_MSGTYPE 5
#define PREFIX_SEQ 6
#define PREFIX_BYTE 4
#define PREFIX_LONG 2
#define PREFIX_EOM 3

struct SerialByte {
  int prefix;
  int data;
} this_byte;

void ReadByte(PRB &ring) {
  char buffer[8];

  while (ring.NumPoints() == 0) {
    usleep(10000);
  }
  unsigned int r = ring.PopData();

  int one_byte = r;
  this_byte.prefix = (one_byte & 0xf0) >> 4;
  this_byte.data = (one_byte & 0x0f);

  fprintf(stderr, "    ReadByte -> 0x%02x, 0x%02x\n",
	  this_byte.prefix, this_byte.data);
}

void ReadCommand(PRB &ring) {
  ReadByte(ring);
  if (this_byte.prefix == NODATA) {
    command_in.cc_status = COMMAND_NONE;
    return;
  }
  if (this_byte.prefix != PREFIX_MSGTYPE) {
    command_in.cc_status = COMMAND_ILLFORMED;
    return;
  }
  command_in.cc_command = this_byte.data;
  if (command_in.cc_command != MSG_CURPOS &&
      command_in.cc_command != MSG_DEVICEID) {
    command_in.cc_status = COMMAND_ILLFORMED;
    return;
  }

  // Fetch the sequence number and the "one-byte"
  ReadByte(ring);
  if (this_byte.prefix != PREFIX_SEQ) {
    command_in.cc_status = COMMAND_ILLFORMED;
    return;
  }
  command_in.cc_seq = this_byte.data;
  ReadByte(ring);
  if (this_byte.prefix != PREFIX_BYTE) {
    command_in.cc_status = COMMAND_ILLFORMED;
    return;
  }
  command_in.cc_byte1 = this_byte.data;

  if (command_in.cc_command == MSG_CURPOS) {
    unsigned long fullword = 0;
    for (int i=0; i<4; i++) {
      ReadByte(ring);
      fullword = (fullword*16) + this_byte.data;
    }
    command_in.cc_word = (int16_t) fullword;
  }

  ReadByte(ring);
  if (this_byte.prefix != PREFIX_EOM) {
    command_in.cc_status = COMMAND_ILLFORMED;
  } else {
    command_in.cc_status = COMMAND_OK;
  }
}

void PackByte(unsigned int *dest, int prefix, int data) {
  *dest = (prefix << 4) + data;
}

void PackWord(unsigned int *dest, long value) {
  int nibbles[4];
  nibbles[0] = (value & 0xf000) >> 12;
  nibbles[1] = (value & 0x0f00) >> 8;
  nibbles[2] = (value & 0x00f0) >> 4;
  nibbles[3] = (value & 0x000f);
  for (int i=0; i<4; i++) {
    PackByte(dest+i, PREFIX_LONG, nibbles[i]);
  }
}

FILE *sender_log = nullptr;

void SendMessage(void) {
  unsigned int msg[16];
  int msg_length;

  PackByte(msg+0, PREFIX_MSGTYPE, command_out.cc_command);
  PackByte(msg+1, PREFIX_SEQ, command_out.cc_seq);
  PackByte(msg+2, PREFIX_BYTE, command_out.cc_byte1);

  if (command_out.cc_command == MSG_MOVE) {
    PackWord(msg+3, command_out.cc_word);
    PackByte(msg+7, PREFIX_EOM, PREFIX_EOM);
    msg_length = 8;
  } else {
    PackByte(msg+3, PREFIX_EOM, PREFIX_EOM);
    msg_length = 4;
  }
  
  for (int i=0; i<msg_length; i++) {
    serialport_writebyte(c14focuser_fd, msg[i]);
    fprintf(sender_log, "0x%02x ", msg[i]);
  }
  fprintf(sender_log, "\n");
  fflush(sender_log);
}

void PrintResponse(void) {
  fprintf(stderr, "Response:\n");
  if (command_in.cc_status != COMMAND_OK) {
    fprintf(stderr, "    cc_status = %d\n",
	    command_in.cc_status);
    return;
  }

  fprintf(stderr, "    cc_command = 0x%02x\n", command_in.cc_command);
  fprintf(stderr, "    cc_seq     = 0x%02x\n", command_in.cc_seq);
  fprintf(stderr, "    cc_byte1   = 0x%02x\n", command_in.cc_byte1);
  if (command_in.cc_command == MSG_CURPOS) {
    fprintf(stderr, "    cc_word    = %ld\n", command_in.cc_word);
  }
}

static void *ListenerThread(void *arg) {
  PRB *ring = (PRB *) arg;
  FILE *listener_log = fopen("/tmp/Listener.txt", "w");
  if (!listener_log) {
    fprintf(stderr, "c14focuser: ListenerThread: Cannot create logfile.\n");
    return nullptr;
  }
  while(1) {
    char buffer[8];
    int r = serialport_read_until(c14focuser_fd, buffer, 0xff, 1, 100/*msec*/);
    if (r == -1) {
      fprintf(stderr, "c14focuser: read from Arduino failed.\n");
      return nullptr;
    } else if (r == -2) {
      // normal timeout: do nothing
      fprintf(stderr, "-");
    } else {
      ring->AddNewData(buffer[0]);
      fprintf(listener_log, "0x%02x ", buffer[0]);
      if (buffer[0] == 0x33) fprintf(listener_log, "\n");
      fflush(listener_log);
      fprintf(stderr, "X");
    }
  }
  fclose(listener_log);
}

#ifdef TEST_MODE

void Test1(PRB &ring) {
  Command &t1 = command_out;

  t1.cc_command = MSG_DEVICE_QUERY;
  t1.cc_seq = 4;
  t1.cc_byte1 = 0;
  t1.cc_word = 0;
  t1.cc_status = COMMAND_OK;

  fprintf(stderr, "Test1 started.\n");

  //sleep(3);

  fprintf(stderr, "Test1: sending message to Arduino.\n");
  SendMessage();

  //fprintf(stderr, "Test1: starting terminal sleep.\n");
  //sleep(5);

  fprintf(stderr, "Test1: reading response from Arduino.\n");
  ReadCommand(ring);
  PrintResponse();
  if (command_in.cc_command == MSG_DEVICEID &&
      command_in.cc_byte1 == 7 &&
      command_in.cc_seq == t1.cc_seq) {
    fprintf(stderr, "Test1: passed.\n\n");
  } else {
    fprintf(stderr, "Test1: failed.\n\n");
  }
}

void Test2(PRB &ring) {
  Command &t1 = command_out;

  t1.cc_command = MSG_QUERY;
  t1.cc_seq = 5;
  t1.cc_byte1 = 0;
  t1.cc_word = 0;
  t1.cc_status = COMMAND_OK;

  fprintf(stderr, "Test2 started.\n");

  fprintf(stderr, "Test2: sending message to Arduino.\n");
  SendMessage();

  fprintf(stderr, "Test2: reading response from Arduino.\n");
  ReadCommand(ring);
  PrintResponse();
  if (command_in.cc_command == MSG_CURPOS &&
      (command_in.cc_byte1 == 0 || command_in.cc_byte1 == 1) &&
      command_in.cc_seq == t1.cc_seq) {
    fprintf(stderr, "Test2: passed.\n\n");
  } else {
    fprintf(stderr, "Test2: failed.\n\n");
  }
}

void Test3(PRB &ring) {
  Command &t1 = command_out;

  t1.cc_command = MSG_MOVE;
  t1.cc_seq = 6;
  t1.cc_byte1 = 1;
  t1.cc_word = 1000;
  t1.cc_status = COMMAND_OK;

  fprintf(stderr, "Test3 started.\n");

  fprintf(stderr, "Test3: sending message to Arduino.\n");
  SendMessage();

  fprintf(stderr, "Test3: reading response from Arduino.\n");
  ReadCommand(ring);
  PrintResponse();
  if (command_in.cc_command == MSG_CURPOS &&
      (command_in.cc_byte1 == 0 || command_in.cc_byte1 == 1) &&
      command_in.cc_seq == t1.cc_seq) {
    fprintf(stderr, "Test3: passed.\n\n");
  } else {
    fprintf(stderr, "Test3: failed.\n\n");
  }
}

int main(int argc, char **argv) {
  PRB shared_memory(24);
  pthread_t listener_thread;
  sender_log = fopen("/tmp/sender.txt", "w");
  InitFocuser();
  int err = pthread_create(&listener_thread,
			   nullptr,
			   ListenerThread,
			   &shared_memory);
  
  sleep(4);
  Test1(shared_memory);
  Test2(shared_memory);
  Test3(shared_memory);
  fclose(sender_log);
  fprintf(stderr, "Ring buffer now holds %d bytes.\n", shared_memory.NumPoints());
  return 0;
}

#else // NOT test mode

static int initialized = 0;
static PRB ring(24);

void get_focus_encoder(void);

void initialize_focuser(void) {
  pthread_t listener_thread;
  InitFocuser();
  sleep(2);
  int err = pthread_create(&listener_thread,
			   nullptr,
			   ListenerThread,
			   &ring);
  get_focus_encoder();
}

static long NetFocusPosition = 0; /* encoder value */
int next_command_seq = 4;

void get_focus_encoder(void) {
  if (!initialized) {
    initialized = 1;
    initialize_focuser();
  }

  command_out.cc_command = MSG_QUERY;
  command_out.cc_seq     = next_command_seq;
  next_command_seq = (next_command_seq+1) % 16;
  command_out.cc_byte1 = 0;
  command_out.cc_word = 0;
  command_out.cc_status = COMMAND_OK;

  SendMessage();
  ReadCommand(ring);
  if (command_in.cc_command == MSG_CURPOS &&
      (command_in.cc_byte1 == 0 || command_in.cc_byte1 == 1) &&
      command_in.cc_seq == command_out.cc_seq) {

    NetFocusPosition = (int16_t) command_in.cc_word;

    fprintf(stderr,
	    "c14focuser: focuser position = %ld\n",
	    (long) NetFocusPosition);
  } else {
    fprintf(stderr, "c14focuser: invalid response to QUERY command:\n");
    PrintResponse();
  }
}

void c14focus(int direction, unsigned long duration) {
  if (!initialized) {
    initialized = 1;
    initialize_focuser();
  }

  long desired_position;
  if (direction == NO_DIRECTION_MOVE_ABSOLUTE) {
    desired_position = duration;
  } else {
    desired_position = NetFocusPosition +
      ((direction == DIRECTION_IN) ? -duration : duration);
  }

  const int delta = abs(desired_position - NetFocusPosition);
  const int direction_code = (desired_position < NetFocusPosition);

  command_out.cc_command = MSG_MOVE;
  command_out.cc_seq = next_command_seq;
  next_command_seq = (next_command_seq+1) % 16;
  command_out.cc_byte1 = direction_code;
  command_out.cc_word = delta;
  command_out.cc_status = COMMAND_OK;

  SendMessage();
  ReadCommand(ring);		// possibly blocks for a long time

  if (command_in.cc_command == MSG_CURPOS &&
      (command_in.cc_byte1 == 0 || command_in.cc_byte1 == 1) &&
      command_in.cc_seq == command_out.cc_seq) {
    NetFocusPosition = (int16_t) command_in.cc_word;
  } else {
    fprintf(stderr, "c14focuser: invalid response to MOVE command:\n");
    PrintResponse();
  }
}

void c14focus_move(int direction,
		unsigned long total_duration,
		unsigned long step_size) {
  int number_of_steps = total_duration/step_size;

  while(number_of_steps-- > 0) {
    c14focus(direction, step_size);
    sleep(2);
  }
}
  
long c14cum_focus_position(void) {
  return NetFocusPosition;
}

#endif


#include "arduino_serial_lib.h"
#include <stdio.h>
#include <stdlib.h>		/* exit() */
#include <unistd.h>		/* sleep(), getopt() */
#include <string.h>
#include <pthread.h>
#include <string>
#include "focus.h"
#include "json.h"

#define TEST_MODE
#define MAX_RESPONSE_SIZE 8192

#define JSON_SET 1
#define JSON_GET 0
#define JSON_CMD 2

char *BuildJSONCommand(int cmd_type,
		       const char *attribute_string,
		       const char *value_string = nullptr);
void get_focus_encoder(void);

// Protected Ring Buffer
class PRB {
public:
  PRB(void);
  ~PRB(void);

  unsigned int buflen;

  // "i" should be incremented with
  // i = (i+1) % this->buflen;
  unsigned int &Get(unsigned int i) { return buffer[i]; }

  unsigned int NumPoints(void);

  void AddNewData(unsigned int value);
  unsigned int PopData(void);

private:
  unsigned int *buffer;
  unsigned int ring_start; // index of first valid entry
  unsigned int ring_next; // index of next valid value
  //sem_t write_protect_semaphore;
};

PRB::PRB(void) : buflen(8192), ring_start(0), ring_next(0), buffer(new unsigned int[8192]) {
#if 0
  int res = sem_init(&write_protect_semaphore,
		     0, // shared across threads
		     1); // initialized to unlocked state
  if (res) {
    perror("sem_init(): initialization error: ");
    exit(-2);
  }
#endif
  ;
}

PRB::~PRB(void) {
  delete [] buffer;
#if 0
  sem_destroy(&write_protect_semaphore);
#endif
}

unsigned int PRB::NumPoints(void) {
  if (ring_start > ring_next) {
    return buflen - (ring_start - ring_next);
  } else {
    return ring_next - ring_start;
  }
}

void PRB::AddNewData(unsigned int value) {
  buffer[ring_next] = value;
  ring_next = (ring_next+1)%buflen;
}

#define PRB_EMPTY 0xffff;
unsigned int PRB::PopData(void) {
  if (ring_start == ring_next) return PRB_EMPTY;
  const unsigned int value = buffer[ring_start];
  ring_start = (ring_start+1)%buflen;
  return value;
}
				   

//****************************************************************
//        end of PRB
//****************************************************************

int focus_fd = -1;
static int &esattofocuser_fd = focus_fd;
static PRB *shared_prb = nullptr;

const char *devname = "/dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_7ac95f39d1b7e8119fe06e2bcb5e5982-if00-port0";

void InitFocuser(void) {
  esattofocuser_fd = serialport_init(devname, 115200);

  if (esattofocuser_fd < 0) {
    perror("Cannot open /dev/serial/by-id for ESATTOFocuser:");
    return;
  }
}

char ReadByte(PRB *ring) {
  char buffer[8];

  while (ring->NumPoints() == 0) {
    usleep(10000);
  }
  unsigned int r = ring->PopData();

  return (char) r;
}

FILE *sender_log = nullptr;

const char *SendMessage(const char *command) {
  const int len = strlen(command);
  int write_res = write(esattofocuser_fd, command, len);
  if (write_res != len) {
    fprintf(stderr, "SendMessage[esatto] fail: %d bytes tried, %d bytes written\n",
	    len, write_res);
    return nullptr;
  }

  char buffer[MAX_RESPONSE_SIZE];
  int bracket_count = 0;
  bool in_quote = false;
  char *dest = buffer;

  do {
    char c = ReadByte(shared_prb);
    if (not in_quote) {
      if (c == '{') bracket_count++;
      if (c == '}') bracket_count--;
    }
    if (c != '\n' or in_quote) {
      *dest++ = c;
    }
    if (c == '"') in_quote = not in_quote;
  } while(bracket_count > 0);
  *dest = 0;

  fprintf(stderr, "ESATTO sent response: %s\n", buffer);
  int leftovers = shared_prb->NumPoints();
  if (leftovers) {
    fprintf(stderr, "still have %d chars left in buffer. Flushing.\n",
	    leftovers);
    while(shared_prb->NumPoints()) {
      char c = ReadByte(shared_prb);
    }
  }
    
  return strdup(buffer);
}

static void *ListenerThread(void *arg) {
  PRB *ring = (PRB *) arg;
  FILE *listener_log = fopen("/tmp/Listener.txt", "w");
  if (!listener_log) {
    fprintf(stderr, "esattofocuser: ListenerThread: Cannot create logfile.\n");
    return nullptr;
  }
  while(1) {
    char buffer[8];
    int r = serialport_read_until(esattofocuser_fd, buffer, 0xff, 1, 100/*msec*/);
    if (r == -1) {
      fprintf(stderr, "esattofocuser: read from USB failed.\n");
      return nullptr;
    } else if (r == -2) {
      // normal timeout: do nothing
      fprintf(stderr, "-");
    } else {
      ring->AddNewData(buffer[0]);
      fprintf(listener_log, "0x%02x ", buffer[0]);
      fflush(listener_log);
      fprintf(stderr, "X");
    }
  }
  fclose(listener_log);
}

#ifdef TEST_MODE

void Test1(PRB &ring, const char *command) {
  fprintf(stderr, "Test1 started.\n");

  //sleep(3);


  //const char *test1_msg = "{\"req\":{\"get\": \"\"}}";
  //const char *test1_msg = "{\"req\":{\"get\": {\"WIFIAP\":{\"IP\":\"\"}}}}";

  fprintf(stderr, "Test1: sending message to Esatto: '%s'\n", command);
  const char *response = SendMessage(command);

  fprintf(stderr, "response = %s\n", response);
  const JSON_Expression j(response);

  //j.Print(stderr);
  const JSON_Expression *busy = j.GetValue("res.get.MOT1.POSITION");
  
  fprintf(stderr, "position = %lf\n", busy->Value_double());
}

void Test2(PRB &ring) {
  fprintf(stderr, "Test2 started.\n");

  //sleep(3);

  char target[12];
  sprintf(target, "%d", 190000);
  const char *result = BuildJSONCommand(JSON_CMD, "MOT1.GOTO", target);
  fprintf(stderr, "Test2: sending message to Esatto: '%s'\n", result);

  const char *response = SendMessage(result);
  free((void *)result);

  fprintf(stderr, "response = %s\n", response);

  const char *query = BuildJSONCommand(JSON_GET, "MOT1.POSITION");
  do {
    const char *query_resp = SendMessage(query);

    const JSON_Expression j(query_resp);
    const JSON_Expression *busy = j.GetValue("res.get.MOT1.STATUS.BUSY");
    if (!busy) {
      fprintf(stderr, "ERROR: unable to find BUSY field.\n");
      break;
    }
    int busy_val = busy->Value_int();
    if (busy_val) {
      sleep(1);
    } else {
      break;
    }
    free((void *) query_resp);
  } while(1);
  fprintf(stderr, "Test2: No longer busy.\n");
}

int main(int argc, char **argv) {
  PRB shared_memory;
  shared_prb = &shared_memory;
  
  pthread_t listener_thread;
  sender_log = fopen("/tmp/sender.txt", "w");
  InitFocuser();
  int err = pthread_create(&listener_thread,
			   nullptr,
			   ListenerThread,
			   &shared_memory);
  
  const char *result = BuildJSONCommand(false, "MOT1.POSITION");
  fprintf(stderr, "command result = '%s'\n", result);
  //sleep(1);
  Test1(shared_memory, result);
  Test2(shared_memory);
  get_focus_encoder();
  fclose(sender_log);
  fprintf(stderr, "Ring buffer now holds %d bytes.\n", shared_memory.NumPoints());
  while(shared_memory.NumPoints()) {
    char c = ReadByte(shared_prb);
    fprintf(stderr, "char = 0x%02x: '%c'\n", c, c);
  }
  return 0;
}

//#else // NOT test mode

static int initialized = 0;
static PRB ring;

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

std::string AttributeToJSON(const char *dot_string, const char *value_string) {
  const char *s = dot_string;
  while(*s and *s != '.') s++;
  char name[1+(s-dot_string)];
  strncpy(name, dot_string, (s-dot_string));
  name[s-dot_string]=0;
  std::string ending {"\"\""};
  if (*s == '.') {
    ending = AttributeToJSON(s+1, value_string);
  } else if(value_string) {
    ending = std::string(value_string);
  }
  return std::string("{\"") + name + "\":" + ending + "}";
}

char *BuildJSONCommand(int cmd_type,
		       const char *attribute_string,
		       const char *value_string) {
  std::string cmd_start { "{\"req\":" };
  switch(cmd_type) {
  case(JSON_SET):
    cmd_start += "{\"set\":";
    break;

  case(JSON_GET):
    cmd_start += "{\"get\":";
    break;

  case(JSON_CMD):
    cmd_start += "{\"cmd\":";
    break;

  default:
    fprintf(stderr, "ERROR: BuildJSONCommand: bad type: %d\n", cmd_type);
    return nullptr;
  }
    
  cmd_start += AttributeToJSON(attribute_string, value_string);
  cmd_start += "}}";
  return strdup(cmd_start.c_str());
}

void get_focus_encoder(void) {
#if 0
  if (!initialized) {
    initialized = 1;
    initialize_focuser();
  }
#endif

  const char *query_msg = "{\"req\":{\"get\":{\"MOT1\":{\"POSITION\":\"\"}}}}";
  const char *response = SendMessage(query_msg);
}

void focus(int direction, unsigned long duration) {
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

}

void focus_move(int direction,
		unsigned long total_duration,
		unsigned long step_size) {
  int number_of_steps = total_duration/step_size;

  while(number_of_steps-- > 0) {
    focus(direction, step_size);
    sleep(2);
  }
}
  
long cum_focus_position(void) {
  return NetFocusPosition;
}

#endif


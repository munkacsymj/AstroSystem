#include "arduino_serial_lib.h"
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>
#include <stdio.h>
#include <stdlib.h>		/* exit() */
#include <unistd.h>		/* sleep(), getopt() */
#include <string.h>
#include <pthread.h>
#include <string>
#include <time.h>
#include "focus.h"
#include "json.h"
#include "prb.h"

#define MAX_RESPONSE_SIZE 8192

#define JSON_SET 1
#define JSON_GET 0
#define JSON_CMD 2

char *BuildJSONCommand(int cmd_type,
		       const char *attribute_string,
		       const char *value_string = nullptr);
static void get_focus_encoder(void);
static void InitFocuser(void);
static void ResetFocuserIO(void);

static int focus_fd = -1;
static int &esattofocuser_fd = focus_fd;
static PRB *shared_prb = nullptr;

static int initialized = 0;
static PRB ring(MAX_RESPONSE_SIZE);
static long NetFocusPosition = 0; /* encoder value */
static pthread_t listener_thread;

static const char *devname = "/dev/serial/by-id/usb-Silicon_Labs_CP2102N_USB_to_UART_Bridge_Controller_7ac95f39d1b7e8119fe06e2bcb5e5982-if00-port0";

static const char *GetTimeASCII(void) {
  static char buffer[80];
  const time_t now = time(0);
  strcpy(buffer, ctime(&now));
  buffer[strlen(buffer)-1] = '\0';
  return buffer;
}

static int ReadByte(PRB *ring) {
  char buffer[8];
  // ReadByte() checks for a character every 0.01 seconds. We wait for
  // at most one minute before we give up.
  int timeout = 60*100; // 60 secs at 0.01 sec/cycle.

  while (ring->NumPoints() == 0) {
    usleep(10000);
    if (timeout-- <= 0) {
      fprintf(stderr, "esatto: ReadByte() timeout. Returning EOF.\n");
      // oops. Timed out. Return EOF.
      return EOF;
    }
  }
  unsigned int r = ring->PopData();

  return (char) r;
}

static FILE *sender_log = nullptr;

static const char *SendMessage(const char *command) {
  int retries = 0;
  bool failed = false;

 failure_restart:
  do {
    const int len = strlen(command);
    fprintf(stderr, "Sending command to Esatto: %s\n", command);
    int write_res = write(esattofocuser_fd, command, len);
    if (write_res != len) {
      fprintf(stderr, "SendMessage[esatto] fail: %d bytes tried, %d bytes written\n",
	      len, write_res);
      failed == true;
      ResetFocuserIO();
      retries++;
    } else {
      failed = false;
      retries = 0;
    }
  } while (failed and retries < 3);
  if (failed) {
    fprintf(stderr, "SendMessage[esatto]: giving up. Too many failures.\n");
    return nullptr;
  }

  char buffer[MAX_RESPONSE_SIZE];
  int bracket_count = 0;
  bool in_quote = false;
  char *dest = buffer;

  do {
    int c = ReadByte(shared_prb);
    if (c == EOF) {
      // ERROR: ReadByte timed out
      failed = true;
      ResetFocuserIO();
      retries++;
      goto failure_restart;
    }
    if (not in_quote) {
      if (c == '{') bracket_count++;
      if (c == '}') bracket_count--;
    }
    if ((c != '\n' and c != 0) or in_quote) {
      *dest++ = c;
    }
    if (c == '"') in_quote = not in_quote;
  } while(bracket_count > 0);
  *dest = 0;

  fprintf(stderr, "ESATTO sent response: %s\n", buffer);
#if 0 // this is the old way -- weren't sure how many leftovers...
  int leftovers = shared_prb->NumPoints();
  if (leftovers) {
    fprintf(stderr, "still have %d chars left in buffer. Flushing.\n",
    	    leftovers);
    while(shared_prb->NumPoints()) {
      char c = ReadByte(shared_prb);
    }
  }
#else // this is the new way -- ALWAYS read two leftovers...
  {
    char c = ReadByte(shared_prb);
    if (c != 0x0a and c != 0x0d) {
      fprintf(stderr, "ESATTO: leftover[0] not LF: 0x%02x\n", c);
    }
    char c1 = ReadByte(shared_prb);
    if (c1 != 0x0a and c1 != 0x0d) {
      fprintf(stderr, "ESATTO: leftover[1] not LF: 0x%02x\n", c1);
    }
  }
#endif
  
  return strdup(buffer); // could be an empty string if timed out.
}

static void *ListenerThread(void *arg) {
  PRB *ring = (PRB *) arg;
  FILE *listener_log = fopen("/tmp/Listener_esatto.txt", "w");
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
      //fprintf(stderr, "-");
      ;
    } else {
      ring->AddNewData(buffer[0]);
      //fprintf(listener_log, "0x%02x ", buffer[0]);
      //fflush(listener_log);
      //fprintf(stderr, "X");
    }
  }
  fclose(listener_log);
}

static void GetFullStatus(void) {
  const char *cmd = "{\"req\":{\"get\":\"\"}}";
  const char *response = SendMessage(cmd);
  fprintf(stderr, "Full status response = %s\n", response);
}

static void initialize_focuser(void) {
  fprintf(stderr, "%s esatto: initialize_focuser()\n", GetTimeASCII());
  InitFocuser();
  sleep(2);
  int err = pthread_create(&listener_thread,
			   nullptr,
			   ListenerThread,
			   &ring);
  get_focus_encoder();
  fprintf(stderr, "%s esatto: NetFocusPosition = %ld\n",
	  GetTimeASCII(), NetFocusPosition);
  GetFullStatus();
}

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

void get_focus_encoder(void) {
  if (!initialized) {
    initialized = 1;
    initialize_focuser();
  }

  const char *query_msg = BuildJSONCommand(JSON_GET, "MOT1.POSITION");
  const char *response = SendMessage(query_msg);
  const JSON_Expression j(response);
  const JSON_Expression *position_value = j.GetValue("res.get.MOT1.POSITION");
  if (!position_value) {
    fprintf(stderr, "ERROR: unable to find POSITION field.\n");
    return;
  }
  long pos_value = position_value->Value_int();
  NetFocusPosition = pos_value;

  free((void *)query_msg);
  free((void *)response);
}

static void ResetFocuserIO(void) {
  fprintf(stderr, "%s Initiating ESATTO I/O port reset. Closing old fd.\n",
	  GetTimeASCII());
  pthread_cancel(listener_thread);
  shared_prb->Reset();
  serialport_close(esattofocuser_fd);
  fprintf(stderr, "%s Close() completed. Re-opening.\n",
	  GetTimeASCII());
  sleep(3);
  initialize_focuser();		// this will create new pthread
  fprintf(stderr, "%s ResetFocuserIO() completed.\n", GetTimeASCII());

#if 0
  fprintf(stderr, "ResetFocuserIO() issuing DEV_RESET command.\n");
  int rc = ioctl(esattofocuser_fd, USBDEVFS_RESET, 0);
  if (rc < 0) {
    perror("Error in ioctl.");
  } else {
    fprintf(stderr, "Reset successful.\n");
  }
#endif
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

static void InitFocuser(void) {
  shared_prb = &ring;

  esattofocuser_fd = serialport_init(devname, 115200);

  if (esattofocuser_fd < 0) {
    perror("Cannot open /dev/serial/by-id for ESATTOFocuser:");
    return;
  }
}

#ifdef TEST_MODE

void Test1(PRB &ring, const char *command) {
  fprintf(stderr, "Test1 started.\n");

  //sleep(3);


  const char *test1_msg = "{\"req\":{\"get\": \"\"}}";
  //const char *test1_msg = "{\"req\":{\"get\": {\"WIFIAP\":{\"IP\":\"\"}}}}";
  command = test1_msg;

  fprintf(stderr, "Test1: sending message to Esatto: '%s'\n", command);
  const char *response = SendMessage(command);

  fprintf(stderr, "response = %s\n", response);
  const JSON_Expression j(response);

  //j.Print(stderr);
  const JSON_Expression *busy = j.GetValue("res.get.MOT1.POSITION");
  
  fprintf(stderr, "position = %lf\n", busy->Value_int());
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
  PRB shared_memory(MAX_RESPONSE_SIZE);
  shared_prb = &shared_memory;
  
  pthread_t listener_thread;
  sender_log = fopen("/tmp/sender.txt", "w");
  InitFocuser();
  int err = pthread_create(&listener_thread,
			   nullptr,
			   ListenerThread,
			   &shared_memory);
  
  const char *result = BuildJSONCommand(false, "MOT1.POSITION");
  //fprintf(stderr, "command result = '%s'\n", result);
  //sleep(1);
  Test1(shared_memory, result);
  return 0;
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

#else // NOT test mode

void esattofocus(int direction, unsigned long duration) {
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

  if (desired_position < 0) {
    desired_position = 0;
  }
  if (desired_position > 439000) {
    desired_position = 439000;
  }

  char target[12];
  sprintf(target, "%ld", desired_position);
  const char *goto_cmd = BuildJSONCommand(JSON_CMD, "MOT1.GOTO", target);
  const char *goto_response = SendMessage(goto_cmd);

  free((void *)goto_cmd);
  free((void *)goto_response);

  // Wait for focuser
  const char *query = BuildJSONCommand(JSON_GET, "MOT1.POSITION");
  long last_ticks = -1;
  int number_no_change = 0;
  
  do {
    const char *query_resp = SendMessage(query);

    const JSON_Expression j(query_resp);
#ifdef OLD_ESATTO
    const JSON_Expression *busy = j.GetValue("res.get.MOT1.STATUS.BUSY");
    if (!busy) {
      fprintf(stderr, "ERROR: unable to find BUSY field.\n");
      break;
    }
    int busy_val = busy->Value_int();
    if (busy_val) {
      usleep(500000); // 1/2 second
    } else {
      break;
    }
#else
    const JSON_Expression *loc = j.GetValue("res.get.MOT1.POSITION");
    long cur_ticks = loc->Value_int();
    if (cur_ticks != last_ticks and labs(cur_ticks - desired_position) < 15) {
      break;
    } else {
      if (cur_ticks == last_ticks) {
	if (++number_no_change > 5) {
	  break;
	}
      }
      last_ticks = cur_ticks;
      usleep(500000); // 1/2 second
    }
#endif
    free((void *) query_resp);
  } while(1);
  get_focus_encoder();
}

long esattocum_focus_position(void) {
  return NetFocusPosition;
}


#endif


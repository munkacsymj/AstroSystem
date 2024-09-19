#include "arduino_serial_lib.h"
#include <stdio.h>
#include <stdlib.h>		/* exit() */
#include <unistd.h>		/* sleep(), getopt() */

#define CMD_STATUS 0xA0
#define CMD_MOVE_UP 0xA1
#define CMD_MOVE_DOWN 0xA2
#define CMD_HALT 0xA3

static int flatlight_fd = -1;

const char *devname = "/dev/serial/by-id/usb-FTDI_FT231X_USB_UART_DN0402M3-if00-port0";

void InitFlatLight(void) {
  flatlight_fd = serialport_init(devname, 9600);

  if (flatlight_fd < 0) {
    perror("Cannot open /dev/ttyUSB0 for FlatLight:");
    return;
  }

  // finite number of tries to get a status byte
  for (int i=0; i<10; i++) {
    char buffer[8];
    serialport_writebyte(flatlight_fd, CMD_STATUS);
    unsigned int r = serialport_read_until(flatlight_fd, buffer, 0xaa, 1, 100 /*msec*/);
    if (r == -1) {
      fprintf(stderr, "Cannot read.\n");
      return;
    } else if (r == -2) {
      /* fprintf(stderr, "timeout.\n"); */
      ; // ignore for now
    } else if (r == 0) {
      break;
    }
    sleep(1);
  } // end for finite number of tries
}

void FlatLightMoveUp(void) {
  serialport_writebyte(flatlight_fd, CMD_MOVE_UP);
  fprintf(stderr, "Send move up command.\n");
}
void FlatLightMoveDown(void) {
  serialport_writebyte(flatlight_fd, CMD_MOVE_DOWN);
  fprintf(stderr, "Send move down command.\n");
}

unsigned char GetFlatLightStatusByte(void) {
  // finite number of tries to get a status byte
  for (int i=0; i<10; i++) {
    char buffer[8];
    serialport_writebyte(flatlight_fd, CMD_STATUS);
    unsigned int r = serialport_read_until(flatlight_fd, buffer, 0xaa, 1, 100 /*msec*/);
    if (r == -1) {
      fprintf(stderr, "Cannot read.\n");
      return 0;
    } else if (r == -2) {
      /* fprintf(stderr, "timeout.\n"); */
      ; // ignore for now
    } else if (r == 0) {
      return ~(0x0f & buffer[0]);
    }
    sleep(1);
  } // end for finite number of tries
  return 0;
}




/*  cooler.cc -- Program to control the camera cooler
 *
 *  Copyright (C) 2007, 2017 Mark J. Munkacsy

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program (file: COPYING).  If not, see
 *   <http://www.gnu.org/licenses/>. 
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>		// sleep()
#include <time.h>		// time()
#include <algorithm>		// min()
#include <camera_api.h>

//
// Invocation:
//    cooler startup [-n]
//    cooler shutdown
//    cooler adjust
//    cooler hold
//    cooler -t xx.x (temp in DegC)
//    cooler -p 0.43 (power 0 -> 1.0)
//

void usage(void) {
  fprintf(stderr,
	  "Usage: cooler [startup|shutdown|hold|adjust]|[-t -xx.x | -p 0.xx -> 1.00]\n");
  exit(-2);
}

#define CMD_STARTUP  1
#define CMD_SHUTDOWN 2
#define CMD_TEMP     3
#define CMD_POWER    4
#define CMD_ADJUST   5
#define CMD_HOLD     6

void do_startup(bool perform_adjust);
void do_shutdown(void);
void do_adjust(void);
void do_hold(void);

int main(int argc, char **argv) {
  int command = 0;
  bool perform_adjust = true;

  if(argc < 2) usage();
  if(argc > 3) usage();

  if(argc == 2) {
    if(strcmp(argv[1], "startup") == 0) command = CMD_STARTUP;
    if(strcmp(argv[1], "shutdown") == 0) command = CMD_SHUTDOWN;
    if(strcmp(argv[1], "adjust") == 0) command = CMD_ADJUST;
    if(strcmp(argv[1], "hold") == 0) command = CMD_HOLD;

    if(!command) usage();
  } else {
    if(strcmp(argv[1], "startup") == 0) {
      command = CMD_STARTUP;
      if (strcmp(argv[2], "-n") == 0) {
	perform_adjust = false;
      } else {
	usage();
      }
    }

    if(strcmp(argv[1], "-t") == 0) command = CMD_TEMP;
    if(strcmp(argv[1], "-p") == 0) command = CMD_POWER;

    if(!command) usage();
  }

  double cmd_temp;
  double cmd_power;
  CoolerCommand outbound_cmd;

  switch (command) {
  case CMD_TEMP:
    if(sscanf(argv[2], "%lf", &cmd_temp) != 1) usage();
    if(cmd_temp > 0.0 && argv[2][0] != '+') {
      fprintf(stderr, "cooler: must use + to set positive temperature\n");
      exit(-21);
    }
    outbound_cmd.SetCoolerSetpoint(cmd_temp);
    outbound_cmd.Send();
    break;

  case CMD_POWER:
    if(sscanf(argv[2], "%lf", &cmd_power) != 1) usage();
    if(cmd_power < 0.0 || cmd_power > 1.0) {
      fprintf(stderr, "cooler: %f not in range of 0..1\n", cmd_power);
    } else {
      outbound_cmd.SetCoolerManual(cmd_power);
      outbound_cmd.Send();
    }
    break;

  case CMD_ADJUST:
    do_adjust();
    break;

  case CMD_HOLD:
    do_hold();
    break;

  case CMD_SHUTDOWN:
    do_shutdown();
    break;

  case CMD_STARTUP:
    do_startup(perform_adjust);
    break;
  }
  return 0;
}
    
void do_startup(bool perform_adjust) {
  // 5 stepts:
  // Step 1: ramp to 100% over 8 minutes
  // Step 2: Wait 1 min at 100%
  // Step 3: Make ordered temp equal the actual temp
  // Step 4: Wait 1 min
  // Step 5: Target power level is 92%. Calculate delta_T to adjust power
  //           to get to 92%. Change ordered delta_T.
  double ambient_t;
  double ccd_t;
  double setpoint_t;
  int initial_power;
  int initial_mode;
  double humidity;

  if(CCD_cooler_data(&ambient_t,
		     &ccd_t,
		     &setpoint_t,
		     &initial_power,
		     &humidity,
		     &initial_mode) == 0) {
    fprintf(stderr, "do_startup: cannot get cooler data.\n");
    return;
  }

  CoolerCommand cmd;

#if 1
  ////////////////////////////////
  //    STEP 1                  //
  ////////////////////////////////
  int i = (int) (initial_power);
  while(i < 100) {
    i++;
    fprintf(stderr, "P = %d%%\n", i);
    cmd.SetCoolerManual((double) i / 100.0);
    cmd.Send();
    sleep(5);			// 8 min = 480 sec, close to 500 sec
  }
#else
  ////////////////////////////////
  //    ALTERNATE STEP 1
  ////////////////////////////////
  time_t start_time = time(nullptr);
  double current_setpoint = ccd_t-0.3;
  int last_power = -1;
  cmd.SetCoolerSetpoint(current_setpoint);
  cmd.Send();
  fprintf(stderr, "time, ccd_t, setpoint, power, target power\n");
  bool in_hold = false;
  int hold_count = 0;
  
  while(1) {
    sleep(2);
    time_t now = time(nullptr);
    time_t delta_t = (now - start_time);
    double target_power = std::min(100.0, delta_t*(1.0/15.0)+std::max(20, initial_power));
    int current_power;
    if(CCD_cooler_data(&ambient_t,
		       &ccd_t,
		       &setpoint_t,
		       &current_power, // range 0..100
		       &humidity,
		       &initial_mode) == 0) {
      fprintf(stderr, "do_startup: cannot get cooler data.\n");
      return;
    }
    double delta_power = current_power - target_power;
    double power_change = (last_power < 0 ? 0.0 : current_power-last_power);
    last_power = current_power;
    bool changed = false;
    if (in_hold) {
      if (fabs(power_change) < 0.5) {
	hold_count++;
	if (hold_count >= 4) {
	  fprintf(stderr, "Done with hold. Continuing.\n");
	  in_hold = false;
	}
      } else {
	hold_count = 0;
      }
    } else if (fabs(power_change) > 12) {
      fprintf(stderr, "Excessive change. Holding for a moment\n");
      in_hold = true;
      hold_count = 0;
    } else if (delta_power < 2.0 /* percent */) {
      // power too low, more aggressive setpoint needed
      current_setpoint -= 0.1;
      changed = true;
    } else if (delta_power > 2.0 /*percent*/) {
      // power too high
      current_setpoint += 0.1;
      changed = true;
    }
    if (changed) {
      cmd.SetCoolerSetpoint(current_setpoint);
      cmd.Send();
    }
    fprintf(stderr, "%d %.1lf %.1lf %d %.0lf\n",
	    (int) delta_t, ccd_t, current_setpoint, current_power, target_power);
    if (current_power >= 99 and
	target_power >= 99) break;
  }

#endif

  if (perform_adjust) {
    ////////////////////////////////
    //    STEP 2                  //
    ////////////////////////////////
    fprintf(stderr, "Waiting 1 min for temp to stabilize.\n");
    sleep(60);

    ////////////////////////////////
    //    STEP 3                  //
    ////////////////////////////////
    if(CCD_cooler_data(&ambient_t,
		       &ccd_t,
		       &setpoint_t,
		       &initial_power,
		       &humidity,
		       &initial_mode) == 0) {
      fprintf(stderr, "do_startup: cannot get cooler data.\n");
      return;
    }
    fprintf(stderr, "Current T = %f. Set as setpoint.\n", ccd_t);
    cmd.SetCoolerSetpoint(ccd_t);
    cmd.Send();

    ////////////////////////////////
    //    STEP 4                  //
    ////////////////////////////////
    fprintf(stderr, "Waiting 1 min for temp to stabilize.\n");
    sleep(60);

    do_adjust();
  }
}

void do_shutdown(void) {
  // slow ramp down from initial power level to 0.
  // Then perform cooler shutdown.
  double ambient_t;
  double ccd_t;
  double setpoint_t;
  int initial_power;
  int initial_mode;
  double humidity;

  if(CCD_cooler_data(&ambient_t,
		     &ccd_t,
		     &setpoint_t,
		     &initial_power,
		     &humidity,
		     &initial_mode) == 0) {
    fprintf(stderr, "do_shutdown: cannot get cooler data.\n");
    return;
  }

  // ramp down at the rate of 1% power every 4 seconds.
  CoolerCommand cmd;
  for(int current_power = initial_power; current_power; current_power--) {
    fprintf(stderr, "P = %d%%\n", current_power);
    cmd.SetCoolerManual((double) current_power / 100.0);
    cmd.Send();
    sleep(4);
  }

  cmd.SetCoolerOff();
  cmd.Send();
}

void do_adjust(void) {
  double ambient_t;
  double ccd_t;
  double setpoint_t;
  int initial_power;
  int initial_mode;
  double humidity;

  CoolerCommand cmd;

  ////////////////////////////////
  //    STEP 5                  //
  ////////////////////////////////
  if(CCD_cooler_data(&ambient_t,
		     &ccd_t,
		     &setpoint_t,
		     &initial_power,
		     &humidity,
		     &initial_mode) == 0) {
    fprintf(stderr, "do_adjust: cannot get cooler data.\n");
    return;
  }

  // Target value of initial_power is 92%
  int delta_power = 92 - initial_power;
  double delta_temp = delta_power * 0.24;
  // if power is too low, delta_power is > 0 and delta_temp is > 0
  double target_temp = ccd_t - delta_temp;
  fprintf(stderr, "Current T = %f. Current power = %d%%.\n",
	  ccd_t, initial_power);
  fprintf(stderr, "... new setpoint will be %f\n", target_temp);

  cmd.SetCoolerSetpoint(target_temp);
  cmd.Send();
}

void do_hold(void) {
  double ambient_t;
  double ccd_t;
  double setpoint_t;
  int initial_power;
  int initial_mode;
  double humidity;

  CoolerCommand cmd;

  ////////////////////////////////
  //    STEP 5                  //
  ////////////////////////////////
  if(CCD_cooler_data(&ambient_t,
		     &ccd_t,
		     &setpoint_t,
		     &initial_power,
		     &humidity,
		     &initial_mode) == 0) {
    fprintf(stderr, "do_hold: cannot get cooler data.\n");
    return;
  }

  fprintf(stderr, "New setpoint will be %lf\n", ccd_t);
  cmd.SetCoolerSetpoint(ccd_t);
  cmd.Send();
}

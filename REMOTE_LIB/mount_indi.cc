/*  mount_indi.cc -- Implements user view of mount
 *
 *  Copyright (C) 2024 Mark J. Munkacsy

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

#include <time.h>
#include <assert.h>
#include <unistd.h>		// usleep()

#include "mount_indi.h"
#include "scope_api.h"
#include <system_config.h>

// This is being invoked, which means that a mount device was
// encountered. We are probably (right now) receiving properties for
// the device.
MOUNT_INDI::MOUNT_INDI(AstroDevice *device, const char *connection_port) :
  LocalDevice(device, connection_port), dev(device) {
  this->DoINDIRegistrations();

  dev->indi_device.watchProperty("EQUATORIAL_EOD_COORD",
				 [this](INDI::Property p) {
				   //std::cerr << "EQ_EOD_COORD update.\n";
				   this->blocker.Signal();
				 },
				 INDI::BaseDevice::WATCH_UPDATE);
}

void
MOUNT_INDI::InitializeMount(void) {
  ;
}

int
MOUNT_INDI::StarSync(DEC_RA *Location) {
  if (not this->dev->WaitForProperties({&mount->mount_dosync},
				       5 /*seconds*/)) {
    std::cerr << "MOUNT: timeout waiting for ON_COORD_SET property.\n";
    return 0;
  }

  this->mount_doslewstop.setState(ISS_OFF);
  this->mount_doslewtrack.setState(ISS_OFF);
  this->mount_dosync.setState(ISS_ON);
  this->dev->local_client->sendNewSwitch(this->mount_dosync.property->indi_property);

  // Epoch of the day is used to send command to mount
  DEC_RA target=ToEpoch(*Location, EPOCH(2000), EpochOfToday());
  this->mount_ra_eod.setValue(target.ra());
  this->mount_dec_eod.setValue(target.dec() * 180.0/M_PI);
  std::cerr << "mount: sync RA " << this->mount_ra_eod.getValue() << std::endl;
  std::cerr << "mount: syncDEC " << this->mount_dec_eod.getValue() << std::endl;
  this->dev->local_client->sendNewNumber(this->mount_ra_eod.property->indi_property);

  this->mount_doslewstop.setState(ISS_OFF);
  this->mount_doslewtrack.setState(ISS_ON);
  this->mount_dosync.setState(ISS_OFF);
  this->dev->local_client->sendNewSwitch(this->mount_dosync.property->indi_property);

  std::cerr << "StarSync(): completed.\n";
  
  return 1; // success
}
  
// Position is always provided in J2000
int
MOUNT_INDI::MoveTo(DEC_RA &location, int encourage_flip) { // returns immediately; no wait
  this->Device()->local_client->Log("MoveTo()");
  if (not this->Device()->WaitForProperties({
	&mount_ra_eod,
	&mount_latitude,
	&mount_tgt_ra,
	&mount_park}, 15)) { // 15 sec timeout
      std::cerr << "MOUNT: timeout waiting for property definitions. Mount not connected??\n";
      return 0;
  }
  if (this->mount_park.getState() == ISS_ON) {
    std::cerr << "ERROR: Cannot Move. Mount is parked.\n";
    throw std::runtime_error("Cannot move parked mount");
    return 0; // failure
  }
  std::cerr << "mount: tgt RA " << this->mount_tgt_ra.getValue() << std::endl;
  std::cerr << "mount: tgt DEC " << this->mount_tgt_dec.getValue() << std::endl;
  if (this->mount_debug_enable.available and
      this->mount_debug_disable.available) {
    std::cerr << "Enabling INDI mount debug logging.\n";
    this->mount_debug_enable.setState(ISS_ON);
    this->mount_debug_disable.setState(ISS_OFF);
    this->dev->local_client->sendNewSwitch(this->mount_debug_enable.property->indi_property);
    while(not (this->mount_log_file.available and this->mount_log_debug.available)) {
      std::cerr << "   ...waiting for logging info to be available.\n";
      sleep(1);
    }
    this->mount_log_file.setState(ISS_ON);
    this->dev->local_client->sendNewSwitch(this->mount_log_file.property->indi_property);
    this->mount_log_debug.setState(ISS_ON);
    this->dev->local_client->sendNewSwitch(this->mount_log_debug.property->indi_property);
    sleep(1);
  } else {
    std::cerr << "Unable to initiate INDI mount debug logging.\n";
  }

#if 0
  std::cerr << "mount: do_stop: "
	    << (this->mount_doslewstop.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
  std::cerr << "mount: do_track: "
	    << (this->mount_doslewtrack.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
  std::cerr << "mount: do_sync: "
	    << (this->mount_dosync.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
  std::cerr << "mount: park: "
	    << (this->mount_park.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
  std::cerr << "mount: unpark: "
	    << (this->mount_unpark.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
  std::cerr << "mount: type: altaz "
	    << (this->mount_type_altaz.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
  std::cerr << "mount: type: eq fork "
	    << (this->mount_type_eqfork.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
  std::cerr << "mount: type: eq german "
	    << (this->mount_type_eqgem.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
  std::cerr << "mount: tgt RA " << this->mount_tgt_ra.getValue() << std::endl;
  std::cerr << "mount: tgt DEC " << this->mount_tgt_dec.getValue() << std::endl;
  std::cerr << "mount: debug(err) "
	    << (this->mount_debug_err.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
  std::cerr << "mount: debug(warn) "
	    << (this->mount_debug_warn.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
  std::cerr << "mount: debug(sess) "
	    << (this->mount_debug_sess.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
  std::cerr << "mount: debug(debug) "
	    << (this->mount_debug_debug.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
  std::cerr << "mount: log(err) "
	    << (this->mount_log_err.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
  std::cerr << "mount: log(warn) "
	    << (this->mount_log_warn.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
  std::cerr << "mount: log(sess) "
	    << (this->mount_log_sess.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
  std::cerr << "mount: log(debug) "
	    << (this->mount_log_debug.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
  std::cerr << "mount: log output_file "
	    << (this->mount_log_file.getState() == ISS_ON ? "true" : "false")
	    << std::endl;
#endif
  
  // Epoch of the day is used to send command to mount
  DEC_RA target=ToEpoch(location, EPOCH(2000), EpochOfToday());
  this->requested_location = target;
  this->blocker.Setup();
  this->mount_ra_eod.setValue(target.ra());
  this->mount_dec_eod.setValue(target.dec() * 180.0/M_PI);
  std::cerr << "mount: new RA " << this->mount_ra_eod.getValue() << std::endl;
  std::cerr << "mount: new DEC " << this->mount_dec_eod.getValue() << std::endl;
  this->dev->local_client->sendNewNumber(this->mount_ra_eod.property->indi_property);
  std::cerr << "MOUNT_INDI: new coordinates sent: "
	    << target.string_ra_of() << ' ' << target.string_dec_of() << std::endl;
  sleep(1);
  std::cerr << "mount: tgt RA " << this->mount_tgt_ra.getValue() << std::endl;
  std::cerr << "mount: tgt DEC " << this->mount_tgt_dec.getValue() << std::endl;
  // do not block here
  return 1; // success
}

void
MOUNT_INDI::WaitForMoveDone(void) {			    // blocks until completed
#if 0 // see below
  double error_hrs = 0.0;
  double error_dec_radians = 0.0;
#endif
  int timeout_count = 30; // 30 seconds should always be enough
  do {
    //std::cerr << "Wait: waiting one second.\n";
    int ret_code = this->blocker.Wait(1000);
    // non-zero ret_code means timeout happened.
    if (ret_code) {
      // timeout
      timeout_count--;
      if (timeout_count <= 0) {
	std::cerr << "ERROR: MOUNT_INDI: scope motion timeout (30 sec)\n";
	return;
      }
    } else {
      timeout_count = 30;	// no timeout: reset timeout count
    }
#if 0
    double ra = this->mount_ra_eod.getValue(); // hours
    double dec = this->mount_dec_eod.getValue(); // deg
    error_hrs = fabs(ra - this->requested_location.ra());
    error_dec_radians = fabs(dec - this->requested_location.dec()*180.0/M_PI);
    //std::cerr << "Wait: ra = " << ra << ", dec = " << dec
    //	      << ", err_ra = " << error_hrs
    //	      << ", err_dec = " << error_dec_radians
    //	      << std::endl;
  } while (error_hrs > 3.0/60.0 or error_dec_radians > (3.0/60.0)*M_PI/180.0);
#else
    auto cur_state = this->mount_dec_eod.property->indi_property.getState();
    if (cur_state != IPS_BUSY) break;
  } while(1);
#endif
  // let mount settle
  sleep(2);
}

bool
MOUNT_INDI::SlewInProgress(void) {			    // non-blocking test
  double ra = this->mount_ra_eod.getValue(); // hours
  double dec = this->mount_dec_eod.getValue(); // deg
  double error_hrs = fabs(ra - this->requested_location.ra());
  double error_dec_radians = fabs(dec - this->requested_location.dec()*M_PI/180.0);
  return (error_hrs > 3.0/60.0 or error_dec_radians > (3.0/60.0)*M_PI/180.0);
}

void
MOUNT_INDI::GoToFlatLight(void) {	// blocks until completed
  assert("ERROR: GoToFlatLight() is unimplemented.");
}

void
MOUNT_INDI::ControlTrackingMotor(int turn_off) {
  if (not this->Device()->WaitForProperties({
	&mount_track_enable },15)) { // 15 sec timeout
    std::cerr << "MOUNT: timeout waiting for property definitions. Mount not connected??\n";
  } else {
    this->mount_track_enable.setState(turn_off ? ISS_OFF : ISS_ON);
    this->mount_track_disable.setState(turn_off ? ISS_ON : ISS_OFF);
    this->dev->local_client->sendNewSwitch(this->mount_track_enable.property->indi_property);
  }
}

void
MOUNT_INDI::Park(void) {
  // maybe there needs to be a Wait() before this?
  if (this->mount_park.getState() == ISS_ON) {
    std::cerr << "Mount already parked.\n";
    return;
  }
  
  this->Device()->local_client->Log("Park()");
  //this->blocker.Setup();
  this->mount_unpark.setState(ISS_OFF);
  this->mount_park.setState(ISS_ON);
  this->dev->local_client->sendNewSwitch(this->mount_park.property->indi_property);

#if 1
  bool finished = false;
  int tries = 0;
  do {
    if (this->mount_park.getState() == ISS_ON and
	this->mount_unpark.property->indi_property.getState() == IPS_OK) {
      finished = true;
      break;
    } else {
      usleep(100000);
      if (tries++ > 600) break;
    }
  } while(not finished);
  if (not finished) {
    std::cerr << "ERROR: Park timeout.\n";
    this->Device()->local_client->Log("ERROR: Park timeout.");
  } else {
    this->Device()->local_client->Log("Park() completed.");
    std::cerr << "Parked.\n";
  }

#else
  // wait for complete
  int ret_code = this->blocker.Wait(60.0*1000); // 60 seconds
  if (ret_code) {
    std::cerr << "Park: error: "
	      << strerror(ret_code) << std::endl;
  }
#endif
}

void
MOUNT_INDI::Unpark(void) {
  // maybe there needs to be a Wait() before this?
  if (this->mount_unpark.getState() == ISS_ON) {
    std::cerr << "Mount already unparked.\n";
    return;
  }

  this->Device()->local_client->Log("Unpark()");
  this->blocker.Setup();
  this->mount_unpark.setState(ISS_ON);
  this->mount_park.setState(ISS_OFF);
  this->dev->local_client->sendNewSwitch(this->mount_park.property->indi_property);

#if 1
  bool finished = false;
  int tries = 0;
  do {
    if (this->mount_unpark.getState() == ISS_ON and
	this->mount_unpark.property->indi_property.getState() == IPS_OK) {
      finished = true;
      break;
    } else {
      usleep(100000);
      if (tries++ > 600) break;
    }
  } while(not finished);
  if (not finished) {
    std::cerr << "ERROR: Park timeout.\n";
    this->Device()->local_client->Log("ERROR: Unpark timeout.");
  } else {
    this->Device()->local_client->Log("Unpark() completed.");
    std::cerr << "Unparked.\n";
  }
#else
  // wait for complete
  int ret_code = this->blocker.Wait(10.0*1000); // 10 seconds
  if (ret_code) {
    std::cerr << "Unpark: error: "
	      << strerror(ret_code) << std::endl;
  }
#endif
}

bool
MOUNT_INDI::ScopeOnWestSideOfPier(void) {
  if (not this->mount_side_w.available) {
    assert("ERROR: Cannot determine mount pier side: unimplemented?");
  }
  return (this->mount_side_w.getState() == ISS_ON);
}

int
MOUNT_INDI::SmallMove(double delta_ra_arcmin, double delta_dec_arcmin) {
  this->Device()->local_client->Log("SmallMove()");
  // This always works. A nudge command would be even better (GM2000)
  DEC_RA orig_loc = this->ScopePointsAtJ2000();
  DEC_RA target_loc(orig_loc.dec() + (delta_dec_arcmin/60.0)*M_PI/180.0,
		    orig_loc.ra_radians() +
		    ((delta_ra_arcmin/60.0)*M_PI/180.0)/cos(orig_loc.dec()));
  this->MoveTo(target_loc, /*encourage_flip*/ 0);
  this->WaitForMoveDone();
  return 0;
}

//****************************************************************
//        Guiding
//****************************************************************
void
MOUNT_INDI::SetupGuiding(void) {
  static bool setup_complete = false;

  if (setup_complete) return;

  if (not this->Device()->WaitForProperties({
	&mount_guide100,
	&mount_guideN,
        &mount_use_pulseguiding}, 15)) { // 15 sec timeout
    assert("MOUNT: timeout waiting for guide properties.\n");
  }
    
  // Set guide speed to 1.0 X sidereal (for AP1200)
  if (this->mount_guide100.getState() != ISS_ON) {
    this->mount_guide100.setState(ISS_ON);
    this->mount_guide050.setState(ISS_OFF);
    this->mount_guide025.setState(ISS_OFF);
    this->dev->local_client->sendNewSwitch(this->mount_guide100.property->indi_property);
  }
  // Enable pulse guiding
  //if (this->mount_use_pulseguiding.getState() != ISS_ON) {
    this->mount_use_pulseguiding.setState(ISS_ON);
    this->mount_disable_pulseguiding.setState(ISS_OFF);
    this->dev->local_client->sendNewSwitch(this->mount_use_pulseguiding.property->indi_property);
    //}

  setup_complete = true;
}


// Need to know guide speed... see drifter.cc for more info.
void
MOUNT_INDI::Guide(double NorthSeconds, double EastSeconds) {
  this->SetupGuiding(); // make sure guide speed is correct
  if (NorthSeconds < 0.0) {
    this->mount_guideS.setValue(-1000.0 * NorthSeconds);
    this->mount_guideN.setValue(0.0);
  } else {
    this->mount_guideN.setValue(1000.0 * NorthSeconds);
    this->mount_guideS.setValue(0.0);
  }
  if (EastSeconds < 0.0) {
    this->mount_guideW.setValue(-1000.0 * EastSeconds);
    this->mount_guideE.setValue(0.0);
  } else {
    this->mount_guideE.setValue(1000.0 * EastSeconds);
    this->mount_guideW.setValue(0.0);
  }
  std::cerr << "MOUNT_INDI::Guide(N="
	    << 1000.0*NorthSeconds << ", E="
	    << 1000.0*EastSeconds << ")\n";
  this->dev->local_client->sendNewNumber(this->mount_guideE.property->indi_property);
  this->dev->local_client->sendNewNumber(this->mount_guideN.property->indi_property);
  double wait_time = fabs(NorthSeconds);
  if (fabs(EastSeconds) > wait_time) wait_time = fabs(EastSeconds);
  usleep((useconds_t) (wait_time*1000000));
}

// Do NOT trust the epoch of the Dec/RA that this returns. It is
// something internal to the mount.
DEC_RA
MOUNT_INDI::RawScopePointsAt(void) {
  for (int loop = 0; loop < 10; loop++) {
    if (this->mount_ra.available) {
      double ra = this->mount_ra.getValue(); // hours
      double dec = this->mount_dec.getValue(); // deg
      return DEC_RA(dec*M_PI/180.0, ra*M_PI/12.0);
    } else if (this->mount_ra_eod.available) {
      double ra = this->mount_ra_eod.getValue(); // hours
      double dec = this->mount_dec_eod.getValue(); // deg
      return DEC_RA(dec*M_PI/180.0, ra*M_PI/12.0);
    }
    std::cerr << "Waiting for mount pos to be available.\n";
    sleep(1);
  }
  std::cerr << "MOUNT_INDI: no dec/ra avail from mount\n";
  INDIDisconnectINDI();
  exit(-2);
  /*NOTREACHED*/
  return DEC_RA(0.0,0.0);  
}

// This always returns a position in J2000
DEC_RA
MOUNT_INDI::ScopePointsAtJ2000(void) {
  for (int loop = 0; loop < 10; loop++) {
    if (this->mount_ra.available) {
      double ra = this->mount_ra.getValue(); // hours
      double dec = this->mount_dec.getValue(); // deg
      return DEC_RA(dec*M_PI/180.0, ra*M_PI/12.0);
    } else if (this->mount_ra_eod.available) {
      double ra = this->mount_ra_eod.getValue(); // hours
      double dec = this->mount_dec_eod.getValue(); // deg
      DEC_RA j_now(dec*M_PI/180.0, ra*M_PI/12.0);
      return ToEpoch(j_now, EpochOfToday(), EPOCH(2000));
    }
    std::cerr << "Waiting for mount pos to be available.\n";
    sleep(1);
  }
  std::cerr << "MOUNT_INDI: no dec/ra avail from mount\n";
  INDIDisconnectINDI();
  exit(-2);
  /*NOTREACHED*/
  return DEC_RA(0.0,0.0);
}


//****************************************************************
//        Local Sidereal Time
// Some mounts provide this, some don't. Here we compute it from
// scratch. This is accurate to about +/- 2 sec, according to USNO.
//****************************************************************
double
MOUNT_INDI::GetLocalSiderealTime(void) {// Return value in hours 0..24
  // algorithm from usno.navy.mil
  time_t now = time(0);
  JULIAN jdtt(now);
  struct tm prior_midnight;
  (void) localtime_r(&now, &prior_midnight);
  prior_midnight.tm_sec =
    prior_midnight.tm_min =
    prior_midnight.tm_hour =0;
  time_t midnight = mktime(&prior_midnight);
  JULIAN jd0(midnight);
  double dtt = jdtt.day() - 2451545.0;
  double dut = jd0.day() - 2451545.0;
  double t = dtt/36525.0; 	// number centuries since 2000
  double gmst = fmod(6.697375+0.065707485828*dut
		     +0.0854103*t+0.0000258*t*t, 24.0); // hours 0..24
  // now add local longitude
  double last = gmst + (system_config.Longitude()/15.0);
  if (last < 0.0) last += 24.0;
  if (last >= 24.0) last -= 24.0;
  return last;
}
  
  
  
//****************************************************************
//        DoINDIRegistrations
//****************************************************************
void
MOUNT_INDI::DoINDIRegistrations(void) {
  //mount_indi_port.Initialize("/dev/ap1200");
  mount_latitude.Initialize(system_config.Latitude());
  mount_longitude.Initialize(system_config.Longitude());
  mount_elevation.Initialize(10.0); // meters????

  std::time_t current_time;
  std::time(&current_time);
  struct std::tm *timeinfo = std::localtime(&current_time);
  const int offset = timeinfo->tm_gmtoff;
  static char offset_str[12];
  sprintf(offset_str, "+%d", offset);
  mount_utc_offset.Initialize(offset_str);

  static char utc_str[sizeof "2011-10-08T07:07:09Z"];
  strftime(utc_str, sizeof utc_str, "%FT%TZ", gmtime(&current_time));
  mount_utc.Initialize(utc_str);
}

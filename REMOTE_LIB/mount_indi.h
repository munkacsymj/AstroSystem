/* This may look like C code, but it is really -*-c++-*- */
/*  mount_indi.h -- Implements user view of color filter wheel
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
#pragma once

#include "blocker_indi.h"
#include "dec_ra.h"
#include "astro_indi.h"

class MOUNT_INDI : public LocalDevice {
public:
  MOUNT_INDI(AstroDevice *device, const char *connection_port);
  ~MOUNT_INDI(void);

  AstroDevice *Device(void) const { return dev; }
  void InitializeMount(void);

  bool MOUNTPresent(void) const { return mount_ra.available; }

  int MoveTo(DEC_RA &location, int encourage_flip); // returns immediately; no wait
  void WaitForMoveDone(void);			    // blocks until completed
  bool SlewInProgress(void);			    // non-blocking test

  void GoToFlatLight(void);	// blocks until completed

  void ControlTrackingMotor(int turn_off);
  void Park(void);
  void Unpark(void);

  int StarSync(DEC_RA *Location);

  bool ScopeOnWestSideOfPier(void);

  int SmallMove(double delta_ra_arcmin, double delta_dec_arcmin);
  // Need to know guide speed... see drifter.cc for more info.
  void Guide(double NorthSeconds, double EastSeconds);

  // This returns something "raw" from the mount
  DEC_RA RawScopePointsAt(void);
  // This always returns a position in J2000
  DEC_RA ScopePointsAtJ2000(void);
  
  double GetLocalSiderealTime(void); // Return value in hours 0..24

  void MoveFilterWheel(int position, bool block=false);
  void WaitForFilterWheel(void);

  void DoINDIRegistrations(void);

private:
  DEC_RA requested_location;
  Blocker blocker;
  AstroDevice *dev {nullptr};
  void SetupGuiding(void);
  
  AstroValueNumber mount_ra {this,"EQUATORIAL_COORD", "RA"}; // j2000, hours
  AstroValueNumber mount_dec{this,"EQUATORIAL_COORD", "DEC"}; // j2000, deg
  AstroValueNumber mount_ra_eod {this,"EQUATORIAL_EOD_COORD", "RA"}; // jnow, hours
  AstroValueNumber mount_dec_eod{this,"EQUATORIAL_EOD_COORD", "DEC"}; // jnow, deg
  AstroValueNumber mount_alt{this,"HORIZONTAL_COORD", "ALT"}; // deg above horizon
  AstroValueNumber mount_az {this,"HORIZONTAL_COORD", "AZ"}; // deg east of north
  AstroValueSwitch mount_doslewstop {this,"ON_COORD_SET", "SLEW"};
  AstroValueSwitch mount_doslewtrack{this,"ON_COORD_SET", "TRACK"};
  AstroValueSwitch mount_dosync     {this,"ON_COORD_SET", "SYNC"};
  AstroValueSwitch mount_moveN {this,"TELESCOPE_MOTION_NS", "MOTION_NORTH"};
  AstroValueSwitch mount_moveS {this,"TELESCOPE_MOTION_NS", "MOTION_SOUTH"};
  AstroValueSwitch mount_moveW {this,"TELESCOPE_MOTION_WE", "MOTION_WEST"};
  AstroValueSwitch mount_moveE {this,"TELESCOPE_MOTION_WE", "MOTION_EAST"};
  AstroValueSwitch mount_guide025 {this, "Guide Rate", "0.25"};
  AstroValueSwitch mount_guide050 {this, "Guide Rate", "0.5"};
  AstroValueSwitch mount_guide100 {this, "Guide Rate", "1.0"};
  AstroValueNumber mount_guideN {this,"TELESCOPE_TIMED_GUIDE_NS", "TIMED_GUIDE_N"};
  AstroValueNumber mount_guideS {this,"TELESCOPE_TIMED_GUIDE_NS", "TIMED_GUIDE_S"};
  AstroValueNumber mount_guideW {this,"TELESCOPE_TIMED_GUIDE_WE", "TIMED_GUIDE_W"};
  AstroValueNumber mount_guideE {this,"TELESCOPE_TIMED_GUIDE_WE", "TIMED_GUIDE_E"};
  AstroValueSwitch mount_use_pulseguiding {this, "Use Pulse Cmd", "On"};
  AstroValueSwitch mount_disable_pulseguiding {this, "Use Pulse Cmd", "Off"};
  AstroValueSwitch mount_slew_guide  {this,"TELESCOPE_SLEW_RATE", "SLEW_GUIDE"};
  AstroValueSwitch mount_slew_center {this,"TELESCOPE_SLEW_RATE", "SLEW_CENTERING"};
  AstroValueSwitch mount_slew_find   {this,"TELESCOPE_SLEW_RATE", "SLEW_FIND"};
  AstroValueSwitch mount_slew_max    {this,"TELESCOPE_SLEW_RATE", "SLEW_MAX"};
  AstroValueSwitch mount_track_enable  {this, "TELESCOPE_TRACK_STATE", "TRACK_ON"};
  AstroValueSwitch mount_track_disable {this, "TELESCOPE_TRACK_STATE", "TRACK_OFF"};
  AstroValueSwitch mount_park  {this,"TELESCOPE_PARK", "PARK"};
  AstroValueSwitch mount_unpark{this,"TELESCOPE_PARK", "UNPARK"};
  AstroValueSwitch mount_side_e {this,"TELESCOPE_PIER_SIDE", "PIER_EAST"};
  AstroValueSwitch mount_side_w {this,"TELESCOPE_PIER_SIDE", "PIER_WEST"};
  AstroValueSwitch mount_debug_enable {this, "DEBUG", "ENABLE"};
  AstroValueSwitch mount_debug_disable {this, "DEBUG", "DISABLE"};
  AstroValueSwitch mount_type_altaz {this,"MOUNT_TYPE", "ALTAZ"};
  AstroValueSwitch mount_type_eqfork {this,"MOUNT_TYPE", "EQ_FORK"};
  AstroValueSwitch mount_type_eqgem {this,"MOUNT_TYPE", "EQ_GEM"};
  AstroValueNumber mount_latitude {this,"GEOGRAPHIC_COORD", "LAT"};
  AstroValueNumber mount_longitude {this,"GEOGRAPHIC_COORD", "LONG"};
  AstroValueNumber mount_elevation {this,"GEOGRAPHIC_COORD", "ELEV"};
  AstroValueText   mount_utc {this,"TIME_UTC", "UTC"};
  AstroValueText   mount_utc_offset {this,"TIME_UTC", "OFFSET"};
  AstroValueNumber mount_tgt_ra  {this,"TARGET_EOD_COORD", "RA" };
  AstroValueNumber mount_tgt_dec {this,"TARGET_EOD_COORD", "DEC"};
  AstroValueSwitch mount_debug_err   {this,"DEBUG_LEVEL", "DBG_ERROR"};
  AstroValueSwitch mount_debug_warn  {this,"DEBUG_LEVEL", "DBG_WARNING"};
  AstroValueSwitch mount_debug_sess  {this,"DEBUG_LEVEL", "DBG_SESSION"};
  AstroValueSwitch mount_debug_debug {this,"DEBUG_LEVEL", "DBG_DEBUG"};
  AstroValueSwitch mount_log_err   {this,"LOGGING_LEVEL", "LOG_ERROR"};
  AstroValueSwitch mount_log_warn  {this,"LOGGING_LEVEL", "LOG_WARNING"};
  AstroValueSwitch mount_log_sess  {this,"LOGGING_LEVEL", "LOG_SESSION"};
  AstroValueSwitch mount_log_debug {this,"LOGGING_LEVEL", "LOG_DEBUG"};
  AstroValueSwitch mount_log_file  {this,"LOG_OUTPUT", "FILE_DEBUG"};
  AstroValueText   mount_indi_port {this,"DEVICE_PORT", "PORT"};
  
  friend void MOUNTPropertyUpdate(INDI::Property property);

};

extern MOUNT_INDI *mount;


/* This may look like C code, but it is really -*-c++-*- */
/*  astro_indi.h -- Implements user view of camera
 *
 *  Copyright (C) 2023 Mark J. Munkacsy

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
#ifndef _ASTRO_INDI_H
#define _ASTRO_INDI_H

#include <iostream>
#include <list>
#include <libindi/indiapi.h>
#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

struct AstroValue;
class AstroDevice;

extern AstroDevice *ccd_dev;
extern AstroDevice *fine_focus_dev;
extern AstroDevice *coarse_focus_dev;
extern AstroDevice *cooler_dev;
extern AstroDevice *cfw_dev;
extern AstroDevice *mount_dev;

// The AstroClient object is created as soon as someone calls either
// connect_to_camera or connect_to_scope.
class AstroClient : public INDI::BaseClient {
public:
  AstroClient(void);
  ~AstroClient(void);
  void RefreshDevices(void);
  void Log(std::string message);
protected:
  virtual void newDevice(INDI::BaseDevice dp) override;
  virtual void removeDevice(INDI::BaseDevice dp) override;
  virtual void newProperty(INDI::Property property) override;
  virtual void removeProperty(INDI::Property property) {}
  virtual void newBLOB(IBLOB *bp) {}
  virtual void newSwitch(ISwitchVectorProperty *svp) {}
  virtual void newNumber(INumberVectorProperty *nvp) {}
  virtual void newMessage(INDI::BaseDevice *dp, int messageID) {}
  virtual void newText(ITextVectorProperty *tvp) {}
  virtual void newLight(ILightVectorProperty *lpv) {}
  //virtual void serverConnected() override { std::cout << "serverConnected()\n"; }
  //virtual void serverDisconnected(int exit_code) override { std::cout << "serverDisconnected()\n"; }
  virtual void serverConnected() override { ; }
  virtual void serverDisconnected(int exit_code) override { ; }
  virtual void updateProperty(INDI::Property property) override;

private:
  void logProperty(INDI::Property property);
  //private:
  //INDI::BaseDevice *cd_simulator {nullptr};
};

class AstroInitialize;

// An AstroProperty is created as soon as we hear of the existence of
// the corresponding INDI property.
class AstroProperty {
public:
  AstroProperty(INDI::Property property, AstroDevice *dev);
  ~AstroProperty(void);
  
  INDI::Property indi_property;
  const char *property_name;
  INDI_PROPERTY_TYPE property_type;
  AstroDevice *device;
  std::list<AstroValue *> value_list;
  std::list<AstroInitialize *> initialization_list;

  size_t size(void) const; // number of elements in the property
  const char *getElemName(size_t elem_index) const;
  double getElemValueNumber(size_t elem_index) const;
  const char *getElemValueText(size_t elem_index) const;
  ISState getElemValueSState(size_t elem_index) const; // SWITCH
  IPState getElemValueLState(size_t elem_index) const; // LIGHT
};

// AstroDevices come into existence when the INDI server tells us of
// the existence of an INDI device.
class AstroDevice {
public:
  AstroDevice(INDI::BaseDevice dp, AstroClient *client);
  ~AstroDevice(void);

  std::list<AstroProperty *> properties;
  std::list<AstroValue *> lookups; // See "Property Lookup Info" in astro_indi.cc

  AstroClient *local_client;
  const char *device_name;
  INDI::BaseDevice indi_device;
  bool is_connected {false};
  bool driver_info_avail {false};
  std::list<INDI::Property> pending_properties;
  void ProcessPendingProperties(void);
  void SetupCache(INDI::Property p, AstroProperty *ap);
  virtual void ConnectToHardware(void);
  const char *connection_port {nullptr};
  
  // returns true when successful. If timeout_secs is zero, will block forever if properties
  // don't show up. Returns false if timeout before all values available.
  bool WaitForProperties(std::list<AstroValue *> waitlist, int timeout_secs);
};

// LocalDevice is the parent class of the individual "working" devices
// named "mount", "camera", etc. These get created at about the same
// time as AstroDevices. 
class LocalDevice {
public:
  LocalDevice(AstroDevice *ad, const char *connection_port);
  ~LocalDevice(void) {;}
  void Register(AstroValue *av) { astro_device->lookups.push_back(av); }
  // returns TRUE if connect succeeded.
  bool WaitForConnect(int timeout_secs);
protected:
private:
  AstroDevice *astro_device;
};

// These get created as part of creating LocalDevices. This normally
// happens before we become aware of the existence of the
// corresponding INDI properties.
class AstroValue {
public:
  AstroValue(LocalDevice *ld,
	     const char *prop_name,
	     const char *element_name) :
    property_name(prop_name),
    value_name(element_name) {ld->Register(this);}
  const char *property_name;
  const char *value_name;
  int property_index;
  bool available {false};
  //bool cache_setup {false};
  //void *value_ptr {nullptr};
  AstroProperty *property {nullptr};
  std::list<AstroInitialize *> initialization_list;
  const char *getStateAsString(void) { return property->indi_property.getStateAsString(); }

  IPState getState(void);
  void SendINDIUpdate(void);
};

class AstroValueNumber : public AstroValue {
public:
  AstroValueNumber(LocalDevice *ld,
		   const char *property_name,
		   const char *element_name) : AstroValue(ld, property_name,
							  element_name){;}
  double getValue(void);
  IPState getINDIState(void);
  double getMin(void) const;
  double getMax(void) const;
  void setValue(double value);
  void Initialize(double init_value);
};

class AstroValueSwitch : public AstroValue {
public:
  AstroValueSwitch(LocalDevice *ld,
		   const char *property_name,
		   const char *element_name) : AstroValue(ld,property_name,
							  element_name) {;}
  ISState getState(void);
  void setState(ISState value);

  IPState getINDIState(void);
  void InitializeOneOf(void);
  void Initialize(ISState value);
};

class AstroValueText : public AstroValue {
public:
  AstroValueText(LocalDevice *ld,
		 const char *property_name,
		 const char *element_name) : AstroValue(ld,property_name,
							element_name) {;}
  // should be freed after use with free()
  const char *getValue(void);
  void setValue(const char *value); // value will be copied with strdup()
  void Initialize(const char *value);
};

class AstroValueLight : public AstroValue {
public:
  AstroValueLight(LocalDevice *ld,
		  const char *property_name,
		  const char *element_name) : AstroValue(ld,property_name,
							 element_name) {;}
  IPState getValue(void);
  void setValue(IPState value);
};

class AstroValueBLOB : public AstroValue {
public:
  AstroValueBLOB(LocalDevice *ld,
		 const char *property_name,
		 const char *element_name) : AstroValue(ld,property_name,
							element_name) {;}
  void *getValue(void);
  void setValue(void * value);
};

bool AstroINDIConnected(void);
void ConnectAstroINDI(void);
void INDIDisconnectINDI(void);

#define COOLER_HUMIDITY_WORKS     0x01
#define COOLER_AMBIENT_WORKS      0x02
#define CAMERA_GAIN_WORKS         0x04
#define CAMERA_OFFSET_WORKS       0x08
#define CAMERA_READOUT_MODE_WORKS 0x10
#define CAMERA_BINNING_WORKS      0x20
#define CAMERA_USB_TRAFFIC_WORKS  0x40

#define EXP_TYPE_LIGHT  1
#define EXP_TYPE_DARK   2

enum ExposureType {
  EXP_DARKFRAME,
  EXP_BIASFRAME,
  EXP_LIGHTFRAME
};

struct CameraState {
  double CCD_temp;		// actual temp
  double CCD_setpoint;		// desired temp
  double Ambient_temp;
  double humidity;
  double cooler_power; // 0 .. 100
  int    cooler_status;		// bitmap
  int    filter_position;	// Lowest position is 1, not zero
  ExposureType exposure_type;
  unsigned int capabilities;    // bitwise or of CAMERA...WORKS, above
  int    sensor_width;		// unbinned
  int    sensor_height;		// unbinned
  int    frame_width;
  int    frame_height;
  int    frame_crop_left;
  int    frame_crop_right;
};

void ConnectAstroINDI(void);

#endif

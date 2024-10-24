/*  astro_indi.cc -- Implements user view of camera
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

#include <time.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <pthread.h>
#include "astro_indi.h"
#include <libindi/basedevice.h>
#include "blocker_indi.h"
#include "camera_indi.h"
#include "cooler_indi.h"
#include "cfw_indi.h"
#include "mount_indi.h"
#include "focuser_indi.h"
#include <system_config.h>

//#define LISTNEWPROPERTIES

std::list<AstroDevice *> known_devices;
std::list<AstroProperty *> known_properties;

std::ofstream property_log("/tmp/property.log", std::ios::app);

// These are the five devices that may or may not exist. Try to be
// tolerant of configurations that have missing devices. Some physical
// hardware (i.e., a single AstroDevice) may show up twice in this
// list if the one device does multiple things (e.g., a CCD/CFW pair).
AstroDevice *ccd_dev = nullptr;
AstroDevice *fine_focus_dev = nullptr;
AstroDevice *coarse_focus_dev = nullptr;
AstroDevice *cooler_dev = nullptr;
AstroDevice *cfw_dev = nullptr;
AstroDevice *mount_dev = nullptr;

CAMERA_INDI *camera { nullptr };
FOCUSER_INDI *focuser { nullptr };
FOCUSER_INDI *coarse_focuser { nullptr };
FOCUSER_INDI *fine_focuser { nullptr };
CCDCooler *cooler { nullptr };
CFW_INDI *cfw { nullptr };
MOUNT_INDI *mount { nullptr };

enum AstroDeviceType {
  ccd_t, fine_focus_t, coarse_focus_t, cooler_t, cfw_t, mount_t
};

struct KnownDevice {
  const char *indi_device_name;
  const char *local_device_name;
  std::list<AstroDevice **> dev_pointers;
  std::list<AstroDeviceType> devtype;
  const char *connection_port;
};

AstroDevice *dummy_unused_device = nullptr;

// This list tells us about devices that we hear about from the INDI
// server. This tells us lots of things about the device without
// having to wait for all of its properties to arrive.
static std::list<KnownDevice> predefined_devices {
  {"AstroPhysics V2", "AP1200",
   {&mount_dev}, {mount_t}, "/dev/serial/by-id/usb-Prolific_Technology_Inc._USB-Serial_Controller_AIASb136G03-if00-port0"},
  {"Telescope Simulator", "mount simulator",
   {&mount_dev}, {mount_t}, nullptr},
  {"CCD Simulator", "camera simulator",
   {&ccd_dev,&cooler_dev,&dummy_unused_device}, {ccd_t,cooler_t,cfw_t}, nullptr},
  {"SmartFocus", "JMI focuser",
   {&fine_focus_dev}, {fine_focus_t}, "/dev/serial/by-id/usb-Prolific_Technology_Inc._USB-Serial_Controller_BSCBe11BS13-if00-port0"},
  {"SBIG CCD", "ST-10XME",
   {&ccd_dev, &cooler_dev, &dummy_unused_device}, {ccd_t, cooler_t}, nullptr},
  {"QHY CCD QHY268M-d7178a4", "QHY268M",
   {&ccd_dev, &cooler_dev, &dummy_unused_device}, {ccd_t, cooler_t, cfw_t}, nullptr},
  {"Focuser Simulator", "focuser simulator",
   {&fine_focus_dev}, {fine_focus_t}, nullptr}
};
  
static bool AstroAttached = false;
static AstroClient *astro_client {nullptr};

class AstroInitialize {
public:
  AstroInitialize(AstroValue *var) :
    property(var->property),
    element(var) {;}
  ~AstroInitialize(void);
  virtual void DoInitialize(void) = 0;
protected:
  AstroProperty *property;
  AstroValue *element;
};

class AstroInitializeNumber : public AstroInitialize {
public:
  AstroInitializeNumber(AstroValueNumber *var, double init_value=0.0) :
    AstroInitialize(var),
    value(init_value),
  num_element(var) {;}
  ~AstroInitializeNumber(void) {;}
  virtual void DoInitialize(void);
protected:
  double value;
  AstroValueNumber *num_element;
};

class AstroInitializeOneOfSwitch : public AstroInitialize {
public:
  AstroInitializeOneOfSwitch(AstroValueSwitch *var);
  ~AstroInitializeOneOfSwitch(void) {;}
  virtual void DoInitialize(void) = 0;
};

class AstroInitializeText : public AstroInitialize {
public:
  AstroInitializeText(AstroValueText *var, const char *init_value) :
    AstroInitialize(var),
    value(init_value),
    text_element(var) {;}
  ~AstroInitializeText(void) {;}
  virtual void DoInitialize(void);
protected:
  const char *value;
  AstroValueText *text_element;
};

void
AstroInitializeNumber::DoInitialize(void) {
  num_element->setValue(this->value);
  this->property = num_element->property;
  this->property->device->local_client->sendNewNumber(property->indi_property);
}

void
AstroInitializeText::DoInitialize(void) {
  text_element->setValue(this->value);
  this->property = text_element->property;
  //std::cerr << "Property[" << this->property->property_name
  //	    << "] initialized to " << '"' << this->value << '"' << '\n';
  this->property->device->local_client->sendNewText(property->indi_property);
}

size_t AstroProperty::size(void) const {
  switch (this->property_type) {
  case INDI_NUMBER:
    return this->indi_property.getNumber()->count();
  case INDI_SWITCH:
    return this->indi_property.getSwitch()->count();
  case INDI_LIGHT:
    return this->indi_property.getLight()->count();
  case INDI_TEXT:
    return this->indi_property.getText()->count();
  case INDI_BLOB:
    return this->indi_property.getBLOB()->count();
  case INDI_UNKNOWN:
    return 0;
  }
  return 0;
}

const char *
AstroProperty::getElemName(size_t elem_index) const {
  switch (this->property_type) {
  case INDI_NUMBER:
    return this->indi_property.getNumber()->at(elem_index)->getName();
  case INDI_SWITCH:
    return this->indi_property.getSwitch()->at(elem_index)->getName();
  case INDI_LIGHT:
    return this->indi_property.getLight()->at(elem_index)->getName();
  case INDI_TEXT:
    return this->indi_property.getText()->at(elem_index)->getName();
  case INDI_BLOB:
    return this->indi_property.getBLOB()->at(elem_index)->getName();
  case INDI_UNKNOWN:
    return "<unknown>";
  }
  return "";
    
}
  
AstroProperty::AstroProperty(INDI::Property property, AstroDevice *dev) :
  indi_property(property) {
  this->property_name = strdup(property.getName());
  this->device = dev;
  this->device->properties.push_back(this);
  
  known_properties.push_back(this);
#ifdef LISTNEWPROPERTIES
  std::cout << "newProperty: " << property.getName() << '\n';
#endif

  INDI_PROPERTY_TYPE p_type = property.getType();
  this->property_type = p_type;
  for (size_t i = 0; i < this->size(); i++) {
    const char *element_name = this->getElemName(i);
#ifdef LISTNEWPROPERTIES
    std::cout << "    newElement: " << element_name << ' ';

    switch(p_type) {
    case INDI_NUMBER:
      std::cout <<  property.getNumber()->at(i)->getValue();
      break;
    case INDI_SWITCH:
      std::cout <<  property.getSwitch()->at(i)->getStateAsString();
      break;
    case INDI_LIGHT:
      std::cout <<  property.getLight()->at(i)->getStateAsString();
      break;
    case INDI_TEXT:
      std::cout <<  property.getText()->at(i)->getText();
      break;
    case INDI_BLOB:
      std::cout <<  "{BLOB}";
      break;
    case INDI_UNKNOWN:
      std::cout << "{unknown}";
      break;
    }      
#endif
    
    for (AstroValue *item : this->device->lookups) {
      if (strcmp(this->property_name, item->property_name) == 0 and
	  strcmp(element_name, item->value_name) == 0) {
#ifdef LISTNEWPROPERTIES
	std::cout << " [matched!]";
#endif
	item->property_index = i;
	item->available = true;
	item->property = this;
	for (auto x : item->initialization_list) {
	  this->initialization_list.push_back(x);
	}
	item->initialization_list.clear();
	break;
      }
    }
#ifdef LISTNEWPROPERTIES
    std::cout << std::endl;
#endif
  }
  for (auto init : this->initialization_list) {
    init->DoInitialize();
  }
  this->initialization_list.clear();
}

// return true if successful
bool
AstroDevice::WaitForProperties(std::list<AstroValue *> waitlist, int timeout_secs) {
  int time_msec = 0;
  bool satisfied = true;
  do {
    satisfied = true;
    for (AstroValue *x : waitlist) {
      if (not x->available) {
	satisfied = false;
	break;
      }
    }
    if (not satisfied) {
      usleep(1000000); // 100 msec
      time_msec += 100;
      if (timeout_secs > 0 and time_msec > timeout_secs*1000) break;
    }
  } while (not satisfied);
  return satisfied;
}


bool AstroINDIConnected(void) { return AstroAttached; }

void INDIDisconnectINDI(void) {
  if (AstroAttached) {
    AstroAttached = false;
    astro_client->disconnectServer();
    sleep(1);
    delete astro_client;
  }
}

void ConnectAstroINDI(void) {
  if (AstroAttached) return;
  // atexit() is something of a backup plan B, since it is invoked too
  // late in the process shutdown to prevent bad things from
  // happening, but I left it in here as a (weak) insurance policy.
  atexit(INDIDisconnectINDI);
  
  if (astro_client == nullptr) {
    astro_client = new AstroClient;
  }
  
  astro_client->setServer("localhost", 7624);
  //astro_client->watchDevice("*");
  //astro_client->watchDevice("CCD Simulator");
  (void) astro_client->connectServer();
  //std::cout << "connectServer() returned " << rv << '\n';
  astro_client->setBLOBMode(B_ALSO, "SBIG CCD", nullptr);
  //astro_client->enableDirectBlobAccess("Simple CCD", nullptr);
  AstroAttached = true;

  struct timespec delay { 0, 10000000 }; // 0.01 seconds
  nanosleep(&delay, nullptr);
  sleep(2);
  //astro_client->RefreshDevices();

#if 0
  std::cout << "known_devices (from ConnectAstroINDI()):\n";
  for (auto dev : known_devices) {
    std::cout << dev->device_name << std::endl;
    // Now need to sort out which device is which, using getDriverInterface()

  }
#endif
  AstroAttached = true;
}

AstroDevice *GetDeviceByName(const char *dev_name) {
  for(auto d : known_devices) {
    if (strcmp(d->device_name, dev_name) == 0) {
      return d;
    }
  }
  return nullptr;
}

AstroProperty *GetPropertyByName(const INDI::Property property) {
  AstroDevice *this_device = GetDeviceByName(property.getDeviceName());
  if (this_device == nullptr) return nullptr;
  
  for (auto p : known_properties) {
    if (p->device == this_device and
	strcmp(p->property_name, property.getName()) == 0) {
      return p;
    }
  }
  return nullptr;
}
  
const char *active_camera = nullptr;
const char *active_mount = nullptr;
const char *active_focuser = nullptr;
const char *active_cfw = nullptr;

// An AstroDevice can be in one of several states:
//    1) The device is known to exist, but the "DRIVER_INFO" property
//    hasn't arrived yet. New property notifications are accumulating
//    in pending_properties.
//    2) DRIVER_INFO has arrived, so properties in pending_properties
//    are being processed instead of being held.
//    3) The CONNECT property has arrived along with a device-specific
//    set of additional "connect-prerequisite"
//    properties. If the hardware is already connected, setup is
//    complete. If not, connection prerequisites are set up (e.g.,
//    DEVICE_PORT) and then CONNECT is enabled.
//    4) We wait for the CONNECT property to indicate that connection
//    has happened.
//    5) The device is fully connected.
AstroDevice::AstroDevice(INDI::BaseDevice dp, AstroClient *client) : local_client(client),
								     indi_device(dp) {
  this->device_name = strdup(dp.getDeviceName());
}

AstroDevice::~AstroDevice(void) {
  free((void *) this->device_name);
}

void AstroClient::newDevice(INDI::BaseDevice dp) {
  //std::cout << "Received newDevice notification.\n"; std::cout.flush();
  const char *indi_name = dp.getDeviceName();
  AstroDevice *this_device = GetDeviceByName(indi_name);
  if (this_device == nullptr) {
    //std::cout << "This is a new device: " << indi_name << '\n'; std::cout.flush();
    this_device = new AstroDevice(dp, this);
    known_devices.push_back(this_device);

    bool dev_found = false;
    for (const KnownDevice &kd : predefined_devices) {
      if (strcmp(kd.indi_device_name, indi_name) == 0) {
	dev_found = true;
	for (AstroDevice **p : kd.dev_pointers) {
	  (*p) = this_device;
	}
	for (AstroDeviceType dt : kd.devtype) {
	  switch(dt) {
	  case ccd_t:
	    camera = new CAMERA_INDI(this_device, kd.connection_port, kd.local_device_name);
	    break;
	  case fine_focus_t:
	    fine_focuser = new FOCUSER_INDI(this_device, kd.connection_port);
	    focuser = fine_focuser;
	    break;
	  case coarse_focus_t:
	    coarse_focuser = new FOCUSER_INDI(this_device, kd.connection_port);
	    if (system_config.NumFocusers() == 1) focuser = coarse_focuser;
	    break;
	  case cooler_t:
	    cooler = new CCDCooler(this_device, kd.connection_port);
	    break;
	  case cfw_t:
	    cfw = new CFW_INDI(this_device, kd.connection_port);
	    break;
	  case mount_t:
	    mount = new MOUNT_INDI(this_device, kd.connection_port);
	    break;
	  }
	}
      }
    }
    if (not dev_found) {
      std::cerr << "Warning: device " << indi_name
		<< " being ignored (no match to predefined_devices).\n";
    }
  }
}

void PurgeProperties(AstroDevice *d) {
  // STEP 1: Get rid of lookups
  for (AstroValue *element : d->lookups) {
    element->available = false;
    if (element->property) {
      element->property->value_list.clear();
      element->property->initialization_list.clear();
    }
  }
  d->lookups.clear();
  d->is_connected = false;
  d->driver_info_avail = false;
  d->pending_properties.clear();

  for (AstroProperty *p : d->properties) {
    p->value_list.clear();
    p->initialization_list.clear();
  }
}

void AstroClient::removeDevice(INDI::BaseDevice dp) {
  std::cout << "Received removeDevice notification: "
	    << dp.getDeviceName() << '\n';
  AstroDevice *this_device = GetDeviceByName(dp.getDeviceName());
  if (this_device) {
    PurgeProperties(this_device);
    known_devices.remove(this_device);
  }
}

LocalDevice::LocalDevice(AstroDevice *ad, const char *connection_port) : astro_device(ad) {
  this->astro_device->connection_port = connection_port;
  //std::cerr << "Completed LocalDevice() init for " << this->astro_device->device_name << std::endl;
}

void
AstroDevice::ConnectToHardware(void) {
  //std::cerr << "ConnectToHardware() started for "
  //	    << this->device_name << std::endl;
  this->local_client->connectDevice(this->device_name);
}

bool
LocalDevice::WaitForConnect(int timeout_secs) {
  //std::cerr << "WaitForConnect() started waiting\n";
  // already connected?
  if (astro_device->is_connected) return true;

  // set the CONNECT property
#if 1
  this->astro_device->ConnectToHardware();
#else
  INDI::PropertySwitch connect_prop = astro_device->indi_device.getSwitch("CONNECT");

  //std::cerr << "WaitForConnect(): setting CONNECT property state.\n";
  connect_prop[0].setState(ISS_ON);
  connect_prop[1].setState(ISS_OFF);
  astro_device->local_client->sendNewSwitch(connect_prop);
#endif
  int tries = 100 * timeout_secs;
  do {
    usleep(10000); // 10 msec
  } while(tries-- > 0 and astro_device->is_connected == false);
  return astro_device->is_connected;
}
    
static std::string CurrentDateTime(void) {
  struct tm t_data;
  time_t now = time(0);
  (void) localtime_r(&now, &t_data);
  char buffer[128];
  strftime(buffer, sizeof(buffer), "%D %T (%Z)", &t_data);
  return std::string(buffer);
}

void
AstroClient::logProperty(INDI::Property property) {
  property_log << CurrentDateTime() << ' '
	       << property.getName() << " ["
	       << property.getStateAsString() << "] ";
  int p_size;
  switch(property.getType()) {
  case INDI_NUMBER:
    p_size = property.getNumber()->count();
    break;
  case INDI_SWITCH:
    p_size = property.getSwitch()->count();
    break;
  case INDI_LIGHT:
    p_size = property.getLight()->count();
    break;
  case INDI_TEXT:
    p_size = property.getText()->count();
    break;
  case INDI_BLOB:
    p_size = property.getBLOB()->count();
    break;
  default:
  case INDI_UNKNOWN:
    p_size = 0;
    break;
  }
  for (int i=0; i<p_size; i++) {
    if (p_size > 3) {
      property_log << "\n    ... ";
    } else {
      property_log << " ";
    }
    switch(property.getType()) {
    case INDI_NUMBER:
      property_log << property.getNumber()->at(i)->getName()
		   << ": " << property.getNumber()->at(i)->getValue();
      break;
    case INDI_SWITCH:
      property_log << property.getSwitch()->at(i)->getName()
		   << ": " << property.getSwitch()->at(i)->getStateAsString();
      break;
    case INDI_LIGHT:
      property_log << property.getLight()->at(i)->getName()
		   <<": " << property.getLight()->at(i)->getStateAsString();
      break;
    case INDI_TEXT:
      property_log << property.getText()->at(i)->getName()
		   << ": " << property.getText()->at(i)->getText();
      break;
    case INDI_BLOB:
      break;
    case INDI_UNKNOWN:
      break;
    }
    if (p_size <= 3) {
      property_log << ",";
    }
  }
  property_log << std::endl;
  property_log.flush();
}

void AstroClient::newProperty(INDI::Property property) {
  //this->logProperty(property);
  //std::cout << "Received newProperty notification: "
  //	    << property.getDeviceName() << ": "
  //	    << property.getName() << '\n';
  AstroDevice *this_device = GetDeviceByName(property.getDeviceName());
  // ignore properties for devices we don't know about
  if (this_device == nullptr) {
    std::cerr << "newProperty() ignoring property for unknown device.\n";
    return;
  }
  
  // Do we already know about this property? (Don't want to create
  // multiple AstroProperty objects for one property.)
  for (auto p : known_properties) {
    if (p->device == this_device and
	strcmp(p->property_name, property.getName()) == 0) {
      std::cerr << "newProperty() ignoring property previously processed.\n";
      return; // we already know about this property
    }
  }

  // Hold the property (even if only temporarily) on the
  // pending_properties list.
  this_device->pending_properties.push_back(property);
  if (strcmp(property.getName(), "DRIVER_INFO") == 0) {
    this_device->driver_info_avail = true;
  }
  
  if (this_device->driver_info_avail) {
    this_device->ProcessPendingProperties();
  } else {
#if 0
    if (!this_device->driver_info_avail) {
      std::cerr << "newProperty() still waiting for driver_info.\n";
    }
    ;
#endif
  }

  if (strcmp(property.getName(), "CONNECTION") == 0) {
    AstroDevice *this_device = GetDeviceByName(property.getDeviceName());
    this_device->is_connected = (property.getSwitch()->at(0)->getState() == ISS_ON);
    //std::cerr << "CONNECTION property update. is_connected for '" << property.getDeviceName()
    //	      << "' set " << this_device->is_connected << std::endl;
  }
    

  // Initiate a connection, if it's time
  if (this_device->connection_port != nullptr and
      strcmp(property.getName(), "DEVICE_PORT") == 0) {
    property.getText()->at(0)->setText(this_device->connection_port);
    this->sendNewText(property);
    // let  this be processed
    //sleep(1);
    this_device->ConnectToHardware();
  } else if (strcmp(property.getName(), "CONNECTION") == 0) {
    this_device->ConnectToHardware();
  }
}

void AstroDevice::ProcessPendingProperties(void) {
  // this is a new property for a known device...
  //std::cerr << "AstroDevice::ProcessPendingProperties()\n";
  while(this->pending_properties.size()) {
    //std::cerr << "... popping a property.\n";
    INDI::Property property = this->pending_properties.front();
    this->pending_properties.pop_front();
    AstroProperty *this_property = new AstroProperty(property, this);
    this->properties.push_back(this_property);
  }
  //std::cerr << "AstroDevice::ProcessPendingProperties() finished.\n";
}

AstroClient::AstroClient(void) {
  ;
}

AstroClient::~AstroClient(void) {
  ; // should probably be clearing and deleting known_properties and
    // known_devices  
}

void
AstroClient::RefreshDevices(void) {
  std::vector<INDI::BaseDevice> q_devices = this->getDevices();
  std::cout << "getDevices() returned "
	    << q_devices.size() << " devices.\n";
  for (auto &d : q_devices) {
    this->newDevice(d);
  }

  for (auto p : known_devices) {
    const INDI::Properties properties = p->indi_device.getProperties();
    for (auto prop : properties) {
      this->newProperty(prop);
    }
  }
}

//****************************************************************
//        Camera APIs (includes CFW commands)
//****************************************************************
void AstroConnectToCamera(void) {
  if (ccd_dev == nullptr or not ccd_dev->is_connected) {
    std::cerr << "No camera avail; waiting 5 sec to see if it appears.\n";
    sleep(5);
  }
  if (ccd_dev == nullptr or not ccd_dev->is_connected) {
    std::cerr << "Still no camera. Quitting.\n";
    exit(-1);
  }
}

void AstroChangeTempSetpoint(double temp) { // CCD_TEMPERATURE
  INDI::PropertyNumber ccdTemperature = ccd_dev->indi_device.getProperty("CCD_TEMPERATURE");
  if (!ccdTemperature.isValid()) {
    std::cerr << "Error: unable to find CCD_TEMPERATURE property.\n";
  } else {
    // is valid...
    ccdTemperature[0].setValue(temp);
    astro_client->sendNewProperty(ccdTemperature);
  }
}

void AstroChangeCoolerPower(double power); // 0 .. 100 CCD_COOLER_POWER.CCD_COOLER_VALUE
void AstroSelectFilter(int position); // FILTER_SLOT.FILTER_SLOT_VALUE
void AstroSetBinning(int binning);
void AstroResetFrame(void);
void AstroSetExposureType(int type); // EXP_TYPE_LIGHT or EXP_TYPE_DARK
void AstroSetFrame(int box_left, int box_right, int box_bottom, int box_top);
void *AstroExpose(double time_in_seconds, bool block);
void *AstroExposeWaitForDone(void); // blocks until done
double SecondsRemainingInExposure(void);
void AstroSetExposureType(ExposureType exposure_type);

void
AstroValueSwitch::setState(ISState value) {
  property->device->local_client->Log(std::string("setState[") +
				      value_name +
				      "] to " +
				      std::to_string(value));
  property->indi_property.getSwitch()->at(property_index)->setState(value);
}

ISState
AstroValueSwitch::getState(void) {
  return property->indi_property.getSwitch()->at(property_index)->getState();
}

IPState
AstroValueSwitch::getINDIState(void) {
  return property->indi_property.getSwitch()->getState();
}

IPState
AstroValueNumber::getINDIState(void) {
  return property->indi_property.getNumber()->getState();
}

void
AstroValueText::setValue(const char *value) {
  property->device->local_client->Log(std::string("setValue[") +
				      value_name +
				      "] to " +
				      value);
  property->indi_property.getText()->at(property_index)->setText(value);
}

void
AstroValueNumber::setValue(double value) {
  property->device->local_client->Log(std::string("setValue[") +
				      value_name +
				      "] to " +
				      std::to_string(value));
  property->indi_property.getNumber()->at(property_index)->setValue(value);
}

double
AstroValueNumber::getValue(void) {
  return property->indi_property.getNumber()->at(property_index)->getValue();
}

double
AstroValueNumber::getMin(void) const {
  return property->indi_property.getNumber()->at(property_index)->getMin();
}

double
AstroValueNumber::getMax(void) const {
  return property->indi_property.getNumber()->at(property_index)->getMax();
}

void
AstroValueNumber::Initialize(double init_value) {
  AstroInitializeNumber *ain = new AstroInitializeNumber(this, init_value);
  this->initialization_list.push_back(ain);
}

void
AstroValueText::Initialize(const char *init) {
  AstroInitializeText *ait = new AstroInitializeText(this, init);
  this->initialization_list.push_back(ait);
}

void
AstroValue::SendINDIUpdate(void) {
  AstroClient *my_client = this->property->device->local_client;
  INDI::Property indi_property(this->property->indi_property);
  
  switch (this->property->property_type) {
  case INDI_NUMBER:
    return my_client->sendNewNumber(indi_property);
  case INDI_SWITCH:
    return my_client->sendNewSwitch(indi_property);
  case INDI_LIGHT:
    std::cerr << "SendINDIUpdate(): Can't change read-only INDI_LIGHT property.\n";
    return;
  case INDI_TEXT:
    return my_client->sendNewText(indi_property);
  case INDI_BLOB:
    std::cerr << "SendINDIUpdate(): BLOB sending not implemented.\n";
    break;
  case INDI_UNKNOWN:
    std::cerr << "SendINDIUpdate(): Can't send property of type UNKNOWN.\n";
    break;
  }
}

void
AstroClient::Log(std::string message) {
  property_log << CurrentDateTime() << ' ' << message << '\n';
  property_log.flush();
}


void
AstroClient::updateProperty(INDI::Property property) {
  this->logProperty(property);
  if (strcmp(property.getName(), "CONNECTION") == 0) {
    AstroDevice *this_device = GetDeviceByName(property.getDeviceName());
    this_device->is_connected = (property.getSwitch()->at(0)->getState() == ISS_ON);
    //std::cerr << "CONNECT property update. is_connected for '" << property.getDeviceName()
    //	      << "' set " << this_device->is_connected << std::endl;
  }
}


//#define TEST

// To start the server:
// indiserver -v indi_simulator_ccd indi_simulator_telescope indi_simulator_focus
//
#ifdef TEST
int main(int argc, char **argv) {
  ConnectAstroINDI();

  sleep(10);

  std::cout << "known_devices, from main():\n";
  for (auto dev : known_devices) {
    std::cout << dev->device_name << std::endl;
    //uint16_t interfaces = dev->indi_device->getDriverInterface();
    //if (interfaces & INDI::BASE_DEVICE::TELESCOPE_INTERFACE) {
    //if (mount_dev) {
    // Now need to sort out which device is which, using getDriverInterface()
  }
  return 0;
}
#endif

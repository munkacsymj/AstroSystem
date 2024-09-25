#include <stdio.h>
#include <qhyccd.h>

qhyccd_handle *camhandle = nullptr;

int main(int argc, char **argv) {

  int ret = InitQHYCCDResource();
  if (ret == QHYCCD_SUCCESS) {
    fprintf(stderr, "InitQHYCCDResource() completed okay.\n");
  } else {
    fprintf(stderr, "InitQHYCCDResource() failed.\n");
  }

  int num = ScanQHYCCD();
  fprintf(stderr, "Found %d camera(s).\n", num);
  if (num == 0) {
    fprintf(stderr, "No camera found. Give up.\n");
    exit(3);
  }
  if (num > 1) {
    fprintf(stderr, "Multiple cameras found. Give up.\n");
    exit(3);
  }

  char id[32];
  char CameraModelName[64];
  ret = GetQHYCCDId(0, id);
  if (ret == QHYCCD_SUCCESS) {
    fprintf(stderr, "GetQHYCCDId() returned %s\n", id);
    ret = GetQHYCCDModel(id, CameraModelName);
    if (ret == QHYCCD_SUCCESS) {
      fprintf(stderr, "GetQHYCCDModel() returned %s\n", CameraModelName);
    } else {
      fprintf(stderr, "GetQHYCCDModel() failed.\n");
    }
  } else {
    fprintf(stderr, "GetQHYCCDId() failed.\n");
  }
  
  camhandle = OpenQHYCCD(id);
  if (camhandle != nullptr) {
    fprintf(stderr, "OpenQHYCCD() successful.\n");
  } else {
    fprintf(stderr, "OpenQHYCCD() failed.\n");
  }

  ret = SetQHYCCDStreamMode(camhandle, 0x00);
  if (ret == QHYCCD_SUCCESS) {
    fprintf(stderr, "Mode successfully set to SingleFrameMode.\n");
  } else {
    fprintf(stderr, "SetQHYCCDStreamMode() failed.\n");
  }

  ret = InitQHYCCD(camhandle);
  if (ret == QHYCCD_SUCCESS) {
    fprintf(stderr, "Camera init completed: success.\n");
  } else {
    fprintf(stderr, "Camera init failed.\n");
  }


  ret = IsQHYCCDControlAvailable(camhandle, CONTROL_CURTEMP);
  fprintf(stderr, "get/set current temp: %d\n", ret);

  ret = IsQHYCCDControlAvailable(camhandle, CONTROL_CURPWM);
  fprintf(stderr, "get/set current PWM: %d\n", ret);

  ret = IsQHYCCDControlAvailable(camhandle, CONTROL_COOLER);
  fprintf(stderr, "get targetTemp: %d\n", ret);

  ret = IsQHYCCDControlAvailable(camhandle, CONTROL_MANULPWM);
  fprintf(stderr, "set manual mode: %d\n", ret);

  ret = SetQHYCCDParam(camhandle, CONTROL_MANULPWM, 30);
  fprintf(stderr, "set PWM: %d\n", ret);

  double v;
  v = GetQHYCCDParam(camhandle, CONTROL_CURTEMP);
  fprintf(stderr, "get CONTROL_CURTEMP: %lf\n", v);

  v = GetQHYCCDParam(camhandle, CONTROL_CURPWM);
  fprintf(stderr, "get CONTROL_CURPWM: %lf\n", v);

  v = GetQHYCCDParam(camhandle, CONTROL_COOLER);
  fprintf(stderr, "get CONTROL_COOLER: %lf\n", v);

  return 0;
}

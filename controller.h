#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <libusb-1.0/libusb.h>

typedef enum {
  CONTROLLER_TYPE_UNKNOWN = 0,
  CONTROLLER_TYPE_XBOX_360,
  CONTROLLER_TYPE_XBOX_ONE,
  CONTROLLER_TYPE_PLAYSTATION_DS3,
  CONTROLLER_TYPE_PLAYSTATION_DS4,
  CONTROLLER_TYPE_NINTENDO_SWITCH,
  CONTROLLER_TYPE_GENERIC_HID,
  CONTROLLER_TYPE_OTHER
} ControllerType;

#define VENDOR_MICROSOFT 0x045e
#define VENDOR_SONY 0x054c
#define VENDOR_NINTENDO 0x057e
#define VENDOR_LOGITECH 0x046d
#define VENDOR_VALVE 0x28de

#define PRODUCT_XBOX_360 0x028e
#define PRODUCT_XBOX_360_W 0x0719
#define PRODUCT_XBOX_ONE 0x02ea
#define PRODUCT_XBOX_ONE_S 0x02fd
#define PRODUCT_XBOX_ONE_ELITE 0x02e3

#define PRODUCT_DS3 0x0268
#define PRODUCT_DS4 0x05c4
#define PRODUCT_DS4_V2 0x09cc

#define PRODUCT_SWITCH_PRO 0x2009
#define PRODUCT_JOYCON_L 0x2006
#define PRODUCT_JOYCON_R 0x2007

#define PRODUCT_F310 0xc216
#define PRODUCT_F510 0xc218
#define PRODUCT_F710 0xc219

#define PRODUCT_STEAM_CONTROLLER 0x1102

typedef struct {
  libusb_device *device;
  ControllerType type;
  uint16_t vendor_id;
  uint16_t product_id;
  char name[64];
} ControllerInfo;

ControllerType detect_controller_type(libusb_device *device);
int is_controller(libusb_device *device);
int open_controller(libusb_device *device, libusb_device_handle **handle);
void close_controller(libusb_device_handle *handle);
int get_controller_name(libusb_device *device, char *name, size_t name_size);
int find_all_controllers(libusb_context *lctx, ControllerInfo **controllers,
                         int *count);
const char *controller_type_to_string(ControllerType type);

#endif /* CONTROLLER_H */

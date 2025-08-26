#include "controller.h"
#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>

ControllerType detect_controller_type(libusb_device *device) {
  struct libusb_device_descriptor DESC;

  if (libusb_get_device_descriptor(device, &DESC) != 0) {
    return CONTROLLER_TYPE_UNKNOWN;
  }

  if (DESC.idVendor == VENDOR_MICROSOFT) {
    switch (DESC.idProduct) {
    case PRODUCT_XBOX_360:
    case PRODUCT_XBOX_360_W:
      return CONTROLLER_TYPE_XBOX_360;
    case PRODUCT_XBOX_ONE:
    case PRODUCT_XBOX_ONE_S:
    case PRODUCT_XBOX_ONE_ELITE:
      return CONTROLLER_TYPE_XBOX_ONE;
    }
  }

  if (DESC.idVendor == VENDOR_SONY) {
    switch (DESC.idProduct) {
    case PRODUCT_DS3:
      return CONTROLLER_TYPE_PLAYSTATION_DS3;
    case PRODUCT_DS4:
    case PRODUCT_DS4_V2:
      return CONTROLLER_TYPE_PLAYSTATION_DS4;
    }
  }

  if (DESC.idVendor == VENDOR_NINTENDO) {
    switch (DESC.idProduct) {
    case PRODUCT_SWITCH_PRO:
    case PRODUCT_JOYCON_L:
    case PRODUCT_JOYCON_R:
      return CONTROLLER_TYPE_NINTENDO_SWITCH;
    }
  }

  if (DESC.idVendor == VENDOR_LOGITECH) {
    switch (DESC.idProduct) {
    case PRODUCT_F310:
    case PRODUCT_F510:
    case PRODUCT_F710:
      return CONTROLLER_TYPE_GENERIC_HID;
    }
  }

  if (DESC.idVendor == VENDOR_VALVE &&
      DESC.idProduct == PRODUCT_STEAM_CONTROLLER) {
    return CONTROLLER_TYPE_OTHER;
  }
  return CONTROLLER_TYPE_UNKNOWN;
}
int is_controller(libusb_device *device) {
  ControllerType type = detect_controller_type(device);
  return (type != CONTROLLER_TYPE_UNKNOWN);
}

int get_controller_name(libusb_device *device, char *name, size_t name_size) {
  struct libusb_device_descriptor DESC;

  if (libusb_get_device_descriptor(device, &DESC) != 0) {
    return -1;
  }

  ControllerType type = detect_controller_type(device);
  const char *type_name = controller_type_to_string(type);

  if (type != CONTROLLER_TYPE_UNKNOWN) {
    snprintf(name, name_size, "%s (0x%04x:0x%04x)", type_name, DESC.idVendor,
             DESC.idProduct);
  } else {
    snprintf(name, name_size, "Unknown Controller (0x%04x:0x%04x)",
             DESC.idVendor, DESC.idProduct);
  }
  return 0;
}

int find_all_controllers(libusb_context *lctx, ControllerInfo **controllers,
                         int *count) {
  libusb_device **list;
  ssize_t cnt = libusb_get_device_list(lctx, &list);

  if (cnt < 0) {
    fprintf(stderr, "Failed to get list of devices: %s\n",
            libusb_strerror((int)cnt));
    return -1;
  }

  int controller_count = 0;
  for (int i = 0; i < cnt; i++) {
    if (is_controller(list[i])) {
      controller_count++;
    }
  }

  if (controller_count == 0) {
    libusb_free_device_list(list, 1);
    *controllers = NULL;
    *count = 0;
    return 0;
  }

  ControllerInfo *found_controllers =
      malloc(controller_count * sizeof(ControllerInfo));

  if (!found_controllers) {
    libusb_free_device_list(list, 1);
    return -1;
  }

  int index = 0;
  for (int i = 0; i < cnt; i++) {
    if (is_controller(list[i])) {
      struct libusb_device_descriptor DESC;
      found_controllers[index].device = list[i];
      found_controllers[index].type = detect_controller_type(list[i]);
      libusb_get_device_descriptor(list[i], &DESC);
      found_controllers[index].vendor_id = DESC.idVendor;
      found_controllers[index].product_id = DESC.idProduct;
      get_controller_name(list[i], found_controllers[index].name,
                          sizeof(found_controllers[index].name));
      index++;
    }
  }

  libusb_free_device_list(list, 1);
  *controllers = found_controllers;
  *count = controller_count;
  return 0;
}

int open_controller(libusb_device *device, libusb_device_handle **handle) {
  int err = libusb_open(device, handle);
  if (err) {
    fprintf(stderr, "Failed to open device: %s\n", libusb_strerror(err));
    return -1;
  }
  return 0;
}

void close_controller(libusb_device_handle *handle) {
  if (handle) {
    libusb_close(handle);
  }
}

const char *controller_type_to_string(ControllerType type) {
  switch (type) {
  case CONTROLLER_TYPE_XBOX_360:
    return "Xbox 360 Controller";
  case CONTROLLER_TYPE_XBOX_ONE:
    return "Xbox One Controller";
  case CONTROLLER_TYPE_PLAYSTATION_DS3:
    return "PlayStation DualShock 3";
  case CONTROLLER_TYPE_PLAYSTATION_DS4:
    return "PlayStation DualShock 4";
  case CONTROLLER_TYPE_NINTENDO_SWITCH:
    return "Nintendo Switch Controller";
  case CONTROLLER_TYPE_GENERIC_HID:
    return "Generic HID Controller";
  case CONTROLLER_TYPE_OTHER:
    return "Other Controller";
  default:
    return "Unknown Controller";
  }
}

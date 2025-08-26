#include "controller.h"
#include <libusb-1.0/libusb.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

static volatile int keep_reading = 0;
static libusb_device_handle *active_handle = NULL;

/*
 *
 * Por agora estou a utilizar Synchronous device I/O q é blocking segundo a
 * documentação da libusb Talvez troque para Asynchronous device I/O para fazer
 * macros, complex binds e GUI(no maximo vou usar um TUI), etc...
 *
 */

int read_input(libusb_device_handle *handle, ControllerState *state) {
  uint8_t buffer[MAX_INPUT_PACKET_SIZE]; // Devo mudar isto para usar a
                                         // libusb_get_max_iso_packet_size()
  int actual_lenght;
  int ret;

  // So para Xbox360, tenho que usar um switch mais tarde
  ret = libusb_interrupt_transfer(handle, 0x81, buffer, sizeof(buffer),
                                  &actual_lenght, 0);

  if (ret == LIBUSB_ERROR_TIMEOUT)
    return 0;

  if (actual_lenght >= 20) {

    state->buttons[0] = buffer[2];
    state->buttons[1] = buffer[4];

    state->left_thumb_x = (int16_t)((buffer[7] << 8) | buffer[6]);
    state->left_thumb_y = (int16_t)((buffer[9] << 8) | buffer[8]);
    state->right_thumb_x = (int16_t)((buffer[11] << 8) | buffer[10]);
    state->right_thumb_y = (int16_t)((buffer[13] << 8) | buffer[12]);

    state->left_trigger = buffer[14];
    state->right_trigger = buffer[15];

    return 1;
  }

  return 0;
}

void *input_reader_thread(void *arg) {
  (void)arg;
  ControllerState state;

  while (keep_reading) {
    int result = read_input(active_handle, &state);

    if (result > 0) {
      printf("Buttons: %02x %02x | L: (%d,%d) | R: (%d,%d) | Triggers: %d,%d\n",
             state.buttons[0], state.buttons[1], state.left_thumb_x,
             state.left_thumb_y, state.right_thumb_x, state.right_thumb_y,
             state.left_trigger, state.right_trigger);
    } else if (result < 0) {
      break;
    }
    usleep(500000);
  }
  return NULL;
}

int start_input_reader(libusb_device_handle *handle) {
  pthread_t thread;
  int ret;

  if (libusb_kernel_driver_active(handle, 0) == 1) {
    ret = libusb_detach_kernel_driver(handle, 0);
    if (ret != 0) {
      fprintf(stderr, "Failed to detach kernel driver: %s\n",
              libusb_strerror(ret));
      return -1;
    }
    printf("Kernel driver detached");
  }
  ret = libusb_claim_interface(handle, 0);
  if (ret != 0) {
    fprintf(stderr, "Failed to claim the interface: %s\n",
            libusb_strerror(ret));
    return -1;
  }

  active_handle = handle;
  keep_reading = 1;

  ret = pthread_create(&thread, NULL, input_reader_thread, NULL);
  if (ret != 0) {
    fprintf(stderr, "Failed to innitialize reader thread\n");
    keep_reading = 0;
    libusb_release_interface(handle, 0);
    return -1;
  }

  pthread_detach(thread);
  return 0;
}

void stop_input_reader() {
  keep_reading = 0;
  if (active_handle) {
    libusb_release_interface(active_handle, 0);
    active_handle = NULL;
  }
}

int is_button_pressed(const ControllerState *state, int button) {
  switch (button) {
  case XBOX_BUTTON_A:
    return (state->buttons[0] & 0x01);
  case XBOX_BUTTON_B:
    return (state->buttons[0] & 0x02);
  case XBOX_BUTTON_X:
    return (state->buttons[0] & 0x04);
  case XBOX_BUTTON_Y:
    return (state->buttons[0] & 0x08);
  case XBOX_BUTTON_LB:
    return (state->buttons[0] & 0x10);
  case XBOX_BUTTON_RB:
    return (state->buttons[0] & 0x20);
  case XBOX_BUTTON_BACK:
    return (state->buttons[0] & 0x40);
  case XBOX_BUTTON_START:
    return (state->buttons[0] & 0x80);
  case XBOX_BUTTON_HOME:
    return (state->buttons[1] & 0x04);
  case XBOX_BUTTON_L3:
    return (state->buttons[1] & 0x20);
  case XBOX_BUTTON_R3:
    return (state->buttons[1] & 0x40);
  default:
    return 0;
  }
}

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

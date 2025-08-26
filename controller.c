#include "controller.h"
#include <libusb-1.0/libusb.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
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
    // Parse buttons correctly
    uint8_t buttons1 = buffer[2];
    uint8_t buttons2 = buffer[3];

    // Main buttons (buttons1)
    state->buttons[0] = buttons1; // Store raw button bytes for debugging
    state->buttons[1] = buttons2;

    state->dpad_up = (buttons1 & 0x01) ? 1 : 0;    // Bit 0 - Up
    state->dpad_down = (buttons1 & 0x02) ? 1 : 0;  // Bit 1 - Down
    state->dpad_left = (buttons1 & 0x04) ? 1 : 0;  // Bit 2 - Left
    state->dpad_right = (buttons1 & 0x08) ? 1 : 0; // Bit 3 - Right

    // A/B/X/Y buttons are in buttons2 (buffer[3]) - upper 4 bits
    state->a_button = (buttons2 & 0x10) ? 1 : 0; // Bit 4 - A
    state->b_button = (buttons2 & 0x20) ? 1 : 0; // Bit 5 - B
    state->x_button = (buttons2 & 0x40) ? 1 : 0; // Bit 6 - X
    state->y_button = (buttons2 & 0x80) ? 1 : 0; // Bit 7 - Y

    // Other buttons - we need to test these to see where they are
    // For now, let's assume they might be in the remaining bits
    state->lb_button = 0;    // Need to test LB
    state->rb_button = 0;    // Need to test RB
    state->back_button = 0;  // Need to test Back
    state->start_button = 0; // Need to test Start
    state->l3_button = 0;    // Need to test L3
    state->r3_button = 0;    // Need to test R3
    state->xbox_button = 0;  // Need to test Xbox

    return 1;
  }

  return 0;
}

int wait_for_button_press(libusb_device_handle *handle, const char *button_name,
                          uint8_t *found_byte,
                          uint8_t *found_bit) { // Use uint8_t, not u_int8_t
  uint8_t buffer[MAX_INPUT_PACKET_SIZE];
  uint8_t last_buffer[MAX_INPUT_PACKET_SIZE] = {0};
  int attempts = 0;

  printf("Press the %s button and hold until told\n",
         button_name); // Add newline

  while (attempts < 1200) { // Increase timeout attempts
    int actual_length = 0;  // Fix spelling: actual_length, not actual_lenght
    int ret = libusb_interrupt_transfer(handle, 0x81, buffer, sizeof(buffer),
                                        &actual_length, 100);

    if (ret ==
        LIBUSB_ERROR_TIMEOUT) { // Check for timeout, not LIBUSB_ERROR_OTHER
      attempts++;
      usleep(10000); // Small delay between attempts
      continue;
    }

    if (ret != 0 || actual_length < 20) {
      attempts++;
      continue;
    }

    // DEBUG: Print raw data to see what's happening
    for (int byte = 2; byte < 6; byte++) {
      for (int bit = 0; bit < 8; bit++) {
        uint8_t mask = (1 << bit);
        if ((buffer[byte] & mask) && !(last_buffer[byte] & mask)) {
          *found_byte = byte;
          *found_bit = bit;

          printf("Detected %s button on byte %d bit %d\n", button_name,
                 *found_byte, *found_bit); // Add * to dereference pointers

          printf("Please release the button\n");

          // Wait for button release
          int release_attempts = 0;
          while (release_attempts < 50) {
            ret = libusb_interrupt_transfer(handle, 0x81, buffer,
                                            sizeof(buffer), &actual_length, 50);

            if (ret == 0 && actual_length >= 20) {
              if (!(buffer[byte] & mask)) {
                printf("Button released! Ready for next input\n");
                return 0;
              }
            }
            release_attempts++;
            usleep(10000);
          }
          printf("Timeout waiting for button release\n");
          return -1;
        }
      }
    }
    memcpy(last_buffer, buffer, sizeof(last_buffer));
    attempts++;
    usleep(10000);
  }
  printf("Timeout waiting for button press\n");
  return -1;
}
int interactive_setup(libusb_device_handle *handle, ControllerConfig *config) {
  printf("=== Controller Interactive Setup ===\n");
  printf("We'll now map each button. Press each button when prompted.\n\n");

  // Map each button
  if (wait_for_button_press(handle, "A", &config->a_button_byte,
                            &config->a_button_bit) != 0)
    return -1;
  if (wait_for_button_press(handle, "B", &config->b_button_byte,
                            &config->b_button_bit) != 0)
    return -1;
  if (wait_for_button_press(handle, "X", &config->x_button_byte,
                            &config->x_button_bit) != 0)
    return -1;
  if (wait_for_button_press(handle, "Y", &config->y_button_byte,
                            &config->y_button_bit) != 0)
    return -1;
  if (wait_for_button_press(handle, "Left Bumper (LB)", &config->lb_button_byte,
                            &config->lb_button_bit) != 0)
    return -1;
  if (wait_for_button_press(handle, "Right Bumper (RB)",
                            &config->rb_button_byte,
                            &config->rb_button_bit) != 0)
    return -1;
  if (wait_for_button_press(handle, "Back", &config->back_button_byte,
                            &config->back_button_bit) != 0)
    return -1;
  if (wait_for_button_press(handle, "Start", &config->start_button_byte,
                            &config->start_button_bit) != 0)
    return -1;
  if (wait_for_button_press(handle, "Left Stick Click (L3)",
                            &config->l3_button_byte,
                            &config->l3_button_bit) != 0)
    return -1;
  if (wait_for_button_press(handle, "Right Stick Click (R3)",
                            &config->r3_button_byte,
                            &config->r3_button_bit) != 0)
    return -1;
  if (wait_for_button_press(handle, "Xbox Guide", &config->xbox_button_byte,
                            &config->xbox_button_bit) != 0)
    return -1;
  if (wait_for_button_press(handle, "D-pad Up", &config->dpad_up_byte,
                            &config->dpad_up_bit) != 0)
    return -1;
  if (wait_for_button_press(handle, "D-pad Down", &config->dpad_down_byte,
                            &config->dpad_down_bit) != 0)
    return -1;
  if (wait_for_button_press(handle, "D-pad Left", &config->dpad_left_byte,
                            &config->dpad_left_bit) != 0)
    return -1;
  if (wait_for_button_press(handle, "D-pad Right", &config->dpad_right_byte,
                            &config->dpad_right_bit) != 0)
    return -1;

  printf("Enter a name for this controller configuration: ");
  fgets(config->controller_name, sizeof(config->controller_name), stdin);
  config->controller_name[strcspn(config->controller_name, "\n")] =
      0; // Remove newline

  printf("\nSetup complete! Configuration saved.\n");
  return 0;
}

int read_controller_input_with_config(libusb_device_handle *handle,
                                      ControllerState *state,
                                      const ControllerConfig *config) {
  uint8_t buffer[MAX_INPUT_PACKET_SIZE];
  int actual_length;
  int ret;

  ret = libusb_interrupt_transfer(handle, 0x81, buffer, sizeof(buffer),
                                  &actual_length, 0);

  if (ret == LIBUSB_ERROR_TIMEOUT) {
    return 0;
  }

  if (ret < 0 || actual_length < 20) {
    return -1;
  }

  // Use the configuration to map buttons
  state->a_button =
      (buffer[config->a_button_byte] & (1 << config->a_button_bit)) ? 1 : 0;
  state->b_button =
      (buffer[config->b_button_byte] & (1 << config->b_button_bit)) ? 1 : 0;
  state->x_button =
      (buffer[config->x_button_byte] & (1 << config->x_button_bit)) ? 1 : 0;
  state->y_button =
      (buffer[config->y_button_byte] & (1 << config->y_button_bit)) ? 1 : 0;
  state->lb_button =
      (buffer[config->lb_button_byte] & (1 << config->lb_button_bit)) ? 1 : 0;
  state->rb_button =
      (buffer[config->rb_button_byte] & (1 << config->rb_button_bit)) ? 1 : 0;
  state->back_button =
      (buffer[config->back_button_byte] & (1 << config->back_button_bit)) ? 1
                                                                          : 0;
  state->start_button =
      (buffer[config->start_button_byte] & (1 << config->start_button_bit)) ? 1
                                                                            : 0;
  state->l3_button =
      (buffer[config->l3_button_byte] & (1 << config->l3_button_bit)) ? 1 : 0;
  state->r3_button =
      (buffer[config->r3_button_byte] & (1 << config->r3_button_bit)) ? 1 : 0;
  state->xbox_button =
      (buffer[config->xbox_button_byte] & (1 << config->xbox_button_bit)) ? 1
                                                                          : 0;
  state->dpad_up =
      (buffer[config->dpad_up_byte] & (1 << config->dpad_up_bit)) ? 1 : 0;
  state->dpad_down =
      (buffer[config->dpad_down_byte] & (1 << config->dpad_down_bit)) ? 1 : 0;
  state->dpad_left =
      (buffer[config->dpad_left_byte] & (1 << config->dpad_left_bit)) ? 1 : 0;
  state->dpad_right =
      (buffer[config->dpad_right_byte] & (1 << config->dpad_right_bit)) ? 1 : 0;

  // Analog inputs (these are usually standard)
  state->left_trigger = buffer[4];
  state->right_trigger = buffer[5];
  state->left_thumb_x = (int16_t)((buffer[7] << 8) | buffer[6]);
  state->left_thumb_y = (int16_t)((buffer[9] << 8) | buffer[8]);
  state->right_thumb_x = (int16_t)((buffer[11] << 8) | buffer[10]);
  state->right_thumb_y = (int16_t)((buffer[13] << 8) | buffer[12]);

  return 1;
}

void *input_reader_thread(void *arg) {
  (void)arg;
  ControllerState state;

  while (keep_reading) {
    int result = read_input(active_handle, &state);

    if (result > 0) {
      /*      printf("Buttons: A:%d B:%d X:%d Y:%d | ", state.a_button,
         state.b_button, state.x_button, state.y_button); printf("LB:%d RB:%d |
         ", state.lb_button, state.rb_button); printf("Back:%d Start:%d | ",
         state.back_button, state.start_button); printf("L3:%d R3:%d Xbox:%d |
         ", state.l3_button, state.r3_button, state.xbox_button); printf("D-pad:
         U:%d R:%d D:%d L:%d | ", state.dpad_up, state.dpad_right,
                   state.dpad_down, state.dpad_left);
            printf("L: (%d,%d) R: (%d,%d) | ", state.left_thumb_x,
         state.left_thumb_y, state.right_thumb_x, state.right_thumb_y);
            printf("Triggers: L:%d R:%d\n", state.left_trigger,
         state.right_trigger);
      */
    } else if (result < 0) {
      break;
    }
    usleep(10000);
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

void save_config(const ControllerConfig *config, const char *filename) {
  FILE *file = fopen(filename, "w");
  if (!file) {
    fprintf(stderr, "Failed to open config file %s for writing\n", filename);
    return;
  }

  fprintf(file, "[ControllerConfig]\n");
  fprintf(file, "name=%s\n", config->controller_name);

  fprintf(file, "a_button_byte=%d\n", config->a_button_byte);
  fprintf(file, "a_button_bit=%d\n", config->a_button_bit);

  fprintf(file, "b_button_byte=%d\n", config->b_button_byte);
  fprintf(file, "b_button_bit=%d\n", config->b_button_bit);

  fprintf(file, "x_button_byte=%d\n", config->x_button_byte);
  fprintf(file, "x_button_bit=%d\n", config->x_button_bit);

  fprintf(file, "y_button_byte=%d\n", config->y_button_byte);
  fprintf(file, "y_button_bit=%d\n", config->y_button_bit);

  fprintf(file, "lb_button_byte=%d\n", config->lb_button_byte);
  fprintf(file, "lb_button_bit=%d\n", config->lb_button_bit);

  fprintf(file, "rb_button_byte=%d\n", config->rb_button_byte);
  fprintf(file, "rb_button_bit=%d\n", config->rb_button_bit);

  fprintf(file, "back_button_byte=%d\n", config->back_button_byte);
  fprintf(file, "back_button_bit=%d\n", config->back_button_bit);

  fprintf(file, "start_button_byte=%d\n", config->start_button_byte);
  fprintf(file, "start_button_bit=%d\n", config->start_button_bit);

  fprintf(file, "l3_button_byte=%d\n", config->l3_button_byte);
  fprintf(file, "l3_button_bit=%d\n", config->l3_button_bit);

  fprintf(file, "r3_button_byte=%d\n", config->r3_button_byte);
  fprintf(file, "r3_button_bit=%d\n", config->r3_button_bit);

  fprintf(file, "xbox_button_byte=%d\n", config->xbox_button_byte);
  fprintf(file, "xbox_button_bit=%d\n", config->xbox_button_bit);

  fprintf(file, "dpad_up_byte=%d\n", config->dpad_up_byte);
  fprintf(file, "dpad_up_bit=%d\n", config->dpad_up_bit);

  fprintf(file, "dpad_down_byte=%d\n", config->dpad_down_byte);
  fprintf(file, "dpad_down_bit=%d\n", config->dpad_down_bit);

  fprintf(file, "dpad_left_byte=%d\n", config->dpad_left_byte);
  fprintf(file, "dpad_left_bit=%d\n", config->dpad_left_bit);

  fprintf(file, "dpad_right_byte=%d\n", config->dpad_right_byte);
  fprintf(file, "dpad_right_bit=%d\n", config->dpad_right_bit);

  fclose(file);
}

int load_config(ControllerConfig *config, const char *filename) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    return -1;
  }

  char line[256];
  while (fgets(line, sizeof(line), file)) {
    // Remove newline
    line[strcspn(line, "\n")] = 0;

    // Skip comments and empty lines
    if (line[0] == '#' || line[0] == '[' || line[0] == '\0') {
      continue;
    }

    // Parse key=value pairs
    char *key = strtok(line, "=");
    char *value = strtok(NULL, "=");

    if (!key || !value)
      continue;

    if (strcmp(key, "name") == 0) {
      strncpy(config->controller_name, value, sizeof(config->controller_name));
    } else if (strcmp(key, "a_button_byte") == 0)
      config->a_button_byte = atoi(value);
    else if (strcmp(key, "a_button_bit") == 0)
      config->a_button_bit = atoi(value);
    else if (strcmp(key, "b_button_byte") == 0)
      config->b_button_byte = atoi(value);
    else if (strcmp(key, "b_button_bit") == 0)
      config->b_button_bit = atoi(value);
    else if (strcmp(key, "x_button_byte") == 0)
      config->x_button_byte = atoi(value);
    else if (strcmp(key, "x_button_bit") == 0)
      config->x_button_bit = atoi(value);
    else if (strcmp(key, "y_button_byte") == 0)
      config->y_button_byte = atoi(value);
    else if (strcmp(key, "y_button_bit") == 0)
      config->y_button_bit = atoi(value);
    else if (strcmp(key, "lb_button_byte") == 0)
      config->lb_button_byte = atoi(value);
    else if (strcmp(key, "lb_button_bit") == 0)
      config->lb_button_bit = atoi(value);
    else if (strcmp(key, "rb_button_byte") == 0)
      config->rb_button_byte = atoi(value);
    else if (strcmp(key, "rb_button_bit") == 0)
      config->rb_button_bit = atoi(value);
    else if (strcmp(key, "back_button_byte") == 0)
      config->back_button_byte = atoi(value);
    else if (strcmp(key, "back_button_bit") == 0)
      config->back_button_bit = atoi(value);
    else if (strcmp(key, "start_button_byte") == 0)
      config->start_button_byte = atoi(value);
    else if (strcmp(key, "start_button_bit") == 0)
      config->start_button_bit = atoi(value);
    else if (strcmp(key, "l3_button_byte") == 0)
      config->l3_button_byte = atoi(value);
    else if (strcmp(key, "l3_button_bit") == 0)
      config->l3_button_bit = atoi(value);
    else if (strcmp(key, "r3_button_byte") == 0)
      config->r3_button_byte = atoi(value);
    else if (strcmp(key, "r3_button_bit") == 0)
      config->r3_button_bit = atoi(value);
    else if (strcmp(key, "xbox_button_byte") == 0)
      config->xbox_button_byte = atoi(value);
    else if (strcmp(key, "xbox_button_bit") == 0)
      config->xbox_button_bit = atoi(value);
    else if (strcmp(key, "dpad_up_byte") == 0)
      config->dpad_up_byte = atoi(value);
    else if (strcmp(key, "dpad_up_bit") == 0)
      config->dpad_up_bit = atoi(value);
    else if (strcmp(key, "dpad_down_byte") == 0)
      config->dpad_down_byte = atoi(value);
    else if (strcmp(key, "dpad_down_bit") == 0)
      config->dpad_down_bit = atoi(value);
    else if (strcmp(key, "dpad_left_byte") == 0)
      config->dpad_left_byte = atoi(value);
    else if (strcmp(key, "dpad_left_bit") == 0)
      config->dpad_left_bit = atoi(value);
    else if (strcmp(key, "dpad_right_byte") == 0)
      config->dpad_right_byte = atoi(value);
    else if (strcmp(key, "dpad_right_bit") == 0)
      config->dpad_right_bit = atoi(value);
  }

  fclose(file);
  return 0;
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

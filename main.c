#include "main.h"
#include "controller.h"
#include <ctype.h>
#include <libusb-1.0/libusb.h>
#include <signal.h>
#include <stdlib.h>

static volatile int running = 0;

void signal_handler(int sig) { running = 0; }

int check_root_permissions() {
  if (geteuid() != 0) {
    return 0;
  }

  char *sudo_uid = getenv("SUDO_UID");
  if (sudo_uid != NULL) {
    return 2;
  }

  return 1;
}

int discover_devices(libusb_context *lctx) {
  ControllerInfo *controllers = NULL;
  int count = 0;

  if (find_all_controllers(lctx, &controllers, &count) != 0) {
    fprintf(stderr, "Failed to scan for controllers\n");
    return -1;
  }

  if (count == 0) {
    printf("No controllers found");
    return 0;
  }

  printf("Found %d controllers:\n", count);
  for (int i = 0; i < count; i++) {
    printf("%d. %s\n", i + 1, controllers[i].name);

    libusb_device_handle *handle;
    if (open_controller(controllers[i].device, &handle) == 0) {
      printf("   Successfully opened\n");

      printf("Would you like to configure this controller? [y/n]");
      char choice;
      scanf("%c", &choice);
      getchar();

      if (tolower(choice) == 'y') {
        ControllerConfig config;

        printf("Starting the interactive setup for %s...\n",
               controllers[i].name);
        if (interactive_setup(handle, &config) == 0) {
          char filename[64];
          snprintf(filename, sizeof(filename), "controller_%04x_%04x.cfg",
                   controllers[i].vendor_id, controllers[i].product_id);
          save_config(&config, filename);
          printf("   Configuration saved to %s\n", filename);

          printf("   Testing configuration. Press buttons to verify (Ctrl+C to "
                 "stop)...\n");
          ControllerState state;

          running = 1;
          while (running) {
            if (read_controller_input_with_config(handle, &state, &config) >
                0) {
              printf(
                  "   A:%d B:%d X:%d Y:%d | LB:%d RB:%d | Back:%d Start:%d | ",
                  state.a_button, state.b_button, state.x_button,
                  state.y_button, state.lb_button, state.rb_button,
                  state.back_button, state.start_button);
              printf("L3:%d R3:%d Xbox:%d | D-pad: U:%d D:%d L:%d R:%d\n",
                     state.l3_button, state.r3_button, state.xbox_button,
                     state.dpad_up, state.dpad_down, state.dpad_left,
                     state.dpad_right);
            }
            usleep(10000);
          }
        }
      } else {
        printf("load...");
      }

      close_controller(handle);
    } else {
      printf("   Failed to open\n");
    }
  }

  free(controllers);

  return count;
}

int main() {
  int permission_status = check_root_permissions();
  if (permission_status == 0) {
    fprintf(stderr, "[Error]: This program requires sudo permissions to "
                    "access USB devices!\n");
    fprintf(stderr, "Please run with: sudo %s\n", "main");
    return 1;
  }

  signal(SIGINT, signal_handler);

  libusb_context *lctx = NULL;
  int acc = libusb_init_context(&lctx, NULL, 0);
  if (acc != 0) {
    fprintf(stderr, "Failed to init libusb context: %s\n",
            libusb_strerror(acc));
    return 1;
  }

  acc = libusb_set_option(lctx, LIBUSB_OPTION_LOG_LEVEL,
                          LIBUSB_LOG_LEVEL_WARNING);
  if (acc != 0) {
    fprintf(stderr, "Failed to set log level: %s\n", libusb_strerror(acc));
    libusb_exit(lctx);
    return 1;
  }

  printf("Scanning for controllers...\n");

  int found = discover_devices(lctx);

  libusb_exit(lctx);

  if (found > 0) {
    printf("\nScan completed. Found %d controllers.\n", found);
  } else {
    printf("\nNo controllers found.\n");
  }

  return 0;
}

#include "main.h"
#include "controller.h"
#include <stdlib.h>

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

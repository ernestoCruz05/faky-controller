#ifndef MAIN_H
#define MAIN_H

#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int check_root_permissions();
int discover_devices(libusb_context *lctx);

#endif /* MAIN_H */

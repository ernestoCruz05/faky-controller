#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <unistd.h>

#define SLEEP_MS(ms) usleep((ms) * 1000)
#define SLEEP_MS(ms) usleep((ms) * 1000)
#define SLEEP_US(us) usleep(us)

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define SET_BIT(value, bit) ((value) |= (1 << (bit)))
#define CLEAR_BIT(value, bit) ((value) &= ~(1 << (bit)))
#define TOGGLE_BIT(value, bit) ((value) ^= (1 << (bit)))
#define CHECK_BIT(value, bit) (((value) >> (bit)) & 1)

#endif /* UTILS_H */
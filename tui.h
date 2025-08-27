#ifndef TUI_H
#define TUI_H

#include <ncurses.h>
#include "controller.h"

#define MAX_MENU_ITEMS 20
#define MAX_CONFIG_NAME 64
#define MAX_KEY_NAME 32

typedef enum {
    TUI_MAIN_MENU,
    TUI_CONTROLLER_SELECT,
    TUI_CONFIG_SELECT,
    TUI_BUTTON_MAPPING,
    TUI_AXIS_MAPPING,
    TUI_SAVE_CONFIG,
    TUI_EXIT
} TUIState;

typedef struct {
    char name[MAX_KEY_NAME];
    int keycode;
    int scancode;
} KeyMapping;

typedef struct {
    char config_name[MAX_CONFIG_NAME];
    ControllerInfo *controller;
    ControllerConfig config;  
    int current_button;
} TUIConfigSession;

int init_tui(void);
void cleanup_tui(void);
int run_tui_config(libusb_context *lctx);

int show_main_menu(void);
int show_controller_list(libusb_context *lctx, ControllerInfo **controllers, int *count);
int show_config_menu(const char *controller_name);
int show_button_mapping_screen(TUIConfigSession *session, libusb_device_handle *handle);
int show_save_config_dialog(TUIConfigSession *session);

void draw_header(const char *title);
void draw_footer(const char *help_text);
void draw_box(int y, int x, int height, int width, const char *title);
void show_message(const char *message, int timeout_ms);
void show_error(const char *error);
int get_string_input(const char *prompt, char *buffer, int max_len);

int wait_for_key_press(char *key_name, int *keycode);
int wait_for_controller_input(libusb_device_handle *handle, ControllerState *state);

int save_tui_config(const TUIConfigSession *session);
int load_config_list(char configs[][MAX_CONFIG_NAME], int max_configs);

const char* get_button_name(int button_index);
const char* get_axis_name(int axis_index);

#endif /* TUI_H */

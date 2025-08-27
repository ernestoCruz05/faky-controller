#include "tui.h"
#include "controller.h"
#include "utils.h"
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ncurses.h>
#include <stdlib.h>

#define COLOR_TITLE 1
#define COLOR_HEADER 2
#define COLOR_FOOTER 3
#define COLOR_HIGHLIGHT 4
#define COLOR_ERROR 5
#define COLOR_SUCCESS 6

static WINDOW *main_win = NULL;
static WINDOW *content_win = NULL;
static int screen_height, screen_width;

static const char* button_names[] = {
    "A Button", "B Button", "X Button", "Y Button",
    "Left Bumper", "Right Bumper", "Back/Select", "Start/Menu",
    "Left Stick Click", "Right Stick Click", "Home/Xbox",
    "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right", "Spare"
};

static const char* axis_names[] = {
    "Left Stick X", "Left Stick Y", "Right Stick X", "Right Stick Y",
    "Left Trigger", "Right Trigger", "Spare 1", "Spare 2"
};

int init_tui(void) {
    initscr();
    
    if (!has_colors()) {
        endwin();
        fprintf(stderr, "Terminal does not support colors\n");
        return -1;
    }
    
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);  
    
    getmaxyx(stdscr, screen_height, screen_width);
    
    init_pair(COLOR_TITLE, COLOR_CYAN, COLOR_BLACK);
    init_pair(COLOR_HEADER, COLOR_WHITE, COLOR_BLUE);
    init_pair(COLOR_FOOTER, COLOR_BLACK, COLOR_WHITE);
    init_pair(COLOR_HIGHLIGHT, COLOR_BLACK, COLOR_YELLOW);
    init_pair(COLOR_ERROR, COLOR_WHITE, COLOR_RED);
    init_pair(COLOR_SUCCESS, COLOR_BLACK, COLOR_GREEN);
    
    main_win = newwin(screen_height, screen_width, 0, 0);
    content_win = newwin(screen_height - 4, screen_width - 4, 2, 2);
    
    if (!main_win || !content_win) {
        cleanup_tui();
        return -1;
    }
    
    return 0;
}

void cleanup_tui(void) {
    if (content_win) delwin(content_win);
    if (main_win) delwin(main_win);
    endwin();
}

void draw_header(const char *title) {
    werase(stdscr);
    
    attron(COLOR_PAIR(COLOR_HEADER));
    for (int i = 0; i < screen_width; i++) {
        mvaddch(0, i, ' ');
    }
    mvprintw(0, (screen_width - strlen(title)) / 2, "%s", title);
    attroff(COLOR_PAIR(COLOR_HEADER));
    
    refresh();
}

void draw_footer(const char *help_text) {
    attron(COLOR_PAIR(COLOR_FOOTER));
    for (int i = 0; i < screen_width; i++) {
        mvaddch(screen_height - 1, i, ' ');
    }
    mvprintw(screen_height - 1, 2, "%s", help_text);
    attroff(COLOR_PAIR(COLOR_FOOTER));
    
    refresh();
}

void draw_box(int y, int x, int height, int width, const char *title) {
    for (int i = 0; i < width; i++) {
        mvaddch(y, x + i, '-');
        mvaddch(y + height - 1, x + i, '-');
    }
    for (int i = 0; i < height; i++) {
        mvaddch(y + i, x, '|');
        mvaddch(y + i, x + width - 1, '|');
    }
    
    mvaddch(y, x, '+');
    mvaddch(y, x + width - 1, '+');
    mvaddch(y + height - 1, x, '+');
    mvaddch(y + height - 1, x + width - 1, '+');
    
    if (title && strlen(title) > 0) {
        attron(COLOR_PAIR(COLOR_TITLE));
        mvprintw(y, x + 2, " %s ", title);
        attroff(COLOR_PAIR(COLOR_TITLE));
    }
}

void show_message(const char *message, int timeout_ms) {
    int msg_len = strlen(message);
    int box_width = msg_len + 6;
    int box_height = 5;
    int start_y = (screen_height - box_height) / 2;
    int start_x = (screen_width - box_width) / 2;
    
    for (int i = 0; i < box_height; i++) {
        for (int j = 0; j < box_width; j++) {
            mvaddch(start_y + i, start_x + j, ' ');
        }
    }
    
    draw_box(start_y, start_x, box_height, box_width, "Message");
    mvprintw(start_y + 2, start_x + 3, "%s", message);
    
    refresh();
    
    if (timeout_ms > 0) {
        usleep(timeout_ms * 1000);
    } else {
        mvprintw(start_y + 3, start_x + 3, "Press any key...");
        refresh();
        getch();
    }
}

void show_error(const char *error) {
    int msg_len = strlen(error);
    int box_width = msg_len + 6;
    int box_height = 5;
    int start_y = (screen_height - box_height) / 2;
    int start_x = (screen_width - box_width) / 2;
    
    attron(COLOR_PAIR(COLOR_ERROR));
    for (int i = 0; i < box_height; i++) {
        for (int j = 0; j < box_width; j++) {
            mvaddch(start_y + i, start_x + j, ' ');
        }
    }
    attroff(COLOR_PAIR(COLOR_ERROR));
    
    draw_box(start_y, start_x, box_height, box_width, "ERROR");
    attron(COLOR_PAIR(COLOR_ERROR));
    mvprintw(start_y + 2, start_x + 3, "%s", error);
    mvprintw(start_y + 3, start_x + 3, "Press any key...");
    attroff(COLOR_PAIR(COLOR_ERROR));
    
    refresh();
    getch();
}

int get_string_input(const char *prompt, char *buffer, int max_len) {
    int prompt_len = strlen(prompt);
    int box_width = (prompt_len > max_len ? prompt_len : max_len) + 6;
    int box_height = 6;
    int start_y = (screen_height - box_height) / 2;
    int start_x = (screen_width - box_width) / 2;
    
    for (int i = 0; i < box_height; i++) {
        for (int j = 0; j < box_width; j++) {
            mvaddch(start_y + i, start_x + j, ' ');
        }
    }
    
    draw_box(start_y, start_x, box_height, box_width, "Input");
    mvprintw(start_y + 2, start_x + 3, "%s", prompt);
    
    mvprintw(start_y + 3, start_x + 3, ">");
    
    refresh();
    
    curs_set(1);
    echo();
    
    move(start_y + 3, start_x + 5);
    getnstr(buffer, max_len - 1);
    
    curs_set(0);
    noecho();
    
    return strlen(buffer) > 0 ? 0 : -1;
}

int show_main_menu(void) {
    char *menu_items[] = {
        "Configure New Controller",
        "Load Existing Configuration",
        "About",
        "Exit"
    };
    int n_items = 4;
    int selected = 0;
    int ch;
    
    while (1) {
        draw_header("Faky Controller - Main Menu");
        draw_footer("↑/↓: Navigate | Enter: Select | Q: Quit");
        
        werase(content_win);
        
        int start_y = (screen_height - n_items) / 2 - 2;
        int start_x = (screen_width - 40) / 2;
        
        draw_box(start_y - 1, start_x - 2, n_items + 4, 44, "Main Menu");
        
        for (int i = 0; i < n_items; i++) {
            if (i == selected) {
                attron(COLOR_PAIR(COLOR_HIGHLIGHT));
                mvprintw(start_y + i + 1, start_x, " > %-36s ", menu_items[i]);
                attroff(COLOR_PAIR(COLOR_HIGHLIGHT));
            } else {
                mvprintw(start_y + i + 1, start_x, "   %-36s ", menu_items[i]);
            }
        }
        
        refresh();
        
        ch = getch();
        switch (ch) {
            case KEY_UP:
                selected = (selected - 1 + n_items) % n_items;
                break;
            case KEY_DOWN:
                selected = (selected + 1) % n_items;
                break;
            case '\n':
            case '\r':
            case KEY_ENTER:
                return selected;
            case 'q':
            case 'Q':
                return 3; 
            case 27: 
                return 3;
        }
    }
}

const char* get_button_name(int button_index) {
    if (button_index >= 0 && button_index < 16) {
        return button_names[button_index];
    }
    return "Unknown";
}

const char* get_axis_name(int axis_index) {
    if (axis_index >= 0 && axis_index < 8) {
        return axis_names[axis_index];
    }
    return "Unknown";
}

int wait_for_key_press(char *key_name, int *keycode) {
    mvprintw(screen_height / 2 + 2, (screen_width - 40) / 2, "Press any key (ESC to cancel)...");
    refresh();
    
    int ch = getch();
    if (ch == 27) { 
        return -1;
    }
    
    *keycode = ch;
    
    switch (ch) {
        case ' ': strcpy(key_name, "Space"); break;
        case '\n': 
        case '\r': strcpy(key_name, "Enter"); break;
        case '\t': strcpy(key_name, "Tab"); break;
        case KEY_UP: strcpy(key_name, "Arrow Up"); break;
        case KEY_DOWN: strcpy(key_name, "Arrow Down"); break;
        case KEY_LEFT: strcpy(key_name, "Arrow Left"); break;
        case KEY_RIGHT: strcpy(key_name, "Arrow Right"); break;
        case KEY_BACKSPACE: strcpy(key_name, "Backspace"); break;
        case KEY_DC: strcpy(key_name, "Delete"); break;
        case KEY_HOME: strcpy(key_name, "Home"); break;
        case KEY_END: strcpy(key_name, "End"); break;
        case KEY_PPAGE: strcpy(key_name, "Page Up"); break;
        case KEY_NPAGE: strcpy(key_name, "Page Down"); break;
        default:
            if (ch >= 32 && ch <= 126) {
                snprintf(key_name, MAX_KEY_NAME, "%c", ch);
            } else if (ch >= KEY_F(1) && ch <= KEY_F(12)) {
                snprintf(key_name, MAX_KEY_NAME, "F%d", ch - KEY_F(0));
            } else {
                snprintf(key_name, MAX_KEY_NAME, "Key_%d", ch);
            }
            break;
    }
    
    return 0;
}

int wait_for_controller_input(libusb_device_handle *handle, ControllerState *state) {
    mvprintw(screen_height / 2 + 2, (screen_width - 50) / 2, "Press any button or move stick (ESC to cancel)...");
    refresh();
    
    ControllerState prev_state = {0};
    
    nodelay(stdscr, TRUE);
    
    while (1) {
        int ch = getch();
        if (ch == 27) { 
            nodelay(stdscr, FALSE);
            return -1;
        }
        
        if (read_input(handle, state) == 0) {
            if (memcmp(state, &prev_state, sizeof(ControllerState)) != 0) {
                nodelay(stdscr, FALSE);
                return 0;
            }
        }
        
        prev_state = *state;
        usleep(50000); 
    }
}

int wait_for_button_press_tui(libusb_device_handle *handle, const char *button_name,
                               uint8_t *found_byte, uint8_t *found_bit) {
    uint8_t buffer[MAX_INPUT_PACKET_SIZE];
    uint8_t last_buffer[MAX_INPUT_PACKET_SIZE] = {0};
    int attempts = 0;
    const int max_attempts = 600; 
    
    nodelay(stdscr, TRUE);
    
    clear();
    draw_header("Button Discovery");
    
    int info_y = screen_height / 2 - 3;
    mvprintw(info_y, (screen_width - 60) / 2, "Press the %s button on your controller", button_name);
    mvprintw(info_y + 1, (screen_width - 60) / 2, "(ESC to cancel)");
    
    mvprintw(info_y + 3, (screen_width - 30) / 2, "Listening for button press...");
    refresh();
    
    while (attempts < max_attempts) {
        int ch = getch();
        if (ch == 27) { 
            nodelay(stdscr, FALSE);
            return -1;
        }
        
        int actual_length = 0;
        int ret = libusb_interrupt_transfer(handle, 0x81, buffer, sizeof(buffer),
                                          &actual_length, 100); 
        
        if (ret == LIBUSB_ERROR_TIMEOUT) {
            attempts++;
            if (attempts % 10 == 0) { 
                mvprintw(info_y + 4, (screen_width - 30) / 2, "Timeout in %d seconds...", 
                        (max_attempts - attempts) / 10);
                refresh();
            }
            continue;
        }
        
        if (ret < 0 || actual_length < 20) {
            attempts++;
            continue;
        }
        
        for (int byte = 2; byte < 6; byte++) {
            for (int bit = 0; bit < 8; bit++) {
                uint8_t mask = (1 << bit);
                if ((buffer[byte] & mask) && !(last_buffer[byte] & mask)) {
                    *found_byte = byte;
                    *found_bit = bit;
                    
                    clear();
                    draw_header("Button Discovered!");
                    
                    mvprintw(screen_height / 2 - 1, (screen_width - 50) / 2, 
                            "✓ Detected %s button:", button_name);
                    mvprintw(screen_height / 2, (screen_width - 50) / 2, 
                            "  Byte: %d, Bit: %d", *found_byte, *found_bit);
                    mvprintw(screen_height / 2 + 2, (screen_width - 50) / 2, 
                            "Please release the button...");
                    refresh();
                    
                    int release_attempts = 0;
                    while (release_attempts < 100) { 
                        int release_ret = libusb_interrupt_transfer(handle, 0x81, buffer,
                                                sizeof(buffer), &actual_length, 100);
                        
                        if (release_ret == 0 && actual_length >= 20) {
                            if (!(buffer[byte] & mask)) {
                                mvprintw(screen_height / 2 + 3, (screen_width - 30) / 2, 
                                        "✓ Button released");
                                refresh();
                                usleep(1000000); 
                                nodelay(stdscr, FALSE);
                                return 0;
                            }
                        }
                        release_attempts++;
                        usleep(100000);
                    }
                    
                    show_error("Timeout waiting for button release");
                    nodelay(stdscr, FALSE);
                    return -1;
                }
            }
        }
        
        memcpy(last_buffer, buffer, sizeof(last_buffer));
        attempts++;
        usleep(10000); 
    }
    
    show_error("Timeout waiting for button press");
    nodelay(stdscr, FALSE);
    return -1;
}

int run_tui_config(libusb_context *lctx) {
    while (1) {
        int choice = show_main_menu();
        
        switch (choice) {
            case 0: { 
                ControllerInfo *controllers = NULL;
                int count = 0;
                
                if (find_all_controllers(lctx, &controllers, &count) != 0) {
                    show_error("Failed to scan for controllers");
                    continue;
                }
                
                if (count == 0) {
                    show_error("No controllers found. Please connect a controller.");
                    continue;
                }
                
                int selected_controller = show_controller_list(lctx, &controllers, &count);
                if (selected_controller >= 0) {
                    libusb_device_handle *handle;
                    if (open_controller(controllers[selected_controller].device, &handle) == 0) {
                        if (claim_interface_safe(handle) != 0) {
                            show_error("Failed to setup interface for configuration");
                            close_controller(handle);
                        } else {
                            TUIConfigSession session = {0};
                            session.controller = &controllers[selected_controller];
                            strcpy(session.config_name, controllers[selected_controller].name);
                            
                            if (show_button_mapping_screen(&session, handle) == 0) {
                                show_save_config_dialog(&session);
                            }
                            
                            release_interface_safe(handle);
                            close_controller(handle);
                        }
                    } else {
                        show_error("Failed to open controller");
                    }
                }
                
                free_controllers(controllers, count);
                break;
            }
            case 1: { 
                char configs[20][MAX_CONFIG_NAME];
                int config_count = load_config_list(configs, 20);
                
                if (config_count == 0) {
                    show_error("No saved configurations found");
                    continue;
                }
                
                int selected_config = show_config_menu("Select Configuration");
                if (selected_config >= 0) {
                    show_message("Configuration loading not implemented yet", 2000);
                }
                break;
            }
            case 2: { 
                draw_header("About Faky Controller");
                draw_footer("Press any key to return");
                
                int box_height = 10;
                int box_width = 60;
                int start_y = (screen_height - box_height) / 2;
                int start_x = (screen_width - box_width) / 2;
                
                draw_box(start_y, start_x, box_height, box_width, "About");
                
                mvprintw(start_y + 2, start_x + 2, "Faky Controller v1.0");
                mvprintw(start_y + 3, start_x + 2, "A controller to keyboard/mouse translator");
                mvprintw(start_y + 4, start_x + 2, "Written in C using libusb");
                mvprintw(start_y + 6, start_x + 2, "Author: Ernesto Cruz");
                mvprintw(start_y + 7, start_x + 2, "License: MIT");
                
                refresh();
                getch();
                break;
            }
            case 3: 
            default:
                return 0;
        }
    }
}

int show_controller_list(libusb_context *lctx __attribute__((unused)), ControllerInfo **controllers, int *count) {
    if (*count == 0) {
        return -1;
    }
    
    int selected = 0;
    int ch;
    
    while (1) {
        draw_header("Select Controller");
        draw_footer("↑/↓: Navigate | Enter: Select | ESC: Back");
        
        int start_y = (screen_height - *count) / 2 - 2;
        int start_x = (screen_width - 60) / 2;
        
        draw_box(start_y - 1, start_x - 2, *count + 4, 64, "Available Controllers");
        
        for (int i = 0; i < *count; i++) {
            char info_line[80];  
            snprintf(info_line, sizeof(info_line), "%-25s [%04x:%04x]", 
                    (*controllers)[i].name, 
                    (*controllers)[i].vendor_id, 
                    (*controllers)[i].product_id);
            
            if (i == selected) {
                attron(COLOR_PAIR(COLOR_HIGHLIGHT));
                mvprintw(start_y + i + 1, start_x, " > %-56s ", info_line);
                attroff(COLOR_PAIR(COLOR_HIGHLIGHT));
            } else {
                mvprintw(start_y + i + 1, start_x, "   %-56s ", info_line);
            }
        }
        
        refresh();
        
        ch = getch();
        switch (ch) {
            case KEY_UP:
                selected = (selected - 1 + *count) % *count;
                break;
            case KEY_DOWN:
                selected = (selected + 1) % *count;
                break;
            case '\n':
            case '\r':
            case KEY_ENTER:
                return selected;
            case 27: 
                return -1;
        }
    }
}

int show_config_menu(const char *controller_name __attribute__((unused))) {
    char configs[20][MAX_CONFIG_NAME];
    int config_count = load_config_list(configs, 20);
    
    if (config_count == 0) {
        show_error("No configurations found");
        return -1;
    }
    
    int selected = 0;
    int ch;
    
    while (1) {
        draw_header("Select Configuration");
        draw_footer("↑/↓: Navigate | Enter: Select | ESC: Back");
        
        int start_y = (screen_height - config_count) / 2 - 2;
        int start_x = (screen_width - 50) / 2;
        
        draw_box(start_y - 1, start_x - 2, config_count + 4, 54, "Saved Configurations");
        
        for (int i = 0; i < config_count; i++) {
            if (i == selected) {
                attron(COLOR_PAIR(COLOR_HIGHLIGHT));
                mvprintw(start_y + i + 1, start_x, " > %-46s ", configs[i]);
                attroff(COLOR_PAIR(COLOR_HIGHLIGHT));
            } else {
                mvprintw(start_y + i + 1, start_x, "   %-46s ", configs[i]);
            }
        }
        
        refresh();
        
        ch = getch();
        switch (ch) {
            case KEY_UP:
                selected = (selected - 1 + config_count) % config_count;
                break;
            case KEY_DOWN:
                selected = (selected + 1) % config_count;
                break;
            case '\n':
            case '\r':
            case KEY_ENTER:
                return selected;
            case 27: 
                return -1;
        }
    }
}

int show_button_mapping_screen(TUIConfigSession *session, libusb_device_handle *handle) {
    int current_button = 0;
    const char* button_names[] = {
        "A", "B", "X", "Y", 
        "Left Bumper (LB)", "Right Bumper (RB)", 
        "Back", "Start",
        "Left Stick Click (L3)", "Right Stick Click (R3)",
        "Xbox Guide", 
        "D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right"
    };
    int total_buttons = 15;
    int ch;
    
    while (current_button < total_buttons) {
        draw_header("Controller Button Discovery");
        draw_footer("Enter: Map | S: Skip | ESC: Cancel | N: Finish");
        
        
        clear();
        
        
        int info_y = 2;
        attron(COLOR_PAIR(COLOR_TITLE));
        mvprintw(info_y, 2, "Controller: %s", session->controller->name);
        mvprintw(info_y + 1, 2, "Configuration: %s", session->config_name);
        attroff(COLOR_PAIR(COLOR_TITLE));
        
        
        int button_y = 5;
        draw_box(button_y - 1, 2, 8, screen_width - 4, "Current Button");
        
        attron(COLOR_PAIR(COLOR_HIGHLIGHT));
        mvprintw(button_y + 1, 4, "Discovering: %s", button_names[current_button]);
        attroff(COLOR_PAIR(COLOR_HIGHLIGHT));
        
        
        uint8_t *byte_ptr = NULL, *bit_ptr = NULL;
        switch (current_button) {
            case 0: byte_ptr = &session->config.a_button_byte; bit_ptr = &session->config.a_button_bit; break;
            case 1: byte_ptr = &session->config.b_button_byte; bit_ptr = &session->config.b_button_bit; break;
            case 2: byte_ptr = &session->config.x_button_byte; bit_ptr = &session->config.x_button_bit; break;
            case 3: byte_ptr = &session->config.y_button_byte; bit_ptr = &session->config.y_button_bit; break;
            case 4: byte_ptr = &session->config.lb_button_byte; bit_ptr = &session->config.lb_button_bit; break;
            case 5: byte_ptr = &session->config.rb_button_byte; bit_ptr = &session->config.rb_button_bit; break;
            case 6: byte_ptr = &session->config.back_button_byte; bit_ptr = &session->config.back_button_bit; break;
            case 7: byte_ptr = &session->config.start_button_byte; bit_ptr = &session->config.start_button_bit; break;
            case 8: byte_ptr = &session->config.l3_button_byte; bit_ptr = &session->config.l3_button_bit; break;
            case 9: byte_ptr = &session->config.r3_button_byte; bit_ptr = &session->config.r3_button_bit; break;
            case 10: byte_ptr = &session->config.xbox_button_byte; bit_ptr = &session->config.xbox_button_bit; break;
            case 11: byte_ptr = &session->config.dpad_up_byte; bit_ptr = &session->config.dpad_up_bit; break;
            case 12: byte_ptr = &session->config.dpad_down_byte; bit_ptr = &session->config.dpad_down_bit; break;
            case 13: byte_ptr = &session->config.dpad_left_byte; bit_ptr = &session->config.dpad_left_bit; break;
            case 14: byte_ptr = &session->config.dpad_right_byte; bit_ptr = &session->config.dpad_right_bit; break;
        }
        
        if (byte_ptr && *byte_ptr != 0) {
            mvprintw(button_y + 3, 4, "Already mapped: Byte %d, Bit %d", *byte_ptr, *bit_ptr);
        } else {
            mvprintw(button_y + 3, 4, "Not discovered yet");
        }
        
        mvprintw(button_y + 5, 4, "Progress: %d of %d buttons", current_button + 1, total_buttons);
        
        
        int status_y = 14;
        draw_box(status_y - 1, 2, 12, screen_width - 4, "Discovery Status");
        
        for (int i = 0; i < total_buttons; i++) {
            char status_line[80];
            uint8_t *check_byte = NULL, *check_bit = NULL;
            
            switch (i) {
                case 0: check_byte = &session->config.a_button_byte; check_bit = &session->config.a_button_bit; break;
                case 1: check_byte = &session->config.b_button_byte; check_bit = &session->config.b_button_bit; break;
                case 2: check_byte = &session->config.x_button_byte; check_bit = &session->config.x_button_bit; break;
                case 3: check_byte = &session->config.y_button_byte; check_bit = &session->config.y_button_bit; break;
                case 4: check_byte = &session->config.lb_button_byte; check_bit = &session->config.lb_button_bit; break;
                case 5: check_byte = &session->config.rb_button_byte; check_bit = &session->config.rb_button_bit; break;
                case 6: check_byte = &session->config.back_button_byte; check_bit = &session->config.back_button_bit; break;
                case 7: check_byte = &session->config.start_button_byte; check_bit = &session->config.start_button_bit; break;
                case 8: check_byte = &session->config.l3_button_byte; check_bit = &session->config.l3_button_bit; break;
                case 9: check_byte = &session->config.r3_button_byte; check_bit = &session->config.r3_button_bit; break;
                case 10: check_byte = &session->config.xbox_button_byte; check_bit = &session->config.xbox_button_bit; break;
                case 11: check_byte = &session->config.dpad_up_byte; check_bit = &session->config.dpad_up_bit; break;
                case 12: check_byte = &session->config.dpad_down_byte; check_bit = &session->config.dpad_down_bit; break;
                case 13: check_byte = &session->config.dpad_left_byte; check_bit = &session->config.dpad_left_bit; break;
                case 14: check_byte = &session->config.dpad_right_byte; check_bit = &session->config.dpad_right_bit; break;
            }
            
            if (check_byte && *check_byte != 0) {
                snprintf(status_line, sizeof(status_line), "✓ %-18s [%d:%d]", 
                        button_names[i], *check_byte, *check_bit);
                attron(COLOR_PAIR(COLOR_SUCCESS));
            } else {
                snprintf(status_line, sizeof(status_line), "  %-18s (pending)", 
                        button_names[i]);
            }
            
            if (i == current_button) {
                attron(A_BOLD);
            }
            
            mvprintw(status_y + 1 + (i % 10), 4 + (i / 10) * 38, "%s", status_line);
            
            attroff(A_BOLD);
            attroff(COLOR_PAIR(COLOR_SUCCESS));
        }
        
        refresh();
        
        ch = getch();
        switch (ch) {
            case '\n':
            case '\r':
            case KEY_ENTER: {
                
                if (byte_ptr && bit_ptr) {
                    if (wait_for_button_press_tui(handle, button_names[current_button], 
                                                byte_ptr, bit_ptr) == 0) {
                        show_message("Button discovered successfully!", 1000);
                        current_button++;
                    } else {
                        show_message("Discovery cancelled or failed", 1000);
                    }
                }
                break;
            }
            case 's':
            case 'S':
                
                current_button++;
                break;
            case 27: 
                return -1;
            case 'n':
            case 'N':
                
                return 0;
        }
    }
    
    show_message("Button discovery completed!", 2000);
    return 0;
}

int show_save_config_dialog(TUIConfigSession *session) {
    char config_name[MAX_CONFIG_NAME];
    strcpy(config_name, session->config_name);
    
    if (get_string_input("Enter configuration name:", config_name, MAX_CONFIG_NAME) == 0) {
        strcpy(session->config_name, config_name);
        
        if (save_tui_config(session) == 0) {
            show_message("Configuration saved successfully!", 2000);
            return 0;
        } else {
            show_error("Failed to save configuration");
            return -1;
        }
    }
    
    return -1;
}

int save_tui_config(const TUIConfigSession *session) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%s.cfg", session->config_name);
    
    FILE *file = fopen(filename, "w");
    if (!file) {
        return -1;
    }
    
    fprintf(file, "[ControllerConfig]\n");
    fprintf(file, "name=%s\n", session->config_name);
    
    if (session->config.a_button_byte != 0) {
        fprintf(file, "a_button_byte=%d\n", session->config.a_button_byte);
        fprintf(file, "a_button_bit=%d\n", session->config.a_button_bit);
    }
    if (session->config.b_button_byte != 0) {
        fprintf(file, "b_button_byte=%d\n", session->config.b_button_byte);
        fprintf(file, "b_button_bit=%d\n", session->config.b_button_bit);
    }
    if (session->config.x_button_byte != 0) {
        fprintf(file, "x_button_byte=%d\n", session->config.x_button_byte);
        fprintf(file, "x_button_bit=%d\n", session->config.x_button_bit);
    }
    if (session->config.y_button_byte != 0) {
        fprintf(file, "y_button_byte=%d\n", session->config.y_button_byte);
        fprintf(file, "y_button_bit=%d\n", session->config.y_button_bit);
    }
    if (session->config.lb_button_byte != 0) {
        fprintf(file, "lb_button_byte=%d\n", session->config.lb_button_byte);
        fprintf(file, "lb_button_bit=%d\n", session->config.lb_button_bit);
    }
    if (session->config.rb_button_byte != 0) {
        fprintf(file, "rb_button_byte=%d\n", session->config.rb_button_byte);
        fprintf(file, "rb_button_bit=%d\n", session->config.rb_button_bit);
    }
    if (session->config.back_button_byte != 0) {
        fprintf(file, "back_button_byte=%d\n", session->config.back_button_byte);
        fprintf(file, "back_button_bit=%d\n", session->config.back_button_bit);
    }
    if (session->config.start_button_byte != 0) {
        fprintf(file, "start_button_byte=%d\n", session->config.start_button_byte);
        fprintf(file, "start_button_bit=%d\n", session->config.start_button_bit);
    }
    if (session->config.l3_button_byte != 0) {
        fprintf(file, "l3_button_byte=%d\n", session->config.l3_button_byte);
        fprintf(file, "l3_button_bit=%d\n", session->config.l3_button_bit);
    }
    if (session->config.r3_button_byte != 0) {
        fprintf(file, "r3_button_byte=%d\n", session->config.r3_button_byte);
        fprintf(file, "r3_button_bit=%d\n", session->config.r3_button_bit);
    }
    if (session->config.xbox_button_byte != 0) {
        fprintf(file, "xbox_button_byte=%d\n", session->config.xbox_button_byte);
        fprintf(file, "xbox_button_bit=%d\n", session->config.xbox_button_bit);
    }
    if (session->config.dpad_up_byte != 0) {
        fprintf(file, "dpad_up_byte=%d\n", session->config.dpad_up_byte);
        fprintf(file, "dpad_up_bit=%d\n", session->config.dpad_up_bit);
    }
    if (session->config.dpad_down_byte != 0) {
        fprintf(file, "dpad_down_byte=%d\n", session->config.dpad_down_byte);
        fprintf(file, "dpad_down_bit=%d\n", session->config.dpad_down_bit);
    }
    if (session->config.dpad_left_byte != 0) {
        fprintf(file, "dpad_left_byte=%d\n", session->config.dpad_left_byte);
        fprintf(file, "dpad_left_bit=%d\n", session->config.dpad_left_bit);
    }
    if (session->config.dpad_right_byte != 0) {
        fprintf(file, "dpad_right_byte=%d\n", session->config.dpad_right_byte);
        fprintf(file, "dpad_right_bit=%d\n", session->config.dpad_right_bit);
    }
    fclose(file);
    return 0;
}

int load_config_list(char configs[][MAX_CONFIG_NAME], int max_configs) {
    DIR *dir = opendir(".");
    if (!dir) return 0;
    
    struct dirent *entry;
    int count = 0;
    
    while ((entry = readdir(dir)) != NULL && count < max_configs) {
        char *ext = strrchr(entry->d_name, '.');
        if (ext && strcmp(ext, ".cfg") == 0) {
            *ext = '\0';
            strncpy(configs[count], entry->d_name, MAX_CONFIG_NAME - 1);
            configs[count][MAX_CONFIG_NAME - 1] = '\0';
            count++;
        }
    }
    
    closedir(dir);
    return count;
}

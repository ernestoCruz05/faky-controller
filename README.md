# Faky Controller

A controller → mouse/keyboard input translator written in C using [libusb](https://libusb.info/).  
This tool aims to let standard game controllers emulate keyboard and mouse input, useful for apps/games without native controller support.

---

##  Features
- USB controller detection via libusb
- (Planned) Mapping of controller inputs to keyboard/mouse events
- Optional udev rules for non-root access
- Modular code layout for adding new controllers and mappings

---

##  Prerequisites

- GCC (or Clang) compiler
- `libusb-1.0` **development** libraries
- `make`
- `sudo` privileges (for udev rule installation)

Install libusb dev package:

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y build-essential libusb-1.0-0-dev

# Fedora/RHEL
sudo dnf install -y libusb1-devel

# Arch Linux
sudo pacman -S --needed libusb
```

---

##  Build from Source

```bash
git clone https://github.com/ernestoCruz05/faky-controller.git
cd faky-controller
make
```

Artifacts:

- `./main` – program binary

---

##  (Optional) Install Udev Rules

Installing udev rules allows running without sudo.

```bash
sudo make install-udev
# Reload rules (usually done by the Makefile as well)
sudo udevadm control --reload-rules
sudo udevadm trigger
```

To remove later:

```bash
sudo make uninstall-udev
sudo udevadm control --reload-rules
sudo udevadm trigger
```

> **Note**: The rule provided is generic and grants read/write to USB HID-like devices used by this app. Adjust as needed for your exact devices/vendor IDs. Or simply run the app as sudo

---

## ▶ Usage

Run with sudo (if udev rules not installed):

```bash
sudo ./main
```

Run without sudo (after installing udev rules):

```bash
./main
```

Build and run in one command:

```bash
make run
```

Clean build files:

```bash
make clean
```

---

##  Project Structure

```
faky-controller/
├── main.c                  # Program entry point (initialization & main loop)
├── main.h                  # Global includes/types for main
├── controller.c            # Controller detection and enumeration (libusb)
├── controller.h            # Controller types, constants, prototypes
├── input.c                 # (Planned) OS input event emission (kbd/mouse)
├── input.h                 # Input handling prototypes
├── translator.c            # (Planned) Mapping logic (controller → input events)
├── translator.h            # Translator configs & APIs
├── Makefile                # Build system with run/install-udev targets
├── 99-faky-controller.rules# Udev rules for non-root access
└── README.md               # This file
```

---

##  Building (details)

Using the provided Makefile (recommended):

```bash
make          # builds ./main
make run      # builds and runs with sudo if needed
make clean    # removes build artifacts
```

Manual compilation example:

```bash
gcc -c main.c -o main.o
gcc -c controller.c -o controller.o
gcc -c input.c -o input.o
gcc -c translator.c -o translator.o
gcc -o main main.o controller.o input.o translator.o -lusb-1.0
```

> If the linker can't find libusb, ensure `libusb-1.0-0-dev` (or distro equivalent) is installed and that your compiler can find it (you may need `pkg-config --cflags --libs libusb-1.0`).

---

##  Configuration

### Adding New Controllers

1. Add vendor/product IDs to `controller.h`:

```c
#define VENDOR_SOME_VENDOR   0xXXXX
#define PRODUCT_SOME_PAD     0xYYYY
```

2. Update controller detection logic in `controller.c` to recognize the new VID/PID and assign a controller type/profile.

3. Extend mappings in `translator.c` to define how buttons/axes translate to keyboard/mouse events.

### Udev Rule (example)

`99-faky-controller.rules` (installed to `/etc/udev/rules.d/`):

```bash
# Allow users in the "plugdev" group to access supported devices without sudo
SUBSYSTEM=="usb", MODE="0660", GROUP="plugdev", ENV{ID_VENDOR_ID}=="*", ENV{ID_MODEL_ID}=="*"
```

You can narrow by specific controllers using hex IDs:

```bash
SUBSYSTEM=="usb", ATTR{idVendor}=="045e", ATTR{idProduct}=="028e", MODE="0660", GROUP="plugdev"
```

After editing rules:

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

---

##  Development Tips

- **Reading devices**: enumerate via libusb and match by VID/PID.

- **Input emission (Linux)**: you'll typically use uinput (virtual input device). This requires appropriate permissions.

- Keep `main.c` minimal—initialize, run loop, and delegate to modules (controller, translator, input).

---

##  Troubleshooting

**Permission errors** (e.g., cannot open device):
- Install udev rules and replug the controller
- Or run with sudo

**Controller not detected**:
- Check cabling and `dmesg | tail`
- Verify VID/PID using `lsusb`
- Ensure the device isn't exclusively claimed by another driver

**Build issues**:
- Confirm libusb dev package installed
- Check linker flags: `pkg-config --cflags --libs libusb-1.0`

**No input events produced**:
- Ensure uinput is available (`lsmod | grep uinput`, or `/dev/uinput` exists)
- Verify your mapping logic and that the event device is created

---

##  Roadmap

- [ ] Input mapping configuration system (profiles)
- [ ] Keyboard/mouse event translation via uinput
- [ ] CLI/interactive config for remapping
- [ ] Profiles per game/app
- [ ] Support for additional controller types
- [ ] (Stretch) Cross-platform abstractions

---

##  Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/awesome`
3. Commit: `git commit -m "Add awesome feature"`
4. Push: `git push origin feature/awesome`
5. Open a Pull Request

Please keep changes modular and include a brief description of design decisions.

---

##  License

MIT License — see [LICENSE](LICENSE) for details.

# MacWiiSupport
Wii and Wii U support kernel extension for Mac OS X as part of [WiiIntosh](https://github.com/WiiIntosh/WiiIntosh).

Currently tested/built for 10.2-10.4, but will eventually support all versions capable of running on the PowerPC G3 (10.0 - 10.4).

### Supported Wii hardware
- Primary interrupt controller
- Hollywood interrupt controller
- XFB framebuffer (temporary)
- SD host controller (front SD card slot)
  - No hotplug
  - Only supports SDHC cards
- OHCI USB 1.1 host controller
  - Unstable
  - Only rear ports are currently supported
  - Bulk/isochronous endpoints unsupported (mass storage and audio/streaming devices)

### Supported Wii U hardware
- Primary interrupt controller
- Latte interrupt controller
- 32-bit framebuffer on TV
- SD host controller (front SD card slot)\
  - No hotplug
  - Only supports SDHC cards
- OHCI USB 1.1 host controller
  - Unstable
  - Only rear ports are currently supported
  - Bulk/isochronous endpoints unsupported (mass storage and audio/streaming devices)

### Unsupported Wii hardware
- EHCI USB 2.0 controller (rear ports)
- DVD drive
- Audio
- Power/reset switches
- Console reboot/shutdown
- Bluetooth (should be possible with full OHCI on versions that have a Bluetooth stack)
- EXI bus
  - GameCube memory slots
  - Real time clock
- Serial bus
  - GameCube controller ports
- WiFi via SDIO controller

### Unsupported Wii U hardware
- EHCI USB 2.0 controller (external ports)
- DVD drive
- Audio
- Power switch
- Console reboot/shutdown
- Bluetooth (should be possible with full OHCI on versions that have a Bluetooth stack)
- EXI bus
  - Real time clock
- WiFi via SDIO controller

### Boot arguments
See the [module list](Docs/modules.md) for boot arguments for each module.

### Credits
- [Apple](https://www.apple.com) for Mac OS X
- [Goldfish64](https://github.com/Goldfish64) for this software
- [Wiibrew.org](https://wiibrew.org) for various documents/info

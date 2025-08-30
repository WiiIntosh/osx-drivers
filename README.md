![WiiIntosh logo](https://github.com/WiiIntosh/.github/blob/main/profile/logo.png?raw=true)
---

[![Build Status](https://github.com/WiiIntosh/osx-drivers/actions/workflows/main.yml/badge.svg?branch=master)](https://github.com/WiiIntosh/osx-drivers/actions)

Wii and Wii U support kernel extension for Mac OS X as part of [WiiIntosh](https://github.com/WiiIntosh).

Currently tested/built for 10.2-10.4, but will eventually support all versions capable of running on the PowerPC G3 (10.0 - 10.4).

### Wii hardware support status
| Hardware                                            | Supported                                   |
|-----------------------------------------------------|---------------------------------------------|
| Broadway primary interrupt controller               | Yes                                         |
| Hollywood secondary interrupt controller            | Yes                                         |
| USB 1.1 (OHCI) controller (rear ports)              | No isochronous transfers (audio/streaming)  |
| USB 1.1 (OHCI) controller (internal Bluetooth)      | No isochronous transfers, BT does not load  |
| USB 2.0 (EHCI) controller (rear ports)              | No                                          |
| SD host controller (front SD slot)                  | Yes, SDHC only                              |
| WiFi via SDIO                                       | No                                          |
| Audio interface (rear A/V)                          | Yes, not all media may work                 |
| Flipper video interface                             | 32-bit framebuffer via Starlet -> XFB       |
| External interface                                  | RTC (partially), slots are nonfunctional    |
| Serial interface (GameCube controllers)             | No                                          |
| DVD drive                                           | No                                          |
| Power/reset switches                                | No                                          |
| Shutdown/reboot functionality                       | Yes                                         |

### Wii U hardware support status
| Hardware                                            | Supported                                   |
|-----------------------------------------------------|---------------------------------------------|
| Espresso primary interrupt controller               | Yes                                         |
| Latte secondary interrupt controller                | Yes                                         |
| USB 1.1 (OHCI) controller (rear ports)              | No isochronous transfers (audio/streaming)  |
| USB 1.1 (OHCI) controller (internal Bluetooth)      | No isochronous transfers, BT does not load  |
| USB 1.1 (OHCI) controller (front ports)             | No                                          |
| USB 2.0 (EHCI) controller (rear ports)              | No                                          |
| USB 2.0 (EHCI) controller (front ports)             | No                                          |
| USB 2.0 (EHCI) controller (GamePad)                 | No                                          |
| SD host controller (front SD slot)                  | Yes, SDHC only                              |
| WiFi via SDIO                                       | No                                          |
| Audio interface (rear A/V)                          | Yes, not all media may work                 |
| Audio interface (GamePad)                           | Yes, not all media may work                 |
| GX2 video interface                                 | 32-bit framebuffer, hardware cursor         |
| External interface                                  | RTC (partially)                             |
| DVD drive                                           | No                                          |
| Power/reset switches                                | No                                          |
| Shutdown/reboot functionality                       | Yes                                         |

### Credits
- [Apple](https://www.apple.com) for Mac OS X
- [Goldfish64](https://github.com/Goldfish64) for this software
- [Wiibrew.org](https://wiibrew.org) / [Wiiubrew.org](https://wiiubrew.org) for various documents/info
- [Yet Another Gamecube Documenation](https://www.gc-forever.com/yagcd/)

![Wiintosh logo](https://github.com/Wiintosh/.github/blob/main/profile/logo.png?raw=true)
---

[![Build Status](https://github.com/Wiintosh/osx-drivers/actions/workflows/main.yml/badge.svg?branch=main)](https://github.com/Wiintosh/osx-drivers/actions)

Wii and Wii U support kernel extensions for Mac OS X as part of [Wiintosh](https://github.com/Wiintosh).

Built for all PowerPC G3 supported versions of Mac OS X (10.0 - 10.41).

### Extensions
* WiiAudio: audio support
* WiiEXI: EXI bus and RTC
* WiiGraphics: Flipper and GX2 graphics support
* WiiPlatform: Platform expert and IPC support
* WiiStorage: SDHC support
* WiiUSB: OHCI support

WiiAudio and WiiGraphics depend on kexts that normally are not part of an installed system's cache (IOAudioFamily and IOGraphicsFamily). These will need to have their `OSBundleRequired` properties changed so that they do.

### Credits
- [Apple](https://www.apple.com) for Mac OS X
- [Goldfish64](https://github.com/Goldfish64) for this software
- [Wiibrew.org](https://wiibrew.org) / [Wiiubrew.org](https://wiiubrew.org) for various documents/info
- [Yet Another Gamecube Documenation](https://www.gc-forever.com/yagcd/)

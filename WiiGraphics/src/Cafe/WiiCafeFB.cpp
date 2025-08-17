//
//  WiiCafeFB.hpp
//  Wii U Cafe graphics framebuffer
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include "WiiCafeFB.hpp"
#include "GX2Regs.hpp"

OSDefineMetaClassAndStructors(WiiCafeFB, super);

#define kCursorPosOffset   4

enum {
  kWiiCafeFBDepth32bpp = 0,
  kWiiCafeFBDepth16bpp,
  kWiiCafeFBDepth8bpp,
  kWiiCafeFBDepthMax
};

//
// Overrides IOFramebuffer::init().
//
bool WiiCafeFB::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _debugEnabled = true;

  _memoryMap    = NULL;
  _baseAddr     = NULL;

  _currentDisplayModeId = 1;
  _currentDepth         = kWiiCafeFBDepth32bpp;
  _gammaValid           = false;
  _clutValid            = false;

  _cursorBuffer = NULL;
  _cursorHwDesc = NULL;

  return super::init(dictionary);
}

//
// Overrides IOFramebuffer::start().
//
bool WiiCafeFB::start(IOService *provider) {
  WIIDBGLOG("Initializing Cafe framebuffer");

  //
  // Map interrupt controller memory.
  //
  _memoryMap = provider->mapDeviceMemoryWithIndex(0);
  if (_memoryMap == NULL) {
    WIISYSLOG("Failed to map graphics controller memory");
    return false;
  }
  _baseAddr = (volatile void *)_memoryMap->getVirtualAddress();
  WIIDBGLOG("Mapped registers to %p (physical 0x%X), length: 0x%X", _baseAddr,
    _memoryMap->getPhysicalAddress(), _memoryMap->getLength());

  //
  // Get the framebuffer memory.
  //
  _fbMemory = provider->getDeviceMemoryWithIndex(1);
  if (_memoryMap == NULL) {
    WIISYSLOG("Failed to get framebuffer memory");
    return false;
  }

  if (!super::start(provider)) {
    WIIDBGLOG("super::start() returned false");
    return false;
  }

  WIIDBGLOG("Initialized Cafe framebuffer");
  return true;
}

//
// Overrides IOFramebuffer::enableController().
//
IOReturn WiiCafeFB::enableController(void) {
  return super::enableController();
}

//
// Overrides IOFramebuffer::IOFramebuffer().
//
// Gets the framebuffer memory.
//
IODeviceMemory *WiiCafeFB::getApertureRange(IOPixelAperture aperture) {
  if (aperture != kIOFBSystemAperture) {
    return NULL;
  }

  _fbMemory->retain();
  return _fbMemory;
}

//
// Overrides IOFramebuffer::getPixelFormats().
//
// Gets the supported pixel formats.
//
const char *WiiCafeFB::getPixelFormats(void) {
  static const char *pixelFormats =
    IO32BitDirectPixels "\0"
    IO16BitDirectPixels "\0"
    IO8BitIndexedPixels "\0"
    "\0";
  return pixelFormats;
}

//
// Overrides IOFramebuffer::getDisplayModeCount().
//
// Gets the number of supported display modes.
//
IOItemCount WiiCafeFB::getDisplayModeCount(void) {
  WIIDBGLOG("getDisplayModeCount");
  return 1;
}

//
// Overrides IOFramebuffer::getDisplayModes().
//
// Gets the supported display modes.
//
IOReturn WiiCafeFB::getDisplayModes(IODisplayModeID *allDisplayModes) {
  WIIDBGLOG("getDisplayModes");
  *allDisplayModes = 1;
  return kIOReturnSuccess;
}

//
// Overrides IOFramebuffer::getInformationForDisplayMode().
//
// Gets detailed information for the specified display mode.
//
IOReturn WiiCafeFB::getInformationForDisplayMode(IODisplayModeID displayMode, IODisplayModeInformation *info) {
  if ((displayMode == 0) || (displayMode > 1)) {
    return kIOReturnBadArgument;
  }

  bzero(info, sizeof (*info));
  info->nominalWidth  = 1280;
  info->nominalHeight = 720;
  info->refreshRate   = 60 << 16;
  info->maxDepthIndex = kWiiCafeFBDepthMax - 1;

  return kIOReturnSuccess;
}

//
// Overrides IOFramebuffer::getInformationForDisplayMode().
//
// Obsolete method.
//
UInt64 WiiCafeFB::getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth) {
  return 0;
}

//
// Overrides IOFramebuffer::getPixelInformation().
//
// Gets pixel information for the specified display mode.
//
IOReturn WiiCafeFB::getPixelInformation(IODisplayModeID displayMode, IOIndex depth, IOPixelAperture aperture, IOPixelInformation *pixelInfo) {
  if (aperture != kIOFBSystemAperture) {
    return kIOReturnUnsupportedMode;
  }

  if ((displayMode == 0) || (displayMode > 1) || (depth >= kWiiCafeFBDepthMax)) {
    return kIOReturnBadArgument;
  }

  bzero(pixelInfo, sizeof (*pixelInfo));
  pixelInfo->activeWidth  = 1280;
  pixelInfo->activeHeight = 720;

  if (depth == kWiiCafeFBDepth32bpp) {
    pixelInfo->pixelType         = kIORGBDirectPixels;
    pixelInfo->bytesPerRow       = pixelInfo->activeWidth * 4;
    pixelInfo->bitsPerPixel      = 32;
    pixelInfo->bitsPerComponent  = 8;
    pixelInfo->componentCount    = 3;
    pixelInfo->componentMasks[0] = 0xFF0000;
    pixelInfo->componentMasks[1] = 0x00FF00;
    pixelInfo->componentMasks[2] = 0x0000FF;

    strncpy(pixelInfo->pixelFormat, IO32BitDirectPixels, sizeof (pixelInfo->pixelFormat));
  } else if (depth == kWiiCafeFBDepth16bpp) {
    pixelInfo->pixelType         = kIORGBDirectPixels;
    pixelInfo->bytesPerRow       = pixelInfo->activeWidth * 2;
    pixelInfo->bitsPerPixel      = 16;
    pixelInfo->bitsPerComponent  = 5;
    pixelInfo->componentCount    = 3;
    pixelInfo->componentMasks[0] = 0x7C00;
    pixelInfo->componentMasks[1] = 0x03E0;
    pixelInfo->componentMasks[2] = 0x001F;

    strncpy(pixelInfo->pixelFormat, IO16BitDirectPixels, sizeof (pixelInfo->pixelFormat));
  } else if (depth == kWiiCafeFBDepth8bpp) {
    pixelInfo->pixelType         = kIOCLUTPixels;
    pixelInfo->bytesPerRow       = pixelInfo->activeWidth;
    pixelInfo->bitsPerPixel      = 8;
    pixelInfo->bitsPerComponent  = 8;
    pixelInfo->componentCount    = 1;
    pixelInfo->componentMasks[0] = 0xFF;

    strncpy(pixelInfo->pixelFormat, IO8BitIndexedPixels, sizeof (pixelInfo->pixelFormat));
  }

  return kIOReturnSuccess;
}

//
// Overrides IOFramebuffer::getCurrentDisplayMode().
//
// Gets the current display mode.
//
IOReturn WiiCafeFB::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) {
  *displayMode = _currentDisplayModeId;
  *depth       = _currentDepth;

  WIIDBGLOG("Current mode: %d, depth: %d", _currentDisplayModeId, _currentDepth);
  return kIOReturnSuccess;
}

//
// Overrides IOFramebuffer::setDisplayMode().
//
// Sets the current display mode.
//
IOReturn WiiCafeFB::setDisplayMode(IODisplayModeID displayMode, IOIndex depth) {
  UInt32 control;
  UInt32 swap;

  if ((displayMode == 0) || (displayMode > 1) || (depth >= kWiiCafeFBDepthMax)) {
    return kIOReturnBadArgument;
  }

  //
  // Disable display.
  //
  writeReg32(kWiiGX2RegD1GrphEnable, 0);

  //
  // Adjust depth and endianness swapping.
  //
  control = readReg32(kWiiGX2RegD1GrphControl);
  control &= ~(kWiiGX2RegD1GrphControlDepthMask | kWiiGX2RegD1GrphControlFormatMask);

  if (depth == kWiiCafeFBDepth32bpp) {
    control |= kWiiGX2RegD1GrphControlDepth32bpp | kWiiGX2RegD1GrphControlFormat32bppARGB8888;
    swap     = kWiiGX2RegD1GrphSwapControlEndianSwap32Bit;
  } else if (depth == kWiiCafeFBDepth16bpp) {
    control |= kWiiGX2RegD1GrphControlDepth16bpp | kWiiGX2RegD1GrphControlFormat16bppARGB555;
    swap     = kWiiGX2RegD1GrphSwapControlEndianSwap16Bit;
  } else if (depth == kWiiCafeFBDepth8bpp) {
    control |= kWiiGX2RegD1GrphControlDepth8bpp | kWiiGX2RegD1GrphControlFormat8bppIndexed;
    swap     = kWiiGX2RegD1GrphSwapControlEndianSwapNone;
  }

  writeReg32(kWiiGX2RegD1GrphControl, control);
  writeReg32(kWiiGX2RegD1GrphSwapControl, swap);

  //
  // Re-enable display.
  //
  writeReg32(kWiiGX2RegD1GrphEnable, kWiiGX2RegD1GrphEnableBit);

  _currentDisplayModeId = displayMode;
  _currentDepth         = depth;

  return kIOReturnSuccess;
}

//
// Overrides IOFramebuffer::getStartupDisplayMode().
//
// Gets the startup display mode.
//
IOReturn WiiCafeFB::getStartupDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) {
  *displayMode = 1;
  *depth       = kWiiCafeFBDepth32bpp;
  return kIOReturnSuccess;
}

//
// Overrides IOFramebuffer::setCLUTWithEntries().
//
// Sets the color lookup table.
//
IOReturn WiiCafeFB::setCLUTWithEntries(IOColorEntry *colors, UInt32 index, UInt32 numEntries, IOOptionBits options) {
  bool byValue = options & kSetCLUTByValue;

  //
  // Build internal color table.
  //
  for (UInt32 i = 0; i < numEntries; i++) {
    UInt32 offset = byValue ? colors[i].index : index + i;
		if (offset > 255){
      continue;
    }

    _clutEntries[offset].red = colors[i].red >> 8;
    _clutEntries[offset].green = colors[i].green >> 8;
    _clutEntries[offset].blue = colors[i].blue >> 8;
  }

  _clutValid = true;
  loadHardwareLUT();

  return kIOReturnSuccess;
}

//
// Overrides IOFramebuffer::setGammaTable().
//
// Sets the gamma table.
//
IOReturn WiiCafeFB::setGammaTable(UInt32 channelCount, UInt32 dataCount, UInt32 dataWidth, void *data) {
  UInt8   *gammaData8;
  UInt16  *gammaData16;

  //
  // Build internal gamma table.
  // OS X 10.1 uses 8-bit data, 10.2 and newer use 16-bit.
  //
  if (dataWidth == 8) {
    gammaData8 = (UInt8 *)data;
    if (channelCount == 3) {
      bcopy(gammaData8, &_gammaTable, 256 * 3);
    } else if (channelCount == 1) {
      for (UInt32 i = 0; i < 256; i++) {
        _gammaTable.red[i]   = gammaData8[i];
        _gammaTable.green[i] = gammaData8[i];
        _gammaTable.blue[i]  = gammaData8[i];
      }
    } else {
      return kIOReturnUnsupported;
    }
  } else if (dataWidth == 16) {
    gammaData16 = (UInt16 *)data;
    if (channelCount == 3) {
      for (UInt32 i = 0; i < 256; i++) {
        _gammaTable.red[i]   = gammaData16[i] >> 8;
        _gammaTable.green[i] = gammaData16[i + 256] >> 8;
        _gammaTable.blue[i]  = gammaData16[i + 512] >> 8;
      }
    } else if (channelCount == 1) {
      for (UInt32 i = 0; i < 256; i++) {
        _gammaTable.red[i]   = gammaData16[i] >> 8;
        _gammaTable.green[i] = gammaData16[i] >> 8;
        _gammaTable.blue[i]  = gammaData16[i] >> 8;
      }
    } else {
      return kIOReturnUnsupported;
    }
  } else {
    return kIOReturnUnsupported;
  }

  _gammaValid = true;
  loadHardwareLUT();

  return kIOReturnSuccess;
}

//
// Overrides IOFramebuffer::getAttribute().
//
// Gets a framebuffer attribute.
//
IOReturn WiiCafeFB::getAttribute(IOSelect attribute, uintptr_t *value) {
  //
  // Report that a hardware cursor is supported.
  //
  if (attribute == kIOHardwareCursorAttribute) {
    if (value != NULL) {
      *value = 1;
    }
    WIIDBGLOG("Hardware cursor supported");
    return kIOReturnSuccess;
  }

  return super::getAttribute(attribute, value);
}

//
// Overrides IOFramebuffer::setCursorImage().
//
// Sets a cursor image as the current hardware cursor.
//
IOReturn WiiCafeFB::setCursorImage(void *cursorImage) {
  IOHardwareCursorDescriptor  cursorDescriptor;
  IOHardwareCursorInfo        cursorInfo;
  IOByteCount                 length;

  //
  // Allocate cursor memory if needed.
  // Max cursor is 32x32x4 (one 4KB page). Cursor must be page-aligned.
  //
  if (_cursorHwDesc == NULL) {
    _cursorHwDesc = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous, kWiiGX2CursorMemSize, PAGE_SIZE);
    if (_cursorHwDesc == NULL) {
      return kIOReturnNoMemory;
    }

    _cursorHwPtr = (volatile UInt32*) _cursorHwDesc->getBytesNoCopy();
    _cursorHwPhysAddr  = _cursorHwDesc->getPhysicalSegment(0, &length);
  }
  
  if (_cursorBuffer == NULL) {
    _cursorBuffer = (UInt32 *)IOMalloc(kWiiGX2CursorMaxSize);
    if (_cursorBuffer == NULL) {
      return kIOReturnNoMemory;
    }
  }

  //
  // Setup cursor descriptor / info structures and convert the cursor image.
  //
  bzero(&cursorDescriptor, sizeof (cursorDescriptor));
  cursorDescriptor.majorVersion = kHardwareCursorDescriptorMajorVersion;
  cursorDescriptor.minorVersion = kHardwareCursorDescriptorMinorVersion;
  cursorDescriptor.width        = kWiiGX2MaxCursorWidth;
  cursorDescriptor.height       = kWiiGX2MaxCursorHeight;
  cursorDescriptor.bitDepth     = 32U;

  bzero(&cursorInfo, sizeof (cursorInfo));
  cursorInfo.majorVersion       = kHardwareCursorInfoMajorVersion;
  cursorInfo.minorVersion       = kHardwareCursorInfoMinorVersion;
  cursorInfo.hardwareCursorData = (UInt8*) _cursorBuffer;

  if (!convertCursorImage(cursorImage, &cursorDescriptor, &cursorInfo)) {
    WIISYSLOG("Failed to convert hardware cursor image");
    return kIOReturnUnsupported;
  }
  if ((cursorInfo.cursorWidth == 0) || (cursorInfo.cursorHeight == 0)) {
    WIISYSLOG("Converted hardware cursor image is invalid size");
    return kIOReturnUnsupported;
  }
  WIIDBGLOG("Converted hardware cursor image at %p (%ux%u)", cursorInfo.hardwareCursorData, cursorInfo.cursorWidth, cursorInfo.cursorHeight);

  //
  // Copy cursor image to hardware buffer.
  //
  // Cursor must be swapped to little endian, and each row needs to be 64 pixels wide.
  //
  for (UInt32 h = 0; h < cursorInfo.cursorHeight; h += 2) {
    for (UInt32 w = 0; w < cursorInfo.cursorWidth; w++) {
      _cursorHwPtr[(h * 64) + w] = OSSwapHostToLittleInt32(_cursorBuffer[h * cursorInfo.cursorWidth + w]);
      _cursorHwPtr[((h + 1) * 64) + w] = OSSwapHostToLittleInt32(_cursorBuffer[(h + 1) * cursorInfo.cursorWidth + w]);
    }
  }
  flushDataCache(_cursorHwPtr, kWiiGX2CursorMemSize);

  //
  // Update hardware buffer to signal to hardware there is a new cursor.
  // OS X seems to offset the position by 4, set hotspot as the hardware cannot handle a negative position.
  //
  writeReg32(kWiiGX2RegD1CursorSurfaceAddress, _cursorHwPhysAddr);
  writeReg32(kWiiGX2RegD1CursorSize,
    ((cursorInfo.cursorHeight - 1) & kWiiGX2RegD1CursorSizeHeightMask) |
    (((cursorInfo.cursorWidth - 1) << kWiiGX2RegD1CursorSizeWidthShift) & kWiiGX2RegD1CursorSizeWidthMask));
  writeReg32(kWiiGX2RegD1CursorHotSpot, (kCursorPosOffset & kWiiGX2RegD1CursorHotSpotYMask) |
  ((kCursorPosOffset << kWiiGX2RegD1CursorHotSpotXShift) & kWiiGX2RegD1CursorHotSpotXMask));
  writeReg32(kWiiGX2RegD1CursorControl, (readReg32(kWiiGX2RegD1CursorControl) & kWiiGX2RegD1CursorControlEnable) | kWiiGX2RegD1CursorControlMode32BitUnAlpha);

  return kIOReturnSuccess;
}

//
// Overrides IOFramebuffer::setCursorState().
//
// Sets the position and visibility of the hardware cursor.
//
IOReturn WiiCafeFB::setCursorState(SInt32 x, SInt32 y, bool visible) {
  UInt32 cursorControl;

  writeReg32(kWiiGX2RegD1CursorPosition,
    ((y + kCursorPosOffset) & kWiiGX2RegD1CursorPositionYMask) |
    (((x + kCursorPosOffset) << kWiiGX2RegD1CursorPositionXShift) & kWiiGX2RegD1CursorPositionXMask));
  cursorControl = readReg32(kWiiGX2RegD1CursorControl);
  if (visible) {
    cursorControl |= kWiiGX2RegD1CursorControlEnable;
  } else {
    cursorControl &= ~(kWiiGX2RegD1CursorControlEnable);
  }
  writeReg32(kWiiGX2RegD1CursorControl, cursorControl);
  return kIOReturnSuccess;
}

//
// Load color/gamma tables into the hardware.
//
void WiiCafeFB::loadHardwareLUT(void) {
  UInt32 colorValue;

  if (!_clutValid || !_gammaValid) {
    return;
  }

  //
  // Reset LUT A.
  //
  writeReg32(kWiiGX2RegDcLutAControl, 0);
  writeReg32(kWiiGX2RegDcLutABlackOffsetBlue, 0);
  writeReg32(kWiiGX2RegDcLutABlackOffsetGreen, 0);
  writeReg32(kWiiGX2RegDcLutABlackOffsetRed, 0);
  writeReg32(kWiiGX2RegDcLutAWhiteOffsetBlue, 0xFFFF);
  writeReg32(kWiiGX2RegDcLutAWhiteOffsetGreen, 0xFFFF);
  writeReg32(kWiiGX2RegDcLutAWhiteOffsetRed, 0xFFFF);

  //
  // Select LUT A for writing color info.
  //
  writeReg32(kWiiGX2RegDcLutRwSelect, 0);
  writeReg32(kWiiGX2RegDcLutRwMode, 0);
  writeReg32(kWiiGX2RegDcLutWriteEnMask, kWiiGX2RegDcLutWriteEnMaskAll);

  //
  // Only load indexed colors in 8-bit mode.
  // Other modes use generated LUT.
  //
  if (_currentDepth == kWiiCafeFBDepth8bpp) {
    writeReg32(kWiiGX2RegDcLutRwIndex, 0);
    for (UInt32 i = 0; i < 256; i++) {
      //
      // Write each color to the LUT.
      // Gamma/color combo is 8-bit, need to shift over to 10-bit.
      //
      colorValue  = (_gammaTable.blue[_clutEntries[i].blue]    << 2) & kWiiGX2RegDcLutColorBlueMask;
      colorValue |= ((_gammaTable.green[_clutEntries[i].green] << 2) << kWiiGX2RegDcLutColorGreenShift) & kWiiGX2RegDcLutColorGreenMask;
      colorValue |= ((_gammaTable.red[_clutEntries[i].red]     << 2) << kWiiGX2RegDcLutColorRedShift) & kWiiGX2RegDcLutColorRedMask;
      writeReg32(kWiiGX2RegDcLutColor, colorValue);
    }
  } else {
    //
    // Start autofill of LUT and wait for completion.
    //
    writeReg32(kWiiGX2RegDcLutAutofill, kWiiGX2RegDcLutAutofillStart);
    while ((readReg32(kWiiGX2RegDcLutAutofill) & kWiiGX2RegDcLutAutofillDone) == 0);
  }

  //
  // Use LUT A for the primary graphics.
  //
  writeReg32(kWiiGX2RegD1GrphLutSelect, 0);
}

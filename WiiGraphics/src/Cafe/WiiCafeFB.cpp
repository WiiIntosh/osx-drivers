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

//
// Overrides IOFramebuffer::init().
//
bool WiiCafeFB::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _debugEnabled = true;

  _memoryMap    = NULL;
  _baseAddr     = NULL;
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
  WIIDBGLOG("getPixelFormats\n");
  return IO32BitDirectPixels;
}

//
// Overrides IOFramebuffer::getDisplayModeCount().
//
// Gets the number of supported display modes.
//
IOItemCount WiiCafeFB::getDisplayModeCount(void) {
  WIIDBGLOG("getDisplayModeCount\n");
  return 1;
}

//
// Overrides IOFramebuffer::getDisplayModes().
//
// Gets the supported display modes.
//
IOReturn WiiCafeFB::getDisplayModes(IODisplayModeID *allDisplayModes) {
  WIIDBGLOG("getDisplayModes\n");
  *allDisplayModes = 1;
  return kIOReturnSuccess;
}

//
// Overrides IOFramebuffer::getInformationForDisplayMode().
//
// Gets detailed information for the specified display mode.
//
IOReturn WiiCafeFB::getInformationForDisplayMode(IODisplayModeID displayMode, IODisplayModeInformation *info) {
  WIIDBGLOG("getInformationForDisplayMode\n");

  if ((displayMode == 0) || (displayMode > 1)) {
    return kIOReturnBadArgument;
  }

  bzero(info, sizeof (*info));
  info->nominalWidth  = 1280; //896;
  info->nominalHeight = 720;//504;
  info->refreshRate   = 60 << 16;
  info->maxDepthIndex = 0;

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
  WIIDBGLOG("getPixelInformation\n");
  if (aperture != kIOFBSystemAperture) {
    return kIOReturnUnsupportedMode;
  }

  if ((displayMode == 0) || (displayMode > 1)) {
    return kIOReturnBadArgument;
  }

  //
  // Pull video parameters from kernel.
  //
  //UInt32 videoBaseAddress  = (UInt32) PE_state.video.v_baseAddr;
 // UInt32 videoWidth        = (UInt32) PE_state.video.v_width;
 // UInt32 videoHeight       = (UInt32) PE_state.video.v_height;
 // UInt32 videoDepth        = (UInt32) PE_state.video.v_depth;
  //UInt32 videoBytesPerRow  = (UInt32) PE_state.video.v_rowBytes;

  bzero(pixelInfo, sizeof (*pixelInfo));

  pixelInfo->bytesPerRow          = 1280 * 4;//896 * 4;
  pixelInfo->bitsPerPixel         = 32;
  pixelInfo->pixelType            = kIORGBDirectPixels;
  pixelInfo->bitsPerComponent     = 8;
  pixelInfo->componentCount       = 3;
  pixelInfo->componentMasks[0]    = 0xFF0000;
  pixelInfo->componentMasks[1]    = 0x00FF00;
  pixelInfo->componentMasks[2]    = 0x0000FF;
  pixelInfo->activeWidth          = 1280;//896;
  pixelInfo->activeHeight         = 720;//504;

  //strncpy(pixelInfo->pixelFormat, IO16BitDirectPixels, sizeof (pixelInfo->pixelFormat));
  strncpy(pixelInfo->pixelFormat, IO32BitDirectPixels, sizeof (pixelInfo->pixelFormat));
  return kIOReturnSuccess;
}

//
// Overrides IOFramebuffer::getCurrentDisplayMode().
//
// Gets the current display mode.
//
IOReturn WiiCafeFB::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) {
  WIIDBGLOG("getCurrentDisplayMode\n");
  *displayMode = 1;
  *depth = 0;
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

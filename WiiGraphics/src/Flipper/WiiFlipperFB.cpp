//
//  WiiFlipperFB.hpp
//  Wii Flipper graphics framebuffer
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <IOKit/IOPlatformExpert.h>

#include "WiiFlipperFB.hpp"

OSDefineMetaClassAndStructors(WiiFlipperFB, super);

//
// Overrides IOFramebuffer::init().
//
bool WiiFlipperFB::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _debugEnabled = true;

  _memoryMap    = NULL;
  _baseAddr     = NULL;
  _fbMemory     = NULL;

  return super::init(dictionary);
}

//
// Overrides IOFramebuffer::start().
//
bool WiiFlipperFB::start(IOService *provider) {
  //
  // Map video interface memory.
  //
  _memoryMap = provider->mapDeviceMemoryWithIndex(0);
  if (_memoryMap == NULL) {
    WIISYSLOG("Failed to map video interface memory");
    return false;
  }
  _baseAddr = (volatile void *)_memoryMap->getVirtualAddress();
  WIIDBGLOG("Mapped registers to %p (physical 0x%X), length: 0x%X", _baseAddr,
    _memoryMap->getPhysicalAddress(), _memoryMap->getLength());

  //
  // Get the framebuffer memory.
  //
  _fbMemory = provider->getDeviceMemoryWithIndex(1);
  if (_fbMemory == NULL) {
    WIISYSLOG("Failed to get framebuffer memory");
    return false;
  }

  if (!super::start(provider)) {
    WIIDBGLOG("super::start() returned false");
    return false;
  }

  //
  // Get the XFB memory.
  //
  _xfbMemory = provider->getDeviceMemoryWithIndex(2);
  if (_xfbMemory == NULL) {
    WIISYSLOG("Failed to get XFB memory");
    return false;
  }

  WIIDBGLOG("Initialized Wii framebuffer");
  return true;
}

IOReturn WiiFlipperFB::enableController(void) {
  return super::enableController();
}

//
// Overrides IOFramebuffer::IOFramebuffer().
//
// Gets the framebuffer memory.
//
IODeviceMemory *WiiFlipperFB::getApertureRange(IOPixelAperture aperture) {
  if (aperture != kIOFBSystemAperture) {
    return NULL;
  }

  _fbMemory->retain();
  return _fbMemory;
}

const char *WiiFlipperFB::getPixelFormats(void) {
  WIIDBGLOG("getPixelFormats\n");
  return IO32BitDirectPixels;
}

IOItemCount WiiFlipperFB::getDisplayModeCount(void) {
  WIIDBGLOG("getDisplayModeCount\n");
  return 1;
}
IOReturn WiiFlipperFB::getDisplayModes(IODisplayModeID *allDisplayModes) {
  WIIDBGLOG("getDisplayModes\n");
  *allDisplayModes = 1;
  return kIOReturnSuccess;
}

IOReturn WiiFlipperFB::getInformationForDisplayMode(IODisplayModeID displayMode, IODisplayModeInformation *info) {
  WIIDBGLOG("getInformationForDisplayMode\n");

  if ((displayMode == 0) || (displayMode > 1)) {
    return kIOReturnBadArgument;
  }

  bzero(info, sizeof (*info));
  info->nominalWidth  = 640;
  info->nominalHeight = 480;
  info->refreshRate   = 60 << 16;
  info->maxDepthIndex = 0;

  return kIOReturnSuccess;
}

UInt64 WiiFlipperFB::getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth) {
  WIIDBGLOG("getPixelFormatsForDisplayMode\n");
  return 0;
}

IOReturn WiiFlipperFB::getPixelInformation(IODisplayModeID displayMode, IOIndex depth, IOPixelAperture aperture, IOPixelInformation *pixelInfo) {
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
  UInt32 videoBaseAddress  = (UInt32) PE_state.video.v_baseAddr;
  UInt32 videoWidth        = (UInt32) PE_state.video.v_width;
  UInt32 videoHeight       = (UInt32) PE_state.video.v_height;
  UInt32 videoDepth        = (UInt32) PE_state.video.v_depth;
  UInt32 videoBytesPerRow  = (UInt32) PE_state.video.v_rowBytes;

  bzero(pixelInfo, sizeof (*pixelInfo));

  pixelInfo->bytesPerRow          = videoBytesPerRow;
  //pixelInfo->bitsPerPixel         = 16;
  //pixelInfo->bitsPerPixel         = 32;
  pixelInfo->pixelType            = kIORGBDirectPixels;
  pixelInfo->bitsPerPixel      = 32;
  pixelInfo->bitsPerComponent  = 8;
  pixelInfo->componentCount    = 3;
  pixelInfo->componentMasks[0] = 0xFF0000;
  pixelInfo->componentMasks[1] = 0x00FF00;
  pixelInfo->componentMasks[2] = 0x0000FF;
  pixelInfo->activeWidth          = videoWidth;
  pixelInfo->activeHeight         = videoHeight;

  //strncpy(pixelInfo->pixelFormat, IO16BitDirectPixels, sizeof (pixelInfo->pixelFormat));
  strncpy(pixelInfo->pixelFormat, IO32BitDirectPixels, sizeof (pixelInfo->pixelFormat));
  return kIOReturnSuccess;
}

IOReturn WiiFlipperFB::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) {
  WIIDBGLOG("getCurrentDisplayMode\n");
  *displayMode = 1;
  *depth = 0;
  return kIOReturnSuccess;
}

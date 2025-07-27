//
//  WiiRvlFB.hpp
//  Wii GX graphics framebuffer
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <IOKit/IOPlatformExpert.h>

#include "WiiRvlFB.hpp"

OSDefineMetaClassAndStructors(WiiRvlFB, super);

//
// Overrides IOFramebuffer::init().
//
bool WiiRvlFB::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _xfbMem  = NULL;

  return super::init(dictionary);
}

//
// Overrides IOFramebuffer::start().
//
bool WiiRvlFB::start(IOService *provider) {
  PE_Video  videoInfo;

  if (!super::start(provider)) {
    WIIDBGLOG("super::start() returned false");
    return false;
  }

  getPlatform()->getConsoleInfo(&videoInfo);

  _xfbMem = IOMemoryDescriptor::withAddress((void*)videoInfo.v_baseAddr, videoInfo.v_height * videoInfo.v_rowBytes, kIODirectionInOut);

  WIIDBGLOG("Initialized Wii framebuffer");
  return true;
}

IOReturn WiiRvlFB::enableController(void) {
  return super::enableController();
}

IODeviceMemory *WiiRvlFB::getApertureRange(IOPixelAperture aperture) {
  WIIDBGLOG("getApertureRange\n");
  if (aperture != kIOFBSystemAperture) {
    return NULL;
  }

  return IODeviceMemory::withRange(_xfbMem->getPhysicalAddress(), _xfbMem->getLength());
}

const char *WiiRvlFB::getPixelFormats(void) {
  WIIDBGLOG("getPixelFormats\n");
  return IO16BitDirectPixels;
}

IOItemCount WiiRvlFB::getDisplayModeCount(void) {
  WIIDBGLOG("getDisplayModeCount\n");
  return 1;
}
IOReturn WiiRvlFB::getDisplayModes(IODisplayModeID *allDisplayModes) {
  WIIDBGLOG("getDisplayModes\n");
  *allDisplayModes = 1;
  return kIOReturnSuccess;
}

IOReturn WiiRvlFB::getInformationForDisplayMode(IODisplayModeID displayMode, IODisplayModeInformation *info) {
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

UInt64 WiiRvlFB::getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth) {
  WIIDBGLOG("getPixelFormatsForDisplayMode\n");
  return 0;
}

IOReturn WiiRvlFB::getPixelInformation(IODisplayModeID displayMode, IOIndex depth, IOPixelAperture aperture, IOPixelInformation *pixelInfo) {
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
  pixelInfo->bitsPerPixel         = 16;
  pixelInfo->pixelType            = kIORGBDirectPixels;
  pixelInfo->bitsPerComponent     = 5;
  pixelInfo->componentCount       = 3;
  pixelInfo->componentMasks[0]    = 0x7c00;
  pixelInfo->componentMasks[1]    = 0x03e0;
  pixelInfo->componentMasks[2]    = 0x001f;
  pixelInfo->activeWidth          = videoWidth;
  pixelInfo->activeHeight         = videoHeight;

  strncpy(pixelInfo->pixelFormat, IO16BitDirectPixels, sizeof (pixelInfo->pixelFormat));
  //strncpy(pixelInfo->pixelFormat, IO32BitDirectPixels, sizeof (pixelInfo->pixelFormat));
  return kIOReturnSuccess;
}

IOReturn WiiRvlFB::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) {
  WIIDBGLOG("getCurrentDisplayMode\n");
  *displayMode = 1;
  *depth = 0;
  return kIOReturnSuccess;
}

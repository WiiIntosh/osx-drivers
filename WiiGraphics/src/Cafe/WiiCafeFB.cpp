//
//  WiiCafeFB.hpp
//  Wii U Cafe graphics framebuffer
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include "WiiCafeFB.hpp"
#include "WiiGX2.hpp"

#include "../../../WiiPlatform/src/PE/WiiPE.hpp"

OSDefineMetaClassAndStructors(WiiCafeFB, super);

//
// Overrides IOFramebuffer::init().
//
bool WiiCafeFB::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _debugEnabled = true;

  _memoryMap  = NULL;
  _baseAddr   = NULL;

  return super::init(dictionary);
}

//
// Overrides IOFramebuffer::start().
//
bool WiiCafeFB::start(IOService *provider) {
  WiiPE *wiiPE;

  WIIDBGLOG("Initializing Cafe framebuffer");

  wiiPE = OSDynamicCast(WiiPE, getPlatform());
  if (wiiPE == NULL) {
    return false;
  }

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

 // wiiPE->setPhysMemoryCacheInhibit(readReg32(kWiiGX2RegD1GrphPriSurfaceAddress), 1280 * 720 * 4);

  if (!super::start(provider)) {
    WIIDBGLOG("super::start() returned false");
    return false;
  }

  WIIDBGLOG("Initialized Cafe framebuffer");
  return true;
}

IOReturn WiiCafeFB::enableController(void) {
  return super::enableController();
}

IODeviceMemory *WiiCafeFB::getApertureRange(IOPixelAperture aperture) {
  WIIDBGLOG("getApertureRange\n");
  if (aperture != kIOFBSystemAperture) {
    return NULL;
  }

  /*vm_offset_t vAddr = ml_io_map(readReg32(kWiiGX2RegD1GrphPriSurfaceAddress), 1280 * 720 * 4);
  IOLog("Mapped this one to 0x%X:0x%X", vAddr, readReg32(kWiiGX2RegD1GrphPriSurfaceAddress));

  IODeviceMemory *devMem = IODeviceMemory::withRange(readReg32(kWiiGX2RegD1GrphPriSurfaceAddress), 1280 * 720 * 4);
  devMem->setMapping(kernel_task, vAddr, 0);

  return devMem;*/

 // writeReg32(kWiiGX2RegD1GrphPriSurfaceAddress, 0x18000000);
  return IODeviceMemory::withRange(readReg32(kWiiGX2RegD1GrphPriSurfaceAddress), 1280 * 720 * 4);
  //return IODeviceMemory::withRange(0x14000000 + 0x38C0000, 896 * 506 * 4);
}

const char *WiiCafeFB::getPixelFormats(void) {
  WIIDBGLOG("getPixelFormats\n");
  return IO32BitDirectPixels;
}

IOItemCount WiiCafeFB::getDisplayModeCount(void) {
  WIIDBGLOG("getDisplayModeCount\n");
  return 1;
}
IOReturn WiiCafeFB::getDisplayModes(IODisplayModeID *allDisplayModes) {
  WIIDBGLOG("getDisplayModes\n");
  *allDisplayModes = 1;
  return kIOReturnSuccess;
}

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

UInt64 WiiCafeFB::getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth) {
  WIIDBGLOG("getPixelFormatsForDisplayMode\n");
  return 0;
}

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

IOReturn WiiCafeFB::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) {
  WIIDBGLOG("getCurrentDisplayMode\n");
  *displayMode = 1;
  *depth = 0;
  return kIOReturnSuccess;
}

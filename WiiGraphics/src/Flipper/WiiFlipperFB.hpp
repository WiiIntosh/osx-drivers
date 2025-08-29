//
//  WiiFlipperFB.hpp
//  Wii Flipper graphics framebuffer
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiFlipperFB_hpp
#define WiiFlipperFB_hpp

#include <IOKit/graphics/IOFramebuffer.h>
#include "WiiCommon.hpp"

//
// Represents the Wii graphics framebuffer.
//
class WiiFlipperFB : public IOFramebuffer {
  OSDeclareDefaultStructors(WiiFlipperFB);
  WiiDeclareLogFunctions("fb");
  typedef IOFramebuffer super;

private:
  IOMemoryMap         *_memoryMap;
  volatile void       *_baseAddr;
  IODeviceMemory      *_fbMemory;
  IODeviceMemory      *_xfbMemory;

  IOMemoryDescriptor *_xfbMem;

public:
  //
  // Overrides.
  //
  bool init(OSDictionary *dictionary = 0);
  bool start(IOService *provider);
  IOReturn enableController(void);
  IODeviceMemory *getApertureRange(IOPixelAperture aperture);
  const char *getPixelFormats(void);
  IOItemCount getDisplayModeCount(void);
  IOReturn getDisplayModes( IODisplayModeID * allDisplayModes);
  IOReturn getInformationForDisplayMode(IODisplayModeID displayMode, IODisplayModeInformation *info);
  UInt64 getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth);
  IOReturn getPixelInformation(IODisplayModeID displayMode, IOIndex depth, IOPixelAperture aperture, IOPixelInformation *pixelInfo);
  IOReturn getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth);
};

#endif

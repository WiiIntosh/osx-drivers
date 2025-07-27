//
//  WiiCafeFB.hpp
//  Wii U Cafe graphics framebuffer
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiCafeFB_hpp
#define WiiCafeFB_hpp

#include <IOKit/graphics/IOFramebuffer.h>
#include "WiiCommon.hpp"

//
// Represents the Wii U graphics framebuffer.
//
class WiiCafeFB : public IOFramebuffer {
  OSDeclareDefaultStructors(WiiCafeFB);
  WiiDeclareLogFunctions("fb");
  typedef IOFramebuffer super;

private:
  IOMemoryMap         *_memoryMap;
  volatile void       *_baseAddr;

  inline UInt32 readReg32(UInt32 offset) {
    return OSReadBigInt32(_baseAddr, offset);
  }
  inline void writeReg32(UInt32 offset, UInt32 data) {
    OSWriteBigInt32(_baseAddr, offset, data);
  }

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

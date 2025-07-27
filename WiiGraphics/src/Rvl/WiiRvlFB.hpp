//
//  WiiRvlFB.hpp
//  Wii GX graphics framebuffer
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiRvlFB_hpp
#define WiiRvlFB_hpp

#include <IOKit/graphics/IOFramebuffer.h>
#include "WiiCommon.hpp"

//
// Represents the Wii graphics framebuffer.
//
class WiiRvlFB : public IOFramebuffer {
  OSDeclareDefaultStructors(WiiRvlFB);
  WiiDeclareLogFunctions("fb");
  typedef IOFramebuffer super;

private:
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

//
//  HollywoodInterruptController.hpp
//  Wii Hollywood platform interrupt controller
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef HollywoodInterruptController_hpp
#define HollywoodInterruptController_hpp

#include <IOKit/IOInterrupts.h>
#include <IOKit/IOInterruptController.h>

#include "WiiCommon.hpp"
#include "WiiHollywood.hpp"

//
// Represents the Hollywood platform interrupt controller.
//
class HollywoodInterruptController : public IOInterruptController {
  OSDeclareDefaultStructors(HollywoodInterruptController);
  WiiDeclareLogFunctions("hlwic");
  typedef IOInterruptController super;

private:
  IOMemoryMap         *_memoryMap;
  volatile void       *_baseAddr;

  inline UInt32 readReg32(UInt32 offset) {
    return OSReadBigInt32(_baseAddr, offset - kWiiHollywoodICOffset);
  }
  inline void writeReg32(UInt32 offset, UInt32 data) {
    OSWriteBigInt32(_baseAddr, offset - kWiiHollywoodICOffset, data);
  }

public:
  //
  // Overrides.
  //
  bool init(OSDictionary *dictionary = 0);
  bool start(IOService *provider);
  IOInterruptAction getInterruptHandlerAddress(void);
  IOReturn handleInterrupt(void *refCon, IOService *nub, int source);
  int getVectorType(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector);
  void disableVectorHard(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector);
  void enableVector(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector);
};

#endif

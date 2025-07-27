//
//  LatteInterruptController.hpp
//  Wii U Latte chipset interrupt controller
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef LatteInterruptController_hpp
#define LatteInterruptController_hpp

#include <IOKit/IOInterrupts.h>
#include <IOKit/IOInterruptController.h>

#include "WiiCommon.hpp"

//
// Represents the Latte chipset interrupt controller.
//
class LatteInterruptController : public IOInterruptController {
  OSDeclareDefaultStructors(LatteInterruptController);
  WiiDeclareLogFunctions("latteic");
  typedef IOInterruptController super;

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
  IOInterruptAction getInterruptHandlerAddress(void);
  IOReturn handleInterrupt(void *refCon, IOService *nub, int source);
  int getVectorType(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector);
  void disableVectorHard(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector);
  void enableVector(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector);
};

#endif

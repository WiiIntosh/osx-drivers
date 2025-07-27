//
//  WiiInterruptController.hpp
//  Wii platform interrupt controller
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiInterruptController_hpp
#define WiiInterruptController_hpp

#include <IOKit/IOInterrupts.h>
#include <IOKit/IOInterruptController.h>
#include "WiiCommon.hpp"
#include "WiiProcessorInterface.hpp"

//
// Represents the Wii platform interrupt controller.
//
class WiiInterruptController : public IOInterruptController {
  OSDeclareDefaultStructors(WiiInterruptController);
  WiiDeclareLogFunctions("pic");
  typedef IOInterruptController super;

private:
  IOMemoryMap         *_memoryMap;
  volatile void       *_baseAddr;
  bool                _isCafe;

  inline UInt32 readReg32(UInt32 offset) {
    return OSReadBigInt32(_baseAddr, offset);
  }
  inline void writeReg32(UInt32 offset, UInt32 data) {
    OSWriteBigInt32(_baseAddr, offset, data);
  }
  inline UInt32 readCafeIntCause32(UInt32 core) {
    return OSReadBigInt32(_baseAddr, kWiiPIRegCafeInterruptCauseBase + (core * 8));
  }
  inline void writeCafeIntCause32(UInt32 core, UInt32 data) {
    OSWriteBigInt32(_baseAddr, kWiiPIRegCafeInterruptCauseBase + (core * 8), data);
  }
  inline UInt32 readCafeIntMask32(UInt32 core) {
    return OSReadBigInt32(_baseAddr, kWiiPIRegCafeInterruptMaskBase + (core * 8));
  }
  inline void writeCafeIntMask32(UInt32 core, UInt32 data) {
    OSWriteBigInt32(_baseAddr, kWiiPIRegCafeInterruptMaskBase + (core * 8), data);
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

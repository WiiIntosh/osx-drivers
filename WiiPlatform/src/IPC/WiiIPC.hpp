//
//  WiiIPC.hpp
//  Wii IPC services
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiIPC_hpp
#define WiiIPC_hpp

#include <IOKit/IOService.h>
#include "WiiCommon.hpp"

//
// Represents the the IPC channel between ARM Starlet/Starbuck and the PowerPC Broadway/Espresso.
//
class WiiIPC : public IOService {
  OSDeclareDefaultStructors(WiiIPC);
  WiiDeclareLogFunctions("ipc");
  typedef IOService super;

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
  IOReturn callPlatformFunction(const OSSymbol *functionName, bool waitForFunction,
                                void *param1, void *param2, void *param3, void *param4);

  int doHaltRestart(unsigned int type);
};

#endif

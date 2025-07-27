//
//  WiiCafeIPC.hpp
//  Wii U IPC service
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiCafeIPC_hpp
#define WiiCafeIPC_hpp

#include <IOKit/IOService.h>
#include "WiiCommon.hpp"

//
// Represents the the IPC channel between Starbuck and the PowerPC.
//
class WiiCafeIPC : public IOService {
  OSDeclareDefaultStructors(WiiCafeIPC);
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

  int doHaltRestart(unsigned int type);
};

#endif

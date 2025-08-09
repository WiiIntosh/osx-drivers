//
//  WiiEXI.hpp
//  Wii EXI controller interface
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiEXI_hpp
#define WiiEXI_hpp

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOService.h>

#include "EXIRegs.hpp"
#include "WiiCommon.hpp"

//
// Represents the Wii EXI controller.
//
class WiiEXI : public IOService {
  OSDeclareDefaultStructors(WiiEXI);
  WiiDeclareLogFunctions("exi");
  typedef IOService super;

private:
  IOMemoryMap         *_memoryMap;
  volatile void       *_baseAddr;
  bool                _isCafe;

  IOWorkLoop                *_workLoop;
  IOCommandGate             *_commandGate;
  IOInterruptEventSource    *_interruptEventSource;

  UInt32                    _rtcBias;

  inline UInt32 readReg32(UInt32 channel, UInt32 offset) {
    return OSReadBigInt32(_baseAddr, (channel * kWiiEXIChannelRegOffset) + offset);
  }
  inline void writeReg32(UInt32 channel, UInt32 offset, UInt32 data) {
    OSWriteBigInt32(_baseAddr, (channel * kWiiEXIChannelRegOffset) + offset, data);
  }

  void handleInterrupt(IOInterruptEventSource *intEventSource, int count);

  IOReturn selectDevice(UInt32 channel, UInt32 device, WiiEXIClock clock, bool enableInt);
  void deselectDevice(UInt32 channel);
  IOReturn readWriteImmData(UInt32 channel, IODirection direction, UInt32 data, UInt32 dataLength, UInt32 *outData);
  //IOReturn readWriteDmaData(UInt32 channel, IOMemoryDescriptor *buffer);

  IOReturn readRTCData(UInt32 *secs);
  IOReturn getRTCGated(UInt32 *secs);

public:
  //
  // Overrides.
  //
  bool init(OSDictionary *dictionary = 0);
  bool start(IOService *provider);

  //
  // EXI functions.
  //
  UInt32 getRTC(void);
};

#endif

//
//  WiiAudioDriver.hpp
//  Wii audio driver
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiAudioDriver_hpp
#define WiiAudioDriver_hpp

#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/audio/IOAudioDevice.h>

#include "WiiCommon.hpp"

#define kWiiAudioBufferSize     0x8000

class WiiAudioEngine;

//
// Represents the Wii audio driver interface.
//
class WiiAudioDriver : public IOAudioDevice {
  OSDeclareDefaultStructors(WiiAudioDriver);
  WiiDeclareLogFunctions("audio");
  typedef IOAudioDevice super;

private:
  IOMemoryMap         *_audioMemoryMap;
  volatile void       *_audioBaseAddr;
  IOService           *_dspDevice;
  IOMemoryMap         *_dspMemoryMap;
  volatile void       *_dspBaseAddr;

  IOFilterInterruptEventSource  *_interruptEventSource;
  IOBufferMemoryDescriptor      *_outputBuffer;
  void                          *_outputBufferPtr;
  void                          *_outputBufferLattePtr;

  WiiAudioEngine      *_audioOutputEngine;
  WiiAudioEngine      *_audioOutputLatteEngine;

  bool                _isCafe;

  inline UInt32 readAudioReg32(UInt32 offset) {
    return OSReadBigInt32(_audioBaseAddr, offset);
  }
  inline void writeAudioReg32(UInt32 offset, UInt32 data) {
    OSWriteBigInt32(_audioBaseAddr, offset, data);
  }
  inline UInt16 readDspReg16(UInt32 offset) {
    return OSReadBigInt16(_dspBaseAddr, offset);
  }
  inline void writeDspReg16(UInt32 offset, UInt16 data) {
    OSWriteBigInt16(_dspBaseAddr, offset, data);
  }

  void handleInterrupt(IOInterruptEventSource *intEventSource, int count);
  bool filterInterrupt(IOFilterInterruptEventSource *filterIntEventSource);

  void dspReset(void);
  void dspLoadSample(IOPhysicalAddress physAddr, IOByteCount length, bool latte);
  void dspStartSample(bool latte);
  void dspStopSample(bool latte);
  UInt32 dspGetBytesLeft(bool latte);

  WiiAudioEngine *createAudioEngine(void *buffer, IOByteCount bufferLength, const char *description);

public:
  //
  // Overrides.
  //
  bool init(OSDictionary *dictionary = 0);
  bool initHardware(IOService *provider);

  //
  // Driver-Engine functions.
  //
  IOReturn startAudioDsp(WiiAudioEngine *audioEngine);
  IOReturn stopAudioDsp(WiiAudioEngine *audioEngine);
  UInt32 getAudioDspBytesLeft(WiiAudioEngine *audioEngine);
};

#endif

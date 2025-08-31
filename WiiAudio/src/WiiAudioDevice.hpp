//
//  WiiAudioDevice.hpp
//  Wii audio driver
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiAudioDevice_hpp
#define WiiAudioDevice_hpp

#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/audio/IOAudioControl.h>
#include <IOKit/audio/IOAudioDevice.h>

#include "WiiCommon.hpp"

class WiiAudioEngine;

//
// Represents the Wii audio device interface.
//
class WiiAudioDevice : public IOAudioDevice {
  OSDeclareDefaultStructors(WiiAudioDevice);
  WiiDeclareLogFunctions("audio");
  typedef IOAudioDevice super;

private:
  // MMIO.
  IOMemoryMap         *_audioMemoryMap;
  volatile void       *_audioBaseAddr;
  IOService           *_dspDevice;
  IOMemoryMap         *_dspMemoryMap;
  volatile void       *_dspBaseAddr;
  bool                _isCafe;

  // Buffers.
  IOFilterInterruptEventSource  *_interruptEventSource;
  IOBufferMemoryDescriptor      *_outputBufferDesc;
  void                          *_outputBuffer;
  void                          *_outputBufferLatte;
  IOPhysicalAddress             _outputBufferPhysAddr;
  IOPhysicalAddress             _outputBufferLattePhysAddr;

  // Audio engines.
  WiiAudioEngine      *_audioOutputEngine;
  WiiAudioEngine      *_audioOutputLatteEngine;

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
  IOReturn handleControlChange(IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
  IOReturn handleLatteControlChange(IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);

  void dspReset(void);
  void dspLoadSample(IOPhysicalAddress physAddr, IOByteCount length, bool latte);
  void dspStartSample(bool latte);
  void dspStopSample(bool latte);
  UInt32 dspGetBytesLeft(bool latte);

  WiiAudioEngine *createAudioEngine(void *buffer, IOByteCount bufferLength, const char *description,
                                    IOAudioControl::IntValueChangeHandler controlHandler);
  IOReturn createAudioPorts(WiiAudioEngine *audioEngine, SInt32 type, const char *name);

public:
  //
  // Overrides.
  //
  bool init(OSDictionary *dictionary = 0);
  bool initHardware(IOService *provider);

  //
  // Device-Engine functions.
  //
  IOReturn startAudioDsp(WiiAudioEngine *audioEngine);
  IOReturn stopAudioDsp(WiiAudioEngine *audioEngine);
  UInt32 getAudioDspBytesLeft(WiiAudioEngine *audioEngine);
};

#endif

//
//  WiiAudioEngine.hpp
//  Wii audio engine
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiAudioEngine_hpp
#define WiiAudioEngine_hpp

#include <IOKit/audio/IOAudioEngine.h>
#include "WiiCommon.hpp"

class WiiAudioDriver;

//
// Represents a Wii audio engine.
//
class WiiAudioEngine : public IOAudioEngine {
  OSDeclareDefaultStructors(WiiAudioEngine);
  WiiDeclareLogFunctions("audeng");
  typedef IOAudioEngine super;

private:
  WiiAudioDriver    *_audioDriver;
  void              *_sampleBuffer;
  IOByteCount       _sampleBufferLength;
  const char        *_deviceDescription;

public:
  //
  // Overrides.
  //
  bool initHardware(IOService *provider);
  UInt32 getCurrentSampleFrame();
  IOReturn performAudioEngineStart();
  IOReturn performAudioEngineStop();
  IOReturn performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate);
  IOReturn clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames,
                             const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);

  bool init(WiiAudioDriver *driver, void *buffer, IOByteCount bufferLength, const char *description);
};

#endif

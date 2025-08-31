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

class WiiAudioDevice;

#define kWiiMinVolume     0
#define kWiiMaxVolume     99

//
// Represents a Wii audio engine.
//
class WiiAudioEngine : public IOAudioEngine {
  OSDeclareDefaultStructors(WiiAudioEngine);
  WiiDeclareLogFunctions("audeng");
  typedef IOAudioEngine super;

private:
  WiiAudioDevice    *_audioDevice;
  void              *_sampleBuffer;
  IOByteCount       _sampleBufferLength;
  const char        *_deviceDescription;

  SInt32            _currentVolume;
  SInt32            _currentMute;
  float             _logTable[kWiiMaxVolume + 1];

  IOReturn handleVolumeChange(IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
  IOReturn handleMuteChange(IOAudioControl *audioControl, SInt32 oldValue, SInt32 newValue);
  void createVolumeLogTable(void);
  IOReturn createControls(void);

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

  bool init(WiiAudioDevice *device, void *buffer, IOByteCount bufferLength, const char *description);
};

#endif

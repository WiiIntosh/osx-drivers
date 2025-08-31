//
//  WiiAudioEngine.cpp
//  Wii audio engine
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <IOKit/audio/IOAudioStream.h>

#include "WiiAudioDevice.hpp"
#include "WiiAudioEngine.hpp"
#include "AudioRegs.hpp"

OSDefineMetaClassAndStructors(WiiAudioEngine, super);

//
// Initializes the class.
//
bool WiiAudioEngine::init(WiiAudioDevice *device, void *buffer, IOByteCount bufferLength, const char *description) {
  IOReturn status;

  WiiCheckDebugArgs();

  if (!super::init(NULL)) {
    return false;
  }

  _audioDevice        = device;
  _sampleBuffer       = buffer;
  _sampleBufferLength = bufferLength;
  _deviceDescription  = description;
  _currentVolume 			= kWiiMaxVolume;
  _currentMute   			= 0;

	createVolumeLogTable();

  status = createControls();
  if (status != kIOReturnSuccess) {
    return false;
  }

  return true;
}

//
// Overrides IOAudioEngine::initHardware().
//
// Initializes the audio engine.
//
bool WiiAudioEngine::initHardware(IOService *provider) {
  IOAudioStream       *audioStream;
  IOAudioSampleRate   sampleRate;
  IOAudioStreamFormat format = {
    kWiiAudioNumChannels,
    kIOAudioStreamSampleFormatLinearPCM,
    kIOAudioStreamNumericRepresentationSignedInt,
    kWiiAudioBitWidth,
    kWiiAudioBitWidth,
    kIOAudioStreamAlignmentHighByte,
    kIOAudioStreamByteOrderBigEndian,
    true,
    0
  };

  WIIDBGLOG("Initializing audio engine (buffer %p)", _sampleBuffer);

  if (!super::initHardware(provider)) {
    return false;
  }

  //
  // Add description for 10.2 and older only.
  //
  if (getKernelVersion() <= kKernelVersionJaguar) {
    setDescription(_deviceDescription);
  }

  sampleRate.whole    = 48000;
  sampleRate.fraction = 0;

  setSampleRate(&sampleRate);
  setNumSampleFramesPerBuffer(_sampleBufferLength / kWiiAudioBytesPerFrame);
  setSampleLatency(32);
  setSampleOffset(32);

  //
  // Create the output stream. Wii has no input hardware.
  //
  audioStream = new IOAudioStream;
  if (audioStream == NULL) {
    return false;
  }
  if (!audioStream->initWithAudioEngine(this, kIOAudioStreamDirectionOutput, 1)) {
    audioStream->release();
    return false;
  }

  audioStream->setSampleBuffer(_sampleBuffer, _sampleBufferLength);
  audioStream->addAvailableFormat(&format, &sampleRate, &sampleRate);
  audioStream->setFormat(&format);

  //
  // Add the output stream.
  //
  addAudioStream(audioStream);
  audioStream->release();

  return true;
}

//
// Overrides IOAudioEngine::getCurrentSampleFrame().
//
// Gets the current frame being processed by the audio hardware.
//
UInt32 WiiAudioEngine::getCurrentSampleFrame() {
  return (_sampleBufferLength - _audioDevice->getAudioDspBytesLeft(this)) / kWiiAudioBytesPerFrame;
}

//
// Overrides IOAudioEngine::performAudioEngineStart().
//
// Starts the audio hardware.
//
IOReturn WiiAudioEngine::performAudioEngineStart() {
  takeTimeStamp(false);
  return _audioDevice->startAudioDsp(this);
}

//
// Overrides IOAudioEngine::performAudioEngineStop().
//
// Stops the audio hardware.
//
IOReturn WiiAudioEngine::performAudioEngineStop() {
  return _audioDevice->stopAudioDsp(this);
}

//
// Overrides IOAudioEngine::performFormatChange().
//
IOReturn WiiAudioEngine::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate) {
  return kIOReturnSuccess; // TODO
}

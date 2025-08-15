//
//  WiiAudioEngine.cpp
//  Wii audio engine
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <IOKit/audio/IOAudioStream.h>

#include "WiiAudioDriver.hpp"
#include "WiiAudioEngine.hpp"
#include "AudioRegs.hpp"

OSDefineMetaClassAndStructors(WiiAudioEngine, super);

//
// Initializes the class.
//
bool WiiAudioEngine::init(WiiAudioDriver *driver, void *buffer, IOByteCount bufferLength, const char *description) {
  WiiCheckDebugArgs();

  if (!super::init(NULL)) {
    return false;
  }

  _audioDriver        = driver;
  _sampleBuffer       = buffer;
  _sampleBufferLength = bufferLength;
  _deviceDescription  = description;

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

  sampleRate.whole    = 48000;
  sampleRate.fraction = 0;

  setDescription(_deviceDescription);
  setSampleRate(&sampleRate);

  setNumSampleFramesPerBuffer(_sampleBufferLength / kWiiAudioNumChannels / (kWiiAudioBitWidth / 8));

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
UInt32 WiiAudioEngine::getCurrentSampleFrame() {
  return ((_sampleBufferLength - _audioDriver->getAudioDspBytesLeft(this)) & ~(1024 - 1)) / kWiiAudioNumChannels / (kWiiAudioBitWidth / 8);
}

//
// Overrides IOAudioEngine::performAudioEngineStart().
//
IOReturn WiiAudioEngine::performAudioEngineStart() {
  takeTimeStamp(false);
  return _audioDriver->startAudioDsp(this);
}

//
// Overrides IOAudioEngine::performAudioEngineStop().
//
IOReturn WiiAudioEngine::performAudioEngineStop() {
  return _audioDriver->stopAudioDsp(this);
}

//
// Overrides IOAudioEngine::performFormatChange().
//
IOReturn WiiAudioEngine::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate) {
  return kIOReturnSuccess; // TODO
}

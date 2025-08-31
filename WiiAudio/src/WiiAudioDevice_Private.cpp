//
//  WiiAudioDevice_Private.cpp
//  Wii audio driver
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <IOKit/audio/IOAudioPort.h>
#include <IOKit/audio/IOAudioSelectorControl.h>
#include <IOKit/audio/IOAudioDefines.h>

#include "WiiAudioDevice.hpp"
#include "WiiAudioEngine.hpp"
#include "AudioRegs.hpp"

//
// Interrupt handler function.
//
void WiiAudioDevice::handleInterrupt(IOInterruptEventSource *intEventSource, int count) {
  // Not called.
}

//
// Handles filtering for the interrupt event source.
//
// This is a primary interrupt handler, need to spend as little time here as possible.
//
bool WiiAudioDevice::filterInterrupt(IOFilterInterruptEventSource *filterIntEventSource) {
  UInt16 dspControl;

  //
  // Check if one of the audio interfaces triggered and interrupt, and hand it off to the appropriate engine.
  //
  dspControl = readDspReg16(kWiiAudioDspRegControlStatus);
  if ((dspControl & (kWiiAudioDspRegControlStatusLatteAudioIntStatus | kWiiAudioDspRegControlStatusAudioIntStatus)) == 0) {
    return false;
  }

  //
  // Record current timestamp on engines.
  //
  if (dspControl & kWiiAudioDspRegControlStatusAudioIntStatus) {
    _audioOutputEngine->takeTimeStamp();
  }
  if (_isCafe && (dspControl & kWiiAudioDspRegControlStatusLatteAudioIntStatus)) {
    _audioOutputLatteEngine->takeTimeStamp();
  }

  dspControl &= ~(kWiiAudioDspRegControlStatusDspIntEnable | kWiiAudioDspRegControlStatusAramIntStatus | kWiiAudioDspRegControlStatusDspIntStatus);
  writeDspReg16(kWiiAudioDspRegControlStatus, dspControl);
  return false;
}

//
// Resets the DSP.
//
void WiiAudioDevice::dspReset(void) {
  WIIDBGLOG("Resetting DSP");
  UInt16 dspControl = readDspReg16(kWiiAudioDspRegControlStatus) &
    ~(kWiiAudioDspRegControlStatusAudioIntStatus |
      kWiiAudioDspRegControlStatusAramIntStatus |
      kWiiAudioDspRegControlStatusDspIntStatus |
      kWiiAudioDspRegControlStatusLatteAudioIntStatus);
  writeDspReg16(kWiiAudioDspRegControlStatus, dspControl | (kWiiAudioDspRegControlStatusBootMode | kWiiAudioDspRegControlStatusReset));
  while (readDspReg16(kWiiAudioDspRegControlStatus) & kWiiAudioDspRegControlStatusReset);
  WIIDBGLOG("DSP reset");
}


//
// Loads a sample to be played.
//
void WiiAudioDevice::dspLoadSample(IOPhysicalAddress physAddr, IOByteCount length, bool latte) {
  WIIDBGLOG("Loading sample: 0x%X, length: 0x%X, Latte: %u", physAddr, length, latte);
  if (latte) {
    writeDspReg16(kWiiAudioDspRegLatteDmaStartHigh, (physAddr >> 16) & 0xFFFF);
    writeDspReg16(kWiiAudioDspRegLatteDmaStartLow, physAddr & 0xFFE0);
    writeDspReg16(kWiiAudioDspRegLatteDmaLength, (readDspReg16(kWiiAudioDspRegLatteDmaLength) & kWiiAudioDspRegLatteDmaLengthPlay) |
      (length >> kWiiAudioDspRegLatteDmaLengthShift));
  } else {
    writeDspReg16(kWiiAudioDspRegDmaStartHigh, (physAddr >> 16) & 0xFFFF);
    writeDspReg16(kWiiAudioDspRegDmaStartLow, physAddr & 0xFFE0);
    writeDspReg16(kWiiAudioDspRegDmaLength, (readDspReg16(kWiiAudioDspRegDmaLength) & kWiiAudioDspRegDmaLengthPlay) |
      (length >> kWiiAudioDspRegDmaLengthShift));
  }
}

//
// Starts playback of a loaded sample.
//
void WiiAudioDevice::dspStartSample(bool latte) {
  WIIDBGLOG("Starting sample, Latte: %u", latte);
  if (latte) {
    writeDspReg16(kWiiAudioDspRegLatteDmaLength, readDspReg16(kWiiAudioDspRegLatteDmaLength) | kWiiAudioDspRegLatteDmaLengthPlay);
  } else {
    writeDspReg16(kWiiAudioDspRegDmaLength, readDspReg16(kWiiAudioDspRegDmaLength) | kWiiAudioDspRegDmaLengthPlay);
  }
}

//
// Stops playback of a loaded sample.
//
void WiiAudioDevice::dspStopSample(bool latte) {
  WIIDBGLOG("Stopping sample, Latte: %u", latte);
  if (latte) {
    writeDspReg16(kWiiAudioDspRegLatteDmaLength, readDspReg16(kWiiAudioDspRegLatteDmaLength) & ~(kWiiAudioDspRegLatteDmaLengthPlay));
  } else {
    writeDspReg16(kWiiAudioDspRegDmaLength, readDspReg16(kWiiAudioDspRegDmaLength) & ~(kWiiAudioDspRegDmaLengthPlay));
  }
}

//
// Gets the bytes left in the sample.
//
UInt32 WiiAudioDevice::dspGetBytesLeft(bool latte) {
  if (latte) {
    return (UInt32)(readDspReg16(kWiiAudioDspRegLatteDmaBytesLeft)) << kWiiAudioDspRegLatteDmaBytesLeftShift;
  } else {
    return (UInt32)(readDspReg16(kWiiAudioDspRegDmaBytesLeft)) << kWiiAudioDspRegDmaBytesLeftShift;
  }
}

//
// Creates an audio engine.
//
WiiAudioEngine *WiiAudioDevice::createAudioEngine(void *buffer, IOByteCount bufferLength, const char *description) {
  WiiAudioEngine  *audioEngine;
  IOAudioControl  *control;

  //
  // Create a new audio engine with the buffer.
  //
  audioEngine = new WiiAudioEngine;
  if (audioEngine == NULL) {
    return NULL;
  }

  if (!audioEngine->init(this, buffer, bufferLength, description)) {
    OSSafeReleaseNULL(audioEngine);
    return NULL;
  }

  return audioEngine;
}

//
// Creates audio ports for an audio engine.
//
IOReturn WiiAudioDevice::createAudioPorts(WiiAudioEngine *audioEngine, SInt32 type, const char *name) {
  IOAudioPort             *outputPort;
  IOAudioSelectorControl  *outputSelector;
  IOReturn                status;

  outputPort = IOAudioPort::withAttributes(kIOAudioPortTypeOutput, "Output port");
  if (outputPort == NULL) {
    return kIOReturnNoMemory;
  }

  outputSelector = IOAudioSelectorControl::createOutputSelector(type, kIOAudioControlChannelIDAll);
	if (outputSelector == NULL) {
    outputPort->release();
    return kIOReturnNoMemory;
  }

  //
  // Add selector for nice name in System Preferences.
  //
  audioEngine->addDefaultAudioControl(outputSelector);
  outputSelector->addAvailableSelection(type, name);
  outputSelector->release();

  //
  // Add the port to the engine.
  //
  status = attachAudioPort(outputPort, audioEngine, NULL);
  outputPort->release();

  return status;
}

//
//  WiiAudioDriver.cpp
//  Wii audio driver
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include "WiiAudioDriver.hpp"
#include "WiiAudioEngine.hpp"
#include "AudioRegs.hpp"

OSDefineMetaClassAndStructors(WiiAudioDriver, super);

//
// Overrides IOAudioDevice::init().
//
bool WiiAudioDriver::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _debugEnabled = true;

  _interruptEventSource     = NULL;
  _outputBuffer             = NULL;
  _audioOutputEngine        = NULL;
  _audioOutputLatteEngine   = NULL;

  return super::init(dictionary);
}

//
// Overrides IOAudioDevice::initHardware().
//
bool WiiAudioDriver::initHardware(IOService *provider) {
  IOPhysicalAddress   outputPhysAddr;
  IOByteCount         length;
  UInt16              dspControl;
  IOReturn            status;

  WIIDBGLOG("Initializing Wii audio");

  if (!super::initHardware(provider)) {
    WIISYSLOG("super::initHardware() returned false");
    return false;
  }

  //
  // Get platform type.
  //
  status = ((IOService*) getPlatform())->callPlatformFunction(kWiiFuncPlatformIsCafe, true, (void*) &_isCafe, NULL, NULL, NULL);
  if (status != kIOReturnSuccess) {
    WIISYSLOG("Failed to get platform type");
    return false;
  }

  setManufacturerName("Nintendo");
  setDeviceName(_isCafe ? "Wii U Audio" : "Wii Audio");
  setDeviceTransportType(kIOAudioDeviceTransportTypeBuiltIn);

  //
  // Map audio interface memory.
  //
  _audioMemoryMap = provider->mapDeviceMemoryWithIndex(0);
  if (_audioMemoryMap == NULL) {
    WIISYSLOG("Failed to map audio memory");
    return false;
  }
  _audioBaseAddr = (volatile void *)_audioMemoryMap->getVirtualAddress();
  WIIDBGLOG("Mapped AI registers to %p (physical 0x%X), length: 0x%X", _audioBaseAddr,
    _audioMemoryMap->getPhysicalAddress(), _audioMemoryMap->getLength());

  //
  // Get DSP and map memory.
  //
  _dspDevice = waitForService(nameMatching("NTDOY,dsp"));
  if (_dspDevice == NULL) {
    return false;
  }
  _dspDevice->retain();

  _dspMemoryMap = _dspDevice->mapDeviceMemoryWithIndex(0);
  if (_dspMemoryMap == NULL) {
    WIISYSLOG("Failed to map DSP memory");
    return false;
  }
  _dspBaseAddr = (volatile void *)_dspMemoryMap->getVirtualAddress();
  WIIDBGLOG("Mapped DSP registers to %p (physical 0x%X), length: 0x%X", _dspBaseAddr,
    _dspMemoryMap->getPhysicalAddress(), _dspMemoryMap->getLength());

  //
  // Allocate output buffer.
  //
  if (_isCafe) {
    _outputBuffer = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous, kWiiAudioBufferSize * 2, PAGE_SIZE);
  } else {
    _outputBuffer = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous, kWiiAudioBufferSize, PAGE_SIZE);
  }
  if (_outputBuffer == NULL) {
    WIISYSLOG("Failed to allocate output buffer");
    return false;
  }

  _outputBufferPtr = _outputBuffer->getBytesNoCopy();
  if (_isCafe) {
    _outputBufferLattePtr = ((UInt8*) _outputBufferPtr) + kWiiAudioBufferSize;
  }
  status = IOSetProcessorCacheMode(kernel_task, (IOVirtualAddress) _outputBufferPtr, _outputBuffer->getLength(), kIOInhibitCache);
  if (status != kIOReturnSuccess) {
    WIISYSLOG("Failed to change mapping of output buffer");
    return false;
  }

  //
  // Create interrupt.
  //
  _interruptEventSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
    OSMemberFunctionCast(IOFilterInterruptEventSource::Action, this, &WiiAudioDriver::handleInterrupt),
    OSMemberFunctionCast(IOFilterInterruptEventSource::Filter, this, &WiiAudioDriver::filterInterrupt),
    _dspDevice, 0);
  if (_interruptEventSource == NULL) {
    WIISYSLOG("Failed to create interrupt");
    return kIOReturnNoResources;
  }
  workLoop->addEventSource(_interruptEventSource);

  //
  // Reset DSP and load buffers.
  //
  dspReset();

  outputPhysAddr = _outputBuffer->getPhysicalSegment(0, &length);
  dspLoadSample(outputPhysAddr, kWiiAudioBufferSize, false);

  if (_isCafe) {
    outputPhysAddr = _outputBuffer->getPhysicalSegment(kWiiAudioBufferSize, &length);
    dspLoadSample(outputPhysAddr, kWiiAudioBufferSize, true);
  }

  //
  // Create audio engines for outputs.
  //
  _audioOutputEngine = createAudioEngine(_outputBufferPtr, kWiiAudioBufferSize, _isCafe ? "Wii U GamePad" : "Wii A/V");
  if (_audioOutputEngine == NULL) {
    WIISYSLOG("Failed to create audio engine");
    return false;
  }

  if (_isCafe) {
    _audioOutputLatteEngine = createAudioEngine(_outputBufferLattePtr, kWiiAudioBufferSize, "Wii U A/V");
    if (_audioOutputLatteEngine == NULL) {
      WIISYSLOG("Failed to create Latte audio engine");
      return false;
    }
  }

  //
  // Enable interrupts.
  //
  dspControl = readDspReg16(kWiiAudioDspRegControlStatus) | kWiiAudioDspRegControlStatusDspIntEnable | kWiiAudioDspRegControlStatusAudioIntEnable;
  if (_isCafe) {
    dspControl |= kWiiAudioDspRegControlStatusLatteAudioIntEnable;
  }
  writeDspReg16(kWiiAudioDspRegControlStatus, dspControl);
  _interruptEventSource->enable();

  //
  // Activate the engines.
  //
  activateAudioEngine(_audioOutputEngine);
  if (_isCafe) {
    activateAudioEngine(_audioOutputLatteEngine);
  }

  WIIDBGLOG("Initialized Wii audio");
  return true;
}

//
// Starts audio playback on the specified engine.
//
IOReturn WiiAudioDriver::startAudioDsp(WiiAudioEngine *audioEngine) {
  if (audioEngine == NULL) {
    return kIOReturnUnsupported;
  }

  if (audioEngine == _audioOutputEngine) {
    dspStartSample(false);
    return kIOReturnSuccess;
  } else if (audioEngine == _audioOutputLatteEngine) {
    dspStartSample(true);
    return kIOReturnSuccess;
  }

  return kIOReturnUnsupported;
}

//
// Stops audio playback on the specified engine.
//
IOReturn WiiAudioDriver::stopAudioDsp(WiiAudioEngine *audioEngine) {
  if (audioEngine == NULL) {
    return kIOReturnUnsupported;
  }

  if (audioEngine == _audioOutputEngine) {
    dspStopSample(false);
    return kIOReturnSuccess;
  } else if (audioEngine == _audioOutputLatteEngine) {
    dspStopSample(true);
    return kIOReturnSuccess;
  }

  return kIOReturnUnsupported;
}

//
// Gets the bytes left on the specified engine.
//
UInt32 WiiAudioDriver::getAudioDspBytesLeft(WiiAudioEngine *audioEngine) {
  if (audioEngine == NULL) {
    return 0;
  }

  if (audioEngine == _audioOutputEngine) {
    return dspGetBytesLeft(false);
  } else if (audioEngine == _audioOutputLatteEngine) {
    return dspGetBytesLeft(true);
  }

  return 0;
}

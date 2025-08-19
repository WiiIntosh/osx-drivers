//
//  WiiAudioDriver.cpp
//  Wii audio driver
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <IOKit/audio/IOAudioDefines.h>

#include "WiiAudioDriver.hpp"
#include "WiiAudioEngine.hpp"
#include "AudioRegs.hpp"

OSDefineMetaClassAndStructors(WiiAudioDriver, super);

#define kWiiAudioBufferSize     0x10000

extern "C" vm_offset_t ml_io_map(vm_offset_t phys_addr, vm_size_t size);

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
  IOByteCount         length;
  UInt16              dspControl;
  IOReturn            status;

  WIIDBGLOG("Initializing Wii audio");

  if (!super::initHardware(provider)) {
    WIISYSLOG("super::initHardware() returned false");
    return false;
  }

  setDeviceName ("Built-in Audio");
  setDeviceShortName ("Built-in");
  setManufacturerName("Nintendo");
  setProperty(kIOAudioDeviceTransportTypeKey, kIOAudioDeviceTransportTypeBuiltIn, 32);

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
  _isCafe = checkPlatformCafe();
  if (_isCafe) {
    _outputBufferDesc = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous, kWiiAudioBufferSize * 2, PAGE_SIZE);
  } else {
    _outputBufferDesc = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous, kWiiAudioBufferSize, PAGE_SIZE);
  }
  if (_outputBufferDesc == NULL) {
    WIISYSLOG("Failed to allocate output buffer");
    return false;
  }

  _outputBufferPhysAddr = _outputBufferDesc->getPhysicalSegment(0, &length);
  if (_isCafe) {
    _outputBufferLattePhysAddr = _outputBufferDesc->getPhysicalSegment(kWiiAudioBufferSize, &length);
  }
  
  //
  // Map as I/O. TODO: Is there a better way to do this? Attempting to use IOSetProcessorCache like others doesn't seem to work.
  //
  _outputBuffer = (void*)ml_io_map(_outputBufferPhysAddr, kWiiAudioBufferSize);
  if (_isCafe) {
    _outputBufferLatte = (void*)ml_io_map(_outputBufferLattePhysAddr, kWiiAudioBufferSize);
  }

  //
  // Reset DSP and load buffers.
  //
  dspReset();
  writeAudioReg32(kWiiAudioIntRegControl, readAudioReg32(kWiiAudioIntRegControl) & ~(kWiiAudioIntRegControlDspFreq32kHz));  
  dspLoadSample(_outputBufferPhysAddr, kWiiAudioBufferSize, false);
  if (_isCafe) {
    dspLoadSample(_outputBufferLattePhysAddr, kWiiAudioBufferSize, true);
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
  // Create audio engines for outputs.
  //
  _audioOutputEngine = createAudioEngine(_outputBuffer, kWiiAudioBufferSize, _isCafe ? "Wii U GamePad" : "Wii A/V",
    OSMemberFunctionCast(IOAudioControl::IntValueChangeHandler, this, &WiiAudioDriver::handleControlChange));
  if (_audioOutputEngine == NULL) {
    WIISYSLOG("Failed to create audio engine");
    return false;
  }
  status = createAudioPorts(_audioOutputEngine,
    _isCafe ? kIOAudioOutputPortSubTypeInternalSpeaker : kIOAudioOutputPortSubTypeExternalSpeaker,
    _isCafe ? "Wii U GamePad" : "Wii A/V");
  if (status != kIOReturnSuccess) {
    WIISYSLOG("Failed to create audio ports");
    return false;
  }

  if (_isCafe) {
    _audioOutputLatteEngine = createAudioEngine(_outputBufferLatte, kWiiAudioBufferSize, "Wii U A/V",
      OSMemberFunctionCast(IOAudioControl::IntValueChangeHandler, this, &WiiAudioDriver::handleLatteControlChange));
    if (_audioOutputLatteEngine == NULL) {
      WIISYSLOG("Failed to create Latte audio engine");
      return false;
    }
    status = createAudioPorts(_audioOutputLatteEngine, kIOAudioOutputPortSubTypeExternalSpeaker, "Wii U A/V");
    if (status != kIOReturnSuccess) {
      WIISYSLOG("Failed to create Latte audio ports");
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

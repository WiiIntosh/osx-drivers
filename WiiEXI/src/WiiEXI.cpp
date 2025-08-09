//
//  WiiEXI.cpp
//  Wii EXI controller interface
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include "WiiEXI.hpp"

OSDefineMetaClassAndStructors(WiiEXI, super);

static int Wii_PE_read_write_time_of_day(unsigned int options, long *secs) {
  WiiEXI *exiService;

  //
  // Get EXI service.
  //
  exiService = OSDynamicCast(WiiEXI, IOService::waitForService(IOService::nameMatching("WiiEXI")));
  if (exiService == NULL) {
    return 1;
  }

  switch(options) {
	  case kPEReadTOD:  
      *secs = exiService->getRTC();
      return 0;
  }

  return 1;
}

//
// Overrides IOService::init().
//
bool WiiEXI::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _debugEnabled = true;

  _memoryMap              = NULL;
  _baseAddr               = NULL;

  _workLoop               = NULL;
  _commandGate            = NULL;
  _interruptEventSource   = NULL;

  return super::init(dictionary);
}

//
// Overrides IOService::start().
//
bool WiiEXI::start(IOService *provider) {
  IOReturn status;

  if (!super::start(provider)) {
    WIISYSLOG("super::start() returned false");
    return false;
  }

  WIIDBGLOG("Got here");
  IOSleep(500);

  //
  // Get platform type.
  //
  status = ((IOService*) getPlatform())->callPlatformFunction(kWiiFuncPlatformIsCafe, true, (void*) &_isCafe, NULL, NULL, NULL);
  if (status != kIOReturnSuccess) {
    WIISYSLOG("Failed to get platform type");
    return false;
  }

  WIIDBGLOG("and here");
  IOSleep(500);

  //
  // Map controller memory.
  //
  _memoryMap = provider->mapDeviceMemoryWithIndex(0);
  if (_memoryMap == NULL) {
    WIISYSLOG("Failed to map EXI memory");
    return false;
  }
  _baseAddr = (volatile void *)_memoryMap->getVirtualAddress();
  WIIDBGLOG("Mapped registers to %p (physical 0x%X), length: 0x%X", _baseAddr,
    _memoryMap->getPhysicalAddress(), _memoryMap->getLength());

  //
  // Reset EXI.
  //
  for (UInt32 i = 0; i < kWiiEXIMaxChannels; i++) {
    writeReg32(i, kWiiEXIRegParam, 0);
    writeReg32(i, kWiiEXIRegParam, kWiiEXIRegParamIntStatus | kWiiEXIRegParamTCIntStatus | kWiiEXIRegParamExtIntStatus);
  }

  //
  // Initialize work loop and command pool.
  //
  _workLoop = IOWorkLoop::workLoop();
  if (_workLoop == NULL) {
    WIISYSLOG("Failed to create work loop");
    return false;
  }
  
  //
  // Allocate command gate and interrupt.
  //
  _commandGate = IOCommandGate::commandGate(this);
  if (_commandGate == NULL) {
    WIISYSLOG("Failed to create command gate");
    return false;
  }
  _workLoop->addEventSource(_commandGate);
  _commandGate->enable();

  _interruptEventSource = IOInterruptEventSource::interruptEventSource(this,
    OSMemberFunctionCast(IOInterruptEventSource::Action, this, &WiiEXI::handleInterrupt),
    provider, 0);
  if (_interruptEventSource == NULL) {
    WIISYSLOG("Failed to create interrupt");
    return false;
  }
  _workLoop->addEventSource(_interruptEventSource);
  _interruptEventSource->enable();

  //
  // Get the RTC bias.
  //
  if (_isCafe) {
    status = waitForService(nameMatching("WiiCafeIPC"))->callPlatformFunction(kWiiFuncIPCGetRTCBias, true, (void*) &_rtcBias, NULL, NULL, NULL);
    if (status != kIOReturnSuccess) {
      WIISYSLOG("Failed to read RTC bias from IPC");
      return false;
    }
  }

  WIIDBGLOG("then here");
  IOSleep(500);
  
  registerService();

  //
  // Register ourselves as the RTC.
  //
  PE_read_write_time_of_day = Wii_PE_read_write_time_of_day;
  publishResource("IORTC", this);

  WIIDBGLOG("Initialized EXI");
  return true;
}

//
// Gets the current RTC time.
//
UInt32 WiiEXI::getRTC(void) {
  UInt32 data;

  if (_commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &WiiEXI::getRTCGated), &data) != kIOReturnSuccess) {
    return 0;
  }
  return data;
}

//
//  WiiCafeIPC.cpp
//  Wii U IPC service
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <IOKit/IOPlatformExpert.h>
#include "WiiCafeIPC.hpp"
#include "WiiLatte.hpp"

OSDefineMetaClassAndStructors(WiiCafeIPC, super);

static WiiCafeIPC *gCafeIPC;

#define CMD_POWEROFF 0xCAFE0001
#define CMD_REBOOT   0xCAFE0002

static int handleCafePEHaltRestart(unsigned int type) {
  return gCafeIPC->doHaltRestart(type);
}

//
// Overrides IOService::init().
//
bool WiiCafeIPC::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _memoryMap    = NULL;
  _baseAddr     = NULL;

  return super::init(dictionary);
}

//
// Overrides IOService::start().
//
bool WiiCafeIPC::start(IOService *provider) {
  if (!super::start(provider)) {
    WIISYSLOG("super::start() returned false");
    return false;
  }

  gCafeIPC = this;

  //
  // Map IPC register memory.
  //
  _memoryMap = provider->mapDeviceMemoryWithIndex(0);
  if (_memoryMap == NULL) {
    WIISYSLOG("Failed to map IPC memory");
    return false;
  }
  _baseAddr = (volatile void *)_memoryMap->getVirtualAddress();
  WIIDBGLOG("Mapped registers to %p (physical 0x%X), length: 0x%X", _baseAddr,
    _memoryMap->getPhysicalAddress(), _memoryMap->getLength());

  //
  // Register to handle halts and restarts.
  //
  PE_halt_restart = handleCafePEHaltRestart;

  return true;
}

//
// Shuts down or restarts the system via IPC.
//
int WiiCafeIPC::doHaltRestart(unsigned int type) {
  WIISYSLOG("Type: %u", type);

  switch (type) {
    case kPERestartCPU:
      writeReg32(kWiiLatteIPCPPCMSG, CMD_REBOOT);
      writeReg32(kWiiLatteIPCPPCCTRL, 0x1);
      return 0;

    case kPEHaltCPU:
      writeReg32(kWiiLatteIPCPPCMSG, CMD_POWEROFF);
      writeReg32(kWiiLatteIPCPPCCTRL, 0x1);
      return 0;

    default:
      return -1;
  }
}

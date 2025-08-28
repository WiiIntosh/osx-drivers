//
//  WiiIPC.cpp
//  Wii IPC services
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <IOKit/IOPlatformExpert.h>
#include "IPCRegs.hpp"
#include "WiiIPC.hpp"

OSDefineMetaClassAndStructors(WiiIPC, super);

static WiiIPC *gWiiIPC;

// Power off system.
#define CMD_POWEROFF  0xCAFE0001
// Reboot.
#define CMD_REBOOT    0xCAFE0002
// Get RTC bias (Cafe only).
#define CMD_RTC_BIAS  0xCAFE0003

static int handleWiiPEHaltRestart(unsigned int type) {
  return gWiiIPC->doHaltRestart(type);
}

//
// Overrides IOService::init().
//
bool WiiIPC::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _memoryMap    = NULL;
  _baseAddr     = NULL;

  return super::init(dictionary);
}

//
// Overrides IOService::start().
//
bool WiiIPC::start(IOService *provider) {
  if (!super::start(provider)) {
    WIISYSLOG("super::start() returned false");
    return false;
  }

  gWiiIPC = this;

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
  PE_halt_restart = handleWiiPEHaltRestart;

  registerService();
  return true;
}

//
// Overrides IOService::callPlatformFunction().
//
IOReturn WiiIPC::callPlatformFunction(const OSSymbol *functionName, bool waitForFunction,
                                          void *param1, void *param2, void *param3, void *param4) {
  if (functionName->isEqualTo(kWiiFuncIPCGetRTCBias)) {
    writeReg32(kWiiIPCPPCMSG, CMD_RTC_BIAS);
    writeReg32(kWiiIPCPPCCTRL, 0x1);
    while (readReg32(kWiiIPCPPCCTRL) & 0x1);
    *((UInt32*) param1) = readReg32(kWiiIPCARMMSG);
    return kIOReturnSuccess;
  }

  return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

//
// Shuts down or restarts the system via IPC.
//
int WiiIPC::doHaltRestart(unsigned int type) {
  WIIDBGLOG("Halt type: %u", type);

  switch (type) {
    case kPERestartCPU:
      writeReg32(kWiiIPCPPCMSG, CMD_REBOOT);
      writeReg32(kWiiIPCPPCCTRL, 0x1);
      return 0;

    case kPEHaltCPU:
      writeReg32(kWiiIPCPPCMSG, CMD_POWEROFF);
      writeReg32(kWiiIPCPPCCTRL, 0x1);
      return 0;

    default:
      return -1;
  }
}

//
//  WiiPE.cpp
//  Wii platform expert
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <ppc/proc_reg.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/platform/ApplePlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>

#include "WiiPE.hpp"

OSDefineMetaClassAndStructors(WiiPE, super);

//
// Address of _invalidate_dcache. This function is not exported on 10.4
//
void (*WiiInvalidateDataCacheFunc)(vm_offset_t, unsigned, int) = 0;

#define ESPRESSO_PVR_HIGH       0x70010000

//
// Overrides IODTPlatformExpert::init().
//
bool WiiPE::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _isCafe         = false;
  _mem2Allocator  = NULL;

  return super::init(dictionary);
}

#define kMolStdMachineType	1
#define kChipSetTypeMol		170

//
// Overrides IODTPlatformExpert::start()
//
bool WiiPE::start(IOService *provider) {
  PE_Video  videoInfo;
  UInt32    cpuPvr;
  OSData    *mem2BufferStart;
  OSData    *mem2BufferSize;
  bool      ints;

  setChipSetType(kChipSetTypeMol);
  setMachineType(kMolStdMachineType);
  setBootROMType(kBootROMTypeNewWorld);

  _pePMFeatures     = kStdDesktopPMFeatures;
  _pePrivPMFeatures = kStdDesktopPrivPMFeatures;      
  _peNumBatteriesSupported = kStdDesktopNumBatteries;

  //
  // Get the kernel header and resolve required non-exported functions.
  //
  if (!findKernelMachHeader()) {
    return false;
  }

  WiiInvalidateDataCacheFunc = (void (*)(vm_offset_t, unsigned, int)) resolveKernelSymbol("_invalidate_dcache");
  if (WiiInvalidateDataCacheFunc == 0) {
    return false;
  }

  //
  // Get the PowerPC PVR.
  //
  asm volatile ("mfpvr %0" : "=r"(cpuPvr));
  WIIDBGLOG("PowerPC PVR: 0x%X", cpuPvr);
  _isCafe = (cpuPvr & 0xFFFF0000) == ESPRESSO_PVR_HIGH;

  //
  // Create MEM2 allocator if on Wii.
  //
  if (!_isCafe) {
    mem2BufferStart = OSDynamicCast(OSData, provider->getProperty("wii-mem2-start"));
    mem2BufferSize  = OSDynamicCast(OSData, provider->getProperty("wii-mem2-size"));

    if ((mem2BufferStart == NULL) || (mem2BufferSize == NULL)) {
      WIISYSLOG("MEM2 buffer properties are missing");
      return false;
    }

    _mem2Allocator = IORangeAllocator::withRange(0, 0, 0, IORangeAllocator::kLocking);
    if (_mem2Allocator == NULL) {
      WIISYSLOG("Failed to create MEM2 allocator");
      return false;
    }
    _mem2Allocator->deallocate(*((UInt32*) mem2BufferStart->getBytesNoCopy()), *((UInt32*) mem2BufferSize->getBytesNoCopy()));
  }

  if (!super::start(provider)) {
    WIISYSLOG("super::start() returned false");
    return false;
  }

  //
  // TODO: Need to implement IONVRAM resources, otherwise XNU waits for 30s for these in IOKitResetTime().
  //
  publishResource("IONVRAM");

  //
  // Prevent sleep/doze, Wii hardware is incapable of sleeping but unsure of doze. Seems to cause issues on Wii U and the GPU.
  //
  //getPMRootDomain()->receivePowerNotification(kIOPMPreventSleep);

  WIIDBGLOG("Initialized Wii platform expert");
  return true;
}

//
// Overrides IODTPlatformExpert::callPlatformFunction().
//
IOReturn WiiPE::callPlatformFunction(const OSSymbol *functionName, bool waitForFunction,
                                void *param1, void *param2, void *param3, void *param4) {
  if (functionName->isEqualTo(kWiiFuncPlatformIsCafe)) {
    *((bool*) param1) = _isCafe;
    return kIOReturnSuccess;
  }

  return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

//
// Overrides IODTPlatformExpert::deleteList()
//
const char *WiiPE::deleteList(void) {
  return("('packages', 'psuedo-usb', 'psuedo-hid', 'multiboot', 'rtas')");
}

//
// Overrides IODTPlatformExpert::excludeList()
//
const char *WiiPE::excludeList(void) {
  //
  // List of DT nodes to exclude from enumeration.
  //
  return("('chosen', 'memory', 'openprom', 'AAPL,ROM', 'rom', 'options', 'aliases')");
}

//
// Overrides IODTPlatformExpert::getMachineName()
//
bool WiiPE::getMachineName(char *name, int maxLength) {
  strncpy(name, "Power Macintosh", maxLength);
  return true;
}

//
// Overrides IODTPlatformExpert::getGMTTimeOfDay().
//
long WiiPE::getGMTTimeOfDay(void) {
  mach_timespec_t t;
  long            secs;

  //
  // Wait 30 seconds for RTC to appear.
  // TODO: RTC stores time as local time, XNU expects GMT.
  // Need to store/whatever TZ offset to emulated NVRAM when implemented.
  //
  t.tv_sec = 30;
  t.tv_nsec = 0;
  if (waitForService(resourceMatching("IORTC"), &t) != NULL) {
    if (PE_read_write_time_of_day(kPEReadTOD, &secs) == 0) {
      return secs;
    }
  } else {
    WIISYSLOG("RTC did not show up");
  }

  return 0;
}

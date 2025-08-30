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
// Overrides IODTPlatformExpert::init().
//
bool WiiPE::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _invalidateCacheFunc = NULL;
  _mem2Allocator       = NULL;

  return super::init(dictionary);
}

#define kMolStdMachineType	1
#define kChipSetTypeMol		170

//
// Overrides IODTPlatformExpert::start().
//
bool WiiPE::start(IOService *provider) {
  bool    isCafe;
  OSData  *mem2Data;
  UInt32  *mem2Addr;

  setChipSetType(kChipSetTypeMol);
  setMachineType(kMolStdMachineType);
  setBootROMType(kBootROMTypeNewWorld);

  _pePMFeatures     = kStdDesktopPMFeatures;
  _pePrivPMFeatures = kStdDesktopPrivPMFeatures;
  _peNumBatteriesSupported = kStdDesktopNumBatteries;

  isCafe = checkPlatformCafe();
  WIIDBGLOG("Initializing %s platform expert", isCafe ? "Wii U" : "Wii");
  WIIDBGLOG("PowerPC PVR: 0x%X", getProcessorPVR());

  //
  // Get the kernel header and resolve required non-exported functions.
  //
  if (!findKernelMachHeader()) {
    return false;
  }

  _invalidateCacheFunc = (void (*)(vm_offset_t, unsigned, int)) resolveKernelSymbol("_invalidate_dcache");
  if (_invalidateCacheFunc == NULL) {
    return false;
  }

  //
  // Create MEM2 allocator if on Wii.
  //
  if (!isCafe) {
    mem2Data = OSDynamicCast(OSData, provider->getProperty("mem2-addresses"));
    if ((mem2Data == NULL) || (mem2Data->getLength() < (sizeof (UInt32) * 2))) {
      WIISYSLOG("MEM2 addresses are missing or invalid");
      return false;
    }

    mem2Addr = (UInt32 *) mem2Data->getBytesNoCopy();
    WIIDBGLOG("MEM2 buffer: 0x%X, length: 0x%X", mem2Addr[0], mem2Addr[1]);

    _mem2Allocator = IORangeAllocator::withRange(0, 0, 0, IORangeAllocator::kLocking);
    if (_mem2Allocator == NULL) {
      WIISYSLOG("Failed to create MEM2 allocator");
      return false;
    }
    _mem2Allocator->deallocate(mem2Addr[0], mem2Addr[1]);
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
  getPMRootDomain()->receivePowerNotification(kIOPMPreventSleep);

  WIIDBGLOG("Initialized platform expert");
  return true;
}

//
// Overrides IODTPlatformExpert::callPlatformFunction().
//
IOReturn WiiPE::callPlatformFunction(const OSSymbol *functionName, bool waitForFunction,
                                void *param1, void *param2, void *param3, void *param4) {
  //
  // Get cache invalidation function.
  //
  if (functionName->isEqualTo(kWiiFuncPlatformGetInvalidateCache)) {
    *((WiiInvalidateDataCacheFunc*) param1) = _invalidateCacheFunc;
    return kIOReturnSuccess;
  }

  //
  // Get MEM2 allocator.
  //
  if (functionName->isEqualTo(kWiiFuncPlatformGetMem2Allocator)) {
    WIIDBGLOG("Called %s", kWiiFuncPlatformGetMem2Allocator);
    if (_mem2Allocator == NULL) {
      return kIOReturnUnsupported;
    }

    *((IORangeAllocator**) param1) = _mem2Allocator;
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

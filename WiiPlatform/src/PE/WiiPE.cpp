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
// Private function, but exported in all versions.
// Used for remapping console on 10.2 and older.
//
extern "C" vm_offset_t ml_io_map(vm_offset_t phys_addr, vm_size_t size);

#define mtdbatu(n, reg) asm volatile("mtdbatu " # n ", %0" : : "r" (reg))
#define mtdbatl(n, reg) asm volatile("mtdbatl " # n ", %0" : : "r" (reg))

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
  // Remap the framebuffer to be in noncached I/O memory if on 10.2 and older.
  // 10.3 and newer always maps into noncached memory already.
  //
  /*if (getKernelVersion() < kKernelVersionPanther) {
    getConsoleInfo(&videoInfo);
    videoInfo.v_baseAddr = ml_io_map(videoInfo.v_baseAddr, videoInfo.v_height * videoInfo.v_rowBytes);
    setConsoleInfo(&videoInfo, kPEEnableScreen);

    WIISYSLOG("\033[31mC\033[32mO\033[33mL\033[34mO\033[35mR\033[0m console remapped to 0x%X", videoInfo.v_baseAddr);

    //
    // Ensure DBAT2 and DBAT3 are clear.
    //
    ints = ml_set_interrupts_enabled(false);
    sync();
    isync();

    mtdbatl(2, BAT_INVALID);
    mtdbatu(2, BAT_INVALID);
    mtdbatl(3, BAT_INVALID);
    mtdbatu(3, BAT_INVALID);

    sync();
    isync();
    ml_set_interrupts_enabled(ints);
  }*/

  //
  // Get the kernel header and resolve required non-exported functions.
  //
  if (!findKernelMachHeader()) {
    return false;
  }

  _pmapFindPhysEntryFuncAddr = resolveKernelSymbol("_pmap_find_physentry");
  if (_pmapFindPhysEntryFuncAddr == 0) {
    return false;
  }

  if (getKernelVersion() == kKernelVersionJaguar) {
    _mappingPhysInitFuncAddr = resolveKernelSymbol("_mapping_phys_init");
    if (_mappingPhysInitFuncAddr == 0) {
      return false;
    }
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
  if (!isPlatformCafe()) {
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
  // TODO: Need to implement IORTC and IONVRAM resources, otherwise XNU waits for 30s each for these in IOKitResetTime().
  //
  publishResource("IORTC");
  publishResource("IONVRAM");

  //
  // Prevent sleep/doze, Wii hardware is incapable of sleeping but unsure of doze. Seems to cause issues on Wii U and the GPU.
  //
  //getPMRootDomain()->receivePowerNotification(kIOPMPreventSleep);

  WIIDBGLOG("Initialized Wii platform expert");
  return true;
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
  // TODO
  return 1743947017;
}

//
// Checks if the platform is Wii U (CAFE).
//
bool WiiPE::isPlatformCafe(void) {
  return _isCafe;
}

//
// Gets the MEM2 range allocator.
//
IORangeAllocator *WiiPE::getMem2Allocator(void) {
  return _mem2Allocator;
}

//
//  WiiOHCI.cpp
//  Wii OHCI USB controller interface
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include "WiiOHCI.hpp"

OSDefineMetaClassAndStructors(WiiOHCI, super);

//
// Overrides IOUSBController::init().
//
bool WiiOHCI::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _memoryMap              = NULL;
  _mem2Allocator          = NULL;
  _baseAddr               = NULL;
  _interruptEventSource   = NULL;

  _endpointBufferHeadPtr  = NULL;
  _freeEndpointHeadPtr    = NULL;

  _genTransferBufferHeadPtr   = NULL;
  _freeGenTransferHeadPtr     = NULL;
  _freeGenTransferMem2HeadPtr = NULL;

  _rootHubInterruptTransLock = IOLockAlloc();
  if (_rootHubInterruptTransLock == NULL) {
    return false;
  }
  bzero(_rootHubInterruptTransactions, sizeof (_rootHubInterruptTransactions));

  return super::init(dictionary);
}

//
// Overrides IOUSBController::UIMInitialize().
//
// Initializes the USB controller.
// Called from IOUSBController::start().
//
IOReturn WiiOHCI::UIMInitialize(IOService *provider) {
  UInt8     ohciRevision;
  UInt32    ohciControl;
  UInt32    ohciFrameInterval;
  UInt32    ohciFrameLargestPacket;
  UInt32    ohciRemoteWakeup;
 // WiiPE     *wiiPE;
  IOReturn  status;

  //wiiPE = OSDynamicCast(WiiPE, getPlatform());
 // if (wiiPE == NULL) {
 //   return kIOReturnUnsupported;
 // }

  //
  // TODO: Disable EHCI routing and reset controller to give OHCI exclusive access using EHCI gets implemented.
  //
  IODeviceMemory *ehciMem = IODeviceMemory::withRange(0x0D040000, 0x100);
  IOMemoryMap *ehciMap = ehciMem->map();
  volatile void *ehciBase = (volatile void*) ehciMap->getVirtualAddress();
  OSWriteBigInt32(ehciBase, 0x00, 0x2);
  while (OSReadBigInt32(ehciBase, 0x00) & 0x2) {
    IODelay(1);
  }
  WIIDBGLOG("EHCI is now reset.");
  OSWriteBigInt32(ehciBase, 0x50, 0); // Disable routing to EHCI.

  //
  // Map controller memory.
  //
  _memoryMap = provider->mapDeviceMemoryWithIndex(0);
  if (_memoryMap == NULL) {
    WIISYSLOG("Failed to map OHCI memory");
    return kIOReturnNoResources;
  }
  _baseAddr = (volatile void *)_memoryMap->getVirtualAddress();
  WIIDBGLOG("Mapped registers to %p (physical 0x%X), length: 0x%X", _baseAddr,
    _memoryMap->getPhysicalAddress(), _memoryMap->getLength());

  //
  // Create MEM2 allocator if on Wii.
  // Transfer descriptor buffers must be in MEM2, others are optional but we'll put everything there to take advantage of the space.
  //
  /*if (!wiiPE->isPlatformCafe()) {
    _mem2Allocator = wiiPE->getMem2Allocator();
    if (_mem2Allocator == NULL) {
      WIISYSLOG("Failed to get MEM2 buffer memory");
      return kIOReturnNoResources;
    }
    _mem2Allocator->retain();
  }*/

  //
  // Check revision.
  //
  ohciRevision = readReg32(kOHCIRegRevision) & kOHCIRegRevisionMask;
  WIIDBGLOG("OHCI revision: 0x%X", ohciRevision);
  if (ohciRevision != kOHCIRevision10) {
    return kIOReturnUnsupported;
  }

  //
  // Create interrupt.
  //
  _interruptEventSource = IOInterruptEventSource::interruptEventSource(this,
    OSMemberFunctionCast(IOInterruptEventSource::Action, this, &WiiOHCI::handleInterrupt),
    provider, 0);
  if (_interruptEventSource == NULL) {
    WIISYSLOG("Failed to create interrupt");
    return kIOReturnNoResources;
  }
  _workLoop->addEventSource(_interruptEventSource);

  //
  // Create memory cursor.
  //
  _memoryCursor = IONaturalMemoryCursor::withSpecification(PAGE_SIZE, PAGE_SIZE);
  if (_memoryCursor == NULL) {
    WIISYSLOG("Failed to create memory cursor");
    return kIOReturnNoResources;
  }

  //
  // Save registers prior to reset.
  //
  ohciControl       = readReg32(kOHCIRegControl);
  ohciFrameInterval = readReg32(kOHCIRegFrameInterval);
  ohciRemoteWakeup  = ohciControl & kOHCIRegControlRemoteWakeupConnected;

  //
  // Allocate the HCCA.
  //
  if (_mem2Allocator != NULL) {
    // Wii TODO.
    

  //
  // Wii U can have HCCA located anywhere.
  //
  } else {
    //
    // Wii platforms are not cache coherent, host controller structures must be non-cacheable.
    // Need to allocate an entire page to ensure nothing else will occupy this cache-inhibited area.
    //
    _hccaPtr = (OHCIHostControllerCommArea*) IOMallocContiguous(PAGE_SIZE, PAGE_SIZE, &_hccaPhysAddr);
    if (_hccaPtr == NULL) {
      return kIOReturnNoMemory;
    }
    status = IOSetProcessorCacheMode(kernel_task, (IOVirtualAddress) _hccaPtr, PAGE_SIZE, kIOInhibitCache);
    if (status != kIOReturnSuccess) {
      return status;
    }
  }
  bzero(_hccaPtr, sizeof (*_hccaPtr));

  //
  // Software reset controller.
  // This will put root hub into suspend state.
  //
  writeReg32(kOHCIRegCmdStatus, kOHCIRegCmdStatusHostControllerReset);
  while (readReg32(kOHCIRegCmdStatus) & kOHCIRegCmdStatusHostControllerReset) {
    IODelay(1);
  }

  //
  // Reconfigure controller timings.
  //
  ohciFrameLargestPacket = ((((ohciFrameInterval - 210) * 6) / 7) << kOHCIRegFrameIntervalFSLargestDataPktShift) & kOHCIRegFrameIntervalFSLargestDataPktMask;
  writeReg32(kOHCIRegFrameInterval, ohciFrameInterval | ohciFrameLargestPacket);
  writeReg32(kOHCIRegPeriodicStart, ((ohciFrameInterval * 9) / 10) &kOHCIRegPeriodicStartMask);

  //
  // Set location of HCCA.
  //
  writeReg32(kOHCIRegHCCA, _hccaPhysAddr);

  //
  // Setup endpoint lists.
  //
  status = initControlEndpoints();
  if (status != kIOReturnSuccess) {
    WIISYSLOG("Failed to configure control endpoints");
    return status;
  }

  status = initBulkEndpoints();
  if (status != kIOReturnSuccess) {
    WIISYSLOG("Failed to configure bulk endpoints");
    return status;
  }

  status = initInterruptEndpoints();
  if (status != kIOReturnSuccess) {
    WIISYSLOG("Failed to configure interrupt endpoints");
    return status;
  }

  //
  // Disable all interrupts.
  //
  writeReg32(kOHCIRegIntDisable, -1);
  writeReg32(kOHCIRegIntStatus, -1);

  //
  // Put root hub into reset state.
  //
  writeReg32(kOHCIRegControl, kOHCIRegControlFuncStateReset | ohciRemoteWakeup);
  IOSleep(100);

  //
  // Enable root hub.
  //
  ohciControl = readReg32(kOHCIRegControl) & ~(kOHCIRegControlFuncStateMask);
  ohciControl |= kOHCIRegControlFuncStateOperational | kOHCIRegControlCBSRMask | ohciRemoteWakeup;
  ohciControl |= kOHCIRegControlPeriodicListEnable | kOHCIRegControlControlListEnable | kOHCIRegControlBulkListEnable;
  writeReg32(kOHCIRegControl, ohciControl);
  IOSleep(100);

  //
  // Enable power to hub.
  //
  writeReg32(kOHCIRegRhStatus, kOHCIRegRhStatusSetGlobalPower);

  //
  // Root hub starts at 1.
  //
  _rootHubAddress = 1;

  //
  // Enable interrupts.
  //
  _interruptEventSource->enable();
  writeReg32(kOHCIRegIntEnable, kOHCIRegIntEnableMasterInterruptEnable
    | kOHCIRegIntEnableSchedulingOverrun | kOHCIRegIntEnableWritebackDoneHead
    | kOHCIRegIntEnableResumeDetected | kOHCIRegIntEnableUnrecoverableError
    /*| kOHCIRegIntEnableFrameNumberOverflow*/);

  return kIOReturnSuccess;
}

//
// Overrides IOUSBController::UIMFinalize().
//
// Cleans up the USB controller.
// Called from IOUSBController::stop().
//
IOReturn WiiOHCI::UIMFinalize(void) {
  WIIDBGLOG("start");
  return kIOReturnSuccess;
}

UInt32 WiiOHCI::GetBandwidthAvailable(void) {
  WIIDBGLOG("start");
return 0;
}

UInt64 WiiOHCI::GetFrameNumber(void) {
  WIIDBGLOG("start");
return 0;
}

UInt32 WiiOHCI::GetFrameNumber32(void) {
  WIIDBGLOG("start");
  return 0;
}

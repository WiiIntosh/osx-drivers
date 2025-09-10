//
//  WiiOHCI.cpp
//  Wii OHCI USB controller interface
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <IOKit/IOPlatformExpert.h>
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

  _intWriteDoneHead       = false;
  _intResumeDetected      = false;
  _intUnrecoverableError  = false;
  _intRootHubStatus       = false;

  _invalidateCacheFunc    = NULL;

  _endpointBufferHeadPtr  = NULL;
  _freeEndpointHeadPtr    = NULL;

  _freeBounceBufferHeadPtr      = NULL;
  _freeBounceBufferJumboHeadPtr = NULL;
  _transferBufferHeadPtr        = NULL;
  _freeGenTransferHeadPtr       = NULL;
  _freeIsoTransferHeadPtr       = NULL;

  _frameNumber  = 0;

  _writeDoneHeadLock = IOSimpleLockAlloc();
  if (_writeDoneHeadLock == NULL) {
    return false;
  }

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
  const OSSymbol    *functionSymbol;
  OHCIBounceBuffer  *bounceBuffer;

  IOByteCount     length;
  UInt8           ohciRevision;
  UInt32          ohciControl;
  UInt32          ohciFrameInterval;
  UInt32          ohciFrameLargestPacket;
  UInt32          ohciRemoteWakeup;
  IOReturn        status;

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
  // Get cache invalidation function.
  //
  functionSymbol = OSSymbol::withCString(kWiiFuncPlatformGetInvalidateCache);
  if (functionSymbol == NULL) {
    return false;
  }
  status = getPlatform()->callPlatformFunction(functionSymbol, false, &_invalidateCacheFunc, 0, 0, 0);
  functionSymbol->release();
  if (status != kIOReturnSuccess) {
    return false;
  }

  if (_invalidateCacheFunc == NULL) {
    WIISYSLOG("Failed to get cache invalidation function");
    return false;
  }

  //
  // Get MEM2 allocator if on Wii.
  //
  if (!checkPlatformCafe()) {
    functionSymbol = OSSymbol::withCString(kWiiFuncPlatformGetMem2Allocator);
    if (functionSymbol == NULL) {
      return kIOReturnNoResources;
    }
    status = getPlatform()->callPlatformFunction(functionSymbol, false, &_mem2Allocator, 0, 0, 0);
    functionSymbol->release();
    if (status != kIOReturnSuccess) {
      return status;
    }

    if (_mem2Allocator == NULL) {
      WIISYSLOG("Failed to get MEM2 allocator on Wii");
      return kIOReturnUnsupported;
    }
    _mem2Allocator->retain();
  }

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
  _interruptEventSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
    OSMemberFunctionCast(IOFilterInterruptEventSource::Action, this, &WiiOHCI::handleInterrupt),
    OSMemberFunctionCast(IOFilterInterruptEventSource::Filter, this, &WiiOHCI::filterInterrupt),
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
  // Wii platforms are not cache coherent, host controller structures must be non-cacheable.
  //
  if (_mem2Allocator != NULL) {
    //
    // Allocate HCCA from MEM2 region.
    // Wii has issues with device reads/writes that are smaller than 4 bytes to MEM1.
    //
    if (!_mem2Allocator->allocate(sizeof (*_hccaPtr), &_hccaPhysAddr, sizeof (*_hccaPtr))) {
      return kIOReturnNoMemory;
    }
    _hccaDesc = IOMemoryDescriptor::withPhysicalAddress(_hccaPhysAddr, sizeof (*_hccaPtr), kIODirectionInOut);
    if (_hccaDesc == NULL) {
      return kIOReturnNoMemory;
    }
    _hccaMap = _hccaDesc->map(kIOMapInhibitCache);
    if (_hccaMap == NULL) {
      return kIOReturnNoMemory;
    }
    _hccaPtr = (OHCIHostControllerCommArea*) _hccaMap->getVirtualAddress();
  } else {
    //
    // Allocate HCCA from any memory.
    // Need to allocate an entire page to ensure nothing else will occupy this cache-inhibited area.
    //
    _hccaDesc = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous, PAGE_SIZE, PAGE_SIZE);
    if (_hccaDesc == NULL) {
      return kIOReturnNoMemory;
    }
    _hccaPhysAddr = _hccaDesc->getPhysicalSegment(0, &length);

    _hccaPtr = (OHCIHostControllerCommArea*) ((IOBufferMemoryDescriptor*) _hccaDesc)->getBytesNoCopy();
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

  status = initIsoEndpoints();
  if (status != kIOReturnSuccess) {
    WIISYSLOG("Failed to configure isochronous endpoints");
    return status;
  }

  status = initInterruptEndpoints();
  if (status != kIOReturnSuccess) {
    WIISYSLOG("Failed to configure interrupt endpoints");
    return status;
  }

  //
  // Allocate initial bounce buffers.
  //
  for (UInt32 i = 0; i < kWiiOHCIBounceBufferInitialCount; i++) {
    bounceBuffer = allocateBounceBuffer(false);
    if (bounceBuffer == NULL) {
      return kIOReturnNoMemory;
    }
    returnBounceBuffer(bounceBuffer);
  }

  for (UInt32 i = 0; i < kWiiOHCIBounceBufferJumboInitialCount; i++) {
    bounceBuffer = allocateBounceBuffer(true);
    if (bounceBuffer == NULL) {
      return kIOReturnNoMemory;
    }
    returnBounceBuffer(bounceBuffer);
  }

  //
  // Configure isochronous bounce buffer refresh timer.
  // Timer is on its own workloop to avoid any delays.
  //
  _isoTimerWorkLoop = IOWorkLoop::workLoop();
  if (_isoTimerWorkLoop == NULL) {
    return kIOReturnNoMemory;
  }

  _isoTimerEventSource = IOTimerEventSource::timerEventSource(this,
    OSMemberFunctionCast(IOTimerEventSource::Action, this, &WiiOHCI::handleIsoTimer));
  if (_isoTimerEventSource == NULL) {
    return kIOReturnNoMemory;
  }
  _isoTimerWorkLoop->addEventSource(_isoTimerEventSource);
  _isoTimerEventSource->disable();

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
  ohciControl |= kOHCIRegControlPeriodicListEnable | kOHCIRegControlIsochronousEnable | kOHCIRegControlControlListEnable | kOHCIRegControlBulkListEnable;
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
    | kOHCIRegIntEnableFrameNumberOverflow);

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

//
// Overrides IOUSBController::GetBandwidthAvailable().
//
// Gets the number of bytes available per frame for isochronous transfers.
//
UInt32 WiiOHCI::GetBandwidthAvailable(void) {
  return _isoBandwidthAvailable;
}

//
// Overrides IOUSBController::GetFrameNumber().
//
// Gets the current frame number.
//
UInt64 WiiOHCI::GetFrameNumber(void) {
  UInt64 fullFrameNumber;
  UInt16 hcFrameNumber;

  hcFrameNumber = USBToHostWord(_hccaPtr->frameNumber);
  fullFrameNumber = _frameNumber + hcFrameNumber;
  if (hcFrameNumber < 200) {
    if (readReg32(kOHCIRegIntStatus) & kOHCIRegIntStatusFrameNumberOverflow) {
      fullFrameNumber += BIT16;
    }
  }

  return fullFrameNumber;
}

//
// Overrides IOUSBController::GetFrameNumber32().
//
// Gets the least significant 32 bits of the current frame number.
//
UInt32 WiiOHCI::GetFrameNumber32(void) {
  UInt32 halfFrameNumber;
  UInt16 hcFrameNumber;

  hcFrameNumber = USBToHostWord(_hccaPtr->frameNumber);
  halfFrameNumber = ((UInt32) _frameNumber) + hcFrameNumber;
  if (hcFrameNumber < 200) {
    if (readReg32(kOHCIRegIntStatus) & kOHCIRegIntStatusFrameNumberOverflow) {
      halfFrameNumber += BIT16;
    }
  }

  return halfFrameNumber;
}

//
//  WiiOHCI_Descriptors.cpp
//  Wii OHCI USB controller interface
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include "WiiOHCI.hpp"

//
// Converts the status of a transfer descriptor to I/O Kit status.
//
IOReturn WiiOHCI::convertTDStatus(UInt8 ohciStatus) {
  static const UInt32 statusToErrorMap[] = {
    kIOReturnSuccess,
    kIOUSBCRCErr,
    kIOUSBBitstufErr,
    kIOUSBDataToggleErr,
    kIOUSBPipeStalled,
    kIOReturnNotResponding,
    kIOUSBPIDCheckErr,
    kIOUSBWrongPIDErr,
    kIOReturnOverrun,
    kIOReturnUnderrun,
    kIOUSBReserved1Err,
    kIOUSBReserved2Err,
    kIOUSBBufferOverrunErr,
    kIOUSBBufferUnderrunErr,
    kIOUSBNotSent1Err,
    kIOUSBNotSent2Err
  };

  if (ohciStatus > 15) {
    return kIOReturnInternalError;
  }
  return statusToErrorMap[ohciStatus];
}

//
// Creates a mapping for the specified transfer data.
//
IOReturn WiiOHCI::createPhysTDMapping(OHCITransferData *transfer) {
  OHCIPhysicalMapping *physMapping;

  physMapping = (OHCIPhysicalMapping *) IOMalloc(sizeof (*physMapping));
  if (physMapping == NULL) {
    return kIOReturnNoMemory;
  }

  physMapping->transferDesc = transfer;
  physMapping->physAddr     = transfer->physAddr;

  physMapping->nextMapping = _physMappingHeadPtr;
  _physMappingHeadPtr      = physMapping;

  return kIOReturnSuccess;
}

//
// Gets the transfer data for the specified physical address.
//
OHCITransferData *WiiOHCI::getTDFromPhysMapping(UInt32 physAddr) {
  OHCIPhysicalMapping *physMapping;

  physMapping = _physMappingHeadPtr;
  while (physMapping != NULL) {
    if (physMapping->physAddr == physAddr) {
      return physMapping->transferDesc;
    }

    physMapping = physMapping->nextMapping;
  }

  return NULL;
}

//
// Allocates and adds a page of new endpoints to the free list.
//
IOReturn WiiOHCI::allocateFreeEndpoints(void) {
  OHCIEndpointDescriptor  *hcEndpointDesc;
  IOPhysicalAddress       hcEndpointDescPhysAddr;
  OHCIEndpointData        *endpoint;
  IOReturn                status;

  //
  // Allocate host controller endpoint descriptors out of a page.
  // Wii platforms are not cache coherent, host controller structures must be non-cacheable.
  //
  hcEndpointDesc = (OHCIEndpointDescriptor*) IOMallocContiguous(PAGE_SIZE, PAGE_SIZE, &hcEndpointDescPhysAddr);
  if (hcEndpointDesc == NULL) {
    return kIOReturnNoMemory;
  }
  status = IOSetProcessorCacheMode(kernel_task, (IOVirtualAddress) hcEndpointDesc, PAGE_SIZE, kIOInhibitCache);
  if (status != kIOReturnSuccess) {
    return status;
  }

  //
  // Allocate endpoints, adding in the allocated host controller data.
  // Each one is added to the free list.
  //
  for (UInt32 i = 0; i < (PAGE_SIZE / sizeof (OHCIEndpointDescriptor)); i++) {
    endpoint = (OHCIEndpointData*) IOMalloc(sizeof (*endpoint));
    if (endpoint == NULL) {
      return kIOReturnNoMemory;
    }
    endpoint->ed       = hcEndpointDesc;
    endpoint->physAddr = hcEndpointDescPhysAddr;

    endpoint->nextEndpoint = _freeEndpointHeadPtr;
    _freeEndpointHeadPtr   = endpoint;

    hcEndpointDesc++;
    hcEndpointDescPhysAddr += sizeof (OHCIEndpointDescriptor);
  }

  return kIOReturnSuccess;
}

//
// Allocates and adds a page of new transfers to the free list.
//
IOReturn WiiOHCI::allocateFreeTransfers(bool mem2) {
  OHCITransferDescriptor  *hcTransferDesc;
  IOPhysicalAddress       hcTransferDescPhysAddr;
  OHCITransferData        *transfer;
  IORangeScalar           mem2PhysAddr;
  IOPhysicalSegment       seg;
  IOReturn                status;

  //
  // Allocate host controller transfer descriptors out of a page.
  // Wii platforms are not cache coherent, host controller structures must be non-cacheable.
  //
  hcTransferDesc = (OHCITransferDescriptor*) IOMallocContiguous(PAGE_SIZE, PAGE_SIZE, &hcTransferDescPhysAddr);
  if (hcTransferDesc == NULL) {
    return kIOReturnNoMemory;
  }
  status = IOSetProcessorCacheMode(kernel_task, (IOVirtualAddress) hcTransferDesc, PAGE_SIZE, kIOInhibitCache);
  if (status != kIOReturnSuccess) {
    return status;
  }

  //
  // Allocate transfers, adding in the allocated host controller data.
  // Each one is added to the free list.
  //
  for (UInt32 i = 0; i < (PAGE_SIZE / sizeof (OHCITransferDescriptor)); i++) {
    transfer = (OHCITransferData*) IOMalloc(sizeof (*transfer));
    if (transfer == NULL) {
      return kIOReturnNoMemory;
    }
    transfer->td       = hcTransferDesc;
    transfer->physAddr = hcTransferDescPhysAddr;

    //
    // Create a mapping so we can find this one later from only the physical address.
    //
    status = createPhysTDMapping(transfer);
    if (status != kIOReturnSuccess) {
      return status;
    }

    //
    // Create temporary buffer mapping from MEM2 if on Wii.
    // Some transfer descriptors will be created in MEM2 for the last one in a transfer.
    // TODO
    if (mem2 && (_mem2Allocator != NULL)) {
      if (!_mem2Allocator->allocate(kWiiOHCITempBufferSize, &mem2PhysAddr, kOHCITransferDescriptorAlignment)) {
        return NULL; // TODO
      }
      transfer->tmpBuffer = IOMemoryDescriptor::withPhysicalAddress(mem2PhysAddr, kWiiOHCITempBufferSize, kIODirectionInOut);
      if (transfer->tmpBuffer == NULL) {
        WIIDBGLOG("Failed to allocate a buffer");
        return NULL;
      }
      transfer->tmpBufferPtr = (void*) transfer->tmpBuffer->map(kIOMapInhibitCache)->getVirtualAddress();
      transfer->mem2         = true;

      transfer->nextTransfer   = _freeMem2TransferHeadPtr;
      _freeMem2TransferHeadPtr = transfer;

    //
    // Wii U still needs double buffers due to cache requirements, but can be in any memory location.
    //
    } else {
      //
      // Allocate bounce buffer.
      // Wii platforms are not cache coherent, host controller structures must be non-cacheable.
      // Need to allocate an entire page at a time to ensure nothing else will occupy this cache-inhibited area.
      //
      transfer->tmpBufferPtr = IOMallocContiguous(PAGE_SIZE, PAGE_SIZE, &transfer->tmpBufferPhysAddr);
      if (transfer->tmpBufferPtr == NULL) {
        return kIOReturnNoMemory;
      }
      status = IOSetProcessorCacheMode(kernel_task, (IOVirtualAddress) transfer->tmpBufferPtr, PAGE_SIZE, kIOInhibitCache);
      if (status != kIOReturnSuccess) {
        return status;
      }
      transfer->mem2 = false;

      transfer->nextTransfer = _freeTransferHeadPtr;
      _freeTransferHeadPtr   = transfer;
    }

    hcTransferDesc++;
    hcTransferDescPhysAddr += sizeof (OHCITransferDescriptor);
  }

  return kIOReturnSuccess;
}

//
// Gets a free endpoint from the free linked list.
//
OHCIEndpointData *WiiOHCI::getFreeEndpoint(void) {
  OHCIEndpointData *endpoint;

  if (_freeEndpointHeadPtr == NULL) {
    if (allocateFreeEndpoints() != kIOReturnSuccess) {
      return NULL;
    }
  }
  endpoint = _freeEndpointHeadPtr;

  //
  // Adjust linkage for remaining free EDs.
  //
  _freeEndpointHeadPtr   = endpoint->nextEndpoint;
  endpoint->nextEndpoint = NULL;

  return endpoint;
}

//
// Gets a free transfer from the free linked list.
//
OHCITransferData *WiiOHCI::getFreeTransfer(bool requireMem2) {
  OHCITransferData *transfer;

  if (requireMem2) {
    if (_freeMem2TransferHeadPtr == NULL) {
      if (allocateFreeTransfers(true) != kIOReturnSuccess) {
        return NULL;
      }  
    }
    transfer = _freeMem2TransferHeadPtr;

    //
    // Adjust linkage for remaining free TDs.
    //
    _freeMem2TransferHeadPtr = transfer->nextTransfer;
    transfer->nextTransfer   = NULL;
  } else {
    if (_freeTransferHeadPtr == NULL) {
      if (allocateFreeTransfers(false) != kIOReturnSuccess) {
        return NULL;
      }  
    }
    transfer = _freeTransferHeadPtr;

    //
    // Adjust linkage for remaining free TDs.
    //
    _freeTransferHeadPtr   = transfer->nextTransfer;
    transfer->nextTransfer = NULL;
  }

  return transfer;
}

//
// Returns an endpoint to the free linked list.
//
void WiiOHCI::returnEndpoint(OHCIEndpointData *endpoint) {
  //
  // Remove the tail transfer.
  //
  returnTransfer(endpoint->tailTransfer);

  endpoint->nextEndpoint = _freeEndpointHeadPtr;
  _freeEndpointHeadPtr   = endpoint;
}

//
// Returns a transfer to the free linked list.
//
void WiiOHCI::returnTransfer(OHCITransferData *transfer) {
  if (transfer->mem2) {
    transfer->nextTransfer   = _freeMem2TransferHeadPtr;
    _freeMem2TransferHeadPtr = transfer;
  } else {
    transfer->nextTransfer   = _freeTransferHeadPtr;
    _freeTransferHeadPtr     = transfer;
  }
}

//
// Initializes the control endpoint linked list.
//
IOReturn WiiOHCI::initControlEndpoints(void) {
  //
  // Create the tail endpoint.
  //
  _controlEndpointTailPtr = getFreeEndpoint();
  if (_controlEndpointTailPtr == NULL) {
    return kIOReturnNoMemory;
  }

  //
  // Controller is to skip the tail endpoint, with this endpoint being the end of the list.
  //
  _controlEndpointTailPtr->ed->flags          = HostToUSBLong(kOHCIEDFlagsSkip);
  _controlEndpointTailPtr->ed->nextEDPhysAddr = HostToUSBLong(0);
  _controlEndpointTailPtr->nextEndpoint       = NULL;

  //
  // Create the head endpoint.
  //
  _controlEndpointHeadPtr = getFreeEndpoint();
  if (_controlEndpointHeadPtr == NULL) {
    return kIOReturnNoMemory;
  }

  //
  // Controller is to skip the head endpoint, points to the tail endpoint.
  //
  _controlEndpointHeadPtr->ed->flags          = HostToUSBLong(kOHCIEDFlagsSkip);
  _controlEndpointHeadPtr->ed->nextEDPhysAddr = HostToUSBLong(_controlEndpointTailPtr->physAddr);
  _controlEndpointHeadPtr->nextEndpoint       = _controlEndpointTailPtr;

  //
  // Configure controller to use the new control endpoint list.
  //
  writeReg32(kOHCIRegControlCurrentED, 0);
  writeReg32(kOHCIRegControlHeadED, _controlEndpointHeadPtr->physAddr);

  return kIOReturnSuccess;
}

//
// Initializes the bulk endpoint linked list.
//
IOReturn WiiOHCI::initBulkEndpoints(void) {
  //
  // Create the tail endpoint.
  //
  _bulkEndpointTailPtr = getFreeEndpoint();
  if (_bulkEndpointTailPtr == NULL) {
    return kIOReturnNoMemory;
  }

  //
  // Controller is to skip the tail endpoint, with this endpoint being the end of the list.
  //
  _bulkEndpointTailPtr->ed->flags          = HostToUSBLong(kOHCIEDFlagsSkip);
  _bulkEndpointTailPtr->ed->nextEDPhysAddr = HostToUSBLong(0);
  _bulkEndpointTailPtr->nextEndpoint       = NULL;

  //
  // Create the head endpoint.
  //
  _bulkEndpointHeadPtr = getFreeEndpoint();
  if (_bulkEndpointHeadPtr == NULL) {
    return kIOReturnNoMemory;
  }

  //
  // Controller is to skip the head endpoint, points to the tail endpoint.
  //
  _bulkEndpointHeadPtr->ed->flags          = HostToUSBLong(kOHCIEDFlagsSkip);
  _bulkEndpointHeadPtr->ed->nextEDPhysAddr = HostToUSBLong(_bulkEndpointTailPtr->physAddr);
  _bulkEndpointHeadPtr->nextEndpoint       = _bulkEndpointTailPtr;

  //
  // Configure controller to use the new bulk endpoint list.
  //
  writeReg32(kOHCIRegBulkCurrentED, 0);
  writeReg32(kOHCIRegBulkHeadED, _bulkEndpointHeadPtr->physAddr);

  return kIOReturnSuccess;
}

//
// Initializes the interrupt endpoint tree and HCCA area.
//
IOReturn WiiOHCI::initInterruptEndpoints(void) {
  //
  // Allocate all static interrupt endpoints.
  // These will always be disabled and in place to support placing
  // later allocated endpoints are various poll timings.
  //
  for (unsigned int i = 0; i < kWiiOHCIInterruptNodeCount; i++) {
    _interruptEndpoints[i].headEndpoint = getFreeEndpoint();
    if (_interruptEndpoints[i].headEndpoint == NULL) {
      return kIOReturnNoMemory;
    }

    _interruptEndpoints[i].headEndpoint->ed->flags          = HostToUSBLong(kOHCIEDFlagsSkip);
    _interruptEndpoints[i].headEndpoint->ed->nextEDPhysAddr = HostToUSBLong(0);
    _interruptEndpoints[i].headEndpoint->nextEndpoint       = NULL;

    //
    // First 32 will be static heads in the HCCA.
    //
    if (i < ARRSIZE(_hccaPtr->interruptTablePhysAddr)) {
      _hccaPtr->interruptTablePhysAddr[i] = HostToUSBLong(_interruptEndpoints[i].headEndpoint->physAddr);
    }
  }

  //
  // Build out the tree.
  // Endpoint descriptors in the HCCA (32ms) will join to form a tree, ending at the 1ms node.
  // 32ms - 16ms - 8ms - 4ms - 2ms - 1ms
  //
  for (int i = 0, p = 0, q = 32, z = 0; i < (kWiiOHCIInterruptNodeCount - 1); i++) {
    if (i < ((q / 2) + p)) {
      z = i + q;
    } else {
      z = i + (q / 2);
    }

    //
    // Move up the tree to next lowest polling rate.
    //
    if (i == (p + q - 1)) {
      p = p + q;
      q = q / 2;
    }

    //
    // Link endpoint descriptors together.
    //
    _interruptEndpoints[i].headEndpoint->ed->nextEDPhysAddr = HostToUSBLong(_interruptEndpoints[z].headEndpoint->physAddr);
    _interruptEndpoints[i].headEndpoint->nextEndpoint       = _interruptEndpoints[z].headEndpoint;
    _interruptEndpoints[i].tailEndpoint                     = _interruptEndpoints[i].headEndpoint->nextEndpoint;
  }

  // TODO: isochronous endpoint chain

  return kIOReturnSuccess;
}

//
// Gets the endpoint data for the specified function/endpoint.
//
OHCIEndpointData *WiiOHCI::getEndpoint(UInt8 functionNumber, UInt8 endpointNumber, UInt8 direction,
                                                       UInt8 *type, OHCIEndpointData **outPrevEndpoint) {
  OHCIEndpointData  *currEndpoint;
  OHCIEndpointData  *prevEndpoint;
  UInt32            endpointId;

  //
  // Search for control endpoint.
  //
  if (*type & kWiiOHCIEndpointTypeControl) {
    endpointId = (functionNumber & kOHCIEDFlagsFuncMask) | (((UInt32)endpointNumber << kOHCIEDFlagsEndpointShift) & kOHCIEDFlagsEndpointMask);

    prevEndpoint = _controlEndpointHeadPtr;
    currEndpoint = prevEndpoint->nextEndpoint;
    while (currEndpoint != _controlEndpointTailPtr) {
      //
      // Check if current endpoint matches.
      //
      if ((USBToHostLong(currEndpoint->ed->flags) & (kOHCIEDFlagsFuncMask | kOHCIEDFlagsEndpointMask)) == endpointId) {
        *type = kWiiOHCIEndpointTypeControl;
        if (outPrevEndpoint != NULL) {
          *outPrevEndpoint = prevEndpoint;
        }
        return currEndpoint;
      }

      prevEndpoint = currEndpoint;
      currEndpoint = prevEndpoint->nextEndpoint;
    }
  }

  //
  // Search for bulk endpoint.
  //
  if (*type & kWiiOHCIEndpointTypeBulk) {
    endpointId = (functionNumber & kOHCIEDFlagsFuncMask) | (((UInt32)endpointNumber << kOHCIEDFlagsEndpointShift) & kOHCIEDFlagsEndpointMask);

    prevEndpoint = _bulkEndpointHeadPtr;
    currEndpoint = prevEndpoint->nextEndpoint;
    while (currEndpoint != _bulkEndpointTailPtr) {
      //
      // Check if current endpoint matches.
      //
      if ((USBToHostLong(currEndpoint->ed->flags) & (kOHCIEDFlagsFuncMask | kOHCIEDFlagsEndpointMask)) == endpointId) {
        *type = kWiiOHCIEndpointTypeBulk;
        if (outPrevEndpoint != NULL) {
          *outPrevEndpoint = prevEndpoint;
        }
        return currEndpoint;
      }

      prevEndpoint = currEndpoint;
      currEndpoint = prevEndpoint->nextEndpoint;
    }
  }

  //
  // Search for interrupt endpoint.
  //
  if (*type & kWiiOHCIEndpointTypeInterrupt) {
    endpointId = (functionNumber & kOHCIEDFlagsFuncMask) | (((UInt32)endpointNumber << kOHCIEDFlagsEndpointShift) & kOHCIEDFlagsEndpointMask);
    if (direction == kUSBOut) {
      endpointId |= kOHCIEDFlagsDirectionOut;
    } else if (direction == kUSBIn) {
      endpointId |= kOHCIEDFlagsDirectionIn;
    }

    for (unsigned int i = 0; i < ARRSIZE(_interruptEndpoints); i++) {
      currEndpoint = _interruptEndpoints[i].headEndpoint->nextEndpoint;
      prevEndpoint = _interruptEndpoints[i].headEndpoint;

      while (currEndpoint != _interruptEndpoints[i].tailEndpoint) {
        if ((USBToHostLong(currEndpoint->ed->flags) & (kOHCIEDFlagsFuncMask | kOHCIEDFlagsEndpointMask | kOHCIEDFlagsDirectionMask)) == endpointId) {
          *type = kWiiOHCIEndpointTypeInterrupt;
          if (outPrevEndpoint != NULL) {
            *outPrevEndpoint = prevEndpoint;
          }
          return currEndpoint;
        }

        prevEndpoint = currEndpoint;
        currEndpoint = currEndpoint->nextEndpoint;
      }
    }
  }

  return NULL;
}

//
// Gets an interrupt endpoint head for the specified polling rate.
//
OHCIEndpointData *WiiOHCI::getInterruptEndpointHead(UInt8 pollingRate) {
  UInt16 frameNumber;

  //
  // Get frame number for randomness.
  //
  frameNumber = (UInt16)(readReg32(kOHCIRegFmNumber) & kOHCIRegFmNumberMask);

  if (pollingRate < 1) {
    return NULL;
  } else if (pollingRate < 2) {
    return _interruptEndpoints[62].headEndpoint; // 1ms.
  } else if (pollingRate < 4) {
    return _interruptEndpoints[60 + (frameNumber % 2)].headEndpoint; // 2ms.
  } else if (pollingRate < 8) {
    return _interruptEndpoints[56 + (frameNumber % 4)].headEndpoint; // 4ms.
  } else if (pollingRate < 16) {
    return _interruptEndpoints[48 + (frameNumber % 8)].headEndpoint; // 8ms.
  } else if (pollingRate < 32) {
    return _interruptEndpoints[32 + (frameNumber % 16)].headEndpoint; // 16ms.
  } else {
    return _interruptEndpoints[frameNumber % 32].headEndpoint; // 32ms.
  }
}

//
// Adds a new endpoint to the specified list.
//
IOReturn WiiOHCI::addNewEndpoint(UInt8 functionNumber, UInt8 endpointNumber, UInt16 maxPacketSize,
                                 UInt8 speed, UInt8 direction, OHCIEndpointData *endpointHeadPtr,
                                 bool isIsoTransfer) {
  OHCIEndpointData  *endpoint;
  OHCITransferData  *transferTail;
  UInt32            flags;

  //
  // Get a free endpoint.
  //
  endpoint = getFreeEndpoint();
  if (endpoint == NULL) {
    return kIOReturnNoMemory;
  }

  //
  // Configure flags indicating what device this endpoint is for.
  //
  flags = (functionNumber & kOHCIEDFlagsFuncMask)
    | ((endpointNumber << kOHCIEDFlagsEndpointShift) & kOHCIEDFlagsEndpointMask)
    | ((speed == kUSBDeviceSpeedLow) ? kOHCIEDFlagsLowSpeed : 0)
    | ((maxPacketSize << kOHCIEDFlagsMaxPktSizeShift) & kOHCIEDFlagsMaxPktSizeMask)
    | (isIsoTransfer ? kOHCIEDFlagsIsochronous: 0);

  if (direction == kUSBOut) {
    flags |= kOHCIEDFlagsDirectionOut;
  } else if (direction == kUSBIn) {
    flags |= kOHCIEDFlagsDirectionIn;
  }
  endpoint->ed->flags = HostToUSBLong(flags);

  //
  // Create initial transfer tail.
  //
  transferTail = getFreeTransfer(false);
  if (transferTail == NULL) {
    returnEndpoint(endpoint);
    return kIOReturnNoMemory;
  }

  transferTail->td->nextTDPhysAddr = 0;
  transferTail->nextTransfer       = NULL;

  //
  // Set new transfer as head and tail to indicate no active transfers.
  //
  endpoint->headTransfer       = transferTail;
  endpoint->tailTransfer       = transferTail;
  endpoint->ed->headTDPhysAddr = HostToUSBLong(transferTail->physAddr);
  endpoint->ed->tailTDPhysAddr = HostToUSBLong(transferTail->physAddr);

  //
  // Insert new endpoint into linked list.
  //
  endpoint->nextEndpoint              = endpointHeadPtr->nextEndpoint;
  endpointHeadPtr->nextEndpoint       = endpoint;
  endpoint->ed->nextEDPhysAddr        = endpointHeadPtr->ed->nextEDPhysAddr;
  endpointHeadPtr->ed->nextEDPhysAddr = HostToUSBLong(endpoint->physAddr);

  return kIOReturnSuccess;
}

//
// Removes an endpoint from the specified list.
//
IOReturn WiiOHCI::removeEndpoint(UInt8 functionNumber, UInt8 endpointNumber,
                                 OHCIEndpointData *endpointHeadPtr, OHCIEndpointData *endpointTailPtr) {
  OHCIEndpointData  *currentEndpoint;
  UInt32            deviceNumber;

  deviceNumber    = (functionNumber & kOHCIEDFlagsFuncMask) | (((UInt32)endpointNumber << kOHCIEDFlagsEndpointShift) & kOHCIEDFlagsEndpointMask);
  currentEndpoint = endpointHeadPtr->nextEndpoint;
  while (currentEndpoint != endpointTailPtr) {
    //
    // Check if current endpoint matches.
    //
    if ((USBToHostLong(currentEndpoint->ed->flags) & (kOHCIEDFlagsFuncMask | kOHCIEDFlagsEndpointMask)) == deviceNumber) {
      //
      // Remove the endpoint.
      //
      return kIOReturnSuccess;
    }

    currentEndpoint = currentEndpoint->nextEndpoint;
  }

  return kIOUSBEndpointNotFound;
}

//
// Removes and completes all transfers linked to the specified endpoint.
//
void WiiOHCI::removeEndpointTransfers(OHCIEndpointData *endpoint) {
  OHCITransferData  *currTransfer;
  OHCITransferData  *nextTransfer;
  UInt32            bufferSizeRemaining;

  WIIDBGLOG("Removing all TDs for endpoint phys: 0x%X", endpoint->physAddr);

  //
  // Get the current head and remove entire chain.
  //
  WIIDBGLOG("TD head phys: 0x%X, tail phys: 0x%X", USBToHostLong(endpoint->ed->headTDPhysAddr), USBToHostLong(endpoint->ed->tailTDPhysAddr));
  currTransfer = getTDFromPhysMapping(USBToHostLong(endpoint->ed->headTDPhysAddr) & kOHCIEDTDHeadMask);
  endpoint->headTransfer       = endpoint->tailTransfer;
  endpoint->ed->headTDPhysAddr = endpoint->ed->tailTDPhysAddr;

  //
  // Iterate through chain.
  //
  bufferSizeRemaining = 0;
  while (currTransfer != endpoint->tailTransfer) {
    if (currTransfer == NULL) {
      // Shouldn't occur.
      WIISYSLOG("Got an invalid TD here");
      return;
    }

    WIIDBGLOG("Unlinking TD phys 0x%X, next 0x%X, buf %p", currTransfer->physAddr,
      USBToHostLong(currTransfer->td->nextTDPhysAddr), currTransfer->srcBuffer);

    if (currTransfer->srcBuffer != NULL) {
      OSSafeReleaseNULL(currTransfer->srcBuffer);
    }

    if (currTransfer->type == kOHCITransferTypeIsochronous) {
      // TODO
    } else {
      //
      // No data actually was transfered, so need to account for all buffers in the chain.
      //
      if (USBToHostLong(currTransfer->td->currentBufferPtrPhysAddr) != 0) {
        bufferSizeRemaining += USBToHostLong(currTransfer->td->bufferEndPhysAddr) - USBToHostLong(currTransfer->td->currentBufferPtrPhysAddr) + 1;
      }

      //
      // Invoke completion for final transfer.
      //
      if (currTransfer->last) {
        Complete(currTransfer->completion.gen, kIOReturnAborted, bufferSizeRemaining);
        bufferSizeRemaining = 0;
      }
    }

    nextTransfer = getTDFromPhysMapping(USBToHostLong(currTransfer->td->nextTDPhysAddr));
    returnTransfer(currTransfer);
    currTransfer = nextTransfer;
  }
}

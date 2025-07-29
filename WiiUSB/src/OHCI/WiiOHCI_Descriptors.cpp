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
// Creates a mapping for the specified transfer descriptor.
//
IOReturn WiiOHCI::createPhysTDMapping(OHCITransferDescriptor *transferDesc) {
  OHCIPhysicalMapping *physMapping;

  physMapping = (OHCIPhysicalMapping *) IOMalloc(sizeof (*physMapping));
  if (physMapping == NULL) {
    return kIOReturnNoMemory;
  }

  physMapping->transferDesc = transferDesc;
  physMapping->physAddr     = transferDesc->physAddr;

  physMapping->nextMapping = _physMappingHeadPtr;
  _physMappingHeadPtr      = physMapping;

  return kIOReturnSuccess;
}

//
// Gets the transfer descriptor for the specified physical address.
//
OHCITransferDescriptor *WiiOHCI::getTDFromPhysMapping(UInt32 physAddr) {
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
// Allocates the initial endpoint and transfer descriptors.
//
IOReturn WiiOHCI::allocateDescriptors(void) {
  OHCIEndpointDescriptor      *endpointDescs;
  OHCITransferDescriptor      *transferDescs;
  IOPhysicalSegment           seg;
  IORangeScalar               mem2PhysAddr;
  IOReturn                    status;

  //
  // Create buffers.
  //
  _initialEDBuffer  = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous,
    sizeof (OHCIEndpointDescriptor) * kWiiOHCIInitialNumEDs, kOHCIEndpointDescriptorAlignment);
  _initialTDBuffer = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous,
    sizeof (OHCITransferDescriptor) * kWiiOHCIInitialNumTDs, kOHCITransferDescriptorAlignment);

  if ((_initialEDBuffer == NULL) || (_initialTDBuffer == NULL)) {
    WIISYSLOG("Failed to create desc buffers");
    return kIOReturnNoResources;
  }

  //
  // Configure endpoint descriptors, adding each one to the free list.
  //
  endpointDescs = (OHCIEndpointDescriptor *) _initialEDBuffer->getBytesNoCopy();
  for (int i = 0; i < kWiiOHCIInitialNumEDs; i++) {
    //
    // Get the physical address.
    //
    if (_memoryCursor->getPhysicalSegments(_initialEDBuffer, i * sizeof (endpointDescs[i]), &seg, 1, sizeof (endpointDescs[i])) != 1) {
      return kIOReturnDMAError;
    }
    if (seg.length != sizeof (endpointDescs[i])) { // TODO: Is this even possible.
      WIIDBGLOG("Too small segment here");
      return kIOReturnDMAError;
    }
    endpointDescs[i].physAddr = seg.location;

    //
    // Add to linked list.
    // No need to set the physical address, that will be set when the descriptor is used.
    //
    if (i > 0) {
      endpointDescs[i - 1].nextED = &endpointDescs[i];
    }
  }
  _freeEDHeadPtr = &endpointDescs[0];

  //
  // Configure transfer descriptors, adding each one to the free list.
  // These can be used for either general transfers or isochronous.
  //
  transferDescs = (OHCITransferDescriptor *) _initialTDBuffer->getBytesNoCopy();
  for (int i = 0; i < kWiiOHCIInitialNumTDs; i++) {
    //
    // Get the physical address.
    //
    if (_memoryCursor->getPhysicalSegments(_initialTDBuffer, i * sizeof (transferDescs[i]), &seg, 1, sizeof (transferDescs[i])) != 1) {
      return kIOReturnDMAError;
    }
    if (seg.length != sizeof (transferDescs[i])) { // TODO: Is this even possible.
      WIIDBGLOG("Too small segment here");
      return kIOReturnDMAError;
    }

    transferDescs[i].physAddr = seg.location;
    status = createPhysTDMapping(&transferDescs[i]);
    if (status != kIOReturnSuccess) {
      return status;
    }

    //
    // Create temporary buffer mapping from MEM2 if on Wii.
    //
    if (_mem2Allocator != NULL) {
      if (!_mem2Allocator->allocate(kWiiOHCITempBufferSize, &mem2PhysAddr, kOHCITransferDescriptorAlignment)) {
        return kIOReturnNoResources;
      }
      transferDescs[i].tmpBuffer = IOMemoryDescriptor::withPhysicalAddress(mem2PhysAddr, kWiiOHCITempBufferSize, kIODirectionInOut);
      if (transferDescs[i].tmpBuffer == NULL) {
        WIIDBGLOG("Failed to allocate a buffer");
        return kIOReturnNoResources;
      }
      transferDescs[i].tmpBufferPtr = (void*) transferDescs[i].tmpBuffer->map(kIOMapInhibitCache)->getVirtualAddress();

    //
    // Wii U still needs double buffers due to cache requirements, but can be in any memory location.
    //
    } else {
      transferDescs[i].tmpBuffer = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous, kWiiOHCITempBufferSize, kWiiOHCITempBufferSize);
      if (transferDescs[i].tmpBuffer == NULL) {
        WIISYSLOG("Failed to allocate transfer descriptor double buffer");
        return kIOReturnNoResources;
      }

      //
      // Get physical address.
      //
      if (_memoryCursor->getPhysicalSegments(transferDescs[i].tmpBuffer, 0, &seg, 1, kWiiOHCITempBufferSize) != 1) {
        WIISYSLOG("Failed to get transfer descriptor double buffer address");
        return kIOReturnNoResources;
      }

      transferDescs[i].tmpBufferPhysAddr = seg.location;
      transferDescs[i].tmpBufferPtr      = ((IOBufferMemoryDescriptor*) transferDescs[i].tmpBuffer)->getBytesNoCopy();
    }

    //
    // Add to linked list.
    // No need to set the physical address, that will be set when the descriptor is used.
    //
    if (i > 0) {
      transferDescs[i - 1].nextTD = &transferDescs[i];
    }
  }
  _freeTDHeadPtr = &transferDescs[0];

  return kIOReturnSuccess;
}

//
// Gets a free endpoint descriptor from the free linked list.
//
OHCIEndpointDescriptor *WiiOHCI::getFreeEndpointDescriptor(void) {
  OHCIEndpointDescriptor *freeED;

  freeED = _freeEDHeadPtr;
  if (freeED == NULL) {
    // TODO: Dynamic allocate new EDs.
    return NULL;
  }

  //
  // Adjust linkage for remaining free EDs.
  //
  _freeEDHeadPtr = freeED->nextED;
  freeED->nextED = NULL;
  return freeED;
}

//
// Gets a free transfer descriptor from the free linked list.
//
OHCITransferDescriptor *WiiOHCI::getFreeTransferDescriptor(void) {
  OHCITransferDescriptor *freeTD;

  freeTD = _freeTDHeadPtr;
  if (freeTD == NULL) {
    // TODO: Dynamic allocate new TDs.
    return NULL;
  }

  //
  // Adjust linkage for remaining free TDs.
  //
  _freeTDHeadPtr = freeTD->nextTD;
  freeTD->nextTD = NULL;
  return freeTD;
}

//
// Returns an endpoint descriptor to the free linked list.
//
void WiiOHCI::returnEndpointDescriptor(OHCIEndpointDescriptor *endpointDesc) {
  //
  // Remove the tail transfer descriptor.
  //
  returnTransferDescriptor(endpointDesc->tailTD);

  endpointDesc->nextED = _freeEDHeadPtr;
  _freeEDHeadPtr = endpointDesc;
}

//
// Returns a transfer descriptor to the free linked list.
//
void WiiOHCI::returnTransferDescriptor(OHCITransferDescriptor *transferDesc) {
  transferDesc->nextTD = _freeTDHeadPtr;
  _freeTDHeadPtr = transferDesc;
}

//
// Initializes the control endpoint linked list.
//
IOReturn WiiOHCI::initControlEDs(void) {
  //
  // Create the tail ED.
  //
  _edControlTailPtr = getFreeEndpointDescriptor();
  if (_edControlTailPtr == NULL) {
    return kIOReturnNoMemory;
  }

  //
  // Controller is to skip the tail ED, with this ED being the end of the list.
  //
  _edControlTailPtr->ep.flags           = HostToUSBLong(kOHCIEDFlagsSkip);
  _edControlTailPtr->ep.nextEDPhysAddr  = HostToUSBLong(0);
  _edControlTailPtr->nextED             = NULL;
  flushEndpointDescriptor(_edControlTailPtr);

  //
  // Create the head ED.
  //
  _edControlHeadPtr = getFreeEndpointDescriptor();
  if (_edControlHeadPtr == NULL) {
    return kIOReturnNoMemory;
  }

  //
  // Controller is to skip the head ED, points to the tail ED.
  //
  _edControlHeadPtr->ep.flags           = HostToUSBLong(kOHCIEDFlagsSkip);
  _edControlHeadPtr->ep.nextEDPhysAddr  = HostToUSBLong(_edControlTailPtr->physAddr);
  _edControlHeadPtr->nextED             = _edControlTailPtr;
  flushEndpointDescriptor(_edControlHeadPtr);

  //
  // Configure controller.
  //
  writeReg32(kOHCIRegControlCurrentED, 0);
  writeReg32(kOHCIRegControlHeadED, _edControlHeadPtr->physAddr);

  return kIOReturnSuccess;
}

//
// Initializes the bulk endpoint linked list.
//
IOReturn WiiOHCI::initBulkEDs(void) {
  //
  // Create the tail ED.
  //
  _edBulkTailPtr = getFreeEndpointDescriptor();
  if (_edBulkTailPtr == NULL) {
    return kIOReturnNoMemory;
  }

  //
  // Controller is to skip the tail ED, with this ED being the end of the list.
  //
  _edBulkTailPtr->ep.flags          = HostToUSBLong(kOHCIEDFlagsSkip);
  _edBulkTailPtr->ep.nextEDPhysAddr = HostToUSBLong(0);
  _edBulkTailPtr->nextED            = NULL;
  flushEndpointDescriptor(_edBulkTailPtr);

  //
  // Create the head ED.
  //
  _edBulkHeadPtr = getFreeEndpointDescriptor();
  if (_edBulkHeadPtr == NULL) {
    return kIOReturnNoMemory;
  }

  //
  // Controller is to skip the head ED, points to the tail ED.
  //
  _edBulkHeadPtr->ep.flags          = HostToUSBLong(kOHCIEDFlagsSkip);
  _edBulkHeadPtr->ep.nextEDPhysAddr = HostToUSBLong(_edBulkTailPtr->physAddr);
  _edBulkHeadPtr->nextED            = _edBulkTailPtr;
  flushEndpointDescriptor(_edBulkHeadPtr);

  //
  // Configure controller.
  //
  writeReg32(kOHCIRegBulkCurrentED, 0);
  writeReg32(kOHCIRegBulkHeadED, _edBulkHeadPtr->physAddr);

  return kIOReturnSuccess;
}

//
// Initializes the interrupt endpoint tree and HCCA area.
//
IOReturn WiiOHCI::initInterruptEDs(void) {
  //
  // Allocate all static interrupt endpoints.
  // These will always be disabled and in place to support placing
  // later allocated endpoints are various poll timings.
  //
  for (unsigned int i = 0; i < kWiiOHCIInterruptNodeCount; i++) {
    _edInterrupts[i].headED = getFreeEndpointDescriptor();
    if (_edInterrupts[i].headED == NULL) {
      return kIOReturnNoMemory;
    }

    _edInterrupts[i].headED->ep.flags          = HostToUSBLong(kOHCIEDFlagsSkip);
    _edInterrupts[i].headED->ep.nextEDPhysAddr = HostToUSBLong(0);
    _edInterrupts[i].headED->nextED            = NULL;
    flushEndpointDescriptor(_edInterrupts[i].headED);

    //
    // First 32 will be static heads in the HCCA.
    //
    if (i < ARRSIZE(_hccaPtr->interruptTablePhysAddr)) {
      _hccaPtr->interruptTablePhysAddr[i] = HostToUSBLong(_edInterrupts[i].headED->physAddr);
    }
  }
  flushDataCache(_hccaPtr->interruptTablePhysAddr, sizeof (_hccaPtr->interruptTablePhysAddr));

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
    _edInterrupts[i].headED->ep.nextEDPhysAddr = HostToUSBLong(_edInterrupts[z].headED->physAddr);
    _edInterrupts[i].headED->nextED            = _edInterrupts[z].headED;
    _edInterrupts[i].tailED                    = _edInterrupts[i].headED->nextED;
    flushEndpointDescriptor(_edInterrupts[i].headED);
  }

  // TODO: isochronous endpoint chain

  return kIOReturnSuccess;
}

//
// Gets the endpoint descriptor for the specified function/endpoint.
//
OHCIEndpointDescriptor *WiiOHCI::getEndpoint(UInt8 functionNumber, UInt8 endpointNumber, UInt8 direction,
                                             UInt8 *type, OHCIEndpointDescriptor **outPrevEndpoint) {
  OHCIEndpointDescriptor  *currEndpointDesc;
  OHCIEndpointDescriptor  *prevEndpointDesc;
  UInt32                  endpointId;

  //
  // Search for control endpoint.
  //
  if (*type & kWiiOHCIEndpointTypeControl) {
    endpointId = (functionNumber & kOHCIEDFlagsFuncMask) | (((UInt32)endpointNumber << kOHCIEDFlagsEndpointShift) & kOHCIEDFlagsEndpointMask);

    prevEndpointDesc = _edControlHeadPtr;
    currEndpointDesc = prevEndpointDesc->nextED;
    while (currEndpointDesc != _edControlTailPtr) {
      //
      // Check if current endpoint descriptor matches.
      //
      if ((USBToHostLong(currEndpointDesc->ep.flags) & (kOHCIEDFlagsFuncMask | kOHCIEDFlagsEndpointMask)) == endpointId) {
        *type = kWiiOHCIEndpointTypeControl;
        if (outPrevEndpoint != NULL) {
          *outPrevEndpoint = prevEndpointDesc;
        }
        return currEndpointDesc;
      }

      prevEndpointDesc = currEndpointDesc;
      currEndpointDesc = prevEndpointDesc->nextED;
    }
  }

  //
  // Search for bulk endpoint.
  //
  if (*type & kWiiOHCIEndpointTypeBulk) {
    endpointId = (functionNumber & kOHCIEDFlagsFuncMask) | (((UInt32)endpointNumber << kOHCIEDFlagsEndpointShift) & kOHCIEDFlagsEndpointMask);

    prevEndpointDesc = _edBulkHeadPtr;
    currEndpointDesc = prevEndpointDesc->nextED;
    while (currEndpointDesc != _edBulkTailPtr) {
      //
      // Check if current endpoint descriptor matches.
      //
      if ((USBToHostLong(currEndpointDesc->ep.flags) & (kOHCIEDFlagsFuncMask | kOHCIEDFlagsEndpointMask)) == endpointId) {
        *type = kWiiOHCIEndpointTypeBulk;
        if (outPrevEndpoint != NULL) {
          *outPrevEndpoint = prevEndpointDesc;
        }
        return currEndpointDesc;
      }

      prevEndpointDesc = currEndpointDesc;
      currEndpointDesc = prevEndpointDesc->nextED;
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

    for (unsigned int i = 0; i < ARRSIZE(_edInterrupts); i++) {
      currEndpointDesc = _edInterrupts[i].headED->nextED;
      prevEndpointDesc = _edInterrupts[i].headED;

      while (currEndpointDesc != _edInterrupts[i].tailED) {
        if ((USBToHostLong(currEndpointDesc->ep.flags) & (kOHCIEDFlagsFuncMask | kOHCIEDFlagsEndpointMask | kOHCIEDFlagsDirectionMask)) == endpointId) {
          *type = kWiiOHCIEndpointTypeInterrupt;
          if (outPrevEndpoint != NULL) {
            *outPrevEndpoint = prevEndpointDesc;
          }
          return currEndpointDesc;
        }

        prevEndpointDesc = currEndpointDesc;
        currEndpointDesc = currEndpointDesc->nextED;
      }
    }
  }

  return NULL;
}

//
// Gets an interrupt endpoint head for the specified polling rate.
//
OHCIEndpointDescriptor *WiiOHCI::getInterruptEDHead(UInt8 pollingRate) {
  UInt16 frameNumber;

  //
  // Get frame number for randomness.
  //
  frameNumber = (UInt16)(readReg32(kOHCIRegFmNumber) & kOHCIRegFmNumberMask);

  if (pollingRate < 1) {
    return NULL;
  } else if (pollingRate < 2) {
    return _edInterrupts[62].headED; // 1ms.
  } else if (pollingRate < 4) {
    return _edInterrupts[60 + (frameNumber % 2)].headED; // 2ms.
  } else if (pollingRate < 8) {
    return _edInterrupts[56 + (frameNumber % 4)].headED; // 4ms.
  } else if (pollingRate < 16) {
    return _edInterrupts[48 + (frameNumber % 8)].headED; // 8ms.
  } else if (pollingRate < 32) {
    return _edInterrupts[32 + (frameNumber % 16)].headED; // 16ms.
  } else {
    return _edInterrupts[frameNumber % 32].headED; // 32ms.
  }
}

//
// Adds a new endpoint descriptor to the specified list.
//
IOReturn WiiOHCI::addNewEndpoint(UInt8 functionNumber, UInt8 endpointNumber, UInt16 maxPacketSize,
                                 UInt8 speed, UInt8 direction, OHCIEndpointDescriptor *edHeadPtr,
                                 bool isIsoTransfer) {
  OHCIEndpointDescriptor  *endpointDesc;
  OHCITransferDescriptor  *transferDesc;
  UInt32                  flags;

  //
  // Get a free endpoint descriptor.
  //
  endpointDesc = getFreeEndpointDescriptor();
  if (endpointDesc == NULL) {
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
  endpointDesc->ep.flags = HostToUSBLong(flags);

  //
  // Create initial transfer descriptor.
  //
  transferDesc = getFreeTransferDescriptor();
  if (transferDesc == NULL) {
    returnEndpointDescriptor(endpointDesc);
    return kIOReturnNoMemory;
  }

  transferDesc->td.nextTDPhysAddr  = 0;
  transferDesc->nextTD             = NULL;
  flushTransferDescriptor(transferDesc);

  //
  // Set new transfer descriptor as head and tail to indicate no active transfers.
  //
  endpointDesc->headTD            = transferDesc;
  endpointDesc->tailTD            = transferDesc;
  endpointDesc->ep.headTDPhysAddr = HostToUSBLong(transferDesc->physAddr);
  endpointDesc->ep.tailTDPhysAddr = HostToUSBLong(transferDesc->physAddr);

  //
  // Insert new endpoint descriptor into linked list.
  //
  endpointDesc->nextED            = edHeadPtr->nextED;
  edHeadPtr->nextED               = endpointDesc;
  endpointDesc->ep.nextEDPhysAddr = edHeadPtr->ep.nextEDPhysAddr;
  flushEndpointDescriptor(endpointDesc);
  edHeadPtr->ep.nextEDPhysAddr    = HostToUSBLong(endpointDesc->physAddr);
  flushEndpointDescriptor(edHeadPtr);

  return kIOReturnSuccess;
}

//
// Removes an endpoint from the specified list.
//
IOReturn WiiOHCI::removeEndpoint(UInt8 functionNumber, UInt8 endpointNumber,
                                 OHCIEndpointDescriptor *edHeadPtr, OHCIEndpointDescriptor *edTailPtr) {
  OHCIEndpointDescriptor  *currentEndpointDesc;
  UInt32                  deviceNumber;

  deviceNumber = (functionNumber & kOHCIEDFlagsFuncMask) | (((UInt32)endpointNumber << kOHCIEDFlagsEndpointShift) & kOHCIEDFlagsEndpointMask);
  currentEndpointDesc = edHeadPtr->nextED;
  while (currentEndpointDesc != edTailPtr) {
    //
    // Check if current endpoint descriptor matches.
    //
    if ((USBToHostLong(currentEndpointDesc->ep.flags) & (kOHCIEDFlagsFuncMask | kOHCIEDFlagsEndpointMask)) == deviceNumber) {
      //
      // Remove the endpoint.
      //
      return kIOReturnSuccess;
    }

    currentEndpointDesc = currentEndpointDesc->nextED;
  }

  return kIOUSBEndpointNotFound;
}

//
// Removes and completes all transfer descriptors linked to the specified endpoint descriptor.
//
void WiiOHCI::removeEndpointTransferDescriptors(OHCIEndpointDescriptor *endpointDesc) {
  OHCITransferDescriptor  *currTD;
  OHCITransferDescriptor  *nextTD;
  UInt32                  bufferSizeRemaining;

  //
  // Get the current head and remove entire chain.
  //
  currTD = getTDFromPhysMapping(USBToHostLong(endpointDesc->ep.headTDPhysAddr) & kOHCIEDTDHeadMask);
  endpointDesc->headTD            = endpointDesc->tailTD;
  endpointDesc->ep.headTDPhysAddr = endpointDesc->ep.tailTDPhysAddr;
  flushEndpointDescriptor(endpointDesc);

  //
  // Iterate through chain.
  //
  bufferSizeRemaining = 0;
  while (currTD != endpointDesc->tailTD) {
    if (currTD == NULL) {
      // Shouldn't occur.
      WIISYSLOG("Got an invalid TD here");
      return;
    }

    WIIDBGLOG("Unlinking TD phys 0x%X", currTD->physAddr);

    if (currTD->srcBuffer != NULL) {
      currTD->srcBuffer->release();
    }

    if (currTD->descType == kOHCITransferDescriptorTypeIsochronous) {
      // TODO
    } else {
      //
      // No data actually was transfered, so need to account for all buffers in the chain.
      //
      if (USBToHostLong(currTD->td.currentBufferPtrPhysAddr) != 0) {
        bufferSizeRemaining = 0;
      } else {
        bufferSizeRemaining = USBToHostLong(currTD->td.bufferEndPhysAddr) - USBToHostLong(currTD->td.currentBufferPtrPhysAddr);
      }

      //
      // Invoke completion for final transfer descriptor.
      //
      if (currTD->descFlags & kOHCITransferDescriptorFlagsLastTD) {
        Complete(currTD->completion.gen, kIOReturnAborted, bufferSizeRemaining);
        bufferSizeRemaining = 0;
      }
    }

    nextTD = currTD->nextTD;
    returnTransferDescriptor(currTD);
    currTD = nextTD;
  }
}

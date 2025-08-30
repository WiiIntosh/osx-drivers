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
// Returns the general transfer data from a given physical address.
//
OHCIGenTransferData *WiiOHCI::getGenTransferFromPhys(IOPhysicalAddress physAddr) {
  WiiOHCIGenTransferBuffer  *genTransferBuffer;
  OHCIGenTransferData       *genTransfer;

  //
  // Search general transfer buffers for matching physical address.
  //
  genTransferBuffer = _genTransferBufferHeadPtr;
  while (genTransferBuffer != NULL) {
    genTransfer = genTransferBuffer->getGenTransferFromPhysAddr(physAddr);
    if (genTransfer != NULL) {
      return genTransfer;
    }

    genTransferBuffer = genTransferBuffer->getNextBuffer();
  }

  return NULL;
}

//
// Gets the remaining buffer size, if any.
//
UInt32 WiiOHCI::getGenTransferBufferRemaining(OHCIGenTransferData *genTransfer) {
  if (USBToHostLong(genTransfer->td->currentBufferPtrPhysAddr) == 0) {
    return 0;
  } else {
    return USBToHostLong(genTransfer->td->bufferEndPhysAddr) - USBToHostLong(genTransfer->td->currentBufferPtrPhysAddr) + 1;
  }
}

//
// Allocates and adds a page of new endpoints to the free list.
//
IOReturn WiiOHCI::allocateFreeEndpoints(void) {
  WiiOHCIEndpointBuffer   *endpointBuffer;
  OHCIEndpointData        *endpoint;

  endpointBuffer = WiiOHCIEndpointBuffer::endpointBuffer();
  if (endpointBuffer == NULL) {
    return kIOReturnNoMemory;
  }
  endpointBuffer->setNextBuffer(_endpointBufferHeadPtr);
  _endpointBufferHeadPtr = endpointBuffer;

  //
  // Add the endpoints to the free list.
  //
  for (UInt32 i = 0; i < kWiiOHCIEndpointsPerBuffer; i++) {
    endpoint = endpointBuffer->getEndpoint(i);

    endpoint->nextEndpoint = _freeEndpointHeadPtr;
    _freeEndpointHeadPtr   = endpoint;
  }

  return kIOReturnSuccess;
}

//
// Allocates and adds a page of new general transfers to the free list.
//
IOReturn WiiOHCI::allocateFreeGenTransfers(void) {
  WiiOHCIGenTransferBuffer  *genTransferBuffer;
  OHCIGenTransferData       *genTransfer;

  genTransferBuffer = WiiOHCIGenTransferBuffer::genTransferBuffer();
  if (genTransferBuffer == NULL) {
    return kIOReturnNoMemory;
  }
  genTransferBuffer->setNextBuffer(_genTransferBufferHeadPtr);
  _genTransferBufferHeadPtr = genTransferBuffer;

  //
  // Add the endpoints to the free list.
  //
  for (UInt32 i = 0; i < kWiiOHCIGenTransfersPerBuffer; i++) {
    genTransfer = genTransferBuffer->getGenTransfer(i);

    genTransfer->nextTransfer = _freeGenTransferHeadPtr;
    _freeGenTransferHeadPtr   = genTransfer;
  }

  return kIOReturnSuccess;
}

//
// Gets a free endpoint from the free linked list.
//
OHCIEndpointData *WiiOHCI::getFreeEndpoint(bool isochronous) {
  OHCIEndpointData *endpoint;

  if (_freeEndpointHeadPtr == NULL) {
    if (allocateFreeEndpoints() != kIOReturnSuccess) {
      return NULL;
    }
  }
  endpoint = _freeEndpointHeadPtr;

  //
  // Adjust linkage for remaining free endpoints.
  //
  _freeEndpointHeadPtr   = endpoint->nextEndpoint;
  endpoint->nextEndpoint = NULL;
  endpoint->isochronous  = isochronous;

  return endpoint;
}

//
// Gets a free general transfer from the free linked list.
//
OHCIGenTransferData *WiiOHCI::getFreeGenTransfer(OHCIEndpointData *endpoint) {
  OHCIGenTransferData *transfer;

  if (_freeGenTransferHeadPtr == NULL) {
    if (allocateFreeGenTransfers() != kIOReturnSuccess) {
      return NULL;
    }
  }
  transfer = _freeGenTransferHeadPtr;

  //
  // Adjust linkage for remaining free general transfers.
  //
  _freeGenTransferHeadPtr = transfer->nextTransfer;
  transfer->nextTransfer  = NULL;

  transfer->td->nextTDPhysAddr = 0;
  transfer->nextTransfer       = NULL;
  transfer->bounceBuffer       = NULL;
  transfer->endpoint           = endpoint;

  return transfer;
}

//
// Returns an endpoint to the free linked list.
//
void WiiOHCI::returnEndpoint(OHCIEndpointData *endpoint) {
  //
  // Remove the tail transfer.
  //
  if (endpoint->isochronous) {
    // TODO
  } else {
    returnGenTransfer(endpoint->genTransferTail);
  }

  endpoint->nextEndpoint = _freeEndpointHeadPtr;
  _freeEndpointHeadPtr   = endpoint;
}

//
// Returns a genernal transfer to the free linked list.
//
void WiiOHCI::returnGenTransfer(OHCIGenTransferData *genTransfer) {
  if (genTransfer->bounceBuffer != NULL) {
    returnBounceBuffer(genTransfer->bounceBuffer);
    genTransfer->bounceBuffer = NULL;
  }

  genTransfer->nextTransfer = _freeGenTransferHeadPtr;
  _freeGenTransferHeadPtr   = genTransfer;
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
                                 bool isochronous) {
  OHCIEndpointData    *endpoint;
  OHCIGenTransferData *genTransferTail;
  UInt32              flags;

  //
  // Get a free endpoint.
  //
  endpoint = getFreeEndpoint(isochronous);
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
    | (isochronous ? kOHCIEDFlagsIsochronous: 0);

  if (direction == kUSBOut) {
    flags |= kOHCIEDFlagsDirectionOut;
  } else if (direction == kUSBIn) {
    flags |= kOHCIEDFlagsDirectionIn;
  }
  endpoint->ed->flags = HostToUSBLong(flags);

  //
  // Create initial transfer tail.
  //
  if (isochronous) {

  } else {
    genTransferTail = getFreeGenTransfer(endpoint);
    if (genTransferTail == NULL) {
      returnEndpoint(endpoint);
      return kIOReturnNoMemory;
    }

    //
    // Set new transfer as head and tail to indicate no active transfers.
    //
    endpoint->genTransferHead    = genTransferTail;
    endpoint->genTransferTail    = genTransferTail;
    endpoint->ed->headTDPhysAddr = HostToUSBLong(genTransferTail->physAddr);
    endpoint->ed->tailTDPhysAddr = HostToUSBLong(genTransferTail->physAddr);
  }

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
  OHCIGenTransferData  *currGenTransfer;
  OHCIGenTransferData  *nextGenTransfer;
  UInt32            bufferSizeRemaining;

  WIIDBGLOG("Removing all TDs for endpoint phys: 0x%X", endpoint->physAddr);

  //
  // Get the current head and remove entire chain.
  //
  WIIDBGLOG("TD head phys: 0x%X, tail phys: 0x%X", USBToHostLong(endpoint->ed->headTDPhysAddr), USBToHostLong(endpoint->ed->tailTDPhysAddr));
  if (endpoint->isochronous) {
    // TODO
    return;
  } else {
    currGenTransfer = getGenTransferFromPhys(USBToHostLong(endpoint->ed->headTDPhysAddr) & kOHCIEDTDHeadMask);
  }
  endpoint->genTransferHead    = endpoint->genTransferTail;
  endpoint->ed->headTDPhysAddr = endpoint->ed->tailTDPhysAddr;

  //
  // Iterate through chain.
  //
  bufferSizeRemaining = 0;
  if (endpoint->isochronous) {
    // TODO
    return;
  } else {
    while (currGenTransfer != endpoint->genTransferTail) {
      if (currGenTransfer == NULL) {
        // Shouldn't occur.
        WIISYSLOG("Got an invalid TD here");
        return;
      }

      WIIDBGLOG("Unlinking TD phys 0x%X, next 0x%X, buf %p", currGenTransfer->physAddr,
        USBToHostLong(currGenTransfer->td->nextTDPhysAddr), currGenTransfer->srcBuffer);

      if (currGenTransfer->srcBuffer != NULL) {
        OSSafeReleaseNULL(currGenTransfer->srcBuffer);
      }

      //
      // No data actually was transfered, so need to account for all buffers in the chain.
      //
      if (USBToHostLong(currGenTransfer->td->currentBufferPtrPhysAddr) != 0) {
        bufferSizeRemaining += USBToHostLong(currGenTransfer->td->bufferEndPhysAddr) - USBToHostLong(currGenTransfer->td->currentBufferPtrPhysAddr) + 1;
      }

      //
      // Invoke completion for final transfer.
      //
      if (currGenTransfer->last) {
        Complete(currGenTransfer->completion, kIOReturnAborted, bufferSizeRemaining);
        bufferSizeRemaining = 0;
      }

      nextGenTransfer = getGenTransferFromPhys(USBToHostLong(currGenTransfer->td->nextTDPhysAddr));
      returnGenTransfer(currGenTransfer);
      currGenTransfer = nextGenTransfer;
    }
  }
}

//
// Removes all transfers up until and including one with a completion.
//
void WiiOHCI::completeFailedEndpointGenTransfers(OHCIEndpointData *endpoint, IOReturn tdStatus, UInt32 bufferSizeRemaining) {
  OHCIGenTransferData  *currGenTransfer;
  OHCIGenTransferData  *nextGenTransfer;

  //
  // Mark endpoint as skipped and wait until next frame.
  //
  endpoint->ed->flags |= HostToUSBLong(kOHCIEDFlagsSkip);
  writeReg32(kOHCIRegIntStatus, kOHCIRegIntStatusStartOfFrame);
  while ((readReg32(kOHCIRegIntStatus) & kOHCIRegIntStatusStartOfFrame) == 0) {
    IODelay(10);
  }

  currGenTransfer = getGenTransferFromPhys(USBToHostLong(endpoint->ed->headTDPhysAddr) & kOHCIEDTDHeadMask);
  while (currGenTransfer != endpoint->genTransferTail) {
    if (currGenTransfer == NULL) {
      // Shouldn't occur.
      WIISYSLOG("Got an invalid TD here");
      return;
    }

    //
    // Unlink the transfer descriptor and get the buffer size.
    //
    endpoint->genTransferHead    = currGenTransfer->nextTransfer;
    endpoint->ed->headTDPhysAddr = HostToUSBLong(currGenTransfer->nextTransfer->physAddr) | (endpoint->ed->headTDPhysAddr & ~(HostToUSBLong(kOHCIEDTDHeadMask)));
    bufferSizeRemaining += getGenTransferBufferRemaining(currGenTransfer);

    if (currGenTransfer->srcBuffer != NULL) {
      OSSafeReleaseNULL(currGenTransfer->srcBuffer);
    }

    if (currGenTransfer->last) {
      //
      // For underruns, just pretend it didn't occur.
      //
      if (tdStatus == kIOReturnUnderrun) {
        endpoint->ed->headTDPhysAddr &= ~(HostToUSBLong(kOHCIEDTDHeadHalted));
        tdStatus = kIOReturnSuccess;
      }

      WIIDBGLOG("Completing failed transfer with %u bytes remaining", bufferSizeRemaining);
      endpoint->ed->flags &= ~(HostToUSBLong(kOHCIEDFlagsSkip));
      Complete(currGenTransfer->completion, tdStatus, bufferSizeRemaining);
      returnGenTransfer(currGenTransfer);
      return;
    }

    nextGenTransfer = getGenTransferFromPhys(USBToHostLong(currGenTransfer->td->nextTDPhysAddr));
    returnGenTransfer(currGenTransfer);
    currGenTransfer = nextGenTransfer;
  }
}

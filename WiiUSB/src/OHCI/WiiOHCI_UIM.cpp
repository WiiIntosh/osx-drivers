//
//  WiiOHCI_UIM.cpp
//  Wii OHCI USB controller interface
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include "WiiOHCI.hpp"

//
// Submits a general transfer to be executed by the OHCI controller.
//
// This function is gated and called within the workloop context.
//
IOReturn WiiOHCI::doGeneralTransfer(OHCIEndpointData *endpoint, UInt8 type, IOUSBCompletion completion,
                                    IOMemoryDescriptor *buffer, UInt32 bufferSize, UInt32 flags, UInt32 cmdBits) {
  OHCITransferData  *currTransfer;
  OHCITransferData  *tailTransfer;
  UInt32            bufferRemaining;
  UInt32            tdBufferSize;
  UInt32            offset;

  //
  // Ensure the endpoint is not halted.
  //
  if (USBToHostLong(endpoint->ed->headTDPhysAddr) & kOHCIEDTDHeadHalted) {
    WIISYSLOG("Pipe is stalled");
    return kIOUSBPipeStalled;
  }

  flags &= ~(kOHCIGenTDFlagsConditionCodeMask);
  flags |= ((kOHCITDConditionCodeNotAccessed << kOHCIGenTDFlagsConditionCodeShift) & kOHCIGenTDFlagsConditionCodeMask);

  if (bufferSize > 0) {
    //
    // Create transfer descriptors for buffer.
    //
    offset = 0;
    bufferRemaining = bufferSize;
    while (offset < bufferSize) {
      //
      // Allocate a new tail transfer descriptor.
      //
      tailTransfer = getFreeTransfer(false);
      if (tailTransfer == NULL) {
        return kIOReturnNoMemory;
      }
      tailTransfer->td->nextTDPhysAddr = 0;
      tailTransfer->nextTransfer       = NULL;

      currTransfer = endpoint->tailTransfer;

      tdBufferSize = (bufferRemaining > kWiiOHCITempBufferSize) ? kWiiOHCITempBufferSize : bufferRemaining;
      currTransfer->srcBuffer = IOMemoryDescriptor::withSubRange(buffer, offset, tdBufferSize, buffer->getDirection());
      if (currTransfer->srcBuffer == NULL) {
        WIISYSLOG("Failed to get sub memory descriptor");
        return kIOReturnDMAError;
      }

      //
      // Copy data to bounce buffer if writing to a USB device.
      // This is located in MEM2, MEM1 buffers can work but seem to have issues with non-aligned buffers
      // and buffers not a multiple of 4 on Wii.
      //
      if (currTransfer->srcBuffer->getDirection() & kIODirectionOut) {
        if (currTransfer->srcBuffer->readBytes(0, currTransfer->tmpBufferPtr, tdBufferSize) != tdBufferSize) {
          WIISYSLOG("Failed to copy all bytes into double buffer");
          return kIOReturnDMAError;
        }
      }

      offset          += tdBufferSize;
      bufferRemaining -= tdBufferSize;
      if (offset >= bufferSize) {
        currTransfer->td->flags      = HostToUSBLong(flags);
        currTransfer->completion.gen = completion;
        currTransfer->last           = true;
      } else {
        currTransfer->td->flags      = HostToUSBLong(flags & ~(kOHCIGenTDFlagsBufferRounding));
        currTransfer->last           = false;
      }

      currTransfer->td->currentBufferPtrPhysAddr = HostToUSBLong(currTransfer->tmpBufferPhysAddr);
      currTransfer->td->nextTDPhysAddr           = HostToUSBLong(tailTransfer->physAddr);
      currTransfer->td->bufferEndPhysAddr        = HostToUSBLong(currTransfer->tmpBufferPhysAddr + tdBufferSize - 1);
      currTransfer->actualBufferSize             = tdBufferSize;
      currTransfer->nextTransfer                 = tailTransfer;
      currTransfer->type                         = type;

      WIIDBGLOG("GenTD phys: 0x%X, next 0x%X, buf 0x%X, ep 0x%X, frm 0x%X", currTransfer->physAddr, USBToHostLong(currTransfer->td->nextTDPhysAddr),
        USBToHostLong(currTransfer->td->currentBufferPtrPhysAddr), endpoint->physAddr, readReg32(kOHCIRegFmNumber));

      endpoint->tailTransfer       = tailTransfer;
      endpoint->ed->tailTDPhysAddr = HostToUSBLong(tailTransfer->physAddr);
      writeReg32(kOHCIRegCmdStatus, cmdBits);
    }
  } else {
    //
    // No data to transfer.
    //
    // Allocate a new descriptor.
    //
    tailTransfer = getFreeTransfer(false);
    if (tailTransfer == NULL) {
      WIISYSLOG("Failed to allocate new TD");
      return kIOReturnNoMemory;
    }
    tailTransfer->td->nextTDPhysAddr = 0;
    tailTransfer->nextTransfer       = NULL;

    currTransfer = endpoint->tailTransfer;

    currTransfer->td->flags                    = HostToUSBLong(flags);
    currTransfer->td->currentBufferPtrPhysAddr = 0;
    currTransfer->td->bufferEndPhysAddr        = 0;
    currTransfer->td->nextTDPhysAddr           = HostToUSBLong(tailTransfer->physAddr);
    currTransfer->actualBufferSize             = 0;
    currTransfer->srcBuffer                    = NULL;
    currTransfer->completion.gen               = completion;
    currTransfer->nextTransfer                 = tailTransfer;
    currTransfer->type                         = type;
    currTransfer->last                         = true;
  
    WIIDBGLOG("Added non-data gen TD phys 0x%X, next 0x%X", currTransfer->physAddr, USBToHostLong(currTransfer->td->nextTDPhysAddr));

    endpoint->tailTransfer       = tailTransfer;
    endpoint->ed->tailTDPhysAddr = HostToUSBLong(tailTransfer->physAddr);
    writeReg32(kOHCIRegCmdStatus, cmdBits);
  }

  return kIOReturnSuccess;
}

//
// Walks the completed transfer descriptor queue and completes each one.
//
// This function is gated and called within the workloop context.
//
void WiiOHCI::completeTransferQueue(UInt32 headPhysAddr) {
  OHCITransferData  *currTransfer;
  OHCITransferData  *nextTransfer;
  UInt32            transferStatus;
  UInt32            bufferSizeRemaining;
  IOReturn          tdStatus;

  //
  // Verify there actually is a queue.
  //
  if (headPhysAddr == 0) {
    return;
  }

  WIIDBGLOG("Head done: 0x%X", headPhysAddr);
  currTransfer = getTDFromPhysMapping(headPhysAddr);
  if (currTransfer == NULL) {
    WIISYSLOG("head TD is zero");
    return;
  }

  while (currTransfer != NULL) {
    transferStatus = (USBToHostLong(currTransfer->td->flags) & kOHCIGenTDFlagsConditionCodeMask) >> kOHCIGenTDFlagsConditionCodeShift;
    tdStatus = convertTDStatus(transferStatus);
    WIIDBGLOG("TD phys 0x%X, next 0x%X, stat: 0x%X, 0x%X", currTransfer->physAddr, USBToHostLong(currTransfer->td->nextTDPhysAddr), transferStatus, tdStatus);

    if (tdStatus != kIOReturnSuccess) {
      WIISYSLOG("Got an error here: 0x%X", tdStatus);
    }

    if (currTransfer->type == kOHCITransferTypeIsochronous) {
      // TODO
    } else {
      //
      // Copy data back into original buffer.
      //
      if (currTransfer->srcBuffer != NULL) {
        if (currTransfer->srcBuffer->getDirection() & kIODirectionIn) {
          currTransfer->srcBuffer->writeBytes(0, currTransfer->tmpBufferPtr, currTransfer->actualBufferSize);
        }
        OSSafeReleaseNULL(currTransfer->srcBuffer);
      }

      //
      // Invoke completion if present.
      //
      if (currTransfer->last) {
        if (USBToHostLong(currTransfer->td->currentBufferPtrPhysAddr) == 0) {
          bufferSizeRemaining = 0;
        } else {
          bufferSizeRemaining = USBToHostLong(currTransfer->td->bufferEndPhysAddr) - USBToHostLong(currTransfer->td->currentBufferPtrPhysAddr);
        }

        if (currTransfer->actualBufferSize != 0) {
          WIIDBGLOG("Completing a transfer %u bytes (%u bytes left), end 0x%X",
            currTransfer->actualBufferSize, bufferSizeRemaining, USBToHostLong(currTransfer->td->bufferEndPhysAddr));
        } else {
          WIIDBGLOG("Completing a transfer with no data");
        }

        Complete(currTransfer->completion.gen, tdStatus, bufferSizeRemaining);
      } else {
        WIIDBGLOG("No completion");
      }

      nextTransfer = getTDFromPhysMapping(USBToHostLong(currTransfer->td->nextTDPhysAddr));
    }

    returnTransfer(currTransfer);
    currTransfer = nextTransfer;
  }
}

//
// Overrides IOUSBController::UIMCreateControlEndpoint().
//
// Creates a control endpoint.
// Called from IOUSBController::DoCreateEP().
//
// This function is gated and called within the workloop context.
//
IOReturn WiiOHCI::UIMCreateControlEndpoint(UInt8 functionNumber, UInt8 endpointNumber, UInt16 maxPacketSize, UInt8 speed) {
  WIIDBGLOG("F: %d, EP: %u, spd: %s, psz: %u", functionNumber, endpointNumber,
    (speed == kUSBDeviceSpeedFull) ? "full" : "low", maxPacketSize);

  //
  // Simulate root hub control endpoint creation.
  //
  if (functionNumber == _rootHubAddress) {
    return simulateRootHubControlEDCreate(endpointNumber, maxPacketSize, speed);
  }

  //
  // Add a new control endpoint.
  //
  return addNewEndpoint(functionNumber, endpointNumber, maxPacketSize, speed, kUSBAnyDirn, _controlEndpointHeadPtr);
}

//
// Overrides IOUSBController::UIMCreateControlTransfer() (void* version).
//
// Executes a USB control transfer.
// Called from IOUSBController::ControlTransaction().
//
// This function is gated and called within the workloop context.
//
IOReturn WiiOHCI::UIMCreateControlTransfer(short functionNumber, short endpointNumber, IOUSBCompletion completion, void *CBP,
                                           bool bufferRounding, UInt32 bufferSize, short direction) {
  IOMemoryDescriptor    *desc = NULL;
  IODirection           descDirection;
  IOReturn              status;

  if (direction == kUSBOut) {
    descDirection = kIODirectionOut;
  }
  else if (direction == kUSBIn) {
    descDirection = kIODirectionIn;
  }
  else {
    descDirection = kIODirectionOut;
  }

  //
  // Create memory descriptor for other function type.
  //
  if (bufferSize != 0) {
    desc = IOMemoryDescriptor::withAddress(CBP, bufferSize, descDirection);
    if (desc == NULL) {
      return kIOReturnNoMemory;
    }
  }

  status = UIMCreateControlTransfer(functionNumber, endpointNumber, completion, desc, bufferRounding, bufferSize, direction);

  if (desc != NULL) {
    desc->release();
  }
  return status;
}

//
// Overrides IOUSBController::UIMCreateControlTransfer() (IOMemoryDescriptor* version).
//
// Executes a USB control transfer.
// Called from IOUSBController::ControlTransaction().
//
// This function is gated and called within the workloop context.
//
IOReturn WiiOHCI::UIMCreateControlTransfer(short functionNumber, short endpointNumber, IOUSBCompletion completion,
                                           IOMemoryDescriptor *CBP, bool bufferRounding, UInt32 bufferSize, short direction) {
  OHCIEndpointData  *endpoint;
  UInt8             endpointType;
  UInt32            flags;

  WIIDBGLOG("F: %d, EP: %u, dir: %d, sz: %u", functionNumber, endpointNumber, direction, bufferSize);

  //
  // Locate the control endpoint.
  //
  endpointType = kWiiOHCIEndpointTypeControl;
  endpoint     = getEndpoint(functionNumber, endpointNumber, direction, &endpointType);
  if (endpoint == NULL) {
    WIIDBGLOG("Endpoint not found");
    return kIOUSBEndpointNotFound;
  }

  //
  // SETUP should have only bit 1 on.
  // DATA will have bit 0 as well, alternating on and off for additional data packets.
  // STATUS will always have bit 1 and 0.
  //
  flags = kOHCIGenTDFlagsDataToggleData1;
  if (direction == kUSBOut) {
    flags |= kOHCIGenTDFlagsDirectionOut | kOHCIGenTDFlagsDataToggleData0;
  } else if (direction == kUSBIn) {
    flags |= kOHCIGenTDFlagsDirectionIn | kOHCIGenTDFlagsDataToggleData0;
  } else {
    flags |= kOHCIGenTDFlagsDirectionSetup;
  }
  if (bufferRounding) {
    flags |= kOHCIGenTDFlagsBufferRounding;
  }

  //
  // Submit the control transfer.
  //
  return doGeneralTransfer(endpoint, kOHCITransferTypeControl, completion,
    CBP, bufferSize, flags, kOHCIRegCmdStatusControlListFilled);
}

//
// Overrides IOUSBController::UIMCreateBulkEndpoint().
//
// Creates a bulk endpoint.
// Called from IOUSBController::DoCreateEP().
//
// This function is gated and called within the workloop context.
//
IOReturn WiiOHCI::UIMCreateBulkEndpoint(UInt8 functionNumber, UInt8 endpointNumber, UInt8 direction, UInt8 speed, UInt8 maxPacketSize) {
  WIIDBGLOG("F: %d, EP: %u, spd: %s, psz: %u", functionNumber, endpointNumber,
    (speed == kUSBDeviceSpeedFull) ? "full" : "low", maxPacketSize);

  //
  // Add a new bulk endpoint.
  //
  return addNewEndpoint(functionNumber, endpointNumber, maxPacketSize, speed, kUSBAnyDirn, _bulkEndpointHeadPtr);
}

//
// Overrides IOUSBController::UIMCreateBulkTransfer().
//
// Executes a USB bulk transfer.
// Called from IOUSBController::BulkTransaction().
//
// This function is gated and called within the workloop context.
//
IOReturn WiiOHCI::UIMCreateBulkTransfer(short functionNumber, short endpointNumber, IOUSBCompletion completion,
                                        IOMemoryDescriptor *CBP, bool bufferRounding, UInt32 bufferSize, short direction) {
  OHCIEndpointData  *endpoint;
  UInt8             endpointType;
  UInt32            flags;

  WIIDBGLOG("F: %d, EP: %u, dir: %d, sz: %u", functionNumber, endpointNumber, direction, bufferSize);

  //
  // Locate the bulk endpoint.
  //
  endpointType = kWiiOHCIEndpointTypeBulk;
  endpoint     = getEndpoint(functionNumber, endpointNumber, direction, &endpointType);
  if (endpoint == NULL) {
    WIIDBGLOG("Endpoint not found");
    return kIOUSBEndpointNotFound;
  }

  flags = 0;
  if (direction == kUSBOut) {
    flags |= kOHCIGenTDFlagsDirectionOut;
  } else if (direction == kUSBIn) {
    flags |= kOHCIGenTDFlagsDirectionIn;
  } else {
    flags |= kOHCIGenTDFlagsDirectionSetup;
  }
  if (bufferRounding) {
    flags |= kOHCIGenTDFlagsBufferRounding;
  }

  //
  // Submit the bulk transfer.
  //
  return doGeneralTransfer(endpoint, kOHCITransferTypeBulk, completion,
    CBP, bufferSize, flags, kOHCIRegCmdStatusBulkListFilled);
}

//
// Overrides IOUSBController::UIMCreateInterruptEndpoint().
//
// Creates an interrupt endpoint.
// Called from IOUSBController::DoCreateEP().
//
// This function is gated and called within the workloop context.
//
IOReturn WiiOHCI::UIMCreateInterruptEndpoint(short functionAddress, short endpointNumber, UInt8 direction,
                                             short speed, UInt16 maxPacketSize, short pollingRate) {
  OHCIEndpointData  *endpointIntHead;

  WIIDBGLOG("F: %d, EP: %u, dir: %d, spd: %s, sz: %u, pr: %d", functionAddress, endpointNumber, direction,
    (speed == kUSBDeviceSpeedFull) ? "full" : "low", maxPacketSize, pollingRate);

  //
  // Simulate root hub interrupt endpoint creation.
  //
  if (functionAddress == _rootHubAddress) {
    return simulateRootHubInterruptEDCreate(endpointNumber, direction, speed, maxPacketSize);
  }

  //
  // Get the correct head for the desired polling rate.
  //
  endpointIntHead = getInterruptEndpointHead(pollingRate);
  if (endpointIntHead == NULL) {
    return kIOReturnNoBandwidth;
  }

  //
  // Create an endpoint linked to the desired polling rate head.
  //
  return addNewEndpoint(functionAddress, endpointNumber, maxPacketSize, speed, direction, endpointIntHead);
}

//
// Overrides IOUSBController::UIMCreateInterruptTransfer().
//
// Executes a USB interrupt transfer.
// Called from IOUSBController::InterruptTransaction().
//
// This function is gated and called within the workloop context.
//
IOReturn WiiOHCI::UIMCreateInterruptTransfer(short functionNumber, short endpointNumber, IOUSBCompletion completion,
                                             IOMemoryDescriptor *CBP, bool bufferRounding, UInt32 bufferSize, short direction) {
  OHCIEndpointData  *endpoint;
  UInt8             endpointType;
  UInt32            flags;

  WIIDBGLOG("F: %d, EP: %u, dir: %d, sz: %u", functionNumber, endpointNumber, direction, bufferSize);

  //
  // Simulate root hub interrupt transfer.
  //
  if (functionNumber == _rootHubAddress) {
    simulateRootHubInterruptTransfer(endpointNumber, completion, CBP, bufferRounding, bufferSize, direction);
    return kIOReturnSuccess;
  }

  //
  // Locate the interrupt endpoint.
  //
  endpointType = kWiiOHCIEndpointTypeInterrupt;
  endpoint     = getEndpoint(functionNumber, endpointNumber, direction, &endpointType);
  if (endpoint == NULL) {
    return kIOUSBEndpointNotFound;
  }

  if (direction == kUSBOut) {
    flags = kOHCIGenTDFlagsDirectionOut;
  } else if (direction == kUSBIn) {
    flags = kOHCIGenTDFlagsDirectionIn;
  } else {
    flags = kOHCIGenTDFlagsDirectionSetup;
  }
  if (bufferRounding) {
    flags |= kOHCIGenTDFlagsBufferRounding;
  }

  return doGeneralTransfer(endpoint, kOHCITransferTypeInterrupt, completion, CBP, bufferSize, flags, 0);
}

//
// Overrides IOUSBController::UIMCreateIsochEndpoint().
//
// Creates an isochronous endpoint.
// Called from IOUSBController::DoCreateEP().
//
// This function is gated and called within the workloop context.
//
IOReturn WiiOHCI::UIMCreateIsochEndpoint(short functionAddress, short endpointNumber, UInt32 maxPacketSize, UInt8 direction) {
  WIIDBGLOG("start");
  return kIOReturnUnsupported;
}

//
// Overrides IOUSBController::UIMCreateIsochTransfer().
//
// Executes a USB isochronous transfer.
// Called from IOUSBController::IsocTransaction().
//
// This function is gated and called within the workloop context.
//
IOReturn WiiOHCI::UIMCreateIsochTransfer(short functionAddress, short endpointNumber, IOUSBIsocCompletion completion, UInt8 direction,
                                         UInt64 frameStart, IOMemoryDescriptor *pBuffer, UInt32 frameCount, IOUSBIsocFrame *pFrames) {
  WIIDBGLOG("start");
  return kIOReturnUnsupported;
}

//
// Overrides IOUSBController::UIMAbortEndpoint().
//
// Removes any transfers for an endpoint and resets any stall conditions.
// Called from IOUSBController::DoAbortEP().
//
// This function is gated and called within the workloop context.
//
IOReturn WiiOHCI::UIMAbortEndpoint(short functionNumber, short endpointNumber, short direction) {
  OHCIEndpointData  *endpoint;
  UInt8             endpointType;

  WIIDBGLOG("F: %d, EP: %d, dir: %d", functionNumber, endpointNumber, direction);

  //
  // Locate the endpoint.
  //
  endpointType = kWiiOHCIEndpointTypeAll;
  endpoint     = getEndpoint(functionNumber, endpointNumber, direction, &endpointType);
  if (endpoint == NULL) {
    return kIOUSBEndpointNotFound;
  }
  WIIDBGLOG("Aborting EP phys: 0x%X", endpoint->physAddr);

  //
  // Mark endpoint as skipped and wait until next frame.
  //
  endpoint->ed->flags |= HostToUSBLong(kOHCIEDFlagsSkip);
  writeReg32(kOHCIRegIntStatus, kOHCIRegIntStatusStartOfFrame);
  while ((readReg32(kOHCIRegIntStatus) & kOHCIRegIntStatusStartOfFrame) == 0) {
    IODelay(10);
  }

  //
  // Remove all transfers associated with endpoint and re-activate the endpoint.
  //
  removeEndpointTransfers(endpoint);
  endpoint->ed->flags &= ~(HostToUSBLong(kOHCIEDFlagsSkip));

  return kIOReturnSuccess;
}

//
// Overrides IOUSBController::UIMDeleteEndpoint().
//
// Removes an endpoint for the list of active endpoints.
// Called from IOUSBController::DoDeleteEP().
//
// This function is gated and called within the workloop context.
//
IOReturn WiiOHCI::UIMDeleteEndpoint(short functionNumber, short endpointNumber, short direction) {
  OHCIEndpointData  *endpoint;
  OHCIEndpointData  *prevEndpoint;
  UInt8             endpointType;
  UInt32            listMask;

  WIIDBGLOG("F: %d, EP: %d, dir: %d", functionNumber, endpointNumber, direction);

  //
  // Locate the endpoint.
  //
  endpointType = kWiiOHCIEndpointTypeAll;
  endpoint     = getEndpoint(functionNumber, endpointNumber, direction, &endpointType, &prevEndpoint);
  if (endpoint == NULL) {
    return kIOUSBEndpointNotFound;
  }
  WIIDBGLOG("Deleting EP phys: 0x%X, previous EP phys: 0x%X, type: 0x%X", endpoint->physAddr, prevEndpoint->physAddr, endpointType);

  switch (endpointType) {
    case kWiiOHCIEndpointTypeControl:
      listMask = kOHCIRegControlControlListEnable;
      break;

    case kWiiOHCIEndpointTypeInterrupt:
      listMask = kOHCIRegControlPeriodicListEnable;
      break;

    case kWiiOHCIEndpointTypeBulk:
      listMask = kOHCIRegControlBulkListEnable;
      break;

    case kWiiOHCIEndpointTypeIsochronous:
      listMask = kOHCIRegControlIsochronousEnable;
      break;

    default:
      return kIOReturnBadArgument;
  }

  //
  // Mark endpoint as skipped and wait until next frame.
  //
  endpoint->ed->flags |= HostToUSBLong(kOHCIEDFlagsSkip);
  writeReg32(kOHCIRegIntStatus, kOHCIRegIntStatusStartOfFrame);
  while ((readReg32(kOHCIRegIntStatus) & kOHCIRegIntStatusStartOfFrame) == 0) {
    IODelay(10);
  }

  //
  // Stop processing of endpoints.
  //
  writeReg32(kOHCIRegControl, readReg32(kOHCIRegControl) & ~(listMask));
  writeReg32(kOHCIRegIntStatus, kOHCIRegIntStatusStartOfFrame);
  while ((readReg32(kOHCIRegIntStatus) & kOHCIRegIntStatusStartOfFrame) == 0) {
    IODelay(10);
  }

  //
  // Remove endpoint from linked list and resume endpoint processing.
  //
  prevEndpoint->nextEndpoint       = endpoint->nextEndpoint;
  prevEndpoint->ed->nextEDPhysAddr = endpoint->ed->nextEDPhysAddr;
  writeReg32(kOHCIRegControl, readReg32(kOHCIRegControl) | listMask);
  WIIDBGLOG("Unlinked EP phys: 0x%X", endpoint->physAddr);

  //
  // Remove all transfers associated with endpoint and remove the endpoint.
  //
  removeEndpointTransfers(endpoint);
  returnEndpoint(endpoint);

  return kIOReturnSuccess;
}

//
// Overrides IOUSBController::UIMClearEndpointStall().
//
// Clears all pending transfers and the stall condition from an endpoint.
// Called from IOUSBController::DoClearEPStall() and others.
//
// This function is gated and called within the workloop context.
//
IOReturn WiiOHCI::UIMClearEndpointStall(short functionNumber, short endpointNumber, short direction) {
  OHCIEndpointData  *endpoint;
  UInt8             endpointType;

  WIIDBGLOG("F: %d, EP: %d, dir: %d", functionNumber, endpointNumber, direction);

  //
  // Locate the endpoint.
  //
  endpointType = kWiiOHCIEndpointTypeAll;
  endpoint     = getEndpoint(functionNumber, endpointNumber, direction, &endpointType);
  if (endpoint == NULL) {
    return kIOUSBEndpointNotFound;
  }
  WIIDBGLOG("Clearing EP phys: 0x%X, type 0%X", endpoint->physAddr, endpointType);

  //
  // Reset the transfer queue by unlinking all transfers.
  // This will also clear the current stall bit on the endpoint.
  //
  removeEndpointTransfers(endpoint);

  return kIOReturnSuccess;
}

void WiiOHCI::UIMRootHubStatusChange(void) {
WIIDBGLOG("start");
}

void WiiOHCI::UIMRootHubStatusChange(bool abort) {
WIIDBGLOG("start");
}

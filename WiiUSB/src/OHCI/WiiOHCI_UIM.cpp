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
IOReturn WiiOHCI::doGeneralTransfer(OHCIEndpointData *endpoint, IOUSBCompletion completion,
                                    IOMemoryDescriptor *buffer, UInt32 bufferSize, UInt32 flags, UInt32 cmdBits) {
  OHCIGenTransferData   *genTransferCurr;
  OHCIGenTransferData   *genTransferTail;
  UInt32                bufferRemaining;
  UInt32                transferSize;
  UInt32                offset;

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
    // Create general transfers for buffer.
    //
    offset = 0;
    bufferRemaining = bufferSize;
    while (offset < bufferSize) {
      //
      // Allocate a new tail general transfer.
      //
      genTransferTail = getFreeGenTransfer(endpoint);
      if (genTransferTail == NULL) {
        return kIOReturnNoMemory;
      }
      genTransferCurr = endpoint->genTransferTail;

      //
      // Get a bounce buffer.
      //
      genTransferCurr->bounceBuffer = getFreeBounceBuffer(bufferRemaining > kWiiOHCIBounceBufferSize);

      if (genTransferCurr->bounceBuffer->jumbo) {
        transferSize = (bufferRemaining > kWiiOHCIBounceBufferJumboSize) ? kWiiOHCIBounceBufferJumboSize : bufferRemaining;
      } else {
        transferSize = (bufferRemaining > kWiiOHCIBounceBufferSize) ? kWiiOHCIBounceBufferSize : bufferRemaining;
      }
      genTransferCurr->srcBuffer = IOMemoryDescriptor::withSubRange(buffer, offset, transferSize, buffer->getDirection());
      if (genTransferCurr->srcBuffer == NULL) {
        WIISYSLOG("Failed to get sub memory descriptor");
        return kIOReturnDMAError;
      }

      //
      // Copy data to bounce buffer if writing to a USB device.
      // This is located in MEM2, MEM1 buffers can work but seem to have issues with non-aligned buffers
      // and buffers not a multiple of 4 on Wii.
      //
      if (genTransferCurr->srcBuffer->getDirection() & kIODirectionOut) {
        if (genTransferCurr->srcBuffer->readBytes(0, genTransferCurr->bounceBuffer->buf, transferSize) != transferSize) {
          WIISYSLOG("Failed to copy all bytes into bounce buffer");
          return kIOReturnDMAError;
        }
        flushDataCache(genTransferCurr->bounceBuffer->buf, transferSize);
      }

      offset          += transferSize;
      bufferRemaining -= transferSize;
      if (offset >= bufferSize) {
        genTransferCurr->td->flags  = HostToUSBLong(flags);
        genTransferCurr->completion = completion;
        genTransferCurr->last       = true;
      } else {
        genTransferCurr->td->flags  = HostToUSBLong(flags & ~(kOHCIGenTDFlagsBufferRounding));
        genTransferCurr->last       = false;
      }

      genTransferCurr->td->currentBufferPtrPhysAddr = HostToUSBLong(genTransferCurr->bounceBuffer->physAddr);
      genTransferCurr->td->nextTDPhysAddr           = HostToUSBLong(genTransferTail->physAddr);
      genTransferCurr->td->bufferEndPhysAddr        = HostToUSBLong(genTransferCurr->bounceBuffer->physAddr + transferSize - 1);
      genTransferCurr->actualBufferSize             = transferSize;
      genTransferCurr->nextTransfer                 = genTransferTail;

      WIIDBGLOG("GenTD phys: 0x%X, next 0x%X, buf 0x%X, ep 0x%X, frm 0x%X", genTransferCurr->physAddr, USBToHostLong(genTransferCurr->td->nextTDPhysAddr),
        USBToHostLong(genTransferCurr->td->currentBufferPtrPhysAddr), endpoint->physAddr, readReg32(kOHCIRegFmNumber));

      endpoint->genTransferTail    = genTransferTail;
      endpoint->ed->tailTDPhysAddr = HostToUSBLong(genTransferTail->physAddr);
      writeReg32(kOHCIRegCmdStatus, cmdBits);
    }
  } else {
    //
    // No data to transfer.
    //
    // Allocate a new general transfer.
    //
    genTransferTail = getFreeGenTransfer(endpoint);
    if (genTransferTail == NULL) {
      WIISYSLOG("Failed to allocate new TD");
      return kIOReturnNoMemory;
    }
    genTransferCurr = endpoint->genTransferTail;

    genTransferCurr->td->flags                    = HostToUSBLong(flags);
    genTransferCurr->td->currentBufferPtrPhysAddr = 0;
    genTransferCurr->td->bufferEndPhysAddr        = 0;
    genTransferCurr->td->nextTDPhysAddr           = HostToUSBLong(genTransferTail->physAddr);
    genTransferCurr->bounceBuffer                 = NULL;
    genTransferCurr->actualBufferSize             = 0;
    genTransferCurr->srcBuffer                    = NULL;
    genTransferCurr->completion                   = completion;
    genTransferCurr->nextTransfer                 = genTransferTail;
    genTransferCurr->last                         = true;

    WIIDBGLOG("Added non-data gen TD phys 0x%X, next 0x%X", genTransferCurr->physAddr, USBToHostLong(genTransferCurr->td->nextTDPhysAddr));

    endpoint->genTransferTail    = genTransferTail;
    endpoint->ed->tailTDPhysAddr = HostToUSBLong(genTransferTail->physAddr);
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
  OHCIGenTransferData   *genTransferCurr;
  OHCIGenTransferData   *genTransferNext;
  UInt32                transferStatus;
  UInt32                bufferSizeRemaining;
  IOReturn              tdStatus;

  //
  // Verify there actually is a queue.
  //
  if (headPhysAddr == 0) {
    return;
  }

  WIIDBGLOG("Head done: 0x%X", headPhysAddr);
  genTransferCurr = getGenTransferFromPhys(headPhysAddr);
  if (genTransferCurr == NULL) {
    WIISYSLOG("head TD is NULL"); // TODO: Handle the isochronous ones.
    while (true);
    return;
  }

  while (genTransferCurr != NULL) {
    transferStatus = (USBToHostLong(genTransferCurr->td->flags) & kOHCIGenTDFlagsConditionCodeMask) >> kOHCIGenTDFlagsConditionCodeShift;
    tdStatus = convertTDStatus(transferStatus);
    WIIDBGLOG("TD phys 0x%X, next 0x%X, stat: 0x%X, 0x%X", genTransferCurr->physAddr, USBToHostLong(genTransferCurr->td->nextTDPhysAddr), transferStatus, tdStatus);

    bufferSizeRemaining = getGenTransferBufferRemaining(genTransferCurr);
    WIIDBGLOG("Transferred %u bytes (%u bytes left), ptr 0x%X end 0x%X",
      genTransferCurr->actualBufferSize - bufferSizeRemaining, bufferSizeRemaining,
      USBToHostLong(genTransferCurr->td->currentBufferPtrPhysAddr), USBToHostLong(genTransferCurr->td->bufferEndPhysAddr));

    //
    // Copy data back into original buffer if this was a read, and only if we actually transfered data.
    //
    if (genTransferCurr->srcBuffer != NULL) {
      if (genTransferCurr->srcBuffer->getDirection() & kIODirectionIn) {
        _invalidateCacheFunc((vm_offset_t) genTransferCurr->bounceBuffer->buf, genTransferCurr->actualBufferSize, false);
        if ((genTransferCurr->actualBufferSize - bufferSizeRemaining) > 0) {
          genTransferCurr->srcBuffer->writeBytes(0, genTransferCurr->bounceBuffer->buf, genTransferCurr->actualBufferSize - bufferSizeRemaining);
        }
      }
      OSSafeReleaseNULL(genTransferCurr->srcBuffer);
      UInt32 *buf = (UInt32*)genTransferCurr->bounceBuffer->buf;
      WIIDBGLOG("%08X %08X %08X %08X %08X %08X %08X %08X",
        USBToHostLong(buf[0]), USBToHostLong(buf[1]), USBToHostLong(buf[2]), USBToHostLong(buf[3]),
        USBToHostLong(buf[4]), USBToHostLong(buf[5]), USBToHostLong(buf[6]), USBToHostLong(buf[7]));
    }

    if (tdStatus != kIOReturnSuccess) {
      WIISYSLOG("Got an error here: 0x%X", tdStatus);
    }

    //
    // Invoke completion if present.
    //
    if (genTransferCurr->last) {
      WIIDBGLOG("Calling completion");
      Complete(genTransferCurr->completion, tdStatus, bufferSizeRemaining);
    } else {
      WIIDBGLOG("No completion");

      //
      // If there was an error, need to finish the rest of the chain.
      //
      if (tdStatus != kIOReturnSuccess) {
        WIIDBGLOG("Completing short packet");
        completeFailedEndpointGenTransfers(genTransferCurr->endpoint, tdStatus, bufferSizeRemaining);
      }
    }

    genTransferNext = getGenTransferFromPhys(USBToHostLong(genTransferCurr->td->nextTDPhysAddr));
    returnGenTransfer(genTransferCurr);
    genTransferCurr = genTransferNext;
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
  return doGeneralTransfer(endpoint, completion, CBP, bufferSize, flags, kOHCIRegCmdStatusControlListFilled);
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
  return doGeneralTransfer(endpoint, completion, CBP, bufferSize, flags, kOHCIRegCmdStatusBulkListFilled);
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

  return doGeneralTransfer(endpoint, completion, CBP, bufferSize, flags, 0);
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

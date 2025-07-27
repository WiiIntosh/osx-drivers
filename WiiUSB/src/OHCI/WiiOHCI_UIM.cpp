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
IOReturn WiiOHCI::doGeneralTransfer(OHCIEndpointDescriptor *endpointDesc, UInt8 type, IOUSBCompletion completion,
                                    IOMemoryDescriptor *buffer, UInt32 bufferSize, UInt32 flags, UInt32 cmdBits) {
  OHCITransferDescriptor    *currTD;
  OHCITransferDescriptor    *newTailTD;
  IOPhysicalSegment         seg;
  IOByteCount               offset;

  //
  // Ensure the endpoint is not halted.
  //
  invalidateDataCache(&endpointDesc->ep.headTDPhysAddr, sizeof (endpointDesc->ep.headTDPhysAddr));
  if (USBToHostLong(endpointDesc->ep.headTDPhysAddr) & kOHCIEDTDHeadHalted) {
    WIIDBGLOG("Pipe is stalled");
    return kIOUSBPipeStalled;
  }

  flags &= ~(kOHCIGenTDFlagsConditionCodeMask);
  flags |= ((kOHCITDConditionCodeNotAccessed << kOHCIGenTDFlagsConditionCodeShift) & kOHCIGenTDFlagsConditionCodeMask);

  if (bufferSize > 0) {
    //
    // TODO:
    //
    if (bufferSize > kWiiOHCITempBufferSize) {
      WIISYSLOG("TODO too big a transfer 0x%X", bufferSize);
      while (true);
    }

    //
    // Create transfer descriptors for buffer.
    //
    offset = 0;
    while (offset < bufferSize) {
      //
      // Allocate a new tail transfer descriptor.
      //
      newTailTD = getFreeTransferDescriptor();
      if (newTailTD == NULL) {
        WIIDBGLOG("Failed to allocate new TD");
        return kIOReturnNoMemory;
      }
      newTailTD->td.nextTDPhysAddr = 0;
      newTailTD->nextTD            = NULL;
      flushTransferDescriptor(newTailTD);

      //
      // Populate the current tail transfer descriptor.
      //
      currTD = endpointDesc->tailTD;

      if (_memoryCursor->getPhysicalSegments(currTD->tmpBuffer, 0, &seg, 1, (bufferSize > kWiiOHCITempBufferSize) ? kWiiOHCITempBufferSize : bufferSize) != 1) {
        WIIDBGLOG("DMA error");
        return kIOReturnDMAError;
      }

      currTD->srcBuffer = IOMemoryDescriptor::withSubRange(buffer, offset, seg.length, kIODirectionInOut);
      if (currTD->srcBuffer == NULL) {
        WIIDBGLOG("DMA failed to make the subdesc");
        return kIOReturnDMAError;
      }

      //
      // Copy data to scratch descriptor.
      // This is located in MEM2, MEM1 buffers can work but seem to have issues with non-aligned buffers and buffers not a multiple of 4.
      //
      if (currTD->srcBuffer->readBytes(0, currTD->tmpBufferPtr, seg.length) != seg.length) {
        WIIDBGLOG("DMA didn't copy them all");
        return kIOReturnDMAError;
      }
      flushDataCache(currTD->tmpBufferPtr, kWiiOHCITempBufferSize);
      
      offset += seg.length;
      if (offset >= bufferSize) {
        currTD->td.flags       = HostToUSBLong(flags);
        currTD->completion.gen = completion;
      } else {
        currTD->td.flags = HostToUSBLong(flags & ~(kOHCIGenTDFlagsBufferRounding));
        bzero(&currTD->completion.gen, sizeof (currTD->completion.gen));
      }

      currTD->td.currentBufferPtrPhysAddr = HostToUSBLong(seg.location);
      currTD->td.nextTDPhysAddr           = HostToUSBLong(newTailTD->physAddr);
      currTD->td.bufferEndPhysAddr        = HostToUSBLong(seg.location + seg.length - 1);
      currTD->bufferSize                  = seg.length;
      currTD->nextTD                      = newTailTD;
      currTD->descType                    = type;
      flushTransferDescriptor(currTD);

      //
      // Make the newly allocated tail transfer descriptor the new tail,
      // indicating to the host controller there is work to be done.
      //
      endpointDesc->tailTD            = newTailTD;
      endpointDesc->ep.tailTDPhysAddr = HostToUSBLong(newTailTD->physAddr);
      flushDataCache(&endpointDesc->ep.tailTDPhysAddr, sizeof (endpointDesc->ep.tailTDPhysAddr));
      WIIDBGLOG("Added gen TD phys 0x%X, next 0x%X, buf 0x%X, fr 0x%X", currTD->physAddr, newTailTD->physAddr, seg.location, readReg32(kOHCIRegFmNumber));
      writeReg32(kOHCIRegCmdStatus, cmdBits);
    }
  } else {
    //
    // No data to transfer.
    //
    // Allocate a new descriptor.
    //
    newTailTD = getFreeTransferDescriptor();
    if (newTailTD == NULL) {
      WIIDBGLOG("Failed to allocate new TD");
      return kIOReturnNoMemory;
    }
    newTailTD->td.nextTDPhysAddr = 0;
    newTailTD->nextTD            = NULL;
    flushTransferDescriptor(newTailTD);

    currTD = endpointDesc->tailTD;

    currTD->td.flags                    = HostToUSBLong(flags);
    currTD->td.currentBufferPtrPhysAddr = 0;
    currTD->td.bufferEndPhysAddr        = 0;
    currTD->td.nextTDPhysAddr           = HostToUSBLong(newTailTD->physAddr);
    currTD->bufferSize                  = 0;
    currTD->completion.gen              = completion;
    currTD->nextTD                      = newTailTD;
    currTD->descType                    = type;
    flushTransferDescriptor(currTD);

    //
    // Make the newly allocated tail transfer descriptor the new tail,
    // indicating to the host controller there is work to be done.
    //
    endpointDesc->tailTD            = newTailTD;
    endpointDesc->ep.tailTDPhysAddr = HostToUSBLong(newTailTD->physAddr);
    flushDataCache(&endpointDesc->ep.tailTDPhysAddr, sizeof (endpointDesc->ep.tailTDPhysAddr));
    WIIDBGLOG("Added non-data gen TD phys 0x%X, next 0x%X", currTD->physAddr, newTailTD->physAddr);
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
  OHCITransferDescriptor  *headDoneTD;
  OHCITransferDescriptor  *currTD;
  OHCITransferDescriptor  *nextTD;
  UInt32                  transferStatus;
  //UInt32                  bufferSize;
  UInt32                  bufferSizeRemaining;
  IOReturn                tdStatus;

  //
  // Verify there actually is a queue.
  //
  if (headPhysAddr == 0) {
    WIIDBGLOG("head is zero");
    while (true);
    return;
  }
  headDoneTD = getTDFromPhysMapping(headPhysAddr);
  if (headDoneTD == NULL) {
    WIIDBGLOG("head TD is zero");
    while (true);
    return;
  }

  currTD = headDoneTD;
  while (currTD != NULL) {
    invalidateTransferDescriptor(currTD);
    transferStatus = (USBToHostLong(currTD->td.flags) & kOHCIGenTDFlagsConditionCodeMask) >> kOHCIGenTDFlagsConditionCodeShift;
    tdStatus = convertTDStatus(transferStatus);
    WIIDBGLOG("TD phys 0x%X, stat: 0x%X, 0x%X", currTD->physAddr, transferStatus, tdStatus);

    if (tdStatus != kIOReturnSuccess) {
      WIISYSLOG("Got an error here: 0x%X", tdStatus);
    }

    if (currTD->descType == kOHCITransferDescriptorTypeIsochronous) {
      // TODO
    } else {
      //
      // Invoke completion if present.
      //
      if (currTD->completion.gen.action != NULL) {
        if (USBToHostLong(currTD->td.currentBufferPtrPhysAddr) == 0) {
          bufferSizeRemaining = 0;
        } else {
          bufferSizeRemaining = USBToHostLong(currTD->td.bufferEndPhysAddr) - USBToHostLong(currTD->td.currentBufferPtrPhysAddr);
        }

        if (currTD->bufferSize != 0) {
          WIIDBGLOG("Completing a transfer %u bytes (%u bytes left), en 0x%X", currTD->bufferSize, bufferSizeRemaining, USBToHostLong(currTD->td.bufferEndPhysAddr));
          invalidateDataCache(currTD->tmpBufferPtr, kWiiOHCITempBufferSize);

          currTD->srcBuffer->writeBytes(0, currTD->tmpBufferPtr, currTD->bufferSize);

          UInt64 tmp[3] = { 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL };
          currTD->srcBuffer->readBytes(0, tmp, (currTD->bufferSize > sizeof (tmp)) ? sizeof (tmp) : currTD->bufferSize);
          WIIDBGLOG("0x%016llX%016llX%016llX", tmp[0], tmp[1], tmp[2]);
         // IOSleep(500);

          currTD->srcBuffer->release();
        } else {
          WIIDBGLOG("Completing a transfer with no data");
        }

        Complete(currTD->completion.gen, tdStatus, bufferSizeRemaining);
      } else {
        WIIDBGLOG("No completion");
      }

      nextTD = getTDFromPhysMapping(USBToHostLong(currTD->td.nextTDPhysAddr));
    }

    //
    // Get the next transfer descriptor.
    // The controller may have changed how the descriptors are linked, need to use the physical address.
    //
    returnTransferDescriptor(currTD);
    currTD = nextTD;
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
  // Add a new control endpoint descriptor.
  //
  return addNewEndpoint(functionNumber, endpointNumber, maxPacketSize, speed, kUSBAnyDirn, _edControlHeadPtr);
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
  OHCIEndpointDescriptor  *endpointDesc;
  UInt8                   endpointType;
  UInt32                  flags;

  WIIDBGLOG("F: %d, EP: %u, dir: %d, sz: %u", functionNumber, endpointNumber, direction, bufferSize);

  //
  // Locate the control endpoint.
  //
  endpointType = kWiiOHCIEndpointTypeControl;
  endpointDesc = getEndpoint(functionNumber, endpointNumber, direction, &endpointType);
  if (endpointDesc == NULL) {
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
  return doGeneralTransfer(endpointDesc, kOHCITransferDescriptorTypeControl,
    completion, CBP, bufferSize, flags, kOHCIRegCmdStatusControlListFilled);
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
WIIDBGLOG("start");
return kIOReturnUnsupported;
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
WIIDBGLOG("start");
return kIOReturnUnsupported;
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
  OHCIEndpointDescriptor  *endpointIntHead;

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
  endpointIntHead = getInterruptEDHead(pollingRate);
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
  OHCIEndpointDescriptor  *endpointDesc;
  UInt8                   endpointType;
  UInt32                  flags;

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
  endpointDesc = getEndpoint(functionNumber, endpointNumber, direction, &endpointType);
  if (endpointDesc == NULL) {
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

  return doGeneralTransfer(endpointDesc, kOHCITransferDescriptorTypeInterrupt, completion, CBP, bufferSize, flags, 0);
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
// Removes any transfer descriptors for an endpoint and resets any stall conditions.
// Called from IOUSBController::DoAbortEP().
//
// This function is gated and called within the workloop context.
//
IOReturn WiiOHCI::UIMAbortEndpoint(short functionNumber, short endpointNumber, short direction) {
  OHCIEndpointDescriptor  *endpointDesc;
  UInt8                   endpointType;

  WIIDBGLOG("F: %d, EP: %d, dir: %d", functionNumber, endpointNumber, direction);

  //
  // Locate the endpoint.
  //
  endpointType = kWiiOHCIEndpointTypeAll;
  endpointDesc = getEndpoint(functionNumber, endpointNumber, direction, &endpointType);
  if (endpointDesc == NULL) {
    return kIOUSBEndpointNotFound;
  }
  WIIDBGLOG("Aborting EP phys: 0x%X", endpointDesc->physAddr);

  //
  // Mark endpoint as skipped and wait for next frame.
  //
  endpointDesc->ep.flags |= HostToUSBLong(kOHCIEDFlagsSkip);
  flushEndpointDescriptor(endpointDesc);

  writeReg32(kOHCIRegIntStatus, kOHCIRegIntStatusStartOfFrame);
  while ((readReg32(kOHCIRegIntStatus) & kOHCIRegIntStatusStartOfFrame) == 0) {
    IODelay(10);
  }

  //
  // Remove all transfer descriptors associated with endpoint and re-activate the endpoint.
  //
  removeEndpointTransferDescriptors(endpointDesc);
    endpointDesc->ep.flags &= ~(HostToUSBLong(kOHCIEDFlagsSkip));
  flushEndpointDescriptor(endpointDesc);

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
  OHCIEndpointDescriptor  *endpointDesc;
  OHCIEndpointDescriptor  *prevEndpointDesc;
  UInt8                   endpointType;
  UInt32                  listMask;

  WIIDBGLOG("F: %d, EP: %d, dir: %d", functionNumber, endpointNumber, direction);

  //
  // Locate the endpoint.
  //
  endpointType = kWiiOHCIEndpointTypeAll;
  endpointDesc = getEndpoint(functionNumber, endpointNumber, direction, &endpointType, &prevEndpointDesc);
  if (endpointDesc == NULL) {
    return kIOUSBEndpointNotFound;
  }
  WIIDBGLOG("Deleting EP phys: 0x%X, previous EP phys: 0x%X", endpointDesc->physAddr, prevEndpointDesc->physAddr);

  //
  // Mark endpoint as skipped.
  //
  endpointDesc->ep.flags |= HostToUSBLong(kOHCIEDFlagsSkip);
  flushEndpointDescriptor(endpointDesc);

  //
  // Stop processing and wait for next frame.
  //
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

  writeReg32(kOHCIRegControl, readReg32(kOHCIRegControl) & ~(listMask));
  writeReg32(kOHCIRegIntStatus, kOHCIRegIntStatusStartOfFrame);
  while ((readReg32(kOHCIRegIntStatus) & kOHCIRegIntStatusStartOfFrame) == 0) {
    IODelay(10);
  }

  //
  // Remove endpoint from linked list and resume endpoint processing.
  //
  prevEndpointDesc->nextED            = endpointDesc->nextED;
  prevEndpointDesc->ep.nextEDPhysAddr = endpointDesc->ep.nextEDPhysAddr;
  flushEndpointDescriptor(prevEndpointDesc);
  writeReg32(kOHCIRegControl, readReg32(kOHCIRegControl) | listMask);
  WIIDBGLOG("Unlinked EP phys: 0x%X", endpointDesc->physAddr);

  //
  // Remove all transfer descriptors associated with endpoint and remove the endpoint descriptor.
  //
  removeEndpointTransferDescriptors(endpointDesc);
  returnEndpointDescriptor(endpointDesc);

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
  OHCIEndpointDescriptor  *endpointDesc;
  UInt8                   endpointType;

  WIIDBGLOG("F: %d, EP: %d, dir: %d", functionNumber, endpointNumber, direction);

  //
  // Locate the endpoint.
  //
  endpointType = kWiiOHCIEndpointTypeAll;
  endpointDesc = getEndpoint(functionNumber, endpointNumber, direction, &endpointType);
  if (endpointDesc == NULL) {
    return kIOUSBEndpointNotFound;
  }
  WIIDBGLOG("Clearing EP phys: 0x%X", endpointDesc->physAddr);

  //
  // Reset the transfer queue by unlinking all transfer descriptors.
  // This will also clear the current stall bit on the endpoint.
  //
  removeEndpointTransferDescriptors(endpointDesc);

  return kIOReturnSuccess;
}

void WiiOHCI::UIMRootHubStatusChange(void) {
WIIDBGLOG("start");
}

void WiiOHCI::UIMRootHubStatusChange(bool abort) {
WIIDBGLOG("start");
}

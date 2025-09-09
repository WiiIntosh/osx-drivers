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
  OHCITransferData  *genTransferCurr;
  OHCITransferData  *genTransferTail;
  UInt32            bufferRemaining;
  UInt32            transferSize;
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
    // Create general transfers for buffer.
    //
    offset = 0;
    bufferRemaining = bufferSize;
    while (offset < bufferSize) {
      //
      // Allocate a new tail general transfer.
      //
      genTransferTail = getFreeTransfer(endpoint);
      if (genTransferTail == NULL) {
        return kIOReturnNoMemory;
      }
      genTransferCurr = endpoint->transferTail;

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
        genTransferCurr->genTD->flags  = HostToUSBLong(flags);
        genTransferCurr->genCompletion = completion;
        genTransferCurr->last          = true;
      } else {
        genTransferCurr->genTD->flags  = HostToUSBLong(flags & ~(kOHCIGenTDFlagsBufferRounding));
        genTransferCurr->last          = false;
      }

      genTransferCurr->genTD->currentBufferPtrPhysAddr = HostToUSBLong(genTransferCurr->bounceBuffer->physAddr);
      genTransferCurr->genTD->nextTDPhysAddr           = HostToUSBLong(genTransferTail->physAddr);
      genTransferCurr->genTD->bufferEndPhysAddr        = HostToUSBLong(genTransferCurr->bounceBuffer->physAddr + transferSize - 1);
      genTransferCurr->actualBufferSize                = transferSize;
      genTransferCurr->nextTransfer                    = genTransferTail;

      WIIDBGLOG("GenTD phys: 0x%X, next 0x%X, buf 0x%X, ep 0x%X, frm 0x%X", genTransferCurr->physAddr, USBToHostLong(genTransferCurr->genTD->nextTDPhysAddr),
        USBToHostLong(genTransferCurr->genTD->currentBufferPtrPhysAddr), endpoint->physAddr, readReg32(kOHCIRegFmNumber));

      endpoint->transferTail       = genTransferTail;
      endpoint->ed->tailTDPhysAddr = HostToUSBLong(genTransferTail->physAddr);
      writeReg32(kOHCIRegCmdStatus, cmdBits);
    }
  } else {
    //
    // No data to transfer.
    //
    // Allocate a new general transfer.
    //
    genTransferTail = getFreeTransfer(endpoint);
    if (genTransferTail == NULL) {
      WIISYSLOG("Failed to allocate new TD");
      return kIOReturnNoMemory;
    }
    genTransferCurr = endpoint->transferTail;

    genTransferCurr->genTD->flags                    = HostToUSBLong(flags);
    genTransferCurr->genTD->currentBufferPtrPhysAddr = 0;
    genTransferCurr->genTD->bufferEndPhysAddr        = 0;
    genTransferCurr->genTD->nextTDPhysAddr           = HostToUSBLong(genTransferTail->physAddr);
    genTransferCurr->bounceBuffer                    = NULL;
    genTransferCurr->actualBufferSize                = 0;
    genTransferCurr->srcBuffer                       = NULL;
    genTransferCurr->genCompletion                   = completion;
    genTransferCurr->nextTransfer                    = genTransferTail;
    genTransferCurr->last                            = true;

    WIIDBGLOG("Added non-data gen TD phys 0x%X, next 0x%X", genTransferCurr->physAddr, USBToHostLong(genTransferCurr->genTD->nextTDPhysAddr));

    endpoint->transferTail       = genTransferTail;
    endpoint->ed->tailTDPhysAddr = HostToUSBLong(genTransferTail->physAddr);
    writeReg32(kOHCIRegCmdStatus, cmdBits);
  }

  return kIOReturnSuccess;
}

//
// Prepares the bounce buffer and descriptor for an isochronous transfer.
//
// This function is gated and called within the workloop context.
//
IOReturn WiiOHCI::prepareIsochTransfer(OHCITransferData *transfer, IOMemoryDescriptor *buffer, UInt32 offset, UInt32 transferSize,
                                       UInt16 numPackets, UInt32 flags) {
  UInt32 bufferPage;
  UInt16 packetOffset;

  WIIDBGLOG("TD phys: 0x%X, offset: 0x%X, size: 0x%X, fs: %u, fc: %u", transfer->physAddr,
    offset, transferSize, flags & kOHCIIsoTDFlagsStartingFrameMask, numPackets);
  
  //
  // Create bounce buffer and grab the source buffer.
  // Data will be copied in later just before the frame is sent.
  //
  transfer->bounceBuffer = getFreeBounceBuffer(transferSize > kWiiOHCIBounceBufferSize);
  if (transfer->bounceBuffer == NULL) {
    WIISYSLOG("Failed to get a bounce buffer");
    return kIOReturnDMAError;
  }
  transfer->srcBuffer = IOMemoryDescriptor::withSubRange(buffer, offset, transferSize, buffer->getDirection());
  if (transfer->srcBuffer == NULL) {
    WIISYSLOG("Failed to get sub memory descriptor");
    return kIOReturnDMAError;
  }
  transfer->srcBuffer->prepare();

  transfer->isoBufferCopied  = false;
  transfer->actualBufferSize = transferSize;

  //
  // Calculate offsets for packets.
  // Each one will be consecutive in the bounce buffer.
  //
  bufferPage   = transfer->bounceBuffer->physAddr & ~(PAGE_MASK);
  packetOffset = ((UInt16) (transfer->bounceBuffer->physAddr & PAGE_MASK));
  for (UInt32 i = 0; i < numPackets; i++) {
    transfer->isoTD->packetOffsetStatus[i] = HostToUSBWord(
      (packetOffset & kOHCIIsoTDPktOffsetMask) |
      ((kOHCITDConditionCodeNotAccessedPSW << kOHCIIsoTDPktOffsetConditionCodeShift) & kOHCIIsoTDPktOffsetConditionCodeMask)
    );
    WIIDBGLOG("TD phys: 0x%X, pg: 0x%X, pkt%u offset: 0x%X", transfer->physAddr, bufferPage, i, packetOffset);

    if (transfer->type == kOHCITransferTypeIsochronousLowLatency) {
      packetOffset += transfer->isoLowFrames[transfer->isoFrameIndex + i].frReqCount;
    } else {
      packetOffset += transfer->isoFrames[transfer->isoFrameIndex + i].frReqCount;
    }
  }

  //
  // Set flags and overall buffer location/size.
  //
  flags &= ~(kOHCIIsoTDFlagsConditionCodeMask);
  flags |= ((kOHCITDConditionCodeNotAccessed << kOHCIIsoTDFlagsConditionCodeShift) & kOHCIIsoTDFlagsConditionCodeMask);
  flags |= ((numPackets - 1) << kOHCIIsoTDFlagsFrameCountShift) & kOHCIIsoTDFlagsFrameCountMask;

  transfer->isoTD->flags             = HostToUSBLong(flags);
  transfer->isoTD->bufferPhysPage    = HostToUSBLong(bufferPage);
  transfer->isoTD->bufferEndPhysAddr = HostToUSBLong(bufferPage + packetOffset - 1);

  return kIOReturnSuccess;
}

//
// Submits an isochronous transfer to be executed by the OHCI controller.
//
// This function is gated and called within the workloop context.
//
IOReturn WiiOHCI::doIsochTransfer(short functionAddress, short endpointNumber, IOUSBIsocCompletion completion, UInt8 direction,
                                  UInt64 frameStart, IOMemoryDescriptor *pBuffer, UInt32 frameCount, IOUSBIsocFrame *pFrames,
                                  UInt32 updateFrequency, bool isLowLatency) {
  IOUSBLowLatencyIsocFrame  *framesLowPtr;

  OHCIEndpointData  *endpoint;
  UInt8             endpointType;
  UInt64            currentFrameNumber;
  UInt64            maxOffset = (UInt64)(0x00007FF0);

  OHCITransferData  *currTransfer;
  OHCITransferData  *tailTransfer;
  UInt32            flags;
  UInt32            bufferSize;
  UInt32            transferSize;
  UInt32            offset;
  UInt32            currReqFrameIndex;
  UInt32            currPacketIndex;
  UInt16            currPacketOffset;
  UInt32            currReqFrameSize;
  IOReturn          status;

  if ((frameCount == 0) || (frameCount > 1000)) {
    WIIDBGLOG("Bad frame count: %u", frameCount);
    return kIOReturnBadArgument;
  }

  if ((direction != kUSBOut) && (direction != kUSBIn)) {
    return kIOReturnInternalError;
  }

  //
  // Find isochronous endpoint.
  //
  endpointType = kWiiOHCIEndpointTypeIsochronous;
  endpoint     = getEndpoint(functionAddress, endpointNumber, direction, &endpointType);
  if (endpoint == NULL) {
    return kIOUSBEndpointNotFound;
  }

  currentFrameNumber = GetFrameNumber();
  if (frameStart <= currentFrameNumber) {
    if (frameStart < (currentFrameNumber - maxOffset)) {
      WIISYSLOG("Iso frame is too old, fs: %llu, fc: %llu", frameStart, currentFrameNumber);
      return kIOReturnIsoTooOld;
    }
  } else {
    if (frameStart > (currentFrameNumber + maxOffset)) {
      WIISYSLOG("Iso frame is too new, fs: %llu, fc: %llu", frameStart, currentFrameNumber);
      return kIOReturnIsoTooNew;
    }
  }

  //
  // Get the total buffer size.
  //
  bufferSize = 0;
  if (isLowLatency) {
    framesLowPtr = (IOUSBLowLatencyIsocFrame *) pFrames;
    for (UInt32 i = 0; i < frameCount; i++) {
      if (framesLowPtr[i].frReqCount > kUSBMaxFSIsocEndpointReqCount) {
        WIIDBGLOG("Iso frame too big: %u", framesLowPtr[i].frReqCount);
        return kIOReturnBadArgument;
      }

      bufferSize += framesLowPtr[i].frReqCount;
      framesLowPtr[i].frStatus = kUSBLowLatencyIsochTransferKey;
    }
  } else {
    for (UInt32 i = 0; i < frameCount; i++) {
      if (pFrames[i].frReqCount > kUSBMaxFSIsocEndpointReqCount) {
        WIIDBGLOG("Iso frame too big: %u", pFrames[i].frReqCount);
        return kIOReturnBadArgument;
      }

      bufferSize += pFrames[i].frReqCount;
    }
  }
  WIIDBGLOG("Buffer %s size: 0x%X, fsize: 0x%X, fs: %llu, fc: %u, cf: %llu, cf: %u", (direction == kUSBIn) ? "in" : "out",
    pBuffer->getLength(), bufferSize, frameStart, frameCount, currentFrameNumber, USBToHostWord(_hccaPtr->frameNumber));

  if (bufferSize == 0) {
    WIISYSLOG("Got an empty here"); // TODO handle the empty ones.
  }

  currTransfer    = endpoint->transferTail;
  currPacketIndex = 0;

  //
  // Iterate through frames and construct transfer descriptors.
  //
  currReqFrameIndex = 0;
  transferSize      = 0;
  offset            = 0;
  status            = kIOReturnSuccess;
  while (currReqFrameIndex < frameCount) {
    currReqFrameSize = (isLowLatency ? framesLowPtr[currReqFrameIndex].frReqCount : pFrames[currReqFrameIndex].frReqCount);

    //
    // Allocate a new transfer descriptor tail if current is exhausted.
    //
    if (((transferSize + currReqFrameSize) > kWiiOHCIBounceBufferJumboSize) || (currPacketIndex > 0)) {
      status = prepareIsochTransfer(currTransfer, pBuffer, offset, transferSize, currPacketIndex, flags);
      if (status != kIOReturnSuccess) {
        break;
      }

      //
      // Allocate a new tail transfer descriptor.
      //
      tailTransfer = getFreeTransfer(endpoint);
      if (tailTransfer == NULL) {
        status = kIOReturnNoMemory;
        break;
      }

      currTransfer->last                  = false;
      currTransfer->nextTransfer          = tailTransfer;
      currTransfer->isoTD->nextTDPhysAddr = HostToUSBLong(tailTransfer->physAddr);

      currTransfer    = tailTransfer;
      currPacketIndex = 0;
      offset         += transferSize;
      transferSize    = 0;
    }

    WIIDBGLOG("Frame: %u/%u, TD pkt: %u/%u, size: %u", currReqFrameIndex, frameCount, currPacketIndex,
      ARRSIZE(currTransfer->isoTD->packetOffsetStatus), currReqFrameSize);

    //
    // Initialize transfer descriptor on first packet.
    //
    if (currPacketIndex == 0) {
      currTransfer->isoFrameStart = ((UInt16) (currReqFrameIndex + (UInt16) frameStart)) & kOHCIIsoTDFlagsStartingFrameMask;
      flags = currTransfer->isoFrameStart & kOHCIIsoTDFlagsStartingFrameMask;

      currTransfer->type      = isLowLatency ? kOHCITransferTypeIsochronousLowLatency : kOHCITransferTypeIsochronous;
      currTransfer->direction = direction;
      if (isLowLatency) {
        currTransfer->isoLowFrames = framesLowPtr;
      } else {
        currTransfer->isoFrames = pFrames;
      }
      currTransfer->isoFrameIndex = currReqFrameIndex;
    }

    //
    // Add current frame size to this descriptor.
    //
    transferSize += currReqFrameSize;
    currReqFrameIndex++;
    currPacketIndex++;
  }

  if (status != kIOReturnSuccess) {
    //TODO
    return status;
  }

  //
  // Complete current transfer descriptor.
  // This last one will contain the completion.
  //
  status = prepareIsochTransfer(currTransfer, pBuffer, offset, transferSize, currPacketIndex, flags);
  if (status != kIOReturnSuccess) {
    return status;
  }
  currTransfer->last          = true;
  currTransfer->isoCompletion = completion;

  //
  // Allocate a new tail transfer descriptor.
  //
  tailTransfer = getFreeTransfer(endpoint);
  if (tailTransfer == NULL) {
    return kIOReturnNoMemory;
  }

  currTransfer->nextTransfer          = tailTransfer;
  currTransfer->isoTD->nextTDPhysAddr = HostToUSBLong(tailTransfer->physAddr);

  //
  // Update the tail on the endpoint to process these newly-linked descriptors.
  // Isochronous is a periodic transfer, no status bits to flip to indicate new data.
  //
  endpoint->transferTail       = tailTransfer;
  endpoint->ed->tailTDPhysAddr = HostToUSBLong(tailTransfer->physAddr);

  return kIOReturnSuccess;
}

//
// Completes a general transfer in the completed queue.
//
// This function is gated and called within the workloop context.
//
void WiiOHCI::completeGeneralTransfer(OHCITransferData *transfer) {
  UInt32    transferStatus;
  UInt32    bufferSizeRemaining;
  IOReturn  tdStatus;

  transferStatus = (USBToHostLong(transfer->genTD->flags) & kOHCIGenTDFlagsConditionCodeMask) >> kOHCIGenTDFlagsConditionCodeShift;
  tdStatus = convertTDStatus(transferStatus);
  WIIDBGLOG("GenTD phys 0x%X, next 0x%X, stat: 0x%X, 0x%X", transfer->physAddr, USBToHostLong(transfer->genTD->nextTDPhysAddr), transferStatus, tdStatus);

  bufferSizeRemaining = getGenTransferBufferRemaining(transfer);
  WIIDBGLOG("Transferred %u bytes (%u bytes left), ptr 0x%X end 0x%X",
    transfer->actualBufferSize - bufferSizeRemaining, bufferSizeRemaining,
    USBToHostLong(transfer->genTD->currentBufferPtrPhysAddr), USBToHostLong(transfer->genTD->bufferEndPhysAddr));

  //
  // Copy data back into original buffer if this was a read, and only if we actually transfered data.
  //
  if (transfer->srcBuffer != NULL) {
    if (transfer->srcBuffer->getDirection() & kIODirectionIn) {
      _invalidateCacheFunc((vm_offset_t) transfer->bounceBuffer->buf, transfer->actualBufferSize, false);
      if ((transfer->actualBufferSize - bufferSizeRemaining) > 0) {
        transfer->srcBuffer->writeBytes(0, transfer->bounceBuffer->buf, transfer->actualBufferSize - bufferSizeRemaining);
      }
    }
    OSSafeReleaseNULL(transfer->srcBuffer);
    UInt32 *buf = (UInt32*)transfer->bounceBuffer->buf;
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
  if (transfer->last) {
    WIIDBGLOG("Calling completion");
    Complete(transfer->genCompletion, tdStatus, bufferSizeRemaining);
  } else {
    WIIDBGLOG("No completion");

    //
    // If there was an error, need to finish the rest of the chain.
    //
    if (tdStatus != kIOReturnSuccess) {
      WIIDBGLOG("Completing short packet");
      completeFailedEndpointGenTransfers(transfer->endpoint, tdStatus, bufferSizeRemaining);
    }
  }
}

//
// Completes a isochronous transfer in the completed queue.
//
// This function is gated and called within the workloop context.
//
void WiiOHCI::completeIsochTransfer(OHCITransferData *transfer, IOReturn status) {
  UInt32    hcStatus;
  UInt32    bufferSizeRemaining;
  UInt16    frameCount;
  IOReturn  frameStatus;
  UInt32    offset;
  UInt16    pktOffStatus;
  bool      underrun;
  IOReturn  aggStatus;
  AbsoluteTime timeStamp;

  hcStatus   = (USBToHostLong(transfer->isoTD->flags) & kOHCIIsoTDFlagsConditionCodeMask) >> kOHCIIsoTDFlagsConditionCodeShift;
  frameCount = ((USBToHostLong(transfer->isoTD->flags) & kOHCIIsoTDFlagsFrameCountMask) >> kOHCIIsoTDFlagsFrameCountShift) + 1;

  offset    = 0;
  underrun  = false;
  aggStatus = kIOReturnSuccess;

  clock_get_uptime(&timeStamp);

  //
  // Overruns indicate a no bandwidth condition per the OHCI spec (see 4.3.2.3.5.3 Time Errors).
  //
  if (hcStatus == kOHCITDConditionCodeDataOverrun) {
    status = kIOReturnNoBandwidth;
  }
  WIIDBGLOG("IsoTD phys: 0x%X, next: 0x%X, fc: %u, HCs: 0x%X, s: 0x%X", transfer->physAddr, USBToHostLong(transfer->isoTD->nextTDPhysAddr), frameCount, hcStatus, status);

  //
  // Process each frame.
  //
  for (UInt32 i = 0; i < frameCount; i++) {
    pktOffStatus = USBToHostWord(transfer->isoTD->packetOffsetStatus[i]);
    if (status != kIOReturnSuccess) {
      WIIDBGLOG("IsoTD phys: 0x%X, psw[%u]: 0x%X", transfer->physAddr, i, pktOffStatus);
    }

    //
    // Isochronous has two different frame list structures for regular vs low-latency.
    //
    if (transfer->type == kOHCITransferTypeIsochronousLowLatency) {
      //
      // Check if frame was even accessed.
      //
      if (((pktOffStatus & kOHCIIsoTDPktOffsetConditionCodeMask) >> kOHCIIsoTDPktOffsetConditionCodeShift) == kOHCITDConditionCodeNotAccessedPSW) {
        WIIDBGLOG("IsoTD phys: 0x%X, psw[%u] not accessed", transfer->physAddr, i);
        hcStatus = kOHCITDConditionCodeNotAccessed;
        transfer->isoLowFrames[transfer->isoFrameIndex + i].frActCount = 0;
      } else {
        hcStatus = (pktOffStatus & kOHCIIsoTDPktStatusConditionCodeMask) >> kOHCIIsoTDPktStatusConditionCodeShift;
        //
        // Successful isochronous outbound transfers result in a size field of zero.
        // Successful isochronous inbound transfers result in a size field of the actual data received.
        //
        if (hcStatus != kOHCITDConditionCodeNoError) {
          WIIDBGLOG("some error here 0x%X", hcStatus);
        }

        if ((hcStatus == kOHCITDConditionCodeNoError) && (transfer->direction == kUSBOut)) {
          transfer->isoLowFrames[transfer->isoFrameIndex + i].frActCount = transfer->isoLowFrames[transfer->isoFrameIndex + i].frReqCount;
        } else {
          transfer->isoLowFrames[transfer->isoFrameIndex + i].frActCount = pktOffStatus & kOHCIIsoTDPktStatusSizeMask;
        }

        if (transfer->isoLowFrames[transfer->isoFrameIndex + i].frActCount != transfer->isoLowFrames[transfer->isoFrameIndex + i].frReqCount) {
          WIISYSLOG("mismatch");
        }
      }

      //
      // Translate HC status.
      //
      frameStatus = convertTDStatus(hcStatus);
      if (frameStatus != kIOReturnSuccess) {
        if (frameStatus == kIOReturnUnderrun) {
          underrun = true;
        } else {
          aggStatus = frameStatus;
        }
      }
      transfer->isoLowFrames[transfer->isoFrameIndex + i].frStatus = frameStatus;
      transfer->isoLowFrames[transfer->isoFrameIndex + i].frTimeStamp = timeStamp;
    } else {
      //
      // Check if frame was even accessed.
      //
      if (((pktOffStatus & kOHCIIsoTDPktOffsetConditionCodeMask) >> kOHCIIsoTDPktOffsetConditionCodeShift) == kOHCITDConditionCodeNotAccessedPSW) {
        WIIDBGLOG("IsoTD phys: 0x%X, psw[%u] not accessed", transfer->physAddr, i);
        hcStatus = kOHCITDConditionCodeNotAccessed;
        transfer->isoFrames[transfer->isoFrameIndex + i].frActCount = 0;
      } else {
        hcStatus = (pktOffStatus & kOHCIIsoTDPktStatusConditionCodeMask) >> kOHCIIsoTDPktStatusConditionCodeShift;
        //
        // Successful isochronous outbound transfers result in a size field of zero.
        // Successful isochronous inbound transfers result in a size field of the actual data received.
        //
        if (hcStatus != kOHCITDConditionCodeNoError) {
          WIISYSLOG("some error here 0x%X", hcStatus);
        }

        if ((hcStatus == kOHCITDConditionCodeNoError) && (transfer->direction == kUSBOut)) {
          transfer->isoFrames[transfer->isoFrameIndex + i].frActCount = transfer->isoFrames[transfer->isoFrameIndex + i].frReqCount;
        } else {
          transfer->isoFrames[transfer->isoFrameIndex + i].frActCount = pktOffStatus & kOHCIIsoTDPktStatusSizeMask;
        }

        if (transfer->isoFrames[transfer->isoFrameIndex + i].frActCount != transfer->isoFrames[transfer->isoFrameIndex + i].frReqCount) {
          WIISYSLOG("mismatch");
        }
      }

      //
      // Translate HC status.
      //
      frameStatus = convertTDStatus(hcStatus);
      if (frameStatus != kIOReturnSuccess) {
        if (frameStatus == kIOReturnUnderrun) {
          underrun = true;
        } else {
          aggStatus = frameStatus;
        }
      }
      transfer->isoFrames[transfer->isoFrameIndex + i].frStatus = frameStatus;
    }
  }

  //
  // Release source buffer.
  //
  if (transfer->srcBuffer != NULL) {
    transfer->srcBuffer->complete();
    OSSafeReleaseNULL(transfer->srcBuffer);
  }

  if ((status != kIOReturnSuccess) || (aggStatus != kIOReturnSuccess)) {
    WIISYSLOG("IsoTD phys 0%X err status 0x%X, agg 0x%X", transfer->physAddr, status, aggStatus);
  }

  //
  // Invoke callback if present.
  //
  if (transfer->last) {
    if ((status == kIOReturnSuccess) && ((aggStatus != kIOReturnSuccess) || underrun)) {
      if ((aggStatus == kIOReturnSuccess) && underrun) {
        aggStatus = kIOReturnUnderrun;
      }
      status = aggStatus;
    }

    WIIDBGLOG("IsoTD phys 0x%X, fs %u complete with status 0x%X", transfer->physAddr,
      transfer->isoFrameStart, status);
    (*transfer->isoCompletion.action)(transfer->isoCompletion.target, transfer->isoCompletion.parameter, status, transfer->isoFrames);
  }
}

//
// Walks the completed transfer descriptor queue and completes each one.
//
// This function is gated and called within the workloop context.
//
void WiiOHCI::completeTransferQueue(IOPhysicalAddress headPhysAddr, UInt32 producerCount) {
  OHCITransferData  *prevTransfer;
  OHCITransferData  *currTransfer;
  OHCITransferData  *nextTransfer;
  UInt32            consumerCount;
  UInt8             hcStatus;

  //
  // Verify there actually is a queue.
  //
  if (headPhysAddr == 0) {
    return;
  }

  consumerCount = _writeDoneHeadConsumerCount;
  if (consumerCount == producerCount) {
    WIIDBGLOG("Nothing to process");
    return;
  }

  WIIDBGLOG("Head done: 0x%X", headPhysAddr);
  currTransfer = getTransferFromPhys(headPhysAddr);
  if (currTransfer == NULL) {
    WIISYSLOG("head TD is NULL");
    while (true);
    return;
  }

  //
  // Reverse the queue and get the pointers to transfer data.
  // The host controller links the newest descriptors to the head of the queue.
  //
  prevTransfer = NULL;
  while (true) {
    WIIDBGLOG("TD phys: 0x%X", currTransfer->physAddr);

    currTransfer->nextTransfer = prevTransfer;
    prevTransfer               = currTransfer;
    consumerCount++;
    
    if (consumerCount == producerCount) {
      break;
    }

    if ((currTransfer->type == kOHCITransferTypeIsochronous) || (currTransfer->type == kOHCITransferTypeIsochronousLowLatency)) {
      nextTransfer = getTransferFromPhys(USBToHostLong(currTransfer->isoTD->nextTDPhysAddr));
    } else {
      nextTransfer = getTransferFromPhys(USBToHostLong(currTransfer->genTD->nextTDPhysAddr));
    }
    if (nextTransfer == NULL) {
      break;
    }

    currTransfer = nextTransfer;
  }

  currTransfer                = prevTransfer;
  _writeDoneHeadConsumerCount = consumerCount;

  //
  // Process completion for each transfer.
  //
  while (currTransfer != NULL) {
    if ((currTransfer->type == kOHCITransferTypeIsochronous) || (currTransfer->type == kOHCITransferTypeIsochronousLowLatency)) {
      hcStatus = (USBToHostLong(currTransfer->isoTD->flags) & kOHCIIsoTDFlagsConditionCodeMask) >> kOHCIIsoTDFlagsConditionCodeShift;
      completeIsochTransfer(currTransfer, convertTDStatus(hcStatus));
    } else {
      completeGeneralTransfer(currTransfer);
    }
    
    nextTransfer = currTransfer->nextTransfer;
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
  return addNewEndpoint(functionNumber, endpointNumber, maxPacketSize, speed, direction, _bulkEndpointHeadPtr);
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
  OHCIEndpointData  *endpoint;
  UInt8             endpointType;
  UInt32            endpointFlags;
  UInt32            currMaxPacketSize;
  UInt32            diffMaxPacketSize;
  IOReturn          status;

  WIIDBGLOG("F: %d, EP: %u, dir: %d, sz: %u", functionAddress, endpointNumber, direction, maxPacketSize);

  //
  // Attempt to find existing isochronous endpoint.
  //
  endpointType = kWiiOHCIEndpointTypeIsochronous;
  endpoint     = getEndpoint(functionAddress, endpointNumber, direction, &endpointType);
  if (endpoint != NULL) {
    WIIDBGLOG("Found existing endpoint, adjusting iso bandwidth to sz: %u", maxPacketSize);

    endpointFlags     = USBToHostLong(endpoint->ed->flags);
    currMaxPacketSize = ((endpointFlags & kOHCIEDFlagsMaxPktSizeMask) >> kOHCIEDFlagsMaxPktSizeShift);
    if (maxPacketSize == currMaxPacketSize) {
      WIIDBGLOG("Iso bandwidth requested is the same for sz: %u", maxPacketSize);
      return kIOReturnSuccess;
    }

    //
    // More bandwidth requested.
    //
    if (maxPacketSize > currMaxPacketSize) {
      diffMaxPacketSize = maxPacketSize - currMaxPacketSize;
      if (diffMaxPacketSize > _isoBandwidthAvailable) {
        WIIDBGLOG("No remaining iso bandwidth for sz: %u, available: %u", diffMaxPacketSize, _isoBandwidthAvailable);
        return kIOReturnNoBandwidth;
      }
      _isoBandwidthAvailable -= diffMaxPacketSize;

    //
    // Less bandwidth requested, return the difference.
    //
    } else {
      diffMaxPacketSize = currMaxPacketSize - maxPacketSize;
      _isoBandwidthAvailable += diffMaxPacketSize;
    }

    //
    // Update the endpoint flags with new max packet size.
    //
    endpointFlags &= ~(kOHCIEDFlagsMaxPktSizeMask);
    endpointFlags |= ((maxPacketSize << kOHCIEDFlagsMaxPktSizeShift) & kOHCIEDFlagsMaxPktSizeMask);
    endpoint->ed->flags = HostToUSBLong(endpointFlags);
    return kIOReturnSuccess;
  }

  if (maxPacketSize > _isoBandwidthAvailable) {
    WIIDBGLOG("No remaining iso bandwidth for sz: %u, available: %u", maxPacketSize, _isoBandwidthAvailable);
    return kIOReturnNoBandwidth;
  }

  //
  // Add new isochronous endpoint.
  //
  status = addNewEndpoint(functionAddress, endpointNumber, maxPacketSize, kUSBDeviceSpeedFull,
    direction, _isoEndpointHeadPtr, true);
  if (status != kIOReturnSuccess) {
    return status;
  }

  _isoBandwidthAvailable -= maxPacketSize;
  WIIDBGLOG("Allocated iso bandwidth for sz: %u, available: %u", maxPacketSize, _isoBandwidthAvailable);
  return kIOReturnSuccess;
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
  WIISYSLOG("F: %d, EP: %u, dir: %d, frm: %llu, fc: %u", functionAddress, endpointNumber, direction, frameStart, frameCount);
  return doIsochTransfer(functionAddress, endpointNumber, completion, direction, frameStart, pBuffer,
    frameCount, (void *) pFrames, 0, false);
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
  UInt32            maxPacketSize;

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
  // Free bandwidth from isochronous endpoints.
  //
  if (endpoint->isochronous) {
    maxPacketSize = (USBToHostLong(endpoint->ed->flags) & kOHCIEDFlagsMaxPktSizeMask) >> kOHCIEDFlagsMaxPktSizeShift;
    _isoBandwidthAvailable += maxPacketSize;
    WIIDBGLOG("Returned iso bandwidth: %u bytes, available: %u", maxPacketSize, _isoBandwidthAvailable);
  }

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

//
// Overrides IOUSBController::UIMCreateControlTransfer() (void* version).
//
// Executes a USB control transfer.
// Called from IOUSBController::ControlTransaction().
//
// This function is gated and called within the workloop context.
//
// In IOUSBFamily, this calls the old-style UIMCreateControlTransfer with a log function that always prints.
// Implementing the same thing here to prevent that. The bulk/interrupt versions do not do any logging, no need to override those.
//
IOReturn WiiOHCI::UIMCreateControlTransfer(short functionNumber, short endpointNumber, IOUSBCommand* command,
                                           void *CBP, bool bufferRounding, UInt32 bufferSize, short direction) {
 return UIMCreateControlTransfer(functionNumber, endpointNumber, command->GetUSLCompletion(), CBP, bufferRounding, bufferSize, direction);
}

//
// Overrides IOUSBController::UIMCreateControlTransfer() (void* version).
//
// Executes a USB control transfer.
// Called from IOUSBController::ControlTransaction().
//
// This function is gated and called within the workloop context.
//
// In IOUSBFamily, this calls the old-style UIMCreateControlTransfer with a log function that always prints.
// Implementing the same thing here to prevent that. The bulk/interrupt versions do not do any logging, no need to override those.
//
IOReturn WiiOHCI::UIMCreateControlTransfer(short functionNumber, short endpointNumber, IOUSBCommand* command,
                                           IOMemoryDescriptor *CBP, bool bufferRounding, UInt32 bufferSize, short direction) {
  return UIMCreateControlTransfer(functionNumber, endpointNumber, command->GetUSLCompletion(), CBP, bufferRounding, bufferSize, direction);
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_3
//
// Overrides IOUSBController::UIMCreateIsochTransfer().
//
// Executes a low-latency USB isochronous transfer.
// Called from IOUSBController::LowLatencyIsocTransaction().
//
// This function is gated and called within the workloop context.
//
// This function is specific to 10.3 and newer.
//
IOReturn WiiOHCI::UIMCreateIsochTransfer(short functionAddress, short endpointNumber, IOUSBIsocCompletion completion,
                                         UInt8 direction, UInt64 frameStart, IOMemoryDescriptor *pBuffer, UInt32 frameCount,
                                         IOUSBLowLatencyIsocFrame *pFrames, UInt32 updateFrequency) {
  WIIDBGLOG("F: %d, EP: %u, dir: %d, fr: %llu, fc: %u, up: %u", functionAddress, endpointNumber, direction,
    frameStart, frameCount, updateFrequency);
  return doIsochTransfer(functionAddress, endpointNumber, completion, direction, frameStart, pBuffer,
    frameCount, (IOUSBIsocFrame *) pFrames, updateFrequency, true);
}
#endif

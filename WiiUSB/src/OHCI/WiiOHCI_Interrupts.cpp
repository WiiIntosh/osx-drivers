//
//  WiiOHCI_Interrupts.cpp
//  Wii OHCI USB controller interface
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include "WiiOHCI.hpp"

//
// Interrupt handler filter function.
//
// This function runs in the primary interrupt handler context and should be as simple as possible.
// This may run concurrently with the secondary handler and any workloop functions.
//
bool WiiOHCI::filterInterrupt(IOFilterInterruptEventSource *filterIntEventSource) {
  UInt32            intEnable;
  UInt32            intStatus;
  IOPhysicalAddress newWriteDoneHeadPhysAddr;
  AbsoluteTime      timeStamp;
  UInt16            frameCount;
  UInt16            pktOffStatus;
  UInt32            numTDs;
  UInt16            hcFrameNumber;
  OHCITransferData  *prevTransfer;
  OHCITransferData  *currTransfer;
  bool              signalSecondaryInt;
  
  intEnable = readReg32(kOHCIRegIntEnable);
  intStatus = intEnable & readReg32(kOHCIRegIntStatus);

  //
  // Only handle interrupts if they are enabled, and if it was one that was enabled. TODO: Handle the error ones.
  //
  if (((intEnable & kOHCIRegIntEnableMasterInterruptEnable) == 0) || (intStatus == 0)) {
    return false;
  }
  signalSecondaryInt = false;

  //
  // Scheduling overrun.
  // Clear and move on.
  //
  if (intStatus & kOHCIRegIntStatusSchedulingOverrun) {
    writeReg32(kOHCIRegIntStatus, kOHCIRegIntStatusSchedulingOverrun);
    OSSynchronizeIO();
  }

  //
  // Done queue head written.
  // Process the done queue.
  //
  if (intStatus & kOHCIRegIntStatusWritebackDoneHead) {
    clock_get_uptime(&timeStamp);

    //
    // Get the queue head from HCCA and notify controller its been taken.
    //
    newWriteDoneHeadPhysAddr = USBToHostLong(_hccaPtr->doneHeadPhysAddr) & kOHCIRegDoneHeadMask;
    _hccaPtr->doneHeadPhysAddr = 0;
    writeReg32(kOHCIRegIntStatus, kOHCIRegIntStatusWritebackDoneHead);
    OSSynchronizeIO();

    //
    // Find end of the queue chain.
    //
    numTDs       = 0;
    prevTransfer = NULL;
    currTransfer = getTransferFromPhys(newWriteDoneHeadPhysAddr);
    while (currTransfer != NULL) {
      //
      // Update timestamp and status for low latency isochronous transfers.
      //
      if (currTransfer->type == kOHCITransferTypeIsochronousLowLatency) {
        frameCount = ((USBToHostLong(currTransfer->isoTD->flags) & kOHCIIsoTDFlagsFrameCountMask) >> kOHCIIsoTDFlagsFrameCountShift) + 1;
        for (UInt16 i = 0; i < frameCount; i++) {
          pktOffStatus = USBToHostWord(currTransfer->isoTD->packetOffsetStatus[i]);
          
          currTransfer->isoLowFrames[currTransfer->isoFrameIndex + i].frTimeStamp = timeStamp;
          if (((pktOffStatus & kOHCIIsoTDPktOffsetConditionCodeMask) >> kOHCIIsoTDPktOffsetConditionCodeShift) == kOHCITDConditionCodeNotAccessedPSW) {
            currTransfer->isoLowFrames[currTransfer->isoFrameIndex + i].frStatus   = convertTDStatus(kOHCITDConditionCodeNotAccessed);
            currTransfer->isoLowFrames[currTransfer->isoFrameIndex + i].frActCount = 0;
          } else {
            currTransfer->isoLowFrames[currTransfer->isoFrameIndex + i].frStatus = convertTDStatus((pktOffStatus & kOHCIIsoTDPktStatusConditionCodeMask) >> kOHCIIsoTDPktStatusConditionCodeShift);
            if ((currTransfer->isoLowFrames[currTransfer->isoFrameIndex + i].frStatus == kIOReturnSuccess) && (currTransfer->direction == kUSBOut)) {
              currTransfer->isoLowFrames[currTransfer->isoFrameIndex + i].frActCount = currTransfer->isoLowFrames[currTransfer->isoFrameIndex + i].frReqCount;
            } else {
              currTransfer->isoLowFrames[currTransfer->isoFrameIndex + i].frActCount = pktOffStatus & kOHCIIsoTDPktStatusSizeMask;
            }
          }
        }
      }

      numTDs++;
      prevTransfer = currTransfer;
      currTransfer = getTransferFromPhys(USBToHostLong(currTransfer->genTD->nextTDPhysAddr));
    }

    //
    // Link the existing head of the completed chain to this new one.
    //
    IOSimpleLockLock(_writeDoneHeadLock);
    
    if (prevTransfer != NULL) {
      prevTransfer->genTD->nextTDPhysAddr = HostToUSBLong(_writeDoneHeadPhysAddr);
    }
    _writeDoneHeadPhysAddr       = newWriteDoneHeadPhysAddr;
    _writeDoneHeadProducerCount += numTDs;

    IOSimpleLockUnlock(_writeDoneHeadLock);

    _intWriteDoneHead  = true;
    signalSecondaryInt = true;
  }

  //
  // Start of frame.
  // Clear/disable and move on.
  //
  if (intStatus & kOHCIRegIntStatusStartOfFrame) {
    writeReg32(kOHCIRegIntStatus, kOHCIRegIntStatusStartOfFrame);
    OSSynchronizeIO();

    writeReg32(kOHCIRegIntDisable, kOHCIRegIntDisableStartOfFrame);
    OSSynchronizeIO();
  }

  //
  // Resume detected.
  //
  if (intStatus & kOHCIRegIntStatusResumeDetected) {
    writeReg32(kOHCIRegIntStatus, kOHCIRegIntStatusResumeDetected);
    OSSynchronizeIO();

    _intResumeDetected = true;
    signalSecondaryInt = true;
  }

  //
  // Unrecoverable error.
  //
  if (intStatus & kOHCIRegIntStatusUnrecoverableError) {
    writeReg32(kOHCIRegIntStatus, kOHCIRegIntStatusUnrecoverableError);
    OSSynchronizeIO();

    _intUnrecoverableError = true;
    signalSecondaryInt     = true;
  }

  //
  // Frame number overflow.
  // Increment frame number counter.
  //
  if (intStatus & kOHCIRegIntEnableFrameNumberOverflow) {
    if (USBToHostWord(_hccaPtr->frameNumber) < BIT15) {
      _frameNumber += BIT16;
    }
    writeReg32(kOHCIRegIntStatus, kOHCIRegIntEnableFrameNumberOverflow);
    OSSynchronizeIO();
  }

  //
  // Root hub status change.
  //
  if (intStatus & kOHCIRegIntStatusRootHubStatusChange) {
    writeReg32(kOHCIRegIntDisable, kOHCIRegIntDisableRootHubStatusChange);
    writeReg32(kOHCIRegIntStatus, kOHCIRegIntStatusRootHubStatusChange);
    OSSynchronizeIO();

    _intRootHubStatus  = true;
    signalSecondaryInt = true;
  }

  //
  // Ownership change.
  // Should never occur.
  //
  if (intStatus & kOHCIRegIntStatusOwnershipChange) {
    writeReg32(kOHCIRegIntStatus, kOHCIRegIntStatusOwnershipChange);
    OSSynchronizeIO();
  }

  //
  // Signal the secondary handler manually so the primary is never disabled.
  // Need to keep it moving for various low latency operations.
  //
  if (signalSecondaryInt) {
    _interruptEventSource->signalInterrupt();
  }
  return false;
}

//
// Handles interrupts.
//
// This function is gated and called within the workloop context.
//
void WiiOHCI::handleInterrupt(IOInterruptEventSource *intEventSource, int count) {
  IOInterruptState  intState;
  UInt32            newWriteHeadDoneProducerCount;
  IOPhysicalAddress newWriteHeadDonePhysAddr;

  WIIDBGLOG("Interrupt: WH: %u, RH: %u", _intWriteDoneHead, _intRootHubStatus);

  //
  // Done queue head written.
  //
  if (_intWriteDoneHead) {
    _intWriteDoneHead = false;

    intState = IOSimpleLockLockDisableInterrupt(_writeDoneHeadLock);

    newWriteHeadDonePhysAddr      = _writeDoneHeadPhysAddr;
    newWriteHeadDoneProducerCount = _writeDoneHeadProducerCount;

    IOSimpleLockUnlockEnableInterrupt(_writeDoneHeadLock, intState);

    completeTransferQueue(newWriteHeadDonePhysAddr, newWriteHeadDoneProducerCount);
  }

  //
  // Root hub status change.
  //
  if (_intRootHubStatus) {
    _intRootHubStatus = false;
    completeRootHubInterruptTransfer(false);
  }
}

//
// Handles isochronous timer events.
//
// This function is not called within the regular workloop context.
// The timer is stopped/started when the endpoint list has items removed.
//
void WiiOHCI::handleIsoTimer(IOTimerEventSource *sender) {
  UInt16            hcFrameNumber;
  OHCIEndpointData  *currEndpoint;
  OHCITransferData  *currTransfer;

  //
  // Iterate through each isochronous endpoint and check for transfer descriptors that are about to be sent.
  //
  currEndpoint = _isoEndpointHeadPtr;
  while (currEndpoint != _isoEndpointTailPtr) {
    currTransfer = getTransferFromPhys(USBToHostLong(currEndpoint->ed->headTDPhysAddr) & kOHCIEDTDHeadMask);
    if (currTransfer == NULL) {
      currEndpoint = currEndpoint->nextEndpoint;
      continue;
    }

    //
    // Iterate through each transfer descriptor.
    //
    while (currTransfer != currEndpoint->transferTail) {
      //
      // Check if transfer hasn't already been copied, and is going to be transferred shortly if outbound.
      //
      hcFrameNumber = USBToHostWord(_hccaPtr->frameNumber);
      if (!currTransfer->isoBufferCopied && (currTransfer->isoFrameStart > hcFrameNumber)) {
        if ((currTransfer->isoFrameStart - hcFrameNumber) < 3) {
          if (currTransfer->srcBuffer != NULL) {
            currTransfer->srcBuffer->readBytes(0, currTransfer->bounceBuffer->buf, currTransfer->actualBufferSize);
            flushDataCache(currTransfer->bounceBuffer->buf, currTransfer->actualBufferSize);
          }
          currTransfer->isoBufferCopied = true;
        }
      }

      currTransfer = currTransfer->nextTransfer;
    }

    currEndpoint = currEndpoint->nextEndpoint;
  }

  _isoTimerEventSource->setTimeoutUS(kWiiOHCIIsoTimerRefreshUS);
}

//
// Overrides IOUSBController::PollInterrupts().
//
void WiiOHCI::PollInterrupts(IOUSBCompletionAction safeAction) {
  WIIDBGLOG("start");
}

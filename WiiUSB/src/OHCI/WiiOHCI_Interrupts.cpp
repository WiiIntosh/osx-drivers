//
//  WiiOHCI_Interrupts.cpp
//  Wii OHCI USB controller interface
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include "WiiOHCI.hpp"

//
// Handles interrupts.
//
void WiiOHCI::handleInterrupt(IOInterruptEventSource *intEventSource, int count) {
  UInt32  intStatus;
  UInt32  intEnable;

  intStatus = readReg32(kOHCIRegIntStatus);
  intEnable = readReg32(kOHCIRegIntEnable);

  WIIDBGLOG("interrupt: 0x%X, mask: 0x%X", intStatus, intEnable);

  //
  // Only handle interrupts if they are enabled, and if it was one that was enabled.
  //
  intStatus &= intEnable;
  if (((intEnable & kOHCIRegIntEnableMasterInterruptEnable) == 0) || (intStatus == 0)) {
    return;
  }

  //
  // Queue completed.
  //
  if (intStatus & kOHCIRegIntStatusWritebackDoneHead) {
    //
    // Get the queue head from HCCA.
    //
    invalidateDataCache(&_hccaPtr->doneHeadPhysAddr, sizeof (_hccaPtr->doneHeadPhysAddr));
    completeTransferQueue(USBToHostLong(_hccaPtr->doneHeadPhysAddr) & kOHCIRegDoneHeadMask);
    writeReg32(kOHCIRegIntStatus, kOHCIRegIntStatusWritebackDoneHead);
  }

  //
  // Root hub status change.
  //
  if (intStatus & kOHCIRegIntStatusRootHubStatusChange) {
    writeReg32(kOHCIRegIntStatus, kOHCIRegIntStatusRootHubStatusChange);
    completeRootHubInterruptTransfer(false);
  }
}

//
// Overrides IOUSBController::PollInterrupts().
//
void WiiOHCI::PollInterrupts(IOUSBCompletionAction safeAction) {
  WIIDBGLOG("start");
}

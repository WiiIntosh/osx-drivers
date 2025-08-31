//
//  WiiOHCI_RootHub.cpp
//  Wii OHCI USB controller interface
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include "WiiOHCI.hpp"

#define kWiiRootHubProductStringIndex   1
#define kWiiRootHubVendorStringIndex    2

//
// Overrides IOUSBController::GetRootHubDeviceDescriptor().
//
// Returns the device descriptor for the emulated root hub.
//
IOReturn WiiOHCI::GetRootHubDeviceDescriptor(IOUSBDeviceDescriptor *desc) {
  if (desc == NULL) {
    return kIOReturnNoMemory;
  }

  //
  // Populate the device descriptor for the root hub.
  //
  desc->bLength             = sizeof (*desc);
  desc->bDescriptorType     = kUSBDeviceDesc;
  desc->bcdUSB              = USB_CONSTANT16(kUSBRel10);
  desc->bDeviceClass        = kUSBHubClass;
  desc->bDeviceSubClass     = kUSBHubSubClass;
  desc->bDeviceProtocol     = 0;
  desc->bMaxPacketSize0     = 8;
  desc->idVendor            = USB_CONSTANT16(kAppleVendorID);
  desc->idProduct           = USB_CONSTANT16(kPrdRootHubApple);
  desc->bcdDevice           = USB_CONSTANT16(0x0190);
  desc->iManufacturer       = kWiiRootHubVendorStringIndex;
  desc->iProduct            = kWiiRootHubProductStringIndex;
  desc->iSerialNumber       = 0;
  desc->bNumConfigurations  = 1;

  return kIOReturnSuccess;
}

//
// Overrides IOUSBController::GetRootHubDescriptor().
//
// Returns the hub descriptor for the emulated root hub.
//
IOReturn WiiOHCI::GetRootHubDescriptor(IOUSBHubDescriptor *desc) {
  UInt32  descA;
  UInt32  descB;

  if (desc == NULL) {
    return kIOReturnNoMemory;
  }

  //
  // Get root hub info from controller.
  //
  descA = readReg32(kOHCIRegRhDescriptorA);
  descB = readReg32(kOHCIRegRhDescriptorB);
  WIIDBGLOG("RH desc: 0x%08X, 0x%08X", descA, descB);

  //
  // Populate the hub descriptor for the root hub.
  //
  desc->length        = sizeof (*desc);
  desc->hubType       = kUSBHubDescriptorType;
  desc->numPorts      = descA & kOHCIRegRhDescriptorANumPortsMask;
  desc->powerOnToGood = (descA & kOHCIRegRhDescriptorAPowerOnToPowerGoodTimeMask) >> kOHCIRegRhDescriptorAPowerOnToPowerGoodTimeShift;
  desc->hubCurrent    = 0;

  //
  // Charactistics are little endian.
  //
  desc->characteristics  = 0;
  desc->characteristics |= (descA & kOHCIRegRhDescriptorANoPowerSwitching)   ? kNoPowerSwitchingBit   : 0;
  desc->characteristics |= (descA & kOHCIRegRhDescriptorAPowerModePerPort)   ? kPerPortSwitchingBit   : 0;
  desc->characteristics |= (descA & kOHCIRegRhDescriptorADeviceType)         ? kCompoundDeviceBit     : 0;
  desc->characteristics |= (descA & kOHCIRegRhDescriptorAOverCurrentPerPort) ? kPerPortOverCurrentBit : 0;
  desc->characteristics |= (descA & kOHCIRegRhDescriptorANoOverCurrent)      ? kNoOverCurrentBit      : 0;
  desc->characteristics  = HostToUSBWord(desc->characteristics);

  //
  // Create bitmap specifying removable ports.
  // OHCI only supports 15 ports, zero the rest.
  //
  *((UInt16*)&desc->removablePortFlags[0]) = descB & kOHCIRegRhDescriptorBDeviceRemovableMask;
  *((UInt16*)&desc->removablePortFlags[2]) = 0;
  *((UInt32*)&desc->removablePortFlags[4]) = 0;

  //
  // Create bitmap specifying power mode for each port.
  //
  *((UInt16*)&desc->pwrCtlPortFlags[0]) = (descB & kOHCIRegRhDescriptorBPortPowerControlMask) >> kOHCIRegRhDescriptorBPortPowerControlShift;
  *((UInt16*)&desc->pwrCtlPortFlags[2]) = 0;
  *((UInt32*)&desc->pwrCtlPortFlags[4]) = 0;

  return kIOReturnSuccess;
}

//
// Overrides IOUSBController::SetRootHubDescriptor().
//
// Sets the hub descriptor for the emulated root hub.
//
IOReturn WiiOHCI::SetRootHubDescriptor(OSData *buffer) {
  //
  // Not implemented.
  //
  return kIOReturnSuccess;
}

//
// Overrides IOUSBController::GetRootHubConfDescriptor().
//
// Gets the configuration descriptor for the emulated root hub.
//
IOReturn WiiOHCI::GetRootHubConfDescriptor(OSData *desc) {
  //
  // Root hub configuration descriptor.
  //
  IOUSBConfigurationDescriptor confDescriptor = {
    sizeof (IOUSBConfigurationDescriptor),  // Length.
    kUSBConfDesc,                           // Type.
    USB_CONSTANT16(sizeof (IOUSBConfigurationDescriptor) +
                   sizeof (IOUSBInterfaceDescriptor) +
                   sizeof (IOUSBEndpointDescriptor)),   // Total length.
    1,            // Interface count.
    1,            // Configuration value.
    0,            // Configuration string index (none).
    0x60,         // Attributes (self-powered).
    0             // Max power.
  };

  //
  // Root hub interface descriptor.
  //
  IOUSBInterfaceDescriptor interfaceDescriptor = {
    sizeof (IOUSBInterfaceDescriptor),  // Length.
    kUSBInterfaceDesc,                  // Type.
    0,                // Interface number.
    0,                // Alternate setting.
    1,                // Endpoint count.
    kUSBHubClass,     // Class (hub).
    kUSBHubSubClass,  // Subclass (hub).
    1,                // Interface procotol.
    0                 // Interface string index (none).
  };

  //
  // Root hub endpoint descriptor.
  //
  IOUSBEndpointDescriptor endpointDescriptor = {
    sizeof (IOUSBEndpointDescriptor),   // Length.
    kUSBEndpointDesc,                   // Type.
    0x81,               // Endpoint address.
    kUSBInterrupt,      // Attributes.
    HostToUSBWord(8),   // Max packet size.
    255                 // Interval.
  };

  if (desc == NULL) {
    return kIOReturnNoMemory;
  }

  if (!desc->appendBytes(&confDescriptor, confDescriptor.bLength)) {
    return kIOReturnNoMemory;
  }
  if (!desc->appendBytes(&interfaceDescriptor, interfaceDescriptor.bLength)) {
    return kIOReturnNoMemory;
  }
  if (!desc->appendBytes(&endpointDescriptor, endpointDescriptor.bLength)) {
    return kIOReturnNoMemory;
  }

  return kIOReturnSuccess;
}

//
// Overrides IOUSBController::GetRootHubStatus().
//
// Gets the root hub status.
//
IOReturn WiiOHCI::GetRootHubStatus(IOUSBHubStatus *status) {
  UInt32 rhStatus = readReg32(kOHCIRegRhStatus);
  WIIDBGLOG("RH status: 0x%X", rhStatus);

  //
  // IOUSBFamily expects a little endian value, swap to little endian.
  // On Wii, all registers for all devices are big endian.
  //
  *((UInt32*) status) = HostToUSBLong(rhStatus);
  return kIOReturnSuccess;
}

//
// Overrides IOUSBController::SetRootHubFeature().
//
// Sets a root hub feature.
//
IOReturn WiiOHCI::SetRootHubFeature(UInt16 wValue) {
  //
  // Not implemented.
  //
  return kIOReturnSuccess;
}

//
// Overrides IOUSBController::ClearRootHubFeature().
//
// Clears a root hub feature.
//
IOReturn WiiOHCI::ClearRootHubFeature(UInt16 wValue) {
  //
  // Not implemented.
  //
  return kIOReturnSuccess;
}

//
// Overrides IOUSBController::GetRootHubPortStatus().
//
// Gets the status of the specified port on the root hub.
//
IOReturn WiiOHCI::GetRootHubPortStatus(IOUSBHubPortStatus *status, UInt16 port) {
  UInt32 portStatus;
  if ((port < 1) || (port > 15)) {
    return kIOReturnBadArgument;
  }

  portStatus = readRootHubPort32(port);
  WIIDBGLOG("P%u status: 0x%X", port, portStatus);

  //
  // IOUSBFamily expects a little endian value, swap to little endian.
  // On Wii, all registers for all devices are big endian.
  //
  *((UInt32*) status) = HostToUSBLong(portStatus);
  return kIOReturnSuccess;
}

//
// Overrides IOUSBController::SetRootHubPortFeature().
//
// Sets a root hub port feature.
//
IOReturn WiiOHCI::SetRootHubPortFeature(UInt16 wValue, UInt16 port) {
  UInt32 value;

  WIIDBGLOG("Port: %u, feature: 0x%X", port, wValue);
  switch (wValue) {
    case kUSBHubPortEnableFeature:
      value = kOHCIRegRhPortStatusSetPortEnable;
      break;

    case kUSBHubPortSuspendFeature:
      value = kOHCIRegRhPortStatusSetPortSuspend;
      break;

    case kUSBHubPortResetFeature:
      value = kOHCIRegRhPortStatusSetPortReset;
      break;

    case kUSBHubPortPowerFeature:
      value = kOHCIRegRhPortStatusSetPortPower;
      break;

    default:
      WIISYSLOG("Unknown port %u feature set: 0x%X", port, wValue);
      return kIOReturnUnsupported;
  }

  writeRootHubPort32(port, value);
  return kIOReturnSuccess;
}

//
// Overrides IOUSBController::ClearRootHubPortFeature().
//
// Clears a root hub port feature.
//
IOReturn WiiOHCI::ClearRootHubPortFeature(UInt16 wValue, UInt16 port) {
  UInt32 value;

  WIIDBGLOG("Port: %u, feature: 0x%X", port, wValue);
  switch (wValue) {
    case kUSBHubPortEnableFeature:
      value = kOHCIRegRhPortStatusClearPortEnable;
      break;

    case kUSBHubPortSuspendFeature:
      value = kOHCIRegRhPortStatusClearPortSuspend;
      break;

    case kUSBHubPortPowerFeature:
      value = kOHCIRegRhPortStatusClearPortPower;
      break;

    case kUSBHubPortConnectionChangeFeature:
      value = kOHCIRegRhPortStatusConnectStatusChange;
      break;

    case kUSBHubPortEnableChangeFeature :
      value = kOHCIRegRhPortStatusPortEnableStatusChange;
      break;

    case kUSBHubPortSuspendChangeFeature :
      value = kOHCIRegRhPortStatusPortSuspendStatusChange;
      break;

    case kUSBHubPortOverCurrentChangeFeature :
      value = kOHCIRegRhPortStatusPortOverCurrentIndicatorChange;
      break;

    case kUSBHubPortResetChangeFeature :
      value = kOHCIRegRhPortStatusPortResetStatusChange;
      break;

    default:
      WIISYSLOG("Unknown port %u feature clear: 0x%X", port, wValue);
      return kIOReturnUnsupported;
  }

  writeRootHubPort32(port, value);
  return kIOReturnSuccess;
}

//
// Overrides IOUSBController::GetRootHubPortState().
//
// Gets the state of the specified port on the root hub.
//
IOReturn WiiOHCI::GetRootHubPortState(UInt8 *state, UInt16 port) {
  //
  // Not implemented.
  //
  return kIOReturnSuccess;
}

//
// Overrides IOUSBController::SetHubAddress().
//
// Sets the address of the root hub.
//
IOReturn WiiOHCI::SetHubAddress(UInt16 wValue) {
  WIIDBGLOG("New root hub address: %u", wValue);
  _rootHubAddress = wValue;
  return kIOReturnSuccess;
}

//
// Overrides IOUSBController::GetRootHubStringDescriptor().
//
// Gets a string descriptor for the emulated root hub.
//
IOReturn WiiOHCI::GetRootHubStringDescriptor(UInt8 index, OSData *desc) {
  //
  // Strings are in Unicode.
  //
  UInt8 productName[] = {
    0,          // Length.
    kUSBStringDesc, // Descriptor type.
    0x4F, 0x00, // "O"
    0x48, 0x00, // "H"
    0x43, 0x00, // "C"
    0x49, 0x00, // "I"
    0x20, 0x00, // " "
    0x52, 0x00, // "R"
    0x6F, 0x00, // "o"
    0x6F, 0x00, // "o"
    0x74, 0x00, // "t"
    0x20, 0x00, // " "
    0x48, 0x00, // "H"
    0x75, 0x00, // "u"
    0x62, 0x00, // "b"
    0x20, 0x00, // " "
    0x53, 0x00, // "S"
    0x69, 0x00, // "i"
    0x6d, 0x00, // "m"
    0x75, 0x00, // "u"
    0x6C, 0x00, // "l"
    0x61, 0x00, // "a"
    0x74, 0x00, // "t"
    0x69, 0x00, // "i"
    0x6F, 0x00, // "o"
    0x6E, 0x00, // "n"
  };

  UInt8 vendorName[] = {
    0,          // Length.
    kUSBStringDesc, // Descriptor type.
    0x41, 0x00, // "A"
    0x70, 0x00, // "p"
    0x70, 0x00, // "p"
    0x6C, 0x00, // "l"
    0x65, 0x00, // "e"
    0x20, 0x00, // " "
    0x43, 0x00, // "C"
    0x6F, 0x00, // "o"
    0x6D, 0x00, // "m"
    0x70, 0x00, // "p"
    0x75, 0x00, // "u"
    0x74, 0x00, // "t"
    0x65, 0x00, // "e"
    0x72, 0x00, // "r"
    0x2C, 0x00, // ","
    0x20, 0x00, // " "
    0x49, 0x00, // "I"
    0x6E, 0x00, // "n"
    0x63, 0x00, // "c"
    0x2E, 0x00  // "."
  };

  if (index > kWiiRootHubVendorStringIndex) {
    return kIOReturnBadArgument;
  }

  //
  // Set string lengths.
  //
  vendorName[0]   = sizeof (vendorName);
  productName[0]  = sizeof (productName);

  //
  // Handle product string.
  //
  if (index == kWiiRootHubProductStringIndex) {
    if (desc == NULL) {
      return kIOReturnNoMemory;
    }
    if (!desc->appendBytes(&productName, productName[0])) {
      return kIOReturnNoMemory;
    }
  }

  //
  // Handle vendor string.
  //
  if (index == kWiiRootHubVendorStringIndex) {
    if (desc == NULL) {
      return kIOReturnNoMemory;
    }
    if (!desc->appendBytes(&vendorName, vendorName[0])) {
      return kIOReturnNoMemory;
    }
  }

  return kIOReturnSuccess;
}

//
// Simulates a control endpoint creation for the root hub.
//
IOReturn WiiOHCI::simulateRootHubControlEDCreate(UInt8 endpointNumber, UInt16 maxPacketSize, UInt8 speed) {
  if ((endpointNumber != 0) || (speed != kUSBDeviceSpeedFull)) {
    return kIOReturnBadArgument;
  }
  return kIOReturnSuccess;
}

//
// Simulates an interrupt endpoint creation for the root hub.
//
IOReturn WiiOHCI::simulateRootHubInterruptEDCreate(short endpointNumber, UInt8 direction, short speed, UInt16 maxPacketSize) {
  if ((endpointNumber != 1) || (speed != kUSBDeviceSpeedFull) || (direction != kUSBIn)) {
    return kIOReturnBadArgument;
  }
  return kIOReturnSuccess;
}

//
// Simulates an interrupt transfer for the root hub.
//
void WiiOHCI::simulateRootHubInterruptTransfer(short endpointNumber, IOUSBCompletion completion,
                                                   IOMemoryDescriptor *CBP, bool bufferRounding, UInt32 bufferSize, short direction) {
  //
  // Only endpoint 1 is supported.
  //
  if ((endpointNumber != 1) || (direction != kUSBIn)) {
    Complete(completion, kIOReturnBadArgument, bufferSize);
    return;
  }

  //
  // Find a free slot to simulate the root hub interrupt transfer.
  //
  IOLockLock(_rootHubInterruptTransLock);
  for (unsigned int i = 0; i < ARRSIZE(_rootHubInterruptTransactions); i++) {
    if (_rootHubInterruptTransactions[i].completion.action == NULL) {
      _rootHubInterruptTransactions[i].buffer       = CBP;
      _rootHubInterruptTransactions[i].bufferLength = bufferSize;
      _rootHubInterruptTransactions[i].completion   = completion;
      IOLockUnlock(_rootHubInterruptTransLock);

      //
      // Enable the root hub status change interrupt.
      // These interrupt transfers will get completed when that arrives.
      //
      writeReg32(kOHCIRegIntEnable, readReg32(kOHCIRegIntEnable) | kOHCIRegIntEnableRootHubStatusChange);
      return;
    }
  }

  //
  // No available slots.
  //
  IOLockUnlock(_rootHubInterruptTransLock);
  Complete(completion, kIOReturnNoMemory, bufferSize);
}

//
// Completes any pending root hub interrupt transfers, triggered on the root hub status change interrupt.
//
void WiiOHCI::completeRootHubInterruptTransfer(bool abort) {
  struct WiiOHCIRootHubIntTransaction lastTransaction;
  IOUSBHubStatus      rootHubStatus;
  IOUSBHubPortStatus  portStatus;
  UInt8               numPorts;
  UInt16              statusChangedBitmap;
  UInt32              bufferLengthDelta;

  statusChangedBitmap = 0;
  numPorts            = 0;

  if (!abort) {
    if (GetRootHubStatus(&rootHubStatus) != kIOReturnSuccess) {
      return;
    }

    //
    // Encode the root hub status change and any port changes into the bitmap.
    //
    // OHCI only supports 15 ports, 16-bit bitmap is enough:
    // bit 0: Root hub status changed.
    // bit 1: Root hub port 1 status changed.
    // bit 2: Root hub port 2 status changed.
    // ...
    // bit 15: Root hub port 15 status changed.
    //
    rootHubStatus.changeFlags = USBToHostWord(rootHubStatus.changeFlags);
    if (rootHubStatus.changeFlags != 0) {
      statusChangedBitmap |= 1;
    }

    //
    // Check port statuses.
    //
    numPorts = readReg32(kOHCIRegRhDescriptorA) & kOHCIRegRhDescriptorANumPortsMask;
    for (UInt8 port = 1; port <= numPorts; port++) {
      GetRootHubPortStatus(&portStatus, port);
      portStatus.changeFlags = USBToHostWord(portStatus.changeFlags);
      WIIDBGLOG("Port %u change: 0x%X", port, portStatus.changeFlags);
      if (portStatus.changeFlags != 0) {
        statusChangedBitmap |= (1 << port);
      }
    }

    //
    // Convert the bitmap to little-endian if needed.
    //
    WIIDBGLOG("Bitmap: 0x%X", statusChangedBitmap);
    statusChangedBitmap = HostToUSBWord(statusChangedBitmap);
  }

  if (abort || ((statusChangedBitmap != 0) && (_rootHubInterruptTransactions[0].completion.action != NULL))) {
    //
    // Disable the root hub status change interrupt.
    //
    writeReg32(kOHCIRegIntDisable, kOHCIRegIntDisableRootHubStatusChange);

    //
    // Get first one and move all others forward.
    //
    IOTakeLock(_rootHubInterruptTransLock);
    lastTransaction = _rootHubInterruptTransactions[0];
    for (unsigned int i = 1; i < ARRSIZE(_rootHubInterruptTransactions); i++) {
      _rootHubInterruptTransactions[i - 1] = _rootHubInterruptTransactions[i];
      if (_rootHubInterruptTransactions[i].completion.action == NULL) {
        break;
      }
    }
    IOUnlock(_rootHubInterruptTransLock);

    //
    // Copy the change bitmap and complete the transfer.
    //
    //
    bufferLengthDelta = lastTransaction.bufferLength;
    if (bufferLengthDelta > sizeof (statusChangedBitmap)) {
      bufferLengthDelta = sizeof (statusChangedBitmap);
    }
    if (numPorts < 8) {
      bufferLengthDelta = 1;
    }

    lastTransaction.buffer->writeBytes(0, &statusChangedBitmap, bufferLengthDelta);
    Complete(lastTransaction.completion, abort ? kIOReturnAborted : kIOReturnSuccess, lastTransaction.bufferLength - bufferLengthDelta);
  }
}

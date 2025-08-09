//
//  WiiEXI_Private.cpp
//  Wii EXI controller interface
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include "WiiEXI.hpp"

//
// Interrupt handler function.
//
void WiiEXI::handleInterrupt(IOInterruptEventSource *intEventSource, int count) {
  WIIDBGLOG("Interrupt!");
}

//
// Selects a device on the EXI bus.
//
// This function is gated and called within the workloop context.
//
IOReturn WiiEXI::selectDevice(UInt32 channel, UInt32 device, WiiEXIClock clock, bool enableInt) {
  UInt32 data;

  if ((channel >= kWiiEXIMaxChannels) || (device >= kWiiEXIMaxDevices)) {
    WIIDBGLOG("Invalid channel/device");
    return kIOReturnBadArgument;
  }

  data = readReg32(channel, kWiiEXIRegParam);

  data &= ~(kWiiEXIRegParamSelectMask | kWiiEXIRegParamClockMask);
  data |= (1 << kWiiEXIRegParamSelectShift + device) & kWiiEXIRegParamSelectMask;
  data |= (clock << kWiiEXIRegParamClockShift) & kWiiEXIRegParamClockMask;
  if (enableInt) {
    data |= kWiiEXIRegParamIntMask | kWiiEXIRegParamTCIntMask;
  } else {
    data &= ~(kWiiEXIRegParamIntMask | kWiiEXIRegParamTCIntMask);
  }

  writeReg32(channel, kWiiEXIRegParam, data);
  return kIOReturnSuccess;
}

//
// Deselects a device on the EXI bus.
//
// This function is gated and called within the workloop context.
//
void WiiEXI::deselectDevice(UInt32 channel) {
  UInt32 data;

  if (channel >= kWiiEXIMaxChannels) {
    WIIDBGLOG("Invalid channel");
    return;
  }

  data = readReg32(channel, kWiiEXIRegParam);
  data &= ~(kWiiEXIRegParamSelectMask | kWiiEXIRegParamClockMask | kWiiEXIRegParamIntMask | kWiiEXIRegParamTCIntMask);
  writeReg32(channel, kWiiEXIRegParam, data);
}

//
// Reads or writes immediate data to/from EXI.
//
// This function is gated and called within the workloop context.
//
IOReturn WiiEXI::readWriteImmData(UInt32 channel, IODirection direction, UInt32 data, UInt32 dataLength, UInt32 *outData) {
  UInt32 control;

  if ((channel >= kWiiEXIMaxChannels) || (dataLength == 0) || (dataLength > 4)) {
    WIIDBGLOG("Invalid channel/data length");
    return kIOReturnBadArgument;
  }

  if (direction == kIODirectionIn) {
    control = kWiiEXIRegControlTransTypeRead;
  } else if (direction == kIODirectionOut) {
    control = kWiiEXIRegControlTransTypeWrite;
  } else if (direction == kIODirectionInOut) {
    control = kWiiEXIRegControlTransTypeReadWrite;
  } else {
    WIIDBGLOG("Invalid data direction");
    return kIOReturnBadArgument;
  }
  control |= ((dataLength - 1) << kWiiEXIRegControlTransLengthShift) & kWiiEXIRegControlTransLengthMask;
  control |= kWiiEXIRegControlTransStart;

  //
  // Write data if writing.
  //
  if (direction & kIODirectionOut) {
    WIIDBGLOG("Writing 0x%X", data);
    writeReg32(channel, kWiiEXIRegData, data);
  }
  
  //
  // Perform the transfer. TODO: Probably should have a timeout here.
  //
  writeReg32(channel, kWiiEXIRegControl, control);
  do {
    control = readReg32(channel, kWiiEXIRegControl);
  } while (control & kWiiEXIRegControlTransStart);

  //
  // Read data if reading.
  //
  if ((direction & kIODirectionIn) && (outData != NULL)) {
    *outData = readReg32(channel, kWiiEXIRegData);
    WIIDBGLOG("Read 0x%X", *outData);
  }

  return kIOReturnSuccess;
}

//
// Reads the RTC data.
//
// This function is gated and called within the workloop context.
//
IOReturn WiiEXI::readRTCData(UInt32 *secs) {
  IOReturn  status;

  //
  // TODO: Eventually this needs to play nice with other EXI read/writes
  // if this driver supports the EXI slots on the Wii hardware.
  //

  //
  // Select the RTC device and read it.
  //
  status = selectDevice(kWiiEXIRTCChannelNumber, kWiiEXIRTCDeviceNumber, kWiiEXIRTCClock, false);
  if (status != kIOReturnSuccess) {
    return status;
  }

  status = readWriteImmData(kWiiEXIRTCChannelNumber, kIODirectionOut, kWiiEXIRTCBase, sizeof (UInt32), NULL);
  if (status != kIOReturnSuccess) {
    return status;
  }
  status = readWriteImmData(kWiiEXIRTCChannelNumber, kIODirectionIn, 0, sizeof (UInt32), secs);
  if (status != kIOReturnSuccess) {
    return status;
  }

  deselectDevice(kWiiEXIRTCChannelNumber);
  WIIDBGLOG("Got the data 0x%X", *secs);

  return kIOReturnSuccess;
}

//
// Get the RTC seconds in the format OS X is expecting.
//
IOReturn WiiEXI::getRTCGated(UInt32 *secs) {
  UInt32    data1;
  UInt32    data2;
  IOReturn  status;

  //
  // Loop until we get consistent data.
  //
  while (true) {
    status = readRTCData(&data1);
    if (status != kIOReturnSuccess) {
      return status;
    }
    status = readRTCData(&data2);
    if (status != kIOReturnSuccess) {
      return status;
    }

    if (data1 == data2) {
      break;
    }
  }

  if (_isCafe) {
    //
    // Wii U returns UNIX epoch, nothing else to do.
    //
    WIIDBGLOG("RTC data: 0x%X, bias: 0x%X", data1, _rtcBias);
    *secs = data1 + _rtcBias;
  } else {
    return kIOReturnUnsupported;
  }

  return status;
}

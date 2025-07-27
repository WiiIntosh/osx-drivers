//
//  WiiSDHC_Commands.cpp
//  Wii SD host controller interface (card commands)
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include "WiiSDHC.hpp"
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMultiMemoryDescriptor.h>

//
// Common SD and MMC vendors.
// Taken from https://git.kernel.org/pub/scm/linux/kernel/git/cjb/mmc-utils-old.git/tree/lsmmc.c
//
static const SDVendor SDVendors[] = {
  { 0x01, "Panasonic" },
  { 0x02, "Toshiba/Kingston/Viking" },
  { 0x03, "SanDisk" },
  { 0x08, "Silicon Power" },
  { 0x18, "Infineon" },
  { 0x1B, "Transcend" },
  { 0x1C, "Transcend" },
  { 0x1D, "Corsair" },
  { 0x1E, "Transcend" },
  { 0x1F, "Kingston" },
  { 0x27, "PNY" },
  { 0x28, "Lexar" },
  { 0x30, "SanDisk" },
  { 0x33, "STMicroelectronics" },
  { 0x41, "Kingston" },
  { 0x6F, "STMicroelectronics" }
};

static const SDVendor MMCVendors[] = {
  { 0x00, "SanDisk" },
  { 0x02, "Kingston/SanDisk" },
  { 0x03, "Toshiba" },
  { 0x11, "Toshiba" },
  { 0x13, "Micron" },
  { 0x15, "Samsung/SanDisk/LG" },
  { 0x37, "KingMax" },
  { 0x44, "SanDisk" },
  { 0x2C, "Kingston" },
  { 0x70, "Kingston" },
  { 0x90, "Hynix" }
};

inline UInt16 calcPower(UInt8 exp) {
  UInt16 value = 1;
  for (int i = 0; i < exp; i++) {
    value *= 2;
  }
  return value;
}

//
// Selects or deselects the card.
//
IOReturn WiiSDHC::selectDeselectCard(bool select) {
  return sendCommand(kSDCommandSelectDeselectCard, kSDHCResponseTypeR1b, select ? (_cardAddress << kSDRelativeAddressShift) : 0);
}

//
// Get CSD structure from card.
//
IOReturn WiiSDHC::readCardCSD(void) {
  SDCommandResponse *csdResponse;
  IOReturn          status;

  csdResponse = (SDCommandResponse*) &_cardCSD;
  status = sendCommand(kSDCommandSendCSD, kSDHCResponseTypeR2, _cardAddress << kSDRelativeAddressShift, csdResponse);
  if (status != kIOReturnSuccess) {
    return status;
  }
  WIIDBGLOG("CSD: 0x%08X%08X%08X%08X", csdResponse->data[0], csdResponse->data[1], csdResponse->data[2], csdResponse->data[3]);

  //
  // Calculate card size.
  //
  if (isSDCard()) {
    WIIDBGLOG("CSD struct version: 0x%X", _cardCSD.sd1.csdStructure);
    WIIDBGLOG("CSD supported classes: 0x%X", _cardCSD.sd1.ccc);
    WIIDBGLOG("CSD max clock rate: 0x%X", _cardCSD.sd1.tranSpeed);

    //
    // Calculate SD block size in bytes and blocks.
    //
    UInt64 cardBlockBytes;
    if (_cardCSD.sd1.csdStructure == kSDCSDVersion1_0) {
      cardBlockBytes = ((_cardCSD.sd1.cSize + 1) * calcPower(_cardCSD.sd1.cSizeMultiplier + 2)) * calcPower(_cardCSD.sd1.readBLLength);
    } else if (_cardCSD.sd1.csdStructure == kSDCSDVersion2_0) {
      cardBlockBytes = ((UInt64)_cardCSD.sd2.cSize + 1) * (512 * kByte);
    } else {
      WIISYSLOG("Unsupported SD card");
      return kIOReturnUnsupported;
    }
    _cardBlockCount = (UInt32)(cardBlockBytes / kSDBlockSize);
    WIIDBGLOG("Block count: %u (%llu bytes), high capacity: %u", _cardBlockCount, cardBlockBytes, _isCardHighCapacity);

  } else { // TODO: handle MMC cards.
    //
    // Calculate MMC block size in bytes and blocks.
    //
    /*UInt32 cardBlockBytes = ((_cardCSD.mmc.cSize + 1) * calcPower(_cardCSD.mmc.cSizeMultiplier + 2)) * calcPower(_cardCSD.mmc.readBLLength);
    _cardBlockCount = cardBlockBytes / kSDBlockSize;
    WIIDBGLOG("CSD struct version: 0x%X, spec version: 0x%X", _cardCSD.mmc.csdStructure, _cardCSD.mmc.specVersion);
    WIIDBGLOG("CSD CSIZE: 0x%X, CCC: 0x%X", _cardCSD.mmc.cSize, _cardCSD.mmc.ccc);
    WIIDBGLOG("CSD max clock rate: 0x%X", _cardCSD.mmc.tranSpeed);
    WIIDBGLOG("Block count: %u (%llu bytes), high capacity: %u", _cardBlockCount, cardBlockBytes, _isCardHighCapacity);*/

    //
    // Calculate max clock speed for standard mode.
    //
    /*if (_cardCSD.mmc.tranSpeed == kMMCTranSpeed20MHz) {
      _mmcMaxStandardClock = kSDANormalSpeedClock20MHz;
    } else if (_cardCSD.mmc.tranSpeed == kMMCTranSpeed26MHz) {
      _mmcMaxStandardClock = kSDANormalSpeedClock26MHz;
    } else {
      _mmcMaxStandardClock = kSDANormalSpeedClock20MHz; // TODO:
    }*/
   // WIIDBGLOG("MMC maximum clock speed is %u Hz", _mmcMaxStandardClock);
  }

  return kIOReturnSuccess;
}

//
// Sets the card's bus width.
//
IOReturn WiiSDHC::setCardBusWidth(SDBusWidth busWidth) {
  UInt16   val;
  IOReturn status;

  if (isSDCard()) {
    if (busWidth == kSDBusWidth4) {
      val = kSDBusWidth4Bit;
      WIIDBGLOG("Setting card bus width to 4-bit mode");
    } else {
      val = kSDBusWidth1Bit;
      WIIDBGLOG("Setting card bus width to 1-bit mode");
    }

    status = sendAppCommand(kSDAppCommandSetBusWidth, kSDHCResponseTypeR1, val);
  } else {
    // TODO MMC
    status = kIOReturnUnsupported;
  }

  if (status != kIOReturnSuccess) {
    return status;
  }

  //
  // Controller needs to match.
  //
  setControllerBusWidth(busWidth);
  return kIOReturnSuccess;
}

//
// Sets the card's block length.
//
IOReturn WiiSDHC::setCardBlockLength(UInt16 blockLength) {
  IOReturn status;

  status = sendCommand(kSDCommandSetBlockLength, kSDHCResponseTypeR1, blockLength);
  if (status == kIOReturnSuccess) {
    _cardBlockLength = blockLength;
  }
  return status;
}

//
// Resets the inserted card.
//
IOReturn WiiSDHC::resetCard(void) {
  SDCommandResponse   sdResponse;
  SDCommandResponse   cidResponse;
  UInt8               vendorId;
  IOReturn            status;
  
  //
  // Assume card installed is a v2 SD card.
  // The first reset command is identical for all types.
  //
  _cardType         = kSDCardTypeSD_200;
  _cardAddress      = 0;
  _cardBlockLength  = kSDBlockSize;

  //
  // Send card to IDLE state.
  //
  status = sendCommand(kSDCommandGoIdleState, kSDHCResponseTypeR0, 0);
  if (status != kIOReturnSuccess) {
    return status;
  }
  WIIDBGLOG("Card has been reset and should be in IDLE status");

  //
  // Issue SEND_IF_COND to card.
  // If no response, this is either is SD 1.0 or MMC.
  //
  status = sendCommand(kSDCommandSendIfCond, kSDHCResponseTypeR7, 0x1AA, &sdResponse);
  if (status == kIOReturnTimeout) {
    WIIDBGLOG("Card did not respond to SEND_IF_COND, not an SD 2.00 card");
    _cardType = kSDCardTypeSD_Legacy;
  } else if (status != kIOReturnSuccess) {
    return status;
  }

  //
  // Issue SD card initialization command.
  //
  WIIDBGLOG("Initializing %s card", _cardType == kSDCardTypeSD_Legacy ? "MMC or legacy SD" : "SD 2.00");
  for (int i = 0; i < 20; i++) {
    status = sendAppCommand(kSDAppCommandSendOpCond, kSDHCResponseTypeR3, kSDOCRInitValue, &sdResponse);

    //
    // No response indicates an MMC card.
    //
    if (status == kIOReturnTimeout && _cardType == kSDCardTypeSD_Legacy) {
      WIIDBGLOG("Card did not respond to SEND_OP_COND, not an SD card");
      _cardType = kSDCardTypeMMC;
      break;
    } else if (status != kIOReturnSuccess) {
      return status;
    }

    if (sdResponse.r1 & kSDOCRCardBusy) {
      break;
    }

    //
    // Spec indicates to wait 1sec between attempts.
    //
    IOSleep(1000);
  }

  //
  // Check if SD card has started.
  //
  //
  // If card is still not ready, abort.
  //
  if (!(sdResponse.r1 & kSDOCRCardBusy)) {
    WIISYSLOG("Timed out initializing card");
    return kIOReturnTimeout;
  }
  _isCardHighCapacity = sdResponse.r1 & kSDOCRCCSHighCapacity;

  WIIDBGLOG("Got SD card, OCR: 0x%X", sdResponse.r1);

  //
  // Get CID from card.
  //
  status = sendCommand(kSDCommandAllSendCID, kSDHCResponseTypeR2, 0, &cidResponse);
  if (status != kIOReturnSuccess) {
    return status;
  }

  if (isSDCard()) {
    //
    // Ask card to send address.
    //
    status = sendCommand(kSDCommandSendRelativeAddress, kSDHCResponseTypeR6, 0, &sdResponse);
    if (status != kIOReturnSuccess) {
        return status;
    }
    _cardAddress = sdResponse.r1 >> kSDRelativeAddressShift;
  }

  WIIDBGLOG("Card @ 0x%X has CID of 0x%08X%08X%08X%08X", _cardAddress,
    cidResponse.data[0], cidResponse.data[1], cidResponse.data[2], cidResponse.data[3]);

  memcpy(&_cardCID, cidResponse.data, sizeof (_cardCID));

  //
  // Get card product name, SN, and revision strings from CID.
  //
  if (isSDCard()) {
    _cardProductName[0] = (char)_cardCID.sd.name[0];
    _cardProductName[1] = (char)_cardCID.sd.name[1];
    _cardProductName[2] = (char)_cardCID.sd.name[2];
    _cardProductName[3] = (char)_cardCID.sd.name[3];
    _cardProductName[4] = (char)_cardCID.sd.name[4];
    _cardProductName[5] = '\0';
    _cardProductName[6] = '\0';

    vendorId = _cardCID.sd.manufacturerId;// 0x00275048534433324760DA5D9BB0016B
    snprintf(_cardSN, sizeof (_cardSN), "%lu", _cardCID.sd.serialNumber);
    snprintf(_cardRev, sizeof (_cardRev), "%u.%u", _cardCID.sd.revisionMajor, _cardCID.sd.revisionMinor);

    WIIDBGLOG("Mfg Date: %u/%u, SN: %s, Rev: %s, OEM ID: 0x%X, Mfg ID: 0x%X",
              _cardCID.sd.manufactureMonth, _cardCID.sd.manufactureYear, _cardSN,
              _cardRev, _cardCID.sd.oemId, _cardCID.sd.manufacturerId);
  }

  //
  // Lookup vendor string from card vendor ID.
  //
  _cardVendorName = kIOBlockStorageDeviceTypeGeneric;
  if (isSDCard()) {
    for (unsigned int i = 0; i < ARRSIZE(SDVendors); i++) {
      if (SDVendors[i].manufacturerId == vendorId) {
        _cardVendorName = SDVendors[i].name;
        break;
      }
    }
  } else {
    for (unsigned int i = 0; i < ARRSIZE(MMCVendors); i++) {
      if (MMCVendors[i].manufacturerId == vendorId) {
        _cardVendorName = MMCVendors[i].name;
        break;
      }
    }
  }
  WIIDBGLOG("Found %s card %s %s", isSDCard() ? "SD" : "MMC", _cardVendorName, _cardProductName);

  return kIOReturnSuccess;
} 

//
// Initializes the inserted card.
//
IOReturn WiiSDHC::initCard(void) {
  IOReturn status;

  //
  // Check if card is present.
  //
  if (!isCardPresent()) {
    WIIDBGLOG("No card is currently inserted");
    _isCardPresent = false;
    return kIOReturnNoMedia;
  }
  _isCardPresent = false; // TODO

  //
  // Reset to initialization clock and power on the card.
  //
  status = setControllerClock(kSDHCInitSpeedClock400kHz);
  if (status != kIOReturnSuccess) {
    return status;
  }
  setControllerPower(true);

  status = resetCard();
  if (status != kIOReturnSuccess) {
    return status;
  }

  status = readCardCSD();
  if (status != kIOReturnSuccess) {
    return status;
  }

  status = setControllerClock(kSDHCNormalSpeedClock25MHz);
  if (status != kIOReturnSuccess) {
    return status;
  }

  status = selectDeselectCard(true);
  if (status != kIOReturnSuccess) {
    return status;
  }

  status = setCardBusWidth(kSDBusWidth4);
  if (status != kIOReturnSuccess) {
    return status;
  }

  status = setCardBlockLength(kSDBlockSize);
  if (status != kIOReturnSuccess) {
    return status;
  }

  IOSleep(1000); // TODO: even needed?
  return kIOReturnSuccess;
}

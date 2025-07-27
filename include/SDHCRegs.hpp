//
//  SDHCRegs.hpp
//  SD Host Controller register definitions
//
//  See the SD Host Controller spec v2.0 at sdcard.org
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef SDHCRegs_hpp
#define SDHCRegs_hpp

#include "WiiCommon.hpp"

#define kSDHCMaskTimeout           100000

//
// Timeouts.
//
#define kSDHCResetTimeoutMS     (100 * kWiiMicrosecondMS)
#define kSDHCClockTimeoutMS     (2000 * kWiiMicrosecondMS)
#define kSDHCCommandTimeoutMS   (5000 * kWiiMicrosecondMS)

#define kSDHCInitialCommandPoolSize   10

#define kSDProductNameLength      7
#define kSDSerialNumLength        12
#define kSDRevisionLength         4
#define kSDBlockSize              512

//
// SD Host Controller versions.
//
typedef enum {
  kSDHCVersion1_00  = 0x00,
  kSDHCVersion2_00  = 0x01,
  kSDHCVersion3_00  = 0x02
} SDHCVersion;

//
// Card speeds.
//
#define kSDHCInitSpeedClock400kHz         (400 * kHz)
#define kSDHCNormalSpeedClock20MHz        (20 * MHz)
#define kSDHCNormalSpeedClock25MHz        (25 * MHz)
#define kSDHCNormalSpeedClock26MHz        (26 * MHz)
#define kSDHCHighSpeedClock25MHz          (50 * MHz)

//
// Bus widths.
//
typedef enum {
  kSDBusWidth1,
  kSDBusWidth4,
  kSDBusWidth8
} SDBusWidth;

#define kSDBusWidth1Bit     0
#define kSDBusWidth4Bit     2

//
// Card types.
//
typedef enum {
  // SD card compliant with physical layer version 2.00.
  kSDCardTypeSD_200,
  // Legacy SD card.
  kSDCardTypeSD_Legacy,
  // MMC card.
  kSDCardTypeMMC
} SDCardType;

//
// Used for vendor string lookups.
//
typedef struct {
  UInt8      manufacturerId;
  const char *name;
} SDVendor;

//
// Command response (128 bits).
//
typedef struct {
  union {
    UInt32 data[4];
    struct {
      UInt32 padding[3];
      UInt32 r1;
    };
  };
} SDCommandResponse;

//
// SD host controller registers.
// All are 32-bit and normally are little endian, but on Wii they are big endian.
// Cards are little endian.
//
// DMA address.
#define kSDHCRegSDMA                            0x00
// Block size.
#define kSDHCRegBlockSize                       0x04
// Block count.
#define kSDHCRegBlockCount                      0x06
// Command argument.
#define kSDHCRegArgument                        0x08
// Transfer mode.
#define kSDHCRegTransferMode                    0x0C
#define kSDHCRegTransferModeDMAEnable           BIT0
#define kSDHCRegTransferModeBlockCountEnable    BIT1
#define kSDHCRegTransferModeAutoCMD12           BIT2
#define kSDHCRegTransferModeDataTransferRead    BIT4
#define kSDHCRegTransferModeMultipleBlock       BIT5
// Command register.
#define kSDHCRegCommand                         0x0E
#define kSDHCRegCommandResponseLength136        BIT0
#define kSDHCRegCommandResponseLength48         BIT1
#define kSDHCRegCommandResponseLength48Busy     (BIT0 | BIT1)
#define kSDHCRegCommandCRCEnable                BIT3
#define kSDHCRegCommandIndexCheckEnable         BIT4
#define kSDHCRegCommandDataPresent              BIT5
#define kSDHCRegCommandIndexShift               8
#define kSDHCRegCommandIndexMask                0x3F00
// Response register, 128 bits total.
#define kSDHCRegResponse0                       0x10
#define kSDHCRegResponse1                       0x14
#define kSDHCRegResponse2                       0x18
#define kSDHCRegResponse3                       0x1C
// Data port for PIO access.
#define kSDHCRegBufferDataPort                  0x20
// Present state of controller.
#define kSDHCRegPresentState                    0x24
#define kSDHCRegPresentStateCmdInhibit          BIT0
#define kSDHCRegPresentStateDatInhibit          BIT1
#define kSDHCRegPresentStateDatActive           BIT2
#define kSDHCRegPresentStateWriteActive         BIT8
#define kSDHCRegPresentStateReadActive          BIT9
#define kSDHCRegPresentStateBufferWriteEnable   BIT10
#define kSDHCRegPresentStateBufferReadEnable    BIT11
#define kSDHCRegPresentStateCardInserted        BIT16
#define kSDHCRegPresentStateCardStable          BIT17
#define kSDHCRegPresentStateCardWriteable       BIT19
// Host control register.
#define kSDHCRegHostControl1                    0x28
#define kSDHCRegHostControl1LEDOn               BIT0
#define kSDHCRegHostControl1DataWidth4Bit       BIT1
#define kSDHCRegHostControl1HighSpeedEnable     BIT2
#define kSDHCRegHostControl1DataWidth8Bit       BIT5
#define kSDHCRegHostControl1DataWidthMask       (kSDHCRegHostControl1DataWidth4Bit | kSDHCRegHostControl1DataWidth8Bit)
// Power control register.
#define kSDHCRegPowerControl                    0x29
#define kSDHCRegPowerControlVDD1On              BIT0
#define kSDHCRegPowerControlVDD1_3_3            (BIT1 | BIT2 | BIT3)
#define kSDHCRegPowerControlVDD1_3_0            (BIT2 | BIT3)
#define kSDHCRegPowerControlVDD1_1_8            (BIT1 | BIT3)
// Block gap control.
#define kSDHCRegBlockGapControl                 0x2A
// Wakeup control.
#define kSDHCRegWakeupControl                   0x2B
// Clock control.
#define kSDHCRegClockControl                        0x2C
#define kSDHCRegClockControlIntClockEnable          BIT0
#define kSDHCRegClockControlIntClockStable          BIT1
#define kSDHCRegClockControlSDClockEnable           BIT2
#define kSDHCRegClockControlFreqSelectLowShift      8
#define kSDHCRegClockControlFreqSelectLowMask       0xFF00
#define kSDHCRegClockControlFreqSelectHighRhShift   2
#define kSDHCRegClockControlFreqSelectHighMask      0xC0
// Timeout control.
#define kSDHCRegTimeoutControl                  0x2E
// Software reset register.
#define kSDHCRegSoftwareReset                   0x2F
#define kSDHCRegSoftwareResetAll                BIT0
#define kSDHCRegSoftwareResetCmd                BIT1
#define kSDHCRegSoftwareResetDat                BIT2
// Interrupt status.
#define kSDHCRegNormalIntStatus                   0x30
#define kSDHCRegNormalIntStatusCommandComplete    BIT0
#define kSDHCRegNormalIntStatusTransferComplete   BIT1
#define kSDHCRegNormalIntStatusBlockGapEvent      BIT2
#define kSDHCRegNormalIntStatusDMAInterrupt       BIT3
#define kSDHCRegNormalIntStatusBufferWriteReady   BIT4
#define kSDHCRegNormalIntStatusBufferReadReady    BIT5
#define kSDHCRegNormalIntStatusCardInsertion      BIT6
#define kSDHCRegNormalIntStatusCardRemoval        BIT7
#define kSDHCRegNormalIntStatusCardInterrupt      BIT8
#define kSDHCRegNormalIntStatusErrorInterrupt     BIT15
// Error interrupt status.
#define kSDHCRegErrorIntStatus                    0x32
#define kSDHCRegErrorIntStatusCommandTimeout      BIT0
#define kSDHCRegErrorIntStatusCommandCRC          BIT1
#define kSDHCRegErrorIntStatusCommandEndBit       BIT2
#define kSDHCRegErrorIntStatusCommandIndex        BIT3
#define kSDHCRegErrorIntStatusDataTimeout         BIT4
#define kSDHCRegErrorIntStatusDataCRC             BIT5
#define kSDHCRegErrorIntStatusDataEndBit          BIT6
#define kSDHCRegErrorIntStatusCurrentLimit        BIT7
#define kSDHCRegErrorIntStatusAutoCMD12           BIT8
// Interrupt enable.
#define kSDHCRegNormalIntStatusEnable             0x34
// Error interrupt enable.
#define kSDHCRegErrorIntStatusEnable              0x36
// Interrupt signal enable.
#define kSDHCRegNormalIntSignalEnable             0x38
// Error interrupt signal enable.
#define kSDHCRegErrorIntSignalEnable              0x3A
// Auto CMD12 error status.
#define kSDHCRegAutoCmd12ErrorStatus              0x3C
// Capabilities.
#define kSDHCRegCapabilities                      0x40
#define kSDHCRegCapabilitiesTimeoutClockMHz       BIT7
#define kSDHCRegCapabilitiesBaseClockMaskVer1     0x3F00
#define kSDHCRegCapabilitiesBaseClockMaskVer3     0xFF00
#define kSDHCRegCapabilitiesBaseClockShift        8
#define kSDHCRegCapabilitiesMaxBlockLength1024    BIT16
#define kSDHCRegCapabilitiesMaxBlockLength2048    BIT17
#define kSDHCRegCapabilitiesHighSpeedSupported    BIT21
#define kSDHCRegCapabilitiesSDMASupported         BIT22
#define kSDHCRegCapabilitiesSuspendSupported      BIT23
#define kSDHCRegCapabilitiesVoltage3_3Supported   BIT24
#define kSDHCRegCapabilitiesVoltage3_0Supported   BIT25
#define kSDHCRegCapabilitiesVoltage1_8Supported   BIT26
// Maximum power current capabilities.
#define kSDHCRegMaxCurrentCapabilities            0x48
// Slot interrupt status.
#define kSDHCRegHostControllerSlotIntStatus   0xFC
// Controller version.
#define kSDHCRegHostControllerVersion         0xFE
#define kSDHCRegHostControllerVersionMask     0xFF

//
// OCR bits
// Initial value indicates support for all voltages and high capacity.
//
#define kSDOCRCCSHighCapacity     BIT30
#define kSDOCRCardBusy            BIT31
#define kSDOCRInitValue           (kSDOCRCCSHighCapacity | 0xFF8000)

#define kSDRelativeAddressShift   16

//
// SD commands.
//
typedef enum {
  //
  // Basic commands (class 0).
  //
  kSDCommandGoIdleState           = 0,
  kSDCommandAllSendCID            = 2,
  kSDCommandSendRelativeAddress   = 3,
  kSDCommandSetDSR                = 4,
  kSDCommandSelectDeselectCard    = 7,
  kSDCommandSendIfCond            = 8,
  kSDCommandSendCSD               = 9,
  kSDCommandSendCID               = 10,
  kSDCommandVoltageSwitch         = 11,
  kSDCommandStopTransmission      = 12,
  kSDCommandSendStatus            = 13,
  kSDCommandGoInactiveState       = 15,

  //
  // Block read and write commands (class 2 and class 4).
  //
  kSDCommandSetBlockLength        = 16,
  kSDCommandReadSingleBlock       = 17,
  kSDCommandReadMultipleBlock     = 18,
  kSDCommandSendTuningBlock       = 19,
  kSDCommandSpeedClassControl     = 20,
  kSDCommandAddressExtension      = 22,
  kSDCommandSetBlockCount         = 23,
  kSDCommandWriteSingleBlock      = 24,
  kSDCommandWriteMultipleBlock    = 25,
  kSDCommandProgramCSD            = 27,

  //
  // Block write protection commands (class 6).
  //
  kSDCommandSetWriteProtect       = 28,
  kSDCommandClearWriteProtect     = 29,
  kSDCommandSendWriteProtect      = 30,

  //
  // Erase commands (class 5).
  //
  kSDCommandEraseWriteBlockStart  = 32,
  kSDCommandEraseWriteBlockEnd    = 33,
  kSDCommandErase                 = 38,

  //
  // Lock commands (clsas 7).
  //
  kSDCommandLockUnlock            = 42,

  //
  // Application commands (class 8).
  //
  kSDCommandAppCommand            = 55,
  kSDCommandGeneralCommand        = 56
} SDCommand;

//
// SD application commands.
//
typedef enum {
  kSDAppCommandSetBusWidth              = 6,
  kSDAppCommandSDStatus                 = 13,
  kSDAppCommandSendNumWrBlocks          = 22,
  kSDAppCommandSetWrBlkEraseCount       = 23,
  kSDAppCommandSendOpCond               = 41,
  kSDAppCommandSetClearCardDetect       = 42,
  kSDAppCommandSendSCR                  = 51
} SDAppCommand;

//
// SD Host Controller response flags.
//
typedef enum {
  // No response.
  kSDHCResponseTypeR0     = 0,
  // Standard response.
  kSDHCResponseTypeR1     = kSDHCRegCommandResponseLength48 | kSDHCRegCommandCRCEnable | kSDHCRegCommandIndexCheckEnable,
  // Standard response, wait for busy to clear.
  kSDHCResponseTypeR1b    = kSDHCRegCommandResponseLength48Busy | kSDHCRegCommandCRCEnable | kSDHCRegCommandIndexCheckEnable,
  // CID and CSD response.
  kSDHCResponseTypeR2     = kSDHCRegCommandResponseLength136 | kSDHCRegCommandCRCEnable,
  // OCR response.
  kSDHCResponseTypeR3     = kSDHCRegCommandResponseLength48,
  kSDHCResponseTypeR4     = kSDHCRegCommandResponseLength48,
  kSDHCResponseTypeR5     = kSDHCRegCommandResponseLength48 | kSDHCRegCommandCRCEnable | kSDHCRegCommandIndexCheckEnable,
  kSDHCResponseTypeR5b    = kSDHCRegCommandResponseLength48Busy | kSDHCRegCommandCRCEnable | kSDHCRegCommandIndexCheckEnable,
  // RCA response.
  kSDHCResponseTypeR6     = kSDHCRegCommandResponseLength48 | kSDHCRegCommandCRCEnable | kSDHCRegCommandIndexCheckEnable,
  // SEND_IF_COND response.
  kSDHCResponseTypeR7     = kSDHCRegCommandResponseLength48 | kSDHCRegCommandCRCEnable | kSDHCRegCommandIndexCheckEnable
} SDHCResponseType;
#define kSDHCResponseTypeMask     0x1B


#pragma pack(push, 1)

//
// SD CID register struct order-swapped for big endian.
// CRC is stripped by the controller, but need to include padding here.
//
typedef struct {
  UInt8   padding;
  UInt8   manufacturerId;
  UInt16  oemId;
  UInt8   name[5];
  UInt8   revisionMajor : 4;
  UInt8   revisionMinor : 4;
  UInt32  serialNumber;
  UInt8   reserved : 4;
  UInt8   manufactureYear : 8;
  UInt8   manufactureMonth : 4;
} SDCIDRegister;
OSCompileAssert(sizeof (SDCIDRegister) == 16);

//
// SD CSD versions.
//
// V1.0 - SDSC
// V2.0 - SDHC and SDXC
//
#define kSDCSDVersion1_0    0
#define kSDCSDVersion2_0    1

//
// SD CSD register v1.0 struct order-swapped for big endian.
// CRC is stripped by the controller, but need to include padding here.
//
typedef struct {
  UInt8   padding;

  UInt8   csdStructure : 2;
  UInt8   reserved5 : 6;
  UInt8   taac;
  UInt8   nsac;
  UInt8   tranSpeed;
  UInt16  ccc : 12;
  UInt8   readBLLength : 4;
  UInt8   readBLPartial : 1;
  UInt8   writeBlockMisalign : 1;
  UInt8   readBlockMisalign : 1;
  UInt8   dsrImplemented : 1;

  UInt8   reserved4 : 2;
  UInt16  cSize : 12;
  UInt8   vddReadCurrentMin : 3;
  UInt8   vddReadCurrentMax : 3;
  UInt8   vddWriteCurrentMin : 3;
  UInt8   vddWriteCurrentMax : 3;
  UInt8   cSizeMultiplier : 3;
  UInt8   eraseBlockEnable : 1;
  UInt8   eraseSectorSize : 7;
  UInt8   writeProtectGroupSize : 7;
  UInt8   writeProtectGroupEnable : 1;

  UInt8   reserved3 : 2;
  UInt8   writeSpeedFactor : 3;
  UInt8   writeBLLength : 4;
  UInt8   writeBLPartial : 1;

  UInt8   reserved2 : 5;
  UInt8   fileFormatGroup : 1;
  UInt8   copy : 1;
  UInt8   permWriteProtect : 1;
  UInt8   tmpWriteProtect : 1;
  UInt8   fileFormat : 2;
  UInt8   reserved1 : 2;
} SDCSDRegisterV1;
OSCompileAssert(sizeof (SDCSDRegisterV1) == 16);

//
// SD CSD register v2.0 struct order-swapped for big endian.
// CRC is stripped by the controller, but need to include padding here.
//
typedef struct {
  UInt8   padding;

  UInt8   csdStructure : 2;
  UInt8   reserved6 : 6;
  UInt8   taac;
  UInt8   nsac;
  UInt8   tranSpeed;
  UInt16  ccc : 12;
  UInt8   readBLLength : 4;
  UInt8   readBLPartial : 1;
  UInt8   writeBlockMisalign : 1;
  UInt8   readBlockMisalign : 1;
  UInt8   dsrImplemented : 1;

  UInt8   reserved5 : 6;
  UInt32  cSize : 22;

  UInt8   reserved4 : 1;
  UInt8   eraseBlockEnable : 1;
  UInt8   eraseSectorSize : 7;
  UInt8   writeProtectGroupSize : 7;
  UInt8   writeProtectGroupEnable : 1;

  UInt8   reserved3 : 2;
  UInt8   writeSpeedFactor : 3;
  UInt8   writeBLLength : 4;
  UInt8   writeBLPartial : 1;

  UInt8   reserved2 : 5;
  UInt8   fileFormatGroup : 1;
  UInt8   copy : 1;
  UInt8   permWriteProtect : 1;
  UInt8   tmpWriteProtect : 1;
  UInt8   fileFormat : 2;
  UInt8   reserved1 : 2;
} SDCSDRegisterV2;
OSCompileAssert(sizeof (SDCSDRegisterV2) == 16);

#pragma pack(pop)

#endif

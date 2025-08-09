//
//  EXIRegs.hpp
//  Wii and Wii U EXI registers
//
// See https://wiibrew.org/wiki/Hardware/External_Interface.
// See https://www.gc-forever.com/yagcd/chap5.html#sec5.9.
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef EXIRegs_hpp
#define EXIRegs_hpp

#define kWiiEXIMaxChannels            3
#define kWiiEXIMaxDevices             3

typedef enum {
  kWiiEXIClock1MHz    = 0,
  kWiiEXIClock2MHz    = 1,
  kWiiEXIClock4MHz    = 2,
  kWiiEXIClock8MHz    = 3,
  kWiiEXIClock16MHz   = 4,
  kWiiEXIClock32MHz   = 5
} WiiEXIClock;

//
// Registers.
// Each channel has same registers (0 - 2).
//
#define kWiiEXIRegParam             0x00
#define kWiiEXIRegParamIntMask      BIT0
#define kWiiEXIRegParamIntStatus    BIT1
#define kWiiEXIRegParamTCIntMask    BIT2
#define kWiiEXIRegParamTCIntStatus  BIT3
#define kWiiEXIRegParamClockMask    BITRange(4, 6)
#define kWiiEXIRegParamClockShift   4
#define kWiiEXIRegParamSelectMask   BITRange(7, 9)
#define kWiiEXIRegParamSelectShift  7
#define kWiiEXIRegParamExtIntMask   BIT10
#define kWiiEXIRegParamExtIntStatus BIT11
#define kWiiEXIRegParamExtConn      BIT12
#define kWiiEXIRegParamRomDis       BIT13

#define kWiiEXIRegDMAStart        0x04
#define kWiiEXIRegDMALength       0x08

#define kWiiEXIRegControl                   0x0C
#define kWiiEXIRegControlTransStart         BIT0
#define kWiiEXIRegControlDMAEnable          BIT1
#define kWiiEXIRegControlTransTypeRead      0
#define kWiiEXIRegControlTransTypeWrite     BIT2
#define kWiiEXIRegControlTransTypeReadWrite BIT3
#define kWiiEXIRegControlTransTypeMask      (BIT2 | BIT3)
#define kWiiEXIRegControlTransLengthShift   4
#define kWiiEXIRegControlTransLengthMask    (BIT4 | BIT5)

#define kWiiEXIRegData            0x10

#define kWiiEXIChannelRegOffset   0x14

//
// RTC device.
//
#define kWiiEXIRTCChannelNumber     0
#define kWiiEXIRTCDeviceNumber      1
#define kWiiEXIRTCBase              0x20000000
#define kWiiEXISRAMBase             0x20000100
#define kWiiEXIRTCWrite             0x80000000
#define kWiiEXIRTCClock             kWiiEXIClock8MHz

typedef struct {
  UInt16  checksum[2];
  UInt32  ead[2];
  SInt32  counterBias;
  SInt8   displayOffsetH;  
  UInt8   ntd;
  UInt8   language;
  UInt8   flags;
  UInt16  flashId[12];
  UInt32  wirelessKeyboardId;
  UInt32  wirelessPadId[2];
  UInt8   lastDVDErrorCode;
  UInt8   padding1;
  UInt16  flashIdChecksum[2];
  UInt16  padding2;
} WiiSRAM;
OSCompileAssert(sizeof (WiiSRAM) == 64);

#endif

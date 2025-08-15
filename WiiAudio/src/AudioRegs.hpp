//
//  AudioRegs.hpp
//  Wii audio registers
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef AudioRegs_hpp
#define AudioRegs_hpp

#include "WiiCommon.hpp"

#define kWiiAudioNumChannels        2
#define kWiiAudioBitWidth           16

//
// Audio interface registers.
//
#define kWiiAudioIntRegControl                0x00
#define kWiiAudioIntRegControlPlayStatus      BIT0
#define kWiiAudioIntRegControlAuxFreq32KHz    BIT1
#define kWiiAudioIntRegControlIntEnable       BIT2
#define kWiiAudioIntRegControlIntStatus       BIT3
#define kWiiAudioIntRegControlIntValid        BIT4
#define kWiiAudioIntRegControlResetSampleCnt  BIT5
#define kWiiAudioIntRegControlDspFreq32kHz    BIT6

#define kWiiAudioIntRegVolume                 0x04
#define kWiiAudioIntRegVolumeLeftMask         BITRange(0, 7)
#define kWiiAudioIntRegVolumeRightShift       8
#define kWiiAudioIntRegVolumeRightMask        BITRange(8, 15)

#define kWiiAudioIntRegSampleCounter          0x08
#define kWiiAudioIntRegInterruptTiming        0x0C

//
// DSP registers.
//
#define kWiiAudioDspRegDspMailboxHigh         0x00
#define kWiiAudioDspRegDspMailboxLow          0x02
#define kWiiAudioDspRegCpuMailboxHigh         0x04
#define kWiiAudioDspRegCpuMailboxLow          0x06

// DSP control/status register.
#define kWiiAudioDspRegControlStatus                      0x0A
// Reset DSP.
#define kWiiAudioDspRegControlStatusReset                 BIT0
//
#define kWiiAudioDspRegControlStatusDspIntEnable          BIT1
// Halt DSP.
#define kWiiAudioDspRegControlStatusHalt                  BIT2
// Audio interface interrupt active. Set to clear.
#define kWiiAudioDspRegControlStatusAudioIntStatus        BIT3
// Audio interface interrupt enabled.
#define kWiiAudioDspRegControlStatusAudioIntEnable        BIT4
// ARAM interrupt active. Set to clear.
#define kWiiAudioDspRegControlStatusAramIntStatus         BIT5
// ARAM interrupt enabled.
#define kWiiAudioDspRegControlStatusAramIntEnable         BIT6
// DSP interrupt active. Set to clear.
#define kWiiAudioDspRegControlStatusDspIntStatus          BIT7
// SMA interrupt enabled.
#define kWiiAudioDspRegControlStatusDmaIntEnable          BIT8
// DMA interrupt active. Set to clear.
#define kWiiAudioDspRegControlStatusDmaIntStatus          BIT9
// DMA transfer in progress.
#define kWiiAudioDspRegControlStatusDmaStatus             BIT10
// Boot from IROM on reset.
#define kWiiAudioDspRegControlStatusBootMode              BIT11
// Audio interface (TV) interrupt active. Set to clear.
#define kWiiAudioDspRegControlStatusLatteAudioIntStatus   BIT12
// Audio interface (TV) interrupt enabled.
#define kWiiAudioDspRegControlStatusLatteAudioIntEnable   BIT13

#define kWiiAudioDspRegDmaStartHigh           0x30
#define kWiiAudioDspRegDmaStartLow            0x32
#define kWiiAudioDspRegDmaLength              0x36
#define kWiiAudioDspRegDmaLengthPlay          BIT15
#define kWiiAudioDspRegDmaBytesLeft           0x3A

#define kWiiAudioDspRegLatteDmaStartHigh      0x40
#define kWiiAudioDspRegLatteDmaStartLow       0x42
#define kWiiAudioDspRegLatteDmaLength         0x46
#define kWiiAudioDspRegLatteDmaLengthPlay     BIT15
#define kWiiAudioDspRegLatteDmaBytesLeft      0x4A

#endif

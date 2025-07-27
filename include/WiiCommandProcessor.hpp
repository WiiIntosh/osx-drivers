//
//  WiiCommandProcessor.hpp
//  Wii command processor MMIO registers
//
// See https://wiibrew.org/wiki/Hardware//Command_Processor.
// See https://www.gc-forever.com/yagcd/chap5.html#sec5.1.
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiCommandProcessor_hpp
#define WiiCommandProcessor_hpp

#include "WiiCommon.hpp"

//
// Base address for command processor.
//
#define kWiiCommandProcBaseAddress    0x0C000000
#define kWiiCommandProcBaseLength     0x80

//
// Command processor registers.
// All registers are 16-bit.
//
#define kWiiCommandProcRegStatus                  0x00
#define kWiiCommandProcRegStatusFifoOverflow      BIT0
#define kWiiCommandProcRegStatusFifoUnderflow     BIT1
#define kWiiCommandProcRegStatusReadIdle          BIT2
#define kWiiCommandProcRegStatusCommandIdle       BIT3
#define kWiiCommandProcRegStatusBreakpoint        BIT4

#define kWiiCommandProcRegControl                         0x02
#define kWiiCommandProcRegControlFifoReadEnable           BIT0
#define kWiiCommandProcRegControlIntEnable                BIT1
#define kWiiCommandProcRegControlFifoOverflowIntEnable    BIT2
#define kWiiCommandProcRegControlFifoUnderflowIntEnable   BIT3
#define kWiiCommandProcRegControlLinkEnable               BIT4
#define kWiiCommandProcRegControlBypassProcessorEnable    BIT5

#define kWiiCommandProcRegClear                   0x04
#define kWiiCommandProcRegClearFifoOverflow       BIT0
#define kWiiCommandProcRegClearFifoUnderflow      BIT1

#define kWiiCommandProcRegToken                   0x0E
#define kWiiCommandProcRegBoundingBoxLeft         0x10
#define kWiiCommandProcRegBoundingBoxRight        0x12
#define kWiiCommandProcRegBoundingBoxTop          0x14
#define kWiiCommandProcRegBoundingBoxBottom       0x16
#define kWiiCommandProcRegFifoBaseLow             0x20
#define kWiiCommandProcRegFifoBaseHigh            0x22
#define kWiiCommandProcRegFifoEndLow              0x24
#define kWiiCommandProcRegFifoEndHigh             0x26
#define kWiiCommandProcRegFifoHighWatermarkLow    0x28
#define kWiiCommandProcRegFifoHighWatermarkHigh   0x2A
#define kWiiCommandProcRegFifoLowWatermarkLow     0x2C
#define kWiiCommandProcRegFifoLowWatermarkHigh    0x2E
#define kWiiCommandProcRegFifoReadWriteDistLow    0x30
#define kWiiCommandProcRegFifoReadWriteDistHigh   0x32
#define kWiiCommandProcRegFifoWritePointerLow     0x34
#define kWiiCommandProcRegFifoWritePointerHigh    0x36
#define kWiiCommandProcRegFifoReadPointerLow      0x38
#define kWiiCommandProcRegFifoReadPointerHigh     0x3A
#define kWiiCommandProcRegFifoBypassLow           0x3C
#define kWiiCommandProcRegFifoBypassHigh          0x3E

#endif

//
//  WiiProcessorInterface.hpp
//  Wii processor interface MMIO registers
//
// See https://wiibrew.org/wiki/Hardware/Processor_Interface.
// See https://wiiubrew.org/wiki/Hardware/Processor_interface.
// See https://www.gc-forever.com/yagcd/chap5.html#sec5.4.
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiProcessorInterface_hpp
#define WiiProcessorInterface_hpp

#include "WiiCommon.hpp"

//
// Base address for processor interface.
//
#define kWiiPIBaseAddress             0x0C003000
#define kWiiPIBaseLength              0x100

//
// 32 supported interrupts.
//
#define kWiiPIVectorCount             32

//
// 3 cores on Cafe.
//
#define kWiiPICafeCoreCount           3

//
// Interrupt list.
//
#define kWiiPIVectorGPRuntimeError    0
#define kWiiPIVectorResetSwitch       1
#define kWiiPIVectorDVD               2
#define kWiiPIVectorSerial            3
#define kWiiPIVectorEXI               4
#define kWiiPIVectorAudioInterface    5
#define kWiiPIVectorDSP               6
#define kWiiPIVectorMemoryInterface   7
#define kWiiPIVectorVideoInterface    8
#define kWiiPIVectorPixelEngineToken  9
#define kWiiPIVectorPixelEngineFinish 10
#define kWiiPIVectorCommandProcessor  11
#define kWiiPIVectorDebugger          12
#define kWiiPIVectorHighspeedPort     13
#define kWiiPIVectorHollywood         14

//
// Processor interface registers.
// All registers are 32-bit.
//
// Interrupt cause (Wii only).
// Each bit represents an interrupt.
#define kWiiPIRegInterruptCause               0x00
#define kWiiPIRegInterruptCauseResetSwState   BIT16
// Interrupt mask (Wii only).
// Each bit represents an interrupt. Bit needs to be set to enable interrupt.
#define kWiiPIRegInterruptMask                0x04
// FIFO (Wii only).
#define kWiiPIRegFifoBaseStart                0x0C
#define kWiiPIRegFifoBaseEnd                  0x10
#define kWiiPIRegFifoCurrentWritePointer      0x14

// Interrupt cause/mask (Wii U).
#define kWiiPIRegCafeInterruptCPU0Base        0x78
#define kWiiPIRegCafeInterruptCauseBase       0x78
#define kWiiPIRegCafeInterruptMaskBase        0x7C

#endif

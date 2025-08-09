//
//  GX2Regs.hpp
//  Wii U GX2 GPU MMIO registers
//
// See https://wiiubrew.org/wiki/Hardware/GX2.
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef GX2Regs_hpp
#define GX2Regs_hpp

//
// Base address for GX2.
//
#define kWiiGX2BaseAddress          0x0C200000
#define kWiiGX2BaseLength           0x80000

#define kWiiGX2MaxCursorWidth       32
#define kWiiGX2MaxCursorHeight      32
#define kWiiGX2CursorMaxSize        (kWiiGX2MaxCursorWidth * kWiiGX2MaxCursorHeight * 4)
#define kWiiGX2CursorMemSize        (64 * kWiiGX2MaxCursorHeight * 4)

//
// Registers.
//


#define kWiiGX2RegD1GrphPriSurfaceAddress       0x6110

#define kWiiGX2RegD1CursorControl                   0x6400
#define kWiiGX2RegD1CursorControlEnable             BIT0
#define kWiiGX2RegD1CursorControlMode2Bit           0
#define kWiiGX2RegD1CursorControlMode32BitAND       BIT8
#define kWiiGX2RegD1CursorControlMode32BitPreAlpha  BIT9
#define kWiiGX2RegD1CursorControlMode32BitUnAlpha   (BIT8 | BIT9)
#define kWiiGX2RegD1CursorControlModeMask           (BIT8 | BIT9)
#define kWiiGX2RegD1CursorSurfaceAddress  0x6408
#define kWiiGX2RegD1CursorSize            0x6410
#define kWiiGX2RegD1CursorSizeHeightMask  BITRange(0, 5)
#define kWiiGX2RegD1CursorSizeWidthShift  16
#define kWiiGX2RegD1CursorSizeWidthMask   BITRange(16, 21)
#define kWiiGX2RegD1CursorPosition        0x6414
#define kWiiGX2RegD1CursorPositionYMask   BITRange(0, 12)
#define kWiiGX2RegD1CursorPositionXShift  16
#define kWiiGX2RegD1CursorPositionXMask   BITRange(16, 28)
#define kWiiGX2RegD1CursorHotSpot         0x6418
#define kWiiGX2RegD1CursorHotSpotYMask    BITRange(0, 5)
#define kWiiGX2RegD1CursorHotSpotXShift   16
#define kWiiGX2RegD1CursorHotSpotXMask    BITRange(16, 21)
#define kWiiGX2RegD1CursorColor1          0x641C
#define kWiiGX2RegD1CursorColor2          0x6420

#endif

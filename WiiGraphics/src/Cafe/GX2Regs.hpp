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

#include "WiiCommon.hpp"

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

//
// Primary Display Graphics Control registers.
//
// Graphics enable.
#define kWiiGX2RegD1GrphEnable                      0x6100
#define kWiiGX2RegD1GrphEnableBit                   BIT0
// Graphics control.
#define kWiiGX2RegD1GrphControl                     0x6104
#define kWiiGX2RegD1GrphControlDepth8bpp            (0 << 0)
#define kWiiGX2RegD1GrphControlDepth16bpp           (1 << 0)
#define kWiiGX2RegD1GrphControlDepth32bpp           (2 << 0)
#define kWiiGX2RegD1GrphControlDepthMask            BITRange(0, 1)
#define kWiiGX2RegD1GrphControlFormat8bppIndexed    (0 << 8)
#define kWiiGX2RegD1GrphControlFormat16bppARGB555   (0 << 8)
#define kWiiGX2RegD1GrphControlFormat32bppARGB8888  (0 << 8)
#define kWiiGX2RegD1GrphControlFormatMask           BITRange(8, 10)
// LUT selection.
#define kWiiGX2RegD1GrphLutSelect                   0x6108
#define kWiiGX2RegD1GrphLutSelectB                  BIT0
// Swap control.
#define kWiiGX2RegD1GrphSwapControl                 0x610C
#define kWiiGX2RegD1GrphSwapControlEndianSwapNone   (0 << 0)
#define kWiiGX2RegD1GrphSwapControlEndianSwap16Bit  (1 << 0)
#define kWiiGX2RegD1GrphSwapControlEndianSwap32Bit  (2 << 0)
#define kWiiGX2RegD1GrphSwapControlEndianSwap64Bit  (3 << 0)
#define kWiiGX2RegD1GrphSwapControlEndianSwapMask   BITRange(0, 1)
// Primary surface address.
#define kWiiGX2RegD1GrphPriSurfaceAddress           0x6110
// Secondary surface address.
#define kWiiGX2RegD1GrphSecSurfaceAddress           0x6118
// Pitch.
#define kWiiGX2RegD1GrphPitch                       0x6120
// Surface offset X.
#define kWiiGX2RegD1GrphSurfaceOffsetX              0x6124
// Surface offset Y.
#define kWiiGX2RegD1GrphSurfaceOffsetY              0x6128
// X start coordinate.
#define kWiiGX2RegD1GrphStartX                      0x612C
// Y start coordinate.
#define kWiiGX2RegD1GrphStartY                      0x6130
// X end coordinate.
#define kWiiGX2RegD1GrphEndX                        0x6134
// Y end coordinate.
#define kWiiGX2RegD1GrphEndY                        0x6138

//
// Primary display hardware cursor registers.
//
// Cursor control.
#define kWiiGX2RegD1CursorControl                   0x6400
#define kWiiGX2RegD1CursorControlEnable             BIT0
#define kWiiGX2RegD1CursorControlMode2Bit           0
#define kWiiGX2RegD1CursorControlMode32BitAND       BIT8
#define kWiiGX2RegD1CursorControlMode32BitPreAlpha  BIT9
#define kWiiGX2RegD1CursorControlMode32BitUnAlpha   (BIT8 | BIT9)
#define kWiiGX2RegD1CursorControlModeMask           (BIT8 | BIT9)
// Cursor surface address.
#define kWiiGX2RegD1CursorSurfaceAddress            0x6408
// Cursor size.
#define kWiiGX2RegD1CursorSize                      0x6410
#define kWiiGX2RegD1CursorSizeHeightMask            BITRange(0, 5)
#define kWiiGX2RegD1CursorSizeWidthShift            16
#define kWiiGX2RegD1CursorSizeWidthMask             BITRange(16, 21)
// Cursor position.
#define kWiiGX2RegD1CursorPosition                  0x6414
#define kWiiGX2RegD1CursorPositionYMask             BITRange(0, 12)
#define kWiiGX2RegD1CursorPositionXShift            16
#define kWiiGX2RegD1CursorPositionXMask             BITRange(16, 28)
// Cursor hot spot.
#define kWiiGX2RegD1CursorHotSpot                   0x6418
#define kWiiGX2RegD1CursorHotSpotYMask              BITRange(0, 5)
#define kWiiGX2RegD1CursorHotSpotXShift             16
#define kWiiGX2RegD1CursorHotSpotXMask              BITRange(16, 21)
// Cursor color 1.
#define kWiiGX2RegD1CursorColor1                    0x641C
// Cursor color 2.
#define kWiiGX2RegD1CursorColor2                    0x6420

//
// Display Lookup Table Control registers.
//
// LUT read/write selection.
#define kWiiGX2RegDcLutRwSelect           0x6480
#define kWiiGX2RegDcLutRwSelectUpper      BIT0
// LUT read/write mode.
#define kWiiGX2RegDcLutRwMode             0x6484
#define kWiiGX2RegDcLutRwModePwl          BIT0
// LUT read/write index.
#define kWiiGX2RegDcLutRwIndex            0x6488
#define kWiiGX2RegDcLutRwIndexMask        BITRange(0, 7)
// LUT 30-bit color.
#define kWiiGX2RegDcLutColor              0x6494
#define kWiiGX2RegDcLutColorBlueMask      BITRange(0, 9)
#define kWiiGX2RegDcLutColorGreenShift    10
#define kWiiGX2RegDcLutColorGreenMask     BITRange(10, 19)
#define kWiiGX2RegDcLutColorRedShift      20
#define kWiiGX2RegDcLutColorRedMask       BITRange(20, 29)
// LUT read pipe selection.
#define kWiiGX2RegDcLutReadPipeSelect     0x6498
#define kWiiGX2RegDcLutReadPipeSelect1    BIT0
// LUT write enable mask.
#define kWiiGX2RegDcLutWriteEnMask        0x649C
#define kWiiGX2RegDcLutWriteEnMaskAll     0x3F
// LUT autofill.
#define kWiiGX2RegDcLutAutofill           0x64A0
#define kWiiGX2RegDcLutAutofillStart      BIT0
#define kWiiGX2RegDcLutAutofillDone       BIT1

//
// Display Lookup Table A registers.
//
#define kWiiGX2RegDcLutAControl           0x64C0
#define kWiiGX2RegDcLutABlackOffsetBlue   0x64C4
#define kWiiGX2RegDcLutABlackOffsetGreen  0x64C8
#define kWiiGX2RegDcLutABlackOffsetRed    0x64CC
#define kWiiGX2RegDcLutAWhiteOffsetBlue   0x64D0
#define kWiiGX2RegDcLutAWhiteOffsetGreen  0x64D4
#define kWiiGX2RegDcLutAWhiteOffsetRed    0x64D8

#endif

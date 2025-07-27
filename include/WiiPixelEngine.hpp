//
//  WiiPixelEngine.hpp
//  Wii pixel engine MMIO registers
//
// See https://www.gc-forever.com/yagcd/chap5.html#sec5.2.
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiPixelEngine_hpp
#define WiiPixelEngine_hpp

#include "WiiCommon.hpp"

//
// Base address for pixel engine.
//
#define kWiiPixelEngineBaseAddress      0x0C001000
#define kWiiPixelEngineBaseLength       0x100

//
// Base address for EFB.
//
#define kWiiEFBBaseAddress              0x8000000
#define kWiiEFBBaseLength               0x400000
#define kWiiEFBStride                   (1024 * 4)

//
// Pixel engine registers.
// All registers are 16-bit.
//
// Z configuration.
#define kWiiPixelEngineRegZConfig                       0x00
#define kWiiPixelEngineRegZConfigZCompEnable            BIT0
#define kWiiPixelEngineRegZConfigFuncShift              1
#define kWiiPixelEngineRegZConfigFuncMask               0xE
#define kWiiPixelEngineRegZConfigZUpdateEnable          BIT4
// Alpha configuration.
#define kWiiPixelEngineRegAlphaConfig                   0x02
#define kWiiPixelEngineRegAlphaConfigBoolBlendEnable    BIT0
#define kWiiPixelEngineRegAlphaConfigArithBlendEnable   BIT1
#define kWiiPixelEngineRegAlphaConfigDitherEnable       BIT2
#define kWiiPixelEngineRegAlphaConfigColorUpdateEnable  BIT3
#define kWiiPixelEngineRegAlphaConfigAlphaUpdateEnable  BIT4
#define kWiiPixelEngineRegAlphaConfigDestShift          5
#define kWiiPixelEngineRegAlphaConfigDestMask           0x00E0
#define kWiiPixelEngineRegAlphaConfigSrcShift           8
#define kWiiPixelEngineRegAlphaConfigSrcMask            0x0700
#define kWiiPixelEngineRegAlphaConfigBlendOpShift       12
#define kWiiPixelEngineRegAlphaConfigBlendOpMask        0xF000
// Destination alpha.
#define kWiiPixelEngineRegDestAlpha                     0x04
#define kWiiPixelEngineRegDestAlphaModeMask             0x000F
#define kWiiPixelEngineRegDestAlphaEnable               BIT8
// Alpha mode.
#define kWiiPixelEngineRegAlphaMode                     0x06
#define kWiiPixelEngineRegAlphaModeThresholdMask        0x000F
#define kWiiPixelEngineRegAlphaModeModeShift            8
#define kWiiPixelEngineRegAlphaModeModeMask             0x00F0
// Alpha read.
#define kWiiPixelEngineRegAlphaRead                     0x08
#define kWiiPixelEngineRegAlphaReadModeEnable           0x0004
// Interrupt status.
#define kWiiPixelEngineRegInterruptStatus               0x0A
#define kWiiPixelEngineRegInterruptStatusTokenEnable    BIT0
#define kWiiPixelEngineRegInterruptStatusFinishEnable   BIT1
#define kWiiPixelEngineRegInterruptStatusToken          BIT2
#define kWiiPixelEngineRegInterruptStatusFinish         BIT3
// Token.
#define kWiiPixelEngineRegToken                         0x0E

#endif

//
//  WiiHollywood.hpp
//  Wii Hollywood MMIO registers
//
// See https://wiibrew.org/wiki/Hollywood/IRQs.
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiHollywood_hpp
#define WiiHollywood_hpp

#include "WiiCommon.hpp"

//
// Base address for Hollywood
//
#define kWiiHollywoodBaseAddress        0x0D800000
#define kWiiHollywoodBaseLength         0x400

//
// Hollywood interrupts.
//
#define kWiiHollywoodICOffset               0x30
// 32 supported interrupts on Hollywood.
#define kWiiHollywoodICVectorCount          32
// Interrupt cause for Broadway.
#define kWiiHollywoodICBroadwayIRQCause     0x30
// Interrupt mask for Broadway.
#define kWiiHollywoodICBroadwayIRQMask      0x34
// Interrupt cause for Starlet.
#define kWiiHollywoodICStarletIRQCause      0x38
// Interrupt mask for Starlet.
#define kWiiHollywoodICStarletIRQMask       0x3C

// AHB protection.
#define kWiiHollywoodAHBProtect               0x64
#define kWiiHollywoodAHBProtectBroadwayNAND   BIT1
#define kWiiHollywoodAHBProtectBroadwayAES    BIT2
#define kWiiHollywoodAHBProtectBroadwaySHA1   BIT3
#define kWiiHollywoodAHBProtectBroadwayEHCI   BIT4
#define kWiiHollywoodAHBProtectBroadwayOHCI0  BIT5
#define kWiiHollywoodAHBProtectBroadwayOHCI1  BIT6
#define kWiiHollywoodAHBProtectBroadwaySDHC   BIT7
#define kWiiHollywoodAHBProtectBroadwaySDIO   BIT8
#define kWiiHollywoodAHBProtectStarletNAND    BIT17
#define kWiiHollywoodAHBProtectStarletAES     BIT18
#define kWiiHollywoodAHBProtectStarletSHA1    BIT19
#define kWiiHollywoodAHBProtectStarletEHCI    BIT20
#define kWiiHollywoodAHBProtectStarletOHCI0   BIT21
#define kWiiHollywoodAHBProtectStarletOHCI1   BIT22
#define kWiiHollywoodAHBProtectStarletSDHC    BIT23
#define kWiiHollywoodAHBProtectStarletSDIO    BIT24
#define kWiiHollywoodAHBProtectBroadwayEnable BIT31


// Version.
#define kWiiHollywoodVersion                0x214

#endif

//
//  WiiLatte.hpp
//  Wii U Latte MMIO registers
//
// See https://wiiubrew.org/wiki/Hardware/Latte_IRQs.
// See https://wiiubrew.org/wiki/Hardware/Latte_registers.
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiLatte_hpp
#define WiiLatte_hpp

//
// Latte Wood IPC registers.
//
#define kWiiLatteIPCPPCMSG        0x00
#define kWiiLatteIPCPPCCTRL       0x04
#define kWiiLatteIPCARMMSG        0x08
#define kWiiLatteIPCARMCTRL       0x0C

//
// Base address for Latte interrupts.
//
#define kWiiLatteIntBaseAddress         0x0D800440
#define kWiiLatteIntBaseLength          0x48

// 64 supported interrupts on Latte.
#define kWiiLatteIntVectorCount         64

// PowerPC interrupt registers (cores 0 - 2).
#define kWiiLatteIntPPCRegistersLength          (4 * 4)
// Interrupt cause 0-31.
#define kWiiLatteIntRegPPCInterruptCause0       0x00
// Interrupt cause 32-63.
#define kWiiLatteIntRegPPCInterruptCause1       0x04
// Interrupt mask 0-31.
#define kWiiLatteIntRegPPCInterruptMask0        0x08
// Interrupt mask 32-63.
#define kWiiLatteIntRegPPCInterruptMask1        0x0C

// ARM interrupt registers.
// Interrupt cause 0-31.
#define kWiiLatteIntRegARMInterruptCause0       0x30
// Interrupt cause 32-63.
#define kWiiLatteIntRegARMInterruptCause1       0x34
// Interrupt mask 0-31.
#define kWiiLatteIntRegARMInterruptMask0        0x38
// Interrupt mask 32-63.
#define kWiiLatteIntRegARMInterruptMask1        0x3C
// Interrupt mask 0-31.
#define kWiiLatteIntRegARMFIQMask0              0x40
// Interrupt mask 32-63.
#define kWiiLatteIntRegARMFIQMask1              0x44

#endif

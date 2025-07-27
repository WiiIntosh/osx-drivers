//
//  WiiGXFifo.hpp
//  Wii GX FIFO definitions
//
// See https://www.gc-forever.com/yagcd/chap5.html#sec5.2.
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiGXFifo_hpp
#define WiiGXFifo_hpp

#include "WiiCommon.hpp"

//
// Base address for GX FIFO interface.
//
#define kWiiGXFifoBaseAddress             0x0C008000
#define kWiiGXFifoBaseLength              0x4
#define kWiiGXFifoBaseVirtualAddress      (0xF0000000 | kWiiGXFifoBaseAddress)

//
// GX command opcodes.
//
#define kWiiGXFifoOpNone                  0x00
#define kWiiGXFifoOpLoadCPRegister        0x08
#define kWiiGXFifoOpLoadXFRegister        0x10
#define kWiiGXFifoOpLoadIndexA            0x20
#define kWiiGXFifoOpLoadIndexB            0x28
#define kWiiGXFifoOpLoadIndexC            0x30
#define kWiiGXFifoOpLoadIndexD            0x38
#define kWiiGXFifoOpCallDisplayList       0x40
#define kWiiGXFifoOpInvalidateVertCache   0x48
#define kWiiGXFifoOpLoadBPRegister        0x61
// Draw commands must have Vertex Attribute Table Index in lowest 3 bits.
#define kWiiGXFifoOpDrawQuads             0x80
#define kWiiGXFifoOpDrawTriangles         0x90
#define kWiiGXFifoOpDrawTriangleStrip     0x98
#define kWiiGXFifoOpDrawTriangleFan       0xA0
#define kWiiGXFifoOpDrawLines             0xA8
#define kWiiGXFifoOpDrawLineStrip         0xB0
#define kWiiGXFifoOpDrawPoints            0xB8

//
// Bypass Raster State Registers.
//
#define kWiiBPRegGenMode                  0x00
#define kWiiBPRegDisplayCopyFilter1       0x01
#define kWiiBPRegDisplayCopyFilter2       0x02
#define kWiiBPRegDisplayCopyFilter3       0x03
#define kWiiBPRegDisplayCopyFilter4       0x04

#define kWiiBPRegIndirectMatrixA0         0x06
#define kWiiBPRegIndirectMatrixB0         0x07
#define kWiiBPRegIndirectMatrixC0         0x08
#define kWiiBPRegIndirectMatrixA1         0x09
#define kWiiBPRegIndirectMatrixB1         0x0A
#define kWiiBPRegIndirectMatrixC1         0x0B
#define kWiiBPRegIndirectMatrixA2         0x0C
#define kWiiBPRegIndirectMatrixB2         0x0D
#define kWiiBPRegIndirectMatrixC2         0x0E
#define kWiiBPRegIndirectMask             0x0F
#define kWiiBPRegScissorTopLeft           0x20
#define kWiiBPRegScissorBottomRight       0x21
#define kWiiBPRegLinePointWidth           0x22
#define kWiiBPRegSUCounter                0x23
#define kWiiBPRegRASCounter               0x24

#define kWiiBPRegRASTevRef                0x28

#define kWiiBPRegTextureSSize0            0x30
#define kWiiBPRegTextureTSize0            0x31
#define kWiiBPRegTextureSSize1            0x32
#define kWiiBPRegTextureTSize1            0x33
#define kWiiBPRegTextureSSize2            0x34
#define kWiiBPRegTextureTSize2            0x35
#define kWiiBPRegTextureSSize3            0x36
#define kWiiBPRegTextureTSize3            0x37
#define kWiiBPRegTextureSSize4            0x38
#define kWiiBPRegTextureTSize4            0x39
#define kWiiBPRegTextureSSize5            0x3A
#define kWiiBPRegTextureTSize5            0x3B
#define kWiiBPRegTextureSSize6            0x3C
#define kWiiBPRegTextureTSize6            0x3D
#define kWiiBPRegTextureSSize7            0x3E
#define kWiiBPRegTextureTSize7            0x3F

#define kWiiBPRegPixelZMode               0x40
#define kWiiBPRegPixelColorMode0          0x41
#define kWiiBPRegPixelColorMode1          0x42
#define kWiiBPRegPixelControl             0x43
#define kWiiBPRegFieldMask                0x44
#define kWiiBPRegPixelDone                0x45
#define kWiiBPRegPixelDoneEndOfList       BIT1
#define kWiiBPRegClock0                   0x46
#define kWiiBPRegPixelToken               0x47
#define kWiiBPRegPixelTokenInterrupt      0x48
#define kWiiBPRegEFBSourceTopLeft         0x49

// EFB copy bottom right.
#define kWiiBPRegEFBSourceBottomRight         0x4A
#define kWiiBPRegEFBSourceBottomRightRMask    0x3FF
#define kWiiBPRegEFBSourceBottomRightBShift   10
#define kWiiBPRegEFBSourceBottomRightBMask    0xFFFC00

#define kWiiBPRegXFBTargetPhysAddr            0x4B
#define kWiiBPRegXFBTargetPhysAddrShift       5
#define kWiiBPRegXFBTargetPhysAddrMask        0xFFFFFF

#define kWiiBPRegEFBStride                    0x4D
#define kWiiBPRegEFBStrideShift               5

#define kWiiBPRegCopyYScale               0x4E
#define kWiiBPRegPixelCopyClearAR         0x4F

#define kWiiBPRegPixelCopyClearGB         0x50
#define kWiiBPRegPixelCopyClearZ          0x51

// Copy EFB to XFB.
#define kWiiBPRegPixelExecuteCopyEFB              0x52
#define kWiiBPRegPixelExecuteCopyEFBClampTop      BIT0
#define kWiiBPRegPixelExecuteCopyEFBClampBottom   BIT1
#define kWiiBPRegPixelExecuteCopyEFBClear         BIT11
#define kWiiBPRegPixelExecuteCopyEFBCopyToXFB     BIT14

#define kWiiBPRegCopyFilter0              0x53
#define kWiiBPRegCopyFilter1              0x54
#define kWiiBPRegBoundingBox0             0x55
#define kWiiBPRegBoundingBox1             0x56
#define kWiiBPRegScissorOffset            0x59

#define kWiiBPRegPreloadAddress           0x60
#define kWiiBPRegPreloadTextureMemEven    0x61
#define kWiiBPRegPreloadTextureMemOdd     0x62
#define kWiiBPRegPreloadMode              0x63
#define kWiiBPRegLoadTLUT0                0x64
#define kWiiBPRegLoadTLUT1                0x65
#define kWiiBPRegInvalidateTexture        0x66
#define kWiiBPRegPerf1                    0x67
#define kWiiBPRegFieldMode                0x68
#define kWiiBPRegClock1                   0x69

#define kWiiBPRegSetTextureMode           0x80
#define kWiiBPRegSetTextureLOD            0x84
#define kWiiBPRegSetTextureImage          0x88
#define kWiiBPRegSetTextureImageLODEven   0x8C
#define kWiiBPRegSetTextureImageLODOdd    0x90
#define kWiiBPRegSetTextureImageAddr      0x94

// 15 total register pairs.
#define kWiiBPRegTevColorEnv              0xC0
#define kWiiBPRegTevColorAlpha            0xC1

// 7 total registers.
#define kWiiBPRegKSelect                  0xF6



//
// Command processor registers.
//

#define kWiiCPRegVerticesArrayAddr        0xA0
#define kWiiCPRegNormalsArrayAddr         0xA1
#define kWiiCPRegColorArray0Addr          0xA2
#define kWiiCPRegColorArray1Addr          0xA3
#define kWiiCPRegTexture0CoordArrayAddr   0xA4
#define kWiiCPRegTexture1CoordArrayAddr   0xA5
#define kWiiCPRegTexture2CoordArrayAddr   0xA6
#define kWiiCPRegTexture3CoordArrayAddr   0xA7
#define kWiiCPRegTexture4CoordArrayAddr   0xA8
#define kWiiCPRegTexture5CoordArrayAddr   0xA9
#define kWiiCPRegTexture6CoordArrayAddr   0xAA
#define kWiiCPRegTexture7CoordArrayAddr   0xAB
#define kWiiCPRegGenArray0Addr            0xAC
#define kWiiCPRegGenArray1Addr            0xAD
#define kWiiCPRegGenArray2Addr            0xAE
#define kWiiCPRegGenArray3Addr            0xAF

#define kWiiCPRegVerticesArraySize        0xB0
#define kWiiCPRegNormalsArraySize         0xB1
#define kWiiCPRegColorArray0Size          0xB2
#define kWiiCPRegColorArray1Size          0xB3
#define kWiiCPRegTexture0CoordArraySize   0xB4
#define kWiiCPRegTexture1CoordArraySize   0xB5
#define kWiiCPRegTexture2CoordArraySize   0xB6
#define kWiiCPRegTexture3CoordArraySize   0xB7
#define kWiiCPRegTexture4CoordArraySize   0xB8
#define kWiiCPRegTexture5CoordArraySize   0xB9
#define kWiiCPRegTexture6CoordArraySize   0xBA
#define kWiiCPRegTexture7CoordArraySize   0xBB
#define kWiiCPRegGenArray0Size            0xBC
#define kWiiCPRegGenArray1Size            0xBD
#define kWiiCPRegGenArray2Size            0xBE
#define kWiiCPRegGenArray3Size            0xBF


#endif

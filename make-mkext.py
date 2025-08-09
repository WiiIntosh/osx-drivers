#!/bin/python3

import lzss
import struct
import zlib

#
# See https://github.com/acidanthera/OpenCorePkg/blob/master/Include/Apple/IndustryStandard/AppleMkext.h
#

class KextInfo:
  def __init__(self, plist_path, bin_path):
    self.plist_path             = plist_path
    self.bin_path               = bin_path
    self.plist_lzss             = None
    self.plist_full_length      = 0
    self.bin_lzss               = None
    self.bin_full_length        = 0

def read_kexts(kexts):
  for kext in kexts:
    #
    # Read and compress the kext plist and binary.
    #
    print("Reading plist", kext.plist_path)
    with open(kext.plist_path, 'rb') as f:
      plist = f.read()
      kext.plist_full_length        = len(plist)
      kext.plist_lzss               = lzss.compress(plist)
    print("Reading binary", kext.bin_path)
    with open(kext.bin_path, 'rb') as f:
      bin = f.read()
      kext.bin_full_length          = len(bin)
      kext.bin_lzss                 = lzss.compress(bin)
    

def create_mkext(kexts):
  #
  # Build the MKEXT header. Length and checksum will be calculated later.
  #
  mkextMagic        = 0x4D4B5854  # MKXT
  mkextSignature    = 0x4D4F5358  # MOSX
  mkextLength       = 0
  mkextChecksum     = 0
  mkextVersion      = 0x01008000  # V1
  mkextNumKexts     = len(kexts)
  mkextCpuType      = -1
  mkextCpuSubtype   = -1

  #
  # Calculate the length of the MKEXT.
  #
  mkextLength = struct.calcsize('>IIIIIIii')
  mkextOffset = mkextLength
  mkextSlotOffset = mkextLength
  for kext in kexts:
    mkextLength += struct.calcsize('>IIIIIIII')
    mkextOffset += struct.calcsize('>IIIIIIII')
    mkextLength += (len(kext.plist_lzss) + 0x1F) & ~(0x1F)
    mkextLength += (len(kext.bin_lzss) + 0x1F) & ~(0x1F)

  #
  # Create the MKEXT buffer and header.
  #
  mkext = bytearray(mkextLength)
  struct.pack_into('>IIIIIIii', mkext, 0, mkextMagic, mkextSignature, mkextLength, mkextChecksum, mkextVersion, mkextNumKexts, mkextCpuType, mkextCpuSubtype)
  
  for kext in kexts:
    #
    # Load the kext data into the buffer.
    #
    plistOffset = mkextOffset
    mkext[plistOffset:plistOffset + len(kext.plist_lzss)] = kext.plist_lzss
    mkextOffset += (len(kext.plist_lzss) + 0x1F) & ~(0x1F)
    binOffset = mkextOffset
    mkext[binOffset:binOffset + len(kext.bin_lzss)] = kext.bin_lzss
    mkextOffset += (len(kext.bin_lzss) + 0x1F) & ~(0x1F)
  
    #
    # Populate kext slot
    #
    struct.pack_into('>IIIIIIII', mkext, mkextSlotOffset, plistOffset, len(kext.plist_lzss), kext.plist_full_length, 0, binOffset, len(kext.bin_lzss), kext.bin_full_length, 0)
    mkextSlotOffset += struct.calcsize('>IIIIIIII')

  #
  # Checksum mkext.
  #
  mkextChecksum = zlib.adler32(mkext[16:mkextLength], 1)
  struct.pack_into('>I', mkext, 12, mkextChecksum)

  return mkext

kexts = [ KextInfo('./WiiPlatform/build_kext/WiiPlatform.kext/Contents/Info.plist', './WiiPlatform/build_kext/WiiPlatform.kext/Contents/MacOS/WiiPlatform'),
          KextInfo('./WiiStorage/build_kext/WiiStorage.kext/Contents/Info.plist', './WiiStorage/build_kext/WiiStorage.kext/Contents/MacOS/WiiStorage'),
          KextInfo('./WiiGraphics/build_kext/WiiGraphics.kext/Contents/Info.plist', './WiiGraphics/build_kext/WiiGraphics.kext/Contents/MacOS/WiiGraphics'),
          KextInfo('./WiiUSB/build_kext/WiiUSB.kext/Contents/Info.plist', './WiiUSB/build_kext/WiiUSB.kext/Contents/MacOS/WiiUSB'),
          KextInfo('./WiiEXI/build_kext/WiiEXI.kext/Contents/Info.plist', './WiiEXI/build_kext/WiiEXI.kext/Contents/MacOS/WiiEXI')
        ]

#
# Build the MKEXT
#
read_kexts(kexts)
mkext = create_mkext(kexts)

#
# Save to file.
#
with open('Wii.mkext', 'wb') as f:
  f.write(mkext)

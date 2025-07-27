//
//  WiiSDBlockStorageDevice.hpp
//  Wii SD direct block storage device
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiSDBlockStorageDevice_hpp
#define WiiSDBlockStorageDevice_hpp

#include <IOKit/storage/IOBlockStorageDevice.h>

#include "WiiCommon.hpp"

class WiiSDHC;

//
// Represents the Wii SD direct block storage device
//
class WiiSDBlockStorageDevice : public IOBlockStorageDevice {
  OSDeclareDefaultStructors(WiiSDBlockStorageDevice);
  WiiDeclareLogFunctions("sdblk");
  typedef IOBlockStorageDevice super;

private:
  WiiSDHC   *_wiiSDHC;

public:
  //
  // Overrides.
  //
  bool init(OSDictionary *dictionary = 0);
  bool start(IOService *provider);
  IOReturn doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt32 block, UInt32 nblks, IOStorageCompletion completion);
  IOReturn doSyncReadWrite(IOMemoryDescriptor *buffer, UInt32 block, UInt32 nblks);
  IOReturn doEjectMedia(void);
  IOReturn doFormatMedia(UInt64 byteCapacity);
  UInt32 doGetFormatCapacities(UInt64 *capacities, UInt32 capacitiesMaxCount) const;
  IOReturn doLockUnlockMedia(bool doLock);
  IOReturn doSynchronizeCache(void);
  char *getVendorString(void);
  char *getProductString(void);
  char *getRevisionString(void);
  char *getAdditionalDeviceInfoString(void);
  IOReturn reportBlockSize(UInt64 *blockSize);
  IOReturn reportEjectability(bool *isEjectable);
  IOReturn reportLockability(bool *isLockable);
  IOReturn reportMaxReadTransfer(UInt64 blockSize, UInt64 *max);
  IOReturn reportMaxWriteTransfer(UInt64 blockSize, UInt64 *max);
  IOReturn reportRemovability(bool *isRemovable);
  IOReturn reportMaxValidBlock(UInt64 *maxBlock);
  IOReturn reportMediaState(bool *mediaPresent, bool *changedState = 0);
  IOReturn reportPollRequirements(bool *pollRequired, bool *pollIsExpensive);
  IOReturn reportWriteProtection(bool *isWriteProtected);
};

#endif

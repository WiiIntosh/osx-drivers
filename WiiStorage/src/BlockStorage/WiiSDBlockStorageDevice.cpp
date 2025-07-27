//
//  WiiSDBlockStorageDevice.cpp
//  Wii SD direct block storage device
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <IOKit/IODeviceTreeSupport.h>

#include "WiiSDBlockStorageDevice.hpp"
#include "../SDHC/WiiSDHC.hpp"

OSDefineMetaClassAndStructors(WiiSDBlockStorageDevice, super);

//
// Overrides IOBlockStorageDevice::init()
//
bool WiiSDBlockStorageDevice::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _wiiSDHC = NULL;
  return super::init(dictionary);
}

//
// Overrides IOBlockStorageDevice::start()
//
bool WiiSDBlockStorageDevice::start(IOService *provider) {
  IOReturn      status;

  _wiiSDHC = OSDynamicCast(WiiSDHC, provider);
  if (_wiiSDHC == NULL) {
    WIISYSLOG("Provider is not WiiSDHC");
    return false;
  }

  if (!super::start(provider)) {
    WIISYSLOG("super::start() returned false");
    return false;
  }

  //
  // Required for installer to accept as an installable storage device.
  // TODO: Probably should have the bootloader add this, going to need it for proper XNU rooting.
  //
  //attachToParent(provider, gIODTPlane);
 /* OSNumber *unitNum = OSNumber::withNumber((unsigned long long)0, 32);
  setProperty("IOUnit", unitNum);
  unitNum->release();
  //setLocation("0", gIODTPlane);

  OSDictionary *dict = OSDictionary::withCapacity(1);
	OSString *location = OSString::withCString(kIOPropertyInternalKey);
	if( dict && location ) {
		dict->setObject( kIOPropertyPhysicalInterconnectLocationKey, location );
		setProperty( kIOPropertyProtocolCharacteristicsKey, dict );
	}
	if( location )
		location->release();
	if( dict )
		dict->release();*/

  registerService();

  WIIDBGLOG("Initialized Wii SD block storage");
  return true;
}

//
// Overrides IOBlockStorageDevice::doAsyncReadWrite().
//
// Executes an asynchronous read/write operation.
//
IOReturn WiiSDBlockStorageDevice::doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt32 block, UInt32 nblks, IOStorageCompletion completion) {
  return _wiiSDHC->doAsyncReadWrite(buffer, block, nblks, completion);
}

//
// Overrides IOBlockStorageDevice::doSyncReadWrite().
//
// Executes a synchronous read/write operation. Not supported.
//
IOReturn WiiSDBlockStorageDevice::doSyncReadWrite(IOMemoryDescriptor *buffer, UInt32 block, UInt32 nblks) {
  return kIOReturnUnsupported;
}

//
// Overrides IOBlockStorageDevice::doEjectMedia().
//
// Ejects the device.
//
IOReturn WiiSDBlockStorageDevice::doEjectMedia() {
  return kIOReturnSuccess;
}

//
// Overrides IOBlockStorageDevice::doFormatMedia().
//
// Performs a low-level format. Not supported.
//
IOReturn WiiSDBlockStorageDevice::doFormatMedia(UInt64 byteCapacity) {
  return kIOReturnUnsupported;
}

//
// Overrides IOBlockStorageDevice::doGetFormatCapacities().
//
// Gets a list of capacities supported by low-level formatting. Not supported.
//
UInt32 WiiSDBlockStorageDevice::doGetFormatCapacities(UInt64 *capacities, UInt32 capacitiesMaxCount) const {
  return 0;
}

//
// Overrides IOBlockStorageDevice::doLockUnlockMedia().
//
// Physically locks or unlocks the storage media. Not supported.
//
IOReturn WiiSDBlockStorageDevice::doLockUnlockMedia(bool doLock) {
  return kIOReturnUnsupported;
}

//
// Overrides IOBlockStorageDevice::doSynchronizeCache().
//
// Flushes the storage device cache.
//
IOReturn WiiSDBlockStorageDevice::doSynchronizeCache() {
  return _wiiSDHC->isCardPresent() ? kIOReturnSuccess : kIOReturnNoMedia;
}

//
// Overrides IOBlockStorageDevice::getVendorString().
//
// Returns the storage media vendor string.
//
char* WiiSDBlockStorageDevice::getVendorString() {
  return _wiiSDHC->getVendorString();
}

//
// Overrides IOBlockStorageDevice::getProductString().
//
// Returns the storage media product string.
//
char* WiiSDBlockStorageDevice::getProductString() {
  return _wiiSDHC->getProductString();
}

//
// Overrides IOBlockStorageDevice::getRevisionString().
//
// Returns the storage media revision string.
//
char* WiiSDBlockStorageDevice::getRevisionString() {
  return _wiiSDHC->getRevisionString();
}

//
// Overrides IOBlockStorageDevice::getAdditionalDeviceInfoString().
//
// Returns the storage media serial number string.
//
char* WiiSDBlockStorageDevice::getAdditionalDeviceInfoString() {
  return _wiiSDHC->getSerialNumberString();
}

//
// Overrides IOBlockStorageDevice::reportBlockSize().
//
// Returns the storage media's block size.
//
IOReturn WiiSDBlockStorageDevice::reportBlockSize(UInt64 *blockSize) {
  return _wiiSDHC->reportBlockSize(blockSize);
}

//
// Overrides IOBlockStorageDevice::reportEjectability().
//
// Reports if the media is ejectable. Always true.
//
IOReturn WiiSDBlockStorageDevice::reportEjectability(bool *isEjectable) {
  *isEjectable = false;
  return kIOReturnSuccess;
}

//
// Overrides IOBlockStorageDevice::reportLockability().
//
// Reports if the media is lockable. Always false.
//
IOReturn WiiSDBlockStorageDevice::reportLockability(bool *isLockable) {
  *isLockable = false;
  return kIOReturnSuccess;
}

//
// Overrides IOBlockStorageDevice::reportMaxReadTransfer().
//
// Reports the maximum amount of data that can be read in a single I/O operation.
//
IOReturn WiiSDBlockStorageDevice::reportMaxReadTransfer(UInt64 blockSize, UInt64 *max) {
  return _wiiSDHC->reportMaxReadTransfer(blockSize, max);
}

//
// Overrides IOBlockStorageDevice::reportMaxWriteTransfer().
//
// Reports the maximum amount of data that can be written in a single I/O operation.
//
IOReturn WiiSDBlockStorageDevice::reportMaxWriteTransfer(UInt64 blockSize, UInt64 *max) {
  return _wiiSDHC->reportMaxWriteTransfer(blockSize, max);
}

//
// Overrides IOBlockStorageDevice::reportRemovability().
//
// Reports the removability of the media. Always true.
//
IOReturn WiiSDBlockStorageDevice::reportRemovability(bool *isRemovable) {
  *isRemovable = false;
  return kIOReturnSuccess;
}

//
// Overrides IOBlockStorageDevice::reportMaxValidBlock().
//
// Reports the highest block that is addressable on the media.
//
IOReturn WiiSDBlockStorageDevice::reportMaxValidBlock(UInt64 *maxBlock) {
  return _wiiSDHC->reportMaxValidBlock(maxBlock);
}

//
// Overrides IOBlockStorageDevice::reportMediaState().
//
// Reports the current media state.
//
IOReturn WiiSDBlockStorageDevice::reportMediaState(bool *mediaPresent, bool *changedState) {
  return _wiiSDHC->reportMediaState(mediaPresent, changedState);
}

//
// Overrides IOBlockStorageDevice::reportPollRequirements().
//
// Reports if the media requires polling to determine the state. Always false.
//
IOReturn WiiSDBlockStorageDevice::reportPollRequirements(bool *pollRequired, bool *pollIsExpensive) {
  *pollRequired = false;
  *pollIsExpensive = false;
  return kIOReturnSuccess;
}

//
// Overrides IOBlockStorageDevice::reportWriteProtection().
//
// Reports if the media is write protected.
//
IOReturn WiiSDBlockStorageDevice::reportWriteProtection(bool *isWriteProtected) {
  return _wiiSDHC->reportWriteProtection(isWriteProtected);
}

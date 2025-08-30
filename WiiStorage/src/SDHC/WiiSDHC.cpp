//
//  WiiSDHC.cpp
//  Wii SD host controller interface
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOPlatformExpert.h>

#include "WiiSDHC.hpp"

OSDefineMetaClassAndStructors(WiiSDHC, super);

//
// Overrides IOService::init()
//
bool WiiSDHC::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _memoryMap              = NULL;
  _baseAddr               = NULL;

  _workLoop               = NULL;
  _commandGate            = NULL;
  _currentCommand         = NULL;
  _sdhcState              = kSDHCStateFree;
  _invalidateCacheFunc    = NULL;

  queue_init(&_commandQueue);

  return super::init(dictionary);
}

//
// Overrides IOService::start()
//
bool WiiSDHC::start(IOService *provider) {
  const OSSymbol  *functionSymbol;
  WiiSDCommand    *sdCommand;
  IOReturn        status;

  if (!super::start(provider)) {
    WIISYSLOG("super::start() returned false");
    return false;
  }

  //
  // Map controller memory.
  //
  _memoryMap = provider->mapDeviceMemoryWithIndex(0);
  if (_memoryMap == NULL) {
    WIISYSLOG("Failed to map SDHC memory");
    return false;
  }
  _baseAddr = (volatile void *)_memoryMap->getVirtualAddress();
  WIIDBGLOG("Mapped registers to %p (physical 0x%X), length: 0x%X", _baseAddr,
    _memoryMap->getPhysicalAddress(), _memoryMap->getLength());

  //
  // Get cache invalidation function.
  //
  functionSymbol = OSSymbol::withCString(kWiiFuncPlatformGetInvalidateCache);
  if (functionSymbol == NULL) {
    return false;
  }
  status = getPlatform()->callPlatformFunction(functionSymbol, false, &_invalidateCacheFunc, 0, 0, 0);
  functionSymbol->release();
  if (status != kIOReturnSuccess) {
    return false;
  }

  if (_invalidateCacheFunc == NULL) {
    WIISYSLOG("Failed to get cache invalidation function");
    return false;
  }

  //
  // Initialize work loop and command pool.
  //
  _workLoop = IOWorkLoop::workLoop();
  if (_workLoop == NULL) {
    WIISYSLOG("Failed to create work loop");
    return false;
  }

  //
  // Allocate command gate and interrupt.
  //
  _commandGate = IOCommandGate::commandGate(this);
  if (_commandGate == NULL) {
    WIISYSLOG("Failed to create command gate");
    return false;
  }
  _workLoop->addEventSource(_commandGate);
  _commandGate->enable();

  _interruptEventSource = IOInterruptEventSource::interruptEventSource(this,
    OSMemberFunctionCast(IOInterruptEventSource::Action, this, &WiiSDHC::handleInterrupt),
    provider, 0);
  if (_interruptEventSource == NULL) {
    WIISYSLOG("Failed to create interrupt");
    return false;
  }
  _workLoop->addEventSource(_interruptEventSource);
  _interruptEventSource->enable();

  //
  // DMA memory cursor.
  //
  _memoryCursor = IONaturalMemoryCursor::withSpecification(PAGE_SIZE, PAGE_SIZE);
  if (_memoryCursor == NULL) {
    WIISYSLOG("Failed to create memory cursor");
    return false;
  }

  //
  // Create double buffer for handling DMA inconsistencies.
  //
  _doubleBuffer = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous, kWiiSDHCMaxTransferBlocks * kSDBlockSize * 2, PAGE_SIZE);
  if (_doubleBuffer == NULL) {
    WIISYSLOG("Failed to create double buffer");
    return false;
  }
  _doubleBufferPtr = (UInt8*) _doubleBuffer->getBytesNoCopy();

  if (_memoryCursor->getPhysicalSegments(_doubleBuffer, 0, &_doubleBufferSegment, 1) != 1) {
    WIISYSLOG("Failed to get double buffer segment");
    return false;
  }

  setStorageProperties(this);

  //
  // Reset the controller.
  //
  status = initController();
  if (status != kIOReturnSuccess) {
    WIISYSLOG("Failed to initialize controller with status: 0x%X", status);
    return false;
  }

  status = initCard();
  if (status != kIOReturnSuccess) {
    WIISYSLOG("Failed to initialize card with status: 0x%X", status);
  }


  WIIDBGLOG("SDHC version: 0x%X", readReg32(kSDHCRegHostControllerVersion));
  WIIDBGLOG("SDHC ps: 0x%X", readReg32(kSDHCRegPresentState));
  WIIDBGLOG("SDHC capabilities: 0x%X", readReg32(kSDHCRegCapabilities));

  //
  // Required for installer to accept as an installable storage device.
  //
  OSNumber *unitNum = OSNumber::withNumber((unsigned long long)0, 32);
  setProperty("IOUnit", unitNum);

  registerService();

  WIIDBGLOG("Initialized SD host controller");
  return true;
}

void WiiSDHC::setStorageProperties(IOService *service) {
  //
  // Wii SD card I/O only supports one contiguous buffer.
  // This appears to be disregarded on some versions and the system provides multiple segment buffers anyway.
  //
  service->setProperty(kIOMaximumSegmentCountReadKey, 1, 64);
  service->setProperty(kIOMaximumSegmentCountWriteKey, 1, 64);
  service->setProperty(kIOMaximumBlockCountReadKey, 8, 64);
  service->setProperty(kIOMaximumBlockCountWriteKey, 8, 64);
}

//
// Executes an asynchronous read/write operation.
//
IOReturn WiiSDHC::doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt32 block, UInt32 nblks, IOStorageCompletion completion) {
  bool      isRead;
  IOReturn  status;

  if (nblks > 8) {
    panic("Too many blocks attempted");
    while (true);
  }

  if ((buffer->getDirection() != kIODirectionIn) && (buffer->getDirection() != kIODirectionOut)) {
    return kIOReturnUnsupported;
  }

  //
  // Submit the async command.
  //
  isRead = buffer->getDirection() == kIODirectionIn;
  if (nblks > 1) {
    status = sendCommandAsync(isRead ? kSDCommandReadMultipleBlock : kSDCommandWriteMultipleBlock, kSDHCResponseTypeR1, block, buffer, nblks, completion);
  } else {
    status = sendCommandAsync(isRead ? kSDCommandReadSingleBlock : kSDCommandWriteSingleBlock, kSDHCResponseTypeR1, block, buffer, 1, completion);
  }
  if (status != kIOReturnSuccess) {
    IOSleep(1000);
    panic ("got an error %s here: 0x%X", isRead ? "read" : "write", status);
    while (true);
  }

  return status;
}

//
// Returns the storage media vendor string.
//
char* WiiSDHC::getVendorString() {
  return (char *) _cardVendorName;
}

//
// Returns the storage media product string.
//
char* WiiSDHC::getProductString() {
  return _cardProductName;
}

//
// Returns the storage media revision string.
//
char* WiiSDHC::getRevisionString() {
  return _cardRev;
}

//
// Returns the storage media serial number string.
//
char* WiiSDHC::getSerialNumberString() {
  return _cardSN;
}

//
// Returns the storage media's block size.
//
IOReturn WiiSDHC::reportBlockSize(UInt64 *blockSize) {
  // Block size used for access is always 512 bytes.
  //
  // Some older cards may have larger block sizes, but those will be used for size
  //   calculation only as 512 bytes is the only size supported by all controllers and cards.
  *blockSize = kSDBlockSize;
  return kIOReturnSuccess;
}

//
// Overrides IOBlockStorageDevice::reportMaxReadTransfer().
//
// Reports the maximum amount of data that can be read in a single I/O operation.
//
IOReturn WiiSDHC::reportMaxReadTransfer(UInt64 blockSize, UInt64 *max) {
  *max = kWiiSDHCMaxTransferBlocks * 512;
  return kIOReturnSuccess;
}

//
// Overrides IOBlockStorageDevice::reportMaxWriteTransfer().
//
// Reports the maximum amount of data that can be written in a single I/O operation.
//
IOReturn WiiSDHC::reportMaxWriteTransfer(UInt64 blockSize, UInt64 *max) {
  *max = kWiiSDHCMaxTransferBlocks * 512;
  return kIOReturnSuccess;
}

//
// Overrides IOBlockStorageDevice::reportMaxValidBlock().
//
// Reports the highest block that is addressable on the media.
//
IOReturn WiiSDHC::reportMaxValidBlock(UInt64 *maxBlock) {
  if (!isCardPresent()) {
    return kIOReturnNoMedia;
  }

  *maxBlock = _cardBlockCount - 1;
  return kIOReturnSuccess;
}

//
// Overrides IOBlockStorageDevice::reportMediaState().
//
// Reports the current media state.
//
IOReturn WiiSDHC::reportMediaState(bool *mediaPresent, bool *changedState) {
  WIIDBGLOG("start");

  *mediaPresent = isCardPresent();
  *changedState = *mediaPresent != _isCardPresent;
  return kIOReturnSuccess;
}

//
// Overrides IOBlockStorageDevice::reportWriteProtection().
//
// Reports if the media is write protected.
//
IOReturn WiiSDHC::reportWriteProtection(bool *isWriteProtected) {
  if (!isCardPresent()) {
    return kIOReturnNoMedia;
  }

  *isWriteProtected = isCardWriteProtected();
  return kIOReturnSuccess;
}

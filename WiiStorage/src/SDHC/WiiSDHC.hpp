//
//  WiiSDHC.hpp
//  Wii SD host controller interface
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiSDHC_hpp
#define WiiSDHC_hpp

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOService.h>

#include "WiiCommon.hpp"
#include "WiiSDCommand.hpp"
#include "SDHCRegs.hpp"

#define kWiiSDHCMaxTransferBlocks     8

//
// Represents the Wii SD host controller.
//
class WiiSDHC : public IOService {
  OSDeclareDefaultStructors(WiiSDHC);
  WiiDeclareLogFunctions("sdhc");
  typedef IOService super;

private:
  // MMIO.
  IOMemoryMap         *_memoryMap;
  volatile void       *_baseAddr;

  IOWorkLoop                *_workLoop;
  IOCommandGate             *_commandGate;
  IOInterruptEventSource    *_interruptEventSource;
  IONaturalMemoryCursor     *_memoryCursor;
  IOBufferMemoryDescriptor  *_doubleBuffer;
  UInt8                     *_doubleBufferPtr;
  IOPhysicalSegment         _doubleBufferSegment;
  queue_head_t              _commandQueue;
	WiiSDCommand              *_currentCommand;

  //
  // Controller state.
  //
  enum {
    kSDHCStateFree,
    kSDHCStateBusy
  } _sdhcState;

  //
  // Card states.
  //
  bool            _isCardPresent;
  // Card selected.
  UInt16          _cardAddress;
  bool            _isCardSelected;
  bool            _isCardHighCapacity;
  SDCardType      _cardType;
  UInt16          _cardBlockLength;
  // CID.
  union {
    SDCIDRegister   sd;
  } _cardCID;
  // CSD.
  union {
    SDCSDRegisterV1 sd1;
    SDCSDRegisterV2 sd2;
  } _cardCSD;

  char        _cardProductName[kSDProductNameLength];
  const char  *_cardVendorName;
  char        _cardSN[kSDSerialNumLength];
  char        _cardRev[kSDRevisionLength];
  UInt32      _cardBlockCount;

  inline bool isSDCard() {
    return _cardType != kSDCardTypeMMC;
  }

  inline UInt8 readReg8(UInt32 offset) {
    // Can't handle non 32-bit reads.
    return OSReadBigInt32(_baseAddr, offset & -4) >> (8 * (offset & 3));
  }
  inline UInt16 readReg16(UInt32 offset) {
    // Can't handle non 32-bit reads.
    return OSReadBigInt32(_baseAddr, offset & -4) >> (8 * (offset & 2));
  }
  inline UInt32 readReg32(UInt32 offset) {
    return OSReadBigInt32(_baseAddr, offset);
  }
  inline void writeReg8(UInt32 offset, UInt8 data) {
    // Can't handle non 32-bit writes.
    UInt32 shift = 8 * (offset & 3);
		offset &= -4;

    UInt32 tmp = OSReadBigInt32(_baseAddr, offset);
    tmp = (data << shift) | (tmp & ~(0xFF << shift));
    OSWriteBigInt32(_baseAddr, offset, tmp);
  }
  inline void writeReg16(UInt32 offset, UInt16 data) {
    // Can't handle non 32-bit writes.
    UInt32 shift = 8 * (offset & 2);
		offset &= -4;

    UInt32 tmp = OSReadBigInt32(_baseAddr, offset);
    tmp = (data << shift) | (tmp & ~(0xFFFF << shift));
    OSWriteBigInt32(_baseAddr, offset, tmp);
  }
  inline void writeReg32(UInt32 offset, UInt32 data) {
    OSWriteBigInt32(_baseAddr, offset, data);
  }

  void handleInterrupt(IOInterruptEventSource *intEventSource, int count);
  void handleAsyncReadWriteCompletion(WiiSDCommand *command);

  //
  // Commands.
  //
  IOReturn sendCommand(UInt8 commandIndex, UInt8 responseType, UInt32 argument,
                       IOMemoryDescriptor *buffer, IOByteCount bufferOffset,
                       UInt16 blockCount, SDCommandResponse *outResponse = NULL);
  IOReturn sendCommand(UInt8 commandIndex, UInt8 responseType, UInt32 argument,
                       SDCommandResponse *outResponse = NULL);
  IOReturn sendAppCommand(UInt8 commandIndex, UInt8 responseType, UInt32 argument,
                          SDCommandResponse *outResponse = NULL);
  IOReturn sendCommandAsync(UInt8 commandIndex, UInt8 responseType, UInt32 argument,
                            IOMemoryDescriptor *buffer, UInt16 blockCount, IOStorageCompletion completion);
  void enqueueCommand(WiiSDCommand *command);
  WiiSDCommand *dequeueFirstCommand(void);
  IOReturn executeCommand(WiiSDCommand *command);
  IOReturn executeCommandGated(WiiSDCommand *command);
  void dispatchNext(void);
  void doAsyncIO(UInt32 intStatus = 0);
  IOReturn prepareDataTx(void);
  void completeIO(IOReturn status);

  //
  // Controller.
  //
  // Get controller version.
  inline UInt8 getControllerVersion(void) {
    return (UInt8) (readReg16(kSDHCRegHostControllerVersion) & kSDHCRegHostControllerVersionMask);
  }
  // Get controller present state.
  inline UInt32 getControllerPresentState(void) {
    return readReg32(kSDHCRegPresentState);
  }

  IOReturn resetController(UInt8 bits);
  IOReturn initController(void);
  IOReturn setControllerClock(UInt32 speedHz);
  void setControllerPower(bool enabled);
  void setControllerBusWidth(SDBusWidth busWidth);

  //
  // Card.
  //
  // Check if card is write protected.
  inline bool isCardWriteProtected() {
    return (readReg32(kSDHCRegPresentState) & kSDHCRegPresentStateCardWriteable) == 0;
  }

  IOReturn selectDeselectCard(bool select);
  IOReturn readCardCSD(void);
  IOReturn setCardBusWidth(SDBusWidth busWidth);
  IOReturn setCardBlockLength(UInt16 blockLength);
  IOReturn resetCard(void);
  IOReturn initCard(void);

public:
  //
  // Overrides.
  //
  bool init(OSDictionary *dictionary = 0);
  bool start(IOService *provider);

  //
  // SDHC functions.
  //
  bool isCardPresent() { return (readReg32(kSDHCRegPresentState) & kSDHCRegPresentStateCardInserted) != 0; }
  void setStorageProperties(IOService *service);
  IOReturn doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt32 block, UInt32 nblks, IOStorageCompletion completion);
  char *getVendorString(void);
  char *getProductString(void);
  char *getRevisionString(void);
  char *getSerialNumberString(void);
  IOReturn reportBlockSize(UInt64 *blockSize);
  IOReturn reportMaxReadTransfer(UInt64 blockSize, UInt64 *max);
  IOReturn reportMaxWriteTransfer(UInt64 blockSize, UInt64 *max);
  IOReturn reportMaxValidBlock(UInt64 *maxBlock);
  IOReturn reportMediaState(bool *mediaPresent, bool *changedState = 0);
  IOReturn reportWriteProtection(bool *isWriteProtected);
};

#endif

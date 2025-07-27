//
//  WiiSDCommand.hpp
//  Wii SD command structure
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiSDHCCommand_hpp
#define WiiSDHCCommand_hpp

#include <IOKit/IOCommand.h>
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/IOTypes.h>
#include <IOKit/storage/IOStorage.h>

#include "SDHCRegs.hpp"

//
// Command state.
//
typedef enum {
  kWiiSDCommandStateInitial = 0,
  kWiiSDCommandStateStarted,
  kWiiSDCommandStateCmd,
  kWiiSDCommandStateDataTx,
  kWiiSDCommandStateComplete,
  kWiiSDCommandStateDone
} WiiSDCommandState;

//
// Represents a SD command.
//
class WiiSDCommand : public IOCommand {
  OSDeclareDefaultStructors(WiiSDCommand);
  typedef IOCommand super;

public:
  typedef void (*Action)(OSObject *owner, WiiSDCommand *command);

private:
  UInt8     _commandIndex;
  UInt8     _responseType;
  UInt32    _argument;
  UInt16    _blockCount;
  IOReturn  _status;
  UInt64    _actualByteCount;

  IOMemoryDescriptor  *_buffer;
  IOByteCount         _bufferOffset;
  SDCommandResponse   _response;
  Action              _callbackAction;
  OSObject            *_callbackOwner;
  IOStorageCompletion _storageCompletion;

public:
  // Used to call command synchronously.
  IOSyncer *syncer;

  // Used to queue commands.
  queue_chain_t	queueChain;

  // Command state.
  WiiSDCommandState state;

  // Current segment.
  IOPhysicalSegment bufferSegment;

  //
  // Command functions.
  //
  void zeroCommand(void);
  void executeCallback(void);

  //
  // Gets.
  //
  UInt8 getCommandIndex(void);
  UInt8 getResponseType(void);
  UInt32 getArgument(void);
  UInt16 getBlockCount(void);
  IOReturn getStatus(void);
  UInt64 getActualByteCount(void);
  IOMemoryDescriptor *getBuffer(void);
  IOByteCount getBufferOffset(void);
  SDCommandResponse *getResponseBuffer(void);
  IOStorageCompletion getStorageCompletion(void);

  //
  // Sets.
  //
  void setCommandIndex(UInt8 commandIndex);
  void setResponseType(UInt8 responseType);
  void setArgument(UInt32 argument);
  void setBlockCount(UInt16 blockCount);
  void setStatus(IOReturn status);
  void setActualByteCount(UInt64 actualByteCount);
  void setBuffer(IOMemoryDescriptor *buffer);
  void setBufferOffset(IOByteCount bufferOffset);
  void setCallback(Action action, OSObject *owner);
  void setStorageCompletion(IOStorageCompletion storageCompletion);
};

#endif

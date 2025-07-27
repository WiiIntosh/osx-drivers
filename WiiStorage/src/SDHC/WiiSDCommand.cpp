//
//  WiiSDCommand.cpp
//  Wii SD command structure
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include "WiiSDCommand.hpp"

OSDefineMetaClassAndStructors(WiiSDCommand, super);

//
// Zero out command data prior to re-use.
//
void WiiSDCommand::zeroCommand(void) {
  syncer                = NULL;
  state                 = kWiiSDCommandStateInitial;

  _commandIndex         = 0;
  _responseType         = kSDHCResponseTypeR0;
  _argument             = 0;
  _blockCount           = 0;
  _buffer               = NULL;
  _bufferOffset         = 0;
  _status               = 0;
  _actualByteCount      = 0;
  _callbackAction       = NULL;
  _callbackOwner        = NULL;

  bzero(&_response, sizeof (_response));
  bzero(&_storageCompletion, sizeof (_storageCompletion));
}

//
// Execute command callback.
//
void WiiSDCommand::executeCallback(void) {
  if (_callbackAction != NULL) {
    (_callbackAction)(_callbackOwner, this);
  } else if (syncer != NULL) {
    syncer->signal();
    syncer = NULL;
	}
}

//
// Gets.
//
UInt8 WiiSDCommand::getCommandIndex(void) {
  return _commandIndex;
}

UInt8 WiiSDCommand::getResponseType(void) {
  return _responseType;
}

UInt32 WiiSDCommand::getArgument(void) {
  return _argument;
}

UInt16 WiiSDCommand::getBlockCount(void) {
  return _blockCount;
}

IOReturn WiiSDCommand::getStatus(void) {
  return _status;
}

UInt64 WiiSDCommand::getActualByteCount(void) {
  return _actualByteCount;
}

IOMemoryDescriptor *WiiSDCommand::getBuffer(void) {
  return _buffer;
}

IOByteCount WiiSDCommand::getBufferOffset(void) {
  return _bufferOffset;
}

SDCommandResponse *WiiSDCommand::getResponseBuffer(void) {
  return &_response;
}

IOStorageCompletion WiiSDCommand::getStorageCompletion(void) {
  return _storageCompletion;
}

//
// Sets.
//
void WiiSDCommand::setCommandIndex(UInt8 commandIndex) {
  _commandIndex = commandIndex;
}

void WiiSDCommand::setResponseType(UInt8 responseType) {
  _responseType = responseType;
}

void WiiSDCommand::setArgument(UInt32 argument) {
  _argument = argument;
}

void WiiSDCommand::setBlockCount(UInt16 blockCount) {
  _blockCount = blockCount;
}

void WiiSDCommand::setStatus(IOReturn status) {
  _status = status;
}

void WiiSDCommand::setActualByteCount(UInt64 actualByteCount) {
  _actualByteCount = actualByteCount;
}

void WiiSDCommand::setBuffer(IOMemoryDescriptor *buffer) {
  _buffer = buffer;
}

void WiiSDCommand::setBufferOffset(IOByteCount bufferOffset) {
  _bufferOffset = bufferOffset;
}

void WiiSDCommand::setCallback(Action action, OSObject *owner) {
  _callbackAction = action;
  _callbackOwner  = owner;
}

void WiiSDCommand::setStorageCompletion(IOStorageCompletion storageCompletion) {
  _storageCompletion = storageCompletion;
}

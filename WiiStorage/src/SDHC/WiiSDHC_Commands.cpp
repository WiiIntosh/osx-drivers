//
//  WiiSDHC_Commands.cpp
//  Wii SD host controller interface (host controller commands)
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include "WiiSDHC.hpp"

//
// Sends a synchronous command to the card.
//
IOReturn WiiSDHC::sendCommand(UInt8 commandIndex, UInt8 responseType, UInt32 argument,
                              IOMemoryDescriptor *buffer, IOByteCount bufferOffset,
                              UInt16 blockCount, SDCommandResponse *outResponse) {
  WiiSDCommand  *sdCommand;
  IOSyncer      *syncer;
  IOReturn      status;

  sdCommand = new WiiSDCommand;
  if (sdCommand == NULL) {
    return kIOReturnNoResources;
  }
  sdCommand->zeroCommand();

  sdCommand->setCommandIndex(commandIndex);
  sdCommand->setResponseType(responseType);
  sdCommand->setArgument(argument);
  sdCommand->setBuffer(buffer);
  sdCommand->setBufferOffset(bufferOffset);
  sdCommand->setBlockCount(blockCount);

  syncer = IOSyncer::create();
  sdCommand->syncer = syncer;

  WIIDBGLOG("Sync command: 0x%X, rspType: 0x%X, arg: 0x%X", commandIndex, responseType, argument);
  status = executeCommand(sdCommand);
  if (status != kIOReturnSuccess) {
    sdCommand->release();
    return status;
  }

  syncer->wait();
	status = sdCommand->getStatus();

  if (outResponse != NULL) {
    memcpy(outResponse, sdCommand->getResponseBuffer(), sizeof (*outResponse));
  }

  sdCommand->release();
  WIIDBGLOG("Command complete status 0x%X", status);
  return status;
}

//
// Sends a synchronous command to the card.
//
IOReturn WiiSDHC::sendCommand(UInt8 commandIndex, UInt8 responseType, UInt32 argument,
                              SDCommandResponse *outResponse) {
  return sendCommand(commandIndex, responseType, argument, NULL, 0, 0, outResponse);
}

//
// Sends a synchronous SD application command to the card.
//
IOReturn WiiSDHC::sendAppCommand(UInt8 commandIndex, UInt8 responseType, UInt32 argument,
                                 SDCommandResponse *outResponse) {
  SDCommandResponse   appResponse;
  IOReturn            status;

  status = sendCommand(kSDCommandAppCommand, kSDHCResponseTypeR1, (_cardAddress << kSDRelativeAddressShift), &appResponse);
  if (status != kIOReturnSuccess) {
    return status;
  }

  return sendCommand(commandIndex, responseType, argument, outResponse);
}

//
// Sends an asynchronous SD command to the card.
//
IOReturn WiiSDHC::sendCommandAsync(UInt8 commandIndex, UInt8 responseType, UInt32 argument,
                                   IOMemoryDescriptor *buffer, UInt16 blockCount, IOStorageCompletion completion) {
  WiiSDCommand  *sdCommand;
  IOReturn      status;

  sdCommand = new WiiSDCommand;
  if (sdCommand == NULL) {
    return kIOReturnNoResources;
  }

  sdCommand->zeroCommand();

  sdCommand->setCommandIndex(commandIndex);
  sdCommand->setResponseType(responseType);
  sdCommand->setArgument(argument);
  sdCommand->setBuffer(buffer);
  sdCommand->setBlockCount(blockCount);
  sdCommand->setStorageCompletion(completion);
  sdCommand->setCallback(OSMemberFunctionCast(WiiSDCommand::Action, this, &WiiSDHC::handleAsyncReadWriteCompletion), this);

  WIIDBGLOG("Async command: 0x%X, rspType: 0x%X, arg: 0x%X", commandIndex, responseType, argument);
  status = executeCommand(sdCommand);
  if (status != kIOReturnSuccess) {
    sdCommand->release();
  }

  return status;
}

//
// Adds a command to the queue.
//
// This function must only be called within the work loop context.
//
void WiiSDHC::enqueueCommand(WiiSDCommand *command) {
  queue_enter(&_commandQueue, command, WiiSDCommand*, queueChain);
}

//
// Gets the next command from the queue.
//
// This function must only be called within the work loop context.
//
WiiSDCommand *WiiSDHC::dequeueFirstCommand(void) {
  WiiSDCommand *command = NULL;

	if(!queue_empty(&_commandQueue)) {
		queue_remove_first(&_commandQueue, command, WiiSDCommand*, queueChain);
	}
	return command;
}

//
// Submits a command for execution.
//
IOReturn WiiSDHC::executeCommand(WiiSDCommand *command) {
  if (command == NULL) {
    return kIOReturnBadArgument;
  }

  return _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &WiiSDHC::executeCommandGated), command);
}

//
// Submits a command for execution.
//
// This function must only be called within the work loop context.
//
IOReturn WiiSDHC::executeCommandGated(WiiSDCommand *command) {
  //
  // Queue up the command and execute the next command if one is not already.
  //
  enqueueCommand(command);
  if (_currentCommand == NULL) {
    dispatchNext();
  }

  return kIOReturnSuccess;
}

//
// Starts executing the next command in the queue.
//
// This function must only be called within the work loop context.
//
void WiiSDHC::dispatchNext(void) {
  //
  // Ensure the controller is free. This function sometimes is
  // called directly from a completion, so need to ensure everything executes in the right order.
  //
  if (_sdhcState != kSDHCStateFree) {
    return;
  }
  _sdhcState = kSDHCStateBusy;

	//
  // Get the next command.
  //
	_currentCommand = dequeueFirstCommand();
	if (_currentCommand == NULL) {
    _sdhcState = kSDHCStateFree;
		return;
	}

  _currentCommand->state = kWiiSDCommandStateStarted;

  //
  // Ensure a card is inserted.
  //
  if (!isCardPresent()) {
    completeIO(kIOReturnNoMedia);
    return;
  }

  //
  // Move to command I/O.
  //
  doAsyncIO();
}

//
// Performs I/O on the current command.
//
// This function must only be called within the work loop context.
//
void WiiSDHC::doAsyncIO(UInt32 intStatus) {
  UInt32              timeout;
  UInt16              commandValue;
  UInt16              transferMode;
  IOMemoryDescriptor  *memoryDescriptor;
  IOPhysicalSegment   *seg;
  IOPhysicalAddress   doubleBufferOffset;
  SDCommandResponse   *response;
  IOReturn            status;

  status = kIOReturnSuccess;
  WIIDBGLOG("State machine: %u, int: 0x%X", _currentCommand->state, intStatus);
  switch (_currentCommand->state) {
    //
    // Command is starting.
    //
    case kWiiSDCommandStateStarted:
      //
      // Wait for controller to be ready.
      //
      for (timeout = kSDHCCommandTimeoutMS; timeout > 0; timeout--) {
       if ((readReg32(kSDHCRegPresentState) & (kSDHCRegPresentStateCmdInhibit | kSDHCRegPresentStateDatInhibit)) == 0) {
          break;
        }
        IODelay(1);
      }

      if (timeout == 0) {
        WIISYSLOG("Timed out waiting for command inhibit");
        _currentCommand->state = kWiiSDCommandStateComplete;
        status                 = kIOReturnTimeout;
        break;
      }

      memoryDescriptor = _currentCommand->getBuffer();
      if ((memoryDescriptor != NULL) && (_currentCommand->getBlockCount() == 0)) {
        _currentCommand->setCommandIndex(kSDCommandReadSingleBlock);
      }

      // Build out command register.
      commandValue  = (_currentCommand->getCommandIndex() << kSDHCRegCommandIndexShift) & kSDHCRegCommandIndexMask;
      commandValue |= (_currentCommand->getResponseType() & kSDHCResponseTypeMask);

      //
      // Configure DMA for data transfer if there is one.
      //
      if (memoryDescriptor != NULL) {
        //
        // Ensure buffer is large enough for block count.
        //
        if (memoryDescriptor->getLength() < (_currentCommand->getBlockCount() * _cardBlockLength)) {
          _currentCommand->state = kWiiSDCommandStateComplete;
          status                 = kIOReturnNoMemory;
          break;
        }

        WIIDBGLOG("block %u, count %u, size %u", _currentCommand->getArgument(), _currentCommand->getBlockCount(), _cardBlockLength);
        commandValue |= kSDHCRegCommandDataPresent;
        transferMode = kSDHCRegTransferModeDMAEnable;
        if ((_currentCommand->getCommandIndex() == kSDCommandReadMultipleBlock) || (_currentCommand->getCommandIndex() == kSDCommandWriteMultipleBlock)) {
          transferMode |= kSDHCRegTransferModeBlockCountEnable | kSDHCRegTransferModeMultipleBlock | kSDHCRegTransferModeAutoCMD12;
        }
        if ((_currentCommand->getBlockCount() == 0) || (memoryDescriptor->getDirection() == kIODirectionIn)) {
          transferMode |= kSDHCRegTransferModeDataTransferRead;
        }

        //
        // Prepare for data transfer.
        //
        status = prepareDataTx();
        if (status != kIOReturnSuccess) {
          _currentCommand->state = kWiiSDCommandStateComplete;
          break;
        }

        if (_currentCommand->getBlockCount() == 0) {
          writeReg32(kSDHCRegBlockSize, _cardBlockLength | (1 << 16));
        } else {
          writeReg32(kSDHCRegBlockSize, _cardBlockLength | (_currentCommand->getBlockCount() << 16));
        }
      } else {
        transferMode = 0;
        writeReg32(kSDHCRegBlockSize, 0);
      }

      //
      // Write data for command.
      // Command must be written together with transfer mode as both are 16-bit registers.
      //
      writeReg32(kSDHCRegArgument, _currentCommand->getArgument());
      writeReg32(kSDHCRegTransferMode, transferMode | (commandValue << 16));
      _currentCommand->state = kWiiSDCommandStateCmd;
      break;

    //
    // Command has completed and response is ready.
    // For data commands, data transfer will occur afterwards.
    //
    case kWiiSDCommandStateCmd:
      //
      // Ensure we got the command complete interrupt.
      //
      if ((intStatus & kSDHCRegNormalIntStatusCommandComplete) == 0) {
        WIISYSLOG("Command completed without interrupt? 0x%X", intStatus);
        _currentCommand->state = kWiiSDCommandStateComplete;
        status                 = kIOReturnIOError;
        break;
      }

      //
      // Read response data.
      // Data is big endian, but need to put the ints in the right order.
      //
      response = _currentCommand->getResponseBuffer();
      response->data[3] = readReg32(kSDHCRegResponse0);
      response->data[2] = readReg32(kSDHCRegResponse1);
      response->data[1] = readReg32(kSDHCRegResponse2);
      response->data[0] = readReg32(kSDHCRegResponse3);
      WIIDBGLOG("Card status: 0x%X", response->r1);

      //
      // Command is done if no data.
      //
      if (_currentCommand->getBuffer() == NULL) {
        _currentCommand->state = kWiiSDCommandStateComplete;
        break;
      }
      _currentCommand->state = kWiiSDCommandStateDataTx;

      //
      // Check if DMA is ready or transfer has completed, if not wait until the next interrupt.
      //
      if ((intStatus & (kSDHCRegNormalIntStatusDMAInterrupt | kSDHCRegNormalIntStatusTransferComplete)) == 0) {
        break;
      }

    case kWiiSDCommandStateDataTx:
      //
      // Ensure we got either a DMA or a transfer complete interrupt.
      //
      if ((intStatus & (kSDHCRegNormalIntStatusDMAInterrupt | kSDHCRegNormalIntStatusTransferComplete)) == 0) {
        WIISYSLOG("Command data without interrupt? 0x%X", intStatus);
        _currentCommand->state = kWiiSDCommandStateComplete;
        status                 = kIOReturnIOError;
        break;
      }

      memoryDescriptor = _currentCommand->getBuffer();
      seg = &_currentCommand->bufferSegment;
      if (_currentCommand->getBlockCount() != 0) {
        if (memoryDescriptor->getDirection() == kIODirectionIn) {
          memoryDescriptor->prepare();
          _invalidateCacheFunc((vm_offset_t) _doubleBufferPtr, seg->length, false);
          if (_currentCommand->getArgument() == 0) { // TODO: 10.3 does not like MBR disks
            WIISYSLOG("Got block 0, MBR HACK: 0x%X", *((UInt16*) &_doubleBufferPtr[0x1FE]));
            _doubleBufferPtr[0x1FE] = 0x00;
            _doubleBufferPtr[0x1FF] = 0x00;
            WIISYSLOG("Got block 0, MBR HACK after fix: 0x%X", *((UInt16*) &_doubleBufferPtr[0x1FE]));
          }
          memoryDescriptor->writeBytes(_currentCommand->getBufferOffset() - seg->length, _doubleBufferPtr, seg->length);
          memoryDescriptor->complete();
        }
      }

      //
      // Check if double buffer was used, if so need to copy to original during a read operation.
      //
      /*if ((seg->location & ~(PAGE_MASK)) == _doubleBufferSegment.location) {
        doubleBufferOffset = PAGE_SIZE - seg->length;
        memoryDescriptor->writeBytes(_currentCommand->getBufferOffset() - seg->length, _doubleBufferPtr + doubleBufferOffset, seg->length);
      }*/

      //
      // On transfer completed, verify we processed all the data.
      //
      if ((intStatus & kSDHCRegNormalIntStatusTransferComplete) != 0) {
        if (_currentCommand->getActualByteCount() != (_currentCommand->getBlockCount() * _cardBlockLength)) {
          WIISYSLOG("Didn't get all the data here");
          status = kIOReturnIOError;
        }
        WIIDBGLOG("DMA done");
        _currentCommand->state = kWiiSDCommandStateComplete;
        break;
      }

      //
      // If a DMA interrupt was triggered, but no remaining data is to be processed, just skip until the transfer complete interrupt.
      //
      if (_currentCommand->getActualByteCount() == (_currentCommand->getBlockCount() * _cardBlockLength)) {
        WIIDBGLOG("DMA finish, skip");
        break;
      }

      //
      // More data to process.
      //
      status = prepareDataTx();
      if (status != kIOReturnSuccess) {
        _currentCommand->state = kWiiSDCommandStateComplete;
      }
      break;

    default:
      _currentCommand->state = kWiiSDCommandStateComplete;
      status                 = kIOReturnIOError;
      break;
  }

  if (_currentCommand->state == kWiiSDCommandStateComplete) {
    completeIO(status);
  }
}

//
// Prepares for data transfer during command execution.
//
// This function must only be called within the work loop context.
//
IOReturn WiiSDHC::prepareDataTx(void) {
  IOMemoryDescriptor  *memoryDescriptor;
  IOPhysicalSegment   *seg;
  IOPhysicalAddress   doubleBufferOffset;

  //
  // Copy buffer to double buffer for a write. TODO: Can probably use buffer directly but it must be
  // checked to align to 32 bytes to prevent unexpected corruption from a flush or an invalidate.
  //
  memoryDescriptor = _currentCommand->getBuffer();
  seg              = &_currentCommand->bufferSegment;
  WIIDBGLOG("Buffer offset: 0x%X", _currentCommand->getBufferOffset());

  seg->location = _doubleBufferSegment.location;
  seg->length = _currentCommand->getBlockCount() * _cardBlockLength; // TODO

  /*if (_memoryCursor->getPhysicalSegments(memoryDescriptor, _currentCommand->getBufferOffset(), seg, 1) != 1) {
    WIISYSLOG("Failed to generate DMA segments");
    return kIOReturnDMAError;
  }

  WIIDBGLOG("DMA int seg: 0x%X len: 0x%X", seg->location, seg->length);*/
  /*if ((seg->location + seg->length) > ((seg->location + PAGE_SIZE) & ~(PAGE_MASK))) {
    //
    // Truncate segment so it does not extend beyond the next page boundary.
    //
    seg->length = (seg->location + seg->length) - ((seg->location + PAGE_SIZE) & ~(PAGE_MASK));
    WIIDBGLOG("Truncating segment length to 0x%X", seg->length);

  } else {
    //
    // Ensure segment reaches the next page boundary if remaining data would result in the controller reaching that point.
    // Segments do not need to be aligned, but if they are smaller than a page they must end at the next page boundary.
    //
    // Fallback to the double buffer for this DMA transaction in this instance.
    //
    if ((seg->length < ((_currentCommand->getBlockCount() * _cardBlockLength) - _currentCommand->getActualByteCount()))
      && ((seg->location + seg->length) & PAGE_MASK) != 0) {
      doubleBufferOffset = PAGE_SIZE - seg->length;
      if (memoryDescriptor->getDirection() == kIODirectionOut) {
        memoryDescriptor->readBytes(_currentCommand->getBufferOffset(), _doubleBufferPtr + doubleBufferOffset, seg->length);
      }
      seg->location = _doubleBufferSegment.location + doubleBufferOffset;
    }
  }*/

  if (_currentCommand->getBlockCount() > 0) {
    if (memoryDescriptor->getDirection() == kIODirectionOut) {
      memoryDescriptor->prepare();
      memoryDescriptor->readBytes(_currentCommand->getBufferOffset(), _doubleBufferPtr, seg->length);

      flushDataCache(_doubleBufferPtr, seg->length);
      memoryDescriptor->complete();
    }

    _currentCommand->setBufferOffset(_currentCommand->getBufferOffset() + seg->length);
    _currentCommand->setActualByteCount(_currentCommand->getActualByteCount() + seg->length);
  }

  writeReg32(kSDHCRegSDMA, seg->location);
  return kIOReturnSuccess;
}

//
// Completes the current command.
//
// This function must only be called within the work loop context.
//
void WiiSDHC::completeIO(IOReturn status) {
  WiiSDCommand  *finishedCommand;

  //
  // Get the current command.
  //
  finishedCommand = _currentCommand;
  if (finishedCommand == NULL) {
    return;
  }
  _currentCommand = NULL;
  _sdhcState      = kSDHCStateFree;

  //
  // Command is done, set result and invoke callback.
  //
	finishedCommand->state = kWiiSDCommandStateDone;
  finishedCommand->setStatus(status);
	finishedCommand->executeCallback();

  //
  // Check for more work.
  //
	dispatchNext();
}

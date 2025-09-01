//
//  WiiOHCI_Buffers.cpp
//  Wii OHCI USB controller interface
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include "WiiOHCI.hpp"

OSDefineMetaClassAndStructors(WiiOHCIEndpointBuffer, super);
OSDefineMetaClassAndStructors(WiiOHCITransferBuffer, super);

//
// Overrides OSObject::free().
//
void WiiOHCIEndpointBuffer::free(void) {
  if (_buffer != NULL) {
    _buffer->complete();
    OSSafeReleaseNULL(_buffer);
  }
  super::free();
}

//
// Allocates a new endpoint buffer.
//
WiiOHCIEndpointBuffer *WiiOHCIEndpointBuffer::endpointBuffer(void) {
  WiiOHCIEndpointBuffer *edBuffer;
  IOByteCount           length;

  edBuffer = new WiiOHCIEndpointBuffer;
  if (edBuffer == NULL) {
    return NULL;
  }

  //
  // Allocate host controller endpoint descriptors out of a page.
  // Wii platforms are not cache coherent, host controller structures must be non-cacheable.
  //
  edBuffer->_buffer = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous, PAGE_SIZE, PAGE_SIZE);
  if (edBuffer->_buffer == NULL) {
    edBuffer->release();
    return NULL;
  }

  edBuffer->_buffer->prepare();
  edBuffer->_endpointDescriptors = (OHCIEndpointDescriptor*) edBuffer->_buffer->getBytesNoCopy();
  edBuffer->_physicalAddr        = edBuffer->_buffer->getPhysicalSegment(0, &length);
  IOSetProcessorCacheMode(kernel_task, (IOVirtualAddress) edBuffer->_endpointDescriptors, PAGE_SIZE, kIOInhibitCache);

  //
  // Configure endpoint data.
  //
  for (UInt32 i = 0; i < kWiiOHCIEndpointsPerBuffer; i++) {
    edBuffer->_endpoints[i].ed           = &edBuffer->_endpointDescriptors[i];
    edBuffer->_endpoints[i].physAddr     = edBuffer->_physicalAddr + (i * sizeof (edBuffer->_endpointDescriptors[0]));
    edBuffer->_endpoints[i].nextEndpoint = NULL;
  }

  return edBuffer;
}

//
// Sets the next buffer in the linked list.
//
void WiiOHCIEndpointBuffer::setNextBuffer(WiiOHCIEndpointBuffer *buffer) {
  _nextBuffer = buffer;
}

//
// Gets the next buffer in the linked list.
//
WiiOHCIEndpointBuffer *WiiOHCIEndpointBuffer::getNextBuffer(void) {
  return _nextBuffer;
}

//
// Gets the starting physical address of the buffer.
//
IOPhysicalAddress WiiOHCIEndpointBuffer::getPhysAddr(void) {
  return _physicalAddr;
}

//
// Gets the endpoint data at the specified index.
//
OHCIEndpointData *WiiOHCIEndpointBuffer::getEndpoint(UInt32 index) {
  if (index >= kWiiOHCIEndpointsPerBuffer) {
    return NULL;
  }
  return &_endpoints[index];
}

//
// Overrides OSObject::free().
//
void WiiOHCITransferBuffer::free(void) {
  if (_buffer != NULL) {
    _buffer->complete();
    OSSafeReleaseNULL(_buffer);
  }
  super::free();
}

//
// Allocates a new transfer buffer.
//
WiiOHCITransferBuffer *WiiOHCITransferBuffer::transferBuffer(bool isochronous) {
  WiiOHCITransferBuffer   *transferBuffer;
  IOByteCount             numTransfers;
  IOByteCount             length;

  transferBuffer = new WiiOHCITransferBuffer;
  if (transferBuffer == NULL) {
    return NULL;
  }
  transferBuffer->_isochronous = isochronous;

  //
  // Allocate the transfer data.
  //
  numTransfers = isochronous ? kWiiOHCIIsoTransfersPerBuffer : kWiiOHCIGenTransfersPerBuffer;
  transferBuffer->_transfers = (OHCITransferData *) IOMalloc(numTransfers * sizeof (OHCITransferData));
  if (transferBuffer->_transfers == NULL) {
    transferBuffer->release();
    return NULL;
  }

  //
  // Allocate host controller endpoint descriptors out of a page.
  // Wii platforms are not cache coherent, host controller structures must be non-cacheable.
  //
  transferBuffer->_buffer = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous, PAGE_SIZE, PAGE_SIZE);
  if (transferBuffer->_buffer == NULL) {
    transferBuffer->release();
    return NULL;
  }

  transferBuffer->_buffer->prepare();
  transferBuffer->_physicalAddr = transferBuffer->_buffer->getPhysicalSegment(0, &length);
  if (isochronous) {
    transferBuffer->_isoTDs = (OHCIIsoTransferDescriptor *) transferBuffer->_buffer->getBytesNoCopy();
    IOSetProcessorCacheMode(kernel_task, (IOVirtualAddress) transferBuffer->_isoTDs, PAGE_SIZE, kIOInhibitCache);
  } else {
    transferBuffer->_genTDs = (OHCIGenTransferDescriptor *) transferBuffer->_buffer->getBytesNoCopy();
    IOSetProcessorCacheMode(kernel_task, (IOVirtualAddress) transferBuffer->_genTDs, PAGE_SIZE, kIOInhibitCache);
  }

  //
  // Configure transfer data.
  //
  for (UInt32 i = 0; i < (isochronous ? kWiiOHCIIsoTransfersPerBuffer : kWiiOHCIGenTransfersPerBuffer); i++) {
    transferBuffer->_transfers[i].isochronous = isochronous;
    if (transferBuffer->_transfers[i].isochronous) {
      transferBuffer->_transfers[i].isoTD    = &transferBuffer->_isoTDs[i];
      transferBuffer->_transfers[i].physAddr = transferBuffer->_physicalAddr + (i * sizeof (transferBuffer->_isoTDs[0]));
    } else {
      transferBuffer->_transfers[i].genTD    = &transferBuffer->_genTDs[i];
      transferBuffer->_transfers[i].physAddr = transferBuffer->_physicalAddr + (i * sizeof (transferBuffer->_genTDs[0]));
    }
    transferBuffer->_transfers[i].nextTransfer = NULL;
  }

  return transferBuffer;
}

//
// Gets the next buffer in the linked list.
//
void WiiOHCITransferBuffer::setNextBuffer(WiiOHCITransferBuffer *buffer) {
  _nextBuffer = buffer;
}

//
// Gets the next buffer in the linked list.
//
WiiOHCITransferBuffer *WiiOHCITransferBuffer::getNextBuffer(void) {
  return _nextBuffer;
}

//
// Gets the starting physical address for the buffer.
//
IOPhysicalAddress WiiOHCITransferBuffer::getPhysAddr(void) {
  return _physicalAddr;
}


//
// Gets a transfer at the specified index.
//
OHCITransferData *WiiOHCITransferBuffer::getTransfer(UInt32 index) {
  if (index >= (_isochronous ? kWiiOHCIIsoTransfersPerBuffer : kWiiOHCIGenTransfersPerBuffer)) {
    return NULL;
  }
  return &_transfers[index];
}

//
// Gets the transfer data from a given physical address of a transfer descriptor, assuming this buffer contains it.
//
OHCITransferData *WiiOHCITransferBuffer::getTransferFromPhysAddr(IOPhysicalAddress physAddr) {
  if ((physAddr & ~(PAGE_MASK)) != _physicalAddr) {
    return NULL;
  }
  return &_transfers[(physAddr & PAGE_MASK) / (_isochronous ? sizeof (_isoTDs[0]) : sizeof (_genTDs[0]))];
}

//
// Allocates a new bounce buffer.
//
OHCIBounceBuffer *WiiOHCI::allocateBounceBuffer(bool jumbo) {
  OHCIBounceBuffer  *bounceBuffer;
  IOByteCount       length;
  IOByteCount       bufferLength;

  bounceBuffer = (OHCIBounceBuffer*) IOMalloc(sizeof (OHCIBounceBuffer));
  if (bounceBuffer == NULL) {
    return NULL;
  }

  bounceBuffer->jumbo = jumbo;
  bounceBuffer->next  = NULL;
  bufferLength = jumbo ? kWiiOHCIBounceBufferJumboSize : kWiiOHCIBounceBufferSize;

  //
  // If allocator was provided, use that. Otherwise just allocate from regular kernel memory.
  //
  if (_mem2Allocator != NULL) {
    if (!_mem2Allocator->allocate(bufferLength, &bounceBuffer->physAddr, bufferLength)) {
      return NULL;
    }

    bounceBuffer->desc = IOMemoryDescriptor::withPhysicalAddress(bounceBuffer->physAddr, bufferLength, kIODirectionInOut);
    if (bounceBuffer->desc == NULL) {
      return NULL;
    }
    bounceBuffer->map = bounceBuffer->desc->map(kIOMapCopybackCache);
    if (bounceBuffer->map == NULL) {
      return NULL;
    }

    bounceBuffer->buf = (void*) bounceBuffer->map->getVirtualAddress();
  } else {
    bounceBuffer->desc = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous, bufferLength, bufferLength);
    if (bounceBuffer->desc == NULL) {
      return NULL;
    }

    bounceBuffer->map      = NULL;
    bounceBuffer->physAddr = bounceBuffer->desc->getPhysicalSegment(0, &length);
    bounceBuffer->buf      = ((IOBufferMemoryDescriptor*) bounceBuffer->desc)->getBytesNoCopy();
  }

  return bounceBuffer;
}

//
// Gets a free bounce buffer, or allocates ones if needed.
//
OHCIBounceBuffer *WiiOHCI::getFreeBounceBuffer(bool jumbo) {
  OHCIBounceBuffer  *bounceBuffer;

  if (jumbo) {
    bounceBuffer = _freeBounceBufferJumboHeadPtr;
    if (bounceBuffer != NULL) {
      _freeBounceBufferJumboHeadPtr = bounceBuffer->next;
      bounceBuffer->next            = NULL;
    }
  } else {
    bounceBuffer = _freeBounceBufferHeadPtr;
    if (bounceBuffer != NULL) {
      _freeBounceBufferHeadPtr = bounceBuffer->next;
      bounceBuffer->next       = NULL;
    }
  }

  if (bounceBuffer == NULL) {
    bounceBuffer = allocateBounceBuffer(jumbo);
  }

  return bounceBuffer;
}

//
// Returns a bounce buffer to the free list.
//
void WiiOHCI::returnBounceBuffer(OHCIBounceBuffer *bounceBuffer) {
  if (bounceBuffer->jumbo) {
    bounceBuffer->next = _freeBounceBufferJumboHeadPtr;
    _freeBounceBufferJumboHeadPtr = bounceBuffer;
  } else {
    bounceBuffer->next = _freeBounceBufferHeadPtr;
    _freeBounceBufferHeadPtr = bounceBuffer;
  }
}

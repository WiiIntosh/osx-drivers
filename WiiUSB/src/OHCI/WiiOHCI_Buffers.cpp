//
//  WiiOHCI_Buffers.cpp
//  Wii OHCI USB controller interface
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include "WiiOHCI.hpp"

OSDefineMetaClassAndStructors(WiiOHCIEndpointBuffer, super);
OSDefineMetaClassAndStructors(WiiOHCIGenTransferBuffer, super);

//
// Overrides IOBufferMemoryDescriptor::free().
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

void WiiOHCIEndpointBuffer::setNextBuffer(WiiOHCIEndpointBuffer *buffer) {
  _nextBuffer = buffer;
}

WiiOHCIEndpointBuffer *WiiOHCIEndpointBuffer::getNextBuffer(void) {
  return _nextBuffer;
}

IOPhysicalAddress WiiOHCIEndpointBuffer::getPhysAddr(void) {
  return _physicalAddr;
}

OHCIEndpointData *WiiOHCIEndpointBuffer::getEndpoint(UInt32 index) {
  if (index >= kWiiOHCIEndpointsPerBuffer) {
    return NULL;
  }
  return &_endpoints[index];
}

//
// Overrides IOBufferMemoryDescriptor::free().
//
void WiiOHCIGenTransferBuffer::free(void) {
  if (_buffer != NULL) {
    _buffer->complete();
    OSSafeReleaseNULL(_buffer);
  }
  super::free();
}

//
// Allocates a new general transfer buffer.
//
WiiOHCIGenTransferBuffer *WiiOHCIGenTransferBuffer::genTransferBuffer(void) {
  WiiOHCIGenTransferBuffer  *genTdBuffer;
  IOByteCount               length;

  genTdBuffer = new WiiOHCIGenTransferBuffer;
  if (genTdBuffer == NULL) {
    return NULL;
  }

  //
  // Allocate host controller endpoint descriptors out of a page.
  // Wii platforms are not cache coherent, host controller structures must be non-cacheable.
  //
  genTdBuffer->_buffer = IOBufferMemoryDescriptor::withOptions(kIOMemoryPhysicallyContiguous, PAGE_SIZE, PAGE_SIZE);
  if (genTdBuffer->_buffer == NULL) {
    genTdBuffer->release();
    return NULL;
  }

  genTdBuffer->_buffer->prepare();
  genTdBuffer->_genTransferDescriptors = (OHCIGenTransferDescriptor*) genTdBuffer->_buffer->getBytesNoCopy();
  genTdBuffer->_physicalAddr           = genTdBuffer->_buffer->getPhysicalSegment(0, &length);
  IOSetProcessorCacheMode(kernel_task, (IOVirtualAddress) genTdBuffer->_genTransferDescriptors, PAGE_SIZE, kIOInhibitCache);

  //
  // Configure endpoint data.
  //
  for (UInt32 i = 0; i < kWiiOHCIGenTransfersPerBuffer; i++) {
    genTdBuffer->_genTransfers[i].td           = &genTdBuffer->_genTransferDescriptors[i];
    genTdBuffer->_genTransfers[i].physAddr     = genTdBuffer->_physicalAddr + (i * sizeof (genTdBuffer->_genTransferDescriptors[0]));
    genTdBuffer->_genTransfers[i].nextTransfer = NULL;
  }

  return genTdBuffer;
}

void WiiOHCIGenTransferBuffer::setNextBuffer(WiiOHCIGenTransferBuffer *buffer) {
  _nextBuffer = buffer;
}

WiiOHCIGenTransferBuffer *WiiOHCIGenTransferBuffer::getNextBuffer(void) {
  return _nextBuffer;
}

IOPhysicalAddress WiiOHCIGenTransferBuffer::getPhysAddr(void) {
  return _physicalAddr;
}

OHCIGenTransferData *WiiOHCIGenTransferBuffer::getGenTransfer(UInt32 index) {
  if (index >= kWiiOHCIGenTransfersPerBuffer) {
    return NULL;
  }
  return &_genTransfers[index];
}

//
// Gets the general transfer data from a given physical address of a general transfer descriptor, assuming this buffer contains it.
//
OHCIGenTransferData *WiiOHCIGenTransferBuffer::getGenTransferFromPhysAddr(IOPhysicalAddress physAddr) {
  if ((physAddr & ~(PAGE_MASK)) != _physicalAddr) {
    return NULL;
  }
  return &_genTransfers[(physAddr & PAGE_MASK) / sizeof (_genTransferDescriptors[0])];
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

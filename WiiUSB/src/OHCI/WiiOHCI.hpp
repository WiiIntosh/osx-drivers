//
//  WiiOHCI.hpp
//  Wii OHCI USB controller interface
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiOHCI_hpp
#define WiiOHCI_hpp

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IORangeAllocator.h>
#include <IOKit/usb/IOUSBController.h>

#include "WiiCommon.hpp"
#include "OHCIRegs.hpp"

#define kWiiOHCITempBufferSize      0x800

//
// Total interrupt nodes in tree.
// 32 32ms nodes, 16 16ms nodes, 8 8ms nodes, 4 4ms nodes, 2 2ms nodes, 1 1ms node
//
#define kWiiOHCIInterruptNodeCount    (32 + 16 + 8 + 4 + 2 + 1)

//
// Endpoint type masks.
//
#define kWiiOHCIEndpointTypeControl             BIT0
#define kWiiOHCIEndpointTypeInterrupt           BIT1
#define kWiiOHCIEndpointTypeBulk                BIT2
#define kWiiOHCIEndpointTypeIsochronous         BIT3
#define kWiiOHCIEndpointTypeAll                 BITRange(0, 4)

#define kWiiOHCIEndpointsPerBuffer        (PAGE_SIZE / sizeof (OHCIEndpointDescriptor))
#define kWiiOHCIGenTransfersPerBuffer     (PAGE_SIZE / sizeof (OHCIGenTransferDescriptor))
#define kWiiOHCIIsoTransfersPerBuffer     (PAGE_SIZE / sizeof (OHCIIsoTransferDescriptor))

//
// OHCI endpoint memory buffer.
//
class WiiOHCIEndpointBuffer : public OSObject {
  OSDeclareDefaultStructors(WiiOHCIEndpointBuffer);
  typedef OSObject super;

private:
  IOBufferMemoryDescriptor *_buffer;
  IOPhysicalAddress       _physicalAddr;
  OHCIEndpointDescriptor  *_endpointDescriptors;
  WiiOHCIEndpointBuffer   *_nextBuffer;
  OHCIEndpointData        _endpoints[kWiiOHCIEndpointsPerBuffer];

public:
  //
  // Overrides.
  //
  void free();

  //
  // Buffer functions.
  //
  static WiiOHCIEndpointBuffer *endpointBuffer(void);
  void setNextBuffer(WiiOHCIEndpointBuffer *buffer);
  WiiOHCIEndpointBuffer *getNextBuffer(void);
  IOPhysicalAddress getPhysAddr(void);
  OHCIEndpointData *getEndpoint(UInt32 index);
};

//
// OHCI general transfer memory buffer.
//
class WiiOHCIGenTransferBuffer : public OSObject {
  OSDeclareDefaultStructors(WiiOHCIGenTransferBuffer);
  typedef OSObject super;

private:
  IOBufferMemoryDescriptor  *_buffer;
  IOPhysicalAddress         _physicalAddr;
  OHCIGenTransferDescriptor *_genTransferDescriptors;
  WiiOHCIGenTransferBuffer  *_nextBuffer;
  OHCIGenTransferData       _genTransfers[kWiiOHCIGenTransfersPerBuffer];

public:
  //
  // Overrides.
  //
  void free();

  //
  // Buffer functions.
  //
  static WiiOHCIGenTransferBuffer *genTransferBuffer(void);
  void setNextBuffer(WiiOHCIGenTransferBuffer *buffer);
  WiiOHCIGenTransferBuffer *getNextBuffer(void);
  IOPhysicalAddress getPhysAddr(void);
  OHCIGenTransferData *getGenTransfer(UInt32 index);
  OHCIGenTransferData *getGenTransferFromPhysAddr(IOPhysicalAddress physAddr);
};

//
// Represents the Wii OHCI USB controller.
//
class WiiOHCI : public IOUSBController {
  OSDeclareDefaultStructors(WiiOHCI);
  WiiDeclareLogFunctions("ohci");
  typedef IOUSBController super;

private:
  IOMemoryMap             *_memoryMap;
  volatile void           *_baseAddr;
  IORangeAllocator        *_mem2Allocator;
  IOInterruptEventSource  *_interruptEventSource;
  IONaturalMemoryCursor   *_memoryCursor;

  // _invalidate_dcache pointer. This function is not exported on 10.4
  WiiInvalidateDataCacheFunc  _invalidateCacheFunc;

  //
  // Endpoints.
  //
  WiiOHCIEndpointBuffer *_endpointBufferHeadPtr;

  // Free endpoints.
  OHCIEndpointData      *_freeEndpointHeadPtr;

  // Control endpoints.
  OHCIEndpointData      *_controlEndpointHeadPtr;
  OHCIEndpointData      *_controlEndpointTailPtr;

  // Bulk endpoints.
  OHCIEndpointData      *_bulkEndpointHeadPtr;
  OHCIEndpointData      *_bulkEndpointTailPtr;

  // Interrupt endpoints.
  OHCIIntEndpoint       _interruptEndpoints[kWiiOHCIInterruptNodeCount];

  //
  // Transfers.
  //
  WiiOHCIGenTransferBuffer  *_genTransferBufferHeadPtr;
  OHCIGenTransferData       *_freeGenTransferHeadPtr;
  OHCIGenTransferData       *_freeGenTransferMem2HeadPtr;

  // HCCA.
  IOMemoryDescriptor          *_hccaDesc;
  IOMemoryMap                 *_hccaMap;
  IOPhysicalAddress           _hccaPhysAddr;
  OHCIHostControllerCommArea  *_hccaPtr;

  inline UInt32 readReg32(UInt32 offset) {
    return OSReadBigInt32(_baseAddr, offset);
  }
  inline void writeReg32(UInt32 offset, UInt32 data) {
    OSWriteBigInt32(_baseAddr, offset, data);
  }
  inline UInt32 readRootHubPort32(UInt16 port) {
    return readReg32(kOHCIRegRhPortStatusBase + ((port - 1) * sizeof (UInt32)));
  }
  inline void writeRootHubPort32(UInt16 port, UInt32 data) {
    return writeReg32(kOHCIRegRhPortStatusBase + ((port - 1) * sizeof (UInt32)), data);
  }

  UInt16      _rootHubAddress;

  void handleInterrupt(IOInterruptEventSource *intEventSource, int count);


  //
  // Root hub simulation.
  //
  struct WiiOHCIRootHubIntTransaction {
    IOMemoryDescriptor  *buffer;
    UInt32              bufferLength;
    IOUSBCompletion     completion;
  } _rootHubInterruptTransactions[4];
  IOLock  *_rootHubInterruptTransLock;

  IOReturn simulateRootHubControlEDCreate(UInt8 endpointNumber, UInt16 maxPacketSize, UInt8 speed);
  IOReturn simulateRootHubInterruptEDCreate(short endpointNumber, UInt8 direction, short speed, UInt16 maxPacketSize);
  void simulateRootHubInterruptTransfer(short endpointNumber, IOUSBCompletion completion,
                                        IOMemoryDescriptor *CBP, bool bufferRounding, UInt32 bufferSize, short direction);
  void completeRootHubInterruptTransfer(bool abort);

  //
  // Descriptor functions.
  //
  IOReturn convertTDStatus(UInt8 ohciStatus);
  OHCIGenTransferData *getGenTransferFromPhys(IOPhysicalAddress physAddr);
  UInt32 getGenTransferBufferRemaining(OHCIGenTransferData *genTransfer);
  IOReturn allocateFreeEndpoints(void);
  IOReturn allocateFreeGenTransfers(bool mem2);
  OHCIEndpointData *getFreeEndpoint(bool isochronous = false);
  OHCIGenTransferData *getFreeGenTransfer(OHCIEndpointData *endpoint, bool mem2);
  void returnEndpoint(OHCIEndpointData *endpoint);
  void returnGenTransfer(OHCIGenTransferData *genTransfer);

  IOReturn initControlEndpoints(void);
  IOReturn initBulkEndpoints(void);
  IOReturn initInterruptEndpoints(void);
  OHCIEndpointData *getEndpoint(UInt8 functionNumber, UInt8 endpointNumber, UInt8 direction,
                                UInt8 *type, OHCIEndpointData **outPrevEndpoint = NULL);
  OHCIEndpointData *getInterruptEndpointHead(UInt8 pollingRate);
  IOReturn addNewEndpoint(UInt8 functionNumber, UInt8 endpointNumber, UInt16 maxPacketSize,
                          UInt8 speed, UInt8 direction, OHCIEndpointData *endpointHeadPtr,
                          bool isochronous = false);
  IOReturn removeEndpoint(UInt8 functionNumber, UInt8 endpointNumber,
                          OHCIEndpointData *endpointHeadPtr, OHCIEndpointData *endpointTailPtr);
  void removeEndpointTransfers(OHCIEndpointData *endpoint);
  void completeFailedEndpointGenTransfers(OHCIEndpointData *endpoint, IOReturn tdStatus, UInt32 bufferSizeRemaining);

  //
  // Transfers.
  //
  IOReturn doGeneralTransfer(OHCIEndpointData *endpoint, IOUSBCompletion completion,
                             IOMemoryDescriptor *buffer, UInt32 bufferSize, UInt32 flags, UInt32 cmdBits);
  void completeTransferQueue(UInt32 headPhysAddr);

protected:
  //
  // Overrides.
  //
  IOReturn UIMInitialize(IOService *provider);
  IOReturn UIMFinalize(void);
  IOReturn UIMCreateControlEndpoint(UInt8 functionNumber, UInt8 endpointNumber, UInt16 maxPacketSize, UInt8 speed);
  IOReturn UIMCreateControlTransfer(short functionNumber, short endpointNumber, IOUSBCompletion completion, void *CBP,
                                    bool bufferRounding, UInt32 bufferSize, short direction);
  IOReturn UIMCreateControlTransfer(short functionNumber, short endpointNumber, IOUSBCompletion completion,
                                    IOMemoryDescriptor *CBP, bool bufferRounding, UInt32 bufferSize, short direction);
  IOReturn UIMCreateBulkEndpoint(UInt8 functionNumber, UInt8 endpointNumber, UInt8 direction, UInt8 speed, UInt8 maxPacketSize);
  IOReturn UIMCreateBulkTransfer(short functionNumber, short endpointNumber, IOUSBCompletion completion,
                                 IOMemoryDescriptor *CBP, bool bufferRounding, UInt32 bufferSize, short direction);
  IOReturn UIMCreateInterruptEndpoint(short functionAddress, short endpointNumber, UInt8 direction,
                                      short speed, UInt16 maxPacketSize, short pollingRate);
  IOReturn UIMCreateInterruptTransfer(short functionNumber, short endpointNumber, IOUSBCompletion completion,
                                      IOMemoryDescriptor *CBP, bool bufferRounding, UInt32 bufferSize, short direction);
  IOReturn UIMCreateIsochEndpoint(short functionAddress, short endpointNumber, UInt32 maxPacketSize, UInt8 direction);
  IOReturn UIMCreateIsochTransfer(short functionAddress, short endpointNumber, IOUSBIsocCompletion completion, UInt8 direction,
                                  UInt64 frameStart, IOMemoryDescriptor *pBuffer, UInt32 frameCount, IOUSBIsocFrame *pFrames);
  IOReturn UIMAbortEndpoint(short functionNumber, short endpointNumber, short direction);
  IOReturn UIMDeleteEndpoint(short functionNumber, short endpointNumber, short direction);
  IOReturn UIMClearEndpointStall(short functionNumber, short endpointNumber, short direction);
  void UIMRootHubStatusChange(void);

  void UIMRootHubStatusChange(bool abort);

public:
  //
  // Overrides.
  //
  bool init(OSDictionary *dictionary = 0);

  IOReturn GetRootHubDeviceDescriptor(IOUSBDeviceDescriptor *desc);
  IOReturn GetRootHubDescriptor(IOUSBHubDescriptor *desc);
  IOReturn SetRootHubDescriptor(OSData *buffer);
  IOReturn GetRootHubConfDescriptor(OSData *desc);
  IOReturn GetRootHubStatus(IOUSBHubStatus *status);
  IOReturn SetRootHubFeature(UInt16 wValue);
  IOReturn ClearRootHubFeature(UInt16 wValue);
  IOReturn GetRootHubPortStatus(IOUSBHubPortStatus *status, UInt16 port);
  IOReturn SetRootHubPortFeature(UInt16 wValue, UInt16 port);
  IOReturn ClearRootHubPortFeature(UInt16 wValue, UInt16 port);
  IOReturn GetRootHubPortState(UInt8 *state, UInt16 port);
  IOReturn SetHubAddress(UInt16 wValue);
  UInt32 GetBandwidthAvailable(void);
  UInt64 GetFrameNumber(void);
  UInt32 GetFrameNumber32(void);
  void PollInterrupts(IOUSBCompletionAction safeAction = 0);

  IOReturn GetRootHubStringDescriptor(UInt8 index, OSData *desc);
};

#endif

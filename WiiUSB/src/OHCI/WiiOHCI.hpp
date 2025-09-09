//
//  WiiOHCI.hpp
//  Wii OHCI USB controller interface
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiOHCI_hpp
#define WiiOHCI_hpp

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/IORangeAllocator.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/usb/IOUSBController.h>

#include "WiiCommon.hpp"
#include "OHCIRegs.hpp"

// On Wii, located in MEM2. On Wii U, located anywhere.
#define kWiiOHCIBounceBufferSize              0x100
#define kWiiOHCIBounceBufferInitialCount      128
// Located in any memory.
#define kWiiOHCIBounceBufferJumboSize         0x800
#define kWiiOHCIBounceBufferJumboInitialCount 64
// Refresh rate for isochronous transfer buffers.
#define kWiiOHCIIsoTimerRefreshUS             500

//
// Total interrupt nodes in tree.
// 32 32ms nodes, 16 16ms nodes, 8 8ms nodes, 4 4ms nodes, 2 2ms nodes, 1 1ms node
//
#define kWiiOHCIInterruptNodeCount    (32 + 16 + 8 + 4 + 2 + 1)
#define kWiiOHCIInterruptIsoNode      (kWiiOHCIInterruptNodeCount - 1)

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
// OHCI transfer memory buffer.
//
class WiiOHCITransferBuffer : public OSObject {
  OSDeclareDefaultStructors(WiiOHCITransferBuffer);
  typedef OSObject super;

private:
  IOBufferMemoryDescriptor  *_buffer;
  IOPhysicalAddress         _physicalAddr;
  bool                      _isochronous;
  union {
    OHCIGenTransferDescriptor *_genTDs;
    OHCIIsoTransferDescriptor *_isoTDs;
  };
  OHCITransferData          *_transfers;
  WiiOHCITransferBuffer     *_nextBuffer;

public:
  //
  // Overrides.
  //
  void free();

  //
  // Buffer functions.
  //
  static WiiOHCITransferBuffer *transferBuffer(bool isochronous);
  void setNextBuffer(WiiOHCITransferBuffer *buffer);
  WiiOHCITransferBuffer *getNextBuffer(void);
  IOPhysicalAddress getPhysAddr(void);
  OHCITransferData *getTransfer(UInt32 index);
  OHCITransferData *getTransferFromPhysAddr(IOPhysicalAddress physAddr);
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
  IONaturalMemoryCursor   *_memoryCursor;

  //
  // Interrupts.
  //
  IOFilterInterruptEventSource  *_interruptEventSource;
  IOWorkLoop                    *_isoTimerWorkLoop;
  IOTimerEventSource            *_isoTimerEventSource;
  IOSimpleLock                  *_writeDoneHeadLock;
  volatile IOPhysicalAddress    _writeDoneHeadPhysAddr;
  volatile UInt32               _writeDoneHeadProducerCount;
  volatile UInt32               _writeDoneHeadConsumerCount;
  volatile bool                 _writeDoneHeadChanged;
  volatile bool                 _rootHubStatusChanged;

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

  // Isochronous endpoints.
  OHCIEndpointData      *_isoEndpointHeadPtr;
  OHCIEndpointData      *_isoEndpointTailPtr;
  UInt32						    _isoBandwidthAvailable;

  // Interrupt endpoints.
  OHCIIntEndpoint       _interruptEndpoints[kWiiOHCIInterruptNodeCount];

  //
  // Transfers.
  //
  // Bounce buffers.
  OHCIBounceBuffer          *_freeBounceBufferHeadPtr;
  OHCIBounceBuffer          *_freeBounceBufferJumboHeadPtr;

  // Transfer buffers.
  WiiOHCITransferBuffer     *_transferBufferHeadPtr;
  OHCITransferData          *_freeGenTransferHeadPtr;
  OHCITransferData          *_freeIsoTransferHeadPtr;

  // HCCA.
  IOMemoryDescriptor          *_hccaDesc;
  IOMemoryMap                 *_hccaMap;
  IOPhysicalAddress           _hccaPhysAddr;
  OHCIHostControllerCommArea  *_hccaPtr;
  volatile UInt64				      _frameNumber;

  // Root hub.
  UInt16  _rootHubAddress;
  struct WiiOHCIRootHubIntTransaction {
    IOMemoryDescriptor  *buffer;
    UInt32              bufferLength;
    IOUSBCompletion     completion;
  } _rootHubInterruptTransactions[4];
  IOLock  *_rootHubInterruptTransLock;

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

  //
  // Interrupt functions.
  //
  bool filterInterrupt(IOFilterInterruptEventSource *filterIntEventSource);
  void handleInterrupt(IOInterruptEventSource *intEventSource, int count);
  void handleIsoTimer(IOTimerEventSource *sender);

  IOReturn simulateRootHubControlEDCreate(UInt8 endpointNumber, UInt16 maxPacketSize, UInt8 speed);
  IOReturn simulateRootHubInterruptEDCreate(short endpointNumber, UInt8 direction, short speed, UInt16 maxPacketSize);
  void simulateRootHubInterruptTransfer(short endpointNumber, IOUSBCompletion completion,
                                        IOMemoryDescriptor *CBP, bool bufferRounding, UInt32 bufferSize, short direction);
  void completeRootHubInterruptTransfer(bool abort);

  //
  // Buffer functions.
  //
  OHCIBounceBuffer *allocateBounceBuffer(bool jumbo);
  OHCIBounceBuffer *getFreeBounceBuffer(bool jumbo);
  void returnBounceBuffer(OHCIBounceBuffer *bounceBuffer);

  //
  // Descriptor functions.
  //
  IOReturn convertTDStatus(UInt8 ohciStatus);
  OHCITransferData *getTransferFromPhys(IOPhysicalAddress physAddr);
  UInt32 getGenTransferBufferRemaining(OHCITransferData *genTransfer);
  IOReturn allocateFreeEndpoints(void);
  IOReturn allocateFreeTransfers(bool isochronous);
  OHCIEndpointData *getFreeEndpoint(bool isochronous = false);
  OHCITransferData *getFreeTransfer(OHCIEndpointData *endpoint);
  void returnEndpoint(OHCIEndpointData *endpoint);
  void returnTransfer(OHCITransferData *transfer);

  IOReturn initControlEndpoints(void);
  IOReturn initBulkEndpoints(void);
  IOReturn initIsoEndpoints(void);
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
  IOReturn prepareIsochTransfer(OHCITransferData *transfer, IOMemoryDescriptor *buffer, UInt32 offset, UInt32 transferSize,
                                UInt16 numPackets, UInt32 flags);
  IOReturn doIsochTransfer(short functionAddress, short endpointNumber, IOUSBIsocCompletion completion, UInt8 direction,
                           UInt64 frameStart, IOMemoryDescriptor *pBuffer, UInt32 frameCount, IOUSBIsocFrame *pFrames,
                           UInt32 updateFrequency, bool isLowLatency);
  void completeGeneralTransfer(OHCITransferData *transfer);
  void completeIsochTransfer(OHCITransferData *transfer, IOReturn status);
  void completeTransferQueue(IOPhysicalAddress headPhysAddr, UInt32 producerCount);

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

  //
  // Version-specific overrides.
  //
  IOReturn UIMCreateControlTransfer(short functionNumber, short endpointNumber, IOUSBCommand* command,
                                    void *CBP, bool bufferRounding, UInt32 bufferSize, short direction);
  IOReturn UIMCreateControlTransfer(short functionNumber, short endpointNumber, IOUSBCommand* command,
                                    IOMemoryDescriptor *CBP, bool bufferRounding, UInt32 bufferSize, short direction);
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_3
  IOReturn UIMCreateIsochTransfer(short functionAddress, short endpointNumber, IOUSBIsocCompletion completion,
                                  UInt8 direction, UInt64 frameStart, IOMemoryDescriptor *pBuffer, UInt32 frameCount,
                                  IOUSBLowLatencyIsocFrame *pFrames, UInt32 updateFrequency);
#endif

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

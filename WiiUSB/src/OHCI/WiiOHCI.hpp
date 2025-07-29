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

//
// Initial descriptors.
//
#define kWiiOHCIInitialNumEDs       128
#define kWiiOHCIInitialNumTDs       256

#define kWiiOHCITempBufferSize      0x200

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

  //
  // Descriptors.
  //
  IOBufferMemoryDescriptor  *_initialEDBuffer;
  IOBufferMemoryDescriptor  *_initialTDBuffer;
  OHCIPhysicalMapping       *_physMappingHeadPtr;

  // Free endpoints.
  OHCIEndpointDescriptor    *_freeEDHeadPtr;
  OHCITransferDescriptor    *_freeTDHeadPtr;

  // Control endpoints.
  OHCIEndpointDescriptor    *_edControlHeadPtr;
  OHCIEndpointDescriptor    *_edControlTailPtr;

  // Bulk endpoints.
  OHCIEndpointDescriptor    *_edBulkHeadPtr;
  OHCIEndpointDescriptor    *_edBulkTailPtr;

  // Interrupt endpoints.
  OHCIIntEndpoint           _edInterrupts[kWiiOHCIInterruptNodeCount];

  // HCCA.
  IOPhysicalAddress           _hccaPhysAddr;
  IOMemoryDescriptor          *_hccaBuffer;
  IOMemoryMap                 *_hccaMap;
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

  inline void flushEndpointDescriptor(OHCIEndpointDescriptor *endpointDesc) {
    flushDataCache(&endpointDesc->ep, sizeof (endpointDesc->ep));
  }
  inline void invalidateEndpointDescriptor(OHCIEndpointDescriptor *endpointDesc) {
    invalidateDataCache(&endpointDesc->ep, sizeof (endpointDesc->ep));
  }
  inline void flushTransferDescriptor(OHCITransferDescriptor *transferDesc) {
    flushDataCache(&transferDesc->td, sizeof (transferDesc->td));
  }
  inline void invalidateTransferDescriptor(OHCITransferDescriptor *transferDesc) {
    invalidateDataCache(&transferDesc->td, sizeof (transferDesc->td));
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
  IOReturn createPhysTDMapping(OHCITransferDescriptor *transferDesc);
  OHCITransferDescriptor *getTDFromPhysMapping(UInt32 physAddr);
  IOReturn allocateDescriptors(void);
  OHCIEndpointDescriptor *getFreeEndpointDescriptor(void);
  OHCITransferDescriptor *getFreeTransferDescriptor(void);
  void returnEndpointDescriptor(OHCIEndpointDescriptor *endpointDesc);
  void returnTransferDescriptor(OHCITransferDescriptor *transferDesc);

  IOReturn initControlEDs(void);
  IOReturn initBulkEDs(void);
  IOReturn initInterruptEDs(void);
  OHCIEndpointDescriptor *getEndpoint(UInt8 functionNumber, UInt8 endpointNumber, UInt8 direction,
                                      UInt8 *type, OHCIEndpointDescriptor **outPrevEndpoint = NULL);

  OHCIEndpointDescriptor *getInterruptEDHead(UInt8 pollingRate);
  IOReturn addNewEndpoint(UInt8 functionNumber, UInt8 endpointNumber, UInt16 maxPacketSize,
                          UInt8 speed, UInt8 direction, OHCIEndpointDescriptor *edHeadPtr,
                          bool isIsoTransfer = false);
  IOReturn removeEndpoint(UInt8 functionNumber, UInt8 endpointNumber,
                          OHCIEndpointDescriptor *edHeadPtr, OHCIEndpointDescriptor *edTailPtr);
  void removeEndpointTransferDescriptors(OHCIEndpointDescriptor *endpointDesc);

  //
  // Transfers.
  //
  IOReturn doGeneralTransfer(OHCIEndpointDescriptor *endpointDesc, UInt8 type, IOUSBCompletion completion,
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

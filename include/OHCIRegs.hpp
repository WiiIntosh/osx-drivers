//
//  OHCIRegs.hpp
//  OHCI USB controller register definitions
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef OHCIRegs_hpp
#define OHCIRegs_hpp

#include <IOKit/usb/USB.h>
#include "WiiCommon.hpp"

#define USB_CONSTANT16(x)	((((x) >> 8) & 0x0FF) | ((x & 0xFF) << 8))

//
// Revision 1.0 for OHCI.
//
#define kOHCIRevision10     0x10

//
// OHCI registers.
// All are 32-bit and normally are little endian, but on Wii they are big endian.
//
// HcRevision.
#define kOHCIRegRevision                        0x00
#define kOHCIRegRevisionMask                    0xFF
// HcControl.
#define kOHCIRegControl                         0x04
#define kOHCIRegControlCBSRMask                 0x3
#define kOHCIRegControlPeriodicListEnable       BIT2
#define kOHCIRegControlIsochronousEnable        BIT3
#define kOHCIRegControlControlListEnable        BIT4
#define kOHCIRegControlBulkListEnable           BIT5
#define kOHCIRegControlFuncStateReset           0
#define kOHCIRegControlFuncStateResume          BIT6
#define kOHCIRegControlFuncStateOperational     BIT7
#define kOHCIRegControlFuncStateSuspend         (BIT6 | BIT7)
#define kOHCIRegControlFuncStateMask            (BIT6 | BIT7)
#define kOHCIRegControlInterruptRouting         BIT8
#define kOHCIRegControlRemoteWakeupConnected    BIT9
#define kOHCIRegControlRemoteWakeupEnable       BIT10
// HcCommandStatus.
#define kOHCIRegCmdStatus                       0x08
#define kOHCIRegCmdStatusHostControllerReset    BIT0
#define kOHCIRegCmdStatusControlListFilled      BIT1
#define kOHCIRegCmdStatusBulkListFilled         BIT2
#define kOHCIRegCmdStatusOwnershipChangeRequest BIT3
// HcInterruptStatus.
#define kOHCIRegIntStatus                       0x0C
#define kOHCIRegIntStatusSchedulingOverrun      BIT0
#define kOHCIRegIntStatusWritebackDoneHead      BIT1
#define kOHCIRegIntStatusStartOfFrame           BIT2
#define kOHCIRegIntStatusResumeDetected         BIT3
#define kOHCIRegIntStatusUnrecoverableError     BIT4
#define kOHCIRegIntStatusFrameNumberOverflow    BIT5
#define kOHCIRegIntStatusRootHubStatusChange    BIT6
#define kOHCIRegIntStatusOwnershipChange        BIT30
// HcInterruptEnable.
#define kOHCIRegIntEnable                       0x10
#define kOHCIRegIntEnableSchedulingOverrun      BIT0
#define kOHCIRegIntEnableWritebackDoneHead      BIT1
#define kOHCIRegIntEnableStartOfFrame           BIT2
#define kOHCIRegIntEnableResumeDetected         BIT3
#define kOHCIRegIntEnableUnrecoverableError     BIT4
#define kOHCIRegIntEnableFrameNumberOverflow    BIT5
#define kOHCIRegIntEnableRootHubStatusChange    BIT6
#define kOHCIRegIntEnableOwnershipChange        BIT30
#define kOHCIRegIntEnableMasterInterruptEnable  BIT31
// HcInterruptDisable.
#define kOHCIRegIntDisable                      0x14
#define kOHCIRegIntDisableSchedulingOverrun     BIT0
#define kOHCIRegIntDisableWritebackDoneHead     BIT1
#define kOHCIRegIntDisableStartOfFrame          BIT2
#define kOHCIRegIntDisableResumeDetected        BIT3
#define kOHCIRegIntDisableUnrecoverableError    BIT4
#define kOHCIRegIntDisableFrameNumberOverflow   BIT5
#define kOHCIRegIntDisableRootHubStatusChange   BIT6
#define kOHCIRegIntDisableOwnershipChange       BIT30
#define kOHCIRegIntDisableMasterInterruptEnable BIT31
// HcHCCA.
#define kOHCIRegHCCA                            0x18
// HcPeriodCurrentED.
#define kOHCIRegPeriodCurrentED                 0x1C
// HcControlHeadED.
#define kOHCIRegControlHeadED                   0x20
// HcControlCurrentED.
#define kOHCIRegControlCurrentED                0x24
// HcBulkHeadED.
#define kOHCIRegBulkHeadED                      0x28
// HcBulkCurrentED.
#define kOHCIRegBulkCurrentED                   0x2C
// HcDoneHead.
#define kOHCIRegDoneHead                        0x30
#define kOHCIRegDoneHeadMask                    BITRange(4, 31)
// HcFmInterval.
#define kOHCIRegFrameInterval                       0x34
#define kOHCIRegFrameIntervalMask                   0x00003FFF
#define kOHCIRegFrameIntervalFSLargestDataPktShift  16
#define kOHCIRegFrameIntervalFSLargestDataPktMask   0x7FFF0000
#define kOHCIRegFrameIntervalToggle                 BIT31
// HcFmRemaining.
#define kOHCIRegFrameRemaining                  0x38
#define kOHCIRegFrameRemainingMask              0x00003FFF
#define kOHCIRegFrameRemainingToggle            BIT31
// HcFmNumber.
#define kOHCIRegFmNumber                        0x3C
#define kOHCIRegFmNumberMask                    0x0000FFFF
// HcPeriodicStart.
#define kOHCIRegPeriodicStart                   0x40
#define kOHCIRegPeriodicStartMask               0x00003FFF
// HcLSThreshold.
#define kOHCIRegLSThreshold                     0x44
#define kOHCIRegLSThresholdMask                 0x00000FFF
// HcRhDescriptorA.
#define kOHCIRegRhDescriptorA                             0x48
#define kOHCIRegRhDescriptorANumPortsMask                 0x0000000F
#define kOHCIRegRhDescriptorANoPowerSwitching             BIT8
#define kOHCIRegRhDescriptorAPowerModePerPort             BIT9
#define kOHCIRegRhDescriptorADeviceType                   BIT10
#define kOHCIRegRhDescriptorAOverCurrentPerPort           BIT11
#define kOHCIRegRhDescriptorANoOverCurrent                BIT12
#define kOHCIRegRhDescriptorAPowerOnToPowerGoodTimeShift  24
#define kOHCIRegRhDescriptorAPowerOnToPowerGoodTimeMask   0xFF000000
// HcRhDescriptorB.
#define kOHCIRegRhDescriptorB                             0x4C
#define kOHCIRegRhDescriptorBDeviceRemovableMask          0x0000FFFF
#define kOHCIRegRhDescriptorBPortPowerControlShift        16
#define kOHCIRegRhDescriptorBPortPowerControlMask         0xFFFF0000
// HcRhStatus.
#define kOHCIRegRhStatus                                  0x50
#define kOHCIRegRhStatusLocalPowerStatus                  BIT0
#define kOHCIRegRhStatusClearGlobalPower                  BIT0
#define kOHCIRegRhStatusOverCurrentIndicator              BIT1
#define kOHCIRegRhStatusDeviceRemoteWakupEnable           BIT15
#define kOHCIRegRhStatusSetRemoteWakupEnable              BIT15
#define kOHCIRegRhStatusLocalPowerStatusChange            BIT16
#define kOHCIRegRhStatusSetGlobalPower                    BIT16
#define kOHCIRegRhStatusOverCurrentIndicatorChange        BIT17
#define kOHCIRegRhStatusClearRemoteWakeupEnable           BIT31
// HcRhPortStatusBase[port 1 - X].
#define kOHCIRegRhPortStatusBase                            0x54
#define kOHCIRegRhPortStatusCurrentConnectStatus            BIT0
#define kOHCIRegRhPortStatusClearPortEnable                 BIT0
#define kOHCIRegRhPortStatusPortEnableStatus                BIT1
#define kOHCIRegRhPortStatusSetPortEnable                   BIT1
#define kOHCIRegRhPortStatusPortSuspendStatus               BIT2
#define kOHCIRegRhPortStatusSetPortSuspend                  BIT2
#define kOHCIRegRhPortStatusPortOverCurrentIndicator        BIT3
#define kOHCIRegRhPortStatusClearPortSuspend                BIT3
#define kOHCIRegRhPortStatusPortResetStatus                 BIT4
#define kOHCIRegRhPortStatusSetPortReset                    BIT4
#define kOHCIRegRhPortStatusPortPowerStatus                 BIT8
#define kOHCIRegRhPortStatusSetPortPower                    BIT8
#define kOHCIRegRhPortStatusLowSpeedDeviceAttached          BIT9
#define kOHCIRegRhPortStatusClearPortPower                  BIT9
#define kOHCIRegRhPortStatusConnectStatusChange             BIT16
#define kOHCIRegRhPortStatusPortEnableStatusChange          BIT17
#define kOHCIRegRhPortStatusPortSuspendStatusChange         BIT18
#define kOHCIRegRhPortStatusPortOverCurrentIndicatorChange  BIT19
#define kOHCIRegRhPortStatusPortResetStatusChange           BIT20

//
// OHCI Host Controller Communications Area.
//
// All OHCI controller fields must be little endian.
// This structure must be 256-byte aligned.
//
#define kOHCINumInterruptHeads   32
typedef struct {
  // Physical addresses of interrupt endpoint descriptors.
  volatile UInt32   interruptTablePhysAddr[kOHCINumInterruptHeads];
  // Current frame number.
  volatile UInt16   frameNumber;
  // Padding.
  volatile UInt16   padding;
  // Physical address of done queue head.
  volatile UInt32   doneHeadPhysAddr;
  // Reserved.
  volatile UInt8    reserved[120];
} OHCIHostControllerCommArea;

OSCompileAssert(sizeof (OHCIHostControllerCommArea) == 256);

struct OHCITransferData;

//
// OHCI endpoint descriptor.
//
// This structure must be 16-byte aligned.
//
#define kOHCIEndpointDescriptorAlignment    0x10
typedef struct {
  // Endpoint descriptor flags.
  volatile UInt32 flags;
  // Physical address of the last transfer descriptor linked to this endpoint.
  volatile UInt32 tailTDPhysAddr;
  // Physical address of the first transfer descriptor linked to this endpoint (and flags).
  volatile UInt32 headTDPhysAddr;
  // Physical address of the next endpoint descriptor if non-zero.
  volatile UInt32 nextEDPhysAddr;
} OHCIEndpointDescriptor;
OSCompileAssert(sizeof (OHCIEndpointDescriptor) == kOHCIEndpointDescriptorAlignment);

//
// OHCI endpoint descriptor driver-only data.
//
typedef struct OHCIEndpointData {
  // OHCI endpoint descriptor used by the host controller.
  OHCIEndpointDescriptor  *ed;
  // Physical address of this endpoint descriptor.
  UInt32                  physAddr;
  // Pointer to the last transfer descriptor linked to this endpoint.
  struct OHCITransferData *tailTransfer;
  // Pointer to the first transfer descriptor linked to this endpoint.
  struct OHCITransferData *headTransfer;
  // Pointer to the next endpoint.
  struct OHCIEndpointData *nextEndpoint;
} OHCIEndpointData;

//
// OHCI interrupt endpoint table structure.
//
typedef struct {
  OHCIEndpointData  *headEndpoint;
  OHCIEndpointData  *tailEndpoint;
} OHCIIntEndpoint;

//
// OHCI endpoint descriptor flags.
//
#define kOHCIEDFlagsFuncMask        BITRange(0, 6)
#define kOHCIEDFlagsEndpointShift   7
#define kOHCIEDFlagsEndpointMask    BITRange(7, 10)
#define kOHCIEDFlagsDirectionMask   BITRange(11, 12)
#define kOHCIEDFlagsDirectionTD     0
#define kOHCIEDFlagsDirectionOut    BIT11
#define kOHCIEDFlagsDirectionIn     BIT12
#define kOHCIEDFlagsLowSpeed        BIT13
#define kOHCIEDFlagsSkip            BIT14
#define kOHCIEDFlagsIsochronous     BIT15
#define kOHCIEDFlagsMaxPktSizeShift 16
#define kOHCIEDFlagsMaxPktSizeMask  BITRange(16, 26)
#define kOHCIEDTDHeadHalted         BIT0
#define kOHCIEDTDHeadCarry          BIT1
#define kOHCIEDTDHeadMask           BITRange(4, 31)

//
// OHCI transfer descriptor conditon codes.
//
#define kOHCITDConditionCodeNoError               0
#define kOHCITDConditionCodeCRC                   1
#define kOHCITDConditionCodeBitStuffing           2
#define kOHCITDConditionCodeDataToggleMismatch    3
#define kOHCITDConditionCodeStall                 4
#define kOHCITDConditionCodeDeviceNotResponding   5
#define kOHCITDConditionCodePIDCheckFailure       6
#define kOHCITDConditionCodeUnexpectedPID         7
#define kOHCITDConditionCodeDataOverrun           8
#define kOHCITDConditionCodeDataUnderrun          9
#define kOHCITDConditionCodeBufferOverrun         12
#define kOHCITDConditionCodeBufferUnderrun        13
#define kOHCITDConditionCodeNotAccessed           15

//
// OHCI general transfer descriptor flags.
//
#define kOHCIGenTDFlagsBufferRounding         BIT18
#define kOHCIGenTDFlagsDirectionMask          BITRange(19, 20)
#define kOHCIGenTDFlagsDirectionSetup         0
#define kOHCIGenTDFlagsDirectionOut           BIT19
#define kOHCIGenTDFlagsDirectionIn            BIT20
#define kOHCIGenTDFlagsDelayInterruptShift    21
#define kOHCIGenTDFlagsDelayInterruptMask     BITRange(21, 23)
#define kOHCIGenTDFlagsDelayInterruptNone     kOHCIGenTDFlagsDelayInterruptMask
#define kOHCIGenTDFlagsDataToggleData0        BIT24
#define kOHCIGenTDFlagsDataToggleData1        BIT25
#define kOHCIGenTDFlagsErrorCountShift        26
#define kOHCIGenTDFlagsErrorCountMask         BITRange(26, 27)
#define kOHCIGenTDFlagsConditionCodeShift     28
#define kOHCIGenTDFlagsConditionCodeMask      BITRange(28, 31)

//
// OHCI transfer descriptor.
//
// This structure must be 32-byte aligned.
//
#define kOHCITransferDescriptorAlignment    0x20
typedef struct OHCITransferDescriptor {
  // Transfer descriptor flags.
  volatile UInt32 flags;
  union {
    // Current buffer pointer physical address (general transfers).
    volatile UInt32 currentBufferPtrPhysAddr;
    // Buffer page 0 (isochronous transfers).
    volatile UInt32 bufferPage0;
  };
  // Physical address of next transfer descriptor.
  volatile UInt32 nextTDPhysAddr;
  // Physical address of the last byte in this transfer descriptor's buffer.
  volatile UInt32 bufferEndPhysAddr;
  union {
    // Additional controller data for isochronous transfers.
    struct {
      // Packet status words (isochronous transfers).
      volatile UInt16 packetStatus[8];
    };
  };
} OHCITransferDescriptor;
OSCompileAssert(sizeof (OHCITransferDescriptor) == kOHCITransferDescriptorAlignment);

//
// OHCI transfer driver-only data.
//
typedef struct OHCITransferData {
  // OHCI transfer descriptor used by the host controller.
  OHCITransferDescriptor  *td;
  // Physical address of this transfer descriptor.
  UInt32                  physAddr;
  // Pointer to next linked transfer.
  struct OHCITransferData *nextTransfer;
  // Type of transfer.
  UInt8                   type;
  // Is transfer the last for a transaction.
  bool                    last;
  // Is temporary buffer in MEM2?
  bool                    mem2;

  // Temporary buffer descriptor for this transfer.
  IOMemoryDescriptor        *tmpBuffer;
  // Temporary buffer physical address.
  IOPhysicalAddress         tmpBufferPhysAddr;
  // Temporary buffer pointer mapped into kernel memory.
  void                      *tmpBufferPtr;
  // Used temporary buffer size.
  UInt32                    actualBufferSize;
  // Original buffer descriptor.
  IOMemoryDescriptor        *srcBuffer;

  // Completion callback.
  union {
    IOUSBCompletion     gen;
    IOUSBIsocCompletion iso;
  } completion;
  // Isochronous frame.
  IOUSBIsocFrame      *isoFrame;
  // Isochronous frame number.
  UInt32              isoFrameNum;
} OHCITransferData;

#define kOHCITransferTypeControl      0
#define kOHCITransferTypeInterrupt    1
#define kOHCITransferTypeBulk         2
#define kOHCITransferTypeIsochronous  3

//
// Transfer descriptor to physical address mapping pair.
//
typedef struct OHCIPhysicalMapping {
  UInt32                      physAddr;
  OHCITransferData            *transferDesc;
  struct OHCIPhysicalMapping  *nextMapping;
} OHCIPhysicalMapping;

#endif

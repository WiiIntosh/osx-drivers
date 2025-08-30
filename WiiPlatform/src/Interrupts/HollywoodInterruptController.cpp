//
//  HollywoodInterruptController.cpp
//  Wii Hollywood platform interrupt controller
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <ppc/proc_reg.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>

#include "HollywoodInterruptController.hpp"

OSDefineMetaClassAndStructors(HollywoodInterruptController, super);

//
// Overrides IOInterruptController::init()
//
bool HollywoodInterruptController::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _memoryMap  = NULL;
  _baseAddr   = NULL;

  return super::init(dictionary);
}

//
// Overrides IOInterruptController::start()
//
bool HollywoodInterruptController::start(IOService *provider) {
  OSSymbol  *interruptControllerName;
  bool      vectorLockResult;

  if (!super::start(provider)) {
    WIISYSLOG("super::start() returned false");
    return false;
  }

  //
  // Get the interrupt controller name.
  //
  interruptControllerName = (OSSymbol *)IODTInterruptControllerName(provider);
  if (interruptControllerName == NULL) {
    WIISYSLOG("Failed to get interrupt controller name");
    return false;
  }

  //
  // Map interrupt controller memory.
  //
  _memoryMap = provider->mapDeviceMemoryWithIndex(0);
  if (_memoryMap == NULL) {
    WIISYSLOG("Failed to map interrupt controller memory");
    return false;
  }
  _baseAddr = (volatile void *)_memoryMap->getVirtualAddress();
  WIIDBGLOG("Mapped registers to %p (physical 0x%X), length: 0x%X", _baseAddr,
    _memoryMap->getPhysicalAddress(), _memoryMap->getLength());

  //
  // Mask all vectors for both Broadway and Starlet. TODO: is this correct?
  //
  WIIDBGLOG("Current Broadway IRQs: 0x%X, mask 0x%X", readReg32(kWiiHollywoodICBroadwayIRQCause), readReg32(kWiiHollywoodICBroadwayIRQMask));
  WIIDBGLOG("Current Starlet IRQs:  0x%X, mask 0x%X", readReg32(kWiiHollywoodICStarletIRQCause),readReg32(kWiiHollywoodICStarletIRQMask));
  writeReg32(kWiiHollywoodICBroadwayIRQMask, 0);
  writeReg32(kWiiHollywoodICStarletIRQMask, 0);
  writeReg32(kWiiHollywoodICBroadwayIRQCause, 0xFFFFFFFF);
  writeReg32(kWiiHollywoodICStarletIRQCause, 0xFFFFFFFF);
  eieio();

  WIIDBGLOG("New Broadway IRQs: 0x%X, mask 0x%X", readReg32(kWiiHollywoodICBroadwayIRQCause), readReg32(kWiiHollywoodICBroadwayIRQMask));
  WIIDBGLOG("New Starlet IRQs:  0x%X, mask 0x%X", readReg32(kWiiHollywoodICStarletIRQCause),readReg32(kWiiHollywoodICStarletIRQMask));

  //
  // Allocate vectors.
  //
  vectors = (IOInterruptVector *)IOMalloc(kWiiHollywoodICVectorCount * sizeof (IOInterruptVector));
  if (vectors == NULL) {
    WIISYSLOG("Failed to allocate vectors");
    return false;
  }
  bzero(vectors, kWiiHollywoodICVectorCount * sizeof (IOInterruptVector));

  //
  // Allocate vector locks.
  //
  vectorLockResult = true;
  for (int i = 0; i < kWiiHollywoodICVectorCount; i++) {
    vectors[i].interruptLock = IOLockAlloc();
    if (vectors[i].interruptLock == NULL) {
      vectorLockResult = false;
      break;
    }
  }
  if (!vectorLockResult) {
    WIISYSLOG("Failed to allocate vector locks");
    return false;
  }

  registerService();

  //
  // Register this as a secondary platform interrupt controller.
  //
  provider->registerInterrupt(0, this, getInterruptHandlerAddress(), 0);
  provider->enableInterrupt(0);

  getPlatform()->registerInterruptController(interruptControllerName, this);

  WIIDBGLOG("Initialized Hollywood interrupt controller");
  return true;
}

//
// Overrides IOInterruptController::getInterruptHandlerAddress().
//
// Gets the address of the primary interrupt handler for this controller.
//
IOInterruptAction HollywoodInterruptController::getInterruptHandlerAddress(void) {
  return OSMemberFunctionCast(IOInterruptAction, this, &HollywoodInterruptController::handleInterrupt);
}

//
// Overrides IOInterruptController::handleInterrupt().
//
// Handles all incoming interrupts for this controller and forwards to the appropriate vectors.
//
IOReturn HollywoodInterruptController::handleInterrupt(void *refCon, IOService *nub, int source) {
  IOInterruptVector *vector;
  UInt32            cause;
  UInt32            mask;

  //
  // Get interrupt status/mask and ensure no spurious interrupt.
  //
  cause = readReg32(kWiiHollywoodICBroadwayIRQCause);
  mask  = readReg32(kWiiHollywoodICBroadwayIRQMask);
  if ((cause & mask) == 0) {
    return kIOReturnSuccess;
  }
  cause &= mask;

  //
  // Check all vectors.
  //
  for (int vectorIndex = 0; vectorIndex < kWiiHollywoodICVectorCount; vectorIndex++) {
    //
    // Check if this vector is active.
    //
    if ((cause & (1 << vectorIndex)) == 0) {
      continue;
    }

    vector = &vectors[vectorIndex];
    vector->interruptActive = 1;
    sync();
    isync();

    if (!vector->interruptDisabledSoft) {
      isync();

      //
      // Call the handler if it exists.
      //
      if (vector->interruptRegistered) {
        vector->handler(vector->target, vector->refCon, vector->nub, vector->source);
      }

    } else {
      vector->interruptDisabledHard = 1;
      disableVectorHard(vectorIndex, vector);
    }

    vector->interruptActive = 0;
  }

  return kIOReturnSuccess;
}

//
// Overrides IOInterruptController::getVectorType().
//
// Gets the type of vector.
//
int HollywoodInterruptController::getVectorType(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) {
  //
  // All vectors are level and continue to interrupt as long as the respective device register has not been cleared.
  //
  return kIOInterruptTypeLevel;
}

//
// Overrides IOInterruptController::disableVectorHard().
//
// Masks and disables the specified vector.
//
void HollywoodInterruptController::disableVectorHard(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) {
  UInt32 mask = readReg32(kWiiHollywoodICBroadwayIRQMask);
  mask &= ~(1 << vectorNumber);
  writeReg32(kWiiHollywoodICBroadwayIRQMask, mask);
  eieio();
}

//
// Overrides IOInterruptController::enableVector().
//
// Unmasks and enables the specified vector.
// Acknowledge before masking otherwise a false interrupt may occur when IOInterruptEventSource re-enables the vector.
//
void HollywoodInterruptController::enableVector(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) {
  UInt32 mask = readReg32(kWiiHollywoodICBroadwayIRQMask);
  mask |= (1 << vectorNumber);
  writeReg32(kWiiHollywoodICBroadwayIRQCause, 1 << vectorNumber);
  writeReg32(kWiiHollywoodICBroadwayIRQMask, mask);
  eieio();
}

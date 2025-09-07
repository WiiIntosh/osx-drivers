//
//  LatteInterruptController.cpp
//  Wii U Latte chipset interrupt controller
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <ppc/proc_reg.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>

#include "LatteInterruptController.hpp"
#include "LatteRegs.hpp"
#include "WiiProcessorInterface.hpp"

OSDefineMetaClassAndStructors(LatteInterruptController, super);

//
// Overrides IOInterruptController::init().
//
bool LatteInterruptController::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _memoryMap  = NULL;
  _baseAddr   = NULL;

  return super::init(dictionary);
}

//
// Overrides IOInterruptController::start().
//
bool LatteInterruptController::start(IOService *provider) {
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
  // Mask all vectors for all three Espresso cores and Starbuck.
  //
  for (int i = 0; i < kWiiPICafeCoreCount; i++) {
    writeReg32(kWiiLatteIntRegPPCInterruptMask0 + (i * kWiiLatteIntPPCRegistersLength), 0);
    writeReg32(kWiiLatteIntRegPPCInterruptMask1 + (i * kWiiLatteIntPPCRegistersLength), 0);
    writeReg32(kWiiLatteIntRegPPCInterruptCause0 + (i * kWiiLatteIntPPCRegistersLength), 0xFFFFFFFF);
    writeReg32(kWiiLatteIntRegPPCInterruptCause1 + (i * kWiiLatteIntPPCRegistersLength), 0xFFFFFFFF);
  }

  writeReg32(kWiiLatteIntRegARMInterruptMask0, 0);
  writeReg32(kWiiLatteIntRegARMInterruptMask1, 0);
  writeReg32(kWiiLatteIntRegARMInterruptCause0, 0xFFFFFFFF);
  writeReg32(kWiiLatteIntRegARMInterruptCause1, 0xFFFFFFFF);
  eieio();

  //
  // Allocate vectors.
  //
  vectors = (IOInterruptVector *)IOMalloc(kWiiLatteIntVectorCount * sizeof (IOInterruptVector));
  if (vectors == NULL) {
    WIISYSLOG("Failed to allocate vectors");
    return false;
  }
  bzero(vectors, kWiiLatteIntVectorCount * sizeof (IOInterruptVector));

  //
  // Allocate vector locks.
  //
  vectorLockResult = true;
  for (int i = 0; i < kWiiLatteIntVectorCount; i++) {
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

  WIIDBGLOG("Initialized Latte interrupt controller");
  return true;
}

//
// Overrides IOInterruptController::getInterruptHandlerAddress().
//
// Gets the address of the primary interrupt handler for this controller.
//
IOInterruptAction LatteInterruptController::getInterruptHandlerAddress(void) {
  return OSMemberFunctionCast(IOInterruptAction, this, &LatteInterruptController::handleInterrupt);
}

//
// Overrides IOInterruptController::handleInterrupt().
//
// Handles all incoming interrupts for this controller and forwards to the appropriate vectors.
//
IOReturn LatteInterruptController::handleInterrupt(void *refCon, IOService *nub, int source) {
  IOInterruptVector *vector;
  UInt64            cause;
  UInt64            mask;

  //
  // Get interrupt status/mask and ensure no spurious interrupt.
  //
  cause = ((UInt64)readReg32(kWiiLatteIntRegPPCInterruptCause0) | ((UInt64)readReg32(kWiiLatteIntRegPPCInterruptCause1) << kWiiLatteIntVectorPerRegCount));
  mask  = ((UInt64)readReg32(kWiiLatteIntRegPPCInterruptMask0) | ((UInt64)readReg32(kWiiLatteIntRegPPCInterruptMask1) << kWiiLatteIntVectorPerRegCount));
  if ((cause & mask) == 0) {
    return kIOReturnSuccess;
  }
  cause &= mask;

  //
  // Check all vectors.
  //
  for (int vectorIndex = 0; vectorIndex < kWiiLatteIntVectorCount; vectorIndex++) {
    //
    // Check if this vector is active.
    //
    if ((cause & (1ULL << vectorIndex)) == 0) {
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

  //
  // Acknowledge all asserted interrupts on the controller. Any interrupts will be re-asserted if the
  // respective handlers did not clear the underlying hardware interrupts.
  //
  writeReg32(kWiiLatteIntRegPPCInterruptCause0, (UInt32)cause);
  writeReg32(kWiiLatteIntRegPPCInterruptCause1, (UInt32)(cause >> kWiiLatteIntVectorPerRegCount));
  eieio();

  return kIOReturnSuccess;
}

//
// Overrides IOInterruptController::getVectorType().
//
// Gets the type of vector.
//
int LatteInterruptController::getVectorType(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) {
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
void LatteInterruptController::disableVectorHard(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) {
  UInt32 mask;
  if (vectorNumber < kWiiLatteIntVectorPerRegCount) {
    mask = readReg32(kWiiLatteIntRegPPCInterruptMask0);
    mask &= ~(1 << vectorNumber);
    writeReg32(kWiiLatteIntRegPPCInterruptMask0, mask);
  } else {
    mask = readReg32(kWiiLatteIntRegPPCInterruptMask1);
    mask &= ~(1 << (vectorNumber - kWiiLatteIntVectorPerRegCount));
    writeReg32(kWiiLatteIntRegPPCInterruptMask1, mask);
  }
  eieio();
}

//
// Overrides IOInterruptController::enableVector().
//
// Unmasks and enables the specified vector.
// Acknowledge before masking otherwise a false interrupt may occur when IOInterruptEventSource re-enables the vector.
//
void LatteInterruptController::enableVector(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) {
  UInt32 mask;
  if (vectorNumber < kWiiLatteIntVectorPerRegCount) {
    mask = readReg32(kWiiLatteIntRegPPCInterruptMask0);
    writeReg32(kWiiLatteIntRegPPCInterruptCause0, 1 << vectorNumber);
    mask |= (1 << vectorNumber);
    writeReg32(kWiiLatteIntRegPPCInterruptMask0, mask);
  } else {
    mask = readReg32(kWiiLatteIntRegPPCInterruptMask1);
    writeReg32(kWiiLatteIntRegPPCInterruptCause1, 1 << (vectorNumber - 32));
    mask |= (1 << (vectorNumber - kWiiLatteIntVectorPerRegCount));
    writeReg32(kWiiLatteIntRegPPCInterruptMask1, mask);
  }
  eieio();
}

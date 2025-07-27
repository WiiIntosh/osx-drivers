//
//  WiiInterruptController.cpp
//  Wii U platform interrupt controller
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <ppc/proc_reg.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>

#include "WiiInterruptController.hpp"
#include "WiiProcessorInterface.hpp"
#include "WiiPE.hpp"

OSDefineMetaClassAndStructors(WiiInterruptController, super);

//
// Overrides IOInterruptController::init().
//
bool WiiInterruptController::init(OSDictionary *dictionary) {
  WiiCheckDebugArgs();

  _memoryMap  = NULL;
  _baseAddr   = NULL;
  _isCafe     = false;

  return super::init(dictionary);
}

//
// Overrides IOInterruptController::start().
//
bool WiiInterruptController::start(IOService *provider) {
  OSSymbol    *interruptControllerName;
  bool        vectorLockResult;
  WiiPE       *wiiPE;

  if (!super::start(provider)) {
    WIISYSLOG("super::start() returned false");
    return false;
  }

  wiiPE = OSDynamicCast(WiiPE, getPlatform());
  if (wiiPE == NULL) {
    WIISYSLOG("Platform is not a Wii");
    return false;
  }
  _isCafe = wiiPE->isPlatformCafe();

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
  // Mask all vectors if they are not already.
  // This should have been done by the bootloader.
  //
  if (_isCafe) {
    for (int i = 0; i < kWiiPICafeCoreCount; i++) {
      writeCafeIntMask32(i, 0);
      writeCafeIntCause32(i, 0xFFFFFFFF);
    }
  } else {
    writeReg32(kWiiPIRegInterruptMask, 0);
  }
  eieio();

  //
  // Allocate vectors.
  //
  vectors = (IOInterruptVector *)IOMalloc(kWiiPIVectorCount * sizeof (IOInterruptVector));
  if (vectors == NULL) {
    WIISYSLOG("Failed to allocate vectors");
    return false;
  }
  bzero(vectors, kWiiPIVectorCount * sizeof (IOInterruptVector));

  //
  // Allocate vector locks.
  //
  vectorLockResult = true;
  for (int i = 0; i < kWiiPIVectorCount; i++) {
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
  // Register this as the platform interrupt controller.
  //
  getPlatform()->setCPUInterruptProperties(provider);
  provider->registerInterrupt(0, this, getInterruptHandlerAddress(), 0);
  provider->enableInterrupt(0);

  getPlatform()->registerInterruptController(interruptControllerName, this);

  WIIDBGLOG("Initialized Wii interrupt controller");
  return true;
}

//
// Overrides IOInterruptController::getInterruptHandlerAddress().
//
// Gets the address of the primary interrupt handler for this controller.
//
IOInterruptAction WiiInterruptController::getInterruptHandlerAddress(void) {
  return OSMemberFunctionCast(IOInterruptAction, this, &WiiInterruptController::handleInterrupt);
}

//
// Overrides IOInterruptController::handleInterrupt().
//
// Handles all incoming interrupts for this controller and forwards to the appropriate vectors.
//
IOReturn WiiInterruptController::handleInterrupt(void *refCon, IOService *nub, int source) {
  IOInterruptVector *vector;
  UInt32            cause;
  UInt32            mask;

  //
  // Get interrupt status/mask and ensure no spurious interrupt.
  //
  if (_isCafe) {
    cause = readCafeIntCause32(0); // TODO
    mask  = readCafeIntMask32(0);
  } else {
    cause = readReg32(kWiiPIRegInterruptCause);
    mask  = readReg32(kWiiPIRegInterruptMask);
  }
  if ((cause & mask) == 0) {
    return kIOReturnSuccess;
  }
  cause &= mask;

  //
  // Check all vectors.
  //
  for (int vectorIndex = 0; vectorIndex < kWiiPIVectorCount; vectorIndex++) {
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
int WiiInterruptController::getVectorType(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) {
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
void WiiInterruptController::disableVectorHard(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) {
  UInt32 mask;
  
  if (_isCafe) {
    mask = readCafeIntMask32(0);
    mask &= ~(1 << vectorNumber);
    writeCafeIntMask32(0, mask);
  } else {
    mask = readReg32(kWiiPIRegInterruptMask);
    mask &= ~(1 << vectorNumber);
    writeReg32(kWiiPIRegInterruptMask, mask);
  }

  eieio();
}

//
// Overrides IOInterruptController::enableVector().
//
// Unmasks and enables the specified vector.
//
void WiiInterruptController::enableVector(IOInterruptVectorNumber vectorNumber, IOInterruptVector *vector) {
  UInt32 mask;

  if (_isCafe) {
    mask = readCafeIntMask32(0);
    mask |= (1 << vectorNumber);
    writeCafeIntMask32(0, mask);
  } else {
    mask = readReg32(kWiiPIRegInterruptMask);
    mask |= (1 << vectorNumber);
    writeReg32(kWiiPIRegInterruptMask, mask);
  }

  eieio();
}

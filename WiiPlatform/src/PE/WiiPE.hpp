//
//  WiiPE.hpp
//  Wii platform expert
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiPE_hpp
#define WiiPE_hpp

#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IORangeAllocator.h>

#include <mach-o/loader.h>

#include "WiiCommon.hpp"

//
// Represents the platform expert for the Wii system.
//
class WiiPE : public IODTPlatformExpert {
  OSDeclareDefaultStructors(WiiPE);
  WiiDeclareLogFunctions("pe");
  typedef IODTPlatformExpert super;

private:
  IORangeAllocator    *_mem2Allocator;

  //
  // Patching/symbol lookups.
  //
  UInt8     *_symTab;
  UInt32    _symTabNumSymbols;
  UInt8     *_strTab;
  UInt32    _strTabSize;

  bool findKernelMachHeader(void);
  UInt32 resolveKernelSymbol(const char *symbolName);

public:
  //
  // Overrides.
  //
  bool init(OSDictionary *dictionary = 0);
  bool start(IOService *provider);
  IOReturn callPlatformFunction(const OSSymbol *functionName, bool waitForFunction,
                                void *param1, void *param2, void *param3, void *param4);
  const char *deleteList(void);
  const char *excludeList(void);
  bool getMachineName(char *name, int maxLength);
  long getGMTTimeOfDay(void);
};

#endif

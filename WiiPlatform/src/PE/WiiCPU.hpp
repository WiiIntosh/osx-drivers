//
//  WiiCPU.hpp
//  Wii CPU platform device
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiCPU_hpp
#define WiiCPU_hpp

#include <IOKit/IOCPU.h>
#include "WiiCommon.hpp"

//
// Represents a Wii platform CPU.
//
class WiiCPU : public IOCPU {
  OSDeclareDefaultStructors(WiiCPU);
  WiiDeclareLogFunctions("cpu");
  typedef IOCPU super;

private:
  bool                _isBootCPU;
  UInt32              _numCPUs;

  void ipiHandler(void *refCon, void *nub, int source);

public:
  //
  // Overrides.
  //
  bool init(OSDictionary *dictionary = 0);
  bool start(IOService *provider);
  void initCPU(bool boot);
  void quiesceCPU(void);
  kern_return_t startCPU(vm_offset_t start_paddr, vm_offset_t arg_paddr);
  void haltCPU(void);
  const OSSymbol *getCPUName(void);
};

#endif

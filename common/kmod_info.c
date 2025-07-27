//
//  kmod_info.c
//  Kernel extension info structures
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <mach/mach_types.h>

//
// Functions defined in libkern/c++/OSRuntime.cpp
//
extern kern_return_t OSRuntimeInitializeCPP(kmod_info_t *ki, void *data);
extern kern_return_t OSRuntimeFinalizeCPP(kmod_info_t *ki, void *data);

__private_extern__ kmod_start_func_t *_realmain = 0;
__private_extern__ kmod_stop_func_t *_antimain = 0;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;

__asm__(".constructors_used = 0");
__asm__(".private_extern .constructors_used");
__asm__(".destructors_used = 0");
__asm__(".private_extern .destructors_used");

//
// Start function.
//
__private_extern__ kern_return_t _start(kmod_info_t *ki, void *data) {
  kern_return_t res = OSRuntimeInitializeCPP(ki, data);

  if (!res && _realmain)
    res = (*_realmain)(ki, data);

  return res;
}

//
// Stop function.
//
__private_extern__ kern_return_t _stop(kmod_info_t *ki, void *data) {
  kern_return_t res = OSRuntimeFinalizeCPP(ki, data);

  if (!res && _antimain)
    res = (*_antimain)(ki, data);

  return res;
}

//
// Kmod definition.
//
KMOD_EXPLICIT_DECL(__BUNDLE__.__MODULE__, "__VERSION__", _start, _stop)

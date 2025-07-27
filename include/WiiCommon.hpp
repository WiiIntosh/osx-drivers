//
//  WiiCommon.hpp
//  Wii common functions
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#ifndef WiiCommon_hpp
#define WiiCommon_hpp

#include <IOKit/IOLib.h>

#define ARRSIZE(x)    ((sizeof (x) / sizeof ((x)[0])))

#define kHz   1000
#define MHz   (kHz * 1000)
#define kByte 1024

// 1000 uS in one MS
#define kWiiMicrosecondMS     1000

#define BIT0  (1<<0)
#define BIT1  (1<<1)
#define BIT2  (1<<2)
#define BIT3  (1<<3)
#define BIT4  (1<<4)
#define BIT5  (1<<5)
#define BIT6  (1<<6)
#define BIT7  (1<<7)
#define BIT8  (1<<8)
#define BIT9  (1<<9)
#define BIT10 (1<<10)
#define BIT11 (1<<11)
#define BIT12 (1<<12)
#define BIT13 (1<<13)
#define BIT14 (1<<14)
#define BIT15 (1<<15)
#define BIT16 (1<<16)
#define BIT17 (1<<17)
#define BIT18 (1<<18)
#define BIT19 (1<<19)
#define BIT20 (1<<20)
#define BIT21 (1<<21)
#define BIT22 (1<<22)
#define BIT23 (1<<23)
#define BIT24 (1<<24)
#define BIT25 (1<<25)
#define BIT26 (1<<26)
#define BIT27 (1<<27)
#define BIT28 (1<<28)
#define BIT29 (1<<29)
#define BIT30 (1<<30)
#define BIT31 (1<<31)

#define BITRange(start, end) (                \
  ((((UInt32) 0xFFFFFFFF) << (31 - (end))) >> \
  ((31 - (end)) + (start))) <<                \
  (start)                                     \
)

//
// Major kernel version exported from XNU.
//
extern const int version_major;

//
// Kernel verisons.
//
typedef enum {
  kKernelVersionCheetahPumaBase = 1,
  kKernelVersionPumaUpdated     = 5,
  kKernelVersionJaguar          = 6,
  kKernelVersionPanther         = 7,
  kKernelVersionTiger           = 8,
  kKernelVersionLeopard         = 9,
  kKernelVersionSnowLeopard     = 10
} KernelVersion;

//
// Get major kernel version.
//
inline KernelVersion getKernelVersion() {
	return static_cast<KernelVersion>(version_major);
}

inline bool checkKernelArgument(const char *name) {
  int val[16];
  return PE_parse_boot_arg(name, val);
}

//
// Cache functions.
//
// Flushes virtual buffer to physical memory.
//
inline void flushDataCache(volatile void *buffer, UInt32 size) {
  flush_dcache((vm_offset_t) buffer, size, false);
}

//
// Flushes physical buffer to physical memory.
//
inline void flushDataCachePhys(UInt32 physAddr, UInt32 size) {
  flush_dcache(physAddr, size, true);
}

extern void (*WiiInvalidateDataCacheFunc)(vm_offset_t, unsigned, int);

//
// Invalidates cache for virtual buffer.
//
inline void invalidateDataCache(volatile void *buffer, UInt32 size) {
  // Invokes invalidate_dcache. This is not exported in 10.4 so needed to resolve it earlier ourselves.
  (*WiiInvalidateDataCacheFunc)((vm_offset_t) buffer, size, false);
}

//
// Invalidates cache for physical buffer.
//
inline void invalidateDataCachePhys(UInt32 physAddr, UInt32 size) {
  // Invokes invalidate_dcache. This is not exported in 10.4 so needed to resolve it earlier ourselves.
  (*WiiInvalidateDataCacheFunc)(physAddr, size, true);
}

#if DEBUG
//
// Debug logging function.
//
inline void logPrint(const char *className, const char *funcName, const char *format, va_list va) {
  char tmp[256];
  tmp[0] = '\0';
  vsnprintf(tmp, sizeof (tmp), format, va);
  IOLog("%s::%s(): %s\n", className, funcName, tmp);
}

//
// Log functions for I/O Kit modules.
//
#define WiiDeclareLogFunctions(a) \
  protected: \
  bool _debugEnabled; \
  inline void WiiCheckDebugArgs() { \
    _debugEnabled = checkKernelArgument("-wii" a "dbg"); \
  } \
  inline void WIIDBGLOG_PRINT(const char *func, const char *str, ...) const { \
    if (this->_debugEnabled) { \
      va_list args; \
      va_start(args, str); \
      logPrint(this->getMetaClass()->getClassName(), func, str, args); \
      va_end(args); \
    } \
  } \
    \
  inline void WIISYSLOG_PRINT(const char *func, const char *str, ...) const { \
    va_list args; \
    va_start(args, str); \
    logPrint(this->getMetaClass()->getClassName(), func, str, args); \
    va_end(args); \
  } \
  protected:

//
// Common logging macros to inject function name.
//
#define WIIDBGLOG(str, ...)     WIIDBGLOG_PRINT(__FUNCTION__, str, ## __VA_ARGS__)
#define WIIDATADBGLOG(str, ...) WIIDATADBGLOG_PRINT(__FUNCTION__, str, ## __VA_ARGS__)
#define WIISYSLOG(str, ...)     WIISYSLOG_PRINT(__FUNCTION__, str, ## __VA_ARGS__)

#else

//
// Release print function.
//
inline void logPrint(const char *className, const char *format, va_list va) {
  char tmp[256];
  tmp[0] = '\0';
  vsnprintf(tmp, sizeof (tmp), format, va);
  IOLog("%s: %s\n", className, tmp);
}

//
// Log functions for I/O Kit modules.
//
#define WiiDeclareLogFunctions(a) \
  protected: \
  bool _debugEnabled; \
  inline void WiiCheckDebugArgs() { _debugEnabled = false; } \
  inline void WIIDBGLOG(const char *str, ...) const { } \
    \
  inline void WIISYSLOG(const char *str, ...) const { \
    va_list args; \
    va_start(args, str); \
    logPrint(this->getMetaClass()->getClassName(), str, args); \
    va_end(args); \
  } \
  protected:
#endif

#endif

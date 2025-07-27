//
//  WiiPE_Patcher.cpp
//  Wii platform expert patching functions
//
//  Copyright © 2025 John Davis. All rights reserved.
//

#include <mach-o/nlist.h>
#include <ppc/proc_reg.h>

#include "WiiPE.hpp"

//
// phys_entry structure for 10.2.
//
typedef struct {
	void          *phys_link; // Opaque.
	unsigned int  pte1;
} phys_entry_jaguar_t;

//
// phys_entry structure for 10.3 and newer.
//
#pragma pack(4)							/* Make sure the structure stays as we defined it */
typedef struct {
  addr64_t	ppLink;				/* Physical pointer to aliased mappings and flags */
#define		ppLock		0x8000000000000000LL	/* Lock for alias chain */
#define		ppN			0x4000000000000000LL	/* Not executable */
#define		ppFlags		0x000000000000003FLL	/* Status and flags */
#define		ppI			0x0000000000000020LL	/* Cache inhibited */
#define		ppIb		58						/* Cache inhibited */
#define		ppG			0x0000000000000010LL	/* Guarded */
#define		ppGb		59						/* Guarded */
#define		ppR			0x0000000000000008LL	/* Referenced */
#define		ppRb		60						/* Referenced */
#define		ppC			0x0000000000000004LL	/* Changed */
#define		ppCb		61						/* Changed */
#define		ppPP		0x0000000000000003LL	/* Protection */
#define		ppPPb		62						/* Protection begin */
#define		ppPPe		63						/* Protection end */
} phys_entry_panther_t;
#pragma pack()

//
// Search for and get the pointer to the kernel's Mach-O header.
//
bool WiiPE::findKernelMachHeader(void) {
  struct mach_header      *machHeader;
  UInt8                   *machCommands;
  struct load_command     *loadCommand;
  struct segment_command  *segCommand;
  struct segment_command  *linkedItSegCommand;
  struct symtab_command   *symCommand;
  bool                    foundHeader;

  //
  // Search for a valid Mach-O header.
  // On all PowerPC versions, the kernel is mapped 1:1 with no KASLR. The address of the header will vary between versions.
  //
  foundHeader = false;
  for (UInt32 addr = reinterpret_cast<UInt32>(IOLog) & 0xFFFFF000; addr > 0; addr -= PAGE_SIZE) {
    machHeader = reinterpret_cast<struct mach_header*>(addr);
    if ((machHeader->magic == MH_MAGIC) && (machHeader->cputype == CPU_TYPE_POWERPC) && (machHeader->filetype == MH_EXECUTE)) {
      //
      // Found a valid header, check to see if __TEXT segment is first.
      //
      machCommands = reinterpret_cast<UInt8*>(addr) + sizeof (*machHeader);
      for (UInt32 i = 0; i < machHeader->ncmds; i++) {
        loadCommand = reinterpret_cast<struct load_command*>(machCommands);

        if (loadCommand->cmd == LC_SEGMENT) {
          segCommand = reinterpret_cast<struct segment_command*>(loadCommand);
          if (strncmp(segCommand->segname, "__TEXT", sizeof (segCommand->segname)) == 0) {
            foundHeader = true;
            break;
          }
        }

        machCommands += loadCommand->cmdsize;
      }

      if (foundHeader) {
        WIIDBGLOG("Found kernel Mach-O header at 0x%X", addr);
        break;
      }
    }
  }

  if (!foundHeader) {
    return false;
  }

  //
  // Get the symbols.
  //
  linkedItSegCommand  = NULL;
  symCommand          = NULL;
  machCommands = reinterpret_cast<UInt8*>(machHeader) + sizeof (*machHeader);
  for (UInt32 i = 0; i < machHeader->ncmds; i++) {
    loadCommand = reinterpret_cast<struct load_command*>(machCommands);

    if (loadCommand->cmd == LC_SEGMENT) {
      segCommand = reinterpret_cast<struct segment_command*>(loadCommand);
      if (strncmp(segCommand->segname, "__LINKEDIT", sizeof (segCommand->segname)) == 0) {
        linkedItSegCommand = segCommand;
      }
    } else if (loadCommand->cmd == LC_SYMTAB) {
      symCommand = reinterpret_cast<struct symtab_command*>(loadCommand);
    }

    machCommands += loadCommand->cmdsize;
  }

  if ((symCommand == NULL) || (linkedItSegCommand == NULL)) {
    return false;
  }

  _symTab           = reinterpret_cast<UInt8*>(linkedItSegCommand->vmaddr + symCommand->symoff - linkedItSegCommand->fileoff);
  _symTabNumSymbols = symCommand->nsyms;
  _strTab           = reinterpret_cast<UInt8*>(linkedItSegCommand->vmaddr + symCommand->stroff - linkedItSegCommand->fileoff);
  _strTabSize       = symCommand->strsize;

  WIIDBGLOG("Symtab: %p, strtab: %p", _symTab, _strTab);

  return true;
}

//
// Resolve a kernel symbol. This function can only be called prior to kernel linker jettison.
//
UInt32 WiiPE::resolveKernelSymbol(const char *symbolName) {
  struct nlist  *nlistSym;
  char          *symStr;

  nlistSym = reinterpret_cast<struct nlist*>(_symTab);
  for (UInt32 i = 0; i < _symTabNumSymbols; i++, nlistSym++) {
    //
    // Get the symbol string for the symbol.
    //
    symStr = reinterpret_cast<char*>(_strTab + nlistSym->n_un.n_strx);
    if (strncmp(symbolName, symStr, strlen(symbolName)) == 0) {
      WIIDBGLOG("Found symbol '%s' at 0x%X", symbolName, nlistSym->n_value);
      return nlistSym->n_value;
    }
  }

  WIISYSLOG("Failed to locate symbol '%s'", symbolName);
  return 0;
}

//
// Forces physical memory to be marked as cache inhibited (i.e. for graphics memory).
//
void WiiPE::setPhysMemoryCacheInhibit(UInt32 physAddr, UInt32 physLength) {
  if (getKernelVersion() >= kKernelVersionPanther) {
    ppnum_t paStart = physAddr >> 12;
    ppnum_t paEnd   = paStart + (physLength / PAGE_SIZE);
    ppnum_t paCurr = paStart;
    while (paCurr <= paEnd) {
      phys_entry_panther_t *physEntry = (*(phys_entry_panther_t* (*)(ppnum_t))_pmapFindPhysEntryFuncAddr)(paCurr);
      physEntry->ppLink |= ppI | ppG;
      paCurr++;
    }
  } else if (getKernelVersion() == kKernelVersionJaguar) {
    vm_offset_t paStart = physAddr;
    vm_offset_t paEnd   = paStart + physLength;
    vm_offset_t paCurr = paStart;
    while (paCurr <= paEnd) {
      void *physEntry = (*(void* (*)(vm_offset_t))_pmapFindPhysEntryFuncAddr)(paCurr);
      (*(void (*)(void *pp, unsigned int pa, unsigned int wimg))_mappingPhysInitFuncAddr)(physEntry, paCurr, PTE_WIMG_IO);
      WIISYSLOG("Fixing 0x%X", paCurr);
      paCurr += PAGE_SIZE;
    }
  }
}

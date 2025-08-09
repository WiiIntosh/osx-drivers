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

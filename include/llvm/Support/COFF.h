//===-- llvm/Support/COFF.h -------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains an definitions used in Windows COFF Files.
//
// Structures and enums defined within this file where created using
// information from Microsofts publicly available PE/COFF format document:
// 
// Microsoft Portable Executable and Common Object File Format Specification
// Revision 8.1 - February 15, 2008
//
// As of 5/2/2010, hosted by microsoft at:
// http://www.microsoft.com/whdc/system/platform/firmware/pecoff.mspx
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SUPPORT_WIN_COFF_H
#define LLVM_SUPPORT_WIN_COFF_H

#include "llvm/System/DataTypes.h"
#include <cstring>

namespace llvm {
namespace COFF {

  // Sizes in bytes of various things in the COFF format.
  enum {
    HeaderSize     = 20,
    NameSize       = 8,
    SymbolSize     = 18,
    SectionSize    = 40,
    RelocationSize = 10
  };

  struct header {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
  };

  struct symbol {
    char     Name[NameSize];
    uint32_t Value;
    uint16_t Type;
    uint8_t  StorageClass;
    uint16_t SectionNumber;
    uint8_t  NumberOfAuxSymbols;
  };

  enum symbol_flags {
    SF_TypeMask = 0x0000FFFF,
    SF_TypeShift = 0,

    SF_ClassMask = 0x00FF0000,
    SF_ClassShift = 16,

    SF_WeakReference = 0x01000000
  };

  enum symbol_storage_class {
    IMAGE_SYM_CLASS_END_OF_FUNCTION  = -1,
    IMAGE_SYM_CLASS_NULL             = 0,
    IMAGE_SYM_CLASS_AUTOMATIC        = 1,
    IMAGE_SYM_CLASS_EXTERNAL         = 2,
    IMAGE_SYM_CLASS_STATIC           = 3,
    IMAGE_SYM_CLASS_REGISTER         = 4,
    IMAGE_SYM_CLASS_EXTERNAL_DEF     = 5,
    IMAGE_SYM_CLASS_LABEL            = 6,
    IMAGE_SYM_CLASS_UNDEFINED_LABEL  = 7,
    IMAGE_SYM_CLASS_MEMBER_OF_STRUCT = 8,
    IMAGE_SYM_CLASS_ARGUMENT         = 9,
    IMAGE_SYM_CLASS_STRUCT_TAG       = 10,
    IMAGE_SYM_CLASS_MEMBER_OF_UNION  = 11,
    IMAGE_SYM_CLASS_UNION_TAG        = 12,
    IMAGE_SYM_CLASS_TYPE_DEFINITION  = 13,
    IMAGE_SYM_CLASS_UNDEFINED_STATIC = 14,
    IMAGE_SYM_CLASS_ENUM_TAG         = 15,
    IMAGE_SYM_CLASS_MEMBER_OF_ENUM   = 16,
    IMAGE_SYM_CLASS_REGISTER_PARAM   = 17,
    IMAGE_SYM_CLASS_BIT_FIELD        = 18,
    IMAGE_SYM_CLASS_BLOCK            = 100,
    IMAGE_SYM_CLASS_FUNCTION         = 101,
    IMAGE_SYM_CLASS_END_OF_STRUCT    = 102,
    IMAGE_SYM_CLASS_FILE             = 103,
    IMAGE_SYM_CLASS_SECTION          = 104,
    IMAGE_SYM_CLASS_WEAK_EXTERNAL    = 105,
    IMAGE_SYM_CLASS_CLR_TOKEN        = 107
  };

  struct section {
    char     Name[NameSize];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLineNumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLineNumbers;
    uint32_t Characteristics;
  };

  enum section_characteristics {
    IMAGE_SCN_TYPE_NO_PAD            = 0x00000008,
    IMAGE_SCN_CNT_CODE               = 0x00000020,
    IMAGE_SCN_CNT_INITIALIZED_DATA   = 0x00000040,
    IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x00000080,
    IMAGE_SCN_LNK_OTHER              = 0x00000100,
    IMAGE_SCN_LNK_INFO               = 0x00000200,
    IMAGE_SCN_LNK_REMOVE             = 0x00000800,
    IMAGE_SCN_LNK_COMDAT             = 0x00001000,
    IMAGE_SCN_GPREL                  = 0x00008000,
    IMAGE_SCN_MEM_PURGEABLE          = 0x00020000,
    IMAGE_SCN_MEM_16BIT              = 0x00020000,
    IMAGE_SCN_MEM_LOCKED             = 0x00040000,
    IMAGE_SCN_MEM_PRELOAD            = 0x00080000,
    IMAGE_SCN_ALIGN_1BYTES           = 0x00100000,
    IMAGE_SCN_ALIGN_2BYTES           = 0x00200000,
    IMAGE_SCN_ALIGN_4BYTES           = 0x00300000,
    IMAGE_SCN_ALIGN_8BYTES           = 0x00400000,
    IMAGE_SCN_ALIGN_16BYTES          = 0x00500000,
    IMAGE_SCN_ALIGN_32BYTES          = 0x00600000,
    IMAGE_SCN_ALIGN_64BYTES          = 0x00700000,
    IMAGE_SCN_ALIGN_128BYTES         = 0x00800000,
    IMAGE_SCN_ALIGN_256BYTES         = 0x00900000,
    IMAGE_SCN_ALIGN_512BYTES         = 0x00A00000,
    IMAGE_SCN_ALIGN_1024BYTES        = 0x00B00000,
    IMAGE_SCN_ALIGN_2048BYTES        = 0x00C00000,
    IMAGE_SCN_ALIGN_4096BYTES        = 0x00D00000,
    IMAGE_SCN_ALIGN_8192BYTES        = 0x00E00000,
    IMAGE_SCN_LNK_NRELOC_OVFL        = 0x01000000,
    IMAGE_SCN_MEM_DISCARDABLE        = 0x02000000,
    IMAGE_SCN_MEM_NOT_CACHED         = 0x04000000,
    IMAGE_SCN_MEM_NOT_PAGED          = 0x08000000,
    IMAGE_SCN_MEM_SHARED             = 0x10000000,
    IMAGE_SCN_MEM_EXECUTE            = 0x20000000,
    IMAGE_SCN_MEM_READ               = 0x40000000,
    IMAGE_SCN_MEM_WRITE              = 0x80000000
  };

  struct relocation {
    uint32_t VirtualAddress;
    uint32_t SymbolTableIndex;
    uint16_t Type;
  };

  enum relocation_type_x86 {
    IMAGE_REL_I386_ABSOLUTE = 0x0000,
    IMAGE_REL_I386_DIR16    = 0x0001,
    IMAGE_REL_I386_REL16    = 0x0002,
    IMAGE_REL_I386_DIR32    = 0x0006,
    IMAGE_REL_I386_DIR32NB  = 0x0007,
    IMAGE_REL_I386_SEG12    = 0x0009,
    IMAGE_REL_I386_SECTION  = 0x000A,
    IMAGE_REL_I386_SECREL   = 0x000B,
    IMAGE_REL_I386_TOKEN    = 0x000C,
    IMAGE_REL_I386_SECREL7  = 0x000D,
    IMAGE_REL_I386_REL32    = 0x0014
  };

  enum {
    IMAGE_COMDAT_SELECT_NODUPLICATES = 1,
    IMAGE_COMDAT_SELECT_ANY,
    IMAGE_COMDAT_SELECT_SAME_SIZE,
    IMAGE_COMDAT_SELECT_EXACT_MATCH,
    IMAGE_COMDAT_SELECT_ASSOCIATIVE,
    IMAGE_COMDAT_SELECT_LARGEST
  };

} // End namespace llvm.
} // End namespace COFF.

#endif

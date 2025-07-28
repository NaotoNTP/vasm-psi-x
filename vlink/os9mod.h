/* $VER: vlink os9mod.h V0.16f (09.08.20)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2020  Frank Wille
 */


/* OS9/6809 module header, big endian */
typedef struct
{
  uint8_t m_sync[2];      /* 6809: 0x87,0xcd */
  uint8_t m_size[2];      /* size of complete module, including CRC */
  uint8_t m_name[2];      /* offset to module name, bit7 set on last char */
  uint8_t m_tylan;        /* type and language */
  uint8_t m_attrev;       /* attributes and revision, bit7 means shareable */
  uint8_t m_parity;       /* header parity: XOR of the first 8 bytes */
  uint8_t m_exec[2];      /* offset to program entry */
  uint8_t m_data[2];      /* permanent storage size */
} mh6809;

#define OS9_6809_SYNC 0x87cd
#define OS9_6809_DEFSTK 1024


/* default script */
static const char defaultscript[] =
  "SECTIONS {\n"
  "  .code SIZEOF_HEADERS: {\n"
  "    __ehdr = .;\n"
  "    *(__MODNAME)\n"
  "    *(.t* t* T* .c* c* C*)\n"
  "    *(.ro* ro*)\n"
  "    VBCC_CONSTRUCTORS_ELF\n"
  "    __etext = .;\n"
  "    PROVIDE(etext = .);\n"
  "  }\n"
  "  .dpage 0: AT(LOADADDR(.code)+SIZEOF(.code)) {\n"
  "    BYTE(0);\n"
  "    *(.dp* dp* DP*)\n"
  "  }\n"
  "  .data: {\n"
  "    *(.da* da* DA*)\n"
  "    __edata = .;\n"
  "    PROVIDE(edata = .);\n"
  "  }\n"
  "  .bss (NOLOAD): {\n"
  "    *(.b* b* B* .u* u* U*)\n"
  "    *(COMMON)\n"
  "    __end = .;\n"
  "    PROVIDE(end = .);\n"
  "  }\n"
  "  __bsslen = SIZEOF(.bss);\n"
  "}\n";

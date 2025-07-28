/* $VER: vlink xfile.h V0.18 (20.10.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2019,2024  Frank Wille
 */


/* XFile program header, big endian */
typedef struct
{
  uint8_t x_id[2];        /* 'H','U' - xfile identification */
  uint8_t x_rsrvd1[1];    /* unused - always zero */
  uint8_t x_loadmode;     /* normal, minimal memory, high address */
  uint8_t x_baseaddr[4];  /* linker's base address, usually 0 */
  uint8_t x_execaddr[4];  /* execution offset on load address */
  uint8_t x_textsz[4];    /* .text size in bytes */
  uint8_t x_datasz[4];    /* .data size in bytes */
  uint8_t x_heapsz[4];    /* .bss and .stack size in bytes */
  uint8_t x_relocsz[4];   /* relocation table size in bytes */
  uint8_t x_syminfsz[4];  /* symbol info size in bytes */
  uint8_t x_scdlinsz[4];  /* SCD line info size */
  uint8_t x_scdsymsz[4];  /* SCD symbols size */
  uint8_t x_scdstrsz[4];  /* SCD strings size */
  uint8_t x_rsrvd2[20];   /* unused - always zero */
} XFile;

/* x_loadmode */
#define XLMD_NORMAL     0
#define XLMD_MINMEM     1
#define XLMD_HIGHADDR   2


/* Xfile symbol table */
typedef struct
{
  uint16_t type;
  uint32_t value;
  char name[1];           /* null-terminated symbol name, padded to even */
} XSym;

#define XSYM_ABS	0x0200
#define XSYM_TEXT       0x0201
#define XSYM_DATA       0x0202
#define XSYM_BSS        0x0203
#define XSYM_STACK      0x0204


/* default script */
static const char defaultscript[] =
  "SECTIONS {\n"
  "  . = 0;\n"
  "  .text: {\n"
  "    *(.i* i* I*)\n"
  "    *(.t* t* T* .c* c* C*)\n"
  "    *(.f* f* F*)\n"
  "    PROVIDE(_etext = .);\n"
  "    PROVIDE(__etext = .);\n"
  "    . = ALIGN(2);\n"
  "  }\n"
  "  .data: {\n"
  "    PROVIDE(_LinkerDB = . + 0x8000);\n"
  "    PROVIDE(_SDA_BASE_ = . + 0x8000);\n"
  "    VBCC_CONSTRUCTORS\n"
  "    *(.rodata*)\n"
  "    *(.d* d* D*)\n"
  "    *(.sdata*)\n"
  "    *(__MERGED)\n"
  "    PROVIDE(_edata = .);\n"
  "    PROVIDE(__edata = .);\n"
  "    . = ALIGN(2);\n"
  "  }\n"
  "  .bss: {\n"
  "    *(.sbss*)\n"
  "    *(.scommon)\n"
  "    *(.b* b* B* .u* u* U*)\n"
  "    *(COMMON)\n"
  "    PROVIDE(_end = ALIGN(4));\n"
  "    PROVIDE(__end = ALIGN(4));\n"
  "  }\n"
  "}\n";

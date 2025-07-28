/* $VER: vlink rel_elfx86_64.h V0.14 (24.06.11)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2011  Frank Wille
 */


#ifndef REL_ELFX86_64_H
#define REL_ELFX86_64_H

#define R_X86_64_NONE           0
#define R_X86_64_64             1
#define R_X86_64_PC32           2
#define R_X86_64_GOT32          3
#define R_X86_64_PLT32          4
#define R_X86_64_COPY           5
#define R_X86_64_GLOB_DAT       6
#define R_X86_64_JUMP_SLOT      7
#define R_X86_64_RELATIVE       8
#define R_X86_64_GOTPCREL       9
#define R_X86_64_32             10
#define R_X86_64_32S            11
#define R_X86_64_16             12
#define R_X86_64_PC16           13
#define R_X86_64_8              14
#define R_X86_64_PC8            15

/* TLS relocations */
#define R_X86_64_DTPMOD64       16
#define R_X86_64_DTPOFF64       17
#define R_X86_64_TPOFF64        18
#define R_X86_64_TLSGD          19
#define R_X86_64_TLSLD          20
#define R_X86_64_DTPOFF32       21
#define R_X86_64_GOTTPOFF       22
#define R_X86_64_TPOFF32        23

#endif

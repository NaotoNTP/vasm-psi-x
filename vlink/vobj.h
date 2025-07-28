/* $VER: vlink vobj.h V0.17b (23.01.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2024  Frank Wille
 */

/* maximum VOBJ version to support */
#define VOBJ_MAX_VERSION 2

/* symbol types */
#define LABSYM 1
#define IMPORT 2
#define EXPRESSION 3

/* symbol flags */
#define TYPE(sym) ((sym)->flags&7)
#define TYPE_UNKNOWN  0
#define TYPE_OBJECT   1
#define TYPE_FUNCTION 2
#define TYPE_SECTION  3
#define TYPE_FILE     4

#define EXPORT 8
#define INEVAL 16
#define COMMON 32
#define WEAK 64

/* section flags */
#define UNALLOCATED (1<<2)


typedef lword taddr;

struct vobj_symbol {
  char *name;
  int type,flags,sec,size;
  taddr val;
};

#define VOBJ_REL_S 0x20       /* signed VOBJ reloc type */
#define VOBJ_REL_U 0x40       /* unsigned VOBJ reloc type */
#define FIRST_CPU_RELOC 0x80  /* first cpu-specific reloc type in VOBJ */
#define STD_REL_TYPE(t) ((t)&0x1f)

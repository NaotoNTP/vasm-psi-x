/* $VER: vlink elf32.h V0.17b (26.01.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2011,2024  Frank Wille
 */


#include "elfcommon.h"
#include "elf32std.h"


struct ShdrNode {
  struct node n;
  struct Elf32_Shdr s;
};


/* .stab compilation units */
struct StabCompUnit {
  struct node n;
  long nameidx;
  unsigned long entries;
  struct list stabs;
  struct StrTabList strtab;
};


/* Prototypes from t_elf32.c */

void elf32_parse(struct GlobalVars *,struct LinkFile *,struct Elf32_Ehdr *,
                 int (*)(uint8_t,struct RelocInsert *));
void elf32_initdynlink(struct GlobalVars *);
struct Section *elf32_dyntable(struct GlobalVars *,unsigned long,unsigned long,
                               uint8_t,uint8_t,uint8_t,int);
struct Symbol *elf32_pltgotentry(struct GlobalVars *,struct Section *,DynArg,
                                 uint8_t,unsigned long,unsigned long,int);
struct Symbol *elf32_bssentry(struct GlobalVars *,const char *,struct Symbol *);
void elf32_dynamicentry(struct GlobalVars *,uint32_t,uint32_t,struct Section *);
void elf32_dyncreate(struct GlobalVars *,const char *);
unsigned long elf32_headersize(struct GlobalVars *);
void elf32_writerelocs(struct GlobalVars *,FILE *);
void elf32_writeobject(struct GlobalVars *,FILE *,uint16_t,int8_t,
                       uint8_t (*)(struct Reloc *));
void elf32_writeexec(struct GlobalVars *,FILE *,uint16_t,int8_t,
                     uint8_t (*)(struct Reloc *));

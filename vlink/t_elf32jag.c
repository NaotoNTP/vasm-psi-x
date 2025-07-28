/* $VER: vlink t_elf32jag.c V0.18 (04.02.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2016,2024  Frank Wille
 */


#include "config.h"
#if defined(ELF32_JAG)
#define T_ELF32JAG_C
#include "vlink.h"
#include "elf32.h"
#include "rel_elfjag.h"


static int jag_identify(struct GlobalVars *,char *,uint8_t *,unsigned long,bool);
static void jag_readconv(struct GlobalVars *,struct LinkFile *);
static void jag_writeobject(struct GlobalVars *,FILE *);
static void jag_writeshared(struct GlobalVars *,FILE *);
static void jag_writeexec(struct GlobalVars *,FILE *);

struct FFFuncs fff_elf32jag = {
  "elf32jag",
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  elf32_headersize,
  jag_identify,
  jag_readconv,
  NULL,
  elf_targetlink,
  NULL,
  elf_lnksym,
  elf_setlnksym,
  NULL,NULL,NULL,
  jag_writeobject,
  jag_writeshared,
  jag_writeexec,
  bss_name,sbss_name,
  4,  /* no MMU, just align to 32 bits */
  0,
  0,
  0,
  RTAB_ADDEND,RTAB_STANDARD|RTAB_ADDEND,
  _BIG_ENDIAN_,
  32,1
};



/*****************************************************************/
/*                          Read ELF                             */
/*****************************************************************/


static int jag_identify(struct GlobalVars *gv,char *name,uint8_t *p,
                        unsigned long plen,bool lib)
/* identify ELF-JaguarRISC-32Bit-BigEndian */
{
  return (elf_identify(&fff_elf32jag,name,p,plen,
                       ELFCLASS32,ELFDATA2MSB,EM_JAGRISC,ELF_VER));
}


static int jag_reloc_elf2vlink(uint8_t rtype,struct RelocInsert *ri)
/* Determine vlink internal reloc type from ELF reloc type and fill in
   reloc-insert description information.
   All fields of the RelocInsert structure are preset to zero. */
{
  static struct RelocInsert ri2;

  static struct ELF2vlink convert[] = {
    R_NONE,0,0,-1,
    R_ABS,0,32,-1,                  /* R_JAG_ABS32 */
    R_ABS,0,16,-1,                  /* R_JAG_ABS16 */
    R_ABS,0,8,-1,                   /* R_JAG_ABS8 */
    R_PC|R_S,0,32,-1,               /* R_JAG_REL32 */
    R_PC|R_S,0,16,-1,               /* R_JAG_REL16 */
    R_PC|R_S,0,8,-1,                /* R_JAG_REL8 */
    R_ABS,6,5,-1,                   /* R_JAG_ABS5 */
    R_PC|R_S,6,5,-1,                /* R_JAG_REL5 */
    R_PC|R_S,6,5,~1,                /* R_JAG_JR */
    R_ABS,16,16,~0xffff,            /* R_JAG_ABS32SWP */
    R_PC|R_S,16,16,~0xffff          /* R_JAG_REL32SWP */
  };

  if (rtype <= R_JAG_REL32SWP) {
    ri->bpos = convert[rtype].bpos;
    ri->bsiz = convert[rtype].bsiz;
    ri->mask = convert[rtype].mask;
    rtype = convert[rtype].rtype;

    if (ri->mask == ~0xffff) {
      /* R_JAG_xxxSWP - add a second RelocInsert */
      ri2 = *ri;
      ri2.bpos = 0;
      ri2.mask = 0xffff;
      ri->next = &ri2;
    }
  }
  else
    rtype = R_NONE;

  return (rtype);
}


static void jag_readconv(struct GlobalVars *gv,struct LinkFile *lf)
/* Read ELF-Jaguar executable / object / shared obj. */
{
  if (lf->type == ID_LIBARCH) {
    struct ar_info ai;

    if (ar_init(&ai,(char *)lf->data,lf->length,lf->filename)) {
      while (ar_extract(&ai)) {
        lf->objname = allocstring(ai.name);
        if (elf_check_ar_type(fff[lf->format],lf->pathname,ai.data,
                              ELFCLASS32,ELFDATA2MSB,ELF_VER,1,EM_JAGRISC))
          elf32_parse(gv,lf,(struct Elf32_Ehdr *)ai.data,jag_reloc_elf2vlink);
      }
    }
    else
      ierror("jag_readconv(): archive %s corrupted since last access",
             lf->pathname);
  }
  else {
    lf->objname = lf->filename;
    elf32_parse(gv,lf,(struct Elf32_Ehdr *)lf->data,jag_reloc_elf2vlink);
  }
}



/*****************************************************************/
/*                          Write ELF                            */
/*****************************************************************/


static uint8_t jag_reloc_vlink2elf(struct Reloc *r)
/* Try to map vlink-internal representation to a valid ELF reloc type */
{
  struct RelocInsert *ri;
  uint8_t rt = R_NONE;

  if (isstdreloc(r,R_ABS,32)) return R_JAG_ABS32;
  else if (isstdreloc(r,R_ABS,16)) return R_JAG_ABS16;
  else if (isstdreloc(r,R_ABS,8)) return R_JAG_ABS8;
  else if (isstdreloc(r,R_PC,32)) return R_JAG_REL32;
  else if (isstdreloc(r,R_PC,16)) return R_JAG_REL16;
  else if (isstdreloc(r,R_PC,8)) return R_JAG_REL8;

  else if (ri = r->insert) {
    int pos = ri->bpos;
    int siz = ri->bsiz;
    lword mask = ri->mask;
    struct RelocInsert *ri2 = ri->next;

    if (pos==6 && siz==5) {
      if (mask == ~0)
        rt = r->rtype==R_ABS ? R_JAG_ABS5 : R_JAG_REL5;
      else if (mask == ~1)
        rt = R_JAG_JR;
    }
    else if ((pos&15)==0 && siz==32 && ri2!=NULL && ri2->bsiz==32 &&
             (mask==~0xffff || mask==0xffff) &&
             (ri2->mask==~0xffff || ri2->mask==0xffff) &&
             mask!=ri2->mask) {
      r->offset += mask==0xffff ? (pos >> 3) : (ri2->bpos >> 3);
      rt = r->rtype==R_ABS ? R_JAG_ABS32SWP : R_JAG_REL32SWP;
    }
  }

  return rt;
}


static void jag_writeshared(struct GlobalVars *gv,FILE *f)
/* creates a target-elf32jag shared object (which is pos. independent) */
{
  ierror("jag_writeshared(): Shared object generation has not "
         "yet been implemented");
}


static void jag_writeobject(struct GlobalVars *gv,FILE *f)
/* creates a target-elf32jag relocatable object file */
{
  elf32_writeobject(gv,f,EM_JAGRISC,_BIG_ENDIAN_,jag_reloc_vlink2elf);
}


static void jag_writeexec(struct GlobalVars *gv,FILE *f)
/* creates a target-elf32jag executable file (with absolute addresses) */
{
  elf32_writeexec(gv,f,EM_JAGRISC,_BIG_ENDIAN_,jag_reloc_vlink2elf);
}


#endif /* ELF32_JAG */

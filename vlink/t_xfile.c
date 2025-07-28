/* $VER: vlink t_xfile.c V0.18 (23.12.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2019,2024  Frank Wille
 */

#include "config.h"
#ifdef XFILE
#define T_XFILE_C
#include "vlink.h"
#include "xfile.h"


static int options(struct GlobalVars *,int,const char **,int *);
static void printhelp(void);
static int identify(struct GlobalVars *,char *,uint8_t *,unsigned long,bool);
static void readconv(struct GlobalVars *,struct LinkFile *);
static int targetlink(struct GlobalVars *,struct LinkedSection *,
                      struct Section *);
static unsigned long headersize(struct GlobalVars *);
static void writeobject(struct GlobalVars *,FILE *);
static void writeshared(struct GlobalVars *,FILE *);
static void writeexec(struct GlobalVars *,FILE *);


struct FFFuncs fff_xfile = {
  "xfile",
  defaultscript,
  NULL,
  NULL,
  options,
  printhelp,
  headersize,
  identify,
  readconv,
  NULL,
  targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  writeobject,
  writeshared,
  writeexec,
  bss_name,NULL,
  0,
  0x7ffe,
  0,
  0,
  RTAB_STANDARD,RTAB_STANDARD,
  _BIG_ENDIAN_,
  32,1,
  FFF_BASEINCR
};

/* options */
static uint8_t loadmode;


static int options(struct GlobalVars *gv,int argc,const char *argv[],int *i)
{
  if (!strcmp(argv[*i],"-x-high"))
    loadmode = XLMD_HIGHADDR;
  else return 0;

  return 1;
}


static void printhelp(void)
{
  printf("-x-high           set high-address loading mode\n");
}



/*****************************************************************/
/*                         Read XFile                            */
/*****************************************************************/


static int identify(struct GlobalVars *gv,char *name,uint8_t *p,
                    unsigned long plen,bool lib)
/* identify an XFile-format file */
{
  return ID_UNKNOWN;  /* @@@ no read-support at the moment */
}


static void readconv(struct GlobalVars *gv,struct LinkFile *lf)
{
  ierror("readconv(): Can't read XFile");
}



/*****************************************************************/
/*                         Link XFile                            */
/*****************************************************************/


static int targetlink(struct GlobalVars *gv,struct LinkedSection *ls,
                      struct Section *s)
/* returns 1, if target requires the combination of the two sections, */
/* returns -1, if target doesn't want to combine them, */
/* returns 0, if target doesn't care - standard linking rules are used. */
{
  /* XFile requires that all sections of type CODE or DATA or BSS */
  /* will be combined, because there are only those three available! */
  /* @@@ FIXME: ^ That's not really true... */
  if (ls->type == s->type)
    return 1;

  return 0;
}



/*****************************************************************/
/*                        Write XFile                            */
/*****************************************************************/


static void xfile_initwrite(struct GlobalVars *gv,
                            struct LinkedSection **sections)
/* find exactly one ST_CODE, ST_DATA and ST_UDATA section, which
   will become .text, .data and .bss. */
{
  get_text_data_bss(gv,sections);
  text_data_bss_gaps(sections);  /* calculate gap size between sections */
}


static void xfile_header(FILE *f,unsigned long baseaddr,unsigned long execaddr,
                         unsigned long tsize,unsigned long dsize,
                         unsigned long bsize)
{
  XFile hdr;

  memset(&hdr,0,sizeof(XFile));
  write16be(hdr.x_id,0x4855);  /* "HU" ID for Human68k */
  hdr.x_loadmode = loadmode;
  write32be(hdr.x_baseaddr,baseaddr);
  write32be(hdr.x_execaddr,execaddr-baseaddr);
  write32be(hdr.x_textsz,tsize);
  write32be(hdr.x_datasz,dsize);
  write32be(hdr.x_heapsz,bsize);
  /* x_relocsz and x_syminfsz will be patched later */
  fwritex(f,&hdr,sizeof(XFile));
}


static size_t xfile_symboltable(struct GlobalVars *gv,FILE *f,
                                struct LinkedSection **sections)
/* The symbol table only contains absolute and reloc symbols,
   but doesn't differentiate between local and global scope. */
{
  static int relocsymtype[] = { XSYM_TEXT,XSYM_DATA,XSYM_BSS };
  struct Symbol *sym;
  size_t sz = 0;
  const char *p;
  int i;

  for (i=0; i<3; i++) {
    if (sections[i]) {
      for (sym=(struct Symbol *)sections[i]->symbols.first;
           sym->n.next!=NULL; sym=(struct Symbol *)sym->n.next) {

        if (!discard_symbol(gv,sym)) {
          /* write symbol type */
          if (sym->type == SYM_ABS) {
          	fwrite16be(f,XSYM_ABS);
          }
          else if (sym->type == SYM_RELOC) {
            fwrite16be(f,relocsymtype[i]);
          }
          else {
            error(33,fff_xfile.tname,sym->name,sym_bind[sym->bind],
                  sym_type[sym->type],sym_info[sym->info]);
            continue;
          }

          /* write symbol value */
          fwrite32be(f,sym->value);
          sz += 6;  /* 2 bytes type, 4 bytes value */

          /* write symbol name - 16-bit aligned */
          p = sym->name;
          do {
            fwrite8(f,*p);
            sz++;
          } while (*p++);
          if (sz & 1) {
            fwrite8(f,0);
            sz++;
          }
        }
      }
    }
  }
  return sz;
}


static size_t xfile_writerelocs(struct GlobalVars *gv,FILE *f,
                                struct LinkedSection **sections)
{
  const char *fn = "xfile_writerelocs(): ";
  struct Reloc *rel;
  struct RelocInsert *ri;
  uint32_t lastoffs = 0;
  uint32_t newoffs;
  int32_t diff;
  size_t sz = 0;
  int i;

  for (i=0; i<3; i++) {
    if (sections[i]) {
      sort_relocs(&sections[i]->relocs);

      for (rel=(struct Reloc *)sections[i]->relocs.first;
           rel->n.next!=NULL; rel=(struct Reloc *)rel->n.next) {
        if (ri = rel->insert) {
          /* only absolute 32-bit relocs are supported */
          if (rel->rtype!=R_ABS || ri->bpos!=0 || ri->bsiz!=32) {
            if (rel->rtype==R_ABS && (ri->bpos!=0 || ri->bsiz!=32))
              error(32,fff_xfile.tname,reloc_name[rel->rtype],
                    (int)ri->bpos,(int)ri->bsiz,mtaddr(gv,ri->mask),
                    sections[i]->name,rel->offset);
            continue;
          }
        }
        else
          continue;

        newoffs = sections[i]->base + rel->offset;
        diff = newoffs - lastoffs;
        lastoffs = newoffs;

        if (diff < 0) {
          ierror("%snegative offset difference: "
                 "%s(0x%08lx)+0x%08lx - 0x%08lx",fn,sections[i]->name,
                 sections[i]->base,rel->offset,lastoffs);
        }
        else if (diff > 0xffff) {
          /* write a large distance >= 64K */
          fwrite16be(f,1);
          fwrite32be(f,diff);
          sz += 6;
        }
        else {
          fwrite16be(f,(uint16_t)diff);
          sz += 2;
        }
      }
    }
  }

  return sz;
}


static unsigned long headersize(struct GlobalVars *gv)
{
  return 0;  /* irrelevant */
}


static void writeshared(struct GlobalVars *gv,FILE *f)
{
  error(30);  /* Target file format doesn't support shared objects */
}


static void writeobject(struct GlobalVars *gv,FILE *f)
/* creates a XFile relocatable object file */
{
  ierror("XFile object file generation has not yet been implemented");
}


static void writeexec(struct GlobalVars *gv,FILE *f)
/* creates a XFile executable file (which is relocatable) */
{
  struct LinkedSection *sections[3];
  size_t relocsz,syminfsz;
  int i;

  xfile_initwrite(gv,sections);

  for (i=0; i<3; i++)
    calc_relocs(gv,sections[i]);

  xfile_header(f,sections[0]->base,entry_address(gv),
               sections[0] ? sections[0]->size+sections[0]->gapsize : 0,
               sections[1] ? sections[1]->size+sections[1]->gapsize : 0,
               sections[2] ? sections[2]->size : 0);

  if (sections[0]) {
    fwritex(f,sections[0]->data,sections[0]->filesize);
    fwritegap(gv,f,
              (sections[0]->size-sections[0]->filesize)+sections[0]->gapsize,
              0);
  }

  if (sections[1]) {
    fwritex(f,sections[1]->data,sections[1]->filesize);
    fwritegap(gv,f,
              (sections[1]->size-sections[1]->filesize)+sections[1]->gapsize,
              0);
  }

  relocsz = xfile_writerelocs(gv,f,sections);
  syminfsz = xfile_symboltable(gv,f,sections);

  /* finally patch reloc- and symbol-table size into the header */
  fseek(f,offsetof(XFile,x_relocsz),SEEK_SET);
  fwrite32be(f,relocsz);
  fseek(f,offsetof(XFile,x_syminfsz),SEEK_SET);
  fwrite32be(f,syminfsz);
}


#endif

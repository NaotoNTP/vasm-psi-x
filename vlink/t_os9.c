/* $VER: vlink t_os9.c V0.18 (23.12.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2021,2024  Frank Wille
 */

#include "config.h"
#ifdef OS_9
#define T_OS9_C
#include "vlink.h"
#include "os9mod.h"


static void init(struct GlobalVars *,int);
static int options(struct GlobalVars *,int,const char **,int *);
static void printhelp(void);
static int identify(struct GlobalVars *,char *,uint8_t *,unsigned long,bool);
static void readconv(struct GlobalVars *,struct LinkFile *);
static int targetlink(struct GlobalVars *,struct LinkedSection *,
                      struct Section *);
static unsigned long hdrsize_6809(struct GlobalVars *);
static void writeobject(struct GlobalVars *,FILE *);
static void writeshared(struct GlobalVars *,FILE *);
static void writeexec_6809(struct GlobalVars *,FILE *);


struct FFFuncs fff_os9_6809 = {
  "os9-6809",
  defaultscript,
  NULL,
  init,
  options,
  printhelp,
  hdrsize_6809,
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
  writeexec_6809,
  NULL,NULL,
  0,
  0x7ffe,
  0,
  0,
  RTAB_STANDARD,RTAB_STANDARD,
  _BIG_ENDIAN_,
  16,0,
  FFF_BASEINCR|FFF_NOFILE
};

/* symbol labeling the module name */
static const char modname_sym[] = "__modname";

/* precalculated CRC table */
static uint32_t *crctab;

/* options */
static bool os9noshare;              /* non-shareable module */
static int os9mem,os9rev;            /* module header settings */
static const char *os9name;          /* module name */


static void init(struct GlobalVars *gv,int mode)
/* create an artificial object containing the module name */
{
  if (mode == FFINI_DESTFMT) {
    if (gv->ldscript == NULL)
      gv->pcrel_ctors = TRUE;  /* pcrel-con/destructors for default script */
  }
  else if (mode==FFINI_RESOLVE && gv->dest_name!=NULL &&
      findsymbol(gv,NULL,modname_sym,0)==NULL) {
    size_t modnamelen = strlen(os9name?os9name:base_name(gv->dest_name));
    char *modname = alloc(modnamelen);
    struct ObjectUnit *ou;
    struct Section *sec;

    /* make module name from destination file name */
    strncpy(modname,os9name?os9name:base_name(gv->dest_name),modnamelen);
    modname[modnamelen-1] |= 0x80;  /* set bit 7 on last character */

    /* create artificial object with a __MODNAME section for the name */
    ou = art_objunit(gv,"MODULE",(uint8_t *)modname,modnamelen);
    sec = add_section(ou,"__MODNAME",(uint8_t *)modname,modnamelen,ST_CODE,
                      SF_ALLOC,SP_READ|SP_EXEC,0,TRUE);

    /* add a symbol labeling the module name */
    addsymbol(gv,sec,modname_sym,NULL,0,SYM_RELOC,0,SYMI_OBJECT,
              SYMB_GLOBAL,modnamelen,FALSE);

    /* enqueue artificial object unit into linking process */
    ou->lnkfile->type = ID_OBJECT;
    add_objunit(gv,ou,FALSE);
  }
}


static int options(struct GlobalVars *gv,int argc,const char *argv[],int *i)
{
  if (!strncmp(argv[*i],"-os9-mem=",9)) {
    int last = strlen(argv[*i]) - 1;
    long mem;

    sscanf(&argv[*i][9],"%li",&mem);
    if (argv[*i][last]=='k' || argv[*i][last]=='K')
      mem <<= 10;  /* value is in KBytes instead of Bytes */
    os9mem = mem>256 ? mem : 256;
  }
  else if (!strncmp(argv[*i],"-os9-name=",10)) {
    os9name = &argv[*i][10];
  }
  else if (!strcmp(argv[*i],"-os9-ns")) {
    os9noshare = TRUE;
  }
  else if (!strncmp(argv[*i],"-os9-rev=",9)) {
    long rev;

    sscanf(&argv[*i][9],"%li",&rev);
    if (rev<0 || rev>15)
      error(130,argv[*i]);  /* bad assignment */
    else
      os9rev = rev;
  }
  else return 0;

  return 1;
}


static void printhelp(void)
{
  printf("-os9-mem=<val>[K] set size of stack/parameter area (default 1024)\n"
         "-os9-name=<name>  overwrite the module name in the header\n"
         "-os9-ns           declare module as non-shareable and non-reentrant\n"
         "-os9-rev=<val>    set revision in the module header (0-15, def. 0)\n");
}



/*****************************************************************/
/*                          Read OS9                             */
/*****************************************************************/


static int identify(struct GlobalVars *gv,char *name,uint8_t *p,
                    unsigned long plen,bool lib)
/* identify an OS9 object or module */
{
  return ID_UNKNOWN;  /* @@@ no read-support at the moment */
}


static void readconv(struct GlobalVars *gv,struct LinkFile *lf)
{
  ierror("readconv(): Can't read OS9 files");
}



/*****************************************************************/
/*                      Link OS9 module                          */
/*****************************************************************/


static int targetlink(struct GlobalVars *gv,struct LinkedSection *ls,
                      struct Section *s)
/* returns 1, if target requires the combination of the two sections, */
/* returns -1, if target doesn't want to combine them, */
/* returns 0, if target doesn't care - standard linking rules are used. */
{
  /* OS9 requires that all sections of type CODE or DATA or BSS */
  /* will be combined, because there are only those three available! */
  if (ls->type == s->type)
    return 1;

  return 0;
}



/*****************************************************************/
/*               Write OS9 modules and objects                   */
/*****************************************************************/


static uint16_t modname_address(struct GlobalVars *gv,uint16_t defval)
{
  struct Symbol *sym = findsymbol(gv,NULL,modname_sym,0);
  if (sym == NULL)
    ierror("%s symbol disappeared",modname_sym);
  return sym->relsect->lnksec!=NULL ? (uint16_t)sym->value : defval;
}


static void checkPIC(struct LinkedSection *ls)
/* check that section has position independent code, no absolute relocs */
{
#if 0  /* @@@ cannot be checked easily, as ABS,y must be allowed! */
  struct Reloc *r;

  for (r=(struct Reloc *)ls->relocs.first; r->n.next!=NULL;
       r=(struct Reloc *)r->n.next) {
    if (r->rtype==R_ABS || r->rtype==R_UABS) {
      
    }
  }
#endif
}


static void writedatarefs(FILE *f,struct list *refs,unsigned cnt)
{
  struct Reloc *rel;

  fwrite16be(f,cnt);
  while (rel = (struct Reloc *)remhead(refs)) {
    fwrite16be(f,rel->offset);
    cnt--;
  }
  if (cnt != 0)
    ierror("data refcnt doesn't match");
}


static uint8_t header_parity_check(void *hdr,int len)
{
#if 0
  uint8_t c,r=0,*p=hdr;
  int i,j;

  for (i=0; i<len; i++) {
    c = *p++;
    for (j=0; j<8; j++) {
      r += !(c & 1);
      c >>= 1;
    }
  }
  return r;
#else
  uint8_t c=0,*p=hdr;
  int i;

  for (i=0; i<len; i++)
    c ^= *p++;
  return ~c;
#endif
}


static void crc24_init(uint32_t poly)
{
  uint32_t crc,c;
  int i,j;

  crctab = alloc(sizeof(uint32_t)*256);
  if (crctab == NULL)
    ierror("crc24_init: out of memory");

  for (i=0; i<256; i++) {
    crc = 0;
    c = (uint32_t)i << 16;
    for (j=0; j<8; j++) {
      if ((crc ^ c) & 0x800000L)
        crc = (crc << 1) ^ poly;
      else
        crc <<= 1;
      c <<= 1;
    }
    crctab[i] = crc;
  }
}

   
static void crc24(uint32_t poly,uint8_t *dest,uint8_t *buf,size_t len)
{
  uint32_t crc = 0xffffff;
  size_t i;

  crc24_init(poly);

  for (i=0; i<len; i++)
    crc = (crc<<8) ^ crctab[((crc>>16) ^ (uint32_t)buf[i]) & 0xff];

  crc ^= 0xffffff;
  dest[0] = (crc>>16) & 0xff;
  dest[1] = (crc>>8) & 0xff;
  dest[2] = crc & 0xff;
}


static unsigned long hdrsize_6809(struct GlobalVars *gv)
{
  return sizeof(mh6809);
}


static void writeshared(struct GlobalVars *gv,FILE *f)
{
  error(30);  /* Target file format doesn't support shared objects */
}


static void writeobject(struct GlobalVars *gv,FILE *f)
/* creates an OS9 relocatable object file */
{
  ierror("OS9 object file generation has not yet been implemented");
}


static void writeexec_6809(struct GlobalVars *gv,FILE *f)
/* creates an OS9/6809 module (which is position independent and reentrant) */
{
  struct list dtrefs,ddrefs;
  unsigned dtrefcnt,ddrefcnt;
  struct LinkedSection *ls;
  struct Reloc *rel,*nextrel;
  struct RelocInsert *ri;
  lword rmask=makemask(16);
  size_t initdata_size,bss_size,stk_size,file_size;
  uint16_t entryoffs;
  lword lma;
  uint8_t *buf,crc[3];
  mh6809 hdr;

  /* create output file */
  if ((f = fopen(gv->dest_name,"wb")) == NULL) {
    error(29,gv->dest_name);  /* Can't create output file */
    return;
  }

  entryoffs = entry_address(gv);
  stk_size = os9mem ? os9mem : OS9_6809_DEFSTK;

  /* determine size of initialized data */
  for (ls=(struct LinkedSection *)gv->lnksec.first, initdata_size=bss_size=0;
       ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
    if (ls->type != ST_CODE) {
      if ((ls->flags & SF_ALLOC) && !(ls->ld_flags & LSF_NOLOAD))
        initdata_size += ls->size;
      else
        bss_size += ls->size;
    }
  }

  /* write position-independent code sections, including module header */
  lma = 0;
  while ((ls=load_next_section(gv))!=NULL && ls->type==ST_CODE) {
    if (lma==0 && ls->copybase<sizeof(mh6809))
      error(137,(unsigned)ls->copybase,sizeof(6809));  /* not enough space */
    if (ls->copybase > lma)
      fwritegap(gv,f,ls->copybase-lma,0);
    lma = ls->copybase + ls->size;

    checkPIC(ls);
    calc_relocs(gv,ls);
    fwritefullsect(gv,f,ls);
  }

  /* write size of initialized data */
  fwrite16be(f,initdata_size);

  /* write initialized data sections, which are copied at startup */
  initlist(&dtrefs);
  initlist(&ddrefs);
  dtrefcnt = ddrefcnt = 0;

  if (ls != NULL) do {
    if (ls->size>0 && (ls->flags & SF_ALLOC) && !(ls->ld_flags & LSF_NOLOAD)) {
      if (ls->type == ST_CODE)
        error(136,ls->name);  /* executable section in data segment */

      if (ls->copybase > lma)
        fwritegap(gv,f,ls->copybase-lma,0);
      lma = ls->copybase + ls->size;

      /* move data-text and data-data relocations into their own list */
      rel = (struct Reloc *)ls->relocs.first;
      while (nextrel = (struct Reloc *)rel->n.next) {
        if (rel->rtype==R_ABS || rel->rtype==R_UABS) {
          /* only byte-aligned 16-bit relocs are allowed */
          if ((ri=rel->insert)==NULL || ri->bpos!=0 || ri->bsiz!=16
              || (ri->mask & rmask)!=rmask)
            error(32,fff_os9_6809.tname,reloc_name[rel->rtype],
                  (int)rel->insert->bpos,(int)rel->insert->bsiz,
                  mtaddr(gv,rel->insert->mask),ls->name,rel->offset);

          remnode(&rel->n);
          writesection(gv,ls->data,rel->offset,rel,
                       rel->relocsect.lnk->base+rel->addend);
          rel->offset += ls->base; /* offset relative to whole segment */
          if (rel->relocsect.lnk->type==ST_CODE) {
            addtail(&dtrefs,&rel->n);
            dtrefcnt++;
          }
          else {
            addtail(&ddrefs,&rel->n);
            ddrefcnt++;
          }
        }
        rel = nextrel;
      }

      /* execute remaining relocs and write initialized data sections */
      calc_relocs(gv,ls);
      fwritefullsect(gv,f,ls);
    }
  } while (ls = load_next_section(gv));

  /* write data-text and data-data references */
  writedatarefs(f,&dtrefs,dtrefcnt);
  writedatarefs(f,&ddrefs,ddrefcnt);

  file_size = ftell(f);

  /* prepare the module header */
  write16be(hdr.m_sync,OS9_6809_SYNC);
  write16be(hdr.m_size,file_size+3);    /* including CRC, yet to be written */
  write16be(hdr.m_name,modname_address(gv,sizeof(mh6809)));
  hdr.m_tylan = 0x11;                    /* program module with 6809 code */
  hdr.m_attrev = (os9noshare?0:0x80) + (os9rev&15);
  hdr.m_parity = header_parity_check(&hdr,offsetof(mh6809,m_parity));
  write16be(hdr.m_exec,entryoffs);
  write16be(hdr.m_data,initdata_size+bss_size+stk_size);

  /* write the module header and close the output file */
  fseek(f,0,SEEK_SET);
  fwritex(f,&hdr,sizeof(mh6809));
  fclose(f);

  /* read it again for CRC generation */
  if ((f = fopen(gv->dest_name,"rb+")) == NULL) {
    error(8,gv->dest_name);  /* Cannot open output file */
    return;
  }
  buf = alloc(file_size);
  if (fread(buf,1,file_size,f) == file_size) {
    /* do 24-bit CRC for OS-9 and append it */
    fpos_t pos;

    fgetpos(f,&pos);
    crc24(0x800063,crc,buf,file_size);
    fsetpos(f,&pos);  /* Needed to write after reading acc. to ISO-C! */
    fwritex(f,crc,3);
  }
  else
    error(7,gv->dest_name);

  free(buf);
  fclose(f);
}


#endif

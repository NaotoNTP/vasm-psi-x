/* $VER: vlink t_vobj.c V0.18 (03.07.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2024 Frank Wille
 */

#include "config.h"
#ifdef VOBJ
#define T_VOBJ_C
#include "vlink.h"
#include "vobj.h"
#include "cpurelocs.h"

/*
  Format:

  .byte 0x56,0x4f,0x42,0x4a
  .byte flags
    Bits 0-1:
     1: BIGENDIAN
     2: LITTLENDIAN
    Bits 2-7:
     VOBJ-Version (0-based)
  .number bitsperbyte 
  .number bytespertaddr
  .string cpu
  .number nsections [1-based]
  .number nsymbols [1-based]
  
nsymbols
  .string name
  .number type
  .number flags
  .number secindex
  .number val
  .number size (in target-bytes)

nsections
  .string name
  .string attr
  .number flags
  .number align
  .number size (in target-bytes)
  .number nrelocs
  .number databytes (in target-bytes)
  .byte[databytes*(BITSPERBYTE+7)/8]

nrelocs [standard|special]
standard
   .number type
   .number byteoffset
   .number bitoffset
   .number size
   .number mask
   .number addend
   .number symbolindex | 0 (sectionbase)

special
    .number type
    .number size
    .byte[size]

.number:[taddr]
    .byte 0--127 [0--127]
    .byte 128-191 [x-0x80 bytes little-endian], fill remaining with 0
    .byte 192-255 [x-0xC0 bytes little-endian], fill remaining with 0xff [vobj version 2+]
*/

static int vobj_options(struct GlobalVars *,int,const char **,int *);
static void vobj_printhelp(void);
static unsigned long vobj_headersize(struct GlobalVars *);
static int vobjle_identify(struct GlobalVars *,char *,uint8_t *,
                           unsigned long,bool);
static int vobjbe_identify(struct GlobalVars *,char *,uint8_t *,
                           unsigned long,bool);
static void vobj_readconv(struct GlobalVars *,struct LinkFile *);
static int vobj_targetlink(struct GlobalVars *,struct LinkedSection *,
                              struct Section *);
static void vobj_writeobject(struct GlobalVars *,FILE *);
static void vobj_writeshared(struct GlobalVars *,FILE *);
static void vobj_writeexec(struct GlobalVars *,FILE *);


struct FFFuncs fff_vobj_le = {
  "vobj-le",
  NULL,
  NULL,
  NULL,
  vobj_options,
  vobj_printhelp,
  vobj_headersize,
  vobjle_identify,
  vobj_readconv,
  NULL,
  vobj_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  vobj_writeobject,
  vobj_writeshared,
  vobj_writeexec,
  bss_name,sbss_name,
  0,
  0, /* don't care */
  0,
  0,
  RTAB_UNDEF,0,
  _LITTLE_ENDIAN_,
  0,  /* defined by VOBJ bytespertaddr*bitsperbyte */
  0,  /* ptr alignment is unknown */
  FFF_OUTWORDADDR
};

struct FFFuncs fff_vobj_be = {
  "vobj-be",
  NULL,
  NULL,
  NULL,
  vobj_options,
  vobj_printhelp,
  vobj_headersize,
  vobjbe_identify,
  vobj_readconv,
  NULL,
  vobj_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  vobj_writeobject,
  vobj_writeshared,
  vobj_writeexec,
  bss_name,sbss_name,
  0,
  0, /* don't care */
  0,
  0,
  RTAB_UNDEF,0,
  _BIG_ENDIAN_,
  0,  /* defined by VOBJ bytespertaddr*bitsperbyte */
  0,  /* ptr alignment is unknown */
  FFF_OUTWORDADDR
};


static uint8_t *p;
static int bitspertaddr;
static const char *vobjcpu;
static int vobjcpuid,vobjver;



static void vobj_setcpu(const char *id)
{
  static const char *cpus[] = {  /* MUST be same order as in cpurelocs.h! */
    "PowerPC"
  };
  static const int num_cpus = sizeof(cpus)/sizeof(cpus[0]);
  int i;

  if (vobjcpu!=NULL && strcmp(vobjcpu,id)!=0) {
    error(156,vobjcpu);  /* alternating definitions, keeping the first one */
    return;
  }
  vobjcpu = id;
  vobjcpuid = 0;
  for (i=0; i<num_cpus; i++) {
    if (!strcmp(vobjcpu,cpus[i]))
      vobjcpuid = i + 1;
  }
}


static int vobj_options(struct GlobalVars *gv,
                        int argc,const char *argv[],int *i)
{
  if (!strcmp(argv[*i],"-vobj2")) {
    vobjver = 1<<2;
  }
  else if (!strcmp(argv[*i],"-vobjcpu")) {
    vobj_setcpu(get_arg(argc,argv,i));  /* set vobjcpu and vobjcpuid */
  }
  else return 0;

  return 1;
}


static void vobj_printhelp(void)
{
  printf("-vobj2            use v2 format, which generally reduces file size\n"
         "-vobjcpu <name>   define CPU name, required for spec. relocs with -r\n");
}


static unsigned long vobj_headersize(struct GlobalVars *gv)
{
  return 0;  /* irrelevant - not used for executable files */
}



/*****************************************************************/
/*                           Read VOBJ                           */
/*****************************************************************/

static taddr read_number(int is_signed)
{
  taddr val;
  uint8_t n;
  int i;

  if ((n = *p++) <= 0x7f)
    return (taddr)n;

  if (n >= 0xc0) {  /* v2 negative numbers */
    n -= 0xc0;
    for (i=0,val=~makemask(n*8); n--; i+=8)
      val |= (taddr)*p++ << i;
  }
  else {
    for (i=0,n-=0x80,val=0; n--; i+=8)
      val |= (taddr)*p++ << i;
    if (is_signed)
      val = sign_extend(val,bitspertaddr);  /* v1 negative numbers support */
  }

  return val;
}


static void skip_string(void)
{
  while (*p)
    p++;
  p++;
}


static int vobj_identify(struct GlobalVars *gv,struct FFFuncs *fff,char *name,
                         uint8_t *dat,unsigned long plen,uint8_t e)
{
  struct ar_info ai;
  int id = ID_OBJECT;

  if (ar_init(&ai,(char *)dat,plen,name)) {
    /* library archive detected, extract 1st archive member */
    id = ID_LIBARCH;
    if (!(ar_extract(&ai))) {
      error(38,name);  /* empty archive ignored */
      return ID_IGNORE;
    }
    p = (uint8_t *)ai.data;
    plen = ai.size;
  }
  else
    p = dat;

  if (plen>8 && p[0]==0x56 && p[1]==0x4f && p[2]==0x42 &&
      p[3]==0x4a && (p[4]&3)==e && (p[4]>>2)+1<=VOBJ_MAX_VERSION) {
    int bpt,bpb;

    p += 5;
    bpb = (int)read_number(0);  /* bits per byte */
    if ((bpb & 7) != 0) {
      /* bits per byte are not supported */
      error(113,name,fff->tname,bpb);
    }

    if (gv->bits_per_tbyte == 0)
      gv->bits_per_tbyte = bpb;
    else if (gv->bits_per_tbyte != bpb)
      error(146,name,fff->tname,(int)gv->bits_per_tbyte,bpb);

    bpt = (int)read_number(0);  /* bytes per taddr */
    if ((bpt*bpb+7)/8 > sizeof(taddr)) {
      /* n bytes per target-address are not supported */
      error(114,name,fff->tname,bpt);
    }

    if (gv->tbytes_per_taddr == 0)
      gv->tbytes_per_taddr = bpt;
    else if (gv->tbytes_per_taddr != bpt)
      error(147,name,fff->tname,(int)gv->tbytes_per_taddr,bpt);

    if (bpt * bpb > (int)gv->bits_per_taddr)
      gv->bits_per_taddr = bpt * bpb;  /* set bits per taddr from this VOBJ */

    vobj_setcpu((const char *)p);
    return id;
  }

  return ID_UNKNOWN;
}

static int vobjle_identify(struct GlobalVars *gv,char *name,uint8_t *dat,
                           unsigned long plen,bool lib)
{
  return vobj_identify(gv,&fff_vobj_le,name,dat,plen,2);
}

static int vobjbe_identify(struct GlobalVars *gv,char *name,uint8_t *dat,
                           unsigned long plen,bool lib)
{
  return vobj_identify(gv,&fff_vobj_be,name,dat,plen,1);
}


static void vobj_check_ar_type(struct GlobalVars *gv,struct FFFuncs *ff,
                               const char *name,uint8_t *dat)
/* check all library archive members before conversion */
{
  int bpt,bpb;

  p = dat;
  if (p[0]==0x56 && p[1]==0x4f && p[2]==0x42 && p[3]==0x4a &&
      (p[4]&3)==(ff->endianness ? 1 : 2) &&
      (p[4]>>2)+1<=VOBJ_MAX_VERSION) {
    p += 5;
    bpb = (int)read_number(0);  /* bits per byte */
    if ((bpb & 7) != 0) {
      /* bits per byte are not supported */
      error(113,name,ff->tname,bpb);
    }
    if (gv->bits_per_tbyte != bpb)
      error(146,name,ff->tname,(int)gv->bits_per_tbyte,bpb);

    bpt = (int)read_number(0);  /* bytes per taddr */
    if ((bpt*bpb+7)/8 > sizeof(taddr)) {
      /* n bytes per target-address are not supported */
      error(114,name,ff->tname,bpt);
    }
    if (gv->tbytes_per_taddr != bpt)
      error(147,name,ff->tname,(int)gv->tbytes_per_taddr,bpt);

    if (bpt * bpb > (int)gv->bits_per_taddr)
      gv->bits_per_taddr = bpt * bpb;  /* set bits per taddr from this VOBJ */

    vobj_setcpu((const char *)p);
  }
  else
    error(41,name,ff->tname);
}


static void read_symbol(struct vobj_symbol *vsym)
{
  vsym->name = (char *)p;
  skip_string();
  vsym->type = (int)read_number(0);
  vsym->flags = (int)read_number(0);
  vsym->sec = (int)read_number(0);
  vsym->val = read_number(1);
  vsym->size = (int)read_number(0);
}


static void read_section(struct GlobalVars *gv,struct ObjectUnit *u,
                         uint32_t index,struct vobj_symbol *vsyms,int nsyms)
{
  struct Section *s;
  lword dsize,fsize;
  int nrelocs;
  uint8_t type = ST_DATA;
  uint8_t prot = SP_READ;
  uint8_t flags = 0;
  uint8_t align,*data;
  char *attr;
  char *name = (char *)p;
  struct Reloc *last_reloc;
  int last_sym = -1;
  lword last_offs;

  skip_string();  /* section name */
  for (attr=(char *)p; *attr; attr++) {
    switch (*attr) {
      case 'w': prot |= SP_WRITE; break;
      case 'x': type = ST_CODE; prot |= SP_EXEC; break;
      case 'c': type = ST_CODE; prot |= SP_EXEC; break;
      case 'd': type = ST_DATA; break;
      case 'u': type = ST_UDATA; flags |= SF_UNINITIALIZED; break;
      case 'z': if (!strcmp(vobjcpu,"6502")) flags |= SF_DPAGE; break;
      case 'a': flags |= SF_ALLOC; break;
      case 'N': if (!strcmp(vobjcpu,"6502")) flags |= SF_SMALLDATA; break;
      case 'H': if (!strcmp(vobjcpu,"6502")) flags |= SF_HUGEDATA; break;
    }
  }
  skip_string();
  read_number(0);                 /* ignore flags */
  align = (uint8_t)lshiftcnt(read_number(0));
  dsize = read_number(0);         /* total size of section */
  nrelocs = (int)read_number(0);  /* number of relocation entries */
  fsize = read_number(0);         /* size in file, without 0-bytes */

  if (type == ST_UDATA) {
    data = NULL;
  }
  else if (dsize > fsize) {       /* recreate 0-bytes at end of section */
    data = alloczero(tbytes(gv,dsize));
    section_copy(gv,data,0,p,fsize);
  }
  else
    data = p;

  /* create and add section */
  p += tbytes(gv,fsize);
  s = add_section(u,name,data,(unsigned long)dsize,type,flags,prot,align,0);
  s->id = index;

  /* create relocations and unknown symbol references for this section */
  for (last_reloc=NULL,last_offs=-1; nrelocs>0; nrelocs--) {
    struct Reloc *r;
    size_t record_len = 0;
    char *xrefname = NULL;
    lword offs,mask,addend;
    uint16_t bpos,bsiz;
    unsigned flags;
    int rtype;
    int sym_idx;

    /* read one relocation entry */
    rtype = read_number(0);
    if (rtype >= FIRST_CPU_RELOC)
      record_len = read_number(0);  /* 0 for nreloc */

    if (record_len == 0) {
      offs = read_number(0);
      bpos = (uint16_t)read_number(0);
      bsiz = (uint16_t)read_number(0);
      mask = read_number(1);
      addend = read_number(1);
      sym_idx = (int)read_number(0) - 1;  /* symbol index */
      flags = 0;

      if (offs<0 || bsiz>(sizeof(lword)<<3) || sym_idx<0 || sym_idx>=nsyms)
        goto bad_reloc;

      if (rtype < FIRST_CPU_RELOC) {
        if (rtype & VOBJ_REL_S)
          flags |= RELF_S;
        else if (rtype & VOBJ_REL_U)
          flags |= RELF_U;
        rtype = STD_REL_TYPE(rtype);
      }
      else {
        if (vobjcpuid)
          rtype = MAKE_RELOC_CPU_ID(vobjcpuid) + rtype - FIRST_CPU_RELOC;
        else
          goto bad_reloc;
      }

      if (vsyms[sym_idx].flags & WEAK) {
        xrefname = vsyms[sym_idx].name;
        index = 0;
        flags |= RELF_WEAK;
      }
      else if (vsyms[sym_idx].type == LABSYM) {
        xrefname = NULL;
        index = vsyms[sym_idx].sec;
      }
      else if (vsyms[sym_idx].type == IMPORT) {
        xrefname = vsyms[sym_idx].name;
        index = 0;
      }
      else
        goto bad_reloc;

      if (sym_idx==last_sym && offs==last_offs && last_reloc!=NULL) {
        r = last_reloc;
      }
      else {
        r = newreloc(gv,s,xrefname,NULL,index,(unsigned long)offs,rtype,addend);
        r->flags |= flags;
        last_reloc = r;
        last_offs = offs;
        last_sym = sym_idx;
      }
      addreloc(s,r,bpos,bsiz,mask);

      /* make sure that section reflects the addend for other formats */
      writesection(gv,data,offs,r,addend);
    }
    else {
      /* VOBJ relocation not supported */
      bad_reloc:
      error(115,getobjname(u),fff[u->lnkfile->format]->tname,
            rtype,(unsigned long long)offs,
            (int)bpos,(int)bsiz,(unsigned long long)mask,
            (sym_idx>=0&&sym_idx<nsyms) ? vsyms[sym_idx].name : "?",
            (sym_idx>=0&&sym_idx<nsyms) ? (int)vsyms[sym_idx].type : 0);
    }
  }
}


static void vobj_read(struct GlobalVars *gv,struct LinkFile *lf,uint8_t *data)
{
  struct ObjectUnit *u;
  int nsecs,nsyms,i;
  struct vobj_symbol *vsymbols = NULL;

  if (lf->type == ID_LIBARCH) {  /* check ar-member for correct format */
    vobj_check_ar_type(gv,fff[lf->format],lf->pathname,data);
  }
  p = data + 5;  /* skip ID and endianness */
  bitspertaddr = gv->bits_per_taddr;

  /* skip bits per byte and bytes per address */
  read_number(0);
  read_number(0);

  skip_string();  /* skip cpu-string */

  u = create_objunit(gv,lf,lf->objname);
  nsecs = (int)read_number(0);  /* number of sections */
  nsyms = (int)read_number(0);  /* number of symbols */

  if (nsyms) {
    vsymbols = alloc(nsyms * sizeof(struct vobj_symbol));
    for (i=0; i<nsyms; i++)
      read_symbol(&vsymbols[i]);
  }

  for (i=1; i<=nsecs; i++)
    read_section(gv,u,(uint32_t)i,vsymbols,nsyms);

  /* add relocatable and absolute symbols, ignore unknown symbol-refs */
  for (i=0; i<nsyms; i++) {
    struct vobj_symbol *vs = &vsymbols[i];
    struct Section *s = NULL;
    uint8_t type,bind,info;

    if (vs->flags & WEAK)
      bind = SYMB_WEAK;
    else if (vs->flags & EXPORT)
      bind = SYMB_GLOBAL;
    else
      bind = SYMB_LOCAL;

    if (vs->flags & COMMON) {
      type = SYM_COMMON;
      bind = SYMB_GLOBAL;  /* common symbols are always global */
      s = common_section(gv,u);
    }
    else if (vs->type == EXPRESSION) {
      type = SYM_ABS;
      s = abs_section(u);
    }
    else if (vs->type == LABSYM) {
      type = SYM_RELOC;
      if (!(s = find_sect_id(u,vs->sec))) {
        /* a section with this index doesn't exist! */
        error(53,lf->pathname,vs->name,lf->objname,vs->sec);
      }
    }
    else if (vs->type == IMPORT) {
      type = 0;  /* ignore unknown symbols */
    }
    else {
      /* illegal symbol type */
      error(116,getobjname(u),fff[lf->format]->tname,
            vs->type,vs->name,lf->objname);
      type = 0;
    }

    switch (TYPE(vs)) {
      case TYPE_UNKNOWN: info = SYMI_NOTYPE; break;
      case TYPE_OBJECT: info = SYMI_OBJECT; break;
      case TYPE_FUNCTION: info = SYMI_FUNC; break;
      case TYPE_SECTION: type = 0; break;  /* ignore SECTION symbols */
      case TYPE_FILE: info = SYMI_FILE; break;
      default:
        error(54,lf->pathname,TYPE(vs),vs->name,lf->objname);
        type = 0;
        break;
    }

    if (type) {
      if (bind == SYMB_LOCAL)
        addlocsymbol(gv,s,vs->name,NULL,(lword)vs->val,type,0,info,vs->size);
      else
        addsymbol(gv,s,vs->name,NULL,(lword)vs->val,
                  type,0,info,bind,vs->size,TRUE);
    }
  }
  if (nsyms)
    free(vsymbols);

  add_objunit(gv,u,TRUE);  /* add object unit and fix relocations */
}


static void vobj_readconv(struct GlobalVars *gv,struct LinkFile *lf)
{
  struct ar_info ai;

  if (lf->type == ID_LIBARCH) {
    if (ar_init(&ai,(char *)lf->data,lf->length,lf->filename)) {
      while (ar_extract(&ai)) {
        lf->objname = allocstring(ai.name);
        vobj_read(gv,lf,(uint8_t *)ai.data);
      }
    }
    else
      ierror("vobj_readconv(): archive %s corrupted since last access",
             lf->pathname);
  }
  else {
    lf->objname = lf->filename;
    vobj_read(gv,lf,lf->data);
  }
}


static int vobj_targetlink(struct GlobalVars *gv,struct LinkedSection *ls,
                             struct Section *s)
/* returns 1, if target requires the combination of the two sections, */
/* returns -1, if target doesn't want to combine them, */
/* returns 0, if target doesn't care - standard linking rules are used. */
{
  return 0;
}



/*****************************************************************/
/*                          Write VOBJ                           */
/*****************************************************************/


static void fwrite_number(FILE *f,taddr val)
{
  int i,s,u;
  taddr tmp;

  if (val>=0 && val<=127) {
    fwrite8(f,val);
    return;
  }
  
  if (!vobjver) {
    s = u = sizeof(taddr);
  }
  else {
    for (i=1,s=u=1,tmp=val; i<=sizeof(taddr); i++) {
      if (tmp & 0xff)
        s = i;
      if ((tmp & 0xff) != 0xff)
        u = i;
      tmp >>= 8;
    }
  }

  if (u < s) {
    fwrite8(f,0xC0+u);
    s = u;
  }
  else
    fwrite8(f,0x80+s);

  for (i=s; i>0; i--) {
    fwrite8(f,val&0xff);
    val >>= 8;
  }
}


static void fwrite_string(FILE *f,const char *str)
{
  if (str)
    fwritex(f,str,strlen(str)+1);
}


static int vobj_initwrite(struct GlobalVars *gv)
{
  struct LinkedSection *ls;
  struct Symbol *sym;
  struct Reloc *xref;
  int cnt = gv->nsecs;  /* make room for nsecs section symbols */

  /* count symbols and unresolved references */
  for (ls=(struct LinkedSection *)gv->lnksec.first;
       ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {

    for (xref=(struct Reloc *)ls->xrefs.first;
         xref->n.next!=NULL; xref=(struct Reloc *)xref->n.next) {
      sym = findsymbol(gv,NULL,xref->xrefname,0);
      if (sym == NULL) {
        /* add new undefined symbol to current section */
        sym = newsymbol(xref->xrefname,0,SYM_UNDEF,0,0,SYMB_GLOBAL,0);
        addglobsym(gv,sym);
        addtail(&ls->symbols,&sym->n);
      }
      xref->relocsect.symbol = sym;  /* "resolve" with undefined symbol */
    }

    for (sym=(struct Symbol *)ls->symbols.first;
         sym->n.next!=NULL; sym=(struct Symbol *)sym->n.next) {
      if (!discard_symbol(gv,sym) && sym->info!=SYMI_SECTION)
        sym->extra = ++cnt;
      else
        sym->extra = 0;
    }
  }
  return cnt;
}


static void vobj_header(struct GlobalVars *gv,FILE *f)
{
  int endian = fff[gv->dest_format]->endianness==_BIG_ENDIAN_ ? 1 : 2;

  fwrite32be(f,0x564f424a);  /* "VOBJ" */
  fwrite_number(f,endian|vobjver);
  fwrite_number(f,gv->bits_per_tbyte);
  fwrite_number(f,gv->tbytes_per_taddr);
  if (vobjcpu == NULL) {
    fwrite_string(f,"generic");
    vobjcpuid = 0;
    error(154);  /* No CPU defined for VOBJ output */
  }
  else
    fwrite_string(f,vobjcpu);
}


static int vobj_symbols(struct GlobalVars *gv,FILE *f)
{
  static const char *fn = "vobj_symbols(): ";
  struct LinkedSection *ls;
  struct Symbol *sym;
  unsigned flags,secid,cnt;

  /* create section symbols first */
  for (cnt=0,ls=(struct LinkedSection *)gv->lnksec.first;
       ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
    if (cnt != ls->index)
      ierror("%sSection symbol %s index %u doesn't match section index %u",
             fn,ls->name,(unsigned)cnt,(unsigned)ls->index);
    fwrite_string(f,ls->name);
    fwrite_number(f,LABSYM);
    fwrite_number(f,TYPE_SECTION);
    fwrite_number(f,++cnt);
    fwrite_number(f,0);
    fwrite_number(f,0);
  }

  for (ls=(struct LinkedSection *)gv->lnksec.first;
       ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
    for (sym=(struct Symbol *)ls->symbols.first;
         sym->n.next!=NULL; sym=(struct Symbol *)sym->n.next) {

      if (sym->extra) {
        if ((unsigned)sym->extra != ++cnt)
          ierror("%sSymbol counter wrong at %s (%u!=%u)",
                 fn,sym->name,(unsigned)sym->extra,cnt);

        fwrite_string(f,sym->name);
        flags = secid = 0;

        switch (sym->type) {
          case SYM_COMMON:
            flags |= COMMON|TYPE_OBJECT;
          case SYM_UNDEF:
            fwrite_number(f,IMPORT);
            break;
          case SYM_ABS:
            fwrite_number(f,EXPRESSION);
            break;
          case SYM_RELOC:
            fwrite_number(f,LABSYM);
            if (sym->relsect!=NULL && sym->relsect->lnksec!=NULL)
              secid = sym->relsect->lnksec->index + 1;
            else
              ierror("%sNo lnksec for reloc symbol %s",fn,sym->name);
            break;
          case SYM_INDIR:
            error(155,sym->name,sym->indir_name,fff[gv->dest_format]->tname);
            break;
          default:
            ierror("%sIllegal symbol type %d",fn,(int)sym->type);
            break;
        }
        switch (sym->bind) {
          case SYMB_GLOBAL:
            flags |= EXPORT;
            break;
          case SYMB_WEAK:
            flags |= WEAK;
            break;
        }
        switch (sym->info) {
          case SYMI_OBJECT:
            flags |= TYPE_OBJECT;
            break;
          case SYMI_FUNC:
            flags |= TYPE_FUNCTION;
            break;
          case SYMI_SECTION:
            flags |= TYPE_SECTION;
            break;
          case SYMI_FILE:
            flags |= TYPE_FILE;
            break;
        }

        fwrite_number(f,flags);
        fwrite_number(f,secid);
        fwrite_number(f,sym->value);
        fwrite_number(f,sym->size);
      }
    }
  }
  return cnt;
}


static int write_reloc(struct GlobalVars *gv,FILE *f,
                       struct LinkedSection *ls,struct Reloc *r)
{
  struct RelocInsert *ri = r->insert;
  int rtype = r->rtype;
  int cnt = 0;

  if (vobjcpuid && rtype>=MAKE_RELOC_CPU_ID(vobjcpuid)) {
    /* cpu-specific reloc type is mapped to FIRST_CPU_RELOC */
    rtype = rtype - MAKE_RELOC_CPU_ID(vobjcpuid) + FIRST_CPU_RELOC;
  }
  else {
    /* standard reloc types may have Signed/Unsigned flags */
    if (f!=NULL && rtype>LAST_STANDARD_RELOC)
      error(14,gv->dest_name,fff[gv->dest_format]->tname,rtype,
            ls->name,(unsigned)r->offset);  /* illegal relocation type */

    if (r->flags & RELF_S)
      rtype |= VOBJ_REL_S;
    else if (r->flags & RELF_U)
      rtype |= VOBJ_REL_U;
  }

  if (1 /* @@@ check if cpu-specific relocation is nreloc */) {
    while (ri) {
      if (f != NULL) {
        fwrite_number(f,rtype);
        if (rtype >= FIRST_CPU_RELOC)
          fwrite_number(f,0); /* indicate specific reloc is in nreloc format */
        fwrite_number(f,r->offset);
        fwrite_number(f,ri->bpos);
        fwrite_number(f,ri->bsiz);
        fwrite_number(f,ri->mask);
        fwrite_number(f,r->addend);
        if (r->xrefname == NULL)
          fwrite_number(f,r->relocsect.lnk->index+1);  /* section symbol */
        else
          fwrite_number(f,r->relocsect.symbol->extra); /* xref symbol */
      }
      cnt++;
      ri = ri->next;
    }
  }
  else {
    /* @@@ write cpu-specific relocation */
  }
  return cnt;
}


static int vobj_relocs(struct GlobalVars *gv,FILE *f,struct LinkedSection *ls)
{
  struct Reloc *r;
  int cnt = 0;

  for (r=(struct Reloc *)ls->relocs.first;
       r->n.next!=NULL; r=(struct Reloc *)r->n.next)
    cnt += write_reloc(gv,f,ls,r);

  for (r=(struct Reloc *)ls->xrefs.first;
       r->n.next!=NULL; r=(struct Reloc *)r->n.next)
    cnt += write_reloc(gv,f,ls,r);

  return cnt;
}


static void vobj_section(struct GlobalVars *gv,FILE *f,struct LinkedSection *ls)
{
  static const char *fn = "vobj_section(): ";
  unsigned flags = 0;
  char attr[16],*p;

  fwrite_string(f,ls->name);

  p = attr;
  if (ls->flags & SF_ALLOC)
    *p++ = 'a';
  else
    flags |= UNALLOCATED;

  if ((ls->flags&SF_UNINITIALIZED) || ls->type==ST_UDATA)
    *p++ = 'u';
  else if (ls->type == ST_DATA)
    *p++ = 'd';
  else if (ls->type == ST_CODE)
    *p++ = 'c';
  else
    ierror("%sUnsupported section type %u",(unsigned)ls->type);

  if (ls->protection & SP_READ)
    *p++ = 'r';
  if (ls->protection & SP_WRITE)
    *p++ = 'w';
  if (ls->protection & SP_EXEC)
    *p++ = 'x';

  *p = '\0';
  fwrite_string(f,attr);

  fwrite_number(f,flags);
  fwrite_number(f,1LL<<ls->alignment);
  fwrite_number(f,ls->size);
  fwrite_number(f,vobj_relocs(gv,NULL,ls));

  fwrite_number(f,ls->filesize);
  fwriterawsect(gv,f,ls);

  vobj_relocs(gv,f,ls);
}


static void vobj_writeexec(struct GlobalVars *gv,FILE *f)
{
  error(94);  /* Target file format doesn't support executable files */
}


static void vobj_writeshared(struct GlobalVars *gv,FILE *f)
{
  error(30);  /* Target file format doesn't support shared objects */
}


static void vobj_writeobject(struct GlobalVars *gv,FILE *f)
{
  struct LinkedSection *ls;
  int nsyms;

  nsyms = vobj_initwrite(gv);
  vobj_header(gv,f);
  fwrite_number(f,gv->nsecs);
  fwrite_number(f,nsyms);

  if (vobj_symbols(gv,f) != nsyms)
    ierror("vobj: didn't write %d symbols as expected",nsyms);

  for (ls=(struct LinkedSection *)gv->lnksec.first;
       ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next)
    vobj_section(gv,f,ls);
}


#endif

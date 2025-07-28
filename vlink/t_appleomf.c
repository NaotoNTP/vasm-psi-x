/* $VER: vlink t_appleomf.c V0.18 (23.12.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 2024  Frank Wille
 */

#include "config.h"
#ifdef APPLE_OMF
#define T_APPLEOMF_C
#include "vlink.h"
#include "appleomf.h"


static void init(struct GlobalVars *,int);
static int options(struct GlobalVars *,int,const char **,int *);
static void printhelp(void);
static int identify(struct GlobalVars *,char *,uint8_t *,unsigned long,bool);
static void readconv(struct GlobalVars *,struct LinkFile *);
static int omf_targetlink(struct GlobalVars *,struct LinkedSection *,
                          struct Section *);
static struct Symbol *omf_lnksym(struct GlobalVars *,struct Section *,
                                 struct Reloc *);
static void omf_setlnksym(struct GlobalVars *,struct Symbol *);
static unsigned long headersize(struct GlobalVars *);
static void writeobject(struct GlobalVars *,FILE *);
static void writeshared(struct GlobalVars *,FILE *);
static void writeexec(struct GlobalVars *,FILE *);


struct FFFuncs fff_appleomf = {
  "appleomf",
  NULL,
  NULL,
  init,
  options,
  printhelp,
  headersize,
  identify,
  readconv,
  NULL,
  omf_targetlink,
  NULL,
  omf_lnksym,
  omf_setlnksym,
  NULL,NULL,NULL,
  writeobject,
  writeshared,
  writeexec,
  NULL,NULL,
  OMF_BANKSIZE,
  0,  /* no small data */
  0,
  0,
  RTAB_STANDARD,RTAB_STANDARD,
  _LITTLE_ENDIAN_,
  24,1,
  FFF_RELOCATABLE
};

static int outver = 1;      /* OMF V1.0 is default */
static int numlen = 4;      /* defaults to 4 bytes per value */
static int nsegs,lablen,extrastack;
static unsigned long maxsegsize;
static struct list segrelocs;

/* linker symbols */
static char dbrinit_name[] = "__DBR_init";
#define DBRINIT 0


static void init(struct GlobalVars *gv,int mode)
{
  if (mode==FFINI_DESTFMT && !maxsegsize) {
    maxsegsize = fff[gv->dest_format]->page_size;
  }
  else if (mode == FFINI_MERGE) {
    /* set target-specific flags on all selected sections first */
    struct ObjectUnit *obj;
    struct Section *sec;

    for (obj=(struct ObjectUnit *)gv->selobjects.first;
         obj->n.next!=NULL; obj=(struct ObjectUnit *)obj->n.next) {
      if (obj->lnkfile->type != ID_SHAREDOBJ) {
        for (sec=(struct Section *)obj->sections.first;
             sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
          if (!(sec->flags & (SF_SMALLDATA|SF_DPAGE|SF_HUGEDATA))) {
            /* section data-model flag by name */
            if (strstr(sec->name,".zero") || strstr(sec->name,".dpage"))
              sec->flags |= SF_DPAGE;
            else if (strstr(sec->name,".near"))
              sec->flags |= SF_SMALLDATA;
            else if (strstr(sec->name,".huge"))
              sec->flags |= SF_HUGEDATA;
          }
        }
      }
    }
  }
}


static int options(struct GlobalVars *gv,int argc,const char **argv,int *i)
{
  if (!strncmp(argv[*i],"-maxsegsize=",12)) {
    int mss;

    if (sscanf(&argv[*i][12],"%i",&mss) == 1) {
      maxsegsize = mss;
      return 1;
    }
  }
  else if (!strncmp(argv[*i],"-stack=",7)) {
    if (sscanf(&argv[*i][7],"%i",&extrastack) == 1)
      return 1;
    else
      extrastack = 0;
  }
  else if (!strncmp(argv[*i],"-version=",9)) {
    if (sscanf(&argv[*i][9],"%i",&outver) == 1) {
      if (outver<1 || outver>2) {
        error(161,1,0);  /* assuming 1.0 */
        outver = 1;
      }
      return 1;
    }
  }
  return 0;
}


static void printhelp(void)
{
  printf("-maxsegsize=<size>maximum size for segments with multiple sections\n"
         "-stack=<size>     stack size to add to DP/Stack segment\n"
         "-version=<v>      use OMF version 1 (default) or 2\n");
}



/*****************************************************************/
/*                           Read OMF                            */
/*****************************************************************/


static int identify(struct GlobalVars *gv,char *name,uint8_t *p,
                    unsigned long plen,bool lib)
/* identify an XFile-format file */
{
  return ID_UNKNOWN;  /* @@@ no read-support at the moment */
}


static void readconv(struct GlobalVars *gv,struct LinkFile *lf)
{
  ierror("readconv(): Can't read Apple OMF");
}



/*****************************************************************/
/*                           Link OMF                            */
/*****************************************************************/


static int omf_targetlink(struct GlobalVars *gv,struct LinkedSection *ls,
                          struct Section *s)
/* returns 1, if target requires the combination of the two sections, */
/* returns -1, if target doesn't want to combine them, */
/* returns 0, if target doesn't care - standard linking rules are used. */
{
  unsigned long ssize;

  if (s->lnksec) {
    if (s->lnksec == ls)
      return -1;  /* shouldn't happen - section is already linked to ls */

    /* Section is already in output-section lnksec, so returning -1 would
       mean we merge with the full size of lnksec! */
    ssize = s->lnksec->size;
  }
  else
    ssize = s->size;

  if ((ls->flags&(SF_DPAGE|SF_SMALLDATA|SF_HUGEDATA)) !=
      (s->flags&(SF_DPAGE|SF_SMALLDATA|SF_HUGEDATA)))
    return -1; /* DP/Stack, Near or Huge segments must not merge with others */

  if ((s->flags & SF_DPAGE) && ls->size+ssize>OMF_MAXDPSTKSIZE)
    error(162,fff[gv->dest_format]->tname,ls->name,
          (unsigned long)OMF_MAXDPSTKSIZE);
  else if ((s->flags & SF_SMALLDATA) && ls->size+ssize>OMF_BANKSIZE)
    error(162,fff[gv->dest_format]->tname,ls->name,(unsigned long)OMF_BANKSIZE);
  else if (!(s->flags & SF_HUGEDATA) && ls->size+ssize>maxsegsize)
    return -1;  /* merging section would exceed the maximum segment size */

  return 0;  /* merge sections with same name and attributes */
}


static struct Symbol *omf_lnksym(struct GlobalVars *gv,struct Section *sec,
                                 struct Reloc *xref)
{
  struct Symbol *sym;

  if (!gv->dest_object && !gv->use_ldscript) {
    if (!strcmp(dbrinit_name,xref->xrefname)) {  /* __DBR_init */
      sym = addlnksymbol(gv,dbrinit_name,0,
                         SYM_ABS,SYMF_LNKSYM,SYMI_OBJECT,SYMB_GLOBAL,0);
      sym->extra = DBRINIT;
      return sym;  /* new linker symbol created */
    }
  }
  return NULL;
}


static void omf_setlnksym(struct GlobalVars *gv,struct Symbol *xdef)
{
  if (xdef->flags & SYMF_LNKSYM) {
    struct LinkedSection *ls;

    switch (xdef->extra) {
      case DBRINIT:
        /* base address of first Near (small-data) segment */
        if (ls = find_lnksec(gv,NULL,ST_DATA,SF_SMALLDATA,SF_SMALLDATA,0)) {
          xdef->type = SYM_RELOC;
          xdef->relsect = (struct Section *)ls->sections.first;
        }
        break;
    }
  }
}


/*****************************************************************/
/*                          Write OMF                            */
/*****************************************************************/


static void omf_initwrite(struct GlobalVars *gv)
{
  struct LinkedSection *ls,*dpstk;
  int i;

  /* assign OMF segment numbers 1..nsegs to valid sections, 0 to others */
  for (ls=(struct LinkedSection *)gv->lnksec.first,dpstk=NULL,nsegs=0;
       ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
    if ((ls->flags & SF_ALLOC) && ls->size!=0) {
      ls->index = ++nsegs;  /* OMF segment index */
      if (ls->type!=ST_CODE && (ls->flags & SF_DPAGE))
        dpstk = ls;  /* remember last DP section for stack */
    }
    else
      ls->index = 0;  /* section ignored */
  }

  if (extrastack) {
    /* add stack space at the end of the last DirectPage/Stack segment */
    if (dpstk == NULL) {
      dpstk = create_lnksect(gv,"stack",ST_DATA,SF_ALLOC|SF_DPAGE,
                             SP_READ|SP_WRITE,0,0);
      dpstk->data = (uint8_t *)dpstk;  /* any valid pointer, filesize is 0 */
      dpstk->index = ++nsegs;
    }
    dpstk->size += extrastack;
  }

  initlist(&segrelocs);
}


static void reset_segrelocs(void)
{
  void *r;

  while (r = (OMFReloc *)remhead(&segrelocs))
    free(r);
  initlist(&segrelocs);
}


static void collect_segrelocs(struct GlobalVars *gv,struct LinkedSection *ls)
{
  struct Reloc *rel;
  struct RelocInsert *ri;
  int i;

  reset_segrelocs();
  sort_relocs(&ls->relocs);

  for (rel=(struct Reloc *)ls->relocs.first; rel->n.next!=NULL;
       rel=(struct Reloc *)rel->n.next) {
    if (ri = rel->insert) {
      int rsh = lshiftcnt(ri->mask);
      lword vmask = makemask(ri->bsiz) << rsh;

      if (rel->rtype==R_ABS && ri->next==NULL && ri->bpos==0 && !(ri->bsiz&7)
          && (ri->mask&vmask)==vmask && rel->relocsect.lnk->index) {
        OMFReloc *omfr = alloc(sizeof(OMFReloc));

        omfr->flags = 0;
        omfr->fileno = 1;  /* @@@ for plain load files */
        omfr->segno = rel->relocsect.lnk->index;
        omfr->size = ri->bsiz / 8;
        omfr->shift = -rsh;
        omfr->offset = rel->offset;
        omfr->addend = rel->addend;
        addtail(&segrelocs,&omfr->n);
      }
      else
        error(32,fff[gv->dest_format]->tname,reloc_name[rel->rtype],
              (int)ri->bpos,(int)ri->bsiz,mtaddr(gv,ri->mask),
              ls->name,rel->offset);
    }
    else
      ierror("collect_segrelocs: missing RelocInsert for rtype %d at %s+%lu",
             (int)rel->rtype,ls->name,rel->offset);
  }
}


static void fwritesuper(struct GlobalVars *gv,FILE *f,uint8_t oc,
                        uint32_t sz,uint8_t type)
{
  /* write SUPER record header */
  fwrite8(f,oc);
  fwrite32(fff[gv->dest_format]->endianness,f,sz);
  fwrite8(f,type);
}


static size_t omf_relocs(struct GlobalVars *gv,struct LinkedSection *ls,FILE *f)
{
  static const char *fn = "omf_relocs(): ";
  OMFReloc *r,*next;
  size_t total = 0;
  int omfendian = fff[gv->dest_format]->endianness;
  int i,cnt;

  if (outver > 1) {
    /* try to write SUPER compressed relocs */
    struct list superlist;
    int fno,sno,sh,sz,page;

    for (i=0; i<38; i++) {  /* find relocs for all types */
      initlist(&superlist);
      fno = (i>2 && i<14) ? i-1 : 1;
      if (i >= 14)
        sno = 1 + ((i-14) % 12);
      else
        sno = (i < 2) ? ls->index : 0;  /* 0 = any segno from 0 to 255 */
      sh = (i >= 26) ? -16 : 0;         /* INTERSEG25..36 for bitshift -16 */
      cnt = (i>0 && i<14) ? 3 : 2;
      sz = 0;
      r = (OMFReloc *)segrelocs.first;

      while (next = (OMFReloc *)r->n.next) {
        if ((f!=NULL || !(r->flags&OMFRF_DONE)) &&
            r->fileno==fno && r->size==cnt && r->shift==sh &&
            ((!sno && r->segno<256) || (r->segno == sno)) &&
            r->offset<=0xffff && r->addend>=0 && r->addend<=0xffff) {
          if (f) {
            addtail(&superlist,remnode(&r->n));
          }
          else {
            /* patch addend and ref-segment into the current segment data */
            write16(omfendian,ls->data+r->offset,r->addend>>(-sh));
            if (cnt == 3)
              ls->data[r->offset+2] = r->segno;
            r->flags |= OMFRF_SUPER;
          }

          if (!sz) {
            sz = 2;
            if (r->offset>>8 != 0) {
              sz++;
              page = r->offset >> 8;
            }
            else
              page = 0;
          }
          else if (r->offset>>8 != page) {
            sz += (r->offset>>8 != page+1) ? 2 : 1;
            page = r->offset >> 8;
          }
          sz++;
        }
        r = next;
      }
      if (sz)
        total += 5 + sz;

      if (f!=NULL && sz) {
        fwritesuper(gv,f,OMFOC_SUPER,sz,i);
        page = -1;
        while (r = (OMFReloc *)remhead(&superlist)) {
          for (next=(OMFReloc *)superlist.first,cnt=0;
               next->n.next!=NULL && next->offset>>8==r->offset>>8;
               next=(OMFReloc *)next->n.next)
            cnt++;
          if (++page != r->offset>>8) {
            fwrite8(f,0x80+((r->offset>>8)-page));
            page = r->offset >> 8;
          }
          fwrite8(f,cnt);
          fwrite8(f,r->offset&0xff);
          free(r);
          while (cnt--) {
            r = (OMFReloc *)remhead(&superlist);
            if (r == NULL)
              ierror("%smissing %d super relocs",fn,cnt+1);
            fwrite8(f,r->offset&0xff);
            free(r);
          }
        }
      }
    }
  }

  /* looking for cRELOC- and RELOC-type relocactions */
  for (i=0; i<=1; i++) {
    static const uint8_t opc[2] = { OMFOC_cRELOC, OMFOC_RELOC };

    r = (OMFReloc *)segrelocs.first;
    while (next = (OMFReloc *)r->n.next) {
      if ((f!=NULL || !(r->flags&OMFRF_DONE)) &&
          r->fileno==1 && r->segno==ls->index &&
          (i || (r->offset<=0xffff && r->addend>=0 && r->addend<=0xffff))) {
        if (f) {
          fwrite8(f,opc[i]);
          fwrite8(f,r->size);
          fwrite8(f,r->shift);
          if (i) {
            fwrite32(omfendian,f,r->offset);
            fwrite32(omfendian,f,r->addend);
          }
          else {
            fwrite16(omfendian,f,r->offset);
            fwrite16(omfendian,f,r->addend);
          }
          free(remnode(&r->n));
        }
        else
          r->flags |= OMFRF_RELOC;
        total += 7 + i*4;
      }
      r = next;
    }
  }

  /* looking for cINTERSEG- and INTERSEG-type relocactions */
  for (i=0; i<=1; i++) {
    static const uint8_t opc[2] = { OMFOC_cINTERSEG, OMFOC_INTERSEG };

    r = (OMFReloc *)segrelocs.first;
    while (next = (OMFReloc *)r->n.next) {
      if ((f!=NULL || !(r->flags&OMFRF_DONE)) &&
          (i || (r->fileno==1 && r->segno<256 &&
                 r->offset<=0xffff && r->addend>=0 && r->addend<=0xffff))) {
        if (f) {
          fwrite8(f,opc[i]);
          fwrite8(f,r->size);
          fwrite8(f,r->shift);
          if (i) {
            fwrite32(omfendian,f,r->offset);
            fwrite16(omfendian,f,r->fileno);
            fwrite16(omfendian,f,r->segno);
            fwrite32(omfendian,f,r->addend);
          }
          else {
            fwrite16(omfendian,f,r->offset);
            fwrite8(f,r->segno);
            fwrite16(omfendian,f,r->addend);
          }
          free(remnode(&r->n));
        }
        else
          r->flags |= OMFRF_INTERSEG;
        total += 8 + i*7;
      }
      r = next;
    }
  }

  if (f!=NULL && !listempty(&segrelocs))
    ierror("%ssome segrelocs were left unprocessed");

  return total;
}


static size_t omf_lablen(const char *name)
{
  return lablen ? lablen : 1+strlen(name);
}


static void fwritelabname(FILE *f,const char *name)
{
  size_t len = strlen(name);

  if (lablen) {
    if (len < lablen) {
      fwritex(f,name,len);
      for (; len<lablen; len++)
        fwrite8(f,' ');  /* pad with blanks */
    }
    else
      fwritex(f,name,lablen);
  }
  else {
    fwrite8(f,len);
    fwritex(f,name,len);
  }
}


/* FIXME! */
static uint16_t omf_kind(struct GlobalVars *gv,struct LinkedSection *ls)
{
  uint16_t k;

  k = ls->type==ST_CODE ? SEGT_CODE : SEGT_DATA;
  if (k==SEGT_DATA && (ls->flags & SF_DPAGE))
    k = SEGT_DPSTACK;  /* zero/direct-page section */

  /* @@@ attributes? */
  return k;
}


static uint32_t omf_align(struct GlobalVars *gv,struct LinkedSection *ls)
{
  uint32_t a = 1L << ls->alignment;

  /* FIXME! @@@ implements restrictions of the Apple IIGS loader only */
  if (a <= 1)
    return 0;
  return (a <= 0x100) ? 0x100 : OMF_BANKSIZE;
}


static void fwriteloadname(FILE *f,const char *name)
{
  char buf[12];
  size_t len;

  /* copy loadname and fill rest of buffer with blanks */
  strncpy(buf,name,10);
  buf[10] = '\0';
  len = strlen(buf);
  if (len < 10)
    memset(buf+len,' ',10-len);
  fwritex(f,buf,10);
}


static void omf_seghdr(struct GlobalVars *gv,struct LinkedSection *ls,
                       FILE *f,uint32_t org,size_t length,const char *lname)
{
  int omfendian = fff[gv->dest_format]->endianness;

  if (outver < 2)
    fwrite32(omfendian,f,(length+511)/512);  /* size in 512-byte blocks */
  else
    fwrite32(omfendian,f,length);
  fwrite32(omfendian,f,ls->size-ls->filesize);  /* RESSPC */
  fwrite32(omfendian,f,ls->size);  /* LENGTH */
  if (outver < 2) {
    uint16_t kind = omf_kind(gv,ls);

    if (kind & SEGA_ABSBANK)
      kind = (kind&0xff00) | SEGT_INIT | SEGT_DATA;  /* v1 absolute bank */
    fwrite8(f,((kind&(SEGA_DYNAMIC|SEGA_PRIVATE|SEGA_PIC))>>8)|(kind&0x1f));
  }
  else
    fwrite8(f,0);
  fwrite8(f,lablen);
  fwrite8(f,numlen);
  fwrite8(f,outver);
  if (ls->flags & SF_HUGEDATA)
    fwrite32(omfendian,f,0);  /* may cross bank boundaries */
  else
    fwrite32(omfendian,f,fff[gv->dest_format]->page_size);  /* BANKSIZE */
  if (outver > 1) {
    fwrite16(omfendian,f,omf_kind(gv,ls));  /* v2 KIND */
    fwritegap(gv,f,2,0);
  }
  else
    fwritegap(gv,f,4,0);
  fwrite32(omfendian,f,org);  /* ORG */
  fwrite32(omfendian,f,omf_align(gv,ls));  /* ALIGN */
  fwrite8(f,omfendian);  /* NUMSEX */
  fwrite8(f,0);  /* v1: LCBANK @@@ */
  fwrite16(omfendian,f,ls->index);  /* SEGNUM */
  fwrite32(omfendian,f,0);  /* FIXME! @@@ ENTRY */
  fwrite16(omfendian,f,offsetof(OMFSeghdr,loadname));  /* v1, v2 only */
  fwrite16(omfendian,f,sizeof(OMFSeghdr)+omf_lablen(ls->name));
  fwriteloadname(f,lname);  /* LOADNAME - 10 bytes */
  fwritelabname(f,ls->name);  /* SEGNAME */
}


static void omf_data(struct GlobalVars *gv,struct LinkedSection *ls,FILE *f)
{
  /* write LCONST opcode with file size, followed by section contents */
  if (ls->filesize) {
    fwrite8(f,OMFOC_LCONST);
    fwrite32(fff[gv->dest_format]->endianness,f,ls->filesize);
    fwritex(f,ls->data,ls->filesize);
  }
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
/* creates an OMF relocatable object file */
{
  ierror("Apple OMF object file generation has not yet been implemented");
}


static void writeexec(struct GlobalVars *gv,FILE *f)
/* creates an OMF load file (which is relocatable) */
{
  struct LinkedSection *ls;
  size_t sz;

  omf_initwrite(gv);

  for (ls=(struct LinkedSection *)gv->lnksec.first; ls->n.next!=NULL;
       ls=(struct LinkedSection *)ls->n.next) {
    if (ls->index) {
      collect_segrelocs(gv,ls);

      /* determine size of segment header + body (including relocs) */
      sz = sizeof(OMFSeghdr) + omf_lablen(ls->name);
      if (ls->filesize)
        sz += 5 + ls->filesize;
      sz += omf_relocs(gv,ls,NULL) + 1;  /* including END opcode */

      /* write segment */
      omf_seghdr(gv,ls,f,0,sz,noname);  /* org 0 = relocatable */
      omf_data(gv,ls,f);
      omf_relocs(gv,ls,f);
      fwrite8(f,OMFOC_END);

      if (outver<2 && (sz&511)) {
        /* pad to block boundaries in v1 format */
        fwritegap(gv,f,512-(sz&511),0);
      }
    }
  }
}

#endif

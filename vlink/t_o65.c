/* $VER: vlink t_o65.c V0.18 (09.06.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2024  Frank Wille
 */

#include "config.h"
#ifdef O65
#define T_O65_C
#include "vlink.h"
#include "o65.h"
#include <time.h>


static int options_02(struct GlobalVars *,int,const char **,int *);
static int options_816(struct GlobalVars *,int,const char **,int *);
static void printhelp_02(void);
static void printhelp_816(void);
static int identify_02(struct GlobalVars *,char *,uint8_t *,unsigned long,bool);
static int identify_816(struct GlobalVars *,char *,uint8_t *,unsigned long,bool);
static void readconv(struct GlobalVars *,struct LinkFile *);
static int targetlink(struct GlobalVars *,struct LinkedSection *,
                      struct Section *);
static unsigned long headersize(struct GlobalVars *);
static void writeobject_02(struct GlobalVars *,FILE *);
static void writeobject_816(struct GlobalVars *,FILE *);
static void writeshared(struct GlobalVars *,FILE *);
static void writeexec_02(struct GlobalVars *,FILE *);
static void writeexec_816(struct GlobalVars *,FILE *);


struct FFFuncs fff_o6502 = {
  "o65-02",
  NULL,
  NULL,
  NULL,
  options_02,
  printhelp_02,
  headersize,
  identify_02,
  readconv,
  NULL,
  targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  writeobject_02,
  writeshared,
  writeexec_02,
  bss_name,NULL,
  0,
  0, /* no small data */
  0,
  0,
  RTAB_STANDARD,RTAB_STANDARD,
  _LITTLE_ENDIAN_,
  16,0,
  FFF_BASEINCR
};

struct FFFuncs fff_o65816 = {
  "o65-816",
  NULL,
  NULL,
  NULL,
  options_816,
  printhelp_816,
  headersize,
  identify_816,
  readconv,
  NULL,
  targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  writeobject_816,
  writeshared,
  writeexec_816,
  bss_name,NULL,
  0,
  0, /* no small data */
  0,
  0,
  RTAB_STANDARD,RTAB_STANDARD,
  _LITTLE_ENDIAN_,
  24,0,
  FFF_BASEINCR
};

struct ImportList o65implist; /* list of externally referenced symbol names */

static int o65size;           /* words are 2 or 4 bytes */
static uint8_t *optr;         /* current object file pointer */
static long olen;             /* remaining bytes in object file */

static const uint8_t o65magic[] = { 1,0,'o','6','5',0 };

/* options */
static int o65outsize=2;      /* word-size for output file - always 2 */
static size_t o65stack;       /* highest stack size */
static uint8_t o65paged;      /* page-wise relocation */
static uint8_t o65align;      /* alignment 0,1,2 or 3 (o65paged) */
static uint8_t o65cpu;        /* 6502 cpu code */
static uint8_t o65bsszero;    /* bss must be zeroed by loader */

static int fopts;             /* bit 0: enable file options,
                                 bit 1: includes creation data */
static const char *fopt_name,*fopt_author;


static int check_cpu_name(const char *name)
{
  static const char *cpunames[] = {
    "6502","65c02","65sc02","65ce02","nmos6502","65816"
  };
  int i;

  for (i=0; i<sizeof(cpunames)/sizeof(cpunames[0]); i++) {
    if (!stricmp(name,cpunames[i]))
      return i;
  }
  return 0xff;
}


static int options(int cpu816,struct GlobalVars *gv,
                   int argc,const char *argv[],int *i)
{
  long val;
  int n;

  if (!strcmp(argv[*i],"-o65-align")) {
    uint8_t abits;

    n = sscanf(get_arg(argc,argv,i),"%li",&val);
    if (n!=1 || val<0 || (val>2 && val!=8))
      error(130,argv[*i-1]);  /* bad assignment */
    abits = val>2 ? 3 : val;
    if (abits > o65align)
      o65align = abits;
  }
  else if (!strcmp(argv[*i],"-o65-author") && (*i+1)<argc) {
    fopts |= 1;
    fopt_author = argv[++*i];
  }
  else if (!strcmp(argv[*i],"-o65-bsszero")) {
    o65bsszero = 1;
  }
  else if (!cpu816 && !strncmp(argv[*i],"-o65-cpu",8)) {
    if (argv[*i][8] == '=') {
      n = sscanf(&argv[*i][9],"%li",&val);
      if (n!=1 || val<0 || val>15)
        error(130,argv[*i]);  /* bad assignment */
      o65cpu = val;
    }
    else if (argv[*i][8]=='\0' && (*i+1)<argc) {
      if ((o65cpu = check_cpu_name(argv[++*i])) > 15)
        error(130,argv[*i]);  /* bad assignment */
    }
    else
      return 0;
  }
  else if (!strcmp(argv[*i],"-o65-fopts")) {
    fopts |= 3;
  }
  else if (!strcmp(argv[*i],"-o65-name") && (*i+1)<argc) {
    fopts |= 1;
    fopt_name = argv[++*i];
  }
  else if (!strcmp(argv[*i],"-o65-paged")) {
    o65paged = 1;
    o65align = 3;
  }
  else if (!strcmp(argv[*i],"-o65-stack")) {
    n = sscanf(get_arg(argc,argv,i),"%li",&val);
    if (n!=1 || val>(long)o65stack)
      o65stack = val;
  }
  else
    return 0;

  return 1;
}


static int options_02(struct GlobalVars *gv,int argc,const char **argv,int *i)
{
  return options(0,gv,argc,argv,i);
}


static int options_816(struct GlobalVars *gv,int argc,const char **argv,int *i)
{
  return options(1,gv,argc,argv,i);
}


static void printhelp(int cpu816)
{
  printf("-o65-align <val>  min.alignment (# of bits to be 0): 0, 1, 2 or 8\n"
         "-o65-author <n>   store author name in header\n"
         "-o65-bsszero      request automatic clearing of BSS section\n");
  if (!cpu816)
    printf("-o65-cpu <model>  6502, 65c02, 65sc02, 65ce02, nmos6502, 65816\n");
  printf("-o65-fopts        gen. info in header: file/linker-name, version, date\n"
         "-o65-name <name>  overwrite fopts name\n"
         "-o65-paged        paged alignment and simplified paged relocations\n"
         "-o65-stack <val>  store required stack size in the header\n");
}


static void printhelp_02(void)
{
  printhelp(0);
}


static void printhelp_816(void)
{
  printhelp(1);
}



/*****************************************************************/
/*                          Read o65                             */
/*****************************************************************/


static uint8_t getbyte(void)
{
  olen--;
  return *optr++;
}


static uint16_t getword(void)
{
  uint16_t w = read16le(optr);
  optr += 2;
  olen -= 2;
  return w;
}


static uint32_t getsize(void)
{
  uint32_t s = o65size==2 ? read16le(optr) : read32le(optr);
  optr += o65size;
  olen -= o65size;
  return s;
}


static void skipbytes(size_t n)
{
  optr += n;
  olen -= n;
}


static uint8_t *skipstring(void)
{
  uint8_t *p = optr;

  while (*optr++ != '\0')
    olen--;
  olen--;

  return p;
}


static void o65_newsec(struct o65info *o65,int secno,uint8_t align)
{
  /* section attributes for: .text, .data, .bss, .zero */
  static uint8_t types[NSECS] = {
    ST_CODE,ST_DATA,ST_UDATA,ST_UDATA
  };
  static uint8_t prots[NSECS] = {
    SP_READ|SP_EXEC,SP_READ|SP_WRITE,SP_READ|SP_WRITE,SP_READ|SP_WRITE
  };
  static uint8_t flags[NSECS] = {
    SF_ALLOC,SF_ALLOC,SF_ALLOC|SF_UNINITIALIZED,SF_ALLOC|SF_UNINITIALIZED|SF_DPAGE
  };
  static const char *names[NSECS] = {
    TEXTNAME,DATANAME,BSSNAME,ZERONAME
  };

  if (o65->len[secno] > 0) {
    struct LinkFile *lf = o65->object->lnkfile;

    if (types[secno]!=ST_UDATA && (long)o65->len[secno]>=olen)
      error(49,lf->pathname,names[secno],lf->objname);  /* illegal sec.offs. */

    o65->data[secno] = optr;
    o65->sec[secno] =  add_section(o65->object,names[secno],
                                   types[secno]!=ST_UDATA ? optr : NULL,
                                   o65->len[secno],types[secno],flags[secno],
                                   prots[secno],align,FALSE);

    if (types[secno] != ST_UDATA) {
      skipbytes(o65->len[secno]);
      if (olen < 0)
        error(15,lf->pathname,names[secno],lf->objname);  /* unexpected end */
    }
  }
  else {
    o65->data[secno] = NULL;
    o65->sec[secno] = NULL;
  }
}


static void o65_makesections(struct o65info *o65)
{
  uint8_t align = MO65_ALIGN(o65->mode);
  int i;

  if (align > 2)
    align = 8;  /* page alignment */

  /* create all sections which have a size > 0 */
  for (i=0; i<NSECS; i++)
    o65_newsec(o65,i,align);
}


static int o65_getrelocs(struct GlobalVars *gv,struct o65info *o65,int secno)
{
  struct FFFuncs *ff = fff[o65->object->lnkfile->format];
  int pagedrel = o65->mode & MO65_PAGED;
  int wdc816 = o65->mode & MO65_65816;
  struct Section *sec = o65->sec[secno];
  uint8_t *d = o65->data[secno];
  struct Section *rsec;
  long off=-1,b;
  char *xname;
  unsigned sz;
  lword a,m;
  int i;

  while (olen>0 && (b = getbyte())) {
    if (sec == NULL) {
      error(143,o65->object->lnkfile->pathname,secno);  /* unexpected */
      break;
    }
    while (b == 0xff) {
      off += 254;
      b = getbyte();
    }
    off += b;

    b = getbyte();  /* reloc type and segment */
    i = R65SEG(b);
    if (i == UNDSEGID) {
      /* undefined - lookup external reference in namelist */
      unsigned idx = getsize();

      rsec = NULL;
      if (idx >= o65->namecount) {
        error(139,getobjname(o65->object),idx);  /* index out of range */
        return 0;
      }
      else
        xname = o65->names[idx];
    }
    else {
      xname = NULL;
      rsec = (i>=MINSEGID && i<=MAXSEGID) ? o65->sec[i-MINSEGID] : NULL;
      if (rsec == NULL) {
        error(52,o65->object->lnkfile->pathname,
              sec->name,getobjname(o65->object),i);  /* non existing sect. */
        return 0;
      }
    }

    /* read the addend */
    switch (R65TYPE(b)) {
      case 0x80:  /* 16-bit address */
        a = read16le(d+off);
        m = wdc816 ? 0xffff : -1;  /* ignore bank-byte for 65816 */
        sz = 16;
        break;
      case 0x40:  /* address high-byte */
        a = (((lword)*(d+off)) << 8) | (pagedrel ? 0 : getbyte());
        m = 0xff00;
        sz = 8;
        break;
      case 0x20:  /* address low-byte */
        a = *(d+off);
        m = 0xff;
        sz = 8;
        break;
      case 0xc0:  /* 24-bit segment address */
        a = read16le(d+off) | (((lword)*(d+off+2)) << 16);
        m = -1;
        sz = 24;
        break;
      case 0xa0:  /* segment-byte of 24-bit address */
        a = getword() | (((lword)*(d+off)) << 16);
        m = 0xff0000;
        sz = 8;
        break;
      default:    /* illegal relocation type */
        error(14,getobjname(o65->object),ff->tname,
              (int)R65TYPE(b),sec->name,(unsigned)off);
        return 0;
    }
    if (i != UNDSEGID)
      a -= o65->base[i-MINSEGID];  /* fix addend for base address 0 */

    /* add new relocation or external reference for current section */
    addreloc(sec,newreloc(gv,sec,xname,rsec,0,off,R_ABS,a),0,sz,m);
  }
  return 1;
}


static int o65_read(struct GlobalVars *gv,struct LinkFile *lf,
                    uint8_t *p,unsigned long plen)
{
  struct FFFuncs *ff = fff[lf->format];
  struct o65info o65;
  size_t stksize;
  uint8_t b;
  int i;

  optr = p;
  olen = plen;

chained:
  if (olen < sizeof(o65magic)+2 || memcmp(optr,o65magic,sizeof(o65magic))!=0) {
    error(41,lf->pathname,ff->tname);  /* corrupt */
    return 0;
  }

  optr += sizeof(o65magic);
  olen -= sizeof(o65magic);
  o65.mode = getword();  /* mode follows the magic-id */

  /* determine word-size in o65 structures */
  o65size = (o65.mode & MO65_LONG) ? 4 : 2;
  if (olen <= 9 * o65size) {
    error(41,lf->pathname,ff->tname);  /* corrupt */
    return 0;
  }

  /* read section base addresses and sizes */
  for (i=0; i<NSECS; i++) {
    o65.base[i] = getsize();
    o65.len[i] = getsize();
  }

  /* remember stack size, when higher than current value */
  stksize = getsize();
  if (stksize > o65stack)
    o65stack = stksize;

  /* skip header options */
  while (olen > 0 && (b = getbyte())) {
    switch (getbyte()) {
      case HOPT_FNAME:
        lf->objname = (char *)optr;
      default:
        skipbytes(b-2);
        break;
    }
  }

  /* create the object unit and add all sections with content */
  o65.object = create_objunit(gv,lf,lf->objname);
  o65_makesections(&o65);

  /* build list of undefined reference names */
  o65.namecount = olen>o65size ? getsize() : 0;
  if (o65.namecount) {
    o65.names = alloc(o65.namecount * sizeof(char *));
    for (i=0; i<o65.namecount; i++)
      o65.names[i] = (char *)skipstring();
  }
  else
    o65.names = NULL;

  /* read text and data section relocs */
  if (olen>0 && o65_getrelocs(gv,&o65,TSEC)) {
    if (olen>0 && o65_getrelocs(gv,&o65,DSEC)) {

      /* finally read exported symbols */
      if (olen>=o65size && (i = getsize())>0) do {
        struct Section *sec;
        int32_t val;
        uint8_t type;
        char *name;

        name = (char *)skipstring();
        b = getbyte();    /* segment id */
        val = getsize();  /* symbol value */

        if (b == ABSSEGID) {
          /* absolute value */
          sec = abs_section(o65.object);
          type = SYM_ABS;
        }
        else if (b>=MINSEGID && b<=MAXSEGID) {
          sec = o65.sec[b-MINSEGID];
          val -= o65.base[b-MINSEGID];  /* fix for base address 0 */
          type = SYM_RELOC;
        }
        else
          sec = NULL;

        if (sec != NULL) {
          addsymbol(gv,sec,name,NULL,val,type,0,SYMI_NOTYPE,
                    SYMB_GLOBAL,0,TRUE);
        }
        else
          error(53,lf->pathname,name,getobjname(o65.object),(int)b);
      } while (--i>0 && olen>0);
    }
  }

  if (o65.namecount)
    free(o65.names);

  /* done, enqueue object into the linking process */
  add_objunit(gv,o65.object,FALSE);

  if (o65.mode & MO65_CHAIN) {
    /* another o65 object is chained, read it */
    error(140,lf->pathname,getobjname(o65.object));
    goto chained;
  }

  if (olen != 0) {
    /* otherwise the file is likely corrupt when remaining size is non-zero */
    error(41,lf->pathname,ff->tname);
    return 0;
  }

  return 1;
}


static int identify(char *name,uint8_t *p,unsigned long plen,
                    bool lib,int cpu816)
/* identify an o65 object file or executable */
{
  struct ar_info ai;
  bool arflag = FALSE;
  uint16_t mode;

  if (ar_init(&ai,(char *)p,plen,name)) {
    /* library archive detected, extract 1st archive member */
    arflag = TRUE;
    if (!(ar_extract(&ai))) {
      error(38,name);  /* Empty archive ignored */
      return ID_IGNORE;
    }
    p = ai.data;
    plen = ai.size;
  }

  if (plen < sizeof(o65magic)+2 || memcmp(p,o65magic,sizeof(o65magic))!=0)
    return ID_UNKNOWN;

  mode = read16le(p + sizeof(o65magic));
  if (cpu816 == !(mode & MO65_65816))
    return ID_UNKNOWN;

  if (!arflag)
    return (mode & MO65_OBJ) ? ID_OBJECT : ID_EXECUTABLE;
  return ID_LIBARCH;
}


static int identify_02(struct GlobalVars *gv,char *name,uint8_t *p,
                       unsigned long plen,bool lib)
{
  return identify(name,p,plen,lib,0);
}


static int identify_816(struct GlobalVars *gv,char *name,uint8_t *p,
                        unsigned long plen,bool lib)
{
  return identify(name,p,plen,lib,1);
}


static void readconv(struct GlobalVars *gv,struct LinkFile *lf)
{
  struct ar_info ai;

  if (lf->type == ID_LIBARCH) {
    if (ar_init(&ai,(char *)lf->data,lf->length,lf->filename)) {
      while (ar_extract(&ai)) {
        lf->objname = allocstring(ai.name);
        if (!o65_read(gv,lf,ai.data,ai.size))
          break;
      }
    }
    else
      ierror("o65 readconv(): archive %s corrupted since last access",
             lf->pathname);
  }
  else {
    lf->objname = lf->filename;
    o65_read(gv,lf,lf->data,lf->length);
  }
}



/*****************************************************************/
/*                           Link o65                            */
/*****************************************************************/


static int is_bss_name(const char *p)
{
  size_t len = strlen(p);

  while (len >= 3) {
    if (toupper((unsigned char)p[0]) == 'B' &&
        toupper((unsigned char)p[1]) == 'S' &&
        toupper((unsigned char)p[2]) == 'S')
      return 1;
    p++;
    len--;
  }
  return 0;
}


static int targetlink(struct GlobalVars *gv,struct LinkedSection *ls,
                      struct Section *s)
/* returns 1, if target requires the combination of the two sections, */
/* returns -1, if target doesn't want to combine them, */
/* returns 0, if target doesn't care - standard linking rules are used. */
{
  /* o65 requires merging of all text, data, bss and zero-page sections! */
  if (ls->type == s->type) {
    if (s->type == ST_UDATA) {
      /* bss and zero are both uninitialized, make sure not to mix them */
      if (is_bss_name(ls->name) != is_bss_name(s->name))
        return -1;
    }
    return 1;  /* otherwise merge sections of same type */
  }
  return 0;
}



/*****************************************************************/
/*                          Write o65                            */
/*****************************************************************/


static void fwsize(FILE *f,unsigned w)
{
  if (o65size == 2)
    fwrite16le(f,w);
  else if (o65size == 4)
    fwrite32le(f,w);
  else
    ierror("fwsize: %d",o65size);
}


static void collect_sections(struct GlobalVars *gv,struct LinkedSection **secs)
{
  struct LinkedSection *ls;
  int i,idx;

  for (i=0; i<NSECS; secs[i++]=NULL);

  for (ls=(struct LinkedSection *)gv->lnksec.first;
       ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
    if (ls->flags & SF_ALLOC) {
      switch (ls->type) {
        case ST_UNDEFINED:
          /* @@@ discard undefined sections - they are empty anyway */
          ls->flags &= ~SF_ALLOC;
          break;
        case ST_CODE:
          if (secs[TSEC]==NULL)
            secs[TSEC] = ls;
          else
            error(138,fff[gv->dest_format]->tname,TEXTNAME,
                  secs[TSEC]->name,ls->name);
          break;
        case ST_DATA:
          if (secs[DSEC]==NULL)
            secs[DSEC] = ls;
          else
            error(138,fff[gv->dest_format]->tname,DATANAME,
                  secs[DSEC]->name,ls->name);
          break;
        case ST_UDATA:
          idx = is_bss_name(ls->name) ? BSEC : ZSEC;
          if (secs[idx]==NULL)
            secs[idx] = ls;
          else
            error(138,fff[gv->dest_format]->tname,idx==BSEC?BSSNAME:ZERONAME,
                  secs[idx]->name,ls->name);
          break;
        default:
          ierror("o65: Illegal section type %d (%s)",(int)ls->type,ls->name);
          break;
      }
    }
  }

  /* set highest alignment from all sections */
  if (!o65paged) {
    for (i=0; i<NSECS; i++) {
      if (secs[i] && secs[i]->alignment > o65align)
        o65align = secs[i]->alignment;
    }
    if (o65align > 2)
      o65align = 3;
  }
  else
    o65align = 3;  /* paged relocs imply alignment of 256 bytes */
}


static int o65_simple(struct LinkedSection **secs)
{
  /* return true, when text, data, bss are consecutive */
  lword addr;
  int i;

  for (i=0; i<ZSEC; i++) {
    if (secs[i] != NULL)
      break;
  }
  if (i < ZSEC) {
    for (addr=secs[i]->base; i<ZSEC; i++) {
      if (secs[i] != NULL) {
        if (secs[i]->base != addr)
          return 0;
        addr += secs[i]->size;
      }
    }
  }
  return 1;
}


static size_t o65_fopt(FILE *f,int type,const char *data,size_t len)
{
  if (len > 253) {
    if (f != NULL)
      error(141,(unsigned)len+2);  /* max file option size exceeded */
    return 0;
  }
  if (f != NULL) {
    fwrite8(f,len+2);
    fwrite8(f,type);
    fwritex(f,data,len);
  }
  return len+2;
}


static size_t o65_genfopts(struct GlobalVars *gv,FILE *f)
{
  size_t len = 0;

  if (fopts) {
    if ((fopts & 2) && fopt_name==NULL)
      fopt_name = gv->dest_name;

    if (fopt_name)
      len += o65_fopt(f,0,fopt_name,strlen(fopt_name)+1);

    if (fopts & 2) {
      /* write linker name and version */
      char creator[32];

      snprintf(creator,32,PNAME " %s",version_str);
      len += o65_fopt(f,2,creator,strlen(creator)+1);
    }

    if (fopt_author)
      len += o65_fopt(f,3,fopt_author,strlen(fopt_author)+1);

    if (fopts & 2) {
      /* write creation date */
      char datebuf[32];
      time_t now;

      (void)time(&now);
      strftime(datebuf,32,"%a %b %d %H:%M:%S %Z %Y",localtime(&now));
      len += o65_fopt(f,4,datebuf,strlen(datebuf)+1);
    }
  }
  return len;
}


static void o65_header(struct GlobalVars *gv,FILE *f,
                       struct LinkedSection **secs,int exe,int cpu816)
{
  uint16_t mode;
  int i;

  o65size = o65outsize;
  fwritex(f,o65magic,sizeof(o65magic));

  /* construct mode word */
  mode = o65align & 3;
  if (cpu816)
    mode |= MO65_65816;  /* native 65816 */
  else
    mode |= (o65cpu & 15) << 4;
  if (o65paged)
    mode |= MO65_PAGED;  /* paged (simpler) relocations */
  if (exe) {
    if (o65bsszero)
      mode |= MO65_BSSZERO;
    if (o65_simple(secs))
      mode |= MO65_SIMPLE; /* sections can be loaded consecutive into memory */
  }
  else
    mode |= MO65_OBJ;    /* object file, not executable */
  fwrite16le(f,mode);

  /* section base addresses and sizes */
  for (i=0; i<NSECS; i++) {
    fwsize(f,secs[i] ? secs[i]->base : 0);
    fwsize(f,secs[i] ? secs[i]->size : 0);
  }

  fwsize(f,o65stack);

  /* file options */
  o65_genfopts(gv,f);
  fwrite8(f,0);
}


static void o65_writesections(struct GlobalVars *gv,FILE *f,
                              struct LinkedSection **secs)
{
  fwritefullsect(gv,f,secs[TSEC]);
  fwritefullsect(gv,f,secs[DSEC]);
}


static void o65_addimport(struct Reloc *xref)
{
  struct ImportNode **chain,*new;

  chain = &o65implist.hashtab[elf_hash(xref->xrefname)%IMPHTABSIZE];
  while (new = *chain) {
    if (!strcmp(new->name,xref->xrefname))
      return;  /* name already present */
    chain = &new->hashchain;
  }

  *chain = new = alloczero(sizeof(struct ImportNode));
  new->name = xref->xrefname;
  addtail(&o65implist.l,&new->n);
  o65implist.entries++;
}


static void o65_genxreftab(struct GlobalVars *gv,struct LinkedSection **secs)
{
  unsigned i;

  initlist(&o65implist.l);
  o65implist.hashtab = alloczero(IMPHTABSIZE*sizeof(struct ImportNode *));
  o65implist.entries = 0;

  /* collect unique import names in a list */
  for (i=TSEC; i<=DSEC; i++) {
    if (secs[i] != NULL) {
      struct Reloc *rel;

      for (rel=(struct Reloc *)secs[i]->xrefs.first; rel->n.next!=NULL;
           rel=(struct Reloc *)rel->n.next) {
        /* put imported symbol names into a list */
        if (rel->flags & RELF_INTERNAL)
          continue;  /* internal relocations will never be exported */
        o65_addimport(rel);

        /* write addend to section */
        writesection(gv,secs[i]->data,rel->offset,rel,rel->addend);
      }
    }
  }
}


static unsigned o65_findxref(const char *name)
{
  struct ImportNode **chain = &o65implist.hashtab[elf_hash(name)%IMPHTABSIZE];
  struct ImportNode *imp;

  while (imp = *chain) {
    if (!strcmp(name,imp->name))
      return imp->idx;
    chain = &imp->hashchain;
  }
  ierror("o65_findxref: %s disappeared",name);
  return ~0;
}


static void o65_writexreftab(FILE *f)
{
  struct ImportNode *imp;
  unsigned i = 0;

  fwsize(f,o65implist.entries);

  while (imp = (struct ImportNode *)remhead(&o65implist.l)) {
    imp->idx = i++;
    fwritex(f,imp->name,strlen(imp->name)+1);
    o65implist.entries--;
  }

  if (o65implist.entries)
    ierror("o65_xreftable: remaining entries: %u",o65implist.entries);
}


static uint8_t o65_rtype(struct RelocInsert *ri)
{
  if (ri->next!=NULL || ri->bpos!=0)
    return 0;

  if ((ri->mask & 0xffffff) != 0xffffff && ri->bsiz==8) {
    if ((ri->mask & 0xffffff) == 0xff0000)
      return 0xa0;  /* segment-byte of 24-bit address */
    else if ((ri->mask & 0xffff) == 0xff00)
      return 0x40;  /* high-byte of 16-bit address */
    else if (ri->mask == 0xff)
      return 0x20;  /* low-byte of 16-bit address */
  }
  else if (ri->bsiz == 8)
    return 0x20;  /* probably an 8-bit xref to an absolute value */
  else if (ri->bsiz == 16)
    return 0x80;  /* 16-bit address */
  else if (ri->bsiz == 24)
    return 0xc0;  /* 24-bit address */

  return 0;
}


static uint8_t o65_segid(struct LinkedSection **secs,struct LinkedSection *ls)
{
  int i;

  for (i=0; i<NSECS; i++) {
    if (secs[i] == ls)
      return MINSEGID + i;
  }
  ierror("o65_segid: no o65 section: %s",ls->name);
  return 0;
}


static void o65_writerelocs(struct GlobalVars *gv,FILE *f,
                            struct LinkedSection **secs)
{
  const char *fn = "o65_writerelocs(): ";
  int i;

  for (i=TSEC; i<=DSEC; i++) {
    if (secs[i] != NULL) {
      struct Reloc *rel,*xref,*r;
      int lastoffs = -1;
      int newoffs,diff;
      uint8_t type,segid;
      lword a;

      sort_relocs(&secs[i]->relocs);
      rel = (struct Reloc *)secs[i]->relocs.first;
      sort_relocs(&secs[i]->xrefs);
      xref = (struct Reloc *)secs[i]->xrefs.first;

      while (rel->n.next || xref->n.next) {
        if (xref->n.next==NULL || (rel->n.next && rel->offset<xref->offset)) {
          r = rel;
          newoffs = r->offset;
          rel = (struct Reloc *)rel->n.next;
        }
        else {
          r = xref;
          newoffs = r->offset;
          xref = (struct Reloc *)xref->n.next;
        }

        /* only some ABS address relocations are supported */
        if (r->rtype==R_ABS && (type = o65_rtype(r->insert))) {
          if (r->xrefname) {
            segid = UNDSEGID;
            a = r->addend;
          }
          else {
            segid = o65_segid(secs,r->relocsect.lnk);
            a = r->relocsect.lnk->base + r->addend;
          }

          /* write offset to next relocation field */
          if ((diff = newoffs - lastoffs) <= 0)
            ierror("%s%s reloc list not sorted: %d <= last (%d)",
                   fn,secs[i]->name,newoffs,lastoffs);
          lastoffs = newoffs;

          while (diff > 254) {
            fwrite8(f,255);
            diff -= 254;
          }
          fwrite8(f,diff);

          /* write reloc-type and additional bytes */
          fwrite8(f,type|segid);
          if (segid == UNDSEGID)
            fwsize(f,o65_findxref(r->xrefname));
          switch (type) {
            case 0x40:  /* address high-byte */
              if (!o65paged)
                fwrite8(f,a&0xff);  /* write low-byte */
              break;
            case 0xa0:  /* segment-byte of 24-bit address */
              fwrite16le(f,a&0xffff);
              break;
            default:
              break;
          }
        }
        else {
          struct RelocInsert *ri;

          if (ri = r->insert)
            error(32,fff[gv->dest_format]->tname,reloc_name[r->rtype],
                  (int)ri->bpos,(int)ri->bsiz,mtaddr(gv,ri->mask),
                  secs[i]->name,r->offset);
          else
            ierror("%smissing RelocInsert for rtype %d at %s+%lu",
                   fn,(int)r->rtype,secs[i]->name,r->offset);
        }
      }
    }
    fwrite8(f,0);  /* end of reloc table */
  }
}


static void o65_writexdeftab(struct GlobalVars *gv,FILE *f,
                             struct LinkedSection **secs)
{
  long cntoffs;
  size_t n = 0;
  int i;

  cntoffs = ftell(f);
  fwsize(f,0);  /* remember to write number of exports here */

  for (i=0; i<NSECS; i++) {
    if (secs[i] != NULL) {
      struct Symbol *sym = (struct Symbol *)secs[i]->symbols.first;

      for (sym=(struct Symbol *)secs[i]->symbols.first;
           sym->n.next!=NULL; sym=(struct Symbol *)sym->n.next) {
        if (sym->bind == SYMB_GLOBAL) {
          if (!discard_symbol(gv,sym)) {
            if (sym->type==SYM_ABS || sym->type==SYM_RELOC) {
              fwritex(f,sym->name,strlen(sym->name)+1);
              fwrite8(f,sym->type==SYM_ABS?ABSSEGID:o65_segid(secs,secs[i]));
              fwsize(f,sym->value);
              n++;
            }
            else
              error(33,fff[gv->dest_format]->tname,sym->name,
                    sym_bind[sym->bind],sym_type[sym->type],
                    sym_info[sym->info]);
          }
        }
        else if (sym->bind == SYMB_WEAK)  /* ignore weak symbols */
          error(142,fff[gv->dest_format]->tname,sym->name);
      }
    }
  }

  /* patch number of exported symbols */
  if (n) {
    fseek(f,cntoffs,SEEK_SET);
    fwsize(f,n);
    fseek(f,0,SEEK_END);
  }
}


static unsigned long headersize(struct GlobalVars *gv)
{
  /* Magic header
   * .word mode
   * .size tbase,tlen,dbase,dlen,bbase,blen,zbase,zlen
   * .size stack
   * variable file options
   */
  return sizeof(o65magic) + 2 + 9*o65outsize + o65_genfopts(gv,NULL) + 1;
}


static void writeshared(struct GlobalVars *gv,FILE *f)
{
  error(30);  /* Target file format doesn't support shared objects */
}


static void writeobject(struct GlobalVars *gv,FILE *f,int cpu816)
/* creates an o65 relocatable object file */
{
  struct LinkedSection *secs[NSECS];

  collect_sections(gv,secs);
  o65_header(gv,f,secs,0,cpu816);
  o65_genxreftab(gv,secs);
  calc_relocs(gv,secs[TSEC]);
  calc_relocs(gv,secs[DSEC]);
  o65_writesections(gv,f,secs);
  o65_writexreftab(f);
  o65_writerelocs(gv,f,secs);
  o65_writexdeftab(gv,f,secs);
}


static void writeobject_02(struct GlobalVars *gv,FILE *f)
{
  writeobject(gv,f,0);
}


static void writeobject_816(struct GlobalVars *gv,FILE *f)
{
  writeobject(gv,f,1);
}


static void writeexec(struct GlobalVars *gv,FILE *f,int cpu816)
/* creates an o65 executable file, which may be relocatable */
{
  struct LinkedSection *secs[NSECS];

  collect_sections(gv,secs);
  o65_header(gv,f,secs,1,cpu816);
  o65_genxreftab(gv,secs);
  calc_relocs(gv,secs[TSEC]);
  calc_relocs(gv,secs[DSEC]);
  o65_writesections(gv,f,secs);
  o65_writexreftab(f);
  o65_writerelocs(gv,f,secs);
  o65_writexdeftab(gv,f,secs);
}


static void writeexec_02(struct GlobalVars *gv,FILE *f)
{
  writeexec(gv,f,0);
}


static void writeexec_816(struct GlobalVars *gv,FILE *f)
{
  writeexec(gv,f,1);
}


#endif

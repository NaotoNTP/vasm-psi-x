/* $VER: vlink t_ataritos.c V0.18 (23.12.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2015,2021,2023,2024  Frank Wille
 */

#include "config.h"
#ifdef ATARI_TOS
#define T_ATARITOS_C
#include "vlink.h"
#include "tosdefs.h"


static int identify(struct GlobalVars *,char *,uint8_t *,unsigned long,bool);
static void readconv(struct GlobalVars *,struct LinkFile *);
static int targetlink(struct GlobalVars *,struct LinkedSection *,
                      struct Section *);
static unsigned long headersize(struct GlobalVars *);
static void writeobject(struct GlobalVars *,FILE *);
static void writeshared(struct GlobalVars *,FILE *);
static void writeexec(struct GlobalVars *,FILE *);


struct FFFuncs fff_ataritos = {
  "ataritos",
  defaultscript,
  NULL,
  NULL,
  tos_options,
  tos_printhelp,
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



/*****************************************************************/
/*                       Read Atari DRI                          */
/*****************************************************************/


static int dri_reloccheck(uint8_t *p,size_t words,int syms)
{
  uint16_t rw;

  while (words--) {
    rw = read16be(p);
    if ((rw&7) == 4 || (rw&7) == 6)
      if ((int)(rw >> 3) >= syms)
        return 0;  /* illegal symbol index for external reference */
    /* @@@ may also check if other types have 0-index */
    p += 2;
  }

  return 1;
}


static int tos_reloccheck(uint8_t *p,size_t len,size_t tdsize)
{
  size_t poffs = read32be(p);
  size_t roffs = 4;
  size_t a;

  if (poffs==0 && roffs==len)
    return 1;  /* executable with no relocations */

  p += 4;
  while (roffs<len && poffs<tdsize) {
    a = *p++;
    roffs++;
    if (a == 0)
      return roffs == len;
    else if (a == 1)
      a += 253;
#if 0 /* @@@ odd reloc-offsets allowed? */
    else if (a & 1)
      return 0;
#endif
    poffs += a;
  }

  return 0;
}


static int identify(struct GlobalVars *gv,char *name,uint8_t *p,
                    unsigned long plen,bool lib)
/* identify a DRI object file or TOS executable */
{
  struct ar_info ai;
  PH *hdr;
  bool arflag;

  if (ar_init(&ai,(char *)p,plen,name)) {
    /* Unix ar library archive detected, extract 1st archive member */
    arflag = TRUE;
    if (!(ar_extract(&ai))) {
      error(38,name);  /* Empty archive ignored */
      return ID_IGNORE;
    }
    hdr = (PH *)ai.data;
    plen = ai.size;
  }
  else if (plen>sizeof(struct DRIarheader)+2 &&
           (read16be(p)==DRI_ARMAGIC || read16be(p)==DRI_ARMAGIC+1)) {
    /* DRI library archive, look at first member */
    struct DRIarheader *dahdr = (struct DRIarheader *)(p + 2);

    arflag = TRUE;
    hdr = (PH *)(p + 2 + sizeof(struct DRIarheader));
    plen = read32be(dahdr->a_fsize);
  }
  else {
    arflag = FALSE;
    hdr = (PH *)p;
  }

  if (plen>sizeof(PH) && read16be(hdr->ph_branch)==DRIMAGIC &&
      read32be(hdr->ph_slen)%sizeof(struct DRIsym)==0) {
    size_t tlen = read32be(hdr->ph_tlen);
    size_t dlen = read32be(hdr->ph_dlen);
    size_t slen = read32be(hdr->ph_slen);
    size_t reloffs = sizeof(PH) + tlen + dlen + slen;
    int nsym = slen / sizeof(struct DRIsym);

    /* check for DRI object file first */
    if ((plen == reloffs + tlen + dlen) &&
        dri_reloccheck(p+reloffs,tlen/2,nsym) &&
        dri_reloccheck(p+reloffs+tlen,dlen/2,nsym))
      return arflag ? ID_LIBARCH : ID_OBJECT;

    /* may be an executable with TOS relocs */
    if (!arflag && ((read16be(hdr->ph_abs)!=0 && plen-reloffs==0) ||
        (plen-reloffs>=4 && tos_reloccheck(p+reloffs,plen-reloffs,tlen+dlen))))
      return ID_EXECUTABLE;  /* cannot be in an library archive */
  }
  return ID_UNKNOWN;
}


static int check_sozobonx(struct DRIsym *sym)
{
  return read16be(sym->type) == STYP_XFLAGS &&
         read32be(sym->value) == XVALUE;
}


static struct Section *dri_newsec(struct DRIinfo *dri,int idx,uint8_t *data,
                                  unsigned long size)
{
  /* section attributes for: .text, .data, .bss */
  static uint8_t types[] = {
    ST_CODE,ST_DATA,ST_UDATA
  };
  static uint8_t prots[] = {
    SP_READ|SP_EXEC,SP_READ|SP_WRITE,SP_READ|SP_WRITE
  };
  static uint8_t flags[] = {
    SF_ALLOC,SF_ALLOC,SF_ALLOC|SF_UNINITIALIZED
  };
  static const char *names[] = {
    TEXTNAME,DATANAME,BSSNAME
  };

  if (idx < 2) {
    if (data+size > dri->end) {  /* illegal section offset */
      struct LinkFile *lf = dri->object->lnkfile;
      error(49,lf->pathname,names[idx],lf->objname);
    }
    dri->data[idx] = data;
  }
  return dri->sec[idx] = add_section(dri->object,names[idx],data,size,
                                     types[idx],flags[idx],
                                     prots[idx],TOS_ALIGNMENT,FALSE);
}


static struct Section *dri_get_section(struct DRIinfo *dri,int secno)
{
  struct Section *sec;

  if ((sec = dri->sec[secno]) == NULL) {
    /* reference to empty section - create it */
    sec = dri_newsec(dri,secno,secno<2?dri->end:NULL,0);
  }
  return sec;
}


static uint8_t *dri_make_sections(struct DRIinfo *dri,uint8_t *p)
{
  uint32_t size;

  if (size = read32be(dri->hdr->ph_tlen)) {
    dri_newsec(dri,0,p,size);
    p += size;  /* @@@ Can we assume ph_tlen is always aligned? */
  }
  else
    dri->sec[0] = NULL;

  if (size = read32be(dri->hdr->ph_dlen)) {
    dri_newsec(dri,1,p,size);
    p += size;
  }
  else
    dri->sec[1] = NULL;

  if (size = read32be(dri->hdr->ph_blen))
    dri_newsec(dri,2,NULL,size);
  else
    dri->sec[2] = NULL;

  return p;
}


static lword read_addend(struct DRIinfo *dri,int secno,
                         unsigned long offs,uint16_t sz)
{
  struct Section *s = dri->sec[secno];

  if (s == NULL)
    ierror("DRI read_addend(): No section #%d (offset %lu)",secno,offs);

  switch (sz) {
    case 16:
      return sign_extend(read16be(s->data+offs),16);
    case 32:
      return sign_extend(read32be(s->data+offs),32);
    default:
      ierror("DRI read_addend(): Bad size %u",(unsigned)sz);
  }
  return 0;
}


static size_t namelen(const char *p,size_t maxlen)
/* similar to strnlen() */
{
  size_t len = 0;

  while (maxlen-- && *p!='\0')
    len++;
  return len;
}


static char *dri_symname(struct DRIinfo *dri,int symno)
{
  if (symno < dri->nsym) {
    char *buf,*p;
    int len;

    /* determine symbol name length */
    if (dri->sozobonx) {
      int n = symno + 1;

      len = 0;
      while (n<dri->nsym && check_sozobonx(&dri->symtab[n])) {
        len += DRI_NAMELEN;
        n++;
      }
      len += namelen(dri->symtab[n-1].name,DRI_NAMELEN);
    }
    else {
      if (symno+1<dri->nsym &&
          (read16be(dri->symtab[symno].type) & STYP_LONGNAME)==STYP_LONGNAME)
        len = DRI_NAMELEN +
              namelen(dri->symtab[symno+1].name,sizeof(struct DRIsym));
      else
        len = namelen(dri->symtab[symno].name,DRI_NAMELEN);
    }

    /* allocate and populate new name buffer */
    p = buf = alloc(len + 1);

    if (dri->sozobonx) {
      while (symno+1<dri->nsym && check_sozobonx(&dri->symtab[symno+1])) {
        memcpy(p,dri->symtab[symno].name,DRI_NAMELEN);
        p += DRI_NAMELEN;
        len -= DRI_NAMELEN;
        symno++;
      }
    }
    else {
      if (symno+1<dri->nsym &&
          (read16be(dri->symtab[symno].type)&STYP_LONGNAME)==STYP_LONGNAME) {
        memcpy(p,dri->symtab[symno].name,DRI_NAMELEN);
        p += DRI_NAMELEN;
        len -= DRI_NAMELEN;
        symno++;
      }
    }
    strncpy(p,dri->symtab[symno++].name,len);
    p[len] = '\0';
    dri->symidx = symno;  /* symbol-index for next symbol */
    return buf;
  }
  else
    ierror("dri_symname(): symno %d > %d",symno,dri->nsym);
  return NULL;
}


static int dri_addreloc(struct GlobalVars *gv,struct DRIinfo *dri,
                        int secno,int rsecno,int rsymno,
                        unsigned long offs,uint8_t rtype,uint16_t sz)
{
  struct Section *sec = dri->sec[secno];
  struct Section *rsec = NULL;
  char *xname = NULL;
  lword a = 0;

  if (rsymno >= 0) {
    /* reference to (external) symbol */
    struct DRIsym *rsym;
    uint16_t stype;

    if (rsymno >= dri->nsym) {
      /* symbol index out of range */
      error(139,getobjname(dri->object),(unsigned)rsymno);
      return 0;
    }
    rsym = &dri->symtab[rsymno];
    stype = read16be(rsym->type);

    if (stype & STYP_EXTERNAL) {
      if ((xname = dri_symname(dri,rsymno)) == NULL)
        return 0;
    }
    else if (stype & STYP_TEXT) {
      rsecno = 0;
      a = read32be(rsym->value);
    }
    else if (stype & STYP_DATA) {
      rsecno = 1;
      a = read32be(rsym->value);
    }
    else if (stype & STYP_BSS) {
      rsecno = 2;
      a = read32be(rsym->value);
    }
    else {
      /* symbol reference with unsupported type */
      error(20,dri->object->lnkfile->pathname,dri_symname(dri,rsymno),
            dri->object->objname,(int)stype);
      return 0;
    }
  }

  if (rsecno >= 0) {
    if (rsecno > 2)
      ierror("dri_addreloc(): Illegal section number %d",rsecno);
    rsec = dri_get_section(dri,rsecno);
  }

  a += read_addend(dri,secno,offs,sz);
  if (rtype == R_SD)
    a &= makemask(sz);  /* SD-addends must be unsigned! */

  if (sec == NULL)
    ierror("dri_addreloc(): our section #%d disappered",secno);
  addreloc(sec,newreloc(gv,sec,xname,rsec,0,offs,rtype,a),0,sz,-1);
  return 1;
}


static int dri_make_relocs(struct GlobalVars *gv,struct DRIinfo *dri,int secno,
                           uint8_t *p,size_t plen)
{
  unsigned long offs = 0;
  int sz = 0;
  uint16_t rw;

  if (p + plen > dri->end) {
    error(41,dri->object->lnkfile->pathname,dri->ff->tname);  /* corrupt */
    return 0;
  }

  plen >>= 1;  /* number of potential relocation words to read */
  while (plen--) {
    rw = read16be(p);
    p += 2;

    switch (rw & 7) {
      case 1:  /* relocate relative to data */
        if (!dri_addreloc(gv,dri,secno,1,-1,offs-2*sz,R_ABS,16<<sz))
          return 0;
        sz = 0;
        break;
      case 2:  /* relocate relative to text */
        if (!dri_addreloc(gv,dri,secno,0,-1,offs-2*sz,R_ABS,16<<sz))
          return 0;
        sz = 0;
        break;
      case 3:  /* relocate relative to bss */
        if (!dri_addreloc(gv,dri,secno,2,-1,offs-2*sz,R_ABS,16<<sz))
          return 0;
        sz = 0;
        break;
      case 4:  /* absolute external reference */
        /* 16-bit absolute references are handled as small-data (R_SD) */
        if (!dri_addreloc(gv,dri,secno,-1,rw>>3,offs-2*sz,sz?R_ABS:R_SD,16<<sz))
          return 0;
        sz = 0;
        break;
      case 5:  /* indicates a 32-bit relocation, reloc-info in next word */
        sz = 1;
        break;
      case 6:  /* pc-relative external reference */
        if (!dri_addreloc(gv,dri,secno,-1,rw>>3,offs-2*sz,R_PC,16<<sz))
          return 0;
        sz = 0;
        break;
      default:
        /* reloc types 0 and 7 are both seen as no-ops, which should be ok */
        sz = 0;
        break;
    }
    offs += 2;
  }

  return 1;
}


static int tos_make_relocs(struct GlobalVars *gv,struct DRIinfo *dri,uint8_t *p)
{
  unsigned long tlen = read32be(dri->hdr->ph_tlen);
  unsigned long dlen = read32be(dri->hdr->ph_dlen);
  unsigned long poffs,delta;

  if (dri->end - p < 4) {
    tos_corrupt:
    error(41,dri->object->lnkfile->pathname,dri->ff->tname);  /* corrupt */
    return 0;
  }

  poffs = read32be(p);
  p += 4;
  if (poffs==0 && p==dri->end)
    return 1;  /* no relocations */

  do {
    struct Section *rsec;
    unsigned long offs;
    int secno;
    lword a;

    if (poffs >= tlen) {
      if (poffs >= tlen + dlen)
        goto tos_corrupt;
      secno = 1;
      offs = poffs - tlen;
    }
    else {
      secno = 0;
      offs = poffs;
    }

    a = read_addend(dri,secno,offs,32);
    if (a >= (lword)(tlen+dlen)) {
      /* reloc to bss */
      a -= tlen+dlen;
      rsec = dri_get_section(dri,2);
    }
    else if (a >= (lword)tlen) {
      /* reloc to data */
      a -= tlen;
      rsec = dri_get_section(dri,1);
    }
    else /* reloc to text */
      rsec = dri_get_section(dri,0);

    addreloc(dri->sec[secno],
             newreloc(gv,dri->sec[secno],NULL,rsec,0,offs,R_ABS,a),
             0,32,-1);
    do {
      if ((delta = *p++) == 1)
        poffs += 254;
      else
        poffs += delta;
    } while (delta==1 && p<dri->end);
  } while (delta && p<=dri->end);

  if (p != dri->end)
    goto tos_corrupt;
  return 1;
}


static int dri_make_symbols(struct GlobalVars *gv,struct DRIinfo *dri)
{
  int idx = dri->sozobonx ? 1 : 0;

  while (idx < dri->nsym) {
    struct DRIsym *sym = &dri->symtab[idx];
    uint16_t type = read16be(sym->type);
    int32_t val = read32be(sym->value);
    char *name;

    if (name = dri_symname(dri,idx)) {
      struct Section *sec;
      uint8_t stype = SYM_RELOC;
      uint32_t ssize = 0;  /* only common symbols define a size with DRI */

      idx = dri->symidx;

      if ((type & STYP_EXTERNAL) && val != 0) {
        /* Common symbol, with val defining its size */
        stype = SYM_COMMON;
        ssize = val;
        val = TOS_ALIGNMENT;
        sec = common_section(gv,dri->object);
      }
      else if ((type & (STYP_DEFINED|STYP_EXTERNAL)) == STYP_DEFINED) {
        /* we handle all not externally defined symbols here */
        switch (type &
                (STYP_TEXT|STYP_DATA|STYP_BSS|STYP_REGISTER|STYP_EQUATED)) {
          case STYP_TEXT:
            sec = dri_get_section(dri,0);
            break;
          case STYP_DATA:
            sec = dri_get_section(dri,1);
            break;
          case STYP_BSS:
            sec = dri_get_section(dri,2);
            break;
          case STYP_EQUATED:
            stype = SYM_ABS;
            sec = abs_section(dri->object);
            break;
          default:
            /* ignore REGISTER symbols or strange combinations */
            free(name);
            continue;
        }
      }
      else {
        free(name);
        continue;
      }

      if (type & STYP_GLOBAL)
        addsymbol(gv,sec,name,NULL,val,stype,0,
                  SYMI_NOTYPE,SYMB_GLOBAL,ssize,TRUE);
      else
        addlocsymbol(gv,sec,name,NULL,val,stype,0,SYMI_NOTYPE,ssize);
    }
    else
      idx++;
  }
  return 1;
}


static int dri_read(struct GlobalVars *gv,struct LinkFile *lf,
                    uint8_t *p,unsigned long plen)
{
  struct DRIinfo dri;
  size_t len;

  dri.ff = fff[lf->format];
  if (read16be(p) != DRIMAGIC) {
    error(41,lf->pathname,dri.ff->tname);  /* corrupt */
    return 0;
  }
  dri.object = create_objunit(gv,lf,lf->objname);
  dri.hdr = (PH *)p;
  dri.end = p + plen;

  p += sizeof(PH);
  p = dri_make_sections(&dri,p);

  dri.symtab = (struct DRIsym *)p;
  len = read32be(dri.hdr->ph_slen);
  dri.nsym = len / sizeof(struct DRIsym);
  dri.sozobonx = dri.nsym && !memcmp(dri.symtab->name,XNAME,DRI_NAMELEN)
                 && check_sozobonx(dri.symtab);
  p += len;

  if (lf->type == ID_EXECUTABLE) {
    if (read16be(dri.hdr->ph_abs) == 0) {
      if (!tos_make_relocs(gv,&dri,p))
        return 0;
    }
  }
  else {  /* object file */
    len = read32be(dri.hdr->ph_tlen);
    if (!dri_make_relocs(gv,&dri,0,p,len))  /* text relocs */
      return 0;
    p += len;
    if (!dri_make_relocs(gv,&dri,1,p,read32be(dri.hdr->ph_dlen))) /* data relocs */
      return 0;
  }

  if (!dri_make_symbols(gv,&dri))
    return 0;

  add_objunit(gv,dri.object,FALSE);
  return 1;
}


static void readconv(struct GlobalVars *gv,struct LinkFile *lf)
{
  if (lf->type == ID_LIBARCH) {
    struct ar_info ai;

    if (ar_init(&ai,(char *)lf->data,lf->length,lf->filename)) {
      while (ar_extract(&ai)) {
        lf->objname = allocstring(ai.name);
        if (!dri_read(gv,lf,ai.data,ai.size))
          break;
      }
    }
    else if (read16be(lf->data)==DRI_ARMAGIC ||
             read16be(lf->data)==DRI_ARMAGIC+1) {
      /* convert all DRI library modules */
      uint8_t *p = lf->data + 2;
      unsigned long len = lf->length - 2;
      struct DRIarheader *darh;

      while (len > sizeof(struct DRIarheader)) {
        unsigned long osize;

        darh = (struct DRIarheader *)p;
        osize = read32be(darh->a_fsize);
        p += sizeof(struct DRIarheader);
        lf->objname = allocstring(darh->a_fname);
        if (!dri_read(gv,lf,p,osize))
          break;
        p += osize;
        len -= sizeof(struct DRIarheader) + osize;
      }
    }
    else
      ierror("DRI readconv(): archive %s corrupted since last access",
             lf->pathname);
  }
  else {
    /* convert single DRI object or TOS executable */
    lf->objname = lf->filename;
    dri_read(gv,lf,lf->data,lf->length);
  }
}



/*****************************************************************/
/*                       Link Atari TOS                          */
/*****************************************************************/


static int targetlink(struct GlobalVars *gv,struct LinkedSection *ls,
                      struct Section *s)
/* returns 1, if target requires the combination of the two sections, */
/* returns -1, if target doesn't want to combine them, */
/* returns 0, if target doesn't care - standard linking rules are used. */
{
  /* TOS requires that all sections of type CODE or DATA or BSS */
  /* will be merged, because there are only these three available! */
  if (ls->type == s->type)
    return 1;

  return 0;
}



/*****************************************************************/
/*                      Write Atari TOS                          */
/*****************************************************************/


static int dri_symbol_count(const char *name)
/* determine number of DRI symbol slots to use for this name */
{
  int len = strlen(name);
  int cnt = 1;

  if (len > DRI_NAMELEN) {
    if (sozobonx)
      cnt += (len - 1) / DRI_NAMELEN;
    else if (hisoftdri)
      cnt++;
  }
  return cnt;
}


static int tos_initwrite(struct GlobalVars *gv,
                         struct LinkedSection **sections)
/* find exactly one ST_CODE, ST_DATA and ST_UDATA section, which
   will become .text, .data and .bss,
   then count the number of symbol definitions and references */
{
  struct Symbol *sym;
  struct Reloc *xref;
  int i,cnt;

  if (gv->dest_object || sozobonx)
    hisoftdri = FALSE;  /* disable HiSoft extension in object files */

  get_text_data_bss(gv,sections);

  /* count symbols and unresolved references */
  for (i=0,cnt=0; i<3; i++) {
    if (sections[i]) {
      for (sym=(struct Symbol *)sections[i]->symbols.first;
           sym->n.next!=NULL; sym=(struct Symbol *)sym->n.next) {
        if (!discard_symbol(gv,sym) &&
            (sym->type==SYM_ABS || sym->type==SYM_RELOC ||
             (gv->dest_object && sym->type==SYM_COMMON)))
          cnt += dri_symbol_count(sym->name);
      }
      if (gv->dest_object) {  /* include externally referenced symbols */
        for (xref=(struct Reloc *)sections[i]->xrefs.first;
             xref->n.next!=NULL; xref=(struct Reloc *)xref->n.next)
          cnt += dri_symbol_count(xref->xrefname);
      }
    }
  }

  text_data_bss_gaps(sections);  /* calculate gap size between sections */

  if (cnt && sozobonx)
    cnt++;
  return cnt;
}


static void tos_header(FILE *f,unsigned long tsize,unsigned long dsize,
                       unsigned long bsize,unsigned long ssize,
                       unsigned long flags)
{
  PH hdr;

  write16be(hdr.ph_branch,0x601a);
  write32be(hdr.ph_tlen,tsize);
  write32be(hdr.ph_dlen,dsize);
  write32be(hdr.ph_blen,bsize);
  write32be(hdr.ph_slen,ssize);
  write32be(hdr.ph_magic,0);
  write32be(hdr.ph_flags,flags);
  write16be(hdr.ph_abs,0);

  fwritex(f,&hdr,sizeof(PH));
}


static void write_dri_sym(FILE *f,const char *name,
                          uint16_t type,uint32_t value)
{
  struct DRIsym stab;
  int namelen = strlen(name);
  int longname = (namelen > DRI_NAMELEN) && hisoftdri;
  int szb_extensions = sozobonx ? (namelen-1) / DRI_NAMELEN : 0;

  strncpy(stab.name,name,DRI_NAMELEN);
  write16be(stab.type,longname?(type|STYP_LONGNAME):type);
  write32be(stab.value,value);
  fwritex(f,&stab,sizeof(struct DRIsym));

  if (longname) {
    char rest_of_name[sizeof(struct DRIsym)];

    memset(rest_of_name,0,sizeof(struct DRIsym));
    strncpy(rest_of_name,name+DRI_NAMELEN,sizeof(struct DRIsym));
    fwritex(f,rest_of_name,sizeof(struct DRIsym));
  }
  else {
    int i = DRI_NAMELEN;

    while (szb_extensions--) {
      strncpy(stab.name,name+i,DRI_NAMELEN);
      write16be(stab.type,STYP_XFLAGS);
      write32be(stab.value,XVALUE);
      fwritex(f,&stab,sizeof(struct DRIsym));
      i += DRI_NAMELEN;
    }
  }
}


static int defsymtab(struct GlobalVars *gv,FILE *f,
                     struct LinkedSection **sections)
{
  struct Symbol *sym;
  int i;

  for (i=0; i<3; i++) {
    if (sections[i]) {
      for (sym=(struct Symbol *)sections[i]->symbols.first;
           sym->n.next!=NULL; sym=(struct Symbol *)sym->n.next) {
        if (!discard_symbol(gv,sym)) {
          uint32_t val = sym->value;
          uint16_t t;

          if (sym->type == SYM_ABS) {
            t = STYP_EQUATED;
          }
          else if (sym->type == SYM_RELOC) {
            if (!textbasedsyms)
              val -= sections[i]->base;  /* symbol value as section offset */
            switch (i) {
              case 0: t = STYP_TEXT; break;
              case 1: t = STYP_DATA; break;
              case 2: t = STYP_BSS; break;
            }
          }
          else if (sym->type==SYM_COMMON && gv->dest_object) {
            t = STYP_EXTERNAL;
            val = sym->size;
          }
          else {
            error(33,fff_ataritos.tname,sym->name,sym_bind[sym->bind],
                  sym_type[sym->type],sym_info[sym->info]);
            continue;
          }
          t |= STYP_DEFINED;
          if (sym->bind > SYMB_LOCAL)
            t |= STYP_GLOBAL;

          write_dri_sym(f,sym->name,t,val);
        }
      }
    }
  }
}


static void refsymtab(struct GlobalVars *gv,FILE *f,
                      struct LinkedSection **sections,int idx)
{
  struct Reloc *xref;
  int i;

  for (i=0; i<3; i++) {
    if (sections[i]) {
      for (xref=(struct Reloc *)sections[i]->xrefs.first;
           xref->n.next!=NULL; xref=(struct Reloc *)xref->n.next) {
        write_dri_sym(f,xref->xrefname,STYP_EXTERNAL|STYP_DEFINED,0);
        xref->relocsect.xrefinfo = idx++;  /* remember symbol table index */
      }
    }
  }
}


void tos_writerelocs(struct GlobalVars *gv,FILE *f,
                     struct LinkedSection **sections)
{
  const char *fn = "tos_writerelocs(): ";
  int i;
  struct Reloc *rel;
  struct RelocInsert *ri;
  unsigned long lastoffs = 0;

  for (i=0; i<3; i++) {
    if (sections[i]) {
      sort_relocs(&sections[i]->relocs);
      for (rel=(struct Reloc *)sections[i]->relocs.first;
           rel->n.next!=NULL; rel=(struct Reloc *)rel->n.next) {
        if (ri = rel->insert) {
          if (rel->rtype!=R_ABS || ri->bpos!=0 || ri->bsiz!=32) {
            if (rel->rtype==R_ABS && (ri->bpos!=0 || ri->bsiz!=32))
              error(32,fff_ataritos.tname,reloc_name[rel->rtype],
                    (int)ri->bpos,(int)ri->bsiz,mtaddr(gv,ri->mask),
                    sections[i]->name,rel->offset);
            continue;
          }
        }
        else
          continue;

        if (!lastoffs) {
          /* first relocation offset is 32 bits, the rest are bytes! */
          fwrite32be(f,sections[i]->base + rel->offset);
        }
        else {
          long diff = (sections[i]->base + rel->offset) - lastoffs;

          if (diff < 0) {
            ierror("%snegative offset difference: "
                   "%s(0x%08lx)+0x%08lx - 0x%08lx",fn,sections[i]->name,
                   sections[i]->base,rel->offset,lastoffs);
          }
          while (diff > 254) {
            fwrite8(f,1);
            diff -= 254;
          }
          fwrite8(f,(uint8_t)diff);
        }
        lastoffs = sections[i]->base + rel->offset;
      }
    }
  }

  if (!lastoffs) {
    /* not a single relocation written - write 0-word */
    fwrite32be(f,0);
  }
  else
    fwrite8(f,0);
}


static int secref_type(struct LinkedSection **secs,struct LinkedSection *ls)
{
  static const int rsec_map[] = { 2,1,3 };  /* text, data, bss - reloc type */
  int i;

  for (i=0; i<3; i++) {
    if (secs[i] == ls)
      return rsec_map[i];
  }
  ierror("secref_type: no DRI section: %s",ls->name);
  return 0;
}


static void dri_writerelocs(struct GlobalVars *gv,FILE *f,
                            struct LinkedSection **sections)
{
  const char *fn = "dri_writerelocs(): ";
  int i;

  for (i=0; i<2; i++) {
    if (sections[i]) {
      struct Reloc *rel,*xref;
      unsigned long offs = 0;

      sort_relocs(&sections[i]->relocs);
      rel = (struct Reloc *)sections[i]->relocs.first;
      sort_relocs(&sections[i]->xrefs);
      xref = (struct Reloc *)sections[i]->xrefs.first;

      while (rel->n.next || xref->n.next) {
        struct Reloc *r;
        struct RelocInsert *ri;
        int t;
        int sz = 0;

        if (xref->n.next==NULL || (rel->n.next && rel->offset<xref->offset)) {
          r = rel;
          rel = (struct Reloc *)rel->n.next;
        }
        else {
          r = xref;
          xref = (struct Reloc *)xref->n.next;
        }

        /* determine relocation field width: 0=invalid, 1=16bit, 2=32bit */
        if (ri = r->insert) {
          if (ri->next==NULL && ri->bpos==0 && !(ri->bsiz&15)) {
            if ((sz = ri->bsiz / 16) <= 2) {
              if ((ri->mask & makemask(ri->bsiz)) != makemask(ri->bsiz))
                sz = 0;
            }
          }
        }
        if (!sz) {
          dri_bad_reloc:
          if (ri = r->insert)
            error(32,fff_ataritos.tname,reloc_name[r->rtype],
                  (int)ri->bpos,(int)ri->bsiz,mtaddr(gv,ri->mask),
                  sections[i]->name,r->offset);
          else
            ierror("%smissing RelocInsert for rtype %d at %s+%lu",
                   fn,(int)r->rtype,sections[i]->name,r->offset);
          continue;
        }

        fwritegap(gv,f,r->offset-offs,0);
        offs = r->offset;

        switch (r->rtype) {
          case R_ABS:
            if (r->xrefname)
              t = 4;
            else
              t = secref_type(sections,r->relocsect.lnk);
            break;
          case R_PC:
            if (r->xrefname) {
              t = 6;
              break;
            }
            goto dri_bad_reloc;
          case R_SD:
            if (r->xrefname) {
              t = 4;
              break;
            }
          default:
            goto dri_bad_reloc;
        }

        if (sz == 2)
          fwrite16be(f,5);  /* write longword type indicator to MSW */
        if (t==4 || t==6) {
          /* external reference requires symbol index in bits 3..15 */
          int idx = r->relocsect.xrefinfo;

          if (idx > 0x1fff)
            error(160,fff_ataritos.tname);  /* too many symbols */
          t |= idx << 3;
        }
        fwrite16be(f,t);
        offs += sz << 1;
      }
      fwritegap(gv,f,sections[i]->size+sections[i]->gapsize-offs,0);
    }
  }
}


static void tos_or_dri_output(struct GlobalVars *gv,FILE *f,int exec)
{
  struct LinkedSection *sections[3];
  int nsyms = tos_initwrite(gv,sections);
  int i;

  tos_header(f,sections[0] ? sections[0]->size+sections[0]->gapsize : 0,
             sections[1] ? sections[1]->size+sections[1]->gapsize : 0,
             sections[2] ? sections[2]->size : 0,
             (unsigned long)nsyms*sizeof(struct DRIsym),tos_flags);

  if (exec) {
    for (i=0; i<3; i++)
      calc_relocs(gv,sections[i]);
  }

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

  if (nsyms) {
    if (sozobonx)
      write_dri_sym(f,XNAME,STYP_XFLAGS,XVALUE);
    if (!exec)
      refsymtab(gv,f,sections,sozobonx==TRUE);
    defsymtab(gv,f,sections);
  }

  if (exec)
    tos_writerelocs(gv,f,sections);
  else
    dri_writerelocs(gv,f,sections);
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
/* creates a TOS relocatable object file */
{
  tos_or_dri_output(gv,f,0);
}


static void writeexec(struct GlobalVars *gv,FILE *f)
/* creates a TOS executable file (which is relocatable) */
{
  tos_or_dri_output(gv,f,1);
}


#endif

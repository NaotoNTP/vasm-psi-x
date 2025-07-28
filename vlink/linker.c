/* $VER: vlink linker.c V0.18 (27.12.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2024  Frank Wille
 */


#define LINKER_C
#include "vlink.h"
#include "cpurelocs.h"


static char namebuf[FNAMEBUFSIZE];
static char namebuf2[FNAMEBUFSIZE];

static const char *filetypes[] = {
  "unknown",
  "object",
  "executable",NULL,
  "shared object",NULL,NULL,NULL,
  "library"
};

static const char *sec_names[] = {
  "undefined","code","data","bss",NULL
};


#ifdef DEBUG
#define Dprintf(...) printf(__VA_ARGS__)

#else
#if (defined _WIN32) && (_MSC_VER < 1400)
/* MSVCs older than MSVC2005 do not handle variadic macros; for these
   compilers, we create a dummy function instead of the macro */
static void Dprintf(const char *format, ...) { }
#else
#define Dprintf(...)
#endif
#endif



const char *getobjname(struct ObjectUnit *obj)
/* if library: return "file name(object name)" */
/* else: return "file name" */
{
  const char *fn = obj->lnkfile->filename;

  if (obj->lnkfile->type == ID_LIBARCH) {
    static char *buf;
    const char *on = obj->objname;

    if (strlen(fn)+strlen(on)+2 < FNAMEBUFSIZE) {
      if (buf == namebuf)
        buf = namebuf2;
      else
        buf = namebuf;
      snprintf(buf,FNAMEBUFSIZE,"%s(%s)",fn,on);
      return (buf);
    }
  }
  return (fn);
}


void print_function_name(struct Section *sec,unsigned long offs)
/* Try to determine the function to which the section offset */
/* belongs, by comparing with SYMI_FUNC-type symbol definitions. */
/* If this was successful and the current function is different */
/* from the last one printed, make an output to stderr. */
{
  static const char *infoname[] = { "", "object ", "function " };
  static struct Symbol *last_func;
  struct Symbol *sym,*func=NULL;
  int i;

  for (i=0; i<OBJSYMHTABSIZE; i++) {  /* scan all hash chains */
    for (sym=sec->obj->objsyms[i]; sym; sym=sym->obj_chain) {
      if (sym->relsect == sec) {
        if (sym->info <= SYMI_FUNC) {
          if (sym->type == SYM_RELOC) {
            if ((unsigned long)sym->value <= offs) {
              if (sym->size) {  /* size of function specified? */
                if ((unsigned long)(sym->value+sym->size) > offs) {
                  func = sym;
                  i = OBJSYMHTABSIZE;
                  break;  /* function found! */
                }
              }
              else {  /* no size - find nearest... */
                if (func) {
                  if (sym->value > func->value) {
                    if (func->bind<SYMB_GLOBAL || sym->bind>=SYMB_GLOBAL)
                      func = sym;
                  }
                }
                else
                  func = sym;
              }
            }
          }
        }
      }
    }
  }

  /* print function name */
  if (func && func!=last_func) {
    last_func = func;
    fprintf(stderr,"%s: In %s\"%s\":\n",getobjname(sec->obj),
            infoname[func->info],func->name);
  }
}


static void undef_sym_error(struct Section *sec,struct Reloc *rel,
                            const char *symname)
{
  print_function_name(sec,rel->offset);
  error(21,getobjname(sec->obj),sec->name,rel->offset-sec->offset,symname);
}


static struct Symbol *lnksymbol(struct GlobalVars *gv,struct Section *sec,
                                struct Reloc *xref)
{
  struct Symbol *sym;

  if (sym = findlnksymbol(gv,xref->xrefname))
    return (sym);

  if (fff[gv->dest_format]->lnksymbol)
    return (fff[gv->dest_format]->lnksymbol(gv,sec,xref));

  return (NULL);
}


static char *scan_directory(char *dirname,char *libname,int so_ver)
{
  size_t lnlen=strlen(libname);
  char *dd,*scan,*fname=NULL;
  char maxname[FNAMEBUFSIZE];
  int maxver=0,maxsubver=-1;

  if (so_ver < 0) {
    /* no need to scan the directory */
    fname = libname;
  }
  else {
    if (dd = open_dir(dirname)) {
      while (scan = read_dir(dd)) {
        if (!strncmp(scan,libname,lnlen)) {
          /* found a library archive/shared object name! */
          if (!strcmp(scan,libname) && so_ver==0) {  /* perfect match */
            fname = scan;
            break;
          }
          else {  /* find highest version */
            if (scan[lnlen]=='.' && scan[lnlen+1]) {
              char *p = &scan[lnlen+1];
              int ver = atoi(p++);
              int subver = 0;

              while (*p!='\0' && *p!='.')
                p++;
              if (*p++ == '.')
                subver = atoi(p);

              if (so_ver==0 || so_ver==ver) {
                if ((ver>maxver || (ver==maxver && subver>maxsubver)) &&
                    strlen(scan)<(FNAMEBUFSIZE-1)) {
                  fname = maxname;
                  maxver = ver;
                  maxsubver = subver;
                  strcpy(maxname,scan);
                }
              }
            }
          }
        }
      }
      close_dir(dd);
    }
  }

  if (fname) {
    char *p,*fullpath;

    if (fullpath = path_append(namebuf,dirname,fname,FNAMEBUFSIZE)) {
      if (p = mapfile(fullpath))
        return (p);
    }
  }
  return (NULL);
}


static char *searchlib(struct GlobalVars *gv,char *libname,int so_ver)
{
  struct LibPath *lpn = (struct LibPath *)gv->libpaths.first;
  struct LibPath *nextlpn;
  char *p,*path,*flavour_dir;
  int i,count;
  size_t len;

  while (nextlpn = (struct LibPath *)lpn->n.next) {
    for (count=gv->flavours.n_flavours; count>=0; count--) {
      flavour_dir = gv->flavours.flavour_dir;
      for (flavour_dir[0]='\0',i=0; i<count; i++) {
        if (!path_append(flavour_dir,flavour_dir,gv->flavours.flavours[i],
                         gv->flavours.flavours_len + 1))
          ierror("searchlib(): flavour \"%s\" doesn't fit into path buffer",
                 gv->flavours.flavours[i]);
      }
      len = strlen(lpn->path) + strlen(flavour_dir) + 3;
      if (len < FNAMEBUFSIZE) {
        path = path_append(namebuf2,lpn->path,flavour_dir,FNAMEBUFSIZE);
        if (p = scan_directory(path,libname,so_ver))
          return (p);
      }
    }
    lpn = nextlpn;
  }

  /* scan local directly at last */
  if (p = scan_directory(".",libname,so_ver))
    return (p);

  return (NULL);
}


static char *maplibrary(struct GlobalVars *gv,struct InputFile *ifn)
/* Map a complete file into memory and return its address. */
/* The file's length is returned in *(p-sizeof(size_t)). */
/* All defined library paths will be searched for the file, */
/* before aborting. On success, the complete path of the loaded */
/* file is stored in namebuf[]. */
{
  char *p;
  char libname[FNAMEBUFSIZE];

  if (strlen(ifn->name) < (FNAMEBUFSIZE-16)) {
    if (ifn->dynamic) {
      snprintf(libname,FNAMEBUFSIZE,"lib%s.so",ifn->name);
      if (p = searchlib(gv,libname,ifn->so_ver))
        return (p);
    }
    snprintf(libname,FNAMEBUFSIZE,"lib%s.a",ifn->name);
    if (p = searchlib(gv,libname,-1))
      return (p);
    snprintf(libname,FNAMEBUFSIZE,"%s.lib",ifn->name);
    if (p = searchlib(gv,libname,-1))
      return (p);
  }
  return (NULL);
}


static void addrelref(struct RelRef **rrptr,struct Section *sec)
/* if not already exists, add a new relative reference to another sect. */
{
  struct RelRef *rr;

  if (rr = *rrptr) {
    while (rr->next) {
      if (rr->refsec == sec)
        return;  /* reference already exists */
      rr = rr->next;
    }
  }
  else
    rr = (struct RelRef *)rrptr;

  rr->next = alloczero(sizeof(struct RelRef));
  rr->next->refsec = sec;
}


static bool checkrr(struct RelRef *rr,struct Section *sec)
/* Check if there is a relative reference to the specified section. */
{
  while (rr) {
    if (rr->refsec == sec)
      return (TRUE);
    rr = rr->next;
  }
  return (FALSE);
}


static bool checkrelrefs(struct LinkedSection *ls,struct Section *newsec)
/* Check if there is a relative reference between those sections */
{
  struct Section *sec;

  for (sec=(struct Section *)ls->sections.first;
       sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
    /* check for references to 'newsec' */
    if (checkrr(sec->relrefs,newsec))
      return (TRUE);
    /* check for references from 'newsec' */
    if (checkrr(newsec->relrefs,sec))
      return (TRUE);
  }
  return (FALSE);
}


static char *protstring(uint8_t prot)
/* return pointer to protection string - example: "r---" or "rwxs" */
{
  static char ps[] = "----";
  char *p = ps;

  *p++ = (prot & SP_READ) ? 'r' : '-';
  *p++ = (prot & SP_WRITE) ? 'w' : '-';
  *p++ = (prot & SP_EXEC) ? 'x' : '-';
  *p++ = (prot & SP_SHARE) ? 's' : '-';
  return (ps);
}


static uint8_t cmpsecflags(struct GlobalVars *gv,struct LinkedSection *ls,
                           struct Section *sec)
/* return 0xff if sections are incompatible, otherwise return new flags */
{
  uint8_t old = ls->flags;
  uint8_t new = sec->flags;

  if (ls->ld_flags & LSF_NOLOAD)
    new &= ~SF_ALLOC;
  else if ((old&SF_ALLOC) != (new&SF_ALLOC))
    return 0xff;

  return (fff[gv->dest_format]->cmpsecflags != NULL) ?
         fff[gv->dest_format]->cmpsecflags(ls,sec) : (old | new);
}


static void merge_sec_attrs(struct LinkedSection *lsn,struct Section *sec,
                            uint8_t target_flags)
{
  if (lsn->type == ST_UNDEFINED) {
    lsn->type = sec->type;
    lsn->flags = sec->flags;
    lsn->memattr = sec->memattr;
  }
  else {
    if (lsn->type==ST_UDATA && sec->type==ST_DATA)
      lsn->type = ST_DATA;  /* a DATA-BSS section */
    lsn->flags &= SF_PORTABLE_MASK;
    lsn->flags |= (sec->flags&(SF_SMALLDATA|SF_LINKONCE)) | target_flags;
  }

  if (lsn->protection != sec->protection) {
    if (lsn->protection!=0 &&
        (lsn->protection & sec->protection)!=sec->protection &&
        !listempty(&lsn->sections)) {
      /* merge protection-flags of the two sections */
      char prot1[5],prot2[5];
      strncpy(prot1,protstring(lsn->protection),5);
      strncpy(prot2,protstring(lsn->protection|sec->protection),5);
      strcpy(namebuf2,getobjname(((struct Section *)lsn->sections.last)->obj));
#if 0 /* @@@ FIXME */
      error(22,lsn->name,prot1,namebuf2,prot2,getobjname(sec->obj));
#else
      error(22,lsn->name,prot1,prot2,getobjname(sec->obj));
#endif
    }
    lsn->protection |= sec->protection;
  }

  /* merge memory attributes */
  if (lsn->memattr != sec->memattr) {
    Dprintf("Memory attributes of section %s merged from %08lx to %08lx"
            " in %s.\n",lsn->name,lsn->memattr,lsn->memattr|sec->memattr,
            getobjname(sec->obj));
    lsn->memattr |= sec->memattr;
  }

  if (sec->alignment > lsn->alignment) {
    /* increase alignment to fit the needs of the new section */
    if (lsn->alignment && !listempty(&lsn->sections)) {
      strcpy(namebuf2,getobjname(((struct Section *)lsn->sections.last)->obj));
      Dprintf("Alignment of section %s was changed from "
              "%d in %s to %d in %s.\n",
              lsn->name,1<<lsn->alignment,namebuf2,
              1<<sec->alignment,getobjname(sec->obj));
    }
    lsn->alignment = sec->alignment;
  }
}


static int sec_same_attr(struct LinkedSection *ls,struct Section *sec)
{
  if (ls->protection==sec->protection &&
      (!ls->memattr || !sec->memattr || ls->memattr==sec->memattr)) {
    return (ls->flags & (SF_SMALLDATA|(~SF_PORTABLE_MASK))) ==
           (sec->flags & (SF_SMALLDATA|(~SF_PORTABLE_MASK)));
  }
  return 0;
}


static struct LinkedSection *get_matching_lnksec(struct GlobalVars *gv,
                                                 struct Section *sec,
                                                 struct LinkedSection *myls)
/* find a LinkedSection node which matches the attributes of the
   specified section, but which is different from 'myls' */
{
  struct LinkedSection *lsn = (struct LinkedSection *)gv->lnksec.first;
  struct LinkedSection *nextlsn;
  uint8_t f;
  int tl;

  while (nextlsn = (struct LinkedSection *)lsn->n.next) {
    if (lsn != myls && ((f = cmpsecflags(gv,lsn,sec)) != 0xff)) {
      f &= ~SF_PORTABLE_MASK;
      if (lsn->flags & sec->flags & SF_LINKONCE)
        continue;  /* link only once */

      if (!gv->dest_object) {
        /* target-specific linking */
        if ((tl = fff[gv->dest_format]->targetlink(gv,lsn,sec)) > 0) {
          /* target demands merge of sections */
          Dprintf("targetlink: %s(%s) -> %s\n",getobjname(sec->obj),
                  sec->name,lsn->name);
          merge_sec_attrs(lsn,sec,f);
          return (lsn);
        }
      }
      else
        tl = 0;

      if (tl == 0) {  /* target wants to use the default rules */

        if (!gv->dest_object) {
          /* for final executable only: */

          if (gv->small_code) {
            if (lsn->type==ST_CODE && sec->type==ST_CODE) {
              /* merge all code sections */
              Dprintf("smallcode: %s(%s) -> %s\n",getobjname(sec->obj),
                      sec->name,lsn->name);
              merge_sec_attrs(lsn,sec,f);
              return (lsn);
            }
          }

          if (gv->small_data) {
            if ((lsn->type==ST_DATA || lsn->type==ST_UDATA) &&
                (sec->type==ST_DATA || sec->type==ST_UDATA)) {
              /* merge all data and bss sections */
              Dprintf("smalldata: %s(%s) -> %s\n",getobjname(sec->obj),
                      sec->name,lsn->name);
              merge_sec_attrs(lsn,sec,f);
              return (lsn);
            }
          }

          if (!gv->multibase && (lsn->flags & sec->flags & SF_SMALLDATA)) {
            Dprintf("sd-refs: %s(%s) -> %s\n",getobjname(sec->obj),
                    sec->name,lsn->name);
            merge_sec_attrs(lsn,sec,f);
            return (lsn);
          }
        }  /* final executable only */

        /* check merge-options -mrel, -mtype, -mattr, -mall */
        if (gv->merge_all ||
            (gv->merge_same_type && lsn->type==sec->type) ||
            (gv->merge_same_attr && sec_same_attr(lsn,sec))) {
          merge_sec_attrs(lsn,sec,f);
          return (lsn);
        }
        if (gv->auto_merge && checkrelrefs(lsn,sec)) {
          /* we must link them together, because there are rel. refs */
          if ((lsn->type==ST_CODE || sec->type==ST_CODE) &&
              lsn->type != sec->type)
            /* forces a maybe unwanted combination of code and data */
            error(58,sec_names[lsn->type],lsn->name,sec_names[sec->type],
                  sec->name,getobjname(sec->obj));
          Dprintf("relrefs: %s(%s) -> %s\n",getobjname(sec->obj),
                  sec->name,lsn->name);
          merge_sec_attrs(lsn,sec,f);
          return (lsn);
        }

        /* standard check, if sections could be merged */
        if (myls == NULL) {
          /* check for same type and same name or no name */
          if (lsn->type==sec->type &&
              (*(sec->name)=='\0' || !SECNAMECMP(sec,lsn))) {
            Dprintf("name: %s(%s) -> %s\n",getobjname(sec->obj),
                    sec->name,lsn->name);
            merge_sec_attrs(lsn,sec,f);
            return (lsn);
          }
          else if (*(sec->name)=='\0' && sec->size==0) {
            /* no name and no contents - may contain abs symbols only */
            return (lsn);
          }

          if (!gv->dest_object) {
            /* COMMON sections are merged with any BSS-type section */
            if (is_common_sec(gv,sec)) {
              if (lsn->type==ST_UDATA && (lsn->flags & SF_UNINITIALIZED)) {
                Dprintf("common: %s(%s) -> %s\n",getobjname(sec->obj),
                        sec->name,lsn->name);
                merge_sec_attrs(lsn,sec,f);
                return (lsn);
              }
            }
          }
        }

      } /* default rules */
    }
    lsn = nextlsn;
  }
  return (NULL);
}


static struct Section *last_initialized(struct LinkedSection *ls)
/* search for bss-sections in reverse order, beginning at the end of
   the section-list, and return a pointer to the first initialized one,
   or NULL when all sections were uninitialized */
{
  struct Section *s;

  for (s=(struct Section *)ls->sections.last; s->n.pred!=NULL;
       s=(struct Section *)s->n.pred) {
    if (!(s->flags & SF_UNINITIALIZED))
      return (s);
  }
  return (NULL);
}


static unsigned long allocate_common(struct GlobalVars *gv,bool really,
                                     struct Section *sec,lword addr)
/* Allocate all common symbols to section 'sec' at 'addr' and
   returns number of total bytes allocated.
   Only returns total bytes without symbol allocation with 'really' = FALSE. */
{
  unsigned long abytes,alloc=0;
  struct Symbol *sym;
  int i;

  for (i=0; i<SYMHTABSIZE; i++) {
    for (sym=gv->symbols[i]; sym; sym=sym->glob_chain) {
      /* common symbol from this section name? */
      if (sym->relsect==sec && sym->type==SYM_COMMON) {

        /* allocate and transform into SYM_RELOC */
        abytes = comalign(addr+alloc,sym->value);
        if (really) {
          sym->value = (lword)((addr+alloc) - sec->va) + abytes;
          sym->type = SYM_RELOC;
          if (gv->map_file)
            fprintf(gv->map_file,"Allocating common %s: %x at %llx hex\n",
                    sym->name,(int)sym->size,
                    (unsigned long long)sec->va+sym->value);
        }
        alloc += abytes + sym->size;
      }
    }
  }

  if (really)
    sec->size += alloc;

  return alloc;
}


void print_symbol(struct GlobalVars *gv,FILE *f,struct Symbol *sym)
/* print symbol name, type, value, etc. */
{
  if (sym->type == SYM_COMMON)
    fprintf(f,"  %s: %s%s%s, alignment %d, size %d\n",sym->name,
            sym_bind[sym->bind],sym_type[sym->type],sym_info[sym->info],
            (int)sym->value,(int)sym->size);
  else if (sym->type == SYM_INDIR)
    fprintf(f,"  %s: %s%s%s, referencing %s\n",sym->name,
            sym_bind[sym->bind],sym_type[sym->type],sym_info[sym->info],
            sym->indir_name);
  else {
#if 0
    fprintf(f,"  %s: %s%s%s, value 0x%llx, size %d\n",sym->name,
            sym_bind[sym->bind],sym_type[sym->type],sym_info[sym->info],
            (unsigned long long)sym->value,(int)sym->size);
#else
    fprintf(f,"  0x%0*llx %s: %s%s%s, size %d\n",
            gv->bits_per_taddr/4,(unsigned long long)sym->value,sym->name,
            sym_bind[sym->bind],sym_type[sym->type],sym_info[sym->info],
            (int)sym->size);
#endif
  }
}


bool trace_sym_access(struct GlobalVars *gv,const char *name)
/* check if the symbol with this name should be traced */
{
  struct SymNames *sn;

  if (gv->trace_syms) {
    sn = gv->trace_syms[elf_hash(name)%TRSYMHTABSIZE];
    while (sn) {
      if (!strcmp(name,sn->name))
        return (TRUE);  /* symbol found! */
      sn = sn->next;
    }
  }
  return (FALSE);
}


static void add_undef_syms(struct GlobalVars *gv)
/* create dummy xreferences for symbols marked as undefined
   (-u option or EXTERN script directive) */
{
  struct SymNames *sn;

  if (sn = gv->undef_syms) {
    static uint8_t dat[1];  /* contents of dummy section */
    struct ObjectUnit *obj;
    struct Section *sec = NULL;
    struct Section *dummysec;
    struct Reloc *r;

    /* search first section in an object from the command line */
    for (obj=(struct ObjectUnit *)gv->selobjects.first;
         obj->n.next!=NULL; obj=(struct ObjectUnit *)obj->n.next) {
      if (!listempty(&obj->sections)) {
        sec = (struct Section *)obj->sections.first;
        break;
      }
    }
    if (sec == NULL)
      ierror("add_undef_syms(): no objects or no sections on command line");

    /* make artificial object for external references */
    obj = art_objunit(gv,"UNDEFSYMBOLS",dat,0);
    obj->flags |= OUF_LINKED;
    dummysec = create_section(obj,sec->name,dat,0);
    dummysec->type = sec->type;
    dummysec->protection = sec->protection;
    dummysec->flags = sec->flags;
    addtail(&obj->sections,&dummysec->n);
    addtail(&gv->selobjects,&obj->n);

    do {
      /* add a dummy references of type R_NONE to the dummy section */
      r = newreloc(gv,dummysec,sn->name,NULL,0,0,R_NONE,0);
      addreloc(dummysec,r,0,0,-1);
    }
    while (sn = sn->next);
  }
}


static void make_dynobj(struct GlobalVars *gv)
{
  if (gv->dynobj == NULL) {
    gv->dynobj = art_objunit(gv,"DYNAMIC",NULL,0);
    gv->dynobj->flags |= OUF_LINKED;
    addhead(&gv->selobjects,&gv->dynobj->n);
  }
}


static struct Symbol *dyn_entry(struct GlobalVars *gv,DynArg arg,int entrytype)
{
  make_dynobj(gv);

  if (fff[gv->dest_format]->dynentry)
    return fff[gv->dest_format]->dynentry(gv,arg,entrytype);
  
  /* dynamic symbol reference not supported by target */
  error(126,fff[gv->dest_format]->tname);
  return NULL;
}


static struct Symbol *dyn_ext_entry(struct GlobalVars *gv,
                                    struct Symbol *xdef,struct Reloc *xref,
                                    int entrytype)
/* make a dyn_entry for an external reference */
{
  if (xdef->relsect->obj->lnkfile->type!=ID_SHAREDOBJ
      && entrytype==PLT_ENTRY) {
    /* resolve PLT relocation to a local function directly */
    switch (xref->rtype) {
      case R_PLT:
        xref->rtype = R_ABS;
        break;
      case R_PLTPC:
        xref->rtype = R_PC;
        break;
    }
  }
  else {
    DynArg a;
    struct Symbol *sym;

    a.sym = xdef;
    sym = dyn_entry(gv,a,entrytype);
    if (sym)
      return sym;
  }
  return xdef;
}


static void dyn_reloc_entry(struct GlobalVars *gv,struct Reloc *reloc,
                            int entrytype)
/* make a dyn_entry for a local GOT/PLT reference, fix reloc for entry */
{
  if (reloc->relocsect.ptr->obj->lnkfile->type!=ID_SHAREDOBJ
      && entrytype==PLT_LOCAL) {
    /* resolve PLT relocation to a local function directly */
    switch (reloc->rtype) {
      case R_PLT:
        reloc->rtype = R_ABS;
        break;
      case R_PLTPC:
        reloc->rtype = R_PC;
        break;
    }
  }
  else {
    DynArg a;
    struct Symbol *xdef;

    a.rel = reloc;
    if (xdef = dyn_entry(gv,a,entrytype)) {
      reloc->relocsect.ptr = xdef->relsect;
      reloc->addend = xdef->value;
    }
    else
      ierror("dyn_reloc_entry(): new xdef is NULL");
  }
}


static void dyn_so_needed(struct GlobalVars *gv,struct ObjectUnit *ou)
{
  DynArg a;

  a.name = ou->objname;
  dyn_entry(gv,a,SO_NEEDED);
}


static void dyn_export(struct GlobalVars *gv,struct Symbol *sym)
{
  DynArg a;

  if (!(sym->flags & SYMF_DYNLINK)) {
    sym->flags |= SYMF_DYNEXPORT;
    a.sym = sym;
    dyn_entry(gv,a,SYM_ENTRY);
  }
}


static void init_dynlink(struct GlobalVars *gv)
{
  make_dynobj(gv);

  /* target-specific init */
  if (fff[gv->dest_format]->dyninit)
    fff[gv->dest_format]->dyninit(gv);
}


static void set_last_sec_reloc(struct GlobalVars *gv,
                               struct Section *s,struct Reloc *r)
/* Check if Reloc is the new last relocation in this section and remember
   the offset behind its relocation field. */
{
  int b = gv->bits_per_tbyte;
  struct RelocInsert *ri;
  unsigned long rend;

  for (ri=r->insert; ri!=NULL; ri=ri->next) {
    rend = r->offset + (ri->bpos + ri->bsiz + b - 1) / b;
    if (rend > s->last_reloc)
      s->last_reloc = rend;
  }
}


static void ref_all_sections(struct GlobalVars *gv)
/* find referenced sections for -gc-empty */
{
  struct ObjectUnit *obj;
  struct Section *sec;
  struct Reloc *r;

  for (obj=(struct ObjectUnit *)gv->selobjects.first;
       obj->n.next!=NULL; obj=(struct ObjectUnit *)obj->n.next) {
    for (sec=(struct Section *)obj->sections.first;
         sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
      /* flag all referenced sections */
      for (r=(struct Reloc *)sec->xrefs.first;
           r->n.next!=NULL; r=(struct Reloc *)r->n.next) {
        if (r->relocsect.symbol!=NULL && r->relocsect.symbol->relsect!=NULL)
          r->relocsect.symbol->relsect->flags |= SF_REFERENCED;
      }
      for (r=(struct Reloc *)sec->relocs.first;
           r->n.next!=NULL; r=(struct Reloc *)r->n.next) {
        if (r->relocsect.ptr != NULL)
          r->relocsect.ptr->flags |= SF_REFERENCED;
      }
    }
  }
}


static void ref_section(struct Section *sec)
/* find referenced sections for -gc-all */
{
  struct Reloc *r;

  if (sec == NULL)
    return;

  /* nothing to do, when section was already marked as referenced */
  if ((sec->flags & SF_REFERENCED) != 0)
    return;

  /* mark section as referenced */
  sec->flags |= SF_REFERENCED;
  Dprintf("  %s(%s) referenced\n",getobjname(sec->obj),sec->name);

  /* Find all referenced sections from here, by looking at all relocations
     and symbol references. */

  for (r=(struct Reloc *)sec->xrefs.first;
       r->n.next!=NULL; r=(struct Reloc *)r->n.next) {
    if (r->relocsect.symbol != NULL)
      ref_section(r->relocsect.symbol->relsect);
  }

  for (r=(struct Reloc *)sec->relocs.first;
       r->n.next!=NULL; r=(struct Reloc *)r->n.next) {
    ref_section(r->relocsect.ptr);
  }
}


static void ref_prot_symbols(struct GlobalVars *gv)
/* mark all sections with global and local protected symbols as referenced */
{
  struct SymNames *sn = gv->prot_syms;
  struct ObjectUnit *obj;
  struct Symbol *psym;

  /* find global protected symbols */
  for (sn=gv->prot_syms; sn!=NULL; sn=sn->next) {
    if (psym = findsymbol(gv,NULL,sn->name,0))
      ref_section(psym->relsect);
  }

  /* find local protected symbols in all object units */
  for (obj=(struct ObjectUnit *)gv->selobjects.first;
       obj->n.next!=NULL; obj=(struct ObjectUnit *)obj->n.next) {
    for (sn=gv->prot_syms; sn!=NULL; sn=sn->next) {
      if (psym = findlocsymbol(gv,obj,sn->name))
        ref_section(psym->relsect);
    }
  }
}


static lword next_bank_address(struct GlobalVars *gv,struct LinkedSection *ls)
{
  lword bksize,addr;

  if (!(bksize = ls->banksize)) {
    error(163,ls->name);  /* no valid bank size */
    /* set a dummy to continue and to avoid further errors */
    bksize = 1LL << (gv->bits_per_taddr - 1);
    ls->banksize = bksize;
  }
  addr = ls->relocmem->current;
  return addr + (bksize - addr % bksize);
}


static lword try_update_address(struct GlobalVars *gv,
                                struct LinkedSection *ls,struct Section *sec)
/* Update VMA/LMA to see if the input section fits into the given
   memory regions, return the failed address on error.
   WARNING: VMA and LMA are really updated, although commons are not
   really allocated. You have to save/restore the pointers. */
{
  lword failed;

  if (failed = align_address(ls->relocmem,ls->destmem,sec->alignment))
    return failed;

  if (failed = update_address(ls->relocmem,ls->destmem,sec->size))
    return failed;

  if (is_common_sec(gv,sec) &&
      (!gv->dest_object || gv->alloc_common) && sec->type==ST_UDATA) {
        if (failed = update_address(ls->relocmem,ls->destmem,
                                    allocate_common(gv,FALSE,sec,
                                                    ls->relocmem->current)))
          return failed;
  }
  return 0; /* ok */
}


static bool ld_section_has_space(struct GlobalVars *gv,
                                 struct LinkedSection *ls,struct Section *sec)
/* Check, if the associated memory region of the output section 'ls' has
   sufficient space for our new section 'sec' to merge. */
{
  lword ra = ls->relocmem->current;
  lword da = ls->destmem->current;
  lword failed;

  if (ls->banksize) {
    lword ba = next_bank_address(gv,ls);

    if (!(failed = try_update_address(gv,ls,sec)) &&
        ls->relocmem->current > ba) {
      /* bank-alignment required as sec crosses bank borders */
      sec->internal_flags |= ILF_BANKALIGN;
      ls->relocmem->current = ra;
      ls->destmem->current = da;
      update_address(ls->relocmem,ls->destmem,(ba-ra)+ls->bankoffs);
      failed = try_update_address(gv,ls,sec);
      if (!failed && ls->relocmem->current > ba+ls->bankoffs+ls->banksize)
        error(164,getobjname(sec->obj),sec->name,ls->banksize);
    }
  }
  else
    failed = try_update_address(gv,ls,sec);

  /* restore memory region pointers after test */
  ls->relocmem->current = ra;
  ls->destmem->current = da;

  if (failed) {
    /* remember out of range address of section */
    sec->internal_flags |= ILF_BADADDRINVA;
    sec->va = failed;
    return FALSE;
  }
  sec->internal_flags &= ~ILF_BADADDRINVA;
  return TRUE;
}


static void merge_ld_section(struct GlobalVars *gv,struct Patterns *pat,
                             struct LinkedSection *ls,struct Section *sec)
/* Merge new section 'sec' into the output section 'ls'. */
{
  if (sec->internal_flags & ILF_BANKALIGN) {
    /* align to beginning of next bank first */
    update_address(ls->relocmem,ls->destmem,
                   (next_bank_address(gv,ls) - ls->relocmem->current)
                   + ls->bankoffs);
  }
  align_address(ls->relocmem,ls->destmem,sec->alignment);
  sec->va = ls->relocmem->current;
  update_address(ls->relocmem,ls->destmem,sec->size);

  /* allocate COMMON symbols, if required */
  if (is_common_sec(gv,sec) &&
      (!gv->dest_object || gv->alloc_common) && sec->type==ST_UDATA) {
    update_address(ls->relocmem,ls->destmem,
                   allocate_common(gv,TRUE,sec,ls->relocmem->current));
  }

  sec->offset = sec->va - ls->base;
  sec->filldata = gv->filldata;
  sec->lnksec = ls;
  addtail(&ls->sections,remnode(&sec->n));

  if (!is_ld_script(sec->obj) && (pat->flags & PFL_KEEP)) {
    /* @@@ KEEP for one section will prevent all merged
       sections from being deleted - ok??? */
    ls->ld_flags |= LSF_PRESERVE;
    ls->flags |= SF_ALLOC;  /* @@@ keep implies ALLOC? */
  }
}


static void merge_seclist(struct GlobalVars *gv,struct list *seclist)
{
  struct LinkedSection *ls;
  struct Section *sec,*nextsec;

  do {
    bool create_allowed = TRUE;

    sec = (struct Section *)seclist->first;

    while (nextsec = (struct Section *)sec->n.next) {
      ls = get_matching_lnksec(gv,sec,NULL);

      if (!ls && create_allowed) {
        Dprintf("new: %s(%s) -> %s\n",getobjname(sec->obj),
                sec->name,sec->name);
        ls = create_lnksect(gv,sec->name,sec->type,sec->flags,
                            sec->protection,sec->alignment,sec->memattr);
        create_allowed = FALSE;
      }

      if (ls) {
        /* approximate section's size by assuming a base of 0 */
        ls->size += sec->size + align(ls->size,sec->alignment);
        addtail(&ls->sections,remnode(&sec->n));
        sec->lnksec = ls;
      }

      sec = nextsec;
    }
  }
  while (!(listempty(seclist)));
}


static int sym_addr_cmp(const void *left,const void *right)
/* qsort: compare symbol addresses */
{
  uint64_t addrl = (*(struct Symbol **)left)->value;
  uint64_t addrr = (*(struct Symbol **)right)->value;

  return (addrl<addrr) ? -1 : ((addrl>addrr) ? 1 : 0);
}


void linker_init(struct GlobalVars *gv)
{
  initlist(&gv->linkfiles);
  initlist(&gv->selobjects);
  initlist(&gv->libobjects);
  initlist(&gv->sharedobjects);
  gv->symbols = alloc_hashtable(SYMHTABSIZE);
  initlist(&gv->pripointers);
  initlist(&gv->scriptsymbols);
  gv->got_base_name = gotbase_name;
  gv->plt_base_name = pltbase_name;
  gv->ptr_alignment = fff[gv->dest_format]->ptr_alignment;

  if (gv->reloctab_format != RTAB_UNDEF) {
    if (!(fff[gv->dest_format]->rtab_mask & gv->reloctab_format)) {
      error(122,fff[gv->dest_format]->tname);
      gv->reloctab_format = fff[gv->dest_format]->rtab_format;
    }
  }
  else
    gv->reloctab_format = fff[gv->dest_format]->rtab_format;

  /* init destination format */
  if (fff[gv->dest_format]->init != NULL)
    fff[gv->dest_format]->init(gv,FFINI_DESTFMT);
}


void linker_load(struct GlobalVars *gv)
/* load all objects and libraries into memory, identify their */
/* format, then read all symbols and convert into internal format */
{
  struct InputFile *ifn;
  struct LinkFile *lf;
  uint8_t *objptr;
  const char *objname;
  unsigned long objlen;
  int i,ff;

  init_ld_script(gv);       /* pre-parse linker script, when available */
  if (listempty(&gv->inputlist))
    error(6);  /* no input files */

  if (gv->use_ldscript) {
    gv->common_sec_name = "COMMON";
    gv->scommon_sec_name = ".scommon";
  }
  else {
    gv->common_sec_name = fff[gv->dest_format]->bssname ?
                          fff[gv->dest_format]->bssname : "COMMON";
    gv->scommon_sec_name = fff[gv->dest_format]->sbssname ?
                           fff[gv->dest_format]->sbssname : ".scommon";
  }
  gv->common_sec_hash = elf_hash(gv->common_sec_name);
  gv->scommon_sec_hash = elf_hash(gv->scommon_sec_name);

  if (gv->trace_file)
    fprintf(gv->trace_file,"\nLoading files:\n\n");

  for (ifn=(struct InputFile *)gv->inputlist.first;
       ifn->n.next!=NULL; ifn=(struct InputFile *)ifn->n.next) {
    if (ifn->lib) {
      if (!(objptr = (uint8_t *)maplibrary(gv,ifn))) {
        snprintf(namebuf,FNAMEBUFSIZE,"-l%s",ifn->name);
        error(8,namebuf);  /* cannot open -lxxx */
      }
    }
    else {
      if (objptr = (uint8_t *)mapfile(ifn->name))
        strcpy(namebuf,ifn->name);
      else
        error(8,ifn->name);  /* cannot open xxx */
    }
    objlen = *(size_t *)(objptr - sizeof(size_t));
    objname = base_name(namebuf);

    /* determine the object's file format */
    for (i=0,ff=ID_UNKNOWN; fff[i]; i++) {
      if ((ff = (fff[i]->identify)(gv,(char *)objname,objptr,objlen,ifn->lib))
          != ID_UNKNOWN)
        break;
    }
    if (ff == ID_UNKNOWN)
      error(11,objname);  /* File format not recognized */

    if (ff != ID_IGNORE) {
      /* use endianness of first object read */
      if (gv->endianness < 0)
        gv->endianness = fff[i]->endianness;
      else if (fff[i]->endianness>=0 && gv->endianness!=fff[i]->endianness)
        error(61,objname);  /* endianness differs from previous objects */

      /* determine bits per taddr from highest value in all input files */
      if (fff[i]->addr_bits > gv->bits_per_taddr)
        gv->bits_per_taddr = fff[i]->addr_bits;

      /* create new link file node */
      lf = (struct LinkFile *)alloc(sizeof(struct LinkFile));
      lf->pathname = allocstring(namebuf);
      lf->filename = base_name(lf->pathname);
      lf->data = objptr;
      lf->length = objlen;
      lf->format = (uint8_t)i;
      lf->type = (uint8_t)ff;
      lf->flags = ifn->flags;
      lf->renames = ifn->renames;
      if (gv->trace_file)
        fprintf(gv->trace_file,"%s (%s %s)\n",namebuf,fff[i]->tname,
                                              filetypes[ff]);
      addtail(&gv->linkfiles,&lf->n);
    }
  }

  if (gv->endianness < 0) {
    /* When endianness is still unknown, after identifying all input files,
       we take it from the destination format. */
    gv->endianness = fff[gv->dest_format]->endianness;

    /* The destination format didn't define the endianness either?
       Then guess by using the host endianness. */
    if (gv->endianness < 0) {
      gv->endianness = host_endianness();
      error(148);  /* warn about it */
    }
  }

  if (gv->bits_per_tbyte == 0)
    gv->bits_per_tbyte = 8;  /* default to 8-bit bytes */
  gv->octets_per_tbyte = (gv->bits_per_tbyte + 7) / 8;

  /* target format has priority, provided it defines the bits per taddr */
  if (fff[gv->dest_format]->addr_bits > 0) {
    if (gv->bits_per_taddr == 0)
      gv->bits_per_taddr = fff[gv->dest_format]->addr_bits;
    else if (gv->bits_per_taddr != fff[gv->dest_format]->addr_bits)
      error(149);  /* mismatching taddr sizes */
  }
  if (gv->bits_per_taddr == 0)
    ierror("Neither input nor output formats define target address size");

  if (gv->tbytes_per_taddr == 0)
    gv->tbytes_per_taddr = gv->bits_per_taddr / gv->bits_per_tbyte;

  /* read all files and convert them into internal format */
  for (lf=(struct LinkFile *)gv->linkfiles.first;
       lf->n.next!=NULL; lf=(struct LinkFile *)lf->n.next)
    fff[lf->format]->readconv(gv,lf);

  collect_constructors(gv); /* scan them for con-/destructor functions */
  add_undef_syms(gv);       /* put syms. marked as undef. into 1st sec. */
}


static void libpullmsg(struct GlobalVars *gv,struct ObjectUnit *pull_unit,
                       struct Reloc *xref,uint32_t cmask)
{
  if (gv->map_file) {
    fprintf(gv->map_file,"%s",pull_unit->lnkfile->pathname);
    if (pull_unit->lnkfile->type == ID_LIBARCH)
      fprintf(gv->map_file," (%s)",pull_unit->objname);
    fprintf(gv->map_file," needed due to %s",xref->xrefname);
    if (xref->flags & RELF_CMASK)
      fprintf(gv->map_file,"(%x)",(unsigned)cmask);
    fputc('\n',gv->map_file);
  }
}


void linker_resolve(struct GlobalVars *gv)
/* Resolve all symbol references and pull the required objects into */
/* the gv->selobjects list. */
{
  bool last_actions_done = FALSE;
  bool pseudo_dynlink = (fff[gv->dest_format]->flags&FFF_PSEUDO_DYNLINK)!=0;
  struct ObjectUnit *obj = (struct ObjectUnit *)gv->selobjects.first;

  if (gv->dest_sharedobj || gv->dyn_exp_all) {
    gv->dynamic = TRUE;
    init_dynlink(gv);
  }
  else
    gv->dynamic = FALSE;  /* set to true, when first shared object found */

  if (gv->trace_file)
    fprintf(gv->trace_file,"\nDigesting symbol information:\n\n");

  if (obj->n.next == NULL)
    return;  /* no objects in list */

  do {
    struct Section *sec;
    struct Reloc *xref;
    struct Symbol *xdef;
    struct ObjectUnit *pull_unit;
    uint32_t cmask;

    /* all sections of this object are checked for external references */
    for (sec=(struct Section *)obj->sections.first;
         sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {

      for (xref=(struct Reloc *)sec->xrefs.first;
           xref->n.next!=NULL; xref=(struct Reloc *)xref->n.next) {

        /* remember common mask, when set */
        if (xref->flags & RELF_CMASK)
          cmask = xref->relocsect.cmask->common_mask;
        else
          cmask = 0;

        /* preset as unresolved; warning: union! resets also cmask, id, etc. */
        xref->relocsect.symbol = NULL;

        if (xref->rtype == R_LOADREL) {
          /* addend offsets to load address, nothing to resolve */
          continue;
        }

        if ((xref->flags & RELF_WEAK) &&
            (gv->dest_object || gv->dest_sharedobj)) {
          /* resolve weak symbols only in executables */
          continue;
        }

        /* find a global symbol with this name in any object or library */
        xdef = findsymbol(gv,sec,xref->xrefname,cmask);

        if (xdef!=NULL && xref->rtype==R_LOCALPC) {
          /* R_LOCALPC only accepts symbols which are defined in the
             same ObjectUnit as the reference. */
          if (xdef->relsect->obj != obj)
            xdef = NULL;  /* discard symbol from other object */
        }

        if (xdef == NULL) {
          /* check if reference can be resolved by a linker symbol */
          if (!(xdef = lnksymbol(gv,sec,xref))) {

            /* ref. to undefined symbol is only an error for executables */
            if (!gv->dest_object && !gv->dest_sharedobj) {
              if (xref->flags & RELF_WEAK) {
                /* weak references default to absolute 0 */
                xdef = addlnksymbol(gv,xref->xrefname,0,SYM_ABS,0,
                                    SYMI_NOTYPE,SYMB_GLOBAL,0);
              }
              else {
                print_function_name(sec,xref->offset);
                error(21,getobjname(sec->obj),sec->name,xref->offset,
                      xref->xrefname);
                continue;
              }
            }
            else
              continue;
          }
        }
        /* reference has been resolved after this point */
        if (trace_sym_access(gv,xref->xrefname))
          fprintf(stderr,"Symbol %s referenced from %s\n",
                  xref->xrefname,getobjname(sec->obj));

        /* turn LOCALPC into a PC reloc, once resolved by a local symbol */
        if (xref->rtype == R_LOCALPC)
          xref->rtype = R_PC;

        /* link with symbol's unit */
        if (xdef->relsect && xdef->relsect->type != ST_TMP) {
          pull_unit = xdef->relsect->obj;

          switch (pull_unit->lnkfile->type) {

            case ID_ARTIFICIAL:
              /* linker-generated object - insert it behind the curr. obj. */
              if (!(pull_unit->flags & OUF_LINKED)) {
                if (gv->map_file) {
                  fprintf(gv->map_file,
                          "artificial object (%s) created due to %s\n",
                          pull_unit->objname,xref->xrefname);
                  /* "artificial object (name.o) created due to @__name" */
                }
                insertbehind(&obj->n,&pull_unit->n);
                pull_unit->flags |= OUF_LINKED;
              }
              /* turn into a normal object */
              pull_unit->lnkfile->type = ID_OBJECT;
              xref->relocsect.symbol = xdef;  /* xref was resolved */
              break;

            case ID_SHAREDOBJ:
              if (!(pull_unit->flags & OUF_LINKED)) {
                if (gv->map_file) {
                  libpullmsg(gv,pull_unit,xref,cmask);
                  /* Example: "/usr/lib/libc.so.12.0 needed due to _atexit" */
                }
                insertbehind(&obj->n,remnode(&pull_unit->n));
                add_priptrs(gv,pull_unit);

                pull_objunit(gv,pull_unit);

                if (!pseudo_dynlink) {
                  if (!gv->dynamic) {
                    gv->dynamic = TRUE;  /* we're doing dynamic linking! */
                    init_dynlink(gv);
                  }
                  /* declare the shared object as "needed" */
                  dyn_so_needed(gv,pull_unit);
                }
              }

              if (!gv->dest_object && !pseudo_dynlink) {
                /* ref. to a shared object's symbol needs special treatment */
                switch (xref->rtype) {
                  case R_ABS:
                  case R_PC:
                    if (xdef->info == SYMI_FUNC) {
                      /* abs. or rel. function calls create a PLT entry */
                      xdef = dyn_ext_entry(gv,xdef,xref,PLT_ENTRY);
                    }
                    else {
                      /* alloc bss space and R_COPY from shared object */
                      xdef = dyn_ext_entry(gv,xdef,xref,BSS_ENTRY);
                    }
                    break;
                  case R_GOT:
                  case R_GOTPC:
                  case R_GOTOFF:
                  case R_PLT:
                  case R_PLTPC:
                  case R_PLTOFF:
                  case R_GLOBDAT:
                  case R_JMPSLOT:
                  case R_COPY:
                    /* types are already correct for referencing a shared obj. */
                    break;
                  default:
                    ierror("linker_resolve(): Unsupported reloc %s referencing "
                           "shared object symbol %s",
                           reloc_name[xref->rtype],xref->xrefname);
                    break;
                }
                xref->relocsect.symbol = xdef;  /* xref was resolved */
              }
              break;

            case ID_LIBARCH:
              if (!(pull_unit->flags & OUF_LINKED)) {
                if (gv->map_file) {
                  libpullmsg(gv,pull_unit,xref,cmask);
                  /* "/usr/lib/libc.a (atexit.o) needed due to _atexit" */
                }
                insertbehind(&obj->n,remnode(&pull_unit->n));
                add_priptrs(gv,pull_unit);

                pull_objunit(gv,pull_unit);
              }
              /* fall through */

            default:
              xref->relocsect.symbol = xdef;  /* xref was resolved */
              break;
          }

          /* Handle explicit GOT and PLT references */
          if (!gv->dest_object) {
            switch (xref->rtype) {
              case R_GOT:
              case R_GOTPC:
              case R_GOTOFF:
                xref->relocsect.symbol = dyn_ext_entry(gv,xdef,xref,GOT_ENTRY);
                break;
              case R_PLT:
              case R_PLTPC:
              case R_PLTOFF:
                xref->relocsect.symbol = dyn_ext_entry(gv,xdef,xref,PLT_ENTRY);
                break;
            }
          }

          /* is it a reference from a shared object to one of our symbols? */
          if (obj->lnkfile->type==ID_SHAREDOBJ
              && pull_unit->lnkfile->type!=ID_SHAREDOBJ)
            dyn_export(gv,xdef);  /* then we have to export it */
        }
        else
          xref->relocsect.symbol = xdef;

        xdef->flags |= SYMF_REFERENCED;
      }
    }

    if (obj->n.next->next == NULL && !last_actions_done) {
      if (fff[gv->dest_format]->init != NULL)
        fff[gv->dest_format]->init(gv,FFINI_RESOLVE);
      make_constructors(gv);  /* Con-/Destructor object always at last */
      last_actions_done = TRUE;
    }
    obj = (struct ObjectUnit *)obj->n.next;
  }
  while (obj->n.next);
}


void linker_relrefs(struct GlobalVars *gv)
/* All relocations and unresolved xrefs with a relative reference
   to other sections are collected. A second task is to detect
   and handle base-relative and GOT/PLT references. */
{
  struct ObjectUnit *obj;
  struct Section *sec;
  struct Symbol *sdabase = NULL;
  struct Symbol *sda2base = NULL;
  struct Symbol *r13init = NULL;

  for (obj=(struct ObjectUnit *)gv->selobjects.first;
       obj->n.next!=NULL; obj=(struct ObjectUnit *)obj->n.next) {

    for (sec=(struct Section *)obj->sections.first;
         sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
      struct RelRef **rr = &sec->relrefs;
      struct Symbol *xdef;
      struct Reloc *xref,*reloc;

      for (*rr=NULL,xref=(struct Reloc *)sec->xrefs.first;
           xref->n.next!=NULL; xref=(struct Reloc *)xref->n.next) {

        /* remember offset of sections's last xref */
        set_last_sec_reloc(gv,sec,xref);

        if (xdef = xref->relocsect.symbol) {
          if (xdef->relsect!=NULL &&
              (xdef->type==SYM_RELOC || xdef->type==SYM_COMMON)) {

            if ((xref->rtype==R_SD || xref->rtype==RPPC_SD21 ||
                 xref->rtype==RPPC_MOSDREL) && xdef->type==SYM_COMMON) {
                /* small data common symbol - assign .scommon section */
                xdef->relsect = scommon_section(gv,xdef->relsect->obj);
            }

            if (xref->rtype==R_PC && xdef->relsect!=sec) {
              /* relative reference to different section */
              addrelref(rr,xdef->relsect);
            }

            else if (xref->rtype==R_SD || xref->rtype==RPPC_MOSDREL) {
              /* other section is accessed base relative from this one */
              xdef->relsect->flags |= SF_SMALLDATA;
              if (!gv->textbaserel && xdef->relsect->type==ST_CODE)
                error(121,getobjname(sec->obj),sec->name,xref->offset);
            }
          }
        }
      }

      for (reloc=(struct Reloc *)sec->relocs.first;
           reloc->n.next!=NULL; reloc=(struct Reloc *)reloc->n.next) {

        /* remember offset of sections's last reloc */
        set_last_sec_reloc(gv,sec,reloc);

        if (reloc->rtype==R_PC && reloc->relocsect.ptr!=sec) {
          /* relative reference to different section */
          addrelref(rr,reloc->relocsect.ptr);
        }

        else if (reloc->rtype==R_GOT || reloc->rtype==R_GOTPC) {
          /* a local relocation to a GOT entry may create that entry */
          dyn_reloc_entry(gv,reloc,GOT_LOCAL);
        }

        else if (reloc->rtype==R_PLT || reloc->rtype==R_PLTPC) {
          /* a local relocation to a PLT entry may create that entry */
          dyn_reloc_entry(gv,reloc,PLT_LOCAL);
        }

        else if (reloc->rtype == R_SD) {
          /* other section is accessed base relative from this one */
          reloc->relocsect.ptr->flags |= SF_SMALLDATA;
          if (!sdabase) {
            /* R_SD relocation implies a reference to _SDA_BASE_ */
            if (sdabase = find_any_symbol(gv,sec,sdabase_name))
              sdabase->flags |= SYMF_REFERENCED;
          }
          if (!gv->textbaserel && reloc->relocsect.ptr->type==ST_CODE)
            error(121,getobjname(sec->obj),sec->name,reloc->offset);
        }

        else if (reloc->rtype == RPPC_SD2) {
          if (!sda2base) {
            /* R_SD2 relocation implies a reference to _SDA2_BASE_ */
            if (sda2base = find_any_symbol(gv,sec,sda2base_name))
              sda2base->flags |= SYMF_REFERENCED;
          }
        }

        else if (reloc->rtype == RPPC_MOSDREL) {
          /* other section is accessed base relative from this one */
          reloc->relocsect.ptr->flags |= SF_SMALLDATA;
          if (!r13init) {
            /* RPPC_MOSDREL relocation implies a reference to __r13_init */
            if (r13init = find_any_symbol(gv,sec,r13init_name))
              r13init->flags |= SYMF_REFERENCED;
          }
          if (!gv->textbaserel && reloc->relocsect.ptr->type==ST_CODE)
            error(121,getobjname(sec->obj),sec->name,reloc->offset);
        }

      }
    }
  }
}


void linker_dynprep(struct GlobalVars *gv)
/* Preparations for dynamic linking */
{
  /* hide unreferenced symbols from a shared library */
  hide_shlib_symbols(gv);

  /* export all global symbols when creating a shared library */
  if (gv->dest_sharedobj || gv->dyn_exp_all) {
    int i;

    for (i=0; i<SYMHTABSIZE; i++) {
      struct Symbol *sym;
      struct Symbol **chain = &gv->symbols[i];

      while (sym = *chain) {
        if (sym->bind>=SYMB_GLOBAL && !(sym->flags & SYMF_SHLIB) &&
            sym->relsect!=NULL && (sym->relsect->obj->flags & OUF_LINKED))
          dyn_export(gv,sym);
        chain = &sym->glob_chain;
      }
    }
  }

  /* let the target create and populate dynamic sections when needed */
  if (gv->dynamic && fff[gv->dest_format]->dyncreate)
    fff[gv->dest_format]->dyncreate(gv);
}


void linker_sectrefs(struct GlobalVars *gv)
/* When creating a final executable:
   recursively find all referenced sections starting from the entry-section */
{
  if (gv->gc_sects != GCS_NONE) {
    if (!gv->dest_object && !gv->dest_sharedobj) {
      Dprintf("Finding referenced sections:\n");
      if (gv->gc_sects == GCS_EMPTY)
        ref_all_sections(gv);
      else if (gv->gc_sects == GCS_ALL)
        ref_section(entry_section(gv));
      else
        ierror("");

      /* mark sections with protected symbols as referenced */
      ref_prot_symbols(gv);
    }
    else
      gv->gc_sects = GCS_NONE;
  }
}


void linker_gcsects(struct GlobalVars *gv)
/* garbage collection when linking without linker script */
{
  unsigned gcs;

  if (!gv->use_ldscript && (gcs = gv->gc_sects) != GCS_NONE) {
    struct ObjectUnit *obj;
    struct Section *sec,*nextsec;

    Dprintf("Removing unreferenced sections:\n");
    for (obj=(struct ObjectUnit *)gv->selobjects.first;
         obj->n.next!=NULL; obj=(struct ObjectUnit *)obj->n.next) {
      sec = (struct Section *)obj->sections.first;
      while (nextsec = (struct Section *)sec->n.next) {
        if (!(sec->flags & SF_REFERENCED)) {
          if (gcs == GCS_ALL) {
            remnode(&sec->n);  /* remove section from the linking process */
            Dprintf("  %s(%s) removed (unreferenced)\n",getobjname(obj),sec->name);
          }
          else if (gcs==GCS_EMPTY && sec->size==0) {
            remnode(&sec->n);  /* remove section from the linking process */
            Dprintf("  %s(%s) removed (empty)\n",getobjname(obj),sec->name);
          }
        }
        sec = nextsec;
      }
    }
  }
}


static bool garbage_collected(struct GlobalVars *gv,struct Patterns *pat,
                              struct Section *sec)
{
  unsigned gcs;

  if ((gcs = gv->gc_sects) != GCS_NONE &&
      !(pat->flags & PFL_KEEP) &&
      !(sec->flags & SF_REFERENCED)) {

    if (gcs == GCS_ALL) {
      remnode(&sec->n);  /* remove section from the linking process */
      Dprintf("  %s(%s) ignored (unreferenced)\n",getobjname(sec->obj),sec->name);
      return TRUE;
    }
    else if (gcs==GCS_EMPTY && sec->size==0) {
      remnode(&sec->n);  /* remove section from the linking process */
      Dprintf("  %s(%s) ignored (empty)\n",getobjname(sec->obj),sec->name);
      return TRUE;
    }
  }
  return FALSE;
}


static struct Section **store_msect(int idx,struct Section *sec)
/* store section pointer at index idx of a dynamic array */
{
  static struct Section **secarr;
  static int maxidx;

  if (secarr == NULL) {
    maxidx = 4;
    secarr = alloc(maxidx*sizeof(struct Section *));
  }
  if (idx >= maxidx) {
    maxidx += maxidx;
    secarr = re_alloc(secarr,maxidx*sizeof(struct Section *));
  }
  secarr[idx] = sec;
  return secarr;
}


static int sec_sorta_by_name(const void *left,const void *right)
{
  return strcmp((*(struct Section **)left)->name,
                (*(struct Section **)right)->name);
}

static int sec_sortd_by_name(const void *left,const void *right)
{
  return -strcmp((*(struct Section **)left)->name,
                 (*(struct Section **)right)->name);
}

static int sec_sorta_by_align(const void *left,const void *right)
{
  unsigned lalign = (*(struct Section **)left)->alignment;
  unsigned ralign = (*(struct Section **)right)->alignment;

  return (lalign<ralign) ? -1 : ((lalign>ralign) ? 1 : 0);
}

static int sec_sortd_by_align(const void *left,const void *right)
{
  unsigned lalign = (*(struct Section **)left)->alignment;
  unsigned ralign = (*(struct Section **)right)->alignment;

  return (lalign>ralign) ? -1 : ((lalign<ralign) ? 1 : 0);
}

static int sec_sorta_by_size(const void *left,const void *right)
{
  unsigned long lsize = (*(struct Section **)left)->size;
  unsigned long rsize = (*(struct Section **)right)->size;

  return (lsize<rsize) ? -1 : ((lsize>rsize) ? 1 : 0);
}

static int sec_sortd_by_size(const void *left,const void *right)
{
  unsigned long lsize = (*(struct Section **)left)->size;
  unsigned long rsize = (*(struct Section **)right)->size;

  return (lsize>rsize) ? -1 : ((lsize<rsize) ? 1 : 0);
}


static void sec_sort_lev1(struct GlobalVars *gv,
                          struct Patterns *pat,struct Section **slist,
                          int numsecs,int (*sec_cmp)(const void *, const void *))
{
  if (numsecs > 1) {
    qsort(slist,numsecs,sizeof(struct Section *),sec_cmp);

    if (pat->ssort[1] != PSORT_NONE) {
      /* potential level 2 sorting for elements which are equal on level 1 */
      int i,j;

      for (i=0; i<numsecs-1; i++) {
        for (j=i+1; j<numsecs && !sec_cmp(&slist[i],&slist[j]); j++);
        j -= i;  /* number of equal elements */
        if (j > 1) {
          /* apply second sort level on this range */
          switch (pat->ssort[1]) {
            case PSORT_NAME:
              qsort(&slist[i],j,sizeof(struct Section *),sec_sorta_by_name);
              break;
            case PSORT_NAME|PSORT_REV:
            case PSORT_REV:
              qsort(&slist[i],j,sizeof(struct Section *),sec_sortd_by_name);
              break;
            case PSORT_ALIGN:
              qsort(&slist[i],j,sizeof(struct Section *),sec_sortd_by_align);
              break;
            case PSORT_ALIGN|PSORT_REV:
              qsort(&slist[i],j,sizeof(struct Section *),sec_sorta_by_align);
              break;
            case PSORT_SIZE:
              qsort(&slist[i],j,sizeof(struct Section *),sec_sortd_by_size);
              break;
            case PSORT_SIZE|PSORT_REV:
              qsort(&slist[i],j,sizeof(struct Section *),sec_sorta_by_size);
              break;
            default:
              ierror("level2-sort: mode %u unhandled",(unsigned)pat->ssort[1]);
              break;
          }
        }
      }
    }
  }
}


static void sort_and_merge_sections(struct GlobalVars *gv,
                                    struct Patterns *pat,
                                    struct LinkedSection *ls,
                                    struct Section **secarr,int numsecs)
/* Note, that it may be possible that not all sections from secarr are merged
   into the output section here. For example, because the output section has
   no sufficient space left. In this case the section remains in the list
   of its original object file. */
{
  if (numsecs) {
    struct Section *sec;
    uint8_t stype;
    int i;

    /* sort section pointers in secarr */
    switch (pat->ssort[0]) {
      case PSORT_NAME:
        sec_sort_lev1(gv,pat,secarr,numsecs,sec_sorta_by_name);
        break;
      case PSORT_NAME|PSORT_REV:
      case PSORT_REV:
        sec_sort_lev1(gv,pat,secarr,numsecs,sec_sortd_by_name);
        break;
      case PSORT_ALIGN:
        sec_sort_lev1(gv,pat,secarr,numsecs,sec_sortd_by_align);
        break;
      case PSORT_ALIGN|PSORT_REV:
        sec_sort_lev1(gv,pat,secarr,numsecs,sec_sorta_by_align);
        break;
      case PSORT_SIZE:
        sec_sort_lev1(gv,pat,secarr,numsecs,sec_sortd_by_size);
        break;
      case PSORT_SIZE|PSORT_REV:
        sec_sort_lev1(gv,pat,secarr,numsecs,sec_sorta_by_size);
        break;
      default:
        ierror("level1-sort: mode %u unhandled",(unsigned)pat->ssort[0]);
      case PSORT_NONE:
        break;
    }

    /* Merge all sections in the given order into the output section.
       But do it by section type: ST_CODE first, then ST_DATA and ST_UDATA
       at last, to keep uninitialized sections together. */
    for (stype=0; stype<=ST_LAST; stype++) {
      for (i=0; i<numsecs; i++) {
        uint8_t f;

        sec = secarr[i];
        if (sec->type==stype && ld_section_has_space(gv,ls,sec)) {
          if ((f = cmpsecflags(gv,ls,sec)) == 0xff) {
            /* no warning, because the linker-script should know */
            f = ls->flags ? ls->flags : sec->flags;
          }
          sec->internal_flags &= ~ILF_BADADDRINVA;
          merge_sec_attrs(ls,sec,f);
          merge_ld_section(gv,pat,ls,sec);
        }
      }
    }
  }
}


void linker_merge(struct GlobalVars *gv)
/* Merge the sections with same name and type, or as defined by a
   linker script. Calculate their virtual address and size. */
{
  struct ObjectUnit *obj;
  struct Section *sec,*nextsec;
  struct LinkedSection *ls;
  struct list seclist;
  uint8_t stype;

  if (gv->trace_file)
    fprintf(gv->trace_file,"Merging selected sections:\n");

  if (fff[gv->dest_format]->init != NULL)
    fff[gv->dest_format]->init(gv,FFINI_MERGE);

  if (gv->use_ldscript) {
    /* Linkage rules are defined by a linker script, which means there */
    /* are predefined LinkedSection structures which can be used. */
    struct LinkedSection *maxls=NULL;
    struct Patterns pat;
    unsigned long maxsize = 0;

    memset(&pat,0,sizeof(struct Patterns));
    init_secdef_parse(gv);
    /* Handle one section definition after the other from the
       linker script's SECTIONS block. The script parser cares
       for commands, symbol-definitions and address-assignments. */

    while (ls = next_secdef(gv)) {
      if (ls->ld_flags & LSF_USED)
        error(81,ls->name);  /* multiple use of section in linker script */

      ls->base = ls->relocmem->current;
      ls->copybase = ls->destmem->current;
      ls->ld_flags |= LSF_USED;

      /* The linker scripts create an empty dummy section and adds */
      /* it as the first section into the LinkedSection's list. */
      /* Its purpose is to keep all linker script symbols. */
      /* We have to make sure that its lnksec and va is valid. */
      if (!listempty(&ls->sections)) {
        sec = (struct Section *)ls->sections.first;
        sec->filldata = gv->filldata;
        sec->lnksec = ls;
        sec->va = ls->relocmem->current;
      }

      /* read next file/section name patterns and merge matching sections */
      while ((sec = next_pattern(gv,&pat)) != NULL) {
        if (sec == VALIDPAT) {
          struct Section **msecs = NULL;
          int mcnt = 0;       /* number of matches for these patterns */

          for (obj=(struct ObjectUnit *)gv->selobjects.first;
               obj->n.next!=NULL; obj=(struct ObjectUnit *)obj->n.next) {

            if (obj->lnkfile->type != ID_SHAREDOBJ &&
                pattern_match(pat.fmatch,obj->lnkfile->filename) &&
                !patternlist_match(pat.fexclude,obj->lnkfile->filename)) {
              /* object's file name matches, walk through its sections */

              sec = (struct Section *)obj->sections.first;
              while (nextsec = (struct Section *)sec->n.next) {
                if (sec->lnksec==NULL &&
                    patternlist_match(pat.smatch,sec->name) &&
                    !patternlist_match(pat.sexclude,sec->name) &&
                    !garbage_collected(gv,&pat,sec) &&
                    !(ls->flags&sec->flags&SF_LINKONCE)) {
                  /* section name matches as well and section must not be
                     ignored, so add to merge list */
                  msecs = store_msect(mcnt++,sec);
                }
                sec = nextsec;
              }
            }
          }
          sort_and_merge_sections(gv,&pat,ls,msecs,mcnt);
          free_patterns(&pat);
        }
        else { /* merge art. section created by a data command */
          merge_ld_section(gv,&pat,ls,sec);
          if (ls->type == ST_UNDEFINED)
            ls->type = ST_DATA;   /* sect. becomes data due to data elements */
          ls->flags |= SF_ALLOC;  /* @@@ data should allocate the section */
        }

        /* keep section size up to date */
        if (ls->relocmem->current - ls->base > ls->size)
          ls->size = ls->relocmem->current - ls->base;
        if (sec = last_initialized(ls))
          ls->filesize = (sec->va + sec->size) - ls->base;
        else
          ls->filesize = 0;  /* whole contents is uninitialized */

        if (ls->size>maxsize || maxsize==0) {  /* finds largest section */
          maxsize = ls->size;
          maxls = ls;
        }
      }
    }

    /* Check if there are any sections left, which were not recognized */
    /* by the linker script rules */
    for (obj=(struct ObjectUnit *)gv->selobjects.first;
         obj->n.next!=NULL; obj=(struct ObjectUnit *)obj->n.next) {
      if (obj->lnkfile->type!=ID_SHAREDOBJ && !listempty(&obj->sections)) {
        for (sec=(struct Section *)obj->sections.first; sec->n.next!=NULL;
             sec=(struct Section *)sec->n.next) {
          int i;

          if (sec->size==0 && maxls!=NULL &&
              (*(sec->name)==0 || is_ld_script(sec->obj))) {
            struct Section *lastsec;

            /* @@@ append section without name and contents to the biggest
               LinkedSection - might be a dummy or linker script section
               with abs symbols */
            lastsec = (struct Section *)maxls->sections.last;
            sec->filldata = gv->filldata;
            sec->lnksec = maxls;
            sec->va = lastsec->va + lastsec->size;
            sec->offset = lastsec->offset + lastsec->size;
            addtail(&maxls->sections,remnode(&sec->n));
          }
          else {
            if (sec->internal_flags & ILF_BADADDRINVA) {
              /* No space to fit section into memory region */
              error(158,getobjname(obj),sec->name,(unsigned long long)sec->va);
            }
            else {
              /* Section was not recognized by target linker script */
              error(64,getobjname(obj),sec->name);
            }

            /* kill unallocated common symbols */
            for (i=0; i<OBJSYMHTABSIZE; i++) {
              struct Symbol *sym;

              for (sym = sec->obj->objsyms[i]; sym; sym=sym->obj_chain) {
                if (sym->relsect==sec && sym->type==SYM_COMMON)
                  sym->type = SYM_ABS; /* to prevent an internal error */
              }
            }
          }
        }
      }
    }
  }


  else {  /* !gv->use_ldscript */
    /* Default linkage rules. Link all code, all data, all bss. */
    lword va = gv->start_addr;
    bool baseincr = (fff[gv->dest_format]->flags&FFF_BASEINCR) != 0;
    struct LinkedSection *ls,*newls;
    struct list seclist;

    if (gv->keep_sect_order) {
      /* Make sure to keep the section order as found on the command line. */
      initlist(&seclist);
      for (obj=(struct ObjectUnit *)gv->selobjects.first;
           obj->n.next!=NULL; obj=(struct ObjectUnit *)obj->n.next) {

        if (obj->lnkfile->type != ID_SHAREDOBJ) {
          sec = (struct Section *)obj->sections.first;
          while (nextsec = (struct Section *)sec->n.next) {
            addtail(&seclist,remnode(&sec->n));
            sec = nextsec;
          }
        }
      }
      /* Phase 1: link sec. which fit together, obeying target linking rules*/
      merge_seclist(gv,&seclist);
    }
    else {
      /* Merge sections, beginning with ST_CODE, then ST_DATA and ST_UDATA. */
      for (stype=0; stype<=ST_LAST; stype++) {

        /* collect all sections of current type */
        initlist(&seclist);
        for (obj=(struct ObjectUnit *)gv->selobjects.first;
             obj->n.next!=NULL; obj=(struct ObjectUnit *)obj->n.next) {

          if (obj->lnkfile->type != ID_SHAREDOBJ) {
            sec = (struct Section *)obj->sections.first;
            while (nextsec = (struct Section *)sec->n.next) {
              if (sec->type == stype)
                addtail(&seclist,remnode(&sec->n));
              sec = nextsec;
            }
          }
        }

        /* Phase 1: link sec. which fit together, obeying target linking rules*/
        merge_seclist(gv,&seclist);
      }
    }

    /* Phase 2: resolve dependencies between created LinkedSections */
    do {
      newls = NULL;
      for (ls=(struct LinkedSection *)gv->lnksec.first;
           ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
        for (sec=(struct Section *)ls->sections.first;
             sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
          if (newls = get_matching_lnksec(gv,sec,ls)) {
            /* another LinkedSection matches too - merge them! */
            break;
          }
        }
        if (newls) {
          /* merge with matching LinkedSection, dump the newer one */
          struct Section *firstbss;
          uint8_t tgtfl;

          if (newls->index > ls->index) {
            /* ls is older, so keep it instead of newls */
            struct LinkedSection *dumls = newls;
            newls = ls;
            ls = dumls;
          }
          firstbss = find_first_bss_sec(newls);
          tgtfl = ls->flags & ~SF_PORTABLE_MASK;

          while (sec = (struct Section *)remhead(&ls->sections)) {
            merge_sec_attrs(newls,sec,tgtfl);
            if (!(sec->flags & SF_UNINITIALIZED) && firstbss!=NULL)
              insertbefore(&sec->n,&firstbss->n);
            else
              addtail(&newls->sections,&sec->n);
            sec->lnksec = newls;
          }
          remnode(&ls->n);
          free(ls);

          /* recalculate the appromixate section size, assuming a base of 0 */
          for (sec=(struct Section *)newls->sections.first,newls->size=0;
               sec->n.next!=NULL; sec=(struct Section *)sec->n.next)
            newls->size += sec->size + align(newls->size,sec->alignment);
          break;
        }
      }
    }
    while (newls);

    /* Phase 3: calculate offsets and sizes for final LinkedSections */
    for (ls=(struct LinkedSection *)gv->lnksec.first;
         ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
      ls->base = ls->copybase = va;
      ls->size = ls->filesize = 0;

      for (sec=(struct Section *)ls->sections.first;
           sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
        unsigned long abytes = align(ls->base+ls->size,sec->alignment);

        sec->lnksec = ls;
        sec->offset = ls->size + abytes;
        sec->va = ls->base + sec->offset;
        ls->size += sec->size + abytes;
        if (baseincr)
          va += sec->size + abytes;
        if (!(sec->flags & SF_UNINITIALIZED))
          ls->filesize += sec->size + abytes;

        /* allocate COMMON symbols, if required */
        if (is_common_sec(gv,sec) &&
            (!gv->dest_object || gv->alloc_common)) {
          unsigned long n = allocate_common(gv,TRUE,sec,ls->base+ls->size);

          ls->size += n;
          if (baseincr)
            va += n;
        }
      }
    }
  }

  trim_sections(gv);  /* remove zero-bytes at end of sections */
}


void linker_delunused(struct GlobalVars *gv)
/* remove empty, unused sections without relocations and symbols */
{
  struct LinkedSection *ls = (struct LinkedSection *)gv->lnksec.first;
  struct LinkedSection *firstls=NULL,*nextls;

  gv->nsecs = 0;

  while (nextls = (struct LinkedSection *)ls->n.next) {
    if (firstls == NULL)
      firstls = ls;

    if (ls->size==0 && listempty(&ls->relocs) &&
        listempty(&ls->symbols) && !(ls->ld_flags & LSF_PRESERVE)) {
      remnode(&ls->n);
      if (ls == firstls)
        firstls = NULL;

      if (ls!=firstls && is_common_ls(gv,ls)) {
        /* @@@ Attention! This is a big HACK!
           For the future it should be desirable to have a separate
           list for common symbols, instead of just putting them into
           the symbol list of the first section... @@@ */
        struct Symbol *sym;

        if (firstls == NULL)
          ierror("No other section before COMMON section?");
        while (sym = (struct Symbol *)remhead(&ls->symbols))
          addtail(&firstls->symbols,&sym->n);
      }
    }
    else
      ls->index = gv->nsecs++;  /* reindex remaining sections */

    ls = nextls;
  }
}


void linker_mapfile(struct GlobalVars *gv)
/* print section mapping, when desired */
{
  if (gv->map_file) {
    struct ObjectUnit *obj;
    struct Section *sec;
    struct LinkedSection *ls;

    /* print file names and the new addresses of their sections */
    fprintf(gv->map_file,"\nFiles:\n");

    for (obj=(struct ObjectUnit *)gv->selobjects.first;
         obj->n.next!=NULL; obj=(struct ObjectUnit *)obj->n.next) {
      struct LinkFile *lfile = obj->lnkfile;
      char sep = ' ';

      if (!(obj->flags & OUF_SCRIPT)) {
        if (lfile->type == ID_LIBARCH)
          fprintf(gv->map_file,"  %s (%s):",lfile->pathname,obj->objname);
        else
          fprintf(gv->map_file,"  %s:",obj->objname);

        if (lfile->type != ID_SHAREDOBJ) {
          for (ls=(struct LinkedSection *)gv->lnksec.first;
               ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
            for (sec=(struct Section *)ls->sections.first;
                 sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
              if (sec->obj == obj) {  /* section came from this object? */
                if (!is_common_sec(gv,sec)) {
                  fprintf(gv->map_file,"%c %s %llx(%lx)",sep,sec->name,
                          (unsigned long long)sec->va,sec->size);
                  sep = ',';
                }
              }
            }
          }
        }

        if (sep == ',')  /* any sections listed? */
          fprintf(gv->map_file," hex\n");
        else
          fprintf(gv->map_file,"  symbols only\n"); /* empty or shared obj. */
      }
    }

    /* print section mappings */
    fprintf(gv->map_file,"\n\nSection mapping (numbers in hex):\n");

    for (ls=(struct LinkedSection *)gv->lnksec.first;
         ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
      if (!(ls->size==0 && listempty(&ls->relocs) &&
            listempty(&ls->symbols) && !(ls->ld_flags & LSF_PRESERVE))) {
        fprintf(gv->map_file,"------------------------------\n"
                "  %08llx %s  (size %lx",  /* @@@ FIXME */
                (unsigned long long)ls->copybase,ls->name,ls->size);
        if (ls->filesize < ls->size)
          fprintf(gv->map_file,", allocated %lx",ls->filesize);
        fprintf(gv->map_file,")\n");
  
        for (sec=(struct Section *)ls->sections.first;
             sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
          if (sec->obj!=NULL && !is_ld_script(sec->obj)) {
            fprintf(gv->map_file,"           %08llx - %08llx %s(%s)\n",
                    (unsigned long long)sec->va,
                    (unsigned long long)sec->va+sec->size,
                    sec->obj->objname,sec->name);
          }
        }
      }
    }
  }
}


void linker_copy(struct GlobalVars *gv)
/* Merge contents of linked sections, fix symbol offsets and
   allocate common symbol data. */
{
  struct LinkedSection *ls,*maxls=NULL;
  struct Section *sec;
  unsigned long maxsize = 0;
  struct Symbol **abs_ptr_array = NULL;
  int i,abs_cnt=0;
  struct Symbol *sym,**p;
  struct ObjectUnit *obj;

  if (gv->trace_file)
    fprintf(gv->trace_file,"\n");
  if (gv->map_file)
    fprintf(gv->map_file,"\n");

  for (ls=(struct LinkedSection *)gv->lnksec.first;
       ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
    unsigned long lastsecend = 0;  /* for filling gaps */

    if (gv->trace_file) {
      if (!listempty(&ls->sections) && ls->size>0)
        fprintf(gv->trace_file,"Copying %s:\n",ls->name);
    }
    if (ls->size>maxsize || maxsize==0) {  /* finds largest section */
      maxsize = ls->size;
      maxls = ls;
    }
    /* allocate memory for section, even for uninitialized ones */
    ls->data = alloczero(tbytes(gv,ls->size));

    for (sec=(struct Section *)ls->sections.first;
         sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
      int i;

      if (ls->data && sec->data) {
        /* copy section contents, fill gaps */
        section_fill(gv,ls->data,lastsecend,sec->filldata,
                     (long)(sec->offset-lastsecend));
        section_copy(gv,ls->data,sec->offset,sec->data,sec->size);
        lastsecend = sec->offset + sec->size;
      }

      if (sec->obj) {
        /* find section symbols and fix their offsets */
        for (i=0; i<OBJSYMHTABSIZE; i++) {
          for (sym=sec->obj->objsyms[i]; sym; sym=sym->obj_chain) {
            if (sym->relsect == sec) {
#if 0
              if (sym->type==SYM_COMMON &&
                  (!gv->dest_object || gv->alloc_common)) {
                /* delete remaining copies of common symbols */
                remove_obj_symbol(sym);
              }
#endif
              if (!((sym->flags & (SYMF_REFERENCED|SYMF_PROVIDED))
                    == SYMF_PROVIDED)) { /* ignore unrefd. provided sym. */
                if (sym->type == SYM_RELOC)
                  sym->value += sec->va;  /* was sec->offset */
                addtail(&ls->symbols,&sym->n);
              }
            }
          }
        }
      }
    }

    if (gv->map_file!=NULL || gv->sym_file!=NULL) {
      /* print section's symbols to map file, sorted by address */
      struct Symbol **sym_ptr_array;
      int cnt,acnt;

      /* count symbols in this section, then sort them by address */
      for (cnt=0,acnt=0,sym=(struct Symbol *)ls->symbols.first;
           sym->n.next!=NULL; sym=(struct Symbol *)sym->n.next) {
        if (sym->type == SYM_ABS)
          acnt++;
        else
          cnt++;
      }

      if (acnt) {
        abs_ptr_array = re_alloc(abs_ptr_array,(abs_cnt+acnt)*sizeof(void *));
        for (sym=(struct Symbol *)ls->symbols.first; sym->n.next!=NULL;
             sym=(struct Symbol *)sym->n.next) {
          if (sym->type == SYM_ABS) {
            abs_ptr_array[abs_cnt++] = sym;
            if (--acnt == 0)
              break;
          }
        }
      }

      if (cnt) {
        sym_ptr_array = alloc(cnt * sizeof(void *));
        for (sym=(struct Symbol *)ls->symbols.first,p=sym_ptr_array;
             sym->n.next!=NULL; sym=(struct Symbol *)sym->n.next) {
          if (sym->type != SYM_ABS)
            *p++ = sym;
        }
        if (cnt > 1)
          qsort(sym_ptr_array,cnt,sizeof(void *),sym_addr_cmp);

        if (gv->map_file) {
          fprintf(gv->map_file,"\nSymbols of %s:\n",ls->name);
          for (p=sym_ptr_array,i=0; i<cnt; p++,i++)
            print_symbol(gv,gv->map_file,*p);
        }
        
        if (gv->sym_file) {
          /* output symbol mapping in given format */
          for (p=sym_ptr_array,i=0; i<cnt; p++,i++) {
            if ((*p)->type==SYM_ABS || (*p)->type==SYM_RELOC) {
              if ((gv->sym_file_flags & SFF_NOLOCAL) &&
                  (*p)->bind==SYMB_LOCAL)
                continue;  /* no locals */

              if (gv->sym_file_flags & SFF_VALFIRST)
                fprintf(gv->sym_file,gv->sym_file_format,
                        (unsigned long long)(*p)->value,(*p)->name);
              else if (gv->sym_file_flags & SFF_VALSECOND)
                fprintf(gv->sym_file,gv->sym_file_format,
                        (*p)->name,(unsigned long long)(*p)->value);
              fputc('\n',gv->sym_file);
            }
          }
        }
        free(sym_ptr_array);
      }
    }
  }

  if (gv->map_file && abs_cnt) {
    fprintf(gv->map_file,"\nAbsolute symbols:\n");
    if (abs_cnt > 1)
      qsort(abs_ptr_array,abs_cnt,sizeof(void *),sym_addr_cmp);
    for (p=abs_ptr_array,i=0; i<abs_cnt; p++,i++)
      print_symbol(gv,gv->map_file,*p);
    free(abs_ptr_array);
  }

  if (gv->map_file)
    fprintf(gv->map_file,"\nLinker symbols:\n");

  if (gv->use_ldscript && maxls!=NULL) {
    /* put remaining absolute linker script symbols into the
       symbol list of the largest defined section: */
    while (sym = (struct Symbol *)remhead(&gv->scriptsymbols)) {
      if (!((sym->flags & (SYMF_REFERENCED|SYMF_PROVIDED))
            == SYMF_PROVIDED)) {
        sym->relsect = (struct Section *)maxls->sections.first;
        addtail(&maxls->symbols,&sym->n);
        if (gv->map_file)
          print_symbol(gv,gv->map_file,sym);
      }
    }
  }

  /* last chance to fix linker symbols */
  fixlnksymbols(gv,maxls);

  /* fix offsets of relocatable debugging symbols */
  for (obj=(struct ObjectUnit *)gv->selobjects.first;
       obj->n.next!=NULL; obj=(struct ObjectUnit *)obj->n.next) {
    fixstabs(obj);
  }
}


void linker_relocate(struct GlobalVars *gv)
/* Fix relocations, resolve x-references and create more relocations, */
/* if required. */
{
  const char *fn = "linker_relocate(): ";
  struct Symbol *sdabase,*sda2base,*gotbase,*pltbase,*r13init;
  struct LinkedSection *ls;
  struct Section *sec;

  /* get symbols needed for reloc calculation */
  sdabase = find_any_symbol(gv,NULL,sdabase_name);
  sda2base = find_any_symbol(gv,NULL,sda2base_name);
  gotbase = find_any_symbol(gv,NULL,gv->got_base_name);
  pltbase = find_any_symbol(gv,NULL,gv->plt_base_name);
  r13init = find_any_symbol(gv,NULL,r13init_name);

  for (ls=(struct LinkedSection *)gv->lnksec.first;
       ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {

    /* dyn.relocs appear in uninitialized sections as well, so...*/
    if (/*!(ls->flags&SF_UNINITIALIZED) &&*/ ls->size>0) {
      if (gv->trace_file)
        fprintf(gv->trace_file,"Relocating %s:\n",ls->name);

      for (sec=(struct Section *)ls->sections.first;
           sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
        struct Reloc *rel,*xref;

        /*--------------------------*/
        /* copy and fix relocations */
        /*--------------------------*/
        while (rel = (struct Reloc *)remhead(&sec->relocs)) {
          bool keep = TRUE;
          lword a = 0;

          rel->offset += sec->offset;
          if (rel->rtype != R_MEMID)
            rel->addend += rel->relocsect.ptr->offset;
          rel->relocsect.lnk = rel->relocsect.ptr->lnksec;

          switch (rel->rtype) {

            case R_PLTPC:
            case R_GOTPC:
              if (gv->dest_object)
                break;
              rel->rtype = R_PC;
              /* fall through */

            case R_PC:          /* Normal, PC-relative reference */
            case R_LOCALPC:
              /* resolve relative relocs from the same section */
              if (rel->relocsect.lnk == ls) {
                a = (rel->relocsect.lnk->base + rel->addend) -
                    (ls->base + rel->offset);
                a = writesection(gv,ls->data,rel->offset,rel,a);
                keep = FALSE;
              }
              break;

            case R_SECOFF:      /* symbol's section-offset */
              if (!gv->dest_object) {
                a = rel->addend;
                a = writesection(gv,ls->data,rel->offset,rel,a);
                keep = FALSE;
              }
              break;

            case R_MEMID:       /* destination's memory id (bank) */
              if (!gv->dest_object) {
                lword id = rel->relocsect.lnk->relocmem ?
                           rel->relocsect.lnk->relocmem->id:MEM_NOID;
                if (id == MEM_NOID) {
                  error(165,getobjname(sec->obj),sec->name,
                        rel->offset-sec->offset,rel->relocsect.lnk->name,
                        rel->relocsect.lnk->name);
                  id = 0;
                }
                a = id + rel->addend;
                a = writesection(gv,ls->data,rel->offset,rel,a);
                keep = FALSE;
              }
              break;

            case R_GOT:         /* GOT offset */
            case R_GOTOFF:
              if (!gv->dest_object) {
                if (gotbase) {
                  a = rel->relocsect.lnk->base + rel->addend - gotbase->value;
                  a = writesection(gv,ls->data,rel->offset,rel,a);
                  keep = FALSE;
                }
                else
                  undef_sym_error(sec,rel,gv->got_base_name);
              }
              break;

            case R_SD:          /* _SDA_BASE_ relative reference */
              if (!gv->dest_object) {
                /* resolve base-relative relocation for executable file */
                if (sdabase) {
                  a = rel->relocsect.lnk->base +  rel->addend - sdabase->value;
                  a = writesection(gv,ls->data,rel->offset,rel,a);
                  keep = FALSE;
                }
                else
                  undef_sym_error(sec,rel,sdabase_name);
              }
              break;
              
            case RPPC_SD2:       /* _SDA2_BASE_ relative reference */
              if (!gv->dest_object) {
                /* resolve base-relative relocation for executable file */
                if (sda2base) {
                  a = rel->relocsect.lnk->base + rel->addend - sda2base->value;
                  a = writesection(gv,ls->data,rel->offset,rel,a);
                  keep = FALSE;
                }
                else
                  undef_sym_error(sec,rel,sda2base_name);
              }
              break;

            case RPPC_SD21:        /* PPC-EABI base rel. reference */
              if (!gv->dest_object) {
                /* resolve base-relative relocation for executable file */
                const char *secname = rel->relocsect.lnk->name;

                *(ls->data+rel->offset+1) &= 0xe0;
                if (!strcmp(secname,sdata_name) ||
                    !strcmp(secname,sbss_name)) {
                  if (sdabase) {
                    a = rel->relocsect.lnk->base + rel->addend - sdabase->value;
                    *(ls->data+rel->offset+1) |= 13;
                    a = writesection(gv,ls->data,rel->offset,rel,a);
                    keep = FALSE;
                  }
                  else
                    undef_sym_error(sec,rel,sdabase_name);
                }
                else if (!strcmp(secname,sdata2_name) ||
                         !strcmp(secname,sbss2_name)) {
                  if (sda2base) {
                    a = rel->relocsect.lnk->base + rel->addend - sda2base->value;
                    *(ls->data+rel->offset+1) |= 2;
                    a = writesection(gv,ls->data,rel->offset,rel,a);
                    keep = FALSE;
                  }
                  else
                    undef_sym_error(sec,rel,sda2base_name);
                }
                else if (!strcmp(secname,".PPC.EMB.sdata0") ||
                         !strcmp(secname,".PPC.EMB.sbss0")) {
                  a = rel->relocsect.lnk->base + rel->addend;
                  a = writesection(gv,ls->data,rel->offset,rel,a);
                  keep = FALSE;
                }
                else {
                  print_function_name(sec,rel->offset);
                  error(117,getobjname(sec->obj),sec->name,
                        rel->offset-sec->offset,reloc_name[rel->rtype],
                        secname,secname);
                }
              }
              break;

            case RPPC_MOSDREL:     /* __r13_init rel. reference */
              if (!gv->dest_object) {
                /* resolve base-relative relocation for executable file */
                if (r13init) {
                  a = rel->relocsect.lnk->base + rel->addend - r13init->value;
                  a = writesection(gv,ls->data,rel->offset,rel,a);
                  keep = FALSE;
                }
                else
                  undef_sym_error(sec,rel,r13init_name);
              }
              break;

            case RPPC_AOSBREL:     /* .data rel. reference */
              if (!gv->dest_object) {
                /* resolve base-relative relocation for executable file */
                struct LinkedSection *datals;

                if (datals = find_lnksec(gv,data_name,0,0,0,0)) {
                  a = rel->relocsect.lnk->base + rel->addend - datals->base;
                  a = writesection(gv,ls->data,rel->offset,rel,a);
                  keep = FALSE;
                }
                else {
                  print_function_name(sec,rel->offset);
                  error(120,getobjname(sec->obj),sec->name,
                        rel->offset-sec->offset,data_name);
                }
              }
              break;
            
            case R_NONE:
            case R_ABS:
              break;

            default:
              ierror("%sReloc type %d (%s) is not yet supported",
                     fn,(int)rel->rtype,reloc_name[rel->rtype]);
              break;
          }

          if (keep) {
            /* keep relocations which cannot be resolved in output file */
/*@@@       writesection(gv,ls->data,rel->offset,rel,rel->addend); */
            addtail(&ls->relocs,&rel->n);
            a = 0;
          }

          if (a) {  /* relocation out of range! */
            print_function_name(sec,rel->offset);
            error(25,getobjname(sec->obj),sec->name,rel->offset-sec->offset,
                  (int)rel->insert->bsiz,reloc_name[rel->rtype],
                  rel->relocsect.lnk->name,sgnchar(rel->addend),
                  abstaddr(rel->addend),optsgnstr(a),abstaddr(a));
          }
        }


        /*------------------------------------*/
        /* resolve, fix and copy x-references */
        /*------------------------------------*/
        while (xref = (struct Reloc *)remhead(&sec->xrefs)) {
          struct LinkedSection *refls = NULL;
          struct Symbol *xdef;
          int err_no = 0;
          lword a = 0;
          bool make_reloc = FALSE;

          xref->offset += sec->offset;
          xdef = xref->relocsect.symbol;

          if (xdef != NULL &&
            /* dynamic relocations must be left alone */
              !(xref->flags & RELF_DYNLINK) &&
            /* common symbols have to be resolved in the final executable
               only, or when option -dc (allocate commons) is given */
              !(xref->relocsect.symbol->type==SYM_COMMON &&
                (gv->dest_object && !gv->alloc_common))) {

            /* Relative/absolute reference to absolute symbol */
            if (xdef->type == SYM_ABS) {
              a = xdef->value + xref->addend;
              err_no = 26;
            }

            else if (xdef->type == SYM_RELOC) {
              if ((refls = xdef->relsect->lnksec) == NULL) {
                /* Cannot resolve reference to <sym-name>, because section
                   <name> was not recognized by the linker script */
                error(112,getobjname(sec->obj),sec->name,
                      xref->offset-sec->offset,xref->xrefname,
                      xdef->relsect->name);
              }
              else {
                lword symoffset;

                if (refls!=ls &&
                    (refls->ld_flags & ls->ld_flags & LSF_NOXREFS)) {
                  /* reference between overlayed sections (NOCROSSREFS) */
                  print_function_name(sec,xref->offset);
                  error(159,getobjname(sec->obj),sec->name,
                        xref->offset-sec->offset,ls->name,refls->name,
                        xdef->name);
                }

                symoffset = xdef->value - refls->base;
                a = symoffset + xref->addend;

                switch (xref->rtype) {

                  case R_PLTPC:
                  case R_GOTPC:
                    /* PC-relative PLT/GOT reference */
                    if (gv->dest_object)
                      break;
                    xref->rtype = R_PC;
                    /* fall through */

                  case R_PC:
                    /* PC relative reference to relocatable symbol */
                    if (refls != ls) {
                      make_reloc = TRUE;
                    }
                    else {
                      a = (xdef->value + xref->addend) -
                          (sec->lnksec->base + xref->offset);
                      err_no = 28;
                    }
                    break;

                  case R_SECOFF:
                    /* reference to symbol's section offset */
                    err_no = 36;
                    if (gv->dest_object)
                      make_reloc = TRUE;
                    break;

                  case R_MEMID:
                    /* reference to symbol's destination memory id (bank) */
                    err_no = 166;
                    if (!gv->dest_object) {
                      lword id = refls->relocmem?refls->relocmem->id:MEM_NOID;

                      if (id == MEM_NOID) {
                        error(165,getobjname(sec->obj),sec->name,
                              xref->offset-sec->offset,refls->name,
                              xdef->name);
                        id = 0;
                      }
                      a = id + xref->addend;
                    }
                    else  /* keep as reference - do not turn into reloc */
                      addtail(&ls->xrefs,&xref->n);
                    break;

                  case R_GOT:
                    /* _GLOBAL_OFFSET_TABLE_ relative reference to an
                       object's pointer slot in .got */
                  case R_GOTOFF:
                    /* symbol's offset to _GLOBAL_OFFSET_TABLE_ */
                    err_no = 36;
                    if (!gv->dest_object) {
                      if (gotbase) {
                        a = xdef->value + xref->addend - gotbase->value;
                      }
                      else
                        undef_sym_error(sec,xref,gv->got_base_name);
                    }
                    else
                      make_reloc = TRUE;
                    break;

                  case R_SD:
                    /* _SDA_BASE_ relative reference to relocatable symbol */
                    err_no = 36;
                    if (!gv->dest_object) {
                      if (sdabase) {
                        a = xdef->value + xref->addend - sdabase->value;
                      }
                      else
                        undef_sym_error(sec,xref,sdabase_name);
                    }
                    else
                      make_reloc = TRUE;
                    break;

                  case RPPC_SD2:
                    /* _SDA2_BASE_ relative reference to relocatable symbol */
                    err_no = 36;
                    if (!gv->dest_object) {
                      if (sda2base) {
                        a = xdef->value + xref->addend - sda2base->value;
                      }
                      else
                        undef_sym_error(sec,xref,sda2base_name);
                    }
                    else
                      make_reloc = TRUE;
                    break;

                  case RPPC_SD21:
                    /* PPC-EABI: base relative reference via base-reg 0,2 or 13 */
                    err_no = 36;
                    if (!gv->dest_object) {
                      const char *secname = refls->name;

                      *(ls->data+xref->offset+1) &= 0xe0;
                      if (!strcmp(secname,sdata_name) ||
                          !strcmp(secname,sbss_name)) {
                        if (sdabase) {
                          a = xdef->value + xref->addend - sdabase->value;
                          *(ls->data+xref->offset+1) |= 13;
                        }
                        else
                          undef_sym_error(sec,xref,sdabase_name);
                      }
                      else if (!strcmp(secname,sdata2_name) ||
                               !strcmp(secname,sbss2_name)) {
                        if (sda2base) {
                          a = xdef->value + xref->addend - sda2base->value;
                          *(ls->data+xref->offset+1) |= 2;
                        }
                        else
                          undef_sym_error(sec,xref,sda2base_name);
                      }
                      else if (!strcmp(secname,".PPC.EMB.sdata0") ||
                               !strcmp(secname,".PPC.EMB.sbss0")) {
                        a = xdef->value + xref->addend;
                      }
                      else {
                        print_function_name(sec,xref->offset);
                        error(117,getobjname(sec->obj),sec->name,
                              xref->offset-sec->offset,reloc_name[xref->rtype],
                              xdef->name,secname);
                      }
                    }
                    else
                      make_reloc = TRUE;
                    break;

                  case RPPC_MOSDREL:
                    err_no = 36;
                    if (!gv->dest_object) {
                      if (r13init) {
                        a = xdef->value + xref->addend - r13init->value;
                      }
                      else
                        undef_sym_error(sec,xref,r13init_name);
                    }
                    else
                      make_reloc = TRUE;
                    break;

                  case RPPC_AOSBREL:
                    err_no = 36;
                    if (!gv->dest_object) {
                      struct LinkedSection *datals;

                      if (datals = find_lnksec(gv,data_name,0,0,0,0)) {
                        a = xdef->value + xref->addend - datals->base;
                      }
                      else {
                        print_function_name(sec,xref->offset);
                        error(120,getobjname(sec->obj),sec->name,
                              rel->offset-sec->offset,data_name);
                      }
                    }
                    else
                      make_reloc = TRUE;
                    break;

                  case R_ABS:
                    /* Absolute reference to relocatable symbol */
                    make_reloc = TRUE;
                    /* fall through */

                  case R_NONE:
                    break;

                  default:
                    ierror("%sXRef reloc type %d (%s) is not yet supported",
                           fn,(int)xref->rtype,reloc_name[xref->rtype]);
                }
              }
            }
            else
              ierror("%sReferenced symbol has type %d",fn,(int)xdef->type);

            if (make_reloc) {
              /* turn into a relocation */
              if (refls == NULL)
                ierror("%sReferenced output section for %s does not exist",
                       fn,xdef->name);
              xref->addend = a;
              xref->xrefname = NULL;
              xref->relocsect.lnk = refls;
              addtail(&ls->relocs,&xref->n);
            }
            else {
              if (a = writesection(gv,ls->data,xref->offset,xref,a)) {
                /* value of referenced symbol is out of range! */
                print_function_name(sec,xref->offset);
                error(err_no,getobjname(sec->obj),sec->name,
                      xref->offset-sec->offset,
                      xdef->name,mtaddr(gv,xdef->value),
                      sgnchar(xref->addend),abstaddr(xref->addend),
                      optsgnstr(a),abstaddr(a),(int)xref->insert->bsiz);
              }
            }
          }

          else /*@@@if (xref->rtype != R_NONE)*/ {
            /* xref remains in output file untouched */
            addtail(&ls->xrefs,&xref->n);
          }
        }
      }      
    }
  }
}


static char **get_srcnames(struct GlobalVars *gv)
{
  struct LinkedSection *ls;
  struct Section *sec;
  char **nametab = NULL;
  int entries;

  for (ls=(struct LinkedSection *)gv->lnksec.first;
       ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
    for (sec=(struct Section *)ls->sections.first;
         sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
      struct SourceLines *sl;
      char *fullname;

      if (sl = sec->srclines) {
        do {
          if (sl->path) {
            fullname = alloc(strlen(sl->path)+strlen(sl->name)+2);
            sprintf(fullname,"%s%c%s",sl->path,sl->path_sep,sl->name);
          }
          else
            fullname = allocstring(sl->name);

          if (nametab) {
            /* first check if name already exists in table */
            int i;
            char *p;

            for (i=0; i<entries; i++) {
              if ((sl->flags & SLF_NOCASE) ?
                  !stricmp(nametab[i],fullname) :
                  !strcmp(nametab[i],fullname))
                break;
            }
            if (i >= entries) {
              /* new table entry required */
              ++entries;
              nametab = re_alloc(nametab,(entries+1)*sizeof(char **));
              nametab[i] = fullname;
              nametab[i+1] = NULL;
            }
            else
              free(fullname);
            sl->nameidx = i;
          }
          else {  /* first entry, create table */
            entries = 1;
            nametab = alloc(2 * sizeof(char **));
            nametab[0] = fullname;
            nametab[1] = NULL;
            sl->nameidx = 0;
          }
        }
        while (sl = sl->next);
      }
    }
  }
  return nametab;
}


static void write_mapfile_lineoffsets(struct GlobalVars *gv,
                                      const char **nametab)
{
  struct LinkedSection *ls;
  struct Section *sec;
  int fileid;

  fprintf(gv->map_file,"\n\nSource file line offsets\n"
                       "------------------------");
  for (ls=(struct LinkedSection *)gv->lnksec.first;
       ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
    /* next output section, print name */
    fprintf(gv->map_file,"\n%s:\n",ls->name);

    for (sec=(struct Section *)ls->sections.first,fileid=-1;
         sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
      struct SourceLines *sl;

      if (sl = sec->srclines) {
        do {
          srclinetype *lptr;
          srcoffstype *optr;
          unsigned n;

          for (n=sl->entries,lptr=sl->lines,optr=sl->offsets; n;
               n--,optr++,lptr++) {
            fprintf(gv->map_file,"  0x%0*llx line %u",gv->bits_per_taddr/4,
                    (unsigned long long)*optr+sec->va,(unsigned)*lptr);
            if ((int)sl->nameidx != fileid) {
              fileid = sl->nameidx;
              fprintf(gv->map_file," \"%s\"\n",nametab[fileid]);
            }
            else
              fprintf(gv->map_file,"\n");
          }
        }
        while (sl = sl->next);
      }
    }
  }
}

static void write_line_offsets(struct GlobalVars *gv,const char **nametab)
{
  struct LinkedSection *ls;
  struct Section *sec;
  FILE *f;
  int i;

  if (f = fopen(gv->lineoffsfile,"w")) {
    /* first print list of source file names */
    for (i=0; nametab[i]!=NULL; i++)
      fprintf(f,"%d:\"%s\"\n",i+1,nametab[i]);

    for (ls=(struct LinkedSection *)gv->lnksec.first;
         ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
      /* next output section, print name */
      fprintf(f,"\"%s\"\n",ls->name);

      for (sec=(struct Section *)ls->sections.first;
           sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
        struct SourceLines *sl;

        if (sl = sec->srclines) {
          do {
            srclinetype *lptr;
            srcoffstype *optr;
            unsigned n;

            for (n=sl->entries,lptr=sl->lines,optr=sl->offsets; n;
                 n--,optr++,lptr++) {
              fprintf(f,"%d:%u:0x%lx\n",(int)sl->nameidx+1,(unsigned)*lptr,
                      (unsigned long)*optr+sec->va);
            }
          }
          while (sl = sl->next);
        }
      }
    }
  }
  else
    error(8,gv->lineoffsfile);  /* cannot open */
}


void linker_write(struct GlobalVars *gv)
{
  FILE *f;

  if (!gv->errflag) {  /* no error? */
    if (gv->bits_per_tbyte!=8 &&
        !(fff[gv->dest_format]->flags&FFF_OUTWORDADDR)) {
      /* bits per byte not supported by output format */
      error(113,gv->dest_name,fff[gv->dest_format]->tname,
            (int)gv->bits_per_tbyte);
    }

    /* source level debugging line-offset tables */
    if (gv->lineoffsfile!=NULL || gv->map_file!=NULL) {
      const char **srcnames;

      if (srcnames = (const char **)get_srcnames(gv)) {
        if (gv->map_file)
          write_mapfile_lineoffsets(gv,srcnames);
        if (gv->lineoffsfile)
          write_line_offsets(gv,srcnames);
      }
    }

    if (gv->trace_file) {
      if (!gv->output_sections)
        fprintf(gv->trace_file,"\nCreating output file %s (%s).\n",
                gv->dest_name,fff[gv->dest_format]->tname);
      else
        fprintf(gv->trace_file,"\nCreating output files for each "
                               "section (%s).\n",
                fff[gv->dest_format]->tname);
    }

    /* create output file */
    if (!gv->output_sections && !(fff[gv->dest_format]->flags&FFF_NOFILE)) {
      if ((f = fopen(gv->dest_name,"wb")) == NULL) {
        error(29,gv->dest_name);  /* Can't create output file */
        return;
      }
    }
    else {
      f = NULL;
      if (gv->output_sections && !(fff[gv->dest_format]->flags&FFF_SECTOUT)) {
        error(29,"with sections");  /* Can't create output file with sect. */
        return;
      }
    }

    /* write output file */
    if (gv->dest_sharedobj)
      fff[gv->dest_format]->writeshared(gv,f);
    else if (gv->dest_object)
      fff[gv->dest_format]->writeobject(gv,f);
    else
      fff[gv->dest_format]->writeexec(gv,f);

    if (f != NULL) {
      fclose(f);
      if (!gv->dest_sharedobj && !gv->dest_object)
        set_exec(gv->dest_name);  /* set executable flag */
    }
  }
}


void linker_cleanup(struct GlobalVars *gv)
{
}

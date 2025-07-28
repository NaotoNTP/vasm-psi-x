/* $VER: vlink targets.c V0.18 (29.11.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2024  Frank Wille
 */


#define TARGETS_C
#include "vlink.h"


struct FFFuncs *fff[] = {
#ifdef ADOS
  &fff_amigahunk,
#endif
#ifdef EHF
  &fff_ehf,
#endif
#ifdef ATARI_TOS
  &fff_ataritos,
#endif
#ifdef XFILE
  &fff_xfile,
#endif
#ifdef OS_9
  &fff_os9_6809,
#endif
#ifdef O65
  &fff_o6502,
  &fff_o65816,
#endif
#ifdef APPLE_OMF
  &fff_appleomf,
#endif
#ifdef ELF32_PPC_BE
  &fff_elf32ppcbe,
#endif
#ifdef ELF32_AMIGA
  &fff_elf32powerup,
  &fff_elf32morphos,
  &fff_elf32amigaos,
#endif
#ifdef ELF32_M68K
  &fff_elf32m68k,
#endif
#ifdef ELF32_386
  &fff_elf32i386,
#endif
#ifdef ELF32_AROS
  &fff_elf32aros,
#endif
#ifdef ELF32_ARM_LE
  &fff_elf32armle,
#endif
#ifdef ELF32_JAG
  &fff_elf32jag,
#endif
#ifdef ELF64_X86
  &fff_elf64x86,
#endif
#ifdef AOUT_NULL
  &fff_aoutnull,
#endif
#ifdef AOUT_SUN010
  &fff_aoutsun010,
#endif
#ifdef AOUT_SUN020
  &fff_aoutsun020,
#endif
#ifdef AOUT_BSDM68K
  &fff_aoutbsd68k,
#endif
#ifdef AOUT_BSDM68K4K
  &fff_aoutbsd68k4k,
#endif
#ifdef AOUT_MINT
  &fff_aoutmint,
#endif
#ifdef AOUT_BSDI386
  &fff_aoutbsdi386,
#endif
#ifdef AOUT_PC386
  &fff_aoutpc386,
#endif
#ifdef AOUT_JAGUAR
  &fff_aoutjaguar,
#endif
#ifdef VOBJ
  &fff_vobj_le,
  &fff_vobj_be,
#endif

/* the raw binary file formats *must* be the last ones */
#ifdef RAWBIN
  &fff_rawbin,
#endif
#ifdef AMSDOS
  &fff_amsdos,
#endif
#ifdef APPLEBIN
  &fff_applebin,
#endif
#ifdef ATARICOM
  &fff_ataricom,
#endif
#ifdef BBC
  &fff_bbc,
  &fff_bbc2,
#endif
#ifdef CBMPRG
  &fff_cbmprg,
  &fff_cbmreu,
#endif
#ifdef COCOML
  &fff_cocoml,
#endif
#ifdef DRAGONBIN
  &fff_dragonbin,
#endif
#ifdef FOENIX
  &fff_pgxc02,
  &fff_pgx816,
  &fff_pgz24,
  &fff_pgz32,
#endif
#ifdef ORICMC
  &fff_oricmc,
#endif
#ifdef JAGSRV
  &fff_jagsrv,
#endif
#ifdef SINCQL
  &fff_sincql,
#endif
#ifdef SREC19
  &fff_srec19,
#endif
#ifdef SREC28
  &fff_srec28,
#endif
#ifdef SREC37
  &fff_srec37,
#endif
#ifdef IHEX
  &fff_ihex,
#endif
#ifdef SHEX1
  &fff_shex1,
#endif
#ifdef RAWSEG
  &fff_rawseg,
#endif
  NULL
};

const char *sym_type[] = { "undef","abs","reloc","common","indirect" };
const char *sym_info[] = { ""," object"," function"," section"," file" };
const char *sym_bind[] = { "","local ","global ","weak " };

const char *reloc_name[] = {
  "R_NONE",
  "R_ABS",
  "R_PC",
  "R_GOT",
  "R_GOTPC",
  "R_GOTOFF",
  "R_GLOBDAT",
  "R_PLT",
  "R_PLTPC",
  "R_PLTOFF",
  "R_SD",
  "R_UABS",
  "R_LOCALPC",
  "R_LOADREL",
  "R_COPY",
  "R_JMPSLOT",
  "R_SECOFF",
  "","","","","","","","","","","","","","",""
  "R_SD2",
  "R_SD21",
  "R_SECOFF",
  "R_MOSDREL",
  "R_AOSBREL",
  NULL
};


/* common section names */
const char text_name[] = ".text";
const char data_name[] = ".data";
const char bss_name[] = ".bss";
const char sdata_name[] = ".sdata";
const char sbss_name[] = ".sbss";
const char sdata2_name[] = ".sdata2";
const char sbss2_name[] = ".sbss2";
const char ctors_name[] = ".ctors";
const char dtors_name[] = ".dtors";
const char got_name[] = ".got";
const char plt_name[] = ".plt";
const char zero_name[] = ".zero";

const char sdabase_name[] = "_SDA_BASE_";
const char sda2base_name[] = "_SDA2_BASE_";
const char gotbase_name[] = "__GLOBAL_OFFSET_TABLE_";
const char pltbase_name[] = "__PROCEDURE_LINKAGE_TABLE_";
const char dynamic_name[] = "__DYNAMIC";
const char r13init_name[] = "__r13_init";

const char noname[] = "";


/* current list of section-renamings */
struct SecRename *secrenames;


void section_fill(struct GlobalVars *gv,uint8_t *base,
                  size_t offset,uint16_t fill,lword n)
{
  if (n > 0) {
    uint8_t f[2];
    uint8_t *p = base + tbytes(gv,offset);
    int i = ((uintptr_t)p) & 1;

    write16(1,f,fill);  /* pattern in big-endian */
    for (n=tbytes(gv,n); n; n--,i^=1)
      *p++ = f[i];
  }
}


void section_copy(struct GlobalVars *gv,uint8_t *dest,size_t offset,
                  uint8_t *src,size_t sz)
{
  memcpy(dest+tbytes(gv,offset),src,tbytes(gv,sz));
}


struct Symbol *findsymbol(struct GlobalVars *gv,struct Section *sec,
                          const char *name,uint32_t mask)
/* Return pointer to Symbol, otherwise return NULL.
   Make sure to prefer symbols from sec's ObjectUnit. */
{
  if (fff[gv->dest_format]->fndsymbol) {
    return fff[gv->dest_format]->fndsymbol(gv,sec,name,mask);
  }
  else {
    struct Symbol *sym,*found;
    uint32_t minmask = ~0;

    for (sym=gv->symbols[elf_hash(name)%SYMHTABSIZE],found=NULL; sym!=NULL;
         sym=sym->glob_chain) {
      if (!strcmp(name,sym->name)) {
        if (mask) {
          /* find a symbol with the best-matching (minimal) feature-mask */
          uint32_t fmask;

          if (fmask = sym->fmask) {
            if ((mask & fmask) != mask)
              continue;
            if (fmask <= minmask)
              minmask = 0/*fmask*/;
            else
              continue;
          }
          else if (minmask != ~0)
            continue;
        }
        else if (sym->fmask)
          continue;

        if (sec && sym->relsect && sym->relsect->obj==sec->obj) {
          /* a symbol from the referers object unit is always the best match */
          found = sym;
          break;
        }

        if (found) {
          /* prefer symbols from already linked object units */
          if (sym->relsect && (sym->relsect->obj->flags & OUF_LINKED) &&
              (found->relsect && !(found->relsect->obj->flags & OUF_LINKED)))
            found =sym;
        }
        else
          found = sym;
      }
    }
    if (found!=NULL && found->type==SYM_INDIR)
      return findsymbol(gv,sec,found->indir_name,mask);
    return found;
  }
  return NULL;
}


bool check_protection(struct GlobalVars *gv,const char *name)
/* checks if symbol name is protected against stripping */
{
  struct SymNames *sn = gv->prot_syms;

  while (sn) {
    if (!strcmp(sn->name,name))
      return TRUE;
    sn = sn->next;
  }
  return FALSE;
}


static void unlink_objsymbol(struct Symbol *delsym)
/* unlink a symbol from an object unit's hash chain */
{
  const char *fn = "unlink_objsymbol():";
  struct ObjectUnit *ou = delsym->relsect ? delsym->relsect->obj : NULL;

  if (ou) {
    struct Symbol **chain = &ou->objsyms[elf_hash(delsym->name)%OBJSYMHTABSIZE];
    struct Symbol *sym;

    while (sym = *chain) {
      if (sym == delsym)
        break;
      chain = &sym->obj_chain;
    }
    if (sym) {
      /* unlink the symbol node from the chain */
      *chain = delsym->obj_chain;
      delsym->obj_chain = NULL;
    }
    else
      ierror("%s %s could not be found in any object",fn,delsym->name);
  }
  else
    ierror("%s %s has no object or section",fn,delsym->name);
}


static void remove_obj_symbol(struct Symbol *delsym)
/* delete a symbol from an object unit */
{
  unlink_objsymbol(delsym);
  free(delsym);
}


bool addglobsym(struct GlobalVars *gv,struct Symbol *newsym)
/* insert symbol into global symbol hash table */
{
  struct Symbol **chain = &gv->symbols[elf_hash(newsym->name)%SYMHTABSIZE];
  struct Symbol *sym;
  struct ObjectUnit *newou = newsym->relsect ? newsym->relsect->obj : NULL;

  while (sym = *chain) {
    if (!strcmp(newsym->name,sym->name)) {

      if (newsym->type==SYM_ABS && sym->type==SYM_ABS &&
          newsym->value == sym->value)
        return FALSE;  /* absolute symbols with same value are ignored */

      if (newsym->relsect == NULL || sym->relsect == NULL) {
        /* redefined linker script symbol */
        if (newou!=NULL && newou->lnkfile->type<ID_LIBBASE) {
          static const char *objname = "ldscript";

          error(19,newou ? newou->lnkfile->pathname : objname,
                newsym->name, newou ? getobjname(newou) : objname,
                sym->relsect ? getobjname(sym->relsect->obj) : objname);
        }
        /* redefinitions in libraries are silently ignored */
        return FALSE;
      }

      if (sym->bind == SYMB_GLOBAL) {
        /* symbol already defined with global binding */

        if (newsym->bind == SYMB_GLOBAL) {
          if (newou->lnkfile->type < ID_LIBBASE) {
            if (newsym->type==SYM_COMMON || sym->type==SYM_COMMON) {
              if (newsym->type!=SYM_COMMON || (newsym->type==sym->type &&
                  ((newsym->size>sym->size && newsym->value>=sym->value) ||
                   (newsym->size>=sym->size && newsym->value>sym->value)))) {
                /* replace by common symbol with bigger size or alignment */
                newsym->glob_chain = sym->glob_chain;
                remove_obj_symbol(sym);  /* delete old symbol in object unit */
                break;
              }
            }
            else {
              /* Global symbol "x" is already defined in... */
              error(19,newou->lnkfile->pathname,newsym->name,
                    getobjname(newou),getobjname(sym->relsect->obj));
            }
            return FALSE;  /* ignore this symbol */
          }
          /* else: add global library symbol with same name to the chain -
             some targets may want to choose between them! */
        }
        else
          return FALSE;  /* don't replace global by nonglobal symbols */
      }

      else {
        if (newsym->bind == SYMB_WEAK)
          return FALSE;  /* don't replace weak by weak */

        /* replace weak symbol by a global one */
        newsym->glob_chain = sym->glob_chain;
        remove_obj_symbol(sym);  /* delete old symbol in object unit */
        break;
      }
    }

    chain = &sym->glob_chain;
  }

  *chain = newsym;
  if (newou) {
    if (trace_sym_access(gv,newsym->name))
      fprintf(stderr,"Symbol %s defined in section %s in %s\n",
              newsym->name,newsym->relsect->name,getobjname(newou));
  }
  return TRUE;
}


#if 0 /* not used */
void unlink_globsymbol(struct GlobalVars *gv,struct Symbol *sym)
/* remove a symbol from the global symbol list */
{
  static const char *fn = "unlink_globsymbol(): ";

  if (gv->symbols) {
    struct Symbol *cptr;
    struct Symbol **chain = &gv->symbols[elf_hash(sym->name)%SYMHTABSIZE];

    while (cptr = *chain) {
      if (cptr == sym)
        break;
      chain = &cptr->glob_chain;
    }
    if (cptr) {
      /* delete the symbol node from the chain */
      *chain = sym->glob_chain;
      sym->glob_chain = NULL;
    }
    else
      ierror("%s%s could not be found in global symbols list",fn,sym->name);
  }
  else
    ierror("%ssymbols==NULL",fn);
}
#endif


void hide_shlib_symbols(struct GlobalVars *gv)
/* scan for all unreferenced SYMF_SHLIB symbols in the global symbol list
   and remove them - they have to be invisible for the file we create */
{
  int i;

  for (i=0; i<SYMHTABSIZE; i++) {
    struct Symbol *sym;
    struct Symbol **chain = &gv->symbols[i];

    while (sym = *chain) {
      if ((sym->flags & SYMF_SHLIB) && !(sym->flags & SYMF_REFERENCED)) {
        /* remove from global symbol list */
        *chain = sym->glob_chain;
        sym->glob_chain = NULL;
      }
      else
        chain = &sym->glob_chain;
    }        
  }
}


static void add_objsymbol(struct ObjectUnit *ou,struct Symbol *newsym)
/* Add a symbol to an object unit's symbol table. The symbol name
   must be unique within the object, otherwise an internal error
   will occur! */
{
  struct Symbol *sym;
  struct Symbol **chain = &ou->objsyms[elf_hash(newsym->name)%OBJSYMHTABSIZE];

  while (sym = *chain) {
    if (!strcmp(newsym->name,sym->name))
      ierror("add_objsymbol(): %s defined twice",newsym->name);
    chain = &sym->obj_chain;
  }
  *chain = newsym;
}


struct Symbol *newsymbol(const char *name,lword val,uint8_t type,
                         uint8_t flags,uint8_t info,uint8_t bind,
                         uint32_t size)
{
  struct Symbol *sym = alloczero(sizeof(struct Symbol));

  sym->name = name;
  sym->value = val;
  sym->type = type;
  sym->flags = flags;
  sym->info = info;
  sym->bind = bind;
  sym->size = size;
  return sym;
}


struct Symbol *addsymbol(struct GlobalVars *gv,struct Section *s,
                         const char *name,const char *iname,lword val,
                         uint8_t type,uint8_t flags,uint8_t info,uint8_t bind,
                         uint32_t size,bool chkdef)
/* Define a new symbol. If defined twice in the same object unit, then */
/* return a pointer to its first definition. Defining the symbol twice */
/* globally is only allowed in different object units of a library. */
{
  struct ObjectUnit *ou = s->obj;
  uint32_t fmask = 0;
  struct Symbol *sym,**chain;

  if (gv->masked_symbols && bind==SYMB_GLOBAL) {
    /* Symbol name ends with a decimal number used as feature mask.
       gv->masked_symbols defines the separation character. */
    char *p;
    size_t len;

    if (p = strrchr(name,gv->masked_symbols)) {
      if (isdigit((unsigned char)*(++p))) {
        fmask = atoi(p);
        len = p - name;
        p = alloczero(len--);
        strncpy(p,name,len);
        name = (const char *)p;
      }
    }
  }

  /* check if symbol is already defined in this object */
  chain = &ou->objsyms[elf_hash(name)%OBJSYMHTABSIZE];
  while (sym = *chain) {
    if (!strcmp(name,sym->name)) {
      if (chkdef)  /* do we have to warn about multiple def. ourselves? */
        error(56,ou->lnkfile->pathname,name,getobjname(ou));
      return sym;  /* return first definition to caller */
    }
    chain = &sym->obj_chain;
  }

  /* new symbol */
  sym = newsymbol(name,val,type,flags,info,bind,size);
  sym->relsect = s;
  sym->indir_name = iname;
  sym->fmask = fmask;

  if (type == SYM_COMMON) {
    /* alignment of .common section must suit the biggest common-alignment */
    uint8_t com_alignment = lshiftcnt(val);

    if (com_alignment > s->alignment)
      s->alignment = com_alignment;
  }

  if (check_protection(gv,name))
    sym->flags |= SYMF_PROTECTED;

  if (bind==SYMB_GLOBAL || bind==SYMB_WEAK) {
    uint16_t flags = ou->lnkfile->flags;

    if (flags & IFF_DELUNDERSCORE) {
      if (*name == '_')
        sym->name = name + 1;  /* delete preceding underscore, if present */
    }
    else if (flags & IFF_ADDUNDERSCORE) {
      char *new_name = alloc(strlen(name) + 2);
      
      *new_name = '_';
      strcpy(new_name+1,name);
      sym->name = new_name;
    }

    if (!addglobsym(gv,sym)) {
      free(sym);
      return NULL;
    }
  }

  *chain = sym;
  return NULL;  /* ok, symbol exists only once in this object */
}


struct Symbol *findlocsymbol(struct GlobalVars *gv,struct ObjectUnit *ou,
                             const char *name)
/* find a symbol which is local to the provided ObjectUnit */
{
  struct Symbol *sym;
  struct Symbol **chain = &ou->objsyms[elf_hash(name)%OBJSYMHTABSIZE];

  while (sym = *chain) {
    if (!strcmp(sym->name,name))
      return sym;
    chain = &sym->obj_chain;
  }
  return NULL;
}


void addlocsymbol(struct GlobalVars *gv,struct Section *s,char *name,
                  char *iname,lword val,uint8_t type,uint8_t flags,
                  uint8_t info,uint32_t size)
/* Define a new local symbol. Local symbols are allowed to be */
/* multiply defined. */
{
  struct Symbol *sym;
  struct Symbol **chain = &s->obj->objsyms[elf_hash(name)%OBJSYMHTABSIZE];

  while (sym = *chain)
    chain = &sym->obj_chain;
  *chain = sym = newsymbol(name,val,type,flags,info,SYMB_LOCAL,size);
  sym->relsect = s;
  sym->indir_name = iname;
  if (check_protection(gv,name))
    sym->flags |= SYMF_PROTECTED;
}


struct Symbol *addlnksymbol(struct GlobalVars *gv,const char *name,lword val,
                            uint8_t type,uint8_t flags,uint8_t info,
                            uint8_t bind,uint32_t size)
/* Define a new, target-specific, linker symbol. */
{
  struct Symbol *sym;
  struct Symbol **chain;

  if (gv->lnksyms == NULL)
    gv->lnksyms = alloc_hashtable(LNKSYMHTABSIZE);
  chain = &gv->lnksyms[elf_hash(name)%LNKSYMHTABSIZE];

  while (sym = *chain)
    chain = &sym->obj_chain;
  *chain = newsymbol(name,val,type,flags,info,bind,size);
  return *chain;
}


struct Symbol *findlnksymbol(struct GlobalVars *gv,const char *name)
/* return pointer to Symbol, if present */
{
  struct Symbol *sym;

  if (gv->lnksyms) {
    sym = gv->lnksyms[elf_hash(name)%LNKSYMHTABSIZE];
    while (sym) {
      if (!strcmp(name,sym->name))
        return sym;  /* symbol found! */
      sym = sym->obj_chain;
    }
  }
  return NULL;
}


static void unlink_lnksymbol(struct GlobalVars *gv,struct Symbol *sym)
/* remove a linker-symbol from its list */
{
  static const char *fn = "unlink_lnksymbol(): ";

  if (gv->lnksyms) {
    struct Symbol *cptr;
    struct Symbol **chain = &gv->lnksyms[elf_hash(sym->name)%LNKSYMHTABSIZE];

    while (cptr = *chain) {
      if (cptr == sym)
        break;
      chain = &cptr->obj_chain;
    }
    if (cptr) {
      /* delete the symbol node from the chain */
      *chain = sym->obj_chain;
      sym->obj_chain = NULL;
    }
    else
      ierror("%s%s could not be found in linker-symbols list",fn,sym->name);
  }
  else
    ierror("%slnksyms==NULL",fn);
}


void fixlnksymbols(struct GlobalVars *gv,struct LinkedSection *def_ls)
{
  struct Symbol *sym,*next;
  int i;

  if (gv->lnksyms) {
    for (i=0; i<LNKSYMHTABSIZE; i++) {
      for (sym=gv->lnksyms[i]; sym; sym=next) {
        next = sym->obj_chain;

        if (sym->flags & SYMF_LNKSYM) {
          if (fff[gv->dest_format]->setlnksym) {
            /* do target-specific symbol modificatios (relsect, value, etc.) */
            fff[gv->dest_format]->setlnksym(gv,sym);

            if (sym->relsect==NULL && def_ls!=NULL) {
              /* @@@ attach absolute symbols to the provided default section */
              if (!listempty(&def_ls->sections))
                sym->relsect = (struct Section *)def_ls->sections.first;
            }
            if (sym->type == SYM_RELOC)
              sym->value += sym->relsect->va;
#if 0 /* @@@ not needed? */
            add_objsymbol(sym->relsect->obj,sym);
#endif
            if (sym->bind >= SYMB_GLOBAL)
              addglobsym(gv,sym);  /* make it globally visible */
            /* add to final output section */
            addtail(&sym->relsect->lnksec->symbols,&sym->n);
            if (gv->map_file)
              print_symbol(gv,gv->map_file,sym);
          }
        }
      }
      gv->lnksyms[i] = NULL;  /* clear chain, it's unusable now! */
    }
  }
}


struct Symbol *find_any_symbol(struct GlobalVars *gv,struct Section *sec,
                               const char *name)
/* return pointer to a global symbol or linker symbol */
{
  struct Symbol *sym = findsymbol(gv,sec,name,0);

  if (sym == NULL)
    sym = findlnksymbol(gv,name);

  return sym;
}


static struct SymbolMask *makesymbolmask(struct GlobalVars *gv,
                                         const char *name,uint32_t mask)
{
  struct SymbolMask **chain = &gv->symmasks[elf_hash(name)%SMASKHTABSIZE];
  struct SymbolMask *sm;

  while (sm = *chain) {
    if (!strcmp(name,sm->name)) {
      /* mask for this symbol already exists, so add it */
      if (sm->common_mask) {
        if (mask)
          sm->common_mask |= mask;
        else
          sm->common_mask = 0;  /* one reference without mask disables it */
      }
      return sm;
    }
    chain = &sm->next;
  }

  /* make a new symbol mask */
  *chain = sm = alloc(sizeof(struct SymbolMask));
  sm->next = NULL;
  sm->name = name;
  sm->common_mask = mask;
  return sm;
}


static bool check_global_objsym(struct ObjectUnit *ou,struct Symbol **chain,
                                struct Symbol *gsym,struct Symbol *sym)
{
  if (gsym!=NULL && gsym!=sym &&
      gsym->relsect!=NULL && (gsym->relsect->obj->flags & OUF_LINKED)) {

    if (sym->type==SYM_COMMON || gsym->type==SYM_COMMON) {
      if (sym->type!=SYM_COMMON || (sym->type==gsym->type &&
          ((sym->size>gsym->size && sym->value>=gsym->value) ||
          (sym->size>=gsym->size && sym->value>gsym->value)))) {
        /* replace by common symbol with bigger size or alignment */
        sym->glob_chain = gsym->glob_chain;
        remove_obj_symbol(gsym);  /* delete old symbol in object unit */
        *chain = sym;
      }
      else
        return TRUE;  /* ignore the object's symbol */
    }
    else {
      if ((ou->lnkfile->type != ID_SHAREDOBJ) &&
          (gsym->relsect->obj->lnkfile->type != ID_SHAREDOBJ) &&
          ((gsym->relsect->obj->flags & OUF_EHFPPC)
           == (sym->relsect->obj->flags & OUF_EHFPPC))) {
        /* Global symbol "x" is already defined in... */
        error(19,ou->lnkfile->pathname,sym->name,getobjname(ou),
              getobjname(gsym->relsect->obj));
      }
      /* An identical symbol in an EHFPPC and a non-EHFPPC object unit
         of a library is tolerated - "amigaehf" will pick the right one.
         Also redefinitions from shared objects are simply ignored. */
    }
  }
  return FALSE;
}


void pull_objunit(struct GlobalVars *gv,struct ObjectUnit *ou)
/* called when a new object unit is pulled into the linking process */
{
  int i;

  ou->flags |= OUF_LINKED;

  /* Check all global symbols of an object unit against the global symbol
     table for redefinitions or common symbols.
     This is required when a new unit has been pulled into the linking
     process to resolve an undefined reference. */
  for (i=0; i<OBJSYMHTABSIZE; i++) {
    struct Symbol **chain = &ou->objsyms[i];
    struct Symbol *sym;

    while (sym = *chain) {
      if (sym->bind==SYMB_GLOBAL) {
        struct Symbol **gchain = &gv->symbols[elf_hash(sym->name)%SYMHTABSIZE];
        struct Symbol *gsym;

        while (gsym = *gchain) {
          if (!strcmp(sym->name,gsym->name)) {
            if (check_global_objsym(ou,gchain,gsym,sym)) {
              *chain = sym->obj_chain;  /* discard this object symbol */
              break;
            }
          }
          gchain = &(*gchain)->glob_chain;
        }
      }
      chain = &sym->obj_chain;
    }
  }

  if (gv->masked_symbols) {
    /* add to the common feature mask from new symbol references */
    struct Section *sec;
    struct Reloc *r;

    for (sec=(struct Section *)ou->sections.first;
         sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
      for (r=(struct Reloc *)sec->xrefs.first;
           r->n.next!=NULL; r=(struct Reloc *)r->n.next) {
        if (r->flags & RELF_SMASK) {
          r->flags &= ~RELF_SMASK;
          r->relocsect.cmask = makesymbolmask(gv,r->xrefname,
                                              r->relocsect.smask);
          r->flags |= RELF_CMASK;
        }
        else
          (void)makesymbolmask(gv,r->xrefname,0);
      }
    }
  }
}


struct Section *getinpsecoffs(struct LinkedSection *ls,
                              unsigned long ooff,unsigned long *ioff)
/* determine the offset on an input-section for the given output section
   offset */
{
  struct Section *sec;

  for (sec=(struct Section *)ls->sections.first;
       sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
    if (ooff>=sec->offset && ooff<sec->offset+sec->size) {
      *ioff = ooff-sec->offset;
      return sec;
    }
  }
  ierror("getinpsecoffs: (%s+0x%lx) does not belong to any input section!",
         ls->name,ooff);
  return NULL;
}


struct RelocInsert *initRelocInsert(struct RelocInsert *ri,uint16_t pos,
                                    uint16_t siz,lword msk)
{
  if (ri == NULL)
    ri = alloc(sizeof(struct RelocInsert));
  ri->next = NULL;
  ri->bpos = pos;
  ri->bsiz = siz;
  ri->mask = msk;
  return ri;
}


struct Reloc *newreloc(struct GlobalVars *gv,struct Section *sec,
                       const char *xrefname,struct Section *rs,uint32_t id,
                       unsigned long offset,int rtype,lword addend)
/* allocate and init new relocation structure */
{
  struct Reloc *r = alloczero(sizeof(struct Reloc));

  if (xrefname) {
    /* external symbol reference */

    if (sec->obj) {
      uint16_t flags = sec->obj->lnkfile->flags;

      if (gv->masked_symbols) {
        /* Referenced symbol may end with a decimal number used as a
           mask for feature-requirements.
           gv->masked_symbols defines the separation character. */
        uint32_t fmask=0;
        size_t len;
        uint8_t t;
        char *p;

        if (p = strrchr(xrefname,gv->masked_symbols)) {
          if (isdigit((unsigned char)*(++p))) {
            fmask = atoi(p);
            len = p - xrefname;
            p = alloczero(len--);
            strncpy(p,xrefname,len);
            xrefname = (const char *)p;
          }
        }

        t = sec->obj->lnkfile->type;
        if (t==ID_LIBARCH && gv->whole_archive)
          t = ID_OBJECT;
        switch (t) {
          case ID_EXECUTABLE:
          case ID_OBJECT:
            /* Construct a common ORed mask from all symbols with this
               name, which are currently part of the linking process. */
            r->relocsect.cmask = makesymbolmask(gv,xrefname,fmask);
            if (fmask)
              r->flags |= RELF_CMASK;
            break;
          default:
            /* otherwise just remember the feat. mask for later references */
            if (fmask) {
              r->relocsect.smask = fmask;
              r->flags |= RELF_SMASK;
            }
            break;
        }
      }

      if (flags & IFF_DELUNDERSCORE) {
        if (*xrefname == '_')
          r->xrefname = ++xrefname;
      }
      else if (flags & IFF_ADDUNDERSCORE) {
        char *new_name = alloc(strlen(xrefname) + 2);

        *new_name = '_';
        strcpy(new_name+1,xrefname);
        r->xrefname = new_name;
      }
    }
    r->xrefname = xrefname;
  }
  else if (rs)
    r->relocsect.ptr = rs;
  else
    r->relocsect.id = id;

  r->offset = offset;
  r->addend = addend;
  if (rtype & R_S)
    r->flags |= RELF_S;
  if (rtype & R_U)
    r->flags |= RELF_U;
  r->rtype = rtype & ~(R_S|R_U);
  return r;
}


void addreloc(struct Section *sec,struct Reloc *r,
              uint16_t pos,uint16_t siz,lword mask)
/* Add a relocation description of the current type to this relocation,
   which will be inserted into the sections reloc list, if not
   already done. */
{
  struct RelocInsert *ri,*new;

  new = initRelocInsert(NULL,pos,siz,mask);
  if (ri = r->insert) {
    while (ri->next)
      ri = ri->next;
    ri->next = new;
  }
  else
    r->insert = new;

  if (r->n.next==NULL && sec!=NULL) {
    if (r->xrefname)
      addtail(&sec->xrefs,&r->n);  /* add to current section's XRef list */
    else
      addtail(&sec->relocs,&r->n);  /* add to current section's Reloc list */
  }
}


void addreloc_ri(struct Section *sec,struct Reloc *r,struct RelocInsert *ri)
/* Call addreloc with information from the supplied RelocInsert structure.
   May result in multiple addreloc-calls. */
{
  while (ri != NULL) {
    addreloc(sec,r,ri->bpos,ri->bsiz,ri->mask);
    ri = ri->next;
  }
}


bool isstdreloc(struct Reloc *r,uint8_t type,uint16_t size)
/* return true when relocation type matches standard requirements */
{
  struct RelocInsert *ri;

  if (r->rtype==type && (ri = r->insert)!=NULL) {
    if (ri->bpos==0 && ri->bsiz==size && ri->mask==-1 && ri->next==NULL)
      return TRUE;
  }
  return FALSE;
}


struct Reloc *findreloc(struct Section *sec,unsigned long offset)
/* return possible relocation at offset */
{
  if (sec) {
    struct Reloc *reloc;

    for (reloc=(struct Reloc *)sec->relocs.first;
         reloc->n.next!=NULL; reloc=(struct Reloc *)reloc->n.next) {
      if (reloc->offset == offset)
        return reloc;
    }
  }
  return NULL;
}


void addstabs(struct ObjectUnit *ou,struct Section *sec,char *name,
              uint8_t type,int8_t other,int16_t desc,uint32_t value)
/* add an stab entry for debugging */
{
  struct StabDebug *stab = alloc(sizeof(struct StabDebug));

  stab->relsect = sec;
  stab->name.ptr = name;
  if (name) {
    if (*name == '\0')
      stab->name.ptr = NULL;
  }
  stab->n_type = type;
  stab->n_othr = other;
  stab->n_desc = desc;
  stab->n_value = value;
  addtail(&ou->stabs,&stab->n);
}


void fixstabs(struct ObjectUnit *ou)
/* fix offsets of relocatable stab entries */
{
  struct StabDebug *stab;

  for (stab=(struct StabDebug *)ou->stabs.first;
       stab->n.next!=NULL; stab=(struct StabDebug *)stab->n.next) {
    if (stab->relsect) {
      stab->n_value += (uint32_t)stab->relsect->va;
    }
  }
}


struct SourceLines *newsourcelines(struct Section *s,const char *srcname)
/* Append a new SourceLines record for assinging section offsets to
   line numbers of the given source name. */
{
  struct SourceLines *sl;
  const char *name;

  if (sl = s->srclines) {
    while (sl->next)
      sl = sl->next;
    sl->next = alloczero(sizeof(struct SourceLines));
    sl = sl->next;
  }
  else
    sl = s->srclines =  alloczero(sizeof(struct SourceLines));

  /* separate file name from path */
  name = base_name(srcname);
  if (name != srcname) {
    size_t pathlen = name - srcname;
    char *pathbuf = alloc(pathlen);

    memcpy(pathbuf,srcname,--pathlen);  /* without separator */
    pathbuf[pathlen] = '\0';
    sl->path = pathbuf;
    sl->path_sep = srcname[pathlen];  /* remember path-separator here */
  }
  sl->name = name;
  sl->lang = DW_LANG_DEFAULT;
  return sl;
}


void allocsrclinetab(struct SourceLines *sl,size_t entries)
/* allocate source-line/offset arrays */
{
  sl->entries = entries;
  sl->lines = alloc(entries * sizeof(srclinetype));
  sl->offsets = alloc(entries * sizeof(srcoffstype));
}


lword readsection(struct GlobalVars *gv,uint8_t rtype,
                  uint8_t *src,size_t secoffs,struct RelocInsert *ri)
/* Read data from section at 'src' + 'secoffs', using the field-offsets,
   sizes and masks from the supplied list of RelocInsert structures. */
{
  int maxfldsz = 0;
  lword data = 0;

  src += tbytes(gv,secoffs);

  while (ri != NULL) {
    lword mask = ri->mask;
    lword v;
    int n;

    n = mask==-1 ? (int)ri->bsiz : highest_bit_set(mask) + 1;
    if (n > maxfldsz)
      maxfldsz = n;

    /* read from bitfield */
    v = readreloc(gv,src,ri->bpos,ri->bsiz);

    /* mask and denormalize the read value using 'mask' */
    n = lshiftcnt(mask);
    mask >>= n;
    v &= mask;
    v <<= n;

    /* add to data value, next RelocInsert */
    data += v;
    ri = ri->next;
  }

  /* always sign-extend - target has to trim the value when needed */
  return sign_extend(data,maxfldsz);
}


lword writesection(struct GlobalVars *gv,uint8_t *dest,size_t secoffs,
                   struct Reloc *r,lword v)
/* Write 'v' into the bit-field defined by the relocation type in 'r'.
   Do range checks first, depending on the reloc type.
   Returns 0 on success or the masked and normalized value which failed
   on the range check. */
{
  uint8_t t = r->rtype;
  struct RelocInsert *ri;
  int sign;

  if (t == R_NONE)
    return 0;

  if (r->flags & RELF_S)
    sign = 1;
  else if (r->flags & RELF_U)
    sign = 2;
  else
    sign = 0;

  /* Reset all relocation fields to zero. */
  dest += tbytes(gv,secoffs);
  for (ri=r->insert; ri!=NULL; ri=ri->next)
    writereloc(gv,dest,ri->bpos,ri->bsiz,0);

  /* add value to relocation fields */
  for (ri=r->insert; ri!=NULL; ri=ri->next) {
    lword mask = ri->mask;
    lword insval = v & mask;
    lword oldval;
    int bpos = ri->bpos;
    int bsiz = ri->bsiz;

    insval >>= lshiftcnt(mask);  /* normalize according to mask */
    if (mask < 0) {
      if (sign!=2 && bsiz<(int)gv->bits_per_taddr)  /* sign extend addresses */
        insval = sign_extend(insval,gv->bits_per_taddr);
      if (!checkrange(insval,sign,bsiz))
        return insval;  /* range check failed on 'insval' */
    }
    else if (sign == 1)
      insval = sign_extend(insval,bsiz);

    /* add to value already present in this field */
    oldval = readreloc(gv,dest,bpos,bsiz);
    if (sign != 2)
      oldval = sign_extend(oldval,bsiz);
    writereloc(gv,dest,bpos,bsiz,oldval+insval);
  }

  return 0;
}


int writetaddr(struct GlobalVars *gv,void *dst,size_t offs,lword d)
{
  uint8_t *p = dst;

  p += tbytes(gv,offs);
  writereloc(gv,p,0,gv->bits_per_taddr,d);
  return (int)(gv->bits_per_taddr + 7) / 8;
}


void calc_relocs(struct GlobalVars *gv,struct LinkedSection *ls)
/* calculate and insert all relocations of a section */
{
  const char *fn = "calc_reloc(): ";
  struct Reloc *r;

  if (ls == NULL)
    return;

  for (r=(struct Reloc *)ls->relocs.first; r->n.next!=NULL;
       r=(struct Reloc *)r->n.next) {
    lword s,a,p,val;

    if (r->relocsect.lnk == NULL) {
      if (r->flags & RELF_DYNLINK)
        continue;  /* NULL, because it was resolved by a shared object */
      else
        ierror("calc_relocs: Reloc type %d (%s) at %s+0x%lx (addend 0x%llx)"
               " is missing a relocsect.lnk",
               (int)r->rtype,reloc_name[r->rtype],ls->name,r->offset,
               (unsigned long long)r->addend);
    }

    s = r->relocsect.lnk->base;
    a = r->addend;
    p = ls->base + r->offset;
    val = 0;

    switch (r->rtype) {

      case R_NONE:
        continue;

      case R_ABS:
        val = s+a;
        break;

      case R_PC:
        val = s+a-p;
        break;

#if 0 /* @@@ shouldn't occur - already resolved by linker_relocate() ??? */
      case R_SD:
        val = s+a - _SDA_BASE_; /* @@@ */
        break;
#endif

      default:
        ierror("%sReloc type %d (%s) is currently not supported",
               fn,(int)r->rtype,reloc_name[r->rtype]);
        break;
    }

    if (val = writesection(gv,ls->data,r->offset,r,val)) {
      struct RelocInsert *ri;

      /* Calculated value doesn't fit into relocation type x ... */
      if (ri = r->insert) {
        struct Section *isec;
        unsigned long ioffs;

        isec = getinpsecoffs(ls,r->offset,&ioffs);
        /*print_function_name(isec,ioffs); <- sym-values are modified! */
        error(35,gv->dest_name,ls->name,r->offset,getobjname(isec->obj),
              isec->name,ioffs,optsgnstr(val),abstaddr(val),
              reloc_name[r->rtype],(int)ri->bpos,(int)ri->bsiz,
              mtaddr(gv,ri->mask));
      }
      else
        ierror("%sReloc (%s+%lx), type=%s, without RelocInsert",
               fn,ls->name,r->offset,reloc_name[r->rtype]);
    }
  }
}


static int reloc_offset_cmp(const void *left,const void *right)
/* qsort: compare relocation offsets */
{
  unsigned long offsl = (*(struct Reloc **)left)->offset;
  unsigned long offsr = (*(struct Reloc **)right)->offset;

  return (offsl<offsr) ? -1 : ((offsl>offsr) ? 1 : 0);
}


void sort_relocs(struct list *rlist)
/* sorts a section's relocation list by their section offsets */
{
  struct Reloc *rel,**rel_ptr_array,**p;
  int cnt = 0;

  /* count relocs and make a pointer array */
  for (rel=(struct Reloc *)rlist->first; rel->n.next!=NULL;
       rel=(struct Reloc *)rel->n.next)
    cnt++;
  if (cnt > 1) {
    rel_ptr_array = alloc(cnt * sizeof(void *));
    for (rel=(struct Reloc *)rlist->first,p=rel_ptr_array;
         rel->n.next!=NULL; rel=(struct Reloc *)rel->n.next)
      *p++ = rel;

    /* sort pointer array */
    qsort(rel_ptr_array,cnt,sizeof(void *),reloc_offset_cmp);

    /* rebuild reloc list from sorted pointer array, then free it */
    initlist(rlist);
    for (p=rel_ptr_array; cnt; cnt--) {
      rel = *p++;
      addtail(rlist,&rel->n);
    }
    free(rel_ptr_array);
  }
}


void add_priptrs(struct GlobalVars *gv,struct ObjectUnit *ou)
/* Inserts all PriPointer nodes of an object into the global list. */
/* The node's position depends on 1. section name, 2. list name, */
/* 3. priority. */
{
  struct PriPointer *newpp,*nextpp,*pp;

  while (newpp = (struct PriPointer *)remhead(&ou->pripointers)) {
    pp = (struct PriPointer *)gv->pripointers.first;

    while (nextpp = (struct PriPointer *)pp->n.next) {
      int c;

      if ((c = strcmp(newpp->secname,pp->secname)) > 0)
        break;

      if (!c) {
        if ((c = strcmp(newpp->listname,pp->listname)) > 0)
          break;

        if (!c) {
          if (newpp->priority < pp->priority)
            break;

          if (newpp->priority==pp->priority &&
              !strcmp(newpp->xrefname+1,pp->xrefname+1)) {
            /* same priority and identical name, except for first character */
            if (*newpp->xrefname == *pp->xrefname)
              error(151,getobjname(ou),newpp->xrefname);  /* duplicate */
            else if (*newpp->xrefname == '@') {
              /* @(fastcall) PriPointer replaces _(standard) */
              remnode(&pp->n);
              pp = nextpp;
              break;
            }
            newpp = NULL;  /* do not insert, ignore */
            break;
          }
        }
      }

      pp = nextpp;
    }

    if (newpp)
      insertbefore(&newpp->n,&pp->n);
  }
}


static void new_priptr(struct ObjectUnit *ou,const char *sec,const char *label,
                       int pri,const char *xref,lword addend)
/* Inserts a new longword into the object's PriPointers list. */
{
  struct PriPointer *newpp = alloc(sizeof(struct PriPointer));

  newpp->secname = sec;
  newpp->listname = label;
  newpp->priority = pri;
  newpp->xrefname = xref;
  newpp->addend = addend;
  addtail(&ou->pripointers,&newpp->n);
}


static const char *xtors_secname(struct GlobalVars *gv,const char *defname)
{
  const char *name = defname;

  if (gv->collect_ctors_secname) {
    /* required to put constructors/destructors in non-default sections? */
    if (strcmp(gv->collect_ctors_secname,ctors_name) &&
        strcmp(gv->collect_ctors_secname,dtors_name))
      name = gv->collect_ctors_secname;
  }
  else if (gv->collect_ctors_type == CCDT_SASC)
    name = "__MERGED";
  return name;
}


static void add_xtor_sym(struct GlobalVars *gv,int ctor,const char *name)
/* adds a constructor/destructor dummy symbol for linker_resolve() */
{
  if (ctor ? gv->ctor_symbol==NULL : gv->dtor_symbol==NULL) {
    struct Symbol *sym;

    if (sym = addlnksymbol(gv,name,0,SYM_ABS,0,SYMI_OBJECT,SYMB_GLOBAL,0)) {
      if (ctor)
        gv->ctor_symbol = sym;
      else
        gv->dtor_symbol = sym;
    }
    else
      error(59,name);  /* Can't define symbol as ctors/dtors label */
  }
}


static int vbcc_xtors_pri(const char *s)
/* Return priority of a vbcc constructor/destructor function name.
   Its priority may be specified by a number behind the 2nd underscore.
   Example: _INIT_9_OpenLibs (constructor with priority 9) */
{
  if (*s++ == '_')
    if (isdigit((unsigned char)*s))
      return atoi(s);
  return 0;
}


static int sasc_xtors_pri(const char *s)
/* Return priority of a SAS/C constructor/destructor function name.
   Its priority may be specified by a number behind the 2nd underscore.
   For SAS/C a lower value means higher priority!
   Example: _STI_110_OpenLibs (constructor with priority 110) */
{
  if (*s++ == '_')
    if (isdigit((unsigned char)*s))
      return 30000-atoi(s);  /* 30000 is the default priority, i.e. 0 */
  return 0;
}


static void add_xtors(struct GlobalVars *gv,struct list *objlist,
                      int (*xtors_pri)(const char *),int elf,
                      const char *cname,const char *dname,
                      const char *csecname,const char *dsecname,
                      const char *clabel,const char *dlabel)
{
  struct ObjectUnit *obj;
  int clen = strlen(cname);
  int dlen = strlen(dname);
  int i;

  for (obj=(struct ObjectUnit *)objlist->first;
       obj->n.next!=NULL; obj=(struct ObjectUnit *)obj->n.next) {
    for (i=0; i<OBJSYMHTABSIZE; i++) {
      struct Symbol *sym = obj->objsyms[i];
      const char *p;

      while (sym) {
        if (sym->bind==SYMB_GLOBAL) {
          p = sym->name;
          if (!elf && (*p=='_' || *p=='@')) {
            if (*p++ == '@') {  /* m68k fastcall-ABI */
              if (*p == '$')    /* m68k vbcccall-ABI */
                p++;
            }
          }
          if (!strncmp(p,cname,clen))
            new_priptr(obj,csecname,clabel,xtors_pri(p+clen),sym->name,0);
          else if (!strncmp(p,dname,dlen))
            new_priptr(obj,dsecname,dlabel,xtors_pri(p+dlen),sym->name,0);
        }
        sym = sym->obj_chain;
      }
    }

    /* Con-/destructors from selobjects are already known to the linker.
       The rest is added when their objects are pulled in from a library. */
    if (objlist == &gv->selobjects)
      add_priptrs(gv,obj);
  }
}


void collect_constructors(struct GlobalVars *gv)
/* Scan all selected and unselected object modules for constructor-
   and destructor functions of the required type. */
{
  if (!gv->dest_object) {
    const char *csec = xtors_secname(gv,ctors_name);
    const char *dsec = xtors_secname(gv,dtors_name);
    const char *sasc_ctor = "_STI";
    const char *sasc_dtor = "_STD";
    const char *vbcc_ctor = "_INIT";
    const char *vbcc_dtor = "_EXIT";
    const char *ctor_label = "___CTOR_LIST__";
    const char *dtor_label = "___DTOR_LIST__";
    int elf = 0;

    switch (gv->collect_ctors_type) {

      case CCDT_NONE:
        break;

      case CCDT_GNU:
        break;  /* @@@ already put into .ctors/.dtors anyway? */

      case CCDT_VBCC_ELF:  /* no leading underscores */
        elf = 1;
        ctor_label++;
        dtor_label++;
      case CCDT_VBCC:
        add_xtor_sym(gv,1,ctor_label);  /* define __CTOR_LIST__ */
        add_xtor_sym(gv,0,dtor_label);  /* define __DTOR_LIST__ */
        add_xtors(gv,&gv->selobjects,vbcc_xtors_pri,elf,
                  vbcc_ctor,vbcc_dtor,csec,dsec,ctor_label,dtor_label);
        add_xtors(gv,&gv->libobjects,vbcc_xtors_pri,elf,
                  vbcc_ctor,vbcc_dtor,csec,dsec,ctor_label,dtor_label);
        break;

      case CCDT_SASC:
        /* ___ctors/___dtors will be directed to __CTOR_LIST__/__DTOR_LIST */
        add_xtor_sym(gv,1,ctor_label);  /* define __CTOR_LIST__ */
        add_xtor_sym(gv,0,dtor_label);  /* define __DTOR_LIST__ */
        add_xtors(gv,&gv->selobjects,sasc_xtors_pri,0,
                  sasc_ctor,sasc_dtor,csec,dsec,ctor_label,dtor_label);
        add_xtors(gv,&gv->libobjects,sasc_xtors_pri,0,
                  sasc_ctor,sasc_dtor,csec,dsec,ctor_label,dtor_label);
        break;

      default:
        ierror("collect_constructors(): Unsupported type: %u\n",
               gv->collect_ctors_type);
        break;
    }
  }
}


struct Section *find_sect_type(struct ObjectUnit *ou,uint8_t type,uint8_t prot)
/* find a section in current object unit with approp. type and protection */
{
  struct Section *sec;

  for (sec=(struct Section *)ou->sections.first;
       sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
    if (sec->type==type && (sec->protection&prot)==prot
        && (sec->flags & SF_ALLOC))
      return sec;
  }
  return NULL;
}


struct Section *find_sect_id(struct ObjectUnit *ou,uint32_t id)
/* find a section by its identification value */
{
  struct Section *sec;

  for (sec=(struct Section *)ou->sections.first;
       sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
    if (sec->id == id)
      return sec;
  }
  return NULL;
}


struct Section *find_sect_name(struct ObjectUnit *ou,const char *name)
/* find a section by its name */
{
  struct Section *sec;

  for (sec=(struct Section *)ou->sections.first;
       sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
    if (!strcmp(name,sec->name))
      return sec;
  }
  return NULL;
}


struct Section *find_first_bss_sec(struct LinkedSection *ls)
/* returns pointer to first BSS-type section in list, or zero */
{
  struct Section *sec;

  for (sec=(struct Section *)ls->sections.first;
       sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
    if (sec->flags & SF_UNINITIALIZED)
      return sec;
  }
  return NULL;
}


struct LinkedSection *find_lnksec(struct GlobalVars *gv,const char *name,
                                  uint8_t type,uint8_t flags,uint8_t fmask,
                                  uint8_t prot)
/* Return pointer to the first section which fits the passed */
/* name-, type- and flags-conditions. If a condition is 0, then */
/* it is ignored. If no appropriate section was found, return NULL. */
{
  struct LinkedSection *ls = (struct LinkedSection *)gv->lnksec.first;
  struct LinkedSection *nextls;

  while (nextls = (struct LinkedSection *)ls->n.next) {
    bool found = TRUE;

    if (name)
      if (strcmp(ls->name,name))
        found = FALSE;
    if (type)
      if (ls->type != type)
        found = FALSE;
    if (fmask)
      if ((ls->flags&fmask) != (flags&fmask))
        found = FALSE;
    if (prot)
      if ((ls->protection & prot) != prot)
        found = FALSE;
    if (found)
      return ls;
    ls = nextls;
  }
  return NULL;
}


struct ObjectUnit *create_objunit(struct GlobalVars *gv,
                                  struct LinkFile *lf,const char *objname)
/* creates and initializes an ObjectUnit node */
{
  struct ObjectUnit *ou = alloc(sizeof(struct ObjectUnit));

  ou->lnkfile = lf;
  if (objname)
    ou->objname = objname;
  else
    ou->objname = noname;
  initlist(&ou->sections);  /* empty section list */
  ou->common = ou->scommon = NULL;
  ou->objsyms = alloc_hashtable(OBJSYMHTABSIZE);
  initlist(&ou->stabs);  /* empty stabs list */
  initlist(&ou->pripointers);  /* empty PriPointer list */
  ou->flags = 0;
  ou->min_alignment = gv->min_alignment;
  return ou;
}


struct ObjectUnit *art_objunit(struct GlobalVars *gv,const char *n,
                               uint8_t *d,unsigned long len)
/* creates and initializes an artificial linker-object */
{
  struct LinkFile *lf = alloczero(sizeof(struct LinkFile));

  lf->pathname = lf->filename = lf->objname = n;
  lf->data = d;
  lf->length = len;
  lf->format = gv->dest_format;
  lf->type = ID_ARTIFICIAL;
  return create_objunit(gv,lf,n);
}


void add_objunit(struct GlobalVars *gv,struct ObjectUnit *ou,bool fixrelocs)
/* adds an ObjectUnit to the appropriate list */
{
  if (ou) {
    uint8_t t = ou->lnkfile->type;

    if (t==ID_LIBARCH && gv->whole_archive)
      t = ID_OBJECT;  /* force linking of a whole archive */

    switch (t) {
      case ID_OBJECT:
      case ID_EXECUTABLE:
        ou->flags |= OUF_LINKED;  /* objects are always linked */
        addtail(&gv->selobjects,&ou->n);
        break;
      case ID_LIBARCH:
        addtail(&gv->libobjects,&ou->n);
        break;
      case ID_SHAREDOBJ:
        addtail(&gv->sharedobjects,&ou->n);
        break;
      default:
        ierror("add_objunit(): Link File type = %d",
               (int)ou->lnkfile->type);
    }

    if (fixrelocs) {  /* convert section index into address */
      struct Section *sec;
      struct Reloc *r;
      uint32_t idx;

      for (sec=(struct Section *)ou->sections.first;
           sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
        for (r=(struct Reloc *)sec->relocs.first;
             r->n.next!=NULL; r=(struct Reloc *)r->n.next) {
          idx = r->relocsect.id;
          if ((r->relocsect.ptr = find_sect_id(ou,idx)) == NULL) {
            /* section with this index doesn't exist */
            error(52,ou->lnkfile->pathname,sec->name,getobjname(ou),(int)idx);
          }
        }
      }
    }
  }
}


struct SecAttrOvr *addsecattrovr(struct GlobalVars *gv,char *name,
                                 uint32_t flags)
/* Create a new SecAttrOvr node and append it to the list. When a node
   for the same input section name is already present, then reuse it.
   Print a warning, when trying to reset the same attribute. */
{
  struct SecAttrOvr *sao;

  for (sao=gv->secattrovrs; sao!=NULL; sao=sao->next) {
    if (!strcmp(sao->name,name))
      break;
  }

  if (sao != NULL) {
    if ((sao->flags & flags) != 0)
      error(129,name);  /* resetting attribute for section */
  }
  else {
    struct SecAttrOvr *prev = gv->secattrovrs;

    sao = alloczero(sizeof(struct SecAttrOvr)+strlen(name));
    strcpy(sao->name,name);
    if (prev) {
      while (prev->next)
        prev = prev->next;
      prev->next = sao;
    }
    else
      gv->secattrovrs = sao;
  }

  sao->flags |= flags;
  return sao;
}


struct SecAttrOvr *getsecattrovr(struct GlobalVars *gv,const char *name,
                                 uint32_t flags)
/* Return a SecAttrOvr node which matches section name and flags. */
{
  struct SecAttrOvr *sao;

  if (name == NULL)
    name = noname;
  for (sao=gv->secattrovrs; sao!=NULL; sao=sao->next) {
    if (!strcmp(sao->name,name) && (sao->flags & flags)!=0)
      break;
  }
  return sao;
}


void addsecrename(const char *orgname,const char *newname)
/* Create a new SecRename node and append it to the list. When a node
   for the same input section name is already present, then reuse it.
   When the new name matches the original name: remove the node. */
{
  struct SecRename *sr,*prev;

  for (sr=secrenames,prev=NULL; sr!=NULL; sr=sr->next) {
    if (!strcmp(sr->orgname,orgname))
      break;
    prev = sr;
  }

  if (sr != NULL) {
    if (!strcmp(sr->orgname,newname)) {
      /* renaming is disabled - remove that node */
      if (prev)
        prev->next = sr->next;
      else
        secrenames = sr->next;
      free(sr);
    }
    else
      sr->newname = newname;
  }
  else {
    sr = alloc(sizeof(struct SecRename));
    sr->next = NULL;
    sr->orgname = orgname;
    sr->newname = newname;
    if (prev)
      prev->next = sr;
    else
      secrenames = sr;
  }
}


struct SecRename *getsecrename(void)
/* Return a copy of the current section-renaming list. */
{
  struct SecRename *sr,*newsr,*srcopy,*srlast;

  for (sr=secrenames,srcopy=NULL; sr!=NULL; sr=sr->next) {
    newsr = alloc(sizeof(struct SecRename));
    newsr->next = NULL;
    newsr->orgname = sr->orgname;
    newsr->newname = sr->newname;

    if (srlast = srcopy) {
      while (srlast->next != NULL)
        srlast = srlast->next;
      srlast->next = newsr;
    }
    else
      srcopy = newsr;
  }
  return srcopy;
}


static const char *do_rename(struct SecRename *sr,const char *name)
/* Find matching SecRename node and return the new name, if present.
   Otherwise return the original name. */
{
  static unsigned long unique_sec_id;
  char *newname,*p;

  if (name != NULL) {
    for (; sr!=NULL; sr=sr->next) {
      if (!strcmp(sr->orgname,name)) {
        name = sr->newname;
        break;
      }
    }

    /* replace "DONTMERGE" by a unique section id */
    if (p = strstr(name,"DONTMERGE_")) {
      newname = alloc(strlen(name)+1);
      memcpy(newname,name,p-name);
      sprintf(newname+(p-name),"%09lu%s",unique_sec_id++,p+9);
      return newname;
    }
  }
  else
    return noname;

  return name;
}


struct Section *create_section(struct ObjectUnit *ou,const char *name,
                               uint8_t *data,unsigned long size)
/* creates and initializes a Section node */
{
  static uint32_t idcnt;
  struct Section *s = alloczero(sizeof(struct Section));

  s->name = do_rename(ou->lnkfile->renames,name);
  s->hash = elf_hash(s->name);
  s->data = data;
  s->size = size;
  s->obj = ou;
  s->id = idcnt++;       /* target dependent - ELF replaces this with shndx */
  initlist(&s->relocs);  /* empty relocation list */
  initlist(&s->xrefs);   /* empty xref list */
  return s;
}


struct Section *add_section(struct ObjectUnit *ou,const char *name,
                            uint8_t *data,unsigned long size,
                            uint8_t type,uint8_t flags,uint8_t protection,
                            uint8_t align,bool inv)
/* create a new section and add it to the object ou */
{
  struct Section *sec = create_section(ou,name,data,size);

  sec->type = type;
  sec->flags = flags;
  sec->protection = protection;
  sec->alignment = ou->min_alignment>align ? ou->min_alignment : align;
  if (inv)
    sec->id = INVALID;
  if (type != ST_TMP)  /* TMP sections must not be part of the link process */
    addtail(&ou->sections,&sec->n);
  return sec;
}


bool is_common_sec(struct GlobalVars *gv,struct Section *sec)
{
  return !SECNAMECMPH(sec,gv->common_sec_name,gv->common_sec_hash) ||
         !SECNAMECMPH(sec,gv->scommon_sec_name,gv->scommon_sec_hash);
}


bool is_common_ls(struct GlobalVars *gv,struct LinkedSection *ls)
{
  return !SECNAMECMPH(ls,gv->common_sec_name,gv->common_sec_hash) ||
         !SECNAMECMPH(ls,gv->scommon_sec_name,gv->scommon_sec_hash);
}


struct Section *common_section(struct GlobalVars *gv,struct ObjectUnit *ou)
/* returns the dummy section for COMMON symbols, or creates it */
{
  struct Section *s;

  if (!(s = ou->common)) {
    s = add_section(ou,gv->common_sec_name,NULL,0,ST_UDATA,
                    SF_ALLOC|SF_UNINITIALIZED,SP_READ|SP_WRITE,0,TRUE);
    ou->common = s;
  }
  return s;
}


struct Section *scommon_section(struct GlobalVars *gv,struct ObjectUnit *ou)
/* returns the dummy section for small-data COMMON symbols, or creates it */
{
  struct Section *s;

  if (!(s = ou->scommon)) {
    s = add_section(ou,gv->scommon_sec_name,NULL,0,ST_UDATA,
                    SF_ALLOC|SF_UNINITIALIZED,SP_READ|SP_WRITE,0,TRUE);
    if (ou->common)
      s->alignment = ou->common->alignment;  /* inherit .common alignment */
    ou->scommon = s;
  }
  return s;
}


struct Section *abs_section(struct ObjectUnit *ou)
/* return first section available or create a new one */
{
  struct Section *s;

  if (listempty(&ou->sections))
    s = add_section(ou,noname,NULL,0,ST_UDATA,
                    SF_ALLOC|SF_UNINITIALIZED,SP_READ|SP_WRITE,0,TRUE);
  else
    s = (struct Section *)ou->sections.first;

  return s;
}


#if UNUSED
struct Section *dummy_section(struct GlobalVars *gv,struct ObjectUnit *ou)
/* Make a dummy section already attached to a dummy-LinkedSection, which
   won't appear in any section lists.
   Can be used for relocatable linker symbols, which need a specific value. */
{
  struct Section *s;

  if (!(s = gv->dummysec)) {
    struct LinkedSection *ls = alloczero(sizeof(struct LinkedSection));

    s = add_section(ou,"*linker*",NULL,0,ST_TMP,
                    SF_ALLOC|SF_UNINITIALIZED,SP_READ|SP_WRITE,0,TRUE);
    s->lnksec = ls;
    ls->name = s->name;
    ls->hash = s->hash;
    ls->type = s->type;
    ls->flags = s->flags;
    ls->protection = s->protection;
    ls->index = INVALID;
    initlist(&ls->sections);
    initlist(&ls->relocs);
    initlist(&ls->xrefs);
    initlist(&ls->symbols);
    gv->dummysec = s;
  }
  return s;
}
#endif


struct LinkedSection *create_lnksect(struct GlobalVars *gv,const char *name,
                                     uint8_t type,uint8_t flags,
                                     uint8_t protection,uint8_t alignment,
                                     uint32_t memattr)
/* create and initialize a LinkedSection node and include */
/* it in the global list */
{
  struct LinkedSection *ls = alloczero(sizeof(struct LinkedSection));

  ls->index = gv->nsecs++;
  ls->name = name;
  ls->hash = elf_hash(name);
  ls->type = type;
  ls->flags = flags;
  ls->protection = protection;
  ls->alignment = alignment;
  ls->memattr = memattr;
  initlist(&ls->sections);
  initlist(&ls->relocs);
  initlist(&ls->xrefs);
  initlist(&ls->symbols);
  addtail(&gv->lnksec,&ls->n);
  return ls;
}


static struct Section *add_xtor_section(struct GlobalVars *gv,
                                        struct ObjectUnit *ou,const char *name,
                                        uint8_t *data,unsigned long size)
{
  struct Section *sec = add_section(ou,name,data,size,ST_DATA,
                                    SF_ALLOC,SP_READ,gv->ptr_alignment,FALSE);
  struct SecAttrOvr *sao;

  if (sao = getsecattrovr(gv,name,SAO_MEMFLAGS))
    sec->memattr = sao->memflags;

  return sec;
}


static void write_constructors(struct GlobalVars *gv,struct ObjectUnit *ou,
                               struct Symbol *labelsym,int cnt,
                               size_t offset,const char *secname)
{
  uint8_t *data = ou->lnkfile->data;
  unsigned long asize;
  struct Section *sec;
  struct PriPointer *pp;
  int extraslots;

#if 0
  asize = (unsigned long)fff[gv->dest_format]->addr_bits / 8;
  if(asize == 0)
    asize = gv->bits_per_taddr / 8;
#else
  asize = gv->tbytes_per_taddr;
#endif

  /* Format for vbcc constructors: <num>, [ <ptrs>... ], NULL */
  /* Format for SAS/C constructors: [ <ptrs>...], NULL */
  switch (gv->collect_ctors_type) {
    case CCDT_SASC:
      extraslots = 1;
      break;
    default:
      extraslots = 2;
      break;
  }
  /* create a new section, if required */
  for (sec=(struct Section *)ou->sections.first;
       sec->n.next!=NULL; sec=(struct Section *)sec->n.next) {
    if (!strcmp(secname,sec->name))
      break;
  }
  if (sec->n.next == NULL) {
    data += tbytes(gv,offset);
    offset = 0;
    sec = add_xtor_section(gv,ou,secname,data,extraslots*asize);
  }
  else
    sec->size += extraslots*asize;

  /* assign constructor/destructor label symbol to start of list */
  unlink_lnksymbol(gv,labelsym);
  labelsym->relsect = sec;
  labelsym->type = SYM_RELOC;
  labelsym->value = offset;
  labelsym->size = extraslots * asize;
  add_objsymbol(ou,labelsym);
  addglobsym(gv,labelsym);  /* make it globally visible */

  if (gv->collect_ctors_type != CCDT_SASC) {
    writetaddr(gv,data,offset,(lword)cnt);
    offset += asize;
  }

  /* write con-/destructor pointers */
  for (pp=(struct PriPointer *)gv->pripointers.first;
       pp->n.next!=NULL; pp=(struct PriPointer *)pp->n.next) {
    if (!strcmp(pp->listname,labelsym->name)) {
      writetaddr(gv,data,offset,pp->addend);
      if (pp->xrefname)
        addreloc(sec,newreloc(gv,sec,pp->xrefname,NULL,0,(unsigned long)offset,
                              gv->pcrel_ctors?R_PC:R_ABS,pp->addend),
                 0,asize*gv->bits_per_tbyte,-1);
      sec->size += asize;
      labelsym->size += asize;
      offset += asize;
    }
  }
}


void make_constructors(struct GlobalVars *gv)
/* Create an artificial object for constructor/destructor lists
   and fill them will entries from the PriPointer list. */
{
  if (!gv->dest_object && (gv->ctor_symbol || gv->dtor_symbol)) {
    int nctors=0,ndtors=0;
    bool ctors=FALSE,dtors=FALSE;
    const char *csecname=NULL,*dsecname=NULL;
    size_t clen,dlen;
    struct PriPointer *pp;
    uint8_t *data;
    struct ObjectUnit *ou;
    size_t asize;

    /* check if constructors or destructors are needed (referenced) */
    if (gv->ctor_symbol) {
      if (gv->ctor_symbol->flags & SYMF_REFERENCED) {
        ctors = TRUE;
        for (pp=(struct PriPointer *)gv->pripointers.first;
             pp->n.next!=NULL; pp=(struct PriPointer *)pp->n.next) {
          if (!strcmp(pp->listname,gv->ctor_symbol->name)) {
            if (csecname) {
              if (strcmp(csecname,pp->secname))
                error(125);  /* CTORS/DTORS spread over multiple sections */
            }
            else
              csecname = pp->secname;
          }
        }
        if (!csecname)
          csecname = xtors_secname(gv,ctors_name);
      }
    }
    if (gv->dtor_symbol) {
      if (gv->dtor_symbol->flags & SYMF_REFERENCED) {
        dtors = TRUE;
        for (pp=(struct PriPointer *)gv->pripointers.first;
             pp->n.next!=NULL; pp=(struct PriPointer *)pp->n.next) {
          if (!strcmp(pp->listname,gv->dtor_symbol->name)) {
            if (dsecname) {
              if (strcmp(dsecname,pp->secname))
                error(125);  /* CTORS/DTORS spread over multiple sections */
            }
            else
              dsecname = pp->secname;
          }
        }
        if (!dsecname)
          dsecname = xtors_secname(gv,dtors_name);
      }
    }
    if (!ctors && !dtors)
      return;

    /* count number of constructor and destructor pointers */
    for (pp=(struct PriPointer *)gv->pripointers.first;
         pp->n.next!=NULL; pp=(struct PriPointer *)pp->n.next) {
      if (ctors) {
        if (!strcmp(pp->listname,gv->ctor_symbol->name))
          nctors++;
      }
      if (dtors) {
        if (!strcmp(pp->listname,gv->dtor_symbol->name))
          ndtors++;
      }
    }

#if 0
    asize = fff[gv->dest_format]->addr_bits / 8;
    if(asize == 0)
      asize = gv->bits_per_taddr / 8;
#else
    asize = gv->tbytes_per_taddr;
#endif

    /* create artificial object */
    clen = asize * (ctors ? nctors+2 : 0);
    dlen = asize * (dtors ? ndtors+2 : 0);
    data = alloczero(tbytes(gv,clen+dlen));
    ou = art_objunit(gv,"INITEXIT",data,tbytes(gv,clen+dlen));

    /* write constructors/destructors */
    if (ctors)
      write_constructors(gv,ou,gv->ctor_symbol,nctors,0,csecname);
    if (dtors)
      write_constructors(gv,ou,gv->dtor_symbol,ndtors,clen,dsecname);

    /* enqueue artificial object unit into linking process */
    ou->lnkfile->type = ID_OBJECT;
    add_objunit(gv,ou,FALSE);
  }
}


struct LinkedSection *smalldata_section(struct GlobalVars *gv)
/* Return pointer to first small data LinkedSection. If not existing, */
/* return first data/bss section or the first section. */
{
  struct LinkedSection *ls = (struct LinkedSection *)gv->lnksec.first;
  struct LinkedSection *ldls=NULL,*firstls=NULL,*nextls;

  while (nextls = (struct LinkedSection *)ls->n.next) {
    if ((ls->flags & SF_ALLOC) && ls->size!=0) {
      if (firstls == NULL)
        firstls = ls;
      if (ls->type==ST_DATA || ls->type==ST_UDATA && ldls==NULL)
        ldls = ls;
      if (ls->flags & SF_SMALLDATA)  /* first SD section found! */
        return ls;
    }
    ls = nextls;
  }
  return ldls ? ldls : firstls;
}


void get_text_data_bss(struct GlobalVars *gv,struct LinkedSection **sections)
/* find exactly one ST_CODE, ST_DATA and ST_UDATA section, which
   will become .text, .data and .bss */
{
  static const char *fn = "get_text_data_bss(): ";
  struct LinkedSection *ls;

  sections[0] = sections[1] = sections[2] = NULL;
  for (ls=(struct LinkedSection *)gv->lnksec.first;
       ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
    if (ls->flags & SF_ALLOC) {
      switch (ls->type) {
        case ST_UNDEFINED:
          /* @@@ discard undefined sections - they are empty anyway */
          ls->flags &= ~SF_ALLOC;
          break;
        case ST_CODE:
          if (sections[0]==NULL)
            sections[0] = ls;
          else
            error(138,fff[gv->dest_format]->tname,"text",
                  sections[0]->name,ls->name);
          break;
        case ST_DATA:
          if (sections[1]==NULL)
            sections[1] = ls;
          else
            error(138,fff[gv->dest_format]->tname,"data",
                  sections[1]->name,ls->name);
          break;
        case ST_UDATA:
          if (sections[2]==NULL)
            sections[2] = ls;
          else
            error(138,fff[gv->dest_format]->tname,"bss",
                  sections[2]->name,ls->name);
          break;
        default:
          ierror("%sIllegal section type %d (%s)",fn,(int)ls->type,ls->name);
          break;
      }
    }
  }
}


void text_data_bss_gaps(struct LinkedSection **sections)
/* calculate gap size between text-data and data-bss */
{
  if (sections[0]) {
    lword nextsecbase = sections[1] ? sections[1]->base :
                                      (sections[2] ? sections[2]->base : 0);
    if (nextsecbase) {
      sections[0]->gapsize = nextsecbase -
                             (sections[0]->base + sections[0]->size);
    }
  }
  if (sections[1] && sections[2]) {
    sections[1]->gapsize = sections[2]->base -
                           (sections[1]->base + sections[1]->size);
  }
}


bool discard_symbol(struct GlobalVars *gv,struct Symbol *sym)
/* checks if symbol can be discarded and excluded from the output file */
{
  if (sym->flags & SYMF_PROTECTED)
    return FALSE;

  if (gv->strip_symbols < STRIP_ALL) {
    if (sym->bind!=SYMB_LOCAL || gv->discard_local==DISLOC_NONE)
      return FALSE;

    if (gv->discard_local==DISLOC_TMP) {
      char c = sym->name[0];

      if (!((c=='L' || c=='l' || c=='.')
          && isdigit((unsigned char)sym->name[1])))
        return FALSE;
    }
  }
  return TRUE;
}


lword entry_address(struct GlobalVars *gv)
/* returns address of entry point for executables */
{
  struct Symbol *sym;
  struct LinkedSection *ls;

  if (gv->entry_name) {
    lword entry = 0;

    if (sym = findsymbol(gv,NULL,gv->entry_name,0)) {
      return (lword)sym->value;
    }
    else if (isdigit((unsigned char)*gv->entry_name)) {
      long long tmp;

      if (sscanf(gv->entry_name,"%lli",&tmp) == 1)
        return entry = tmp;
    }
  }

  /* plan b: search for _start symbol: */
  if (sym = findsymbol(gv,NULL,"_start",0))
      return (lword)sym->value;

  /* plan c: search for first executable section */
  if (ls = find_lnksec(gv,NULL,ST_CODE,SF_ALLOC,SF_ALLOC|SF_UNINITIALIZED,
                       SP_READ|SP_EXEC))
    return ls->base;

  return 0;
}


struct Section *entry_section(struct GlobalVars *gv)
{
  struct Section *sec;
  struct Symbol *sym;

  /* get section of entry-symbol or _start */
  if (gv->entry_name) {
    if ((sym = findsymbol(gv,NULL,gv->entry_name,0)) == NULL)
      error(131);  /* need a valid symbolic entry */
  }
  else
    sym = findsymbol(gv,NULL,"_start",0);

  if (sym == NULL) {
    /* no entry: use the first code section from the first real object file */
    struct ObjectUnit *ou = (struct ObjectUnit *)gv->selobjects.first;

    while (ou->n.next!=NULL && (ou->flags&OUF_SCRIPT))
      ou = (struct ObjectUnit *)ou->n.next;
    sec = ou->n.next ? find_sect_type(ou,ST_CODE,SP_READ|SP_EXEC) : NULL;
  }
  else
    sec = sym->relsect;

  if (sec == NULL)
    error(132);  /* executable code section in 1st object required */
  return sec;
}


struct Symbol *bss_entry(struct GlobalVars *gv,struct ObjectUnit *ou,
                         const char *secname,struct Symbol *xdef)
/* Create a BSS section in the object ou with space for xdef's size. 
   The symbol will be changed to the base address of this section and
   enqueued in the section's object symbol list.
   A size of 0 will leave the symbol untouched (no need to copy), which
   is indicated by returning NULL.
   This function is called for R_COPY objects, used for dynamic linking. */
{
  if (xdef->size) {
    /* change xdef to point to our new object in sec */
    unlink_objsymbol(xdef);
    xdef->value = 0;
    xdef->relsect = add_section(ou,secname,NULL,xdef->size,ST_UDATA,
                                SF_ALLOC|SF_UNINITIALIZED,
                                SP_READ|SP_WRITE,gv->ptr_alignment,TRUE);
    add_objsymbol(ou,xdef);
    return xdef;
  }

  return NULL;
}


void trim_sections(struct GlobalVars *gv)
{
  struct LinkedSection *ls;
  struct Section *sec,*nextsec;

  /* Remove zero-bytes at the end of a section from filesize in executables */
  if (!gv->dest_object && !gv->keep_trailing_zeros) {
    for (ls=(struct LinkedSection *)gv->lnksec.first;
         ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
      for (sec=(struct Section *)ls->sections.last;
           sec->n.pred!=NULL; sec=(struct Section *)sec->n.pred) {
        if (sec->data!=NULL && !(sec->flags & SF_UNINITIALIZED)) {
          /* This is the last initialized sub-section, so check for
             trailing zero-bytes, which can be subtracted from filesize. */
          unsigned long secinit = sec->size;
          uint8_t *p = sec->data + tbytes(gv,secinit) - gv->octets_per_tbyte;

          while (secinit>sec->last_reloc && readtbyte(gv,p)==0) {
            --secinit;
            p -= gv->octets_per_tbyte;
          }
          ls->filesize = sec->offset + secinit;
          break;
        }
      }
    }
  }
}


void untrim_sections(struct GlobalVars *gv,int bss_too)
{
  struct LinkedSection *ls;

  /* set filesize=size in all sections */
  for (ls=(struct LinkedSection *)gv->lnksec.first;
       ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
    if (bss_too || (ls->type!=ST_UDATA && !(ls->flags&SF_UNINITIALIZED)))
      ls->filesize = ls->size;
  }
}


struct LinkedSection *load_next_section(struct GlobalVars *gv)
/* return pointer to next section with lowest LMA and remove it from list */
{
  struct LinkedSection *ls = (struct LinkedSection *)gv->lnksec.first;
  struct LinkedSection *nextls,*minls = NULL;

  while (nextls = (struct LinkedSection *)ls->n.next) {
    if (minls) {
      if (ls->copybase < minls->copybase)
        minls = ls;
    }
    else
      minls = ls;
    ls = nextls;
  }
  if (minls) {
    remnode(&minls->n);
  }
  return minls;
}

/* $VER: vlink t_rawseg.c V0.18 (23.12.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2024  Frank Wille
 */


#include "config.h"
#if defined(RAWSEG)
#define T_RAWSEG_C
#include "vlink.h"
#include "elfcommon.h"


struct SegReloc {
  struct SegReloc *next;
  struct Phdr *seg;
  lword offset;
};


static unsigned long rawseg_headersize(struct GlobalVars *);
static int rawseg_identify(struct GlobalVars *,char *,uint8_t *,
                           unsigned long,bool);
static void rawseg_readconv(struct GlobalVars *,struct LinkFile *);
static int rawseg_targetlink(struct GlobalVars *,struct LinkedSection *,
                              struct Section *);
static void rawseg_writeobject(struct GlobalVars *,FILE *);
static void rawseg_writeshared(struct GlobalVars *,FILE *);
static void rawseg_writeexec(struct GlobalVars *,FILE *);

static const char defaultscript[] =
  "PHDRS {\n"
  "  text PT_LOAD;\n"
  "  data PT_LOAD;\n"
  "}\n"
  "SECTIONS {\n"
  "  .text: { *(.text*) *(CODE*) *(text*) *(seg*) } :text\n"
  "  .data: { *(.data*) *(DATA*) *(data*) } :data\n"
  "  .bss: { *(.bss*) *(BSS*) *(bss*) }\n"
  "}\n";


#ifdef RAWSEG
struct FFFuncs fff_rawseg = {
  "rawseg",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  rawseg_headersize,
  rawseg_identify,
  rawseg_readconv,
  NULL,
  rawseg_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawseg_writeobject,
  rawseg_writeshared,
  rawseg_writeexec,
  NULL,NULL,
  0,
  0x8000,
  0,
  0,
  RTAB_UNDEF,0,
  -1,   /* endianness undefined, only write */
  0,0   /* addr_bits from input */
};
#endif


/*****************************************************************/
/*                        Read Binary                            */
/*****************************************************************/


static unsigned long rawseg_headersize(struct GlobalVars *gv)
{
  return 0;  /* no header - it's pure binary! */
}


static int rawseg_identify(struct GlobalVars *gv,char *name,uint8_t *p,
                           unsigned long plen,bool lib)
/* identify a binary file */
{
  return ID_UNKNOWN;  /* binaries are only allowed to be written! */
}


static void rawseg_readconv(struct GlobalVars *gv,struct LinkFile *lf)
{
  ierror("rawseg_readconv(): Can't read raw-binaries");
}


static int rawseg_targetlink(struct GlobalVars *gv,struct LinkedSection *ls,
                             struct Section *s)
/* returns 1, if target requires the combination of the two sections, */
/* returns -1, if target doesn't want to combine them, */
/* returns 0, if target doesn't care - standard linking rules are used. */
{
  ierror("rawseg_targetlink(): Impossible to link raw-binaries");
  return 0;
}


/*****************************************************************/
/*                        Write Binary                           */
/*****************************************************************/

static void rawseg_writeshared(struct GlobalVars *gv,FILE *f)
{
  error(30);  /* Target file format doesn't support shared objects */
}


static void rawseg_writeobject(struct GlobalVars *gv,FILE *f)
{
  error(62);  /* Target file format doesn't support relocatable objects */
}


static void rawseg_writeexec(struct GlobalVars *gv,FILE *f)
/* creates a new file for each segment, writes file name, start address */
/* and length into the output file */
{
  const char *fn = "rawseg_writeexec(): ";
  bool firstsec;
  lword addr;
  FILE *segf;
  struct Phdr *p;
  struct LinkedSection *ls,*prevls;
  struct SegReloc *srlist;
  char buf[256];

  for (p=gv->phdrlist; p; p=p->next) {
    if (p->type==PT_LOAD && (p->flags&PHDR_USED) &&
        p->start!=ADDR_NONE && p->start_vma!=ADDR_NONE) {

      firstsec = TRUE;
      srlist = NULL;
      segf = NULL;

      /* write segment's sections */
      for (ls=(struct LinkedSection *)gv->lnksec.first;
           ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
        if (ls->copybase>=p->start && (ls->copybase+ls->size)<=p->mem_end &&
            (ls->flags & SF_ALLOC) && !(ls->ld_flags & LSF_NOLOAD)) {

          if (gv->keep_relocs) {
            /* remember relocations, adjusted from section to segment base */
            struct Reloc *r;
            struct RelocInsert *ri;
            struct LinkedSection *relsec;
            struct Phdr *relph;

            for (r=(struct Reloc *)ls->relocs.first;
                 r->n.next!=NULL; r=(struct Reloc *)r->n.next) {
              if (ri = r->insert) {
                if (r->rtype!=R_ABS ||
                    ri->bpos!=0 || ri->bsiz!=gv->bits_per_taddr) {
                  /* only abs. relocs with target addr size are supported */
                  error(32,fff_rawseg.tname,reloc_name[r->rtype],
                        (int)ri->bpos,(int)ri->bsiz,
                        mtaddr(gv,ri->mask),ls->name,r->offset);
                  continue;
                }
              }
              else
                continue;

              if (r->relocsect.lnk == NULL) {
                if (r->flags & RELF_DYNLINK)
                  continue;  /* NULL, because it was resolved by a sh.obj. */
                else
                  ierror("%sReloc type %d (%s) at %s+0x%lx "
                         "(addend 0x%llx) is missing a relocsect.lnk",
                         fn,(int)r->rtype,reloc_name[r->rtype],ls->name,
                         r->offset,(unsigned long long)r->addend);
              }
              relsec = r->relocsect.lnk;
              /* @@@ Check for SF_ALLOC? Shouldn't matter here. */

              /* find out to which segment relsec belongs */
              for (relph=gv->phdrlist; relph; relph=relph->next) {
                if (relph->type==PT_LOAD && (relph->flags&PHDR_USED) &&
                    relph->start!=ADDR_NONE && relph->start_vma!=ADDR_NONE) {
                  if (relsec->copybase>=relph->start &&
                      (relsec->copybase+relsec->size)<=relph->mem_end)
                    break;
                }
              }
              /* use segment's base address for relocation instead */
              if (relph) {
                lword segoffs,v;
                struct SegReloc *newsr,*srp;

                segoffs = relsec->copybase - relph->start;
                v = writesection(gv,ls->data,r->offset,r,r->addend+segoffs);
                if (v != 0) {
                  /* Calculated value doesn't fit into relocation type x ... */
                  if (ri = r->insert) {
                    struct Section *isec;
                    unsigned long ioffs;

                    isec = getinpsecoffs(ls,r->offset,&ioffs);
                    /*print_function_name(isec,ioffs);*/
                    error(35,gv->dest_name,ls->name,r->offset,
                          getobjname(isec->obj),isec->name,ioffs,
                          v,reloc_name[r->rtype],(int)ri->bpos,
                          (int)ri->bsiz,(unsigned long long)ri->mask);
                  }
                  else
                    ierror("%sReloc (%s+%lx), type=%s, without RelocInsert",
                           fn,ls->name,r->offset,reloc_name[r->rtype]);
                }

                /* remember relocation offset and segment for later */
                newsr = alloc(sizeof(struct SegReloc));
                newsr->next = NULL;
                newsr->seg = relph;
                newsr->offset = (ls->copybase - p->start) + r->offset;
                if (srp = srlist) {
                  while (srp->next)
                    srp = srp->next;
                  srp->next = newsr;
                }
                else
                  srlist = newsr;
              }
              else
                ierror("%sNo segment for reloc offset section '%s'",
                       fn,relsec->name);
            }
          }
          else
            calc_relocs(gv,ls);

          if (segf == NULL) {
            /* create output file for segment, when not already opened */
            snprintf(buf,256,"%s.%s",gv->dest_name,p->name);
            segf = fopen(buf,"wb");
            if (segf == NULL) {
              error(29,buf);  /* cannot create file */
              break;
            }
            /* write file name, start addr. and length of segment to output */
            fprintf(f,"\"%s\" 0x%llx 0x%llx\n",
                    buf,(unsigned long long)p->start,
                    (unsigned long long)p->mem_end-p->start);
          }

          if (!firstsec) {
            /* write an alignment gap, when needed */
            if (ls->copybase > addr)
              fwritegap(gv,segf,ls->copybase-addr,0);
            else if (ls->copybase < addr)
              error(98,fff[gv->dest_format]->tname,ls->name,prevls->name);
          }
          else
            firstsec = FALSE;

          fwritefullsect(gv,segf,ls);

          addr = ls->copybase + ls->size;
          prevls = ls;
        }
      }
      if (segf != NULL)
        fclose(segf);

      if (srlist) {
        /* write relocation files for this segment */
        struct Phdr *relph;
        struct SegReloc *sr;
        unsigned rcnt;
        FILE *rf;

        for (relph=gv->phdrlist; relph; relph=relph->next) {
          for (sr=srlist,rf=NULL,rcnt=0; sr; sr=sr->next) {
            if (sr->seg == relph) {
              if (!rf) {
                snprintf(buf,256,"%s.%s.rel%s",
                         gv->dest_name,p->name,relph->name);
                rf = fopen(buf,"wb");
                if (rf == NULL) {
                  error(29,buf);  /* cannot create file */
                  continue;
                }
                /* number of relocs will be stored here */
                fwritetaddr(gv,rf,0);
              }
              fwritetaddr(gv,rf,sr->offset);
              rcnt++;
            }
          }
          if (rf) {
            /* write number of relocs into the first word */
            fseek(rf,0,SEEK_SET);
            fwritetaddr(gv,rf,(lword)rcnt);
            fclose(rf);
          }
        }
      }
    }
  }
}

#endif

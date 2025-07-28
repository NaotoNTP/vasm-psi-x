/* $VER: vlink vlink.h V0.18 (23.12.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2024  Frank Wille
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <ctype.h>
#include <limits.h>
#include "config.h"

/* program's name */
#define PNAME "vlink"

/* integer types - non ISO-C99 compliant compilers have to define TYPESxxBIT */
#if defined(TYPES32BIT)
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int int16_t;
typedef unsigned short int uint16_t;
typedef signed long int int32_t;
typedef unsigned long int uint32_t;
typedef signed long long int int64_t;
typedef unsigned long long int uint64_t;
typedef unsigned int uintptr_t;
#elif defined(TYPES64BIT)
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed long int int64_t;
typedef unsigned long int uint64_t;
typedef unsigned long uintptr_t;
#elif defined(_MSC_VER) && (_MSC_VER < 1600)
typedef __int8 int8_t;	            /* prior to VS2010 stdint.h is missing */
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>  /* C99 */
#endif

/* linker word for addresses, values, offsets, etc. */
typedef int64_t lword;

/* boolean values */
typedef int bool;

/* program constants */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif
#define INVALID (~0)
#define ADDR_NONE (~0)		/* no defined address: 0xffffffff */

#define MAXLEN 256		/* maximum length for symbols and buffers */
#define FNAMEBUFSIZE 1024       /* buffer size for file names */
#define MAX_FWALIGN 8192        /* max. alignment, when writing target file */

/* macros */
#define SECNAMECMP(s1,s2)       ((s1)->hash==(s2)->hash ? \
                                  strcmp((s1)->name,(s2)->name) : -1)
#define SECNAMECMPS(s,n)        ((s)->hash==elf_hash(n) ? \
                                  strcmp((s)->name,(n)) : -1)
#define SECNAMECMPH(s,n,h)      ((s)->hash==(h) ? \
                                  strcmp((s)->name,(n)) : -1)

/* structures */

struct node {
  struct node *next;
  struct node *pred;
};

struct list {
  struct node *first;
  struct node *dummy;
  struct node *last;
};


struct LibPath {                /* libpaths list. */
  struct node n;                /* Here we search for library archives */
  const char *path;             /* and shared objects. */
};

struct Flavours {               /* library flavours */
  int n_flavours;
  int flavours_len;
  const char **flavours;
  char *flavour_dir;
};

struct SecRename {              /* renamed input sections */
  struct SecRename *next;
  const char *orgname;
  const char *newname;
};

struct InputFile {              /* inputlist nodes */
  struct node n;                /* contains names & flags of all inp. files */
  const char *name;
  struct SecRename *renames;
  bool lib;                     /* search library */
  bool dynamic;                 /* try to link dynamic first */
  int so_ver;                   /* minimum version of shared object */
  uint16_t flags;
};

#define IFF_DELUNDERSCORE 1     /* remove preceding underscore from symbols */
#define IFF_ADDUNDERSCORE 2     /* add preceding underscore to symbols */


struct LinkFile {
  struct node n;
  const char *pathname;         /* full path: /usr/lib/libm.a */
  const char *filename;         /* file name: libm.a */
  const char *objname;          /* current obj. name: sin.o (archives only)*/
  uint8_t *data;                /* pointer to file data */
  unsigned long length;         /* length of file */
  struct SecRename *renames;    /* current input section renames */
  uint8_t format;               /* file format - index into targets table */
  uint8_t type;                 /* ID_OBJECT/SHAREDOBJ/LIBARCH */
  uint16_t flags;               /* flags from InputFile */
};


struct Expr {
  int type;
  struct Expr *left;
  struct Expr *right;
  lword val;
};

/* expression types */
enum {
  ADD,SUB,MUL,DIV,MOD,NEG,CPL,LAND,LOR,BAND,BOR,XOR,NOT,LSH,RSH,
  LT,GT,LEQ,GEQ,NEQ,EQ,ABS,REL
};


struct ObjectUnit {
  struct node n;
  struct LinkFile *lnkfile;     /* LinkFile, where the obj. belongs to */
  const char *objname;          /* name of object, if available, e.g. sin.o */
  struct list sections;         /* list of all sections in this object */
  struct Section *common;       /* dummy section for common symbols */
  struct Section *scommon;      /* dummy section for baserel. common sym. */
  struct Symbol **objsyms;      /* all symbols from this object unit */
  struct list stabs;            /* stab debugging symbols */
  struct list pripointers;      /* PriPointers of this unit */
  uint16_t flags;
  uint8_t  min_alignment;       /* minimal alignment for all sections */
  uint8_t  extra;               /* multi-purpose field */
};

#define OUF_LINKED 0x0001       /* object unit is marked as linked */
#define OUF_EHFPPC 0x0100       /* target ehf: PPC object unit */
#define OUF_SCRIPT 0x8000       /* linker script dummy object */
#define OBJSYMHTABSIZE 0x20     /* number of entries in symbol hash table */

struct RelRef {
  struct RelRef *next;
  struct Section *refsec;       /* relative referenced sections */
};

typedef unsigned srclinetype;
typedef unsigned long srcoffstype;

struct SourceLines {
  struct SourceLines *next;
  const char *path;             /* full path to source text */
  const char *name;             /* source text file name */
  char path_sep;                /* path separator character for name */
  unsigned char flags;
  unsigned short nameidx;       /* source text name table index */
  unsigned lang;                /* source language */
  unsigned entries;             /* number of line/offset entries */
  srclinetype *lines;	        /* array of line-numbers */
  srcoffstype *offsets;         /* array of section-offsets */
};
/* flags */
#define SLF_NOCASE      (1<<0)  /* file names are not case-sensitive */
/* lang */
#define DW_LANG_DEFAULT 0x0001  /* FIXME: C89 as default? */

struct Section {
  struct node n;
  struct ObjectUnit *obj;       /* link to ObjectUnit */
  struct LinkedSection *lnksec; /* ptr to merged output sections */
  const char *name;             /* section's name, e.g. .text, .data, ... */
  unsigned long hash;           /* section name's hash code */
  uint32_t id;                  /* unique section id - target dependent */
  uint8_t type;                 /* type: code, data, bss */
  uint8_t flags;
  uint8_t protection;           /* readable, writable, executable, ... */
  uint8_t alignment;            /* number of bits, which have to be zero */
  uint32_t memattr;             /* target-specific memory attributes */
  lword va;                     /* the section's virtual address */
  unsigned long offset;         /* offset relative to 1st sec. of same type */
  uint8_t *data;                /* the section's contents */
  unsigned long size;           /* the section's size in bytes (+alignment) */
  unsigned long last_reloc;	/* offset to location behind last reloc/xref */
  struct list relocs;           /* relocations for this section */
  struct list xrefs;            /* external references to unknown symbols */
  struct RelRef *relrefs;       /* all sections which are referenced rel. */
  struct SourceLines *srclines; /* optional source line debugging info */
  int link;                     /* link to other section (e.g. ELF-strtab) */
  uint16_t filldata;            /* used to fill gaps */
  unsigned internal_flags;      /* internal linker flags - see below */
};

/* section types */
#define ST_UNDEFINED 0
#define ST_CODE 1               /* section contains code */
#define ST_DATA 2               /* section contains initialized data */
#define ST_UDATA 3              /* section contains uninitialized data */
#define ST_TMP 4                /* a temporary, linker-generated section */
#define ST_LAST 3               /* last real section type */

/* section flags */
#define SF_ALLOC           0x01 /* allocate section in memory */
#define SF_UNINITIALIZED   0x02 /* section has uninitialized contents */
#define SF_SMALLDATA       0x04 /* section is referenced base-relative */
#define SF_LINKONCE        0x08 /* link only a single section with this name */
#define SF_REFERENCED      0x10 /* section was referenced */
#define SF_PORTABLE_MASK   0x3f /* mask for target-independent flags */
/* target specific section flags: */
#define SF_EHFPPC          0x40 /* target ehf: section contains PPC code */
#define SF_DPAGE           0x40 /* 6502/65816: zero/direct-page section */
#define SF_HUGEDATA        0x80 /* 65816: huge data section */

/* protection */
#define SP_READ 1
#define SP_WRITE 2
#define SP_EXEC 4
#define SP_SHARE 8

/* internal flags */
#define ILF_BADADDRINVA    (1<<0)  /* address out of range, saved in va */
#define ILF_BANKALIGN      (1<<1)  /* must be aligned to next bank in output */


struct SymbolMask {
  struct SymbolMask *next;
  const char *name;
  uint32_t common_mask;         /* common/ORed feature-mask of all references */
};

#define SMASKHTABSIZE 0x1000

struct RelocInsert {            /* describes how to insert a reloc addend */
  struct RelocInsert *next;
  uint16_t bpos;                /* bit-position counted from leftmost bit */
  uint16_t bsiz;                /* field size in bits */
  lword mask;                   /* mask to apply to addend */
};                              /*  addend will be normalized after that! */

struct Reloc {                  /* relocation information */
  struct node n;
  const char *xrefname;         /* not 0: this a an external sym. reference */
  union {
    uint32_t id;                /* section index */
    struct Section *ptr;        /* base addr of this sect. has to be added */
    struct LinkedSection *lnk;  /* base addr of merged output sections */
    struct Symbol *symbol;      /* symbol-pointer, if x-ref. was resolved */
    uint32_t smask;             /* symbol name's feat.mask (!OUF_LINKED) */
    struct SymbolMask *cmask;	/* ORed feat.mask of all xrefs with this name */
    int xrefinfo;               /* xref info/index field for output funcs */
  } relocsect;
  unsigned long offset;         /* section-offset of relocation */
  lword addend;                 /* add this to relocation value */
  struct RelocInsert *insert;   /* reloc insert field description */
  int rtype;                    /* general reloc type - see below */
  unsigned flags;               /* relocation specific flags */
};

/* general relocation types */
#define R_NONE 0
#define R_ABS 1                 /* absolute address relocation */
#define R_PC 2                  /* pc-relative relocation */
#define R_GOT 3                 /* symbol's pointer in global offset table */
#define R_GOTPC 4               /* pc-relative global offset table reloc */
#define R_GOTOFF 5              /* offset to global offset table base */
#define R_GLOBDAT 6             /* set global offset table entry */
#define R_PLT 7                 /* absolute procedure linkage table reloc */
#define R_PLTPC 8               /* pc-relative procedure linkage table reloc */
#define R_PLTOFF 9              /* offset to procedure linkage table */
#define R_SD 10                 /* small data, base-relative reloc */
#define R_UABS 11               /* unaligned absolute address relocation */
#define R_LOCALPC 12            /* pc-relative to local symbol */
#define R_LOADREL 13            /* relative to load address, no symbol */
#define R_COPY 14               /* copy from shared object */
#define R_JMPSLOT 15            /* procedure linkage table entry */
#define R_SECOFF 16             /* symbol's offset to start of section */
#define R_MEMID 17              /* ID-code of section's destination memory */
#define LAST_STANDARD_RELOC R_MEMID

/* internal relocation types */
#define R_ABSCOPY 128           /* absolute sh.obj. ref., needs R_COPY */
#define R_PCCOPY 129            /* pc-relative sh.obj. ref., needs R_COPY */

/* temporary signed/unsigned flags on type for newreloc() */
#define R_S 0x400               /* newreloc() signed relocation type */
#define R_U 0x800               /* newreloc() unsigned relocation type */

/* Reloc flags */
#define RELF_WEAK 1             /* reference is weak and defaults to 0 */
#define RELF_SMASK 2            /* reference has an smask (!OUF_LINKED) */
#define RELF_CMASK 4            /* reference has a cmask-pointer (LINKED!) */
#define RELF_INTERNAL 0x10      /* linker-internal relocation, not exported */
#define RELF_PLT 0x40           /* dynamic PLT relocation */
#define RELF_DYN 0x80           /* other dynamic relocation */
#define RELF_DYNLINK 0xc0       /* takes part in dynamic linking */
#define RELF_S 0x100            /* relocation is signed */
#define RELF_U 0x200            /* relocation is unsigned */


struct Symbol {
  struct node n;
  struct Symbol *glob_chain;    /* next symbol in global hash chain */
  struct Symbol *obj_chain;     /* next symbol in object hash chain */
  const char *name;             /* symbol's name */
  const char *indir_name;       /* indirect symbol name (SYM_INDIR) */
  lword value;                  /* abs. value, reloc. offset or alignment */
  struct Section *relsect;      /* symbol def. relative to this section */
  /* SYM_ABS are rel. to 1st section, SYM_COMMON rel. to a BSS section, */
  /* Linker-script symbols do have a NULL here */
  uint8_t type;                 /* absolute, relocatable or extern */
  uint8_t flags;
  uint8_t info;                 /* section, function or object */
  uint8_t bind;                 /* local, global or weak binding */
  uint32_t size;                /* symbol's size in bytes */
  uint32_t fmask;               /* gv->masked_symbols: feature bit-mask or 0 */
  int extra;                    /* extra data, used by some targets */
};

/* symbol type */
#define SYM_UNDEF 0
#define SYM_ABS 1               /* absolute value */
#define SYM_RELOC 2             /* relocation offset, relative to section */
#define SYM_COMMON 3            /* alignment constraints in value */
#define SYM_INDIR 4             /* indirect symbol, use indir_name instead */

/* symbol flags */
#define SYMF_LNKSYM 1           /* a linker-generated symbol */
#define SYMF_REFERENCED 2       /* symbol was referenced */
#define SYMF_PROTECTED 4        /* symbol is protected against stripping */
#define SYMF_PROVIDED 8         /* provided symbols, removed when unref. */
#define SYMF_SHLIB 0x10         /* symbol definition from a shared library */
#define SYMF_DYNIMPORT 0x40     /* imported symbol for dynamic linking */
#define SYMF_DYNEXPORT 0x80     /* exported symbol for dynamic linking */
#define SYMF_DYNLINK 0xc0       /* takes part in dynamic linking */

/* object type */
#define SYMI_NOTYPE 0
#define SYMI_OBJECT 1
#define SYMI_FUNC 2
#define SYMI_SECTION 3
#define SYMI_FILE 4

/* symbol bind */
#define SYMB_NONE 0
#define SYMB_LOCAL 1
#define SYMB_GLOBAL 2
#define SYMB_WEAK 3

/* extra */
#define SYMX_SPECIAL 0x80000000 /* Bit 31 = target-specific lnk. symbol */


struct SymNames {
  struct SymNames *next;        /* next symbol name in hash chain */
  const char *name;             /* symbol's name */
  lword value;                  /* optional value */
};


struct StabDebug {
  struct node n;
  struct Section *relsect;      /* symbol def. relative to this section */
  union {
    char *ptr;                  /* NULL-pointer reflects an n_strx of 0 */
    long idx;
  } name;
  uint8_t n_type;               /* stab type */
  int8_t n_othr;
  int16_t n_desc;
  uint32_t n_value;
};


struct MemoryDescr {
  struct MemoryDescr *next;
  const char *name;
  lword org;
  lword len;
  lword current;
  lword id;
};

/* default memory region parameters */
#define MEM_DEFORG (0LL)
#define MEM_DEFLEN (0x7fffffffffffffffLL)
#define MEM_NOID (~0LL)


/* overwrite attributes of an input section */
struct SecAttrOvr {
  struct SecAttrOvr *next;
  uint32_t flags;
  uint32_t memflags;
  char name[1];
};

#define SAO_MEMFLAGS 0x00000001 /* overwrite input section's memory flags */


struct LinkedSection {          /* linked sections of same type and name */
  struct node n;
  int index;                    /* section index 0..gv->nsecs */
  const char *name;             /* section's name, e.g. .text, .data, ... */
  unsigned long hash;           /* section name's hash code */
  uint8_t type;                 /* type: code, data, bss */
  uint8_t flags;
  uint8_t protection;           /* readable, writable, executable, ... */
  uint8_t alignment;            /* number of bits, which have to be zero */
  uint32_t memattr;             /* target-specific memory attributes */
  uint16_t ld_flags;            /* linker-flags for this section */
  uint16_t reserved;
  struct MemoryDescr *destmem;  /* destination memory block (ld-scripts) */
  struct MemoryDescr *relocmem; /* relocation memory block (ld-scripts) */
  lword copybase;               /* section's virtual copy address (destmem) */
  lword base;                   /* the section's virtual address (relocmem) */
  unsigned long size;           /* the section's size in bytes */
  unsigned long filesize;       /* size in file, rest is filled with '0' */
  unsigned long gapsize;        /* bytes to fill until next section */
  unsigned long banksize;       /* nonzero: sections must not cross banks */
  unsigned long bankoffs;       /* to skip on next bank alignment */
  struct list sections;         /* s. which have been linked together */
  uint8_t *data;                /* the section's contents */
  struct list relocs;           /* relocations for this section */
  struct list xrefs;            /* external references to unknown symbols */
  struct list symbols;          /* the section's symbol definitions */
};

/* linking flags (ld_flags) */
#define LSF_USED           0x01 /* section used in linker script */
#define LSF_NOLOAD         0x02 /* used on empty LinkedSection (ldscript) */
#define LSF_PRESERVE       0x04 /* don't delete when unused/empty */
#define LSF_NOXREFS        0x08 /* no xrefs allowed to this sect. (overlays) */


struct Phdr {
  struct Phdr *next;
  const char *name;
  struct MemoryDescr *vmregion; /* assigned memory regions */
  struct MemoryDescr *lmregion;
  uint32_t type;                /* PT_LOAD, PT_DYNAMIC, ... */
  uint16_t flags;               /* PF_X/W/R and header-flags, see below */
  uint8_t alignment;            /* number of bits, which have to be zero */
  uint8_t reserved;
  lword start_vma;              /* start VMA of segment */
  lword start;                  /* overwrites LMA from sections */
  lword file_end;               /* end LMA in file (initialized) */
  lword mem_end;                /* end LMA in memory (will be cleared) */
  unsigned long offset;         /* file offset */
  unsigned long alignment_gap;  /* gap at start of seg. for page alignm. */
};

#define PHDR_CLOSED     0x8000  /* segment can't receive any more sections */
#define PHDR_FILEHDR    0x0800  /* FILEHDR included in segment */
#define PHDR_PHDRS      0x0400  /* PHDRS included in segment */
#define PHDR_ADDR       0x0080  /* load address for segment (LMA) given */
#define PHDR_FLAGS      0x0040  /* flags given */
#define PHDR_USED       0x0010  /* this program header is used for output */
#define PHDR_PFMASK     0x000f  /* mask for seg. permissions "PF_?" */


/* Global defines */
#define DEF_MAXERRORS 999999    /* don't want this feature now... */
#define STRIP_NONE 0
#define STRIP_DEBUG 1           /* strip debugger symbols only */
#define STRIP_ALL 2             /* strip all symbols */
#define DISLOC_NONE 0
#define DISLOC_TMP 1            /* discard temporary local symbols */
#define DISLOC_ALL 2            /* discard all local symbols */


struct GlobalVars {
  /* options */
  struct list libpaths;         /* paths where to search for libraries */
  struct list inputlist;        /* list of input files */
  struct Flavours flavours;     /* library flavours */
  const char *dest_name;        /* output (executable) file name */
  const char *osec_base_name;   /* base name when outputting sections */
  uint8_t dest_format;          /* output file format */
  bool dest_object;             /* output file is a relocatable object */
  bool dest_sharedobj;          /* output as shared object */
  bool dyn_exp_all;             /* export all globals as dynamic symbols */
  bool keep_relocs;             /* keep relocations in final executable */
  bool alloc_common;            /* force allocation of common symbols */
  bool alloc_addr;              /* force allocation of address symbols */
  bool whole_archive;           /* always link with whole archives */
  uint8_t strip_symbols;        /* strip symbols */
  uint8_t discard_local;        /* discard local symbols */
  uint8_t reloctab_format;      /* format of relocation table (.rel/.rela) */
  bool small_code;              /* combine all code sections */
  bool small_data;              /* combine all data sections */
  bool multibase;               /* don't merge all base-rel. accessed sect.*/
  bool no_page_align;           /* page-alignment disabled */
  bool fix_unnamed;             /* unnamed section get a default name */
  bool textbaserel;             /* allow base-relative access on code secs. */
  bool output_sections;         /* output each section as a new file */
  bool output_le;               /* output a tbyte in little-endian order */
  bool auto_merge;              /* merge sections with pc-rel. references */
  bool merge_same_type;         /* merge all sections of same type */
  bool merge_same_attr;         /* merge all sections with same attributes */
  bool merge_all;               /* merge everything into a single section */
  uint8_t gc_sects;             /* garbage-collect unreferenced sections */
  bool keep_trailing_zeros;     /* keep trailing zero-bytes at end of sect. */
  bool keep_sect_order;         /* keep order of section as found in objs */
  char masked_symbols;          /* symbols may use a feature-mask */
  bool fail_on_warning;         /* return with error code from warnings */
  uint8_t bits_per_tbyte;       /* bits per target byte (word) */
  char reserved[3];
  size_t octets_per_tbyte;      /* host-bytes (8-bit) per target byte */
  uint8_t bits_per_taddr;       /* bits in target address (taddr, lword) */
  uint8_t tbytes_per_taddr;     /* target bytes in a target address word */
  uint8_t min_alignment;        /* minimal section alignment (default 0) */
  uint8_t ptr_alignment;        /* minimum alignment for pointers */
  FILE *map_file;               /* map file */
  FILE *trace_file;             /* linker trace output */
  FILE *sym_file;               /* sym/val file following a given format */
  char *sym_file_format;        /* format string for symbol files */
  unsigned sym_file_flags;      /* flags to control symbol file output */
  struct SymNames **trace_syms; /* trace-symbol hash table */
  struct SymNames *prot_syms;   /* list of protected symbols */
  struct SymNames *undef_syms;  /* list of undefined symbols */
  struct SymNames *lnk_syms;    /* list of command line linker symbols */
  struct SecAttrOvr *secattrovrs; /* input section attribute overwrites */
  struct SecRename *secrenames; /* input section renaming */
  const char *scriptname;
  const char *ldscript;         /* linker-script to be used for output file */
  const char *entry_name;       /* entry point symbol or addr (-e option) */
  lword start_addr;             /* -Ttext sets base address of first sect. */
  const char *soname;           /* real name of shared object (-soname) */
  const char *interp_path;      /* path to program interpreter (ELF) */
  struct list rpaths;           /* library paths for dynamic linker (ELF) */
  const char *lineoffsfile;     /* optional source line/offsets output */

  /* errors */
  bool dontwarn;                /* suppress warnings */
  bool errflag;                 /* if true, don't create output file */
  int maxerrors;                /* # of errors to display, before aborting */
  int errcnt;                   /* number of errors displayed */
  int warncnt;                  /* number of warnings displayed */
  int returncode;               /* return code for exit() */

  /* linking process */
  struct list linkfiles;        /* list of all link files (obj., libs,...)*/
  struct list selobjects;       /* list of included object units */
  struct list libobjects;       /* list of non-included library-objects */
  struct list sharedobjects;    /* list of shared objects */
  struct Symbol **symbols;      /* global symbol hash table */
  struct Symbol **lnksyms;      /* target-specific linker symbols hash tab */
  struct SymbolMask **symmasks; /* hash table of ORed symbol masks */
  struct list scriptsymbols;    /* symbols defined by linker script */
  struct list pripointers;      /* list of PriPointer nodes */
  struct list lnksec;           /* list of linked sections */
  int nsecs;                    /* total number of sections in lnksec */
  struct ObjectUnit *dynobj;    /* artif. object for dynamic sections */
  struct LinkedSection *firstSD;/* first small data section */
  struct Section *dummysec;     /* contains nothing, has base addr. 0 */
  struct Phdr *phdrlist;        /* list of defined PHDRs (ELF only) */
  uint16_t filldata;            /* used to fill alignment-gaps */
  int8_t endianness;             /* linking in big endian mode (1), */
                                /*  little endian (0), undefined (-1) */
  uint8_t collect_ctors_type;   /* method to collect constructors */
  const char *collect_ctors_secname; /* default section name for construct. */
  struct Symbol *ctor_symbol;   /* constructor list label: __CTOR_LIST__ */
  struct Symbol *dtor_symbol;   /* destructor list label: __DTOR_LIST__ */
  const char *common_sec_name;  /* section name for common symbols */
  unsigned long common_sec_hash;
  const char *scommon_sec_name; /* section name for small data com.symbols */
  unsigned long scommon_sec_hash;
  const char *got_base_name;    /* GOT label: _GLOBAL_OFFSET_TABLE_ */
  const char *plt_base_name;    /* PLT label: _PROCEDURE_LINKAGE_TABLE_ */
  bool pcrel_ctors;             /* write pc-relative con-/destructors */
  bool dynamic;                 /* dynamic linking - requires interpreter */
  bool use_ldscript;            /* true means there are LinkedSections, */
                                /*  generated by a linker-script */
};

#define SYMHTABSIZE 0x10000     /* number of entries in symbol hash table */
#define TRSYMHTABSIZE 0x40
#define DEFAULT_INTERP_PATH "/usr/lib/ld.so.1"

/* endianness */
#define _LITTLE_ENDIAN_ (0)
#define _BIG_ENDIAN_ (1)

/* gc_sects */
#define GCS_NONE        0       /* no section garbage-collection */
#define GCS_EMPTY       1       /* delete empty unreferenced sections */
#define GCS_ALL         2       /* delete all unreferenced sections */

/* reloctab_format */
#define RTAB_UNDEF      0x00    /* format not preset by user */
#define RTAB_STANDARD   0x01    /* standard, addends in code */
#define RTAB_ADDEND     0x02    /* table includes addends */
#define RTAB_SHORTOFF   0x04    /* table uses short offsets (e.g. 16 bit) */

/* symbol file flags */
#define SFF_VALFIRST  1         /* value left, name right */
#define SFF_VALSECOND 2         /* name left, value right */
#define SFF_NOLOCAL   4         /* skip local symbols */

/* collect_ctors_type */
#define CCDT_NONE     0         /* don't look for constructors/destructors */
#define CCDT_GNU      1         /* GNU-style: ??? */
#define CCDT_VBCC     2         /* VBCC-style: __INIT[_<pri=0-9>]_<name>()
                                               __EXIT[_<pri=0-9>]_<name>() */
#define CCDT_VBCC_ELF 3         /* VBCC-style: _INIT[_<pri=0-9>]_<name>()
                                               _EXIT[_<pri=0-9>]_<name>() */
#define CCDT_SASC     4         /* SAS/C-style: __STI[_<pri>]_<name>()
                                                __STD[_<pri>]_<name>() */

/* pattern structure to match file and section names from linker scripts */
struct Patterns {
  char *fmatch;                 /* file name match pattern */
  char **fexclude;              /* file name exclusion patterns */
  unsigned fsort;               /* file name sorting mode */
  char **smatch;                /* section name match patterns */
  char **sexclude;              /* section name exclusion patterns */
  unsigned ssortlev;            /* current level of section sorting */
  unsigned ssort[2];            /* levels of section sorting mode */
  unsigned flags;
};

#define PSORT_MAXLEV    2       /* maximum of two sorting levels */

/* sort modes */
#define PSORT_NONE      0       /* disable sorting */
#define PSORT_NAME      1       /* sort sections by name */
#define PSORT_ALIGN     2       /* sort sections by alignment */
#define PSORT_SIZE      3       /* sort sections by size */
#define PSORT_MODE      0x3f    /* mask sort mode */
#define PSORT_REV       0x40    /* reverse sorting flag */

/* flags */
#define PFL_KEEP        1       /* keep these sections in output */


typedef union {                 /* argument type for dynentry() */
  const char *name;
  struct Symbol *sym;
  struct Reloc *rel;
} DynArg;

struct FFFuncs {                /* file format specific functions and data */
  const char *tname;            /* name of file format */
  const char *exeldscript;      /* default linker-script for executables */
  const char *soldscript;       /* default linker-script for shared objects */
  void                          /* optional init function for the target */
    (*init)(struct GlobalVars *,int);
  int                           /* file format specific options */
    (*options)(struct GlobalVars *,int,const char **,int *);
  void                          /* print help for specific options */
    (*printhelp)(void);
  unsigned long                 /* size of header before first section */
    (*headersize)(struct GlobalVars *);
  int                           /* format identification */
    (*identify)(struct GlobalVars *,char *,uint8_t *,unsigned long,bool);
  void                          /* read file and convert into internal fmt. */
    (*readconv)(struct GlobalVars *,struct LinkFile *);
  uint8_t                       /* compare target-specific section flags */
    (*cmpsecflags)(struct LinkedSection *,struct Section *);
  int                           /* chk. if target requires linking of sect.*/
    (*targetlink)(struct GlobalVars *,struct LinkedSection *,struct Section *);
  struct Symbol *               /* optional target-specific find-symbol */
    (*fndsymbol)(struct GlobalVars *,struct Section *,const char *name,uint32_t);
  struct Symbol *               /* resolve linker-symbol reference */
    (*lnksymbol)(struct GlobalVars *,struct Section *,struct Reloc *);
  void                          /* init sym structure during resolve_xref() */
    (*setlnksym)(struct GlobalVars *,struct Symbol *);
  void                          /* prepare dynamic linking structures */
    (*dyninit)(struct GlobalVars *);
  struct Symbol *               /* make entry into a dynamic linking section */
    (*dynentry)(struct GlobalVars *,DynArg,int);
  void /* create dynamic linking structures from collected information */
    (*dyncreate)(struct GlobalVars *);
  void                          /* write object file */
    (*writeobject)(struct GlobalVars *,FILE *);
  void                          /* write shared object file */
    (*writeshared)(struct GlobalVars *,FILE *);
  void                          /* write executable file */
    (*writeexec)(struct GlobalVars *,FILE *);
  const char *bssname;          /* default name for bss section */
  const char *sbssname;         /* default name for small-data-bss section */
  uint32_t page_size;           /* page size for exec section alignment */
  int32_t baseoff;              /* sec. offset in bytes for base registers */
  int32_t gotoff;               /* offset in .got for _GLOBAL_OFFSET_TABLE_ */
  uint32_t id;                  /* general purpose id (e.g. MID for a.out) */
  uint8_t rtab_format;          /* reloc-table format (GV.reloctab_format) */
  uint8_t rtab_mask;            /* mask of allowed reloc-table formats */
  int8_t endianness;            /* 1=bigEndian, 0=littleEndian */
  int8_t addr_bits;             /* bits in a target address (16, 32, 64) */
  uint8_t ptr_alignment;        /* minimum alignment for pointers */
  uint32_t flags;               /* general and target-family specific flags */
};

/* Init modes */
#define FFINI_STARTUP 0         /* all targets on early startp */
#define FFINI_DESTFMT 1         /* init dest.target in linker_init() only */
#define FFINI_RESOLVE 2         /* dest.target at the end of linker_resolve() */
#define FFINI_MERGE 3           /* dest.target before merging sections */
/* Return codes from identify() */
#define ID_IGNORE (-1)          /* ignore file - e.g. an empty archive */
#define ID_UNKNOWN 0            /* unknown file format */
#define ID_OBJECT 1             /* object file in current format */
#define ID_EXECUTABLE 2         /* executable file in current format */
#define ID_ARTIFICIAL 3         /* an art. object, created by the linker */
/* ^ objects and executables must have a smaller id-value! */
#define ID_LIBBASE 4            /* all ids >= 4 are libraries (s.o. or .a) */
#define ID_SHAREDBASE 4
#define ID_SHAREDOBJ 4          /* shared object file in current format */
#define ID_ARCHBASE 8
#define ID_LIBARCH 8            /* linker library archive in curr. format */
/* number of entries in linker symbol hash table */
#define LNKSYMHTABSIZE 0x10

/* types for dynentry() */
#define GOT_ENTRY 1             /* allocate new .got entry */
#define PLT_ENTRY 2             /* allocate a new jump-slot in .plt/.got */
#define BSS_ENTRY 3             /* allocate space in .bss for R_COPY */
#define GOT_LOCAL 4             /* .got entry for local symbol */
#define PLT_LOCAL 5             /* .plt entry for local (does not exist) */
#define STR_ENTRY 6             /* allocate a new string into .dynstr */
#define SYM_ENTRY 7             /* allocate a new symbol into .dynsym */
#define SO_NEEDED 8             /* new shared object is needed */

/* Target flags - bits 24 to 31 are reserved for target-family flags */
#define FFF_BASEINCR 1          /* Section base address is not reset to 0 */
                                /* for the following sections while */
                                /* linking without a linker script */
#define FFF_RELOCATABLE 2       /* Target generates relocatable executables */
                                /* - also defines whether the distance */
                                /* between sections is not constant */
#define FFF_PSEUDO_DYNLINK 4    /* Dynamic linking is not really done, only */
                                /* symbol references are checked whether they */
                                /* can be resolved by the shared object */
#define FFF_DYN_RESOLVE_ALL 8   /* All dynamic symbol references have to */
                                /* be resolved at link-time, even the */
                                /* inter-DLL ones. */
#define FFF_SECTOUT 0x10        /* Target allows to create a new file for */
                                /* each section. */
#define FFF_NOFILE 0x20         /* Target creates output files itself */
#define FFF_KEEPRELOCS 0x40     /* Binary target allows reloc table appended */
#define FFF_OUTWORDADDR 0x80    /* Output supports bits_per_tbyte != 8 */


/* List of artificially generated pointers or long words, which are */
/* sorted by section-name, list-name and priority. */
struct PriPointer {
  struct node n;
  const char *secname;
  const char *listname;
  int priority;
  const char *xrefname;
  lword addend;
};


/* global functions */
#include "ar.h"

/* main.c */
#ifndef MAIN_C
extern struct GlobalVars gvars;
#endif
const char *get_arg(int,const char **,int *);
lword get_assign_arg(int,const char **,int *,char *,size_t);
void cleanup(struct GlobalVars *);

/* version.c */
#ifndef VERSION_C
extern const char *version_str;
#endif
void show_version(void);
void show_usage(struct GlobalVars *,int);

/* support.c */
#ifndef SUPPORT_C
extern const char *endian_name[];
#endif
void *alloc(size_t);
void *re_alloc(void *,size_t);
void *alloczero(size_t);
char *allocstring(const char *);
void *alloc_hashtable(size_t);
void initlist(struct list *);
void insertbefore(struct node *,struct node *);
void insertbehind(struct node *,struct node *);
void addhead(struct list *,struct node *);
void addtail(struct list *,struct node *);
struct node *remhead(struct list *);
struct node *remnode(struct node *);
int stricmp(const char *,const char *);
char *mapfile(const char *);
const char *base_name(const char *);
char *check_name(char *);
bool checkrange(lword,int,int);
int8_t host_endianness(void);
uint16_t swap16(uint16_t);
uint32_t swap32(uint32_t);
uint64_t swap64(uint64_t);
uint16_t read16be(void *);
uint32_t read32be(void *);
uint64_t read64be(void *);
void write16be(void *,uint16_t);
void write32be(void *,uint32_t);
void write64be(void *,uint64_t);
uint16_t read16le(void *);
uint32_t read32le(void *);
uint64_t read64le(void *);
void write16le(void *,uint16_t);
void write32le(void *,uint32_t);
void write64le(void *,uint64_t);
uint16_t read16(bool,void *);
uint32_t read32(bool,void *);
uint64_t read64(bool,void *);
void write16(bool,void *,uint16_t);
void write32(bool,void *,uint32_t);
void write64(bool,void *,uint64_t);
lword readbf(bool,void *,int,int,int);
void writebf(bool,void *,int,int,int,lword);
lword readtbyte(struct GlobalVars *,void *);
void writetbyte(struct GlobalVars *,void *,lword);
void writetbytes(struct GlobalVars *,int,void *,lword);
lword readreloc(struct GlobalVars *,void *,int,int);
void writereloc(struct GlobalVars *,void *,int,int,lword);
void fwritex(FILE *,const void *,size_t);
void fwrite32be(FILE *,uint32_t);
void fwrite16be(FILE *,uint16_t);
void fwrite32le(FILE *,uint32_t);
void fwrite16le(FILE *,uint16_t);
void fwrite16(int,FILE *,uint16_t);
void fwrite32(int,FILE *,uint32_t);
void fwrite8(FILE *,uint8_t);
void fwrite8(FILE *,uint8_t);
void fwritetbyte(struct GlobalVars *,FILE *,lword);
void fwritetaddr(struct GlobalVars *,FILE *,lword);
void fwrite_align(struct GlobalVars *,FILE *,uint32_t,uint32_t);
void fwritegap(struct GlobalVars *,FILE *,long,lword);
void fwriterawsect(struct GlobalVars *,FILE *,struct LinkedSection *);
void fwritefullsect(struct GlobalVars *,FILE *,struct LinkedSection *);
unsigned long elf_hash(const char *);
unsigned long align(lword,unsigned long);
unsigned long comalign(lword,lword);
int shiftcnt(uint32_t);
int lshiftcnt(lword);
int highest_bit_set(lword);
lword sign_extend(lword,int);
void add_symnames(struct SymNames **,const char *,lword);

#define listempty(x) ((x)->first->next==NULL)
#define makemask(x) ((lword)(1LL<<(x))-1)
#define mtaddr(gv,x) (unsigned long long)((x)&((lword)(1LL<<(gv)->bits_per_taddr)-1))
#define abstaddr(x) (unsigned long long)((x)<0?-(x):(x))
#define sgnchar(x) ((x)<0?'-':'+')
#define optsgnstr(x) ((x)<0?"-":"")

/* errors.c */
void disable_warning(int);
void error(int,...);
void ierror(char *,...);

/* linker.c */
void linker_init(struct GlobalVars *);
void linker_load(struct GlobalVars *);
void linker_resolve(struct GlobalVars *);
void linker_relrefs(struct GlobalVars *);
void linker_dynprep(struct GlobalVars *);
void linker_sectrefs(struct GlobalVars *);
void linker_gcsects(struct GlobalVars *);
void linker_merge(struct GlobalVars *);
void linker_delunused(struct GlobalVars *);
void linker_mapfile(struct GlobalVars *);
void linker_copy(struct GlobalVars *);
void linker_relocate(struct GlobalVars *);
void linker_write(struct GlobalVars *);
void linker_cleanup(struct GlobalVars *);
const char *getobjname(struct ObjectUnit *);
void print_function_name(struct Section *,unsigned long);
void print_symbol(struct GlobalVars *,FILE *,struct Symbol *);
bool trace_sym_access(struct GlobalVars *,const char *);

/* targets.c */
#ifndef TARGETS_C
extern struct FFFuncs *fff[];
extern const char *sym_type[];
extern const char *sym_info[];
extern const char *sym_bind[];
extern const char *reloc_name[];
extern const char text_name[];
extern const char data_name[];
extern const char bss_name[];
extern const char sdata_name[];
extern const char sbss_name[];
extern const char sdata2_name[];
extern const char sbss2_name[];
extern const char ctors_name[];
extern const char dtors_name[];
extern const char got_name[];
extern const char plt_name[];
extern const char zero_name[];
extern const char sdabase_name[];
extern const char sda2base_name[];
extern const char gotbase_name[];
extern const char pltbase_name[];
extern const char dynamic_name[];
extern const char r13init_name[];
extern const char noname[];
#endif
#define tbytes(gv,n) ((size_t)(n) * gv->octets_per_tbyte)
void section_fill(struct GlobalVars *,uint8_t *,size_t,uint16_t,lword);
void section_copy(struct GlobalVars *,uint8_t *,size_t,uint8_t *,size_t);
bool check_protection(struct GlobalVars *,const char *);
struct Symbol *findsymbol(struct GlobalVars *,struct Section *,
                          const char *,uint32_t);
void hide_shlib_symbols(struct GlobalVars *);
struct Symbol *newsymbol(const char *,lword,uint8_t,uint8_t,uint8_t,uint8_t,
                         uint32_t);
struct Symbol *addsymbol(struct GlobalVars *,struct Section *,
                         const char *,const char *,lword,
                         uint8_t,uint8_t,uint8_t,uint8_t,uint32_t,bool);
struct Symbol *findlocsymbol(struct GlobalVars *,struct ObjectUnit *,
                             const char *);
void addlocsymbol(struct GlobalVars *,struct Section *,char *,char *,
                  lword,uint8_t,uint8_t,uint8_t,uint32_t);
bool addglobsym(struct GlobalVars *,struct Symbol *);
struct Symbol *addlnksymbol(struct GlobalVars *,const char *,lword,
                            uint8_t,uint8_t,uint8_t,uint8_t,uint32_t);
struct Symbol *findlnksymbol(struct GlobalVars *,const char *);
void fixlnksymbols(struct GlobalVars *,struct LinkedSection *);
struct Symbol *find_any_symbol(struct GlobalVars *,
                               struct Section *,const char *);
void pull_objunit(struct GlobalVars *,struct ObjectUnit *);
struct Section *getinpsecoffs(struct LinkedSection *,unsigned long,
                              unsigned long *);
struct RelocInsert *initRelocInsert(struct RelocInsert *,
                                    uint16_t,uint16_t,lword);
struct Reloc *newreloc(struct GlobalVars *,struct Section *,
                       const char *,struct Section *,uint32_t,
                       unsigned long,int,lword);
void addreloc(struct Section *,struct Reloc *,uint16_t,uint16_t,lword);
void addreloc_ri(struct Section *,struct Reloc *,struct RelocInsert *);
bool isstdreloc(struct Reloc *,uint8_t,uint16_t);
struct Reloc *findreloc(struct Section *,unsigned long);
void addstabs(struct ObjectUnit *,struct Section *,char *,
              uint8_t,int8_t,int16_t,uint32_t);
void fixstabs(struct ObjectUnit *);
struct SourceLines *newsourcelines(struct Section *,const char *);
void allocsrclinetab(struct SourceLines *,size_t);
lword readsection(struct GlobalVars *,uint8_t,uint8_t *,size_t,
                  struct RelocInsert *);
lword writesection(struct GlobalVars *,uint8_t *,size_t,struct Reloc *,lword);
int writetaddr(struct GlobalVars *,void *,size_t,lword);
void calc_relocs(struct GlobalVars *,struct LinkedSection *);
void sort_relocs(struct list *);
struct Section *create_section(struct ObjectUnit *,const char *,
                               uint8_t *,unsigned long);
struct Section *add_section(struct ObjectUnit *,const char *,uint8_t *,
                            unsigned long,uint8_t,uint8_t,uint8_t,uint8_t,bool);
bool is_common_sec(struct GlobalVars *,struct Section *);
bool is_common_ls(struct GlobalVars *,struct LinkedSection *);
struct Section *common_section(struct GlobalVars *,struct ObjectUnit *);
struct Section *scommon_section(struct GlobalVars *,struct ObjectUnit *);
struct Section *abs_section(struct ObjectUnit *);
struct Section *dummy_section(struct GlobalVars *,struct ObjectUnit *);
struct LinkedSection *create_lnksect(struct GlobalVars *,const char *,
                                     uint8_t,uint8_t,uint8_t,uint8_t,uint32_t);
struct Section *find_sect_type(struct ObjectUnit *,uint8_t,uint8_t);
struct Section *find_sect_id(struct ObjectUnit *,uint32_t);
struct Section *find_sect_name(struct ObjectUnit *,const char *);
struct Section *find_first_bss_sec(struct LinkedSection *);
struct LinkedSection *find_lnksec(struct GlobalVars *,const char *,
                                  uint8_t,uint8_t,uint8_t,uint8_t);
struct LinkedSection *smalldata_section(struct GlobalVars *);
void add_objunit(struct GlobalVars *,struct ObjectUnit *,bool);
struct ObjectUnit *create_objunit(struct GlobalVars *,
                                  struct LinkFile *,const char *);
struct ObjectUnit *art_objunit(struct GlobalVars *,const char *,
                               uint8_t *,unsigned long);
void collect_constructors(struct GlobalVars *);
void add_priptrs(struct GlobalVars *,struct ObjectUnit *);
void make_constructors(struct GlobalVars *);
void get_text_data_bss(struct GlobalVars *,struct LinkedSection **);
void text_data_bss_gaps(struct LinkedSection **);
bool discard_symbol(struct GlobalVars *,struct Symbol *);
lword entry_address(struct GlobalVars *gv);
struct Section *entry_section(struct GlobalVars *);
struct Symbol *bss_entry(struct GlobalVars *,struct ObjectUnit *,
                         const char *,struct Symbol *);
struct SecAttrOvr *addsecattrovr(struct GlobalVars *,char *,uint32_t);
struct SecAttrOvr *getsecattrovr(struct GlobalVars *,const char *,uint32_t);
void addsecrename(const char *,const char *);
struct SecRename *getsecrename(void);
void trim_sections(struct GlobalVars *);
void untrim_sections(struct GlobalVars *,int);
struct LinkedSection *load_next_section(struct GlobalVars *);

/* dir.c */
char *path_append(char *,const char *,const char *,size_t);
char *open_dir(const char *);
char *read_dir(char *);
void close_dir(char *);
void set_exec(const char *);

/* ldscript.c */
bool is_ld_script(struct ObjectUnit *);
lword update_address(struct MemoryDescr *,struct MemoryDescr *,unsigned long);
lword align_address(struct MemoryDescr *,struct MemoryDescr *,unsigned long);
struct Section *next_pattern(struct GlobalVars *,struct Patterns *);
void free_patterns(struct Patterns *);
struct LinkedSection *next_secdef(struct GlobalVars *);
void init_secdef_parse(struct GlobalVars *);
void init_ld_script(struct GlobalVars *);
/* return value for valid file/section patterns from next_pattern() */
#define VALIDPAT (struct Section *)1

/* pmatch.c */
bool pattern_match(const char *,const char *);
bool patternlist_match(char **,const char *);

/* expr.c */
void skip(void);
char getchr(void);
bool testchr(char);
bool expectchr(char);
void skipblock(int,char,char);
void back(int);
char *gettxtptr(void);
char *getarg(uint8_t);
char *getquoted(void);
int parse_expr(lword,lword *);
int getlineno(void);
void init_parser(struct GlobalVars *,const char *,const char *,int);

/* tosopts.c */
#if defined(AOUT_MINT) || defined(ATARI_TOS)
#ifndef T_TOSOPTS_C
extern uint32_t tos_flags;
extern bool textbasedsyms;  /* symbol offsets based on text section */
extern bool sozobonx;
extern bool hisoftdri;
#endif
int tos_options(struct GlobalVars *,int,const char **,int *);
void tos_printhelp(void);
#endif

/* t_amigaos.c */
#ifndef T_AMIGAOS_C
#if defined(ADOS) || defined(EHF)
extern struct FFFuncs fff_amigahunk;
extern struct FFFuncs fff_ehf;
#endif
#endif

/* t_ataritos.c */
#ifndef T_ATARITOS_C
#ifdef ATARI_TOS
extern struct FFFuncs fff_ataritos;
#endif
#endif

/* t_xfile.c */
#ifndef T_XFILE_C
#ifdef XFILE
extern struct FFFuncs fff_xfile;
#endif
#endif

/* t_os9.c */
#ifndef T_OS9_C
#ifdef OS_9
extern struct FFFuncs fff_os9_6809;
#endif
#endif

/* t_o65.c */
#ifndef T_OS65_C
#ifdef O65
extern struct FFFuncs fff_o6502;
extern struct FFFuncs fff_o65816;
#endif
#endif

/* t_appleomf.c */
#ifndef T_APPLEOMF_C
#ifdef APPLE_OMF
extern struct FFFuncs fff_appleomf;
#endif
#endif

/* t_elf32ppcbe.c */
#ifndef T_ELF32PPCBE_C
#if defined(ELF32_PPC_BE)
extern struct FFFuncs fff_elf32ppcbe;
#endif
#if defined(ELF32_AMIGA)
extern struct FFFuncs fff_elf32powerup;
extern struct FFFuncs fff_elf32morphos;
extern struct FFFuncs fff_elf32amigaos;
#endif
#endif

/* t_elf32m68k.c */
#ifndef T_ELF32M68K_C
#ifdef ELF32_M68K
extern struct FFFuncs fff_elf32m68k;
#endif
#endif

/* t_elf32jag.c */
#ifndef T_ELF32JAG_C
#ifdef ELF32_JAG
extern struct FFFuncs fff_elf32jag;
#endif
#endif

/* t_elf32i386.c */
#ifndef T_ELF32I386_C
#ifdef ELF32_386
extern struct FFFuncs fff_elf32i386;
#endif
#ifdef ELF32_AROS
extern struct FFFuncs fff_elf32aros;
#endif
#endif

/* t_elf32arm.c */
#ifndef T_ELF32ARM_C
#ifdef ELF32_ARM_LE
extern struct FFFuncs fff_elf32armle;
#endif
#endif

/* t_elf64x86.c */
#ifndef T_ELF64X86_C
#ifdef ELF64_X86
extern struct FFFuncs fff_elf64x86;
#endif
#endif

/* t_aoutnull.c */
#ifndef T_AOUTNULL_C
#if defined(AOUT_NULL)
extern struct FFFuncs fff_aoutnull;
#endif
#endif

/* t_aoutm68k.c */
#ifndef T_AOUTM68K_C
#if defined(AOUT_SUN010)
extern struct FFFuncs fff_aoutsun010;
#endif
#if defined(AOUT_SUN020)
extern struct FFFuncs fff_aoutsun020;
#endif
#if defined(AOUT_BSDM68K)
extern struct FFFuncs fff_aoutbsd68k;
#endif
#if defined(AOUT_BSDM68K4K)
extern struct FFFuncs fff_aoutbsd68k4k;
#endif
#if defined(AOUT_JAGUAR)
extern struct FFFuncs fff_aoutjaguar;
#endif
#endif

/* t_aoutmint.c */
#ifndef T_AOUTMINT_C
#if defined(AOUT_MINT)
extern struct FFFuncs fff_aoutmint;
#endif
#endif

/* t_aouti386.c */
#ifndef T_AOUTI386_C
#if defined(AOUT_BSDI386)
extern struct FFFuncs fff_aoutbsdi386;
#endif
#if defined(AOUT_PC386)
extern struct FFFuncs fff_aoutpc386;
#endif
#endif

/* t_rawbin.c */
#ifndef T_RAWBIN_C
#if defined(RAWBIN)
extern struct FFFuncs fff_rawbin;
#endif
#if defined(AMSDOS)
extern struct FFFuncs fff_amsdos;
#endif
#if defined(APPLEBIN)
extern struct FFFuncs fff_applebin;
#endif
#if defined(ATARICOM)
extern struct FFFuncs fff_ataricom;
#endif
#if defined(BBC)
extern struct FFFuncs fff_bbc;
extern struct FFFuncs fff_bbc2;
#endif
#if defined(CBMPRG)
extern struct FFFuncs fff_cbmprg;
extern struct FFFuncs fff_cbmreu;
#endif
#if defined(COCOML)
extern struct FFFuncs fff_cocoml;
#endif
#if defined(DRAGONBIN)
extern struct FFFuncs fff_dragonbin;
#endif
#if defined(FOENIX)
extern struct FFFuncs fff_pgxc02;
extern struct FFFuncs fff_pgx816;
extern struct FFFuncs fff_pgz24;
extern struct FFFuncs fff_pgz32;
#endif
#if defined(ORICMC)
extern struct FFFuncs fff_oricmc;
#endif
#if defined(JAGSRV)
extern struct FFFuncs fff_jagsrv;
#endif
#if defined(SINCQL)
extern struct FFFuncs fff_sincql;
#endif
#if defined(SREC19)
extern struct FFFuncs fff_srec19;
#endif
#if defined(SREC28)
extern struct FFFuncs fff_srec28;
#endif
#if defined(SREC37)
extern struct FFFuncs fff_srec37;
#endif
#if defined(IHEX)
extern struct FFFuncs fff_ihex;
#endif
#if defined(SHEX1)
extern struct FFFuncs fff_shex1;
#endif
#endif

/* t_rawseg.c */
#ifndef T_RAWSEG_C
#if defined(RAWSEG)
extern struct FFFuncs fff_rawseg;
#endif
#endif

/* t_vobj.c */
#ifndef T_VOBJ_C
#if defined(VOBJ)
extern struct FFFuncs fff_vobj_le;
extern struct FFFuncs fff_vobj_be;
#endif
#endif

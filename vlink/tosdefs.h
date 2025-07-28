/* $VER: vlink tosdefs.h V0.18 (31.05.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2010,2024  Frank Wille
 */


#define TEXTNAME text_name
#define DATANAME data_name
#define BSSNAME  bss_name

#define TOS_ALIGNMENT 1  /* 16 bits */


/* TOS program header */
typedef struct
{
  uint8_t ph_branch[2]; /* branch to start of program (0x601a) */
  uint8_t ph_tlen[4];   /* .text length */
  uint8_t ph_dlen[4];   /* .data length */
  uint8_t ph_blen[4];   /* .bss length */
  uint8_t ph_slen[4];   /* length of symbol table */
  uint8_t ph_magic[4];
  uint8_t ph_flags[4];  /* Atari special flags */
  uint8_t ph_abs[2];    /* has to be 0, otherwise no relocation takes place */
} PH;

#define DRIMAGIC 0x601a /* magic code in ph_branch */


/* DRI symbol table */
#define DRI_NAMELEN 8

struct DRIsym
{
  char name[DRI_NAMELEN];
  uint8_t type[2];
  uint8_t value[4];
};

#define STYP_UNDEF 0
#define STYP_BSS 0x0100
#define STYP_TEXT 0x0200
#define STYP_DATA 0x0400
#define STYP_EXTERNAL 0x0800
#define STYP_REGISTER 0x1000
#define STYP_GLOBAL 0x2000
#define STYP_EQUATED 0x4000
#define STYP_DEFINED 0x8000
#define STYP_LONGNAME 0x0048
#define STYP_TFILE 0x0280
#define STYP_TFARC 0x02c0
#define STYP_XFLAGS 0x4200

#define XVALUE 0x87654321       /* SozobonX symbol value for extended name */
#define XNAME "SozobonX"        /* Name of first symbol for SozobonX */


/* DRI library arheader */
struct DRIarheader {
  char a_fname[14];
  uint8_t a_modtim[4];
  uint8_t a_userid;
  uint8_t a_gid;
  uint8_t a_fimode[2];
  uint8_t a_fsize[4];
  uint8_t a_reserved[2];
};

#define DRI_ARMAGIC 0xff65


/* DRI Header Info */
struct DRIinfo {
  struct ObjectUnit *object;
  struct FFFuncs *ff;
  PH *hdr;
  uint8_t *end;
  struct Section *sec[3];  /* text, data, bss */
  uint8_t *data[2];        /* text and data contents */
  struct DRIsym *symtab;
  int nsym;
  int sozobonx;            /* symtab may have SozobonX name extensions */
  int symidx;              /* next symbol index, after dri_symname() */
};


/* default script */
static const char defaultscript[] =
  "SECTIONS {\n"
  "  . = 0;\n"
  "  .text: {\n"
  "    *(.i* i* I*)\n"
  "    *(.t* t* T* .c* c* C*)\n"
  "    *(.f* f* F*)\n"
  "    PROVIDE(_etext = .);\n"
  "    PROVIDE(__etext = .);\n"
  "    . = ALIGN(2);\n"
  "  }\n"
  "  .data: {\n"
  "    PROVIDE(_LinkerDB = . + 0x8000);\n"
  "    PROVIDE(_SDA_BASE_ = . + 0x8000);\n"
  "    VBCC_CONSTRUCTORS\n"
  "    *(.rodata*)\n"
  "    *(.d* d* D*)\n"
  "    *(.sdata*)\n"
  "    *(__MERGED)\n"
  "    PROVIDE(_edata = .);\n"
  "    PROVIDE(__edata = .);\n"
  "    . = ALIGN(2);\n"
  "  }\n"
  "  .bss: {\n"
  "    *(.sbss*)\n"
  "    *(.scommon)\n"
  "    *(.b* b* B* .u* u* U*)\n"
  "    *(COMMON)\n"
  "    PROVIDE(_end = ALIGN(4));\n"
  "    PROVIDE(__end = ALIGN(4));\n"
  "  }\n"
  "}\n";


/* t_ataritos.c prototypes */
void tos_writerelocs(struct GlobalVars *,FILE *,struct LinkedSection **);

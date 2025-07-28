/* $VER: vlink t_rawbin.c V0.18 (23.12.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2024  Frank Wille
 */


#include "config.h"
#if defined(RAWBIN) || defined(IHEX) || defined(SHEX1) || \
    defined(SREC19) || defined(SREC28) || defined(SREC37) || \
    defined(AMSDOS) || defined(APPLEBIN) || defined(ATARICOM) || \
    defined(BBC) || defined(CBMPRG) || defined(COCOML) || \
    defined(DRAGONBIN) || defined(FOENIX) || defined(JAGSRV) || \
    defined(ORICMC) || defined(SINCQL)
#define T_RAWBIN_C
#include "vlink.h"

#define MAXGAP 16  /* from MAXGAP bytes on, a new file will be created */
#define MAXSREC 32 /* max. number of bytes in an S1/S2/S3 record */
#define MAXIREC 32 /* max. number of bytes in an ihex record (actually 255) */

#define QDOSHDR_SIZE 30  /* SinclairQL QDOS header */

/* header types */
enum {
  HDR_NONE,
  HDR_AMSDOS,
  HDR_APPLEBIN,
  HDR_ATARICOM,
  HDR_BBC,
  HDR_CBMPRG,
  HDR_COCOML,
  HDR_DRAGONBIN,
  HDR_JAGSRV,
  HDR_ORICMC,
  HDR_PGXC02,
  HDR_PGX816,
  HDR_PGZ24,
  HDR_PGZ32,
  HDR_QDOS,
  HDR_XTCC
};

/* binary relocs (-q) */
struct BinReloc {
  struct BinReloc *next;
  lword offset;
};

static unsigned long rawbin_headersize(struct GlobalVars *);
static int rawbin_identify(struct GlobalVars *,char *,uint8_t *,
                           unsigned long,bool);
static void rawbin_readconv(struct GlobalVars *,struct LinkFile *);
static int rawbin_targetlink(struct GlobalVars *,struct LinkedSection *,
                              struct Section *);
static void rawbin_writeobject(struct GlobalVars *,FILE *);
static void rawbin_writeshared(struct GlobalVars *,FILE *);
static void rawbin_out(struct GlobalVars *,FILE *,int);
#ifdef RAWBIN
static int rawbin_options(struct GlobalVars *,int,const char **,int *);
static void rawbin_printhelp(void);
static void rawbin_write(struct GlobalVars *,FILE *);
#endif
#ifdef AMSDOS
static unsigned long amsdos_headersize(struct GlobalVars *);
static void amsdos_write(struct GlobalVars *,FILE *);
#endif
#ifdef APPLEBIN
static unsigned long applebin_headersize(struct GlobalVars *);
static void applebin_write(struct GlobalVars *,FILE *);
#endif
#ifdef ATARICOM
static unsigned long ataricom_headersize(struct GlobalVars *);
static void ataricom_write(struct GlobalVars *,FILE *);
#endif
#ifdef BBC
static void bbc_write(struct GlobalVars *,FILE *);
static void bbc_write2(struct GlobalVars *,FILE *);
static FILE *bbcload;
#endif
#ifdef CBMPRG
static unsigned long cbmprg_headersize(struct GlobalVars *);
static void cbmprg_write(struct GlobalVars *,FILE *);
static void cbmreu_write(struct GlobalVars *,FILE *);
#endif
#ifdef COCOML
static unsigned long cocoml_headersize(struct GlobalVars *);
static void cocoml_write(struct GlobalVars *,FILE *);
#endif
#ifdef DRAGONBIN
static unsigned long dragonbin_headersize(struct GlobalVars *);
static void dragonbin_write(struct GlobalVars *,FILE *);
#endif
#ifdef FOENIX
static unsigned long pgx_headersize(struct GlobalVars *);
static void pgxc02_write(struct GlobalVars *,FILE *);
static void pgx816_write(struct GlobalVars *,FILE *);
static unsigned long pgz_headersize(struct GlobalVars *);
static void pgz24_write(struct GlobalVars *,FILE *);
static void pgz32_write(struct GlobalVars *,FILE *);
#endif
#ifdef JAGSRV
#define TOSHDR_SIZE 28
#define JAGR_SIZE 18
static unsigned long jagsrv_headersize(struct GlobalVars *);
static void jagsrv_write(struct GlobalVars *,FILE *);
#endif
#ifdef ORICMC
static int oric_options(struct GlobalVars *,int,const char **,int *);
static void oric_printhelp(void);
static unsigned long oric_headersize(struct GlobalVars *);
static void oric_write(struct GlobalVars *,FILE *);
#endif
#ifdef SINCQL
static int sincql_options(struct GlobalVars *,int,const char **,int *);
static void sincql_printhelp(void);
static unsigned long sincql_headersize(struct GlobalVars *);
static void sincql_write(struct GlobalVars *,FILE *);
#endif
#ifdef SREC19
static void srec19_write(struct GlobalVars *,FILE *);
#endif
#ifdef SREC28
static void srec28_write(struct GlobalVars *,FILE *);
#endif
#ifdef SREC37
static void srec37_write(struct GlobalVars *,FILE *);
#endif
#ifdef IHEX
static void ihex_write(struct GlobalVars *,FILE *);
#endif
#ifdef SHEX1
static void shex1_write(struct GlobalVars *,FILE *);
#endif

static lword execaddr;
static FILE *hdrfile;
static long hdroffs;
static lword relocsize;
static struct LinkedSection *hdrsect;

static bool multifile;      /* used by rawbin */
static bool coalesced;      /* used by rawbin */
static lword fillval;       /* used by rawbin */
static bool autoexec;       /* used by oricmc */
static lword stacksize;     /* used by sinclairql */
static lword udatasize;     /* used by sinclairql */

static const char defaultscript[] =
  "SECTIONS {\n"
  "  .text: { *(.text CODE text) *(seg*) *(.rodata*) }\n"
  "  .data: { *(.data DATA data) PROVIDE(__edata=.); }\n"
  "  .bss: { *(.bss BSS bss) *(COMMON) PROVIDE(__end=.); }\n"
  "  .comment 0 : { *(.comment) }\n"
  "  PROVIDE(__bsslen=SIZEOF(.bss));\n"
  "}\n";


#ifdef RAWBIN
struct FFFuncs fff_rawbin = {
  "rawbin",
  defaultscript,
  NULL,
  NULL,
  rawbin_options,
  rawbin_printhelp,
  rawbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  rawbin_write,
  NULL,NULL,
  0,
  0x8000,
  0,
  0,
  RTAB_UNDEF,0,
  -1, /* endianness undefined, only write */
  0,  /* addr_bits from input */
  0,  /* ptr-alignment unknown */
  FFF_SECTOUT|FFF_KEEPRELOCS|FFF_OUTWORDADDR
};
#endif

#ifdef AMSDOS
static const char amsdosscript[] =
  "MEMORY {\n"
  "  m: org=., len=0x10000-.\n"
  "  b1: org=0x4000, len=0x4000\n"
  "  b2: org=0x4000, len=0x4000\n"
  "  b3: org=0x4000, len=0x4000\n"
  "  b4: org=0x4000, len=0x4000\n"
  "}\n"
  "SECTIONS {\n"
  "  bin: { *(.text) *(.rodata*) *(.data*) *(.bss) *(COMMON) } > m\n"
  "  c4 : { *(.c4) } > b1 AT> m\n"
  "  c5 : { *(.c5) } > b2 AT> m\n"
  "  c6 : { *(.c6) } > b3 AT> m\n"
  "  c7 : { *(.c7) } > b4 AT> m\n"
  "}\n";

struct FFFuncs fff_amsdos = {
  "amsdos",
  amsdosscript,
  NULL,
  NULL,
  NULL,
  NULL,
  amsdos_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  amsdos_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _LITTLE_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
#endif

#ifdef APPLEBIN
struct FFFuncs fff_applebin = {
  "applebin",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  applebin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  applebin_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _LITTLE_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
#endif

#ifdef ATARICOM
struct FFFuncs fff_ataricom = {
  "ataricom",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  ataricom_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  ataricom_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _LITTLE_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
#endif

#ifdef BBC
struct FFFuncs fff_bbc = {
  "bbc",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  rawbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  bbc_write,
  NULL,NULL,
  0,
  0x8000,
  0,
  0,
  RTAB_UNDEF,0,
  _LITTLE_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
struct FFFuncs fff_bbc2 = {
  "bbc2",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  rawbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  bbc_write2,
  NULL,NULL,
  0,
  0x8000,
  0,
  0,
  RTAB_UNDEF,0,
  _LITTLE_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
#endif

#ifdef CBMPRG
struct FFFuncs fff_cbmprg = {
  "cbmprg",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  cbmprg_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  cbmprg_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _LITTLE_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
struct FFFuncs fff_cbmreu = {
  "cbmreu",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  cbmprg_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  cbmreu_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _LITTLE_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
#endif

#ifdef COCOML
struct FFFuncs fff_cocoml = {
  "cocoml",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  cocoml_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  cocoml_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _BIG_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
#endif

#ifdef DRAGONBIN
struct FFFuncs fff_dragonbin = {
  "dragonbin",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  dragonbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  dragonbin_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _BIG_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
#endif

#ifdef FOENIX
struct FFFuncs fff_pgxc02 = {
  "foenixpgx-c02",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  pgx_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  pgxc02_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _LITTLE_ENDIAN_,
  16,0,
  FFF_SECTOUT
};

struct FFFuncs fff_pgx816 = {
  "foenixpgx-816",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  pgx_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  pgx816_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _LITTLE_ENDIAN_,
  24,0,
  FFF_SECTOUT
};

struct FFFuncs fff_pgz24 = {
  "foenixpgz-24",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  pgz_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  pgz24_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  -1,0,0, /* undefined, only write */
  FFF_SECTOUT
};

struct FFFuncs fff_pgz32 = {
  "foenixpgz-32",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  pgz_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  pgz32_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  -1,0,0, /* undefined, only write */
  FFF_SECTOUT
};
#endif

#ifdef JAGSRV
static const char jaguarscript[] =
  "SECTIONS {\n"
  "  . = 0x4000;\n"
  "  .text: {\n"
  "    *(.text CODE text)\n"
  "  }\n"
  "  . = ALIGN(32);\n"
  "  .data: {\n"
  "    VBCC_CONSTRUCTORS\n"
  "    *(.data DATA data)\n"
  "  }\n"
  "  . = ALIGN(32);\n"
  "  .bss: {\n"
  "    *(.bss BSS bss)\n"
  "    *(COMMON)\n"
  "    _BSS_END = ALIGN(32);\n"
  "  }\n"
  "}\n";

struct FFFuncs fff_jagsrv = {
  "jagsrv",
  jaguarscript,
  NULL,
  NULL,
  NULL,
  NULL,
  jagsrv_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  jagsrv_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _BIG_ENDIAN_,
  32,1,
  0
};
#endif

#ifdef ORICMC
struct FFFuncs fff_oricmc = {
  "oricmc",
  defaultscript,
  NULL,
  NULL,
  oric_options,
  oric_printhelp,
  oric_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  oric_write,
  NULL,NULL,
  0,
  0,
  0,
  0,
  RTAB_UNDEF,0,
  _LITTLE_ENDIAN_,
  16,0,
  FFF_SECTOUT
};
#endif

#ifdef SINCQL
struct FFFuncs fff_sincql = {
  "sinclairql",
  defaultscript,
  NULL,
  NULL,
  sincql_options,
  sincql_printhelp,
  sincql_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  sincql_write,
  NULL,NULL,
  0,
  0x7ffe,
  0,
  0,
  RTAB_STANDARD,RTAB_STANDARD,
  _BIG_ENDIAN_,
  32,1,
  FFF_BASEINCR|FFF_KEEPRELOCS
};
#endif
#ifdef SREC19
struct FFFuncs fff_srec19 = {
  "srec19",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  rawbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  srec19_write,
  NULL,NULL,
  0,
  0x8000,
  0,
  0,
  RTAB_UNDEF,0,
  -1, /* endianness undefined, only write */
  16,
  0,  /* ptr-alignment unknown */
  FFF_NOFILE
};
#endif

#ifdef SREC28
struct FFFuncs fff_srec28 = {
  "srec28",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  rawbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  srec28_write,
  NULL,NULL,
  0,
  0x8000,
  0,
  0,
  RTAB_UNDEF,0,
  -1, /* endianness undefined, only write */
  32, /* 24 */
  0,  /* ptr-alignment unknown */
  FFF_NOFILE
};
#endif

#ifdef SREC37
struct FFFuncs fff_srec37 = {
  "srec37",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  rawbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  srec37_write,
  NULL,NULL,
  0,
  0x8000,
  0,
  0,
  RTAB_UNDEF,0,
  -1, /* endianness undefined, only write */
  32,
  0,  /* ptr-alignment unknown */
  FFF_NOFILE
};
#endif

#ifdef IHEX
struct FFFuncs fff_ihex = {
  "ihex",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  rawbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  ihex_write,
  NULL,NULL,
  0,
  0x8000,
  0,
  0,
  RTAB_UNDEF,0,
  -1, /* endianness undefined, only write */
  0,  /* addr_bits from input */
  0,  /* ptr-alignment unknown */
  FFF_NOFILE|FFF_OUTWORDADDR
};
#endif

#ifdef SHEX1
struct FFFuncs fff_shex1 = {
  "oilhex",
  defaultscript,
  NULL,
  NULL,
  NULL,
  NULL,
  rawbin_headersize,
  rawbin_identify,
  rawbin_readconv,
  NULL,
  rawbin_targetlink,
  NULL,
  NULL,
  NULL,
  NULL,NULL,NULL,
  rawbin_writeobject,
  rawbin_writeshared,
  shex1_write,
  NULL,NULL,
  0,
  0x8000,
  0,
  0,
  RTAB_UNDEF,0,
  -1, /* endianness undefined, only write */
  32,
  0,  /* ptr-alignment unknown */
  FFF_NOFILE
};
#endif


#ifdef RAWBIN
static int rawbin_options(struct GlobalVars *gv,
                          int argc,const char **argv,int *i)
{
  if (!strcmp(argv[*i],"-coalesced"))
    coalesced = TRUE;
  else if (!strcmp(argv[*i],"-multifile"))
    multifile = TRUE;
  else if (!strncmp(argv[*i],"-fill=",6)) {
    long long val;
    sscanf(&argv[*i][6],"%lli",&val);
    fillval = val;
  }
  else
    return 0;
  return 1;
}

static void rawbin_printhelp(void)
{
  printf("-coalesced        output all memory segments without gaps\n"
         "-multifile        start a new file for a gap between memory segments\n");
}
#endif

#ifdef ORICMC
static int oric_options(struct GlobalVars *gv,
                        int argc,const char **argv,int *i)
{
  if (!strcmp(argv[*i],"-autox"))
    autoexec = 1;
  else
    return 0;
  return 1;
}

static void oric_printhelp(void)
{
  printf("-autox            set auto-execute flag in the header\n");
}
#endif

#ifdef SINCQL
static int sincqlhdr = HDR_QDOS;

static int sincql_options(struct GlobalVars *gv,
                          int argc,const char **argv,int *i)
{
  long long val;

  if (!strcmp(argv[*i],"-qhdr"))
    sincqlhdr = HDR_QDOS;
  else if (!strcmp(argv[*i],"-xtcc"))
    sincqlhdr = HDR_XTCC;
  else if (!strncmp(argv[*i],"-stack=",7)) {
    if (sscanf(&argv[*i][7],"%lli",&val) == 1)
      stacksize = val;
  }
  else
    return 0;
  return 1;
}

static void sincql_printhelp(void)
{
  printf("-qhdr             prepend QDOS file header (default)\n"
         "-xtcc             append XTcc style trailer\n"
         "-stack=<val>      set stack size, which defaults to 4096\n");
}
#endif


static unsigned long rawbin_headersize(struct GlobalVars *gv)
{
  return 0;  /* no header - it's pure binary! */
}

#ifdef AMSDOS
static unsigned long amsdos_headersize(struct GlobalVars *gv)
{
  return 128;
}
#endif

#ifdef APPLEBIN
static unsigned long applebin_headersize(struct GlobalVars *gv)
{
  return 4;
}
#endif

#ifdef ATARICOM
static unsigned long ataricom_headersize(struct GlobalVars *gv)
{
  return 2;  /* followed by 4 bytes segment header */
}
#endif

#ifdef CBMPRG
static unsigned long cbmprg_headersize(struct GlobalVars *gv)
{
  return 2;
}
#endif

#ifdef COCOML
static unsigned long cocoml_headersize(struct GlobalVars *gv)
{
  return 0;
}
#endif

#ifdef DRAGONBIN
static unsigned long dragonbin_headersize(struct GlobalVars *gv)
{
  return 9;
}
#endif

#ifdef FOENIX
static unsigned long pgx_headersize(struct GlobalVars *gv)
{
  return 8;
}

static unsigned long pgz_headersize(struct GlobalVars *gv)
{
  return 1;  /* followed by 6 or 8 bytes segment header */
}
#endif

#ifdef ORICMC
static unsigned long oric_headersize(struct GlobalVars *gv)
{
  unsigned long len = strlen(gv->dest_name);

  if (len>4 && !stricmp(gv->dest_name+len-4,".tap"))
    len -= 4;
  if (len > 15)
    len = 15;
  return 15+len;
}
#endif


#ifdef JAGSRV
static unsigned long jagsrv_headersize(struct GlobalVars *gv)
{
  return TOSHDR_SIZE+JAGR_SIZE;  /* uses TOS-executable header */
}
#endif

#ifdef SINCQL
static unsigned long sincql_headersize(struct GlobalVars *gv)
{
  if (sincqlhdr == HDR_QDOS)
    return QDOSHDR_SIZE;
  return 0;
}
#endif


/*****************************************************************/
/*                        Read Binary                            */
/*****************************************************************/


static int rawbin_identify(struct GlobalVars *gv,char *name,uint8_t *p,
                           unsigned long plen,bool lib)
/* identify a binary file */
{
  return ID_UNKNOWN;  /* binaries are only allowed to be written! */
}


static void rawbin_readconv(struct GlobalVars *gv,struct LinkFile *lf)
{
  ierror("rawbin_readconv(): Can't read raw-binaries");
}


static int rawbin_targetlink(struct GlobalVars *gv,struct LinkedSection *ls,
                             struct Section *s)
/* returns 1, if target requires the combination of the two sections, */
/* returns -1, if target doesn't want to combine them, */
/* returns 0, if target doesn't care - standard linking rules are used. */
{
  ierror("rawbin_targetlink(): Impossible to link raw-binaries");
  return 0;
}



/*****************************************************************/
/*                        Write Binary                           */
/*****************************************************************/


static FILE *open_ascfile(struct GlobalVars *gv)
{
  FILE *f;

  if ((f = fopen(gv->dest_name,"w")) == NULL) {
    error(29,gv->dest_name);  /* Can't create output file */
    return NULL;
  }
  untrim_sections(gv,1); /* always output trailing 0-bytes in ASCII hex-files */
  return f;
}


static void savehdroffs(struct GlobalVars *gv,FILE *f,
                        size_t bytes,struct LinkedSection *ls)
{
  /* remember location in file header and skip it */
  hdrsect = ls;
  hdrfile = f;
  hdroffs = ftell(f);
  fwritegap(gv,f,bytes,0);
}


static int rewindtohdr(FILE *f)
{
  if (hdrfile == f) {
    fseek(f,hdroffs,SEEK_SET);
    return 1;
  }
  return 0;
}


static void even_padding(FILE *f)
{
  if (ftell(f) & 1)
    fwrite8(f,0);  /* make file size even */
}


static void trace_baseaddr(struct GlobalVars *gv,struct LinkedSection *ls)
{
  if (gv->trace_file)
    fprintf(gv->trace_file,"Base address = 0x%08llx.\n",
            (unsigned long long)ls->copybase);  /* @@@ FIXME! */
}


#ifdef AMSDOS
static void amsdos_header(FILE *f,uint16_t loadaddr,unsigned size)
{
  uint8_t buffer[128];
  uint16_t checksum;
  int i;

  memset(buffer,0,128);                 /* initialize header with zeros */
  memset(&buffer[1],' ',11);            /*  1 > 11: spaces */
  buffer[18] = 2;                       /* 18     : filetype = 2 - binary */
  write16le(&buffer[19],size);          /* 19 > 20: data length */
  write16le(&buffer[21],loadaddr);      /* 21 > 22: load address */
  buffer[23] = 0xff;                    /* 23     : 0xFF */
  write16le(&buffer[24],size);          /* 24 > 25: logical length */
  write16le(&buffer[26],execaddr);      /* 26 > 27: entry point */
  write32le(&buffer[64],size);          /* 64 > 66: filesize (3 bytes) */
  for (checksum=0,i=0; i<67; i++)
    checksum += buffer[i];
  write16le(&buffer[67],checksum);      /* 67 > 68: checksum */
  fwritex(f,buffer,128);
}
#endif


#ifdef JAGSRV
static void jagsrv_header(FILE *f,uint32_t loadaddr,unsigned size)
{
  uint8_t header[TOSHDR_SIZE+JAGR_SIZE];

  memset(header,0,TOSHDR_SIZE+JAGR_SIZE);
  write16be(&header[0],0x601a);
  write32be(&header[2],size+JAGR_SIZE);

  memcpy(&header[TOSHDR_SIZE+0],"JAGR",4);
  write16be(&header[TOSHDR_SIZE+4],3);  /* command 3: Load & Run */
  write32be(&header[TOSHDR_SIZE+6],loadaddr);
  write32be(&header[TOSHDR_SIZE+10],size);
  write32be(&header[TOSHDR_SIZE+14],execaddr);
  fwritex(f,header,TOSHDR_SIZE+JAGR_SIZE);
}
#endif


static int rawbin_segheader(struct GlobalVars *gv,FILE *f,
                            struct LinkedSection *ls,int header)
{
  /* write a segment/section header, when needed */
#ifdef ATARICOM
  if (header == HDR_ATARICOM) {  /* Atari COM section header */
    fwrite16le(f,ls->copybase);
    fwrite16le(f,ls->copybase+ls->size-1);
    return 1;
  }
#endif
#ifdef COCOML
  if (header == HDR_COCOML) {  /* Tandy Color Computer segment header */
    fwrite8(f,0);
    fwrite16be(f,ls->size);
    fwrite16be(f,ls->copybase);
    return 1;
  }
#endif
#ifdef FOENIX
  if (header == HDR_PGZ24) {  /* 24-bit LE load-address and size */
    fwrite16le(f,ls->copybase&0xffff);
    fwrite8(f,(ls->copybase>>16)&0xff);
    fwrite16le(f,ls->size&0xffff);
    fwrite8(f,(ls->size>>16)&0xff);
    return 1;
  }
  if (header == HDR_PGZ32) {  /* 32-bit LE load-address and size */
    fwrite32le(f,ls->copybase);
    fwrite32le(f,ls->size);
    return 1;
  }
#endif
  return 0;
}


static void rawbin_fileheader(struct GlobalVars *gv,FILE *f,
                              struct LinkedSection *ls,int header)
{
  /* write a file header, when needed */
#ifdef AMSDOS
  if (header == HDR_AMSDOS)  /* Amstrad/Schneider CPC */
    amsdos_header(f,ls->copybase,ls->size);
#endif
#ifdef APPLEBIN
  if (header == HDR_APPLEBIN) {  /* Apple DOS binary file header */
    fwrite16le(f,ls->copybase);
    savehdroffs(gv,f,2,ls);      /* insert file length later */
  }
#endif
#ifdef ATARICOM
  if (header == HDR_ATARICOM)  /* Atari COM file header */
    fwrite16le(f,0xffff);
#endif
#ifdef BBC
  if (header == HDR_BBC) { /* BBC info file */
    FILE *inf;
    char *name;
    name = alloc(strlen(gv->dest_name)+6);
    sprintf(name,"%s.inf",gv->dest_name);
    if (!(inf = fopen(name,"w"))) {
      error(29,name);
    }else{
      fprintf(inf,"$.%s FF%04X FF%04X\n",gv->dest_name,(unsigned int)entry_address(gv),(unsigned int)entry_address(gv));
      fclose(inf);
    }
    free(name);
  }
#endif
#ifdef CBMPRG
  if (header == HDR_CBMPRG) {  /* Commodore PET, VIC-20, 64, etc. */
    static int done;
    if (!done)
      fwrite16le(f,ls->copybase);
    done = 1;
  }
#endif
#ifdef DRAGONBIN
  if (header == HDR_DRAGONBIN) {  /* Dragon DOS file header */
    fwrite16be(f,0x5502);
    fwrite16be(f,ls->copybase);
    savehdroffs(gv,f,2,ls);       /* insert file length later */
    fwrite16be(f,execaddr?(uint16_t)execaddr:ls->copybase);
    fwrite8(f,0xaa);
  }
#endif
#ifdef FOENIX
  if (header==HDR_PGXC02 || header==HDR_PGX816) {  /* single seg. header */
    static uint8_t hdr[4] = { 'P','G','X' };
    hdr[3] = header==HDR_PGX816 ? 1 : 3;
    fwritex(f,hdr,sizeof(hdr));
    fwrite32le(f,ls->copybase);
  }
  if (header == HDR_PGZ24) {  /* 24-bit Foenix multi segment header */
    fwrite8(f,'Z');
  }
  if (header == HDR_PGZ32) {  /* 32-bit Foenix multi segment header */
    fwrite8(f,'z');
  }
#endif
#ifdef ORICMC
  if (header == HDR_ORICMC) {  /* Oric machine code file header */
    static const uint8_t hdr[] = {
      0x16,0x16,0x16,0x16,0x24,0,0,0x80
    };
    int i;
    fwritex(f,hdr,sizeof(hdr));
    fwrite8(f,autoexec?0xc7:0);
    savehdroffs(gv,f,2,ls);    /* insert last address later */
    fwrite16be(f,ls->copybase);
    fwrite8(f,0);
    for (i=0; i<15 && gv->dest_name[i] && stricmp(&gv->dest_name[i],".tap");
         i++)
      fwrite8(f,toupper((unsigned char)gv->dest_name[i]));
    fwrite8(f,0);
  }
#endif
#ifdef JAGSRV
  if (header == HDR_JAGSRV) {  /* Atari Jaguar, JAGSRV header for SkunkBoard */
    struct LinkedSection *finls = (struct LinkedSection *)gv->lnksec.last;
    jagsrv_header(f,ls->copybase,
                  (finls->copybase+finls->filesize)-ls->copybase);
  }
#endif
#ifdef SINCQL
  if (header == HDR_QDOS) {
    static const uint8_t hdr[QDOSHDR_SIZE-8] = {
      ']','!','Q','D','O','S',' ','F','i','l','e',' ','H','e','a','d','e','r',
      0,0xf,0,1
    };
    fwritex(f,hdr,sizeof(hdr));
    savehdroffs(gv,f,8,ls);  /* insert dataspace size here */
  }    
#endif

  /* followed by optional section/segment header */
  rawbin_segheader(gv,f,ls,header);
}


static void rawbin_trailer(struct GlobalVars *gv,FILE *f,
                           struct LinkedSection *ls,int header)
{
  /* write a file trailer or patch the header, when needed */
#ifdef APPLEBIN
  if (header == HDR_APPLEBIN && rewindtohdr(f))
    fwrite16le(f,ls->copybase+ls->size-hdrsect->copybase);
#endif
#ifdef COCOML
  if (header == HDR_COCOML) {
    fwrite8(f,0xff);
    fwrite16be(f,0);
    fwrite16be(f,execaddr);
  }
#endif
#ifdef DRAGONBIN
  if (header == HDR_DRAGONBIN && rewindtohdr(f))
    fwrite16be(f,ls->copybase+ls->size-hdrsect->copybase);
#endif
#ifdef FOENIX
  if (header == HDR_PGZ24) {
    fwrite16le(f,execaddr&0xffff);
    fwrite8(f,(execaddr>>16)&0xff);
    fwrite16le(f,0);  /* no more segments */
    fwrite8(f,0);
  }
  if (header == HDR_PGZ32) {
    fwrite32le(f,execaddr);
    fwrite32le(f,0);  /* no more segments */
  }
#endif
#ifdef ORICMC
  if (header == HDR_ORICMC && rewindtohdr(f))
    fwrite16be(f,ls->copybase+ls->size-1);
#endif
#ifdef SINCQL
  if (header == HDR_XTCC) {
    even_padding(f);  /* align to even address */
    fwrite32be(f,0x58546363);  /* XTcc ID */
  }
  else if (header == HDR_QDOS) {
    /* even with QDOS header, the file size must be even, and the
       packed reloc table size is not guaranteed to be even */
    even_padding(f);  /* align to even address */
  }
  if (header==HDR_XTCC || (header == HDR_QDOS && rewindtohdr(f))) {
    fwrite32be(f,(uint32_t)(relocsize+64>udatasize+stacksize?
                            ((relocsize+65)&~1):udatasize+stacksize));
  }
#endif
#ifdef BBC
  if(header == HDR_BBC && bbcload){
    fprintf(bbcload,"*run %s\r",gv->dest_name);
    fclose(bbcload);
  }
#endif
}


static void rawbin_out(struct GlobalVars *gv,FILE *f,int header)
/* creates executable raw-binary files (with absolute addresses) */
{
  const char *fn = "rawbin_out(): ";
  bool dorelocs = gv->keep_relocs && (fff[gv->dest_format]->flags & FFF_KEEPRELOCS);
  FILE *firstfile = f;
  bool firstsec = TRUE;
  lword addr,baseaddr;
  struct LinkedSection *ls,*prevls;
  struct BinReloc *brp,*brlist=NULL;
  size_t brcnt = 0;
  char *name;

  /* determine program's execution address */
  execaddr = entry_address(gv);

  /* section loop */
  while (ls = load_next_section(gv)) {
    if (ls->ld_flags & LSF_NOLOAD)
      continue;  /* do not output no-load sections */

    if (gv->output_sections) {
      if (ls->size != 0) {
        /* make a new file for each output section */
        if (gv->trace_file)
          fprintf(gv->trace_file,"Base address section %s = 0x%08llx.\n",
                  ls->name,(unsigned long long)ls->copybase);  /* @@@ FIXME */
        if (f != NULL)
          ierror("%sopen file with output_sections",fn);
        if (gv->osec_base_name != NULL) {
          /* use a common base name before the section name */
          name = alloc(strlen(gv->osec_base_name)+strlen(ls->name)+2);
	  sprintf(name,"%s.%s",gv->osec_base_name,ls->name);
        }
        else
          name = (char *)ls->name;
        if (!(f = fopen(name,"wb"))) {
          error(29,name);
          break;
        }
        if (gv->osec_base_name != NULL)
          free(name);
        rawbin_fileheader(gv,f,ls,header);
      }
    }
    else if (firstsec) {
      /* write an optional header before the first section */
      baseaddr = ls->copybase;
      trace_baseaddr(gv,ls);
      firstsec = FALSE;
      rawbin_fileheader(gv,f,ls,header);
    }
    else {
      /* handle gaps between this and the previous section */
      if (ls->copybase > addr) {
        if (ls->copybase-addr < MAXGAP || !multifile) {
          if (!rawbin_segheader(gv,f,ls,header) && !coalesced)
            fwritegap(gv,f,ls->copybase-addr,fillval);
        }
        else {  /* open a new file for this section */
          if (f != firstfile)
            fclose(f);
          name = alloc(strlen(gv->dest_name)+strlen(ls->name)+2);
#ifdef BBC
          if(header == HDR_BBC){
            sprintf(name,"%s%s",gv->dest_name,ls->name);
            if(bbcload)
              fprintf(bbcload,"*srload %s%s 8000 %s\r",gv->dest_name,ls->name,ls->name+1);
          }else
#endif
	    sprintf(name,"%s.%s",gv->dest_name,ls->name);
          if (!(f = fopen(name,"wb"))) {
            error(29,name);
            break;
          }
          free(name);
          rawbin_fileheader(gv,f,ls,header);
        }
      }
      else if (ls->copybase < addr)
        error(98,fff[gv->dest_format]->tname,ls->name,prevls->name);
      else  /* next section attaches exactly */
        rawbin_segheader(gv,f,ls,header);
    }

    /* handle relocations */
    if (dorelocs) {
      /* remember relocations, with offsets from program's base address */
      struct Reloc *r;
      struct RelocInsert *ri;
      struct LinkedSection *relsec;
      struct BinReloc *newbr;
      lword v;

      sort_relocs(&ls->relocs);
      for (r=(struct Reloc *)ls->relocs.first;
           r->n.next!=NULL; r=(struct Reloc *)r->n.next) {
        if (ri = r->insert) {
          if (r->rtype!=R_ABS || ri->bpos!=0 || ri->bsiz!=gv->bits_per_taddr)
            continue;
        }
        else
          continue;
        if ((relsec = r->relocsect.lnk) == NULL)
          continue;

        /* use offset from program's base address for the relocation */
        v = writesection(gv,ls->data,r->offset,r,
                         r->addend+(relsec->copybase-baseaddr));
        if (v != 0) {
          /* Calculated value doesn't fit into relocation type x ... */
          if (ri = r->insert) {
            struct Section *isec;
            unsigned long ioffs;

            isec = getinpsecoffs(ls,r->offset,&ioffs);
            /*print_function_name(isec,ioffs);*/
            error(35,gv->dest_name,ls->name,r->offset,getobjname(isec->obj),
                  isec->name,ioffs,optsgnstr(v),abstaddr(v),
                  reloc_name[r->rtype],(int)ri->bpos,(int)ri->bsiz,
                  mtaddr(gv,ri->mask));
          }
          else
            ierror("%sReloc (%s+%lx), type=%s, without RelocInsert",
                    fn,ls->name,r->offset,reloc_name[r->rtype]);
        }

        /* remember relocation offsets for later */
        newbr = alloc(sizeof(struct BinReloc));
        newbr->next = NULL;
        newbr->offset = (ls->copybase - baseaddr) + r->offset;
        if (brp = brlist) {
          while (brp->next)
            brp = brp->next;
          brp->next = newbr;
        }
        else
          brlist = newbr;
        brcnt++;
        r->rtype = R_NONE;  /* disable it for calc_relocs() */
      }
    }

    addr = ls->copybase;

    if (ls->flags & SF_ALLOC) {
      /* resolve all (remaining) relocations */
      calc_relocs(gv,ls);

      /* write section contents */
      fwritefullsect(gv,f,ls);
      addr += ls->size;
    }

    prevls = ls;
  }

  /* write optional relocation table after all sections */
  if (dorelocs) {
    long szoffs = ftell(f);
    fwritetaddr(gv,f,0);  /* table's size in tbytes, excluding this word */

    if (brlist) {
      lword lastoffs,diff,limit;

      limit = (1 << gv->bits_per_tbyte) - 1;
      for (brp=brlist,lastoffs=0; brp; brp=brp->next) {
        diff = brp->offset - lastoffs;
        if (diff < 0) {
          ierror("%snegative reloc offset diff %ld\n",fn,(long)diff);
        }
        else if (diff==0 || diff>limit) {
          fwritetbyte(gv,f,0);    /* 0 indicates an address-size difference */
          fwritetaddr(gv,f,diff); /* ...in the next word */
          relocsize += gv->tbytes_per_taddr;
        }
        else
          fwritetbyte(gv,f,diff);
        relocsize++;
        lastoffs = brp->offset;
      }

      /* now we can write the reloc table's size */
      fseek(f,szoffs,SEEK_SET);
      fwritetaddr(gv,f,(lword)relocsize);
      fseek(f,0,SEEK_END);
    }
  }

  /* write optional trailer */
  rawbin_trailer(gv,f,prevls,header);

  if (f!=NULL && f!=firstfile)
    fclose(f);
}


static void rawbin_writeshared(struct GlobalVars *gv,FILE *f)
{
  error(30);  /* Target file format doesn't support shared objects */
}


static void rawbin_writeobject(struct GlobalVars *gv,FILE *f)
{
  error(62);  /* Target file format doesn't support relocatable objects */
}


#ifdef RAWBIN
static void rawbin_write(struct GlobalVars *gv,FILE *f)
/* creates a raw-binary file, where memory segments are either written to
   multiple files, coalesced to the previous segment or written with a gap
   filled by zero-bytes */
{
  rawbin_out(gv,f,HDR_NONE);
}
#endif


#ifdef AMSDOS
static void amsdos_write(struct GlobalVars *gv,FILE *f)
/* creates one or more raw-binary files with an AMSDOS header, suitable */
/* for loading as an executable on Amstrad/Scheider CPC computers */
{
  multifile = TRUE;
  rawbin_out(gv,f,HDR_AMSDOS);
}
#endif


#ifdef APPLEBIN
static void applebin_write(struct GlobalVars *gv,FILE *f)
/* creates a raw-binary file with an Apple DOS binary header, suitable */
/* for loading as an executable on Apple II computers */
{
  rawbin_out(gv,f,HDR_APPLEBIN);
}
#endif


#ifdef ATARICOM
static void ataricom_write(struct GlobalVars *gv,FILE *f)
/* creates a raw-binary file with an ATARI COM file header, suitable */
/* for loading as an executable on Atari 8-bit computers */
{
  rawbin_out(gv,f,HDR_ATARICOM);
}
#endif


#ifdef CBMPRG
static void cbmprg_write(struct GlobalVars *gv,FILE *f)
/* creates a raw-binary file with a Commodore header, suitable */
/* for loading as an executable on PET, VIC-20, 64, etc. computers */
{
  rawbin_out(gv,f,HDR_CBMPRG);
}

static void cbmreu_write(struct GlobalVars *gv,FILE *f)
/* creates a raw-binary file with a Commodore header, followed by */
/* a REU image. */
{
  multifile = TRUE;
  rawbin_out(gv,f,HDR_CBMPRG);
}
#endif


#ifdef COCOML
static void cocoml_write(struct GlobalVars *gv,FILE *f)
/* creates a raw-binary file with CoCo segment headers and trailer, suitable */
/* for loading as a machine language file on Tandy Color Computer 1,2,3 */
{
  rawbin_out(gv,f,HDR_COCOML);
}
#endif


#ifdef DRAGONBIN
static void dragonbin_write(struct GlobalVars *gv,FILE *f)
{
/* creates a raw-binary file with a Dragon DOS file header, suitable */
/* for loading as an executable on Dragon 32 and 64 computers */
  rawbin_out(gv,f,HDR_DRAGONBIN);
}
#endif


#ifdef FOENIX
static void pgxc02_write(struct GlobalVars *gv,FILE *f)
{
/* creates a raw-binary file with a PGX single-segment file header, */
/* suitable for loading as an executable on the 65C02-based */
/* C256 Foenix computers */
  rawbin_out(gv,f,HDR_PGXC02);
}

static void pgx816_write(struct GlobalVars *gv,FILE *f)
{
/* creates a raw-binary file with a PGX single-segment file header, */
/* suitable for loading as an executable on the 65816-based */
/* C256 Foenix computers */
  rawbin_out(gv,f,HDR_PGX816);
}

static void pgz24_write(struct GlobalVars *gv,FILE *f)
{
/* creates a raw-binary file with a PGZ file header and multiple segment */
/* headers, suitable for loading as an executable on a 16- or 24-bit */
/* Foenix platform */
  rawbin_out(gv,f,HDR_PGZ24);
}

static void pgz32_write(struct GlobalVars *gv,FILE *f)
{
/* creates a raw-binary file with a PGZ file header and multiple segment */
/* headers, suitable for loading as an executable on a 32-bit */
/* Foenix platform */
  rawbin_out(gv,f,HDR_PGZ32);
}
#endif


#ifdef ORICMC
static void oric_write(struct GlobalVars *gv,FILE *f)
{
/* creates a raw-binary file with an Oric machine code file header, */
/* suitable for loading as an executable on Oric-1/Atmos/Telestrat computers */
  rawbin_out(gv,f,HDR_ORICMC);
}
#endif


#ifdef JAGSRV
static void jagsrv_write(struct GlobalVars *gv,FILE *f)
/* creates a raw-binary file with a JAGSRV header, suitable */
/* for loading and executing on a SkunkBoard equipped Atari Jaguar, or */
/* a VirtualJaguar emulator */
{
  rawbin_out(gv,f,HDR_JAGSRV);
}
#endif

#ifdef BBC
static void bbc_write(struct GlobalVars *gv,FILE *f)
/* creates a single raw-binary file plus a bbc info file */
{
  rawbin_out(gv,f,HDR_BBC);
}

static void bbc_write2(struct GlobalVars *gv,FILE *f)
/* creates a single raw-binary file plus a bbc info file */
{
  char *name = alloc(strlen(gv->dest_name)+8);
  sprintf(name,"load%s",gv->dest_name);
  bbcload = fopen(name,"w");
  multifile = TRUE;
  rawbin_out(gv,f,HDR_BBC);
  free(name);
}
#endif

#ifdef SINCQL
static void sincql_write(struct GlobalVars *gv,FILE *f)
/* creates a raw-binary file with a QDOS file header or XTcc trailer,
   suitable for Sinclair QL emulators */
{
  struct LinkedSection *ls;

  for (ls=(struct LinkedSection *)gv->lnksec.first,udatasize=0;
       ls->n.next!=NULL; ls=(struct LinkedSection *)ls->n.next) {
    if (ls->flags & SF_ALLOC) {
      if (ls->ld_flags & LSF_NOLOAD)
        udatasize += ls->size;
      else
        udatasize = 0;  /* @@@ reset when interrupted by initialized data */
    }
  }
  if (!stacksize)
    stacksize = 4096;  /* default */
  if ((udatasize + stacksize) & 1)
    ++udatasize;  /* make sure bss-segment has an even number of bytes */
  rawbin_out(gv,f,sincqlhdr);
}
#endif

#if defined(SREC19) || defined(SREC28) || defined(SREC37)
static void SRecOut(FILE *f,int stype,uint8_t *buf,int len)
{
  uint8_t chksum = 0xff-len-1;

  fprintf(f,"S%1d%02X",stype,(unsigned)(len+1));
  for (; len>0; len--) {
    fprintf(f,"%02X",(unsigned)*buf);
    chksum -= *buf++;
  }
  fprintf(f,"%02X\n",(unsigned)chksum);
}


static void srec_write(struct GlobalVars *gv,FILE *f,int addrsize)
{
  bool firstsec = TRUE;
  struct LinkedSection *ls;
  unsigned long len,addr;
  uint8_t *p,buf[MAXSREC+8];

  /* open ASCII output file */
  if ((f = open_ascfile(gv)) == NULL)
    return;

  execaddr = entry_address(gv);  /* determine program's execution address */

  /* write header */
  buf[0] = buf[1] = 0;
  strncpy((char *)&buf[2],gv->dest_name,MAXSREC+6);
  SRecOut(f,0,buf,(strlen(gv->dest_name)<(MAXSREC+6)) ?
          strlen(gv->dest_name)+2 : MAXSREC+8);

  while (ls = load_next_section(gv)) {
    if (ls->size == 0 || !(ls->flags & SF_ALLOC) || (ls->ld_flags & LSF_NOLOAD))
      continue;  /* ignore empty sections */

    if (firstsec) {
      trace_baseaddr(gv,ls);
      firstsec = FALSE;
    }

    /* resolve all relocations and write section contents */
    calc_relocs(gv,ls);

    for (p=ls->data,addr=ls->copybase,len=ls->filesize; len>0; ) {
      int nbytes = (len>MAXSREC) ? MAXSREC : len;

      switch (addrsize) {
        case 2:
          write16be(buf,(uint16_t)addr);
          memcpy(buf+2,p,nbytes);
          SRecOut(f,1,buf,2+nbytes);
          break;
        case 3:
          buf[0] = (uint8_t)((addr>>16)&0xff);
          buf[1] = (uint8_t)((addr>>8)&0xff);
          buf[2] = (uint8_t)(addr&0xff);
          memcpy(buf+3,p,nbytes);
          SRecOut(f,2,buf,3+nbytes);
          break;
        case 4:
          write32be(buf,(uint32_t)addr);
          memcpy(buf+4,p,nbytes);
          SRecOut(f,3,buf,4+nbytes);
          break;
        default:
          ierror("srec_write(): Illegal SRec-type: %d",addrsize);
          nbytes = len;
          break;
      }
      p += nbytes;
      addr += nbytes;
      len -= nbytes;
    }
  }

  /* write trailer */
  write32be(buf,(uint32_t)execaddr);
  SRecOut(f,11-addrsize,buf+4-addrsize,addrsize);
  fclose(f);
}

#endif


#ifdef SREC19
static void srec19_write(struct GlobalVars *gv,FILE *f)
/* creates a Motorola S-Record file (S0,S1,S9), using 16-bit addresses */
{
  srec_write(gv,f,2);
}
#endif


#ifdef SREC28
static void srec28_write(struct GlobalVars *gv,FILE *f)
/* creates a Motorola S-Record file (S0,S2,S8), using 24-bit addresses */
{
  srec_write(gv,f,3);
}
#endif


#ifdef SREC37
static void srec37_write(struct GlobalVars *gv,FILE *f)
/* creates a Motorola S-Record file (S0,S3,S7), using 32-bit addresses */
{
  srec_write(gv,f,4);
}
#endif


#ifdef IHEX
static void IHexOut(FILE *f,int out_le,unsigned long addr,uint8_t *buf,
                    int len,size_t opb)
{
  static unsigned long hiaddr;
  uint8_t chksum;

  if (((addr&0xffff0000)>>16)!=hiaddr) {
    hiaddr = (addr&0xffff0000) >> 16;
    fprintf(f,":02000004%02X%02X%02X\n",
            (unsigned)(hiaddr>>8)&0xff,
            (unsigned)hiaddr&0xff,
            (unsigned)(-(2+4+(hiaddr>>8)+hiaddr))&0xff);
  }
  fprintf(f,":%02X%02X%02X00",
          (unsigned)(len*opb)&0xff,
          (unsigned)(addr>>8)&0xff,
          (unsigned)addr&0xff);
  chksum = len*opb + (addr>>8) + addr;
  for (; len>0; len--) {
    uint8_t b;
    int i;

    if (out_le) {
      for (i=opb-1; i>=0; i--) {
        b = buf[i];
        fprintf(f,"%02X",(unsigned)b);
        chksum += b;
      }
    }
    else {
      for (i=0; i<opb; i++) {
        b = buf[i];
        fprintf(f,"%02X",(unsigned)b);
        chksum += b;
      }
    }
    buf += opb;
  }
  fprintf(f,"%02X\n",(-chksum)&0xff);
}


static void ihex_write(struct GlobalVars *gv,FILE *f)
{
  size_t opb = gv->octets_per_tbyte;
  bool firstsec = TRUE;
  struct LinkedSection *ls;
  unsigned long addr,len;
  uint8_t *p;

  /* open ASCII output file */
  if ((f = open_ascfile(gv)) == NULL)
    return;

  while (ls = load_next_section(gv)) {
    if (ls->size == 0 || !(ls->flags & SF_ALLOC) || (ls->ld_flags & LSF_NOLOAD))
      continue;  /* ignore empty sections */

    if (firstsec) {
      trace_baseaddr(gv,ls);
      firstsec = FALSE;
    }

    /* resolve all relocations and write section contents */
    calc_relocs(gv,ls);

    p = ls->data;
    addr = ls->copybase * opb;  /* yes, that's probably how it is done */
    len = ls->filesize;
    while (len > 0) {
      int nbytes = len > MAXIREC/opb ? MAXIREC/opb : len;

      if ((((unsigned long)addr)&0xffff) + nbytes*opb > 0xffff)
        nbytes = (0x10000 - (((unsigned long)addr) & 0xffff)) / opb;
      IHexOut(f,gv->output_le,addr,p,nbytes,opb);
      p += nbytes * opb;
      addr += nbytes * opb;
      len -= nbytes;
    }
  }

  fprintf(f,":00000001FF\n");
  fclose(f);
}
#endif

#ifdef SHEX1
static void SHex1Out(FILE *f,unsigned long addr,uint8_t *buf,int len)
{
  int wordcnt=(len+3)/4;
  int i;

  fprintf(f,"%06lX %d",addr,wordcnt);

  for(i=0;i<len;i++){
    if((i&3)==0)
      fprintf(f," ");
    fprintf(f,"%02X",buf[i]);
  }
  for(i=0;i<wordcnt*4-len;i++)
    fprintf(f,"00");
  fprintf(f,"\n");
}


static void shex1_write(struct GlobalVars *gv,FILE *f)
{
  bool firstsec = TRUE;
  struct LinkedSection *ls;
  unsigned long len,addr;
  uint8_t *p;

  /* open ASCII output file */
  if ((f = open_ascfile(gv)) == NULL)
    return;

  while (ls = load_next_section(gv)) {
    if (ls->size == 0 || !(ls->flags & SF_ALLOC) || (ls->ld_flags & LSF_NOLOAD))
      continue;  /* ignore empty sections */

    if (firstsec) {
      trace_baseaddr(gv,ls);
      firstsec = FALSE;
    }

    /* resolve all relocations and write section contents */
    calc_relocs(gv,ls);

    for (p=ls->data,addr=ls->copybase,len=ls->filesize; len>0; ) {
      int nbytes = (len>32) ? 32 : len;

      SHex1Out(f,addr,p,nbytes);
      p += nbytes;
      addr += nbytes;
      len -= nbytes;
    }
  }
  fprintf(f,"000000 0\n");
  fclose(f);
}
#endif


#endif

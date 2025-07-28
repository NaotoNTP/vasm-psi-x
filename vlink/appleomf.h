/* $VER: vlink xfile.h V0.18 (03.07.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 2024  Frank Wille
 */


/* OMF v1/v2 segment header */
typedef struct
{
  uint8_t bytecnt[4];     /* number of bytes(v2)/blocks(v1) to next segment */
  uint8_t resspc[4];      /* cleared data space at the end of the segment */
  uint8_t length[4];      /* total length of segment data (including resspc) */
  uint8_t kindv1;         /* OMF v1: segment type and attributes */
  uint8_t lablen;         /* label length (0 = first byte has length) */
  uint8_t numlen;         /* size of values in bytes */
  uint8_t version;        /* 1 for OMF v1, 2 for OMF v2 */
  uint8_t banksize[4];    /* border of banksize should not be crossed by seg */
  uint8_t kindv2[2];      /* OMF v2: segment type and attributes */
  uint8_t reserved[2];
  uint8_t org[4];         /* absolute start address or 0 for relocatable */
  uint8_t align[4];       /* alignment boundary in bytes */
  uint8_t numsex;         /* 0 is little-, 1 is big-endian */
  uint8_t lcbank;         /* OMF v1: language card bank - unused */
  uint8_t segnum[2];      /* segment number, starting with 1 */
  uint8_t entry[4];       /* offset to entry point of segment */
  uint8_t dispname[2];    /* offset to the loadname field */
  uint8_t dispdata[2];    /* offset to the segment body */
  uint8_t loadname[10];
  /* it follows the segment name (using lablen) and the body */
} OMFSeghdr;

/* segment types */
#define SEGT_CODE         0x00
#define SEGT_DATA         0x01
#define SEGT_JUMPTAB      0x02
#define SEGT_PATHNAME     0x04
#define SEGT_LIBDICT      0x08
#define SEGT_INIT         0x10
#define SEGT_DPSTACK      0x12

/* segment attributes (v2) */
#define SEGA_RELOAD       (1<<10)
#define SEGA_ABSBANK      (1<<11)
#define SEGA_NOSPEC       (1<<12)
#define SEGA_PIC          (1<<13)
#define SEGA_PRIVATE      (1<<14)
#define SEGA_DYNAMIC      (1<<15)


/* OMF body opcodes */
#define OMFOC_END         0x00
#define OMFOC_RELOC       0xe2
#define OMFOC_INTERSEG    0xe3
#define OMFOC_LCONST      0xf2
#define OMFOC_cRELOC      0xf5
#define OMFOC_cINTERSEG   0xf6
#define OMFOC_SUPER       0xf7


/* OMF relocation */
typedef struct
{
  struct node n;
  unsigned flags;
  unsigned short fileno;
  unsigned short segno;
  unsigned short size;          /* relocation field size in bytes */
  short shift;                  /* value shift (right is negative) */
  unsigned long offset;         /* reloc-field offset in current segment */
  long addend;                  /* offset in referenced segno plus addend */
} OMFReloc;

/* flags */
#define OMFRF_SUPER 1           /* used in a SUPER reloc */
#define OMFRF_RELOC 2           /* used in a RELOC or cRELOC */
#define OMFRF_INTERSEG 4        /* used in a INTERSEG or cINTERSEG */
#define OMFRF_DONE (OMFRF_SUPER|OMFRF_RELOC|OMFRF_INTERSEG)


/* limits */
#define OMF_BANKSIZE 0x10000
#define OMF_MAXDPSTKSIZE 0xc000

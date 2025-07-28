/* $VER: vlink o65.h V0.16h (19.01.21)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2021  Frank Wille
 */


#define NSECS 4

/* section names */
#define TEXTNAME text_name
#define DATANAME data_name
#define BSSNAME bss_name
#define ZERONAME zero_name

/* section indexes */
enum {
  TSEC=0,DSEC,BSEC,ZSEC
};


/* O65 Header Info */
struct o65info {
  struct ObjectUnit *object;
  uint16_t mode;
  uint32_t base[NSECS];
  uint32_t len[NSECS];
  struct Section *sec[NSECS];
  uint8_t *data[DSEC+1];  /* only TSEC and DSEC have initialized data */
  unsigned namecount;
  char **names;
};

/* mode bits */
#define MO65_65816     (1<<15)
#define MO65_PAGED     (1<<14)
#define MO65_LONG      (1<<13)
#define MO65_OBJ       (1<<12)
#define MO65_SIMPLE    (1<<11)
#define MO65_CHAIN     (1<<10)
#define MO65_BSSZERO   (1<<9)
#define MO65_CPU2(m)   ((m&0x00f0)>>4)
#define MO65_ALIGN(m)  (m&3)

/* cpu2 values */
enum {
  CPU2_6502=0,CPU2_65C02,CPU2_65SC02,CPU2_65CE02,CPU2_NMOS6502,CPU2_65816
};

/* alignments */
enum {
  AO65_BYTE=0,AO65_WORD,AO65_LONG,AO65_BLOCK
};

/* header options */
enum {
  HOPT_FNAME=0,HOPT_OS,HOPT_GEN,HOPT_AUTHOR,HOPT_TIME
};

/* relocations */
#define R65TYPE(b) (b & 0xf0)
#define R65SEG(b) (b & 0x0f)

#define UNDSEGID 0
#define ABSSEGID 1
#define MINSEGID 2
#define MAXSEGID (MINSEGID+NSECS-1)

struct ImportNode {
  struct node n;
  struct ImportNode *hashchain;
  const char *name;
  unsigned idx;
};

struct ImportList {
  struct list l;
  struct ImportNode **hashtab;
  unsigned entries;
};
#define IMPHTABSIZE 0x100

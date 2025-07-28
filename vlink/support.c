/* $VER: vlink support.c V0.18 (23.12.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2024  Frank Wille
 */


#define SUPPORT_C
#include "vlink.h"

#define GAPBUFSIZE 1024  /* for fwritegap() */

const char *endian_name[2] = { "little", "big" };
static char *unnamed_txt = "unnamed";



void *alloc(size_t size)
/* allocate memory and print error message if not enough available */
{
  void *p;

  if (!size)
    size = 1;
  if (!(p = malloc(size)))
    error(1);  /* out of memory */
#ifdef DEBUG
  memset(p,0x55,size);
#endif
  return p;
}


void *re_alloc(void *old,size_t size)
/* reallocate memory block, preserve old contents */
{
  void *p;

  if (!size)
    size = 1;
  if (!(p = realloc(old,size)))
    error(1);  /* out of memory */
  return p;
}


void *alloczero(size_t size)
/* same as alloc() but zeroes the allocated memory */
{
  void *p = alloc(size);

  memset(p,0,size);
  return p;
}


char *allocstring(const char *s)
/* allocate space for a single string */
/* @@@ this should be improved by some kind of string buffer */
{
  char *p = alloc(strlen(s)+1);

  strcpy(p,s);
  return p;
}


void *alloc_hashtable(size_t entries)
{
  return alloczero(entries * sizeof(void *));
}


void initlist(struct list *l)
/* initializes a list structure */
{
  l->first = (struct node *)&l->dummy;
  l->dummy = NULL;
  l->last = (struct node *)&l->first;
}


void insertbefore(struct node *n,struct node *sn)
/* insert node n directly before node sn */
/* sn must be a real node - no dummy nodes allowed! */
{
  struct node *pn = sn->pred;

  n->next = sn;
  n->pred = pn;
  pn->next = sn->pred = n;
}


void insertbehind(struct node *pn,struct node *n)
/* insert node n directly behind node pn */
/* pn must be a real node - no dummy nodes allowed! */
{
  struct node *sn = pn->next;

  n->next = sn;
  n->pred = pn;
  pn->next = sn->pred = n;
}


void addhead(struct list *l,struct node *n)
/* add node as first element of list */
{
  struct node *fn = l->first;

  n->pred = fn->pred;
  fn->pred = n;
  n->next = fn;
  l->first = n;
}


void addtail(struct list *l,struct node *n)
/* add node as last element of list */
{
  struct node *ln = l->last;

  n->next = ln->next;
  ln->next = n;
  n->pred = ln;
  l->last = n;
}


struct node *remhead(struct list *l)
/* remove first node in list and return a pointer to it */
{
  struct node *n = l->first;

  if (n->next) {
    l->first = n->next;
    n->next->pred = n->pred;
    return n;
  }
  return NULL;
}


struct node *remnode(struct node *n)
/* remove a node from a list */
{
  n->next->pred = n->pred;
  n->pred->next = n->next;
  return n;
}


int stricmp(const char *str1,const char *str2)
{
  while (tolower((unsigned char)*str1) == tolower((unsigned char)*str2)) {
    if (!*str1) return 0;
    str1++; str2++;
  }
  return tolower(*(unsigned char *)str1) - tolower(*(unsigned char *)str2);
}


static size_t filesize(FILE *fp,const char *name)
/* return file size or 0 on error, file is at start of stream afterwards */
{
  long size;

  if (fgetc(fp) == EOF)  /* workaround for strange files or directories */
    return 0;
  if (fseek(fp,0,SEEK_END) >= 0)
    if ((size = ftell(fp)) >= 0)
      if (fseek(fp,0,SEEK_SET) >= 0)
        return (size_t)size;
  fclose(fp);
  error(7,name);  /* read error - doesn't return */
  return 0;
}


char *mapfile(const char *name)
/* Map a complete file into memory and return its address. */
/* The file's length is returned in *(p-sizeof(size_t)). */
/* The last byte is followed by a 0-byte, indicating EOF for */
/* functions which need it. */
{
  FILE *fp;
  char *p=NULL;
  size_t fsiz;

  if (fp = fopen(name,"rb")) {
    fsiz = filesize(fp,name);
    p = alloc(fsiz+sizeof(size_t)+1);
    *(size_t *)p = fsiz;  /* store file size before the text starts */
    p += sizeof(size_t);
    *(p+fsiz) = 0;        /* terminated by 0-byte */
    if (fread(p,1,fsiz,fp) != fsiz) {
      fclose(fp);
      error(7,name);  /* read error */
    }
    fclose(fp);
  }
  return p;
}


const char *base_name(const char *s)
/* returns last part of a path - the file name itself */
{
  char c;
  int l = strlen(s);

  while (l--) {
    c = s[l];
    if (c=='/' || c=='\\' || c==':')
      return &s[l+1];
  }
  return s;
}


char *check_name(char *name)
/* returns "unnamed", if name is a NULL-pointer */
{
  if (name)
    return name;
  return unnamed_txt;
}


bool checkrange(lword val,int sign,int size)
/* Checks if 'val' (signed, unsigned or unknown) fits into 'size' bits.
   sign==0 is unknown, checks signed and unsigned range,
   sign==1 is signed, sign==2 is unsigned.
   Returns FALSE when 'val' is out of range! */
{
  if (size>=0 && size<=sizeof(lword)*CHAR_BIT) {
    lword min = sign!=2 ? -(1LL<<(size-1)) : 0;
    lword max = sign==1 ? ((1LL<<(size-1))-1LL) : ((1LL<<size)-1LL);

    if (val<min || val>max)
      return FALSE;

    return TRUE;
  }

  ierror("checkrange(): size==%d (val=%lld)\n",size,(long long)val);
  return FALSE;  /* size is illegal */
}


int8_t host_endianness(void)
{
  static uint32_t x = 0x01020304;

  return *((uint8_t *)&x)==1 ? _BIG_ENDIAN_ : _LITTLE_ENDIAN_;
}


uint16_t swap16(uint16_t x)
/* 16-bit endian conversion */
{
  return (x&0xff)<<8 | (x&0xff00)>>8;
}


uint32_t swap32(uint32_t x)
/* 32-bit endian conversion */
{
  return (x&0xff)<<24 | (x&0xff00)<<8 |
         (x&0xff0000)>>8 | (x&0xff000000)>>24;
}


uint64_t swap64(uint64_t x)
/* 64-bit endian conversion */
{
  return (x&0xff)<<56 | (x&0xff00)<<40 |
         (x&0xff0000)<<24 | (x&0xff000000)<<8 |
         (x&0xff00000000LL)>>8 | (x&0xff0000000000LL)>>24 |
         (x&0xff000000000000LL)>>40 | (x&0xff00000000000000LL)>>56;
}


uint16_t read16be(void *vp)
/* read 16 bit word in big endian format */
{
  uint8_t *p = (uint8_t *)vp;

  return ((uint16_t)*p)<<8 | ((uint16_t)*(p+1));
}


uint32_t read32be(void *vp)
/* read 32 bit word in big endian format */
{
  uint8_t *p = (uint8_t *)vp;

  return ((uint32_t)*p)<<24 | ((uint32_t)*(p+1))<<16 |
         ((uint32_t)*(p+2))<<8 | ((uint32_t)*(p+3));
}


uint64_t read64be(void *vp)
/* read 64 bit word in big endian format */
{
  uint8_t *p = (uint8_t *)vp;

  return ((uint64_t)*p)<<56 | ((uint64_t)*(p+1))<<48 |
         ((uint64_t)*(p+2))<<40 | ((uint64_t)*(p+3))<<32 |
         ((uint64_t)*(p+4))<<24 | ((uint64_t)*(p+5))<<16 |
         ((uint64_t)*(p+6))<<8 | ((uint64_t)*(p+7));
}


void write16be(void *vp,uint16_t x)
/* write 16 bit word in big endian format */
{
  uint8_t *p = (uint8_t *)vp;

  *p++ = (uint8_t)((x>>8)&0xff);
  *p = (uint8_t)(x&0xff);
}


void write32be(void *vp,uint32_t x)
/* write 32 bit word in big endian format */
{
  uint8_t *p = (uint8_t *)vp;

  *p++ = (uint8_t)((x>>24)&0xff);
  *p++ = (uint8_t)((x>>16)&0xff);
  *p++ = (uint8_t)((x>>8)&0xff);
  *p = (uint8_t)(x&0xff);
}


void write64be(void *vp,uint64_t x)
/* write 64 bit word in big endian format */
{
  uint8_t *p = (uint8_t *)vp;

  *p++ = (uint8_t)((x>>56)&0xff);
  *p++ = (uint8_t)((x>>48)&0xff);
  *p++ = (uint8_t)((x>>40)&0xff);
  *p++ = (uint8_t)((x>>32)&0xff);
  *p++ = (uint8_t)((x>>24)&0xff);
  *p++ = (uint8_t)((x>>16)&0xff);
  *p++ = (uint8_t)((x>>8)&0xff);
  *p = (uint8_t)(x&0xff);
}


uint16_t read16le(void *vp)
/* read 16 bit word in little endian format */
{
  uint8_t *p = (uint8_t *)vp;

  return ((uint16_t)*p) | ((uint16_t)*(p+1))<<8;
}


uint32_t read32le(void *vp)
/* read 32 bit word in little endian format */
{
  uint8_t *p = (uint8_t *)vp;

  return ((uint32_t)*p) | ((uint32_t)*(p+1))<<8 |
         ((uint32_t)*(p+2))<<16 | ((uint32_t)*(p+3))<<24;
}


uint64_t read64le(void *vp)
/* read 64 bit word in little endian format */
{
  uint8_t *p = (uint8_t *)vp;

  return ((uint64_t)*p) | ((uint64_t)*(p+1))<<8 |
         ((uint64_t)*(p+2))<<16 | ((uint64_t)*(p+3))<<24 |
         ((uint64_t)*(p+4))<<32 | ((uint64_t)*(p+5))<<40 |
         ((uint64_t)*(p+6))<<48 | ((uint64_t)*(p+7))<<56;
}


void write16le(void *vp,uint16_t x)
/* write 16 bit word in little endian format */
{
  uint8_t *p = (uint8_t *)vp;

  *p++ = (uint8_t)(x&0xff);
  *p = (uint8_t)((x>>8)&0xff);
}


void write32le(void *vp,uint32_t x)
/* write 32 bit word in little endian format */
{
  uint8_t *p = (uint8_t *)vp;

  *p++ = (uint8_t)(x&0xff);
  *p++ = (uint8_t)((x>>8)&0xff);
  *p++ = (uint8_t)((x>>16)&0xff);
  *p = (uint8_t)((x>>24)&0xff);
}


void write64le(void *vp,uint64_t x)
/* write 64 bit word in little endian format */
{
  uint8_t *p = (uint8_t *)vp;

  *p++ = (uint8_t)(x&0xff);
  *p++ = (uint8_t)((x>>8)&0xff);
  *p++ = (uint8_t)((x>>16)&0xff);
  *p++ = (uint8_t)((x>>24)&0xff);
  *p++ = (uint8_t)((x>>32)&0xff);
  *p++ = (uint8_t)((x>>40)&0xff);
  *p++ = (uint8_t)((x>>48)&0xff);
  *p = (uint8_t)((x>>56)&0xff);
}


uint16_t read16(bool be,void *p)
{
  return (be)?(read16be(p)):(read16le(p));
}


uint32_t read32(bool be,void *p)
{
  return (be)?(read32be(p)):(read32le(p));
}


uint64_t read64(bool be,void *p)
{
  return (be)?(read64be(p)):(read64le(p));
}


void write16(bool be,void *p,uint16_t d)
{
  if (be)
    write16be(p,d);
  else
    write16le(p,d);
}


void write32(bool be,void *p,uint32_t d)
{
  if (be)
    write32be(p,d);
  else
    write32le(p,d);
}


void write64(bool be,void *p,uint64_t d)
{
  if (be)
    write64be(p,d);
  else
    write64le(p,d);
}


lword readbf(bool be,void *src,int fldsiz,int pos,int siz)
/* read value from bitfield with length fldsiz, starting at bit-position pos */
{
  uint8_t *p = src;
  lword d = 0;
  int n;

  /* advance to start-byte (MSB) */
  if (be)
    p += pos >> 3;
  else
    p += fldsiz - (pos >> 3);

  pos &= 7;
  n = (pos + siz + 7) >> 3;  /* number of bytes to read */

  if (be) {
    while (n--) {
      d <<= 8;
      d |= (lword)*p++;
    }
  }
  else {
    while (n--) {
      d <<= 8;
      d |= (lword)*(--p);
    }
  }

  /* normalize and mask the extracted bitfield */
  d >>= (8 - ((pos + siz) & 7)) & 7;
  return d & makemask(siz);
}


void writebf(bool be,void *dst,int fldsiz,int pos,int siz,lword d)
/* write value to bitfield with length fldsiz, starting at bit-position pos */
{
  uint8_t *p = dst;
  uint8_t m,b;
  int n,sh;

  /* advance to start-byte (LSB) */
  if (be)
    p += (pos + siz + 7) >> 3;
  else
    p += fldsiz - ((pos + siz + 7) >> 3);

  pos &= 7;
  n = (pos + siz + 7) >> 3;  /* number of bytes to write */

  sh = (8 - ((pos + siz) & 7)) & 7;
  m = 0xff << sh;  /* initial mask for LSB */
  d <<= sh;  /* shift value to match bitfield */

  while (n--) {
    if (n == 0)
      m &= (1 << (8 - pos)) - 1;  /* apply mask for MSB */

    if (be) {
      /* write right to left, for big-endian target */
      b = *(--p) & ~m;
      *p = b | ((uint8_t)d & m);
    }
    else {
      /* write left to right, for little-endian target */
      b = *p & ~m;
      *p++ = b | ((uint8_t)d & m);
    }
    d >>= 8;
    m = 0xff;
  }
}


lword readtbyte(struct GlobalVars *gv,void *src)
{
  uint8_t *s = src;
  int len;

  if ((len = gv->octets_per_tbyte) > 1) {
    lword val = 0;

    while (len--) {
      val <<= 8;
      val += (lword)*s++;
    }
    return val;
  }
  else
    return *s;
}


void writetbyte(struct GlobalVars *gv,void *dest,lword val)
{
  uint8_t *d = dest;
  int len;

  if ((len = gv->octets_per_tbyte) > 1) {
    d += len;
    while (len--) {
      *(--d) = (uint8_t)val;
      val >>= 8;
    }
  }
  else
    *d = (uint8_t)val;
}


void writetbytes(struct GlobalVars *gv,int n,void *dest,lword val)
{
  int be = gv->endianness != _LITTLE_ENDIAN_;
  uint8_t *d = dest;

  if (be)
    d += n * gv->octets_per_tbyte;  /* we start with LSB, so move behind it */

  while (n--) {
    if (be) {
      /* write right to left, for big-endian target */
      d -= gv->octets_per_tbyte;
      writetbyte(gv,d,val);
    }
    else {
      /* write left to right, for little-endian target */
      writetbyte(gv,d,val);
      d += gv->octets_per_tbyte;
    }
    val >>= gv->bits_per_tbyte;
  }
}


lword readreloc(struct GlobalVars *gv,void *src,int pos,int siz)
/* Read value from a relocation bitfield. The difference is that there is
   no known total field length, so pos/B always defines the offset to the
   first target byte, no matter if LE or BE (with B being the bits per byte).
   Then follow (pos%B+siz+(B-1))/B bytes read in LE or BE format.
   Note that the bits in a byte are counted from highest to lowest for BE
   and from lowest to highest for LE! */
{
  int be = gv->endianness != _LITTLE_ENDIAN_;
  int b = gv->bits_per_tbyte;
  uint8_t *p = src;
  lword d = 0;
  int n;

  /* advance to start-tbyte (MSB for BE, LSB for LE) */
  p += (pos / b) * gv->octets_per_tbyte;

  pos %= b;
  n = (pos + siz + b-1) / b;  /* number of tbytes to read */

  if (be) {
    while (n--) {
      d <<= b;
      d |= readtbyte(gv,p);
      p += gv->octets_per_tbyte;
    }
    /* normalize BE */
    d >>= (b - ((pos + siz) % b)) % b;
  }
  else {
    p += n * gv->octets_per_tbyte;
    while (n--) {
      d <<= b;
      p -= gv->octets_per_tbyte;
      d |= readtbyte(gv,p);
    }
    /* normalize LE */
    d >>= pos;
  }

  /* mask the extracted bitfield */
  return d & makemask(siz);
}


void writereloc(struct GlobalVars *gv,void *dst,int pos,int siz,lword d)
/* Write value to a relocation bitfield. The difference is that there is
   no known total field length, so pos/B always defines the offset to the
   first target byte, no matter if LE or BE (with B being the bits per byte).
   Then follow (pos%B+siz+(B-1))/B bytes written in LE or BE format.
   Note that the bits in a byte are counted from highest to lowest for BE
   and from lowest to highest for LE! */
{
  int be = gv->endianness != _LITTLE_ENDIAN_;
  int b = gv->bits_per_tbyte;
  lword tmask = makemask(b);
  uint8_t *p = dst;
  lword m;
  int n,sh;

  /* advance to start-tbyte (MSB for BE, LSB for LE) */
  p += (pos / b) * gv->octets_per_tbyte;

  pos %= b;
  n = (pos + siz + (b-1)) / b;  /* number of tbytes to write */

  if (be) {
    p += n * gv->octets_per_tbyte;  /* we start with LSB, so move behind it */
    sh = (b - ((pos + siz) % b)) % b;
  }
  else
    sh = pos;

  m = (tmask << sh) & tmask;  /* initial mask for LSB */
  d <<= sh;                   /* shift value to match bitfield */

  while (n--) {
    if (be) {
      /* write right to left, for big-endian target */
      if (n == 0)
        m &= (1LL << (b - pos)) - 1;  /* apply mask for MSB */
      p -= gv->octets_per_tbyte;
      writetbyte(gv,p,(readtbyte(gv,p)&~m)|(d&m));
    }
    else {
      /* write left to right, for little-endian target */
      if (n == 0)
        m &= (2LL << ((pos + siz - 1) % b)) - 1;  /* apply mask for MSB */
      writetbyte(gv,p,(readtbyte(gv,p)&~m)|(d&m));
      p += gv->octets_per_tbyte;
    }
    d >>= b;
    m = tmask;
  }
}


void fwritex(FILE *fp,const void *buf,size_t len)
/* write a buffer of len bytes, with check for len=0 and write error */
{
  if (len) {
    if (!fwrite(buf,1,len,fp)) {
      fclose(fp);
      error(31,gvars.dest_name);  /* write error */
    }
  }
}


void fwrite32be(FILE *fp,uint32_t w)
/* write a big endian 32 bit word */
{
  uint8_t be[4];

  be[0] = (w>>24) & 0xff;
  be[1] = (w>>16) & 0xff;
  be[2] = (w>>8) & 0xff;
  be[3] = w & 0xff;
  fwritex(fp,be,4);
}


void fwrite16be(FILE *fp,uint16_t w)
/* write a big endian 16 bit word */
{
  uint8_t be[2];

  be[0] = (w>>8) & 0xff;
  be[1] = w & 0xff;
  fwritex(fp,be,2);
}


void fwrite32le(FILE *fp,uint32_t w)
/* write a little endian 32 bit word */
{
  uint8_t le[4];

  le[0] = w & 0xff;
  le[1] = (w>>8) & 0xff;
  le[2] = (w>>16) & 0xff;
  le[3] = (w>>24) & 0xff;
  fwritex(fp,le,4);
}


void fwrite16le(FILE *fp,uint16_t w)
/* write a little endian 16 bit word */
{
  uint8_t le[2];

  le[0] = w & 0xff;
  le[1] = (w>>8) & 0xff;
  fwritex(fp,le,2);
}


void fwrite8(FILE *fp,uint8_t w)
/* write a byte */
{
  fwritex(fp,&w,1);
}


void fwrite16(int be,FILE *fp,uint16_t w)
{
  if (be)
    fwrite16be(fp,w);
  else
    fwrite16le(fp,w);
}


void fwrite32(int be,FILE *fp,uint32_t w)
{
  if (be)
    fwrite32be(fp,w);
  else
    fwrite32le(fp,w);
}


void fwritetbyte(struct GlobalVars *gv,FILE *fp,lword w)
/* write a target-byte */
{
  int o = gv->octets_per_tbyte;
  uint8_t buf[16];

#if 0
  if (o > 16*sizeof(uint8_t))
    ierror("fwritetbyte(): octets per tbyte > 16 (%d)",o);
#endif
  if (gv->output_le) {
    int i;

    for (i=0; i<o; i++) {
      buf[i] = (uint8_t)w;
      w >>= 8;
    }
  }
  else {
    while (o--) {
      buf[o] = (uint8_t)w;
      w >>= 8;
    }
    o = gv->octets_per_tbyte;
  }
  fwritex(fp,buf,o);
}


void fwritetaddr(struct GlobalVars *gv,FILE *fp,lword d)
/* write a target address to file */
{
  char buf[16];
  size_t len;

  if (len = (size_t)writetaddr(gv,buf,0,d))
    fwritex(fp,buf,len);
}


void fwrite_align(struct GlobalVars *gv,FILE *fp,uint32_t a,uint32_t n)
/* writes as many zero bytes as required for alignment a (a bits */
/* must be zero) with current file offset n */
{
  static uint8_t alignment_bytes[MAX_FWALIGN];

  a = 1<<a;
  n = tbytes(gv, a - (n & (a-1)) & (a-1));
  if (n > MAX_FWALIGN)
    ierror("fwrite_align(): Alignment > %d required",MAX_FWALIGN);
  fwritex(fp,alignment_bytes,n);
}


void fwritegap(struct GlobalVars *gv,FILE *f,long bytes,lword fill)
{
  uint8_t buf[GAPBUFSIZE];
  size_t len = gv->octets_per_tbyte;  /* @@@ never larger than GAPBUFSIZE! */

  if (fill==0 || len==1) {
    bytes = tbytes(gv,bytes);
    memset(buf,(uint8_t)fill,GAPBUFSIZE);
    do
      fwritex(f,buf,bytes>GAPBUFSIZE?GAPBUFSIZE:bytes);
    while ((bytes-=GAPBUFSIZE) > 0);
  }
  else {
    writetbyte(gv,buf,fill);
    while (bytes-- > 0)
      fwritex(f,buf,len);
  }
}


void fwriterawsect(struct GlobalVars *gv,FILE *fp,struct LinkedSection *ls)
{
  if (gv->bits_per_tbyte>8 && gv->output_le) {
    /* write the octets inside a target-byte in little-endian order */
    size_t sz = ls->filesize;
    uint8_t *p = ls->data;

    while (sz--) {
      fwritetbyte(gv,fp,readtbyte(gv,p));
      p += gv->octets_per_tbyte;
    }
  }
  else  /* target-bytes are available in big-endian order by default */
    fwritex(fp,ls->data,tbytes(gv,ls->filesize));
}


void fwritefullsect(struct GlobalVars *gv,FILE *fp,struct LinkedSection *ls)
/* write section contents and uninitialized part as zero */
{
  if (ls != NULL) {
    fwriterawsect(gv,fp,ls);
    if (ls->filesize < ls->size)
      fwritegap(gv,fp,ls->size - ls->filesize,0);
  }
}


unsigned long elf_hash(const char *_name)
/* calculate a hash code as used in ELF objects */
{
  const unsigned char *name=(const unsigned char *)_name;
  unsigned long h=0,g;

  while (*name) {
    h = (h << 4) + *name++;
    if (g = h & 0xf0000000)
      h ^= g >> 24;
    h &= ~g;
  }
  return h;
}


unsigned long align(lword addr,unsigned long alignment)
/* return number of bytes required to achieve alignment */
{
  lword a = (1<<alignment) - 1;

  return ((addr+a)&~a) - addr;
}


unsigned long comalign(lword addr,lword a)
/* return number of bytes required to achieve alignment */
{
  return ((addr+a-1)&~(a-1)) - addr;
}


int shiftcnt(uint32_t x)
/* returns number of 0-bits before the first 1-bit - something like */
/* an integer-log2() function - returns 0 on x=0 */
{
  int i;

  if (x == 0)
    return 0;
  for (i=0; i<32; i++) {
    if (x & 1)
      break;
    x >>= 1;
  }
  return i;
}


int lshiftcnt(lword x)
/* shiftcnt() of an lword */
{
  int i,n=sizeof(lword)<<3;

  if (x == 0)
    return 0;
  for (i=0; i<n; i++) {
    if (x & 1)
      break;
    x >>= 1;
  }
  return i;
}


int highest_bit_set(lword x)
/* return number of highest bit set */
{
  int i,h=-1,n=sizeof(lword)<<3;

  for (i=0; i<n; i++) {
    if (x & 1)
      h = i;
    x >>= 1;
  }
  return h;
}


lword sign_extend(lword v,int n)
/* sign-extend an n-bit value to lword-size */
{
  if (v & (1LL<<(n-1)))
    v |= ~makemask(n);

  return v;
}


void add_symnames(struct SymNames **snlist,const char *name,lword val)
/* add a new name to a SymNames list */
{
  struct SymNames *new = alloc(sizeof(struct SymNames));
  struct SymNames *sn;

  new->next = NULL;
  new->name = name;
  new->value = val;
  if (sn = *snlist) {
    while (sn->next)
      sn = sn->next;
    sn->next = new;
  }
  else
    *snlist = new;
}

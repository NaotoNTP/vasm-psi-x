/* elf_reloc_z80.h ELF relocation types for Zilog Z80 */
/* (c) in 2025 by 'Naoto' */

#define R_Z80_NONE 0      /* No reloc */
#define R_Z80_8 1         /* Direct 8 bit */
#define R_Z80_8_DIS 2     /* Index Regsister Displacement 8 bit */
#define R_Z80_8_PCREL 3		/* PC relative 8 bit */
#define R_Z80_16 4        /* Direct 16 bit */
#define R_Z80_24 5        /* Direct 24 bit */
#define R_Z80_32 6        /* Direct 32 bit */

/*
#define R_Z80_BYTE0 7
#define R_Z80_BYTE1 8
#define R_Z80_BYTE2 9
#define R_Z80_BYTE3 10
#define R_Z80_WORD0 11
#define R_Z80_WORD1 12
#define R_Z80_16_BE 13
*/

  if (is_std_reloc(*rl)) {
    nreloc *r = (nreloc *)(*rl)->reloc;

    *refsym = r->sym;
    *addend = r->addend;
    pos = r->bitoffset;
    size = r->size;
    *roffset = r->byteoffset;
    mask = r->mask;

    switch (STD_REL_TYPE((*rl)->type)) {

      case REL_NONE:
        t = R_Z80_NONE;
        break;

      case REL_ABS:
        if (pos==0 && mask==~0) {
          if (size == 32)
            t = R_Z80_32;
          else if (size == 24)
            t = R_Z80_24;
          else if (size == 16)
            t = R_Z80_16;
          else if (size == 8)
            t = R_Z80_8;
        }
        break;

      case REL_PC:
        if (pos==0 && mask==~0) {
          if (size == 8)
            t = R_Z80_8_PCREL;
        }
        break;

    }
  }

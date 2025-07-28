/* $VER: vlink rel_elfjag.h V0.15a (22.01.15)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2015  Frank Wille
 */


#ifndef REL_ELFJAG_H
#define REL_ELFJAG_H

#define R_JAG_NONE 0      /* No reloc */
#define R_JAG_ABS32 1     /* Direct 32 bit */
#define R_JAG_ABS16 2     /* Direct 16 bit */
#define R_JAG_ABS8 3      /* Direct 8 bit */
#define R_JAG_REL32 4     /* PC relative 32 bit */
#define R_JAG_REL16 5     /* PC relative 16 bit */
#define R_JAG_REL8 6      /* PC relative 8 bit */
#define R_JAG_ABS5 7      /* Direct 5 bit */
#define R_JAG_REL5 8      /* PC relative 5 bit */
#define R_JAG_JR 9        /* PC relative branch (distance / 2), 5 bit */
#define R_JAG_ABS32SWP 10 /* 32 bit direct, halfwords swapped as in MOVEI */
#define R_JAG_REL32SWP 11 /* 32 bit PC rel., halfwords swapped as in MOVEI */

#endif

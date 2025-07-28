/* $VER: vlink config.h V0.18 (29.11.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2024  Frank Wille
 */

#ifndef CONFIG_H
#define CONFIG_H


/* Edit the following defines for your system: */

/* Default path to search for library. Example: "/usr/lib" */
#ifdef __VBCC__
#if defined(__MORPHOS__)
#define LIBPATH "vlibmos:"
#elif defined(__amigaos4__)
#define LIBPATH "vlibos4:"
#elif defined(__AROS__)
#define LIBPATH "vlibaros:"
#elif defined(AMIGAOS)
#define LIBPATH "vlibos3:"
#endif
#endif

/* Default target file format. Example: "elf32ppcbe" */
#if defined(__MORPHOS__)
#define DEFTARGET "elf32morphos"
#elif defined(__amigaos4__)
#define DEFTARGET "elf32ppcbe"
#elif defined(__AROS__)
#define DEFTARGET "elf32aros"
#elif defined(AMIGAOS)
#define DEFTARGET "amigahunk"
#elif defined(__MINT__)
#define DEFTARGET "aoutmint"
#elif defined(atarist)
#define DEFTARGET "ataritos"
#endif

/* Targets to be included */
#define ADOS                /* AmigaOS 68k hunk format */
#define EHF                 /* WarpOS PPC extended hunk format */
#define ATARI_TOS           /* Atari-ST TOS format */
#define XFILE               /* Sharp X68000 Human68k XFile format */
#define OS_9                /* OS-9 6809 reentrant modules */
#define O65                 /* o65 6502/65816 object/executable file format */
#define APPLE_OMF           /* Apple 65816 OMF */

#define ELF32               /* general 32-bit ELF support */
#define ELF32_PPC_BE        /* ELF PowerPC 32-Bit Big Endian */
#define ELF32_AMIGA         /* ELF PPC relocatable for MorphOS/PowerUp */
#define ELF32_M68K          /* ELF M68k 32-Bit Big Endian */
#define ELF32_386           /* ELF 386 32-Bit Little Endian */
#define ELF32_AROS          /* ELF 386 relocatable for AROS */
#define ELF32_ARM_LE        /* ELF ARM 32-Bit Little Endian */
#define ELF32_JAG           /* ELF Jaguar RISC 32-Bit Big Endian */

#define ELF64               /* general 64-bit ELF support */
#define ELF64_X86           /* ELF x86_64 64-Bit Little Endian */

#define AOUT                /* general a.out support */
#define AOUT_NULL           /* a.out stdandard relocs, undefined endianness */
#define AOUT_SUN010         /* a.out SunOS 68000/010 */
#define AOUT_SUN020         /* a.out SunOS 68020+ */
#define AOUT_BSDM68K        /* a.out NetBSD M68k (68020+) 8k Pages */
#define AOUT_BSDM68K4K      /* a.out NetBSD M68k (68020+) 4k Pages */
#define AOUT_MINT           /* a.out Atari MiNT 680x0, with TOS header */
#define AOUT_JAGUAR         /* a.out Atari Jaguar (M68k+RISC, write-only) */
#define AOUT_BSDI386        /* a.out NetBSD i386 (486,Pentium) 4k Pages */
#define AOUT_PC386          /* a.out PC i386 (GNU MS-DOS?) */

#define AMSDOS              /* Amstrad/Schneider CPC program */
#define APPLEBIN            /* Apple DOS 3.3 binary file */
#define ATARICOM            /* Atari DOS COM format */
#define BBC                 /* BBC binary with info file */
#define CBMPRG              /* Commodore PET, VIC-20, 64, etc. program */
#define COCOML              /* Tandy Color Computer machine lang. file */
#define DRAGONBIN           /* Dragon DOS binary format */
#define FOENIX              /* Foenix 65816 PGX and PGZ formats */
#define IHEX                /* Intel Hex */
#define JAGSRV              /* Jaguar Server (SkunkBoard, VirtualJaguar) */
#define ORICMC              /* ORIC machine code file header */
#define RAWBIN              /* raw binary file (single, multiple, coalesced) */
#define RAWSEG              /* multiple raw segment files */
#define SINCQL              /* Sinclair QL, QDOS header or XTcc trailer */
#define SHEX1               /* Customer specific hex format */
#define SREC19              /* Motorola S-Record 16-bit addresses */
#define SREC28              /* Motorola S-Record 24-bit addresses */
#define SREC37              /* Motorola S-Record 32-bit addresses */

#define VOBJ                /* vasm special object format */

/* dependencies */
#if defined(AOUT_MINT) && !defined(ATARI_TOS)
#define ATARI_TOS           /* a.out-MiNT format needs TOS */
#endif

#endif /* CONFIG_H */

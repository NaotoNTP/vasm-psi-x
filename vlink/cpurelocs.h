/* $VER: vlink cpurelocs.h V0.18 (23.01.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2024  Frank Wille
 */

/*
 * CPU specific relocation types.
 * Consists of a base, which represents a CPU id, and the VOBJ-compatible
 * type - 0x80.
 */

#define RELOC_CPU_ID(r) ((r)>>12) /* cpu-id of this reloc type, 0=standard */
#define RELOC_CPU_TYPE(r) ((r)&0xfff)
#define MAKE_RELOC_CPU_ID(i) ((i)<<12)

/* PowerPC (1) */
#define R_PPC MAKE_RELOC_CPU_ID(1)
enum {
  RPPC_SD2=R_PPC,
  RPPC_SD21,RPPC_SDI16,RPPC_SD2I16,RPPC_MOSDREL,RPPC_AOSBREL
};

/* make entries for new CPUs here */

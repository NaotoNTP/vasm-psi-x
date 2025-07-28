/* $VER: vlink tosopts.c V0.18 (31.05.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2024  Frank Wille
 */

#include "config.h"
#if defined(AOUT_MINT) || defined(ATARI_TOS)
#define TOSOPTS_C
#include "vlink.h"

uint32_t tos_flags;    /* flags field in TOS header */
bool textbasedsyms;    /* symbol offsets based on text section */
bool sozobonx;         /* SozobonX extension for unlimited symbol name length */
bool hisoftdri = TRUE; /* HiSoft symbol name extension */


int tos_options(struct GlobalVars *gv,int argc,const char *argv[],int *i)
{
  if (!strcmp(argv[*i],"-tos-flags")) {
    long fl;

    if (sscanf(get_arg(argc,argv,i),"%li",&fl) == 1)
      tos_flags = fl;
    else return 0;
  }
  else if (!strcmp(argv[*i],"-tos-fastload"))
    tos_flags |= 1;
  else if (!strcmp(argv[*i],"-tos-fastram"))
    tos_flags |= 2;
  else if (!strcmp(argv[*i],"-tos-fastalloc"))
    tos_flags |= 4;
  else if (!strcmp(argv[*i],"-tos-private"))
    tos_flags &= ~0x30;
  else if (!strcmp(argv[*i],"-tos-global"))
    tos_flags |= 0x10;
  else if (!strcmp(argv[*i],"-tos-super"))
    tos_flags |= 0x20;
  else if (!strcmp(argv[*i],"-tos-readable"))
    tos_flags |= 0x30;
  else if (!strcmp(argv[*i],"-tos-textbased"))
    textbasedsyms = TRUE;
  else if (!strcmp(argv[*i],"-tos-stddri"))
    hisoftdri = FALSE;
  else if (!strcmp(argv[*i],"-tos-sozobonx"))
    sozobonx = TRUE;
  else
    return 0;
  return 1;
}


void tos_printhelp(void)
{
  printf("-tos-flags <val>  TOS header flags (32 bits)\n"
         "-tos-fastload     Set the fastload bit in the TOS header\n"
         "-tos-fastram      Set the fastram bit in the TOS header\n"
         "-tos-fastalloc    Set the fastalloc bit in the TOS header\n"
         "-tos-private      Mark memory space as private in the TOS header\n"
         "-tos-global       Mark memory space as global in the TOS header\n"
         "-tos-super        Mark memory as supervisor-only in the TOS header\n"
         "-tos-readable     Mark memory as read-only in the TOS header\n"
         "-tos-sozobonx     Write Sozobon symbol extension (disables HiSoft)\n"
         "-tos-stddri       Write standard DRI symbols (disables HiSoft)\n"
         "-tos-textbased    Write Devpac(MonST)-compatible DRI symbols\n");
}

#endif

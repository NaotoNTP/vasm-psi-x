/* $VER: vlink version.c V0.18 (24.02.24)
 *
 * This file is part of vlink, a portable linker for multiple
 * object formats.
 * Copyright (c) 1997-2024  Frank Wille
 */


/* version/revision */
#define VERSION "0.18"

#define VERSION_C
#include "vlink.h"


#ifdef AMIGAOS
static const char *_ver = "$VER: " PNAME " " VERSION " " __AMIGADATE__ "\r\n";
#endif
const char *version_str = VERSION;



void show_version(void)
{
  printf(PNAME " V%s (c)1997-2024 by Frank Wille\n"
         "build date: " __DATE__ ", " __TIME__ "\n\n",version_str);
}


void show_usage(struct GlobalVars *gv,int verbose)
{
  show_version();

  printf("Usage: " PNAME " [-dhkmnqrstvwxMSXZ] [-B linkmode] [-b targetname] "
         "[-baseoff offset] [-C constructor-type] [-Crel] "
#if 0 /* not implemented */
         "[-D symbol[=value]] "
#endif
         "[-da] [-dc] [-dp] [-EB] [-EL] [-e entrypoint] [-export-dynamic] "
         "[-f flavour] [-fixunnamed] [-F filename] "
         "[-gc-all] [-gc-empty] "
         "[-hunkattr secname=value] [-interp path] [-minalign value] "
         "[-mrel] [-mtype] [-mall] [-multibase] [-nostdlib] "
         "[-N old new] [-o filename] [-osec] [-Rstd/add/short] "
         "[-os9-mem/name/rev] [-P symbol] "
         "[-rpath path] [-sc] [-sd] [-shared] [-soname name] "
         "[-static] [-symfile filename] [-symfmt format] [-symctrl=flags] "
         "[-T filename] [-Ttext addr] [-textbaserel] "
         "[-tos-flags/fastload/fastram/private/global/super/readable] "
         "[-u symbol] [-vicelabels filename] "
         "[-vobj2] [-vobjcpu name] "
         "[-V version] [-y symbol] "
         "input-files...[-L library-search-path] [-l library-specifier]\n\n");

  if (verbose) {
    printf("Options:\n"
           "<input-files>     object files and libraries to link\n"
           "-F<file>          read a list of input files from <file>\n"
           "-o<output>        output file name\n"
           "-b<target>        output file format\n"
           "-l<libspec>       link with specified library (static or dynamic)\n"
           "-L<libpath>       add search path for libraries\n"
           "-f<flavour>       add a library flavour\n"
           "-rpath <path>     add search path for dynamic linker\n"
           "-e<entrypoint>    address of program's entry point\n"
           "-interp <path>    set interpreter path (dynamic linker for ELF)\n"
           "-gc-all           garbage-collect all unreferenced sections\n"
           "-gc-empty         garbage-collect empty unreferenced sections\n"
           "-y<symbol>        trace symbol accesses by the linker\n"
           "-P<symbol>        protect symbol from stripping\n"
#if 0 /* not implemented */
           "-D<symbol>[=exp]  define a symbol\n"
#endif
           "-u<symbol>        mark a symbol as undefined\n"
           "-T<script>        use linker script for output file\n"
           "-Ttext <address>  define start address of first section\n"
           "-B<mode>          link mode: static, dynamic, shareable, symbolic\n"
           "-EB/-EL           set big-endian/little-endian mode\n"
           "-V<version>       minimum version of shared object\n"
           "-C<constr.type>   Set type of con-/destructors to scan for\n"
           "-Crel             Use relative con-/destructor function references\n"
           "-minalign <val>   Minimal section alignment (default 0)\n"
           "-baseoff <offset> offset for base relative relocations\n"
           "-N <old> <new>    Rename input sections\n"
           "-fixunnamed       unnamed sections are named according to their type\n"
           "-nostdlib         don't use default search path\n"
           "-multibase        don't auto-merge base-relative accessed sections\n"
           "-textbaserel      allow base-relative access on code sections\n"
           "-vicelabels       generate label mapping for the VICE debugger\n"
           "-shared           generate shared object\n"
           "-soname <name>    set real name of shared object\n"
           "-export-dynamic   export all global symbols as dynamic symbols\n"
           "-osec             output each section as an individual file\n"
           "-Rstd             standard relocation table\n"
           "-Radd             relocation table with addends\n"
           "-Rshort           relocation table with short offsets\n"
           "-Z                keep trailing zero-bytes in executables\n"
           "-d                force allocation of common symbols (also -dc,-dp)\n"
           "-da               force allocation of address symbols (PowerOpen)\n"
           "-sc               merge all code sections\n"
           "-sd               merge all data and bss sections\n"
           "-mrel             merge sections with pc-relative references\n"
           "-mtype            merge all sections with the same type\n"
           "-mall             merge all sections to a single output section\n"
           "-m                enable feature-mask in symbol names\n"
           "-M                print segment mappings and symbol values\n"
           "-k                keep original section order\n"
           "-n                no page alignment\n"
           "-q                keep relocations in the final executable\n"
           "-r                generate relocatable object\n"
           "-s                strip all symbols\n"
           "-S                strip debugging symbols only\n"
           "-t                trace file accesses by the linker\n"
           "-x                discard all local symbols\n"
           "-X                discard temporary local symbols\n"
           "-w                suppress warnings\n"
           "-v                print version and implemented targets\n"
           "-h                shows this help text\n");

    if (fff[gv->dest_format]->printhelp) {
      printf("\nTarget specific options:\n");
      fff[gv->dest_format]->printhelp();
    }
  }
  else
    printf("Use -h or refer to the documentation for a more detailed "
           "description.\n");
}

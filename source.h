/* source.h - source files, include paths and dependencies */
/* (c) in 2020,2022,2024 by Volker Barthelmann and Frank Wille */

#ifndef SOURCE_H
#define SOURCE_H

/* include paths */
struct include_path {
  struct include_path *next;
  char *path;
  int compdir_based;
};

/* source files */
struct source_file {
  struct source_file *next;
  struct include_path *incpath;
  int index;
  char *name;
  char *text;
  size_t size;
};

/* source texts (main file, include files or macros) */
struct source {
  struct source *parent;
  int parent_line;
  struct source_file *srcfile;
  char *name;
  char *text;
  size_t size;
  struct source *defsrc;
  int defline;
  int srcdebug;
  macro *macro;
  unsigned long repeat;
  int isloop;
  char *irpname;
  struct macarg *irpvals;
  int cond_level;
  struct macarg *argnames;
  int num_params;
  char *param[MAXMACPARAMS+1];
  int param_len[MAXMACPARAMS+1];
  struct macarg *varnames;
#if MAX_QUALIFIERS > 0
  int num_quals;
  char *qual[MAX_QUALIFIERS];
  int qual_len[MAX_QUALIFIERS];
#endif
  unsigned long id;
  char *srcptr;
  int line;
  size_t bufsize;
  char *linebuf;
#ifdef NARGSYM
  expr *nargexp;
#endif
#ifdef CARGSYM
  expr *cargexp;
#endif
#ifdef REPTNSYM
  long reptn;
#endif
  char *callname;
  char *callargs;
};

#define DEPEND_LIST     1
#define DEPEND_MAKE     2
struct deplist {
  struct deplist *next;
  char *filename;
};


extern char *compile_dir;
extern int ignore_multinc,relpath,nocompdir,depend,depend_all;

void write_depends(FILE *);
source *new_source(char *,struct source_file *,char *,size_t);
void end_source(source *);
source *stdin_source(void);
source *include_source(char *);
void include_binary_file(char *,size_t,size_t);
void source_debug_init(int,void *);
struct include_path *new_include_path(char *);
FILE *locate_file(char *,char *,struct include_path **,int);

#endif /* SOURCE_H */

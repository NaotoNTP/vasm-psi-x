/* cond.c - conditional assembly support routines */
/* (c) in 2015,2023 by Frank Wille */

#include "vasm.h"

int clev;  /* conditional level */

static signed char cond[MAXCONDLEV+1];
static char *condsrc[MAXCONDLEV+1];
static int condline[MAXCONDLEV+1];
static int condeval[MAXCONDLEV+1];
static int condtype[MAXCONDLEV+1];
static int ifnesting;


/* initialize conditional assembly */
void cond_init(void)
{
  cond[0] = 1;
  clev = ifnesting = 0;
}


/* return true, when current level allows assembling */
int cond_state(void)
{
  return cond[clev] > 0;
}


/* returns the current level's conditional block type */
int cond_type(void)
{
  return condtype[clev];
}


/* returns the stored expression result for the current level (switch statement) */
int cond_match(int val)
{
  return condeval[clev] == val;
}


/* ensures that all conditional block are closed at the end of the source */
void cond_check(void)
{
  if (clev > 0)
    general_error(66,condsrc[clev],condline[clev]);  /* "endc/endif missing */
}


/* establish a new level of conditional assembly (if statement) */
void cond_if(char flag)
{
  if (++clev >= MAXCONDLEV)
    general_error(65,clev);  /* nesting depth exceeded */

  cond[clev] = flag!=0;
  condsrc[clev] = cur_src->name;
  condline[clev] = cur_src->line;
  condeval[clev] = -1;
  condtype[clev] = 0;
}


/* handle skipped if statement */
void cond_skipif(void)
{
  ifnesting++;
}


/* handle else statement after skipped if-branch */
void cond_else(void)
{
  if (ifnesting == 0)
    cond[clev] = cond[clev] ? -1 : 1;
}


/* handle else statement after assembled if-branch */
void cond_skipelse(void)
{
  if (clev > 0)
    cond[clev] = -1;
  else
    general_error(63);  /* else without if */
}


/* handle else-if statement */
void cond_elseif(char flag)
{
  if (clev > 0) {
    if (!cond[clev])
      cond[clev] = flag!=0;
    else
      cond[clev] = -1;
  }
  else
    general_error(63);  /* else without if */
}


/* handle end-if statement */
void cond_endif(void)
{
  if (ifnesting == 0) {
    if (clev > 0)
      clev--;
    else
      general_error(64);  /* unexpected endif without if */
  }
  else  /* the whole conditional block was ignored */
    ifnesting--;
}


/* establish a new level of conditional assembly (switch statement) */
void cond_switch(int exprval)
{
  if (++clev >= MAXCONDLEV)
    general_error(65,clev);  /* nesting depth exceeded */

  cond[clev] = 0;
  condsrc[clev] = cur_src->name;
  condline[clev] = cur_src->line;
  condeval[clev] = exprval;
  condtype[clev] = 1;
}
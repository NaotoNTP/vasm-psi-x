/* cond.h - conditional assembly support routines */
/* (c) in 2015,2023 by Frank Wille */

#ifndef COND_H
#define COND_H

/* defines */
#ifndef MAXCONDLEV
#define MAXCONDLEV 63
#endif

/* global variables */
extern int clev;

/* functions */
void cond_init(void);
int cond_state(void);
int cond_type(void);
int cond_match(int);
void cond_check(void);
void cond_if(char);
void cond_skipif(void);
void cond_else(void);
void cond_skipelse(void);
void cond_elseif(char);
void cond_endif(void);
void cond_switch(int);

#endif /* COND_H */

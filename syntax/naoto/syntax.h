/* syntax.h  syntax header file for vasm */
/* (c) in 2015,2017 by Frank Wille */

/* macros to recognize identifiers */
int isidchar(char);
#define ISIDSTART(x) ((x)=='.'||(x)=='_'||isalpha((unsigned char)(x)))
#define ISIDCHAR(x) isidchar(x)
#define ISBADID(p,l) ((l)==1&&(*(p)=='.'||*(p)=='_'))
#define ISEOL(p) (*(p)=='\0' || *(p)==commentchar)

/* result of a boolean operation */
#define BOOLEAN(x) -(x)

/* we have a special skip() function for expressions, called exp_skip() */
char *exp_skip(char *);
#define EXPSKIP() s=exp_skip(s)

/* ignore operand field, when the instruction has no operands */
#define IGNORE_FIRST_EXTRA_OP 1

/* symbol which contains the current rept-endr iteration count */
#define REPTNSYM "REPTN"

/* overwrite macro defaults */
#define MAXMACPARAMS 64
char *my_skip_macro_arg(char *);
#define SKIP_MACRO_ARGNAME(p) my_skip_macro_arg(p)
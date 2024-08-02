/* syntax.h  syntax header file for vasm */
/* (c) in 2024 by 'Naoto' */

/* macros to recognize identifiers */
int isidchar(char);
#define ISIDSTART(x) ((x)=='.'||(x)=='@'||(x)=='_'||isalpha((unsigned char)(x)))
#define ISIDCHAR(x) isidchar(x)
#define ISBADID(p,l) ((l)==1&&(*(p)=='.'||*(p)=='@'||*(p)=='_'))
#define ISEOL(p) (*(p)=='\0' || *(p)==commentchar)
#ifdef VASM_CPU_M68K
char *chkidend(char *,char *);
#define CHKIDEND(s,e) chkidend((s),(e))
#endif

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
  "syntax error",ERROR,																											/* 0 */
  "invalid extension",ERROR,																								/* 1 */
  "no space before operands",WARNING,																				/* 2 */
  "too many closing parentheses",WARNING,																		/* 3 */
  "missing closing parentheses",WARNING,																		/* 4 */
  "missing operand",ERROR,																									/* 5 */
  "garbage at end of line",WARNING,																					/* 6 */
  "unexpected use of \"%s\" outside of macro",ERROR,												/* 7 */
  "invalid data operand",ERROR,																							/* 8 */
  "invalid radix base value (%d)",ERROR,																		/* 9 */
  "identifier expected",ERROR,																							/* 10 */
  "assembly failed",FATAL|ERROR,																						/* 11 */
  "unexpected \"%s\" without \"%s\"",ERROR,																	/* 12 */
  "\",\" expected",WARNING,																									/* 13 */
  "maximum module nesting depth exceeded (%d)",ERROR,												/* 14 */
  "invalid message severity",ERROR,																					/* 15 */

  "\nuser-defined message: %s",NOLINE|MESSAGE,															/* 16 */
  "\nuser-defined warning: %s",NOLINE|WARNING,															/* 17 */
  "\nuser-defined error: %s",NOLINE|ERROR,																	/* 18 */
  "\nuser-defined fatal error: %s",NOLINE|FATAL|ERROR,											/* 19 */

  "skipping instruction in struct init",WARNING,														/* 20 */
  "last %d bytes of string constant have been cut",WARNING,									/* 21 */
  "invalid numeric expansion",ERROR,																				/* 22 */
  "enclosed macro argument missing \"}\"",ERROR,														/* 23 */

  "invalid \"public\" parameter (must be either \"on\" or \"off\")",ERROR,					/* 24 */
  "invalid data size value (%d bytes)",ERROR,																				/* 25 */
  "local macro variable <%s> already declared",WARNING,															/* 26 */
	"string symbol <%s> not found",ERROR,																							/* 27 */
	"invalid string symbol assignment",ERROR,																					/* 28 */
	"substring index must be positive",ERROR,																					/* 29 */
	"substring ending index cannot be greater than the starting index",ERROR,					/* 30 */
	"substring ending index cannot be greater than the lenght of the string",ERROR,		/* 31 */
# Atari TOS/MiNT

TARGET = vlink_mint
DIR = MiNT
MD = makedir
RM = rm -f

CC = vc +mint
CCOUT = -o $(DUMMYVARIABLE)	# produces the string "-o "
CFLAGS = -DATARI -c99 -cpu=68020 -c -O1
CONFIG =

LD = vc +mint
LDOUT = -o $(DUMMYVARIABLE)	# produces the string "-o "
LDOPTS =
LIBS =


include make.rules

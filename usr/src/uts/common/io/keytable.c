/*
 * Copyright (C) 1987-1997 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ident	"@(#)keytables.c	1.35	97/11/22 SMI"
		/* SunOS-4.0 1.19	*/

/*
 * This module contains the translation tables for the up-down encoded
 * Sun keyboards.
 */

#include <sys/param.h>
#include <sys/kbd.h>

/* handy way to define control characters in the tables */
#define	c(char)	(char&0x1F)
#define	ESC 0x1B
#define	DEL 0x7F


/* Unshifted keyboard table for Type 3 keyboard */

static struct keymap keytab_s3_lc = {
/*  0 */	HOLE,	BUCKYBITS+SYSTEMBIT,
				HOLE,	LF(2),	HOLE,	TF(1),	TF(2),	HOLE,
/*  8 */	TF(3), 	HOLE,	TF(4),	HOLE,	TF(5),	HOLE,	TF(6),	HOLE,
/* 16 */	TF(7),	TF(8),	TF(9),	SHIFTKEYS+ALT,
						HOLE,	RF(1),	RF(2),	RF(3),
/* 24 */	HOLE, 	LF(3), 	LF(4),	HOLE,	HOLE,	ESC,	'1',	'2',
/* 32 */	'3',	'4',	'5',	'6',	'7',	'8',	'9',	'0',
/* 40 */	'-',	'=',	'`',	'\b',	HOLE,	RF(4),	RF(5),	RF(6),
/* 48 */	HOLE,	LF(5),	HOLE,	LF(6),	HOLE,	'\t',	'q',	'w',
/* 56 */	'e',	'r',	't',	'y',	'u',	'i',	'o',	'p',
/* 64 */	'[',	']',	DEL,	HOLE,	RF(7),	STRING+UPARROW,
								RF(9),	HOLE,
/* 72 */	LF(7),	LF(8),	HOLE,	HOLE,	SHIFTKEYS+LEFTCTRL,
							'a', 	's',	'd',
/* 80 */	'f',	'g',	'h',	'j',	'k',	'l',	';',	'\'',
/* 88 */	'\\',	'\r',	HOLE,	STRING+LEFTARROW,
						RF(11),	STRING+RIGHTARROW,
								HOLE,	LF(9),
/* 96 */	HOLE,	LF(10),	HOLE,	SHIFTKEYS+LEFTSHIFT,
						'z',	'x',	'c',	'v',
/*104 */	'b',	'n',	'm',	',',	'.',	'/',	SHIFTKEYS+RIGHTSHIFT,
									'\n',
/*112 */	RF(13),	STRING+DOWNARROW,
				RF(15),	HOLE,	HOLE,	HOLE,	HOLE,	SHIFTKEYS+CAPSLOCK,
/*120 */	BUCKYBITS+METABIT,
			' ',	BUCKYBITS+METABIT,
					HOLE,	HOLE,	HOLE,	ERROR,	IDLE,
};

/* Shifted keyboard table for Type 3 keyboard */

static struct keymap keytab_s3_uc = {
/*  0 */	HOLE,	BUCKYBITS+SYSTEMBIT,
				HOLE,	LF(2),	HOLE,	TF(1),	TF(2),	HOLE,
/*  8 */	TF(3), 	HOLE,	TF(4),	HOLE,	TF(5),	HOLE,	TF(6),	HOLE,
/* 16 */	TF(7),	TF(8),	TF(9),	SHIFTKEYS+ALT,
						HOLE,	RF(1),	RF(2),	RF(3),
/* 24 */	HOLE, 	LF(3), 	LF(4),	HOLE,	HOLE,	ESC,	'!',    '@',
/* 32 */	'#',	'$',	'%',	'^',	'&',	'*',	'(',	')',
/* 40 */	'_',	'+',	'~',	'\b',	HOLE,	RF(4),	RF(5),	RF(6),
/* 48 */	HOLE,	LF(5),	HOLE,	LF(6),	HOLE,	'\t',	'Q',	'W',
/* 56 */	'E',	'R',	'T',	'Y',	'U',	'I',	'O',	'P',
/* 64 */	'{',	'}',	DEL,	HOLE,	RF(7),	STRING+UPARROW,
								RF(9),	HOLE,
/* 72 */	LF(7),	LF(8),	HOLE,	HOLE,	SHIFTKEYS+LEFTCTRL,
							'A', 	'S',	'D',
/* 80 */	'F',	'G',	'H',	'J',	'K',	'L',	':',	'"',
/* 88 */	'|',	'\r',	HOLE,	STRING+LEFTARROW,
						RF(11),	STRING+RIGHTARROW,
								HOLE,	LF(9),
/* 96 */	HOLE,	LF(10),	HOLE,	SHIFTKEYS+LEFTSHIFT,
						'Z',	'X',	'C',	'V',
/*104 */	'B',	'N',	'M',	'<',	'>',	'?',	SHIFTKEYS+RIGHTSHIFT,
									'\n',
/*112 */	RF(13),	STRING+DOWNARROW,
				RF(15),	HOLE,	HOLE,	HOLE,	HOLE,	SHIFTKEYS+CAPSLOCK,
/*120 */	BUCKYBITS+METABIT,
			' ',	BUCKYBITS+METABIT,
					HOLE,	HOLE,	HOLE,	ERROR,	IDLE,
};

/* Caps Locked keyboard table for Type 3 keyboard */

static struct keymap keytab_s3_cl = {
/*  0 */	HOLE,	BUCKYBITS+SYSTEMBIT,
				HOLE,	LF(2),	HOLE,	TF(1),	TF(2),	HOLE,
/*  8 */	TF(3), 	HOLE,	TF(4),	HOLE,	TF(5),	HOLE,	TF(6),	HOLE,
/* 16 */	TF(7),	TF(8),	TF(9),	SHIFTKEYS+ALT,
						HOLE,	RF(1),	RF(2),	RF(3),
/* 24 */	HOLE, 	LF(3), 	LF(4),	HOLE,	HOLE,	ESC,	'1',	'2',
/* 32 */	'3',	'4',	'5',	'6',	'7',	'8',	'9',	'0',
/* 40 */	'-',	'=',	'`',	'\b',	HOLE,	RF(4),	RF(5),	RF(6),
/* 48 */	HOLE,	LF(5),	HOLE,	LF(6),	HOLE,	'\t',	'Q',	'W',
/* 56 */	'E',	'R',	'T',	'Y',	'U',	'I',	'O',	'P',
/* 64 */	'[',	']',	DEL,	HOLE,	RF(7),	STRING+UPARROW,
								RF(9),	HOLE,
/* 72 */	LF(7),	LF(8),	HOLE,	HOLE,	SHIFTKEYS+LEFTCTRL,
							'A', 	'S',	'D',
/* 80 */	'F',	'G',	'H',	'J',	'K',	'L',	';',	'\'',
/* 88 */	'\\',	'\r',	HOLE,	STRING+LEFTARROW,
						RF(11),	STRING+RIGHTARROW,
								HOLE,	LF(9),
/* 96 */	HOLE,	LF(10),	HOLE,	SHIFTKEYS+LEFTSHIFT,
						'Z',	'X',	'C',	'V',
/*104 */	'B',	'N',	'M',	',',	'.',	'/',	SHIFTKEYS+RIGHTSHIFT,
									'\n',
/*112 */	RF(13),	STRING+DOWNARROW,
				RF(15),	HOLE,	HOLE,	HOLE,	HOLE,	SHIFTKEYS+CAPSLOCK,
/*120 */	BUCKYBITS+METABIT,
			' ',	BUCKYBITS+METABIT,
					HOLE,	HOLE,	HOLE,	ERROR,	IDLE,
};

/* Controlled keyboard table for Type 3 keyboard */

static struct keymap keytab_s3_ct = {
/*  0 */	HOLE,	BUCKYBITS+SYSTEMBIT,
				HOLE,	LF(2),	HOLE,	TF(1),	TF(2),	HOLE,
/*  8 */	TF(3), 	HOLE,	TF(4),	HOLE,	TF(5),	HOLE,	TF(6),	HOLE,
/* 16 */	TF(7),	TF(8),	TF(9),	SHIFTKEYS+ALT,
						HOLE,	RF(1),	RF(2),	RF(3),
/* 24 */	HOLE, 	LF(3), 	LF(4),	HOLE,	HOLE,	ESC,	'1',	c('@'),
/* 32 */	'3',	'4',	'5',	c('^'),	'7',	'8',	'9',	'0',
/* 40 */	c('_'),	'=',	c('^'),	'\b',	HOLE,	RF(4),	RF(5),	RF(6),
/* 48 */	HOLE,	LF(5),	HOLE,	LF(6),	HOLE,	'\t',   c('q'),	c('w'),
/* 56 */	c('e'),	c('r'),	c('t'),	c('y'),	c('u'),	c('i'),	c('o'),	c('p'),
/* 64 */	c('['),	c(']'),	DEL,	HOLE,	RF(7),	STRING+UPARROW,
								RF(9),	HOLE,
/* 72 */	LF(7),	LF(8),	HOLE,	HOLE,	SHIFTKEYS+LEFTCTRL,
							c('a'),	c('s'),	c('d'),
/* 80 */	c('f'),	c('g'),	c('h'),	c('j'),	c('k'),	c('l'),	';',	'\'',
/* 88 */	c('\\'),
			'\r',	HOLE,	STRING+LEFTARROW,
						RF(11),	STRING+RIGHTARROW,
								HOLE,	LF(9),
/* 96 */	HOLE,	LF(10),	HOLE,	SHIFTKEYS+LEFTSHIFT,
						c('z'),	c('x'),	c('c'),	c('v'),
/*104 */	c('b'),	c('n'),	c('m'),	',',	'.',	c('_'),	SHIFTKEYS+RIGHTSHIFT,
									'\n',
/*112 */	RF(13),	STRING+DOWNARROW,
				RF(15),	HOLE,	HOLE,	HOLE,	HOLE,	SHIFTKEYS+CAPSLOCK,
/*120 */	BUCKYBITS+METABIT,
			c(' '),	BUCKYBITS+METABIT,
					HOLE,	HOLE,	HOLE,	ERROR,	IDLE,
};

/* "Key Up" keyboard table for Type 3 keyboard */

static struct keymap keytab_s3_up = {
/*  0 */	HOLE,	BUCKYBITS+SYSTEMBIT,
				HOLE,	NOP,	HOLE,	NOP,	NOP,	HOLE,
/*  8 */	NOP, 	HOLE, 	NOP,	HOLE,	NOP,	HOLE,	NOP,	HOLE,
/* 16 */	NOP, 	NOP, 	NOP,	SHIFTKEYS+ALT,
						HOLE,	NOP,	NOP,	NOP,
/* 24 */	HOLE, 	NOP, 	NOP,	HOLE,	HOLE,	NOP,	NOP,	NOP,
/* 32 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 40 */	NOP,	NOP,	NOP,	NOP,	HOLE,	NOP,	NOP,	NOP,
/* 48 */	HOLE,	NOP,	HOLE,	NOP,	HOLE,	NOP,	NOP,	NOP,
/* 56 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 64 */	NOP,	NOP,	NOP,	HOLE,	NOP,	NOP,	NOP,	HOLE,
/* 72 */	NOP,	NOP,	HOLE,	HOLE,	SHIFTKEYS+LEFTCTRL,
							NOP, 	NOP,	NOP,
/* 80 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 88 */	NOP,	NOP,	HOLE,	NOP,	NOP,	NOP,	HOLE,	NOP,
/* 96 */	HOLE,	NOP,	HOLE,	SHIFTKEYS+LEFTSHIFT,
						NOP,	NOP,	NOP,	NOP,
/*104 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	SHIFTKEYS+RIGHTSHIFT,
									NOP,
/*112 */	NOP,	NOP,	NOP,	HOLE,	HOLE,	HOLE,	HOLE,	NOP,
/*120 */	BUCKYBITS+METABIT,
			NOP,	BUCKYBITS+METABIT,
					HOLE,	HOLE,	HOLE,	HOLE,	RESET,
};

/* Index to keymaps for Type 3 keyboard */
static struct keyboard keyindex_s3 = {
	&keytab_s3_lc,
	&keytab_s3_uc,
	&keytab_s3_cl,
	0,		/* no Alt Graph key, no Alt Graph table */
	0,		/* no Num Lock key, no Num Lock table */
	&keytab_s3_ct,
	&keytab_s3_up,
	0x0000,		/* Shift bits which stay on with idle keyboard */
	0x0000,		/* Bucky bits which stay on with idle keyboard */
	1,	77,	/* abort keys */
	CAPSMASK,	/* Shift bits which toggle on down event */
};


/* Unshifted keyboard table for Type 4 keyboard */

static struct keymap keytab_s4_lc = {
/*  0 */	HOLE,	BUCKYBITS+SYSTEMBIT,
				HOLE,	LF(2),	HOLE,	TF(1),	TF(2),	TF(10),
/*  8 */	TF(3), 	TF(11),	TF(4),	TF(12),	TF(5),	SHIFTKEYS+ALTGRAPH,
								TF(6),	HOLE,
/* 16 */	TF(7),	TF(8),	TF(9),	SHIFTKEYS+ALT,
						HOLE,	RF(1),	RF(2),	RF(3),
/* 24 */	HOLE, 	LF(3), 	LF(4),	HOLE,	HOLE,	ESC,	'1',	'2',
/* 32 */	'3',	'4',	'5',	'6',	'7',	'8',	'9',	'0',
/* 40 */	'-',	'=',	'`',	'\b',	HOLE,	RF(4),	RF(5),	RF(6),
/* 48 */	BF(13),	LF(5),	BF(10),	LF(6),	HOLE,	'\t',	'q',	'w',
/* 56 */	'e',	'r',	't',	'y',	'u',	'i',	'o',	'p',
/* 64 */	'[',	']',	DEL,	COMPOSE,
						RF(7),	STRING+UPARROW,
								RF(9),	BF(15),
/* 72 */	LF(7),	LF(8),	HOLE,	HOLE,	SHIFTKEYS+LEFTCTRL,
							'a', 	's',	'd',
/* 80 */	'f',	'g',	'h',	'j',	'k',	'l',	';',	'\'',
/* 88 */	'\\',	'\r',	BF(11),	STRING+LEFTARROW,
						RF(11),	STRING+RIGHTARROW,
								BF(8),	LF(9),
/* 96 */	HOLE,	LF(10),	SHIFTKEYS+NUMLOCK,
					SHIFTKEYS+LEFTSHIFT,
						'z',	'x',	'c',	'v',
/*104 */	'b',	'n',	'm',	',',	'.',	'/',	SHIFTKEYS+RIGHTSHIFT,
									'\n',
/*112 */	RF(13),	STRING+DOWNARROW,
				RF(15),	HOLE,	HOLE,	HOLE,	LF(16),	SHIFTKEYS+CAPSLOCK,
/*120 */	BUCKYBITS+METABIT,
			' ',	BUCKYBITS+METABIT,
					HOLE,	HOLE,	BF(14),	ERROR,	IDLE,
};

/* Shifted keyboard table for Type 4 keyboard */

static struct keymap keytab_s4_uc = {
/*  0 */	HOLE,	BUCKYBITS+SYSTEMBIT,
				HOLE,	LF(2),	HOLE,	TF(1),	TF(2),	TF(10),
/*  8 */	TF(3), 	TF(11),	TF(4),	TF(12),	TF(5),	SHIFTKEYS+ALTGRAPH,
								TF(6),	HOLE,
/* 16 */	TF(7),	TF(8),	TF(9),	SHIFTKEYS+ALT,
						HOLE,	RF(1),	RF(2),	RF(3),
/* 24 */	HOLE, 	LF(3), 	LF(4),	HOLE,	HOLE,	ESC,	'!',    '@',
/* 32 */	'#',	'$',	'%',	'^',	'&',	'*',	'(',	')',
/* 40 */	'_',	'+',	'~',	'\b',	HOLE,	RF(4),	RF(5),	RF(6),
/* 48 */	BF(13),	LF(5),	BF(10),	LF(6),	HOLE,	'\t',	'Q',	'W',
/* 56 */	'E',	'R',	'T',	'Y',	'U',	'I',	'O',	'P',
/* 64 */	'{',	'}',	DEL,	COMPOSE,
						RF(7),	STRING+UPARROW,
								RF(9),	BF(15),
/* 72 */	LF(7),	LF(8),	HOLE,	HOLE,	SHIFTKEYS+LEFTCTRL,
							'A', 	'S',	'D',
/* 80 */	'F',	'G',	'H',	'J',	'K',	'L',	':',	'"',
/* 88 */	'|',	'\r',	BF(11),	STRING+LEFTARROW,
						RF(11),	STRING+RIGHTARROW,
								BF(8),	LF(9),
/* 96 */	HOLE,	LF(10),	SHIFTKEYS+NUMLOCK,
					SHIFTKEYS+LEFTSHIFT,
						'Z',	'X',	'C',	'V',
/*104 */	'B',	'N',	'M',	'<',	'>',	'?',	SHIFTKEYS+RIGHTSHIFT,
									'\n',
/*112 */	RF(13),	STRING+DOWNARROW,
				RF(15),	HOLE,	HOLE,	HOLE,	LF(16),	SHIFTKEYS+CAPSLOCK,
/*120 */	BUCKYBITS+METABIT,
			' ',	BUCKYBITS+METABIT,
					HOLE,	HOLE,	BF(14),	ERROR,	IDLE,
};

/* Caps Locked keyboard table for Type 4 keyboard */

static struct keymap keytab_s4_cl = {
/*  0 */	HOLE,	BUCKYBITS+SYSTEMBIT,
				HOLE,	LF(2),	HOLE,	TF(1),	TF(2),	TF(10),
/*  8 */	TF(3), 	TF(11),	TF(4),	TF(12),	TF(5),	SHIFTKEYS+ALTGRAPH,
								TF(6),	HOLE,
/* 16 */	TF(7),	TF(8),	TF(9),	SHIFTKEYS+ALT,
						HOLE,	RF(1),	RF(2),	RF(3),
/* 24 */	HOLE, 	LF(3), 	LF(4),	HOLE,	HOLE,	ESC,	'1',	'2',
/* 32 */	'3',	'4',	'5',	'6',	'7',	'8',	'9',	'0',
/* 40 */	'-',	'=',	'`',	'\b',	HOLE,	RF(4),	RF(5),	RF(6),
/* 48 */	BF(13),	LF(5),	BF(10),	LF(6),	HOLE,	'\t',	'Q',	'W',
/* 56 */	'E',	'R',	'T',	'Y',	'U',	'I',	'O',	'P',
/* 64 */	'[',	']',	DEL,	COMPOSE,
						RF(7),	STRING+UPARROW,
								RF(9),	BF(15),
/* 72 */	LF(7),	LF(8),	HOLE,	HOLE,	SHIFTKEYS+LEFTCTRL,
							'A', 	'S',	'D',
/* 80 */	'F',	'G',	'H',	'J',	'K',	'L',	';',	'\'',
/* 88 */	'\\',	'\r',	BF(11),	STRING+LEFTARROW,
						RF(11),	STRING+RIGHTARROW,
								BF(8),	LF(9),
/* 96 */	HOLE,	LF(10),	SHIFTKEYS+NUMLOCK,
					SHIFTKEYS+LEFTSHIFT,
						'Z',	'X',	'C',	'V',
/*104 */	'B',	'N',	'M',	',',	'.',	'/',	SHIFTKEYS+RIGHTSHIFT,
									'\n',
/*112 */	RF(13),	STRING+DOWNARROW,
				RF(15),	HOLE,	HOLE,	HOLE,	LF(16),	SHIFTKEYS+CAPSLOCK,
/*120 */	BUCKYBITS+METABIT,
			' ',	BUCKYBITS+METABIT,
					HOLE,	HOLE,	BF(14),	ERROR,	IDLE,
};

/* Alt Graph keyboard table for Type 4 keyboard */

static struct keymap keytab_s4_ag = {
/*  0 */	HOLE,	BUCKYBITS+SYSTEMBIT,
				HOLE,	LF(2),	HOLE,	TF(1),	TF(2),	TF(10),
/*  8 */	TF(3), 	TF(11),	TF(4),	TF(12),	TF(5),	SHIFTKEYS+ALTGRAPH,
								TF(6),	HOLE,
/* 16 */	TF(7),	TF(8),	TF(9),	SHIFTKEYS+ALT,
						HOLE,	RF(1),	RF(2),	RF(3),
/* 24 */	HOLE, 	LF(3), 	LF(4),	HOLE,	HOLE,	ESC,	NOP,	NOP,
/* 32 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 40 */	NOP,	NOP,	NOP,	'\b',	HOLE,	RF(4),	RF(5),	RF(6),
/* 48 */	BF(13),	LF(5),	BF(10),	LF(6),	HOLE,	'\t',	NOP,	NOP,
/* 56 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 64 */	NOP,	NOP,	DEL,	COMPOSE,
						RF(7),	STRING+UPARROW,
								RF(9),	BF(15),
/* 72 */	LF(7),	LF(8),	HOLE,	HOLE,	SHIFTKEYS+LEFTCTRL,
							NOP, 	NOP,	NOP,
/* 80 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 88 */	NOP,	'\r',	BF(11),	STRING+LEFTARROW,
						RF(11),	STRING+RIGHTARROW,
								BF(8),	LF(9),
/* 96 */	HOLE,	LF(10),	SHIFTKEYS+NUMLOCK,
					SHIFTKEYS+LEFTSHIFT,
						NOP,	NOP,	NOP,	NOP,
/*104 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	SHIFTKEYS+RIGHTSHIFT,
									'\n',
/*112 */	RF(13),	STRING+DOWNARROW,
				RF(15),	HOLE,	HOLE,	HOLE,	LF(16),	SHIFTKEYS+CAPSLOCK,
/*120 */	BUCKYBITS+METABIT,
			' ',	BUCKYBITS+METABIT,
					HOLE,	HOLE,	BF(14),	ERROR,	IDLE,
};

/* Num Locked keyboard table for Type 4 keyboard */

static struct keymap keytab_s4_nl = {
/*  0 */	HOLE,	NONL,	HOLE,	NONL,	HOLE,	NONL,	NONL,	NONL,
/*  8 */	NONL, 	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	HOLE,
/* 16 */	NONL,	NONL,	NONL,	NONL,	HOLE,	NONL,	NONL,	NONL,
/* 24 */	HOLE, 	NONL, 	NONL,	HOLE,	HOLE,	NONL,	NONL,	NONL,
/* 32 */	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,
/* 40 */	NONL,	NONL,	NONL,	NONL,	HOLE,	PADEQUAL,
								PADSLASH,
									PADSTAR,
/* 48 */	NONL,	NONL,	PADDOT,	NONL,	HOLE,	NONL,	NONL,	NONL,
/* 56 */	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,
/* 64 */	NONL,	NONL,	NONL,	NONL,
						PAD7,	PAD8,	PAD9,	PADMINUS,
/* 72 */	NONL,	NONL,	HOLE,	HOLE,	NONL,	NONL, 	NONL,	NONL,
/* 80 */	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,
/* 88 */	NONL,	NONL,	PADENTER,
					PAD4,	PAD5,	PAD6,	PAD0,	NONL,
/* 96 */	HOLE,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,
/*104 */	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,
									NONL,
/*112 */	PAD1,	PAD2,	PAD3,	HOLE,	HOLE,	HOLE,	NONL,	NONL,
/*120 */	NONL,	NONL,	NONL,	HOLE,	HOLE,	PADPLUS,
								ERROR,	IDLE,
};

/* Controlled keyboard table for Type 4 keyboard */

static struct keymap keytab_s4_ct = {
/*  0 */	HOLE,	BUCKYBITS+SYSTEMBIT,
				HOLE,	LF(2),	HOLE,	TF(1),	TF(2),	TF(10),
/*  8 */	TF(3), 	TF(11),	TF(4),	TF(12),	TF(5),	SHIFTKEYS+ALTGRAPH,
								TF(6),	HOLE,
/* 16 */	TF(7),	TF(8),	TF(9),	SHIFTKEYS+ALT,
						HOLE,	RF(1),	RF(2),	RF(3),
/* 24 */	HOLE, 	LF(3), 	LF(4),	HOLE,	HOLE,	ESC,	'1',	c('@'),
/* 32 */	'3',	'4',	'5',	c('^'),	'7',	'8',	'9',	'0',
/* 40 */	c('_'),	'=',	c('^'),	'\b',	HOLE,	RF(4),	RF(5),	RF(6),
/* 48 */	BF(13),	LF(5),	BF(10),	LF(6),	HOLE,	'\t',   c('q'),	c('w'),
/* 56 */	c('e'),	c('r'),	c('t'),	c('y'),	c('u'),	c('i'),	c('o'),	c('p'),
/* 64 */	c('['),	c(']'),	DEL,	COMPOSE,
						RF(7),	STRING+UPARROW,
								RF(9),	BF(15),
/* 72 */	LF(7),	LF(8),	HOLE,	HOLE,	SHIFTKEYS+LEFTCTRL,
							c('a'),	c('s'),	c('d'),
/* 80 */	c('f'),	c('g'),	c('h'),	c('j'),	c('k'),	c('l'),	';',	'\'',
/* 88 */	c('\\'),
			'\r',	BF(11),	STRING+LEFTARROW,
						RF(11),	STRING+RIGHTARROW,
								BF(8),	LF(9),
/* 96 */	HOLE,	LF(10),	SHIFTKEYS+NUMLOCK,
					SHIFTKEYS+LEFTSHIFT,
						c('z'),	c('x'),	c('c'),	c('v'),
/*104 */	c('b'),	c('n'),	c('m'),	',',	'.',	c('_'),	SHIFTKEYS+RIGHTSHIFT,
									'\n',
/*112 */	RF(13),	STRING+DOWNARROW,
				RF(15),	HOLE,	HOLE,	HOLE,	LF(16),	SHIFTKEYS+CAPSLOCK,
/*120 */	BUCKYBITS+METABIT,
			c(' '),	BUCKYBITS+METABIT,
					HOLE,	HOLE,	BF(14),	ERROR,	IDLE,
};

/* "Key Up" keyboard table for Type 4 keyboard */

static struct keymap keytab_s4_up = {
/*  0 */	HOLE,	BUCKYBITS+SYSTEMBIT,
				HOLE,	NOP,	HOLE,	NOP,	NOP,	NOP,
/*  8 */	NOP, 	NOP, 	NOP,	NOP,	NOP,	SHIFTKEYS+ALTGRAPH,
								NOP,	HOLE,
/* 16 */	NOP, 	NOP, 	NOP,	SHIFTKEYS+ALT,
						HOLE,	NOP,	NOP,	NOP,
/* 24 */	HOLE, 	NOP, 	NOP,	HOLE,	HOLE,	NOP,	NOP,	NOP,
/* 32 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 40 */	NOP,	NOP,	NOP,	NOP,	HOLE,	NOP,	NOP,	NOP,
/* 48 */	NOP,	NOP,	NOP,	NOP,	HOLE,	NOP,	NOP,	NOP,
/* 56 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 64 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 72 */	NOP,	NOP,	HOLE,	HOLE,	SHIFTKEYS+LEFTCTRL,
							NOP, 	NOP,	NOP,
/* 80 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 88 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 96 */	HOLE,	NOP,	NOP,
					SHIFTKEYS+LEFTSHIFT,
						NOP,	NOP,	NOP,	NOP,
/*104 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	SHIFTKEYS+RIGHTSHIFT,
									NOP,
/*112 */	NOP,	NOP,	NOP,	HOLE,	HOLE,	HOLE,	NOP,	NOP,
/*120 */	BUCKYBITS+METABIT,
			NOP,	BUCKYBITS+METABIT,
					HOLE,	HOLE,	NOP,	HOLE,	RESET,
};

/* Index to keymaps for Type 4 keyboard */
static struct keyboard keyindex_s4 = {
	&keytab_s4_lc,
	&keytab_s4_uc,
	&keytab_s4_cl,
	&keytab_s4_ag,
	&keytab_s4_nl,
	&keytab_s4_ct,
	&keytab_s4_up,
	0x0000,		/* Shift bits which stay on with idle keyboard */
	0x0000,		/* Bucky bits which stay on with idle keyboard */
	1,	77,	/* abort keys */
	CAPSMASK|NUMLOCKMASK,	/* Shift bits which toggle on down event */
};

#if	defined(i86pc)
/*******************************/
/* PC-101 keyboard definitions */
/*******************************/
/* Unshifted keyboard table for PC keyboard */

static struct keymap keytab_pc_lc = {
/*  0 */	HOLE,	'`',	'1',	'2',	'3',	'4',	'5',	'6',
/*  8 */	'7', 	'8',	'9',	'0',	'-',	'=',	HOLE,	'\b',
/* 16 */	'\t',	'q',	'w',	'e',	'r',	't',	'y',	'u',
/* 24 */	'i',	'o', 	'p', 	'[',	']',	'\\',
							SHIFTKEYS+CAPSLOCK,
									'a',
/* 32 */	's',	'd',	'f',	'g',	'h',	'j',	'k',	'l',
/* 40 */	';',	'\'',	'\\',	'\r',
					SHIFTKEYS+LEFTSHIFT,
							HOLE,	'z',	'x',
/* 48 */	'c',	'v',	'b',	'n',	'm',	',',	'.',	'/',
/* 56 */	NOP,
		    SHIFTKEYS+
		    RIGHTSHIFT,
			SHIFTKEYS+LEFTCTRL,
					HOLE,
					    SHIFTKEYS+ALT,
							' ', SHIFTKEYS+ALT,
									HOLE,
/* 64 */ SHIFTKEYS+RIGHTCTRL,
			HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,
/* 72 */	HOLE,	HOLE,	HOLE,	BF(8),	DEL,
							NOP,	HOLE,
							    STRING+LEFTARROW,
/* 80 */	RF(7),	RF(13),	HOLE,
				    STRING+UPARROW,
					    STRING+DOWNARROW,
							RF(9),	RF(15),	HOLE,
/* 88 */	HOLE,
		STRING+RIGHTARROW,
			    SHIFTKEYS+NUMLOCK,
					RF(7),	STRING+LEFTARROW,
							RF(13),	HOLE,
								PADSLASH,
/* 96 */	STRING+UPARROW,
			RF(11),	STRING+DOWNARROW,
					BF(8),	PADSTAR,
							RF(9),
							  STRING+RIGHTARROW,
									RF(15),
/*104 */	DEL,	PADMINUS,
				PADPLUS,
					HOLE,	PADENTER,
							HOLE,	ESC,	HOLE,
/*112 */	TF(1),	TF(2),	TF(3),	TF(4),	TF(5),	TF(6),	TF(7),	TF(8),
/*120 */	TF(9),	TF(10),	TF(11),	TF(12),	NOP,	NOP,	HOLE,	HOLE,
/*128 */	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,
/*136 */
};

/* Shifted keyboard table for PC keyboard */

static struct keymap keytab_pc_uc = {
/*  0 */	HOLE,	'~',	'!',	'@',	'#',	'$',	'%',	'^',
/*  8 */	'&', 	'*',	'(',	')',	'_',	'+',	HOLE,	'\b',
/* 16 */	'\t',	'Q',	'W',	'E',	'R',	'T',	'Y',	'U',
/* 24 */	'I',	'O', 	'P', 	'{',	'}',	'|',
							SHIFTKEYS+CAPSLOCK,
									'A',
/* 32 */	'S',	'D',	'F',	'G',	'H',	'J',	'K',	'L',
/* 40 */	':',	'"',	'|',	'\r',
					SHIFTKEYS+LEFTSHIFT,
							HOLE,	'Z',	'X',
/* 48 */	'C',	'V',	'B',	'N',	'M',	'<',	'>',	'?',
/* 56 */	NOP,
		    SHIFTKEYS+
		    RIGHTSHIFT,
			SHIFTKEYS+LEFTCTRL,
					HOLE,
					    SHIFTKEYS+ALT,
							' ', SHIFTKEYS+ALT,
									HOLE,
/* 64 */ SHIFTKEYS+RIGHTCTRL,
			HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,
/* 72 */	HOLE,	HOLE,	HOLE,	BF(8),	DEL,	NOP,	HOLE,
							    STRING+LEFTARROW,
/* 80 */	RF(7),	RF(13),	HOLE,	STRING+UPARROW,
						STRING+DOWNARROW,
							RF(9),	RF(15),	HOLE,
/* 88 */	HOLE,
		STRING+RIGHTARROW,
			    SHIFTKEYS+NUMLOCK,
					'7',	'4',	'1',	HOLE,	'/',
/* 96 */	'8',	'5',	'2',	'0',	'*',	'9',	'6',	'3',
/*104 */	'.',	'-',	'+',	HOLE,	'\n',	HOLE,	ESC,	HOLE,
/*112 */	TF(1),	TF(2),	TF(3),	TF(4),	TF(5),	TF(6),	TF(7),	TF(8),
/*120 */	TF(9),	TF(10),	TF(11),	TF(12),	NOP,	NOP,	HOLE,	HOLE,
/*128 */	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,
/*136 */
};

/* Caps Locked keyboard table for PC keyboard */

static struct keymap keytab_pc_cl = {
/*  0 */	HOLE,	'`',	'1',	'2',	'3',	'4',	'5',	'6',
/*  8 */	'7', 	'8',	'9',	'0',	'-',	'=',	HOLE,	'\b',
/* 16 */	'\t',	'Q',	'W',	'E',	'R',	'T',	'Y',	'U',
/* 24 */	'I',	'O', 	'P', 	'[',	']',	'\\',
							SHIFTKEYS+CAPSLOCK,
									'A',
/* 32 */	'S',	'D',	'F',	'G',	'H',	'J',	'K',	'L',
/* 40 */	';',	'\'',	'\\',	'\r',
					SHIFTKEYS+LEFTSHIFT,
							HOLE,	'Z',	'X',
/* 48 */	'C',	'V',	'B',	'N',	'M',	',',	'.',	'/',
/* 56 */	NOP,
		    SHIFTKEYS+
		    RIGHTSHIFT,
			SHIFTKEYS+LEFTCTRL,
					HOLE,
					    SHIFTKEYS+ALT,
							' ', SHIFTKEYS+ALT,
									HOLE,
/* 64 */ SHIFTKEYS+RIGHTCTRL,
			HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,
/* 72 */	HOLE,	HOLE,	HOLE,	BF(8),	DEL,	NOP,	HOLE,
							    STRING+LEFTARROW,
/* 80 */	RF(7),
			RF(13),	HOLE,
				    STRING+UPARROW,
					    STRING+DOWNARROW,
							RF(9),	RF(15),	HOLE,
/* 88 */	HOLE,
		STRING+RIGHTARROW,
			    SHIFTKEYS+NUMLOCK,
					RF(7),	STRING+LEFTARROW,
							RF(13),	HOLE, PADSLASH,
/* 96 */	STRING+UPARROW,
			RF(11),	STRING+DOWNARROW,
					BF(8),	PADSTAR,
							RF(9),
							   STRING+RIGHTARROW,
									RF(15),
/*104 */	DEL,	PADMINUS,
				PADPLUS,
					HOLE,	PADENTER,
							HOLE,	ESC,	HOLE,
/*112 */	TF(1),	TF(2),	TF(3),	TF(4),	TF(5),	TF(6),	TF(7),	TF(8),
/*120 */	TF(9),	TF(10),	TF(11),	TF(12),	NOP,	NOP,	HOLE,	HOLE,
/*128 */	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,
/*136 */
};

/* Alt Graph keyboard table for PC keyboard */

static struct keymap keytab_pc_ag = {
/*  0 */	HOLE,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/*  8 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	HOLE,	NOP,
/* 16 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 24 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
							SHIFTKEYS+CAPSLOCK,
									NOP,
/* 32 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 40 */	NOP,	NOP,	NOP,	NOP,
					SHIFTKEYS+LEFTSHIFT,
							HOLE,	NOP,	NOP,
/* 48 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 56 */	NOP,
		    SHIFTKEYS+
		    RIGHTSHIFT,
			    SHIFTKEYS+
			    LEFTCTRL,	HOLE,
					    SHIFTKEYS+ALT,
							' ',
							    SHIFTKEYS+ALT,
									HOLE,
/* 64 */ SHIFTKEYS+RIGHTCTRL,
			HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,
/* 72 */	HOLE,	HOLE,	HOLE,	BF(8),	DEL,	NOP,	HOLE,
									STRING+
								     LEFTARROW,
/* 80 */	RF(7),	RF(13),	HOLE,	STRING+
					UPARROW,STRING+
					      DOWNARROW,RF(9),	RF(15),	HOLE,
/* 88 */	HOLE,	STRING+
		    RIGHTARROW,
			SHIFTKEYS+NUMLOCK,
					NOP,	NOP,	NOP,	HOLE,	NOP,
/* 96 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/*104 */	NOP,	NOP,	NOP,	HOLE,	NOP,	HOLE,	ESC,	HOLE,
/*112 */	TF(1),	TF(2),	TF(3),	TF(4),	TF(5),	TF(6),	TF(7),	TF(8),
/*120 */	TF(9),	TF(10),	TF(11),	TF(12),	NOP,	NOP,	HOLE,	HOLE,
/*128 */	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,
/*136 */
};

/* Num Locked keyboard table for PC keyboard */

static struct keymap keytab_pc_nl = {
/*  0 */	HOLE,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,
/*  8 */	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	HOLE,	NONL,
/* 16 */	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,
/* 24 */	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,
/* 32 */	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,
/* 40 */	NONL,	NONL,	NONL,	NONL,	NONL,	HOLE,	NONL,	NONL,
/* 48 */	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,
/* 56 */	NONL,	NONL,	NONL,	HOLE,	NONL,	NONL,	NONL,	HOLE,
/* 64 */	NONL,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,
/* 72 */	HOLE,	HOLE,	HOLE,	NONL,	NONL,	NONL,	HOLE,	NONL,
/* 80 */	NONL,	NONL,	HOLE,	NONL,	NONL,	NONL,	NONL,	HOLE,
/* 88 */	HOLE,	NONL,	NONL,	PAD7,	PAD4,	PAD1,	HOLE,	NONL,
/* 96 */	PAD8,	PAD5,	PAD2,	PAD0,	NONL,	PAD9,	PAD6,	PAD3,
/*104 */	PADDOT,	NONL,	NONL,	HOLE,	NONL,	HOLE,	NONL,	HOLE,
/*112 */	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,
/*120 */	NONL,	NONL,	NONL,	NONL,	NONL,	NONL,	HOLE,	HOLE,
/*128 */	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,
/*136 */
};

/* Controlled keyboard table for PC keyboard */

static struct keymap keytab_pc_ct = {
/*  0 */	HOLE,	c('^'),	'1',	c('@'),	'3',	'4',	'5',	c('^'),
/*  8 */	'7', 	'8',	'9',	'0',	c('_'),	'=',	HOLE,	'\b',
/* 16 */	'\t',	c('q'),	c('w'),	c('e'),	c('r'),	c('t'),	c('y'),	c('u'),
/* 24 */	c('i'),	c('o'), c('p'), c('['),	c(']'),	c('\\'),
							SHIFTKEYS+CAPSLOCK,
									c('a'),
/* 32 */	c('s'),	c('d'),	c('f'),	c('g'),	c('h'),	c('j'),	c('k'),	c('l'),
/* 40 */	';',	'\'',	'\\',	'\r',
					SHIFTKEYS+LEFTSHIFT,
							HOLE,	c('z'),	c('x'),
/* 48 */	c('c'),	c('v'),	c('b'),	c('n'),	c('m'),	',',	'.',	c('_'),
/* 56 */	NOP,
		    SHIFTKEYS+
		    RIGHTSHIFT,
			SHIFTKEYS+LEFTCTRL,
					HOLE,
					    SHIFTKEYS+ALT,
							' ', SHIFTKEYS+ALT,
									HOLE,
/* 64 */ SHIFTKEYS+RIGHTCTRL,
			HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,
/* 72 */	HOLE,	HOLE,	HOLE,	BF(8),	DEL,	NOP,	HOLE,
							    STRING+LEFTARROW,
/* 80 */	RF(7),	RF(13),	HOLE,
				    STRING+UPARROW,
					    STRING+DOWNARROW,
							RF(9),	RF(15),	HOLE,
/* 88 */	HOLE,
		STRING+RIGHTARROW,
			    SHIFTKEYS+NUMLOCK,
					PAD7,	PAD4,	PAD1,	HOLE,
								PADSLASH,
/* 96 */	PAD8,	PAD5,	PAD2,	PAD0,	PADSTAR,
							PAD9,	PAD6,	PAD3,
/*104 */	PADDOT,	PADMINUS,
				PADPLUS,
					HOLE,	PADENTER,
							HOLE,	ESC,	HOLE,
/*112 */	TF(1),	TF(2),	TF(3),	TF(4),	TF(5),	TF(6),	TF(7),	TF(8),
/*120 */	TF(9),	TF(10),	TF(11),	TF(12),	NOP,	NOP,	HOLE,	HOLE,
/*128 */	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,
/*136 */
};

/* "Key Up" keyboard table for PC keyboard */


static struct keymap keytab_pc_up = {
/*  0 */	HOLE,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/*  8 */	NOP, 	NOP,	NOP,	NOP,	NOP,	NOP,	HOLE,	NOP,
/* 16 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 24 */	NOP,	NOP, 	NOP, 	NOP,	NOP,	NOP,	NOP,	NOP,
/* 32 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 40 */	NOP,	NOP,	NOP,	NOP,
					SHIFTKEYS+LEFTSHIFT,
							HOLE,	NOP,	NOP,
/* 48 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 56 */	NOP,
		    SHIFTKEYS+
		    RIGHTSHIFT,
			SHIFTKEYS+LEFTCTRL,
					HOLE,
					    SHIFTKEYS+ALT,
							NOP, SHIFTKEYS+ALT,
									HOLE,
/* 64 */ SHIFTKEYS+RIGHTCTRL,
			HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,
/* 72 */	HOLE,	HOLE,	HOLE,	NOP,	NOP,	NOP,	HOLE,	NOP,
/* 80 */	NOP,	NOP,	HOLE,	NOP,	NOP,	NOP,	NOP,	HOLE,
/* 88 */	HOLE,	NOP,	NOP,	NOP,	NOP,	NOP,	HOLE,	NOP,
/* 96 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/*104 */	NOP,	NOP,	NOP,	HOLE,	NOP,	HOLE,	NOP,	HOLE,
/*112 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/*120 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	HOLE,	HOLE,
/*128 */	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,
/*136 */
};

/* Index to keymaps for PC keyboard */
static struct keyboard keyindex_pc = {
	&keytab_pc_lc,
	&keytab_pc_uc,
	&keytab_pc_cl,
	&keytab_pc_ag,
	&keytab_pc_nl,
	&keytab_pc_ct,
	&keytab_pc_up,
	0x0000,		/* Shift bits which stay on with idle keyboard */
	0x0000,		/* Bucky bits which stay on with idle keyboard */
	0,	0,	/* abort keys */
	CAPSMASK|NUMLOCKMASK,	/* Shift bits which toggle on down event */
};
#endif

/*
 * Index table for the whole shebang
 * The first entry is used as the default if the id isn't recognized.
 */
struct keyboards keytables[] = {
	KB_SUN3,	&keyindex_s3,
	KB_SUN4,	&keyindex_s4,
#if	defined(i86pc)
	KB_PC,		&keyindex_pc,
#endif
	0,		NULL,
};

/*
 * Keyboard String Table
 *
 * This defines the strings sent by various keys (as selected in the
 * tables above).
 * The first byte of each string is its length, the rest is data.
 */

#ifdef	__STDC__
/*
 * XXX	This is here to silence compiler warnings. The non-ansi-c form
 *	is retained if somebody can figure out how to replicate it in
 *	ansi-c.
 */
char keystringtab[16][KTAB_STRLEN] = {
	{ '\033', '[', 'H', '\0' },	/* home */
	{ '\033', '[', 'A', '\0' },	/* up */
	{ '\033', '[', 'B', '\0' },	/* down */
	{ '\033', '[', 'D', '\0' },	/* left */
	{ '\033', '[', 'C', '\0' },	/* right */
};
#else	/* __STDC__ */
#define	kstescinit(c)	{'\033', '[', 'c', '\0'}
char keystringtab[16][KTAB_STRLEN] = {
	kstescinit(H) /* home */,
	kstescinit(A) /* up */,
	kstescinit(B) /* down */,
	kstescinit(D) /* left */,
	kstescinit(C) /* right */,
};
#endif	/* __STDC__ */


/*
 * Compose Key Sequence Table
 *
 * Taken from Suncompose.h of openwindows.
 *
 * The idea here is to create a simple index into a table of
 * compose key sequences.  The purpose is to provide a fast
 * lookup mechanism using as little space as possible (while
 * still using a table of triplets).
 *
 * For reference, here is the set of all composable characters:
 * SP !\"\'*+,-./01234:<>?ACDEHILNOPRSTUXY\\^_`acdehilnoprstuxy|~
 *
 * if ascii_char[i] is not composable,
 *	kb_compose_map[i] is -1
 * else
 * 	if ascii_char[i] appears as a first char in compose_table,
 *		kb_compose_map[i] is the index of it's first appearance
 *	else
 *		kb_compose_map[i] is 112	(end of table)
 */

signed char kb_compose_map[ASCII_SET_SIZE] = {
	 -1,	/* 000 (^@) */
	 -1,	/* 001 (^A) */
	 -1,	/* 002 (^B) */
	 -1,	/* 003 (^C) */
	 -1,	/* 004 (^D) */
	 -1,	/* 005 (^E) */
	 -1,	/* 006 (^F) */
	 -1,	/* 007 (^G) */
	 -1,	/* 008 (^H) */
	 -1,	/* 009 (^I) */
	 -1,	/* 010 (^J) */
	 -1,	/* 011 (^K) */
	 -1,	/* 012 (^L) */
	 -1,	/* 013 (^M) */
	 -1,	/* 014 (^N) */
	 -1,	/* 015 (^O) */
	 -1,	/* 016 (^P) */
	 -1,	/* 017 (^Q) */
	 -1,	/* 018 (^R) */
	 -1,	/* 019 (^S) */
	 -1,	/* 020 (^T) */
	 -1,	/* 021 (^U) */
	 -1,	/* 022 (^V) */
	 -1,	/* 023 (^W) */
	 -1,	/* 024 (^X) */
	 -1,	/* 025 (^Y) */
	 -1,	/* 026 (^Z) */
	 -1,	/* 027 (^[) */
	 -1,	/* 028 (^\) */
	 -1,	/* 029 (^]) */
	 -1,	/* 030 (^^) */
	 -1,	/* 031 (^_) */
	  0,	/* 032 (SP) */
	  1,	/* 033 (!) */
	  4,	/* 034 (") */
	 -1,	/* 035 (#) */
	 -1,	/* 036 ($) */
	 -1,	/* 037 (%) */
	 -1,	/* 038 (&) */
	 16,	/* 039 (') */
	 -1,	/* 040 (() */
	 -1,	/* 041 ()) */
	 28,	/* 042 (*) */
	 31,	/* 043 (+) */
	 32,	/* 044 (,) */
	 36,	/* 045 (-) */
	 48,	/* 046 (.) */
	 49,	/* 047 (/) */
	 54,	/* 048 (0) */
	 57,	/* 049 (1) */
	 60,	/* 050 (2) */
	 61,	/* 051 (3) */
	112,	/* 052 (4) */
	 -1,	/* 053 (5) */
	 -1,	/* 054 (6) */
	 -1,	/* 055 (7) */
	 -1,	/* 056 (8) */
	 -1,	/* 057 (9) */
	112,	/* 058 (:) */
	 -1,	/* 059 (;) */
	 63,	/* 060 (<) */
	 -1,	/* 061 (=) */
	 64,	/* 062 (>) */
	 65,	/* 063 (?) */
	 -1,	/* 064 (@) */
	 66,	/* 065 (A) */
	 -1,	/* 066 (B) */
	 70,	/* 067 (C) */
	112,	/* 068 (D) */
	 71,	/* 069 (E) */
	 -1,	/* 070 (F) */
	 -1,	/* 071 (G) */
	 73,	/* 072 (H) */
	 74,	/* 073 (I) */
	 -1,	/* 074 (J) */
	 -1,	/* 075 (K) */
	112,	/* 076 (L) */
	 -1,	/* 077 (M) */
	 76,	/* 078 (N) */
	 77,	/* 079 (O) */
	 84,	/* 080 (P) */
	 -1,	/* 081 (Q) */
	112,	/* 082 (R) */
	112,	/* 083 (S) */
	112,	/* 084 (T) */
	 85,	/* 085 (U) */
	 -1,	/* 086 (V) */
	 -1,	/* 087 (W) */
	112,	/* 088 (X) */
	112,	/* 089 (Y) */
	 -1,	/* 090 (Z) */
	 -1,	/* 091 ([) */
	 87,	/* 092 (\) */
	 -1,	/* 093 (]) */
	 88,	/* 094 (^) */
	 93,	/* 095 (_) */
	 94,	/* 096 (`) */
	 99,	/* 097 (a) */
	 -1,	/* 098 (b) */
	101,	/* 099 (c) */
	112,	/* 100 (d) */
	112,	/* 101 (e) */
	 -1,	/* 102 (f) */
	 -1,	/* 103 (g) */
	102,	/* 104 (h) */
	112,	/* 105 (i) */
	 -1,	/* 106 (j) */
	 -1,	/* 107 (k) */
	112,	/* 108 (l) */
	 -1,	/* 109 (m) */
	103,	/* 110 (n) */
	104,	/* 111 (o) */
	108,	/* 112 (p) */
	 -1,	/* 113 (q) */
	112,	/* 114 (r) */
	109,	/* 115 (s) */
	112,	/* 116 (t) */
	112,	/* 117 (u) */
	 -1,	/* 118 (v) */
	 -1,	/* 119 (w) */
	110,	/* 120 (x) */
	112,	/* 121 (y) */
	 -1,	/* 122 (z) */
	 -1,	/* 123 ({) */
	111,	/* 124 (|) */
	 -1,	/* 125 (}) */
	112,	/* 126 (~) */
	 -1,	/* 127 (DEL) */
};

/*
 * IMPORTANT NOTE:  This table MUST be kept in proper sorted order:
 * 	The first and second characters in each entry must be in ASCII
 *	    collating sequence (left to right).
 *	The table must be in ASCII collating sequence by first character
 *	    (top to bottom).
 */

/* COMPOSE + first character + second character => ISO character */

struct compose_sequence_t kb_compose_table[] = {

	{' ', ' ', 0xA0},	/* 000 */	/* NBSP (non-breaking space) */
	{'!', '!', 0xA1},	/* 001 */	/* inverted ! */
	{'!', 'P', 0xB6},	/* 002 */	/* paragraph mark */
	{'!', 'p', 0xB6},	/* 003 */	/* paragraph mark */
	{'"', '"', 0xA8},	/* 004 */	/* diaresis */
	{'"', 'A', 0xC4},	/* 005 */	/* A with diaresis */
	{'"', 'E', 0xCB},	/* 006 */	/* E with diaresis */
	{'"', 'I', 0xCF},	/* 007 */	/* I with diaresis */
	{'"', 'O', 0xD6},	/* 008 */	/* O with diaresis */
	{'"', 'U', 0xDC},	/* 009 */	/* U with diaresis */
	{'"', 'a', 0xE4},	/* 010 */	/* a with diaresis */
	{'"', 'e', 0xEB},	/* 011 */	/* e with diaresis */
	{'"', 'i', 0xEF},	/* 012 */	/* i with diaresis */
	{'"', 'o', 0xF6},	/* 013 */	/* o with diaresis */
	{'"', 'u', 0xFC},	/* 014 */	/* u with diaresis */
	{'"', 'y', 0xFF},	/* 015 */	/* y with diaresis */
	{'\'','A', 0xC1},	/* 016 */	/* A with acute accent */
	{'\'','E', 0xC9},	/* 017 */	/* E with acute accent */
	{'\'','I', 0xCD},	/* 018 */	/* I with acute accent */
	{'\'','O', 0xD3},	/* 019 */	/* O with acute accent */
	{'\'','U', 0xDA},	/* 020 */	/* U with acute accent */
	{'\'','Y', 0xDD},	/* 021 */	/* Y with acute accent */
	{'\'','a', 0xE1},	/* 022 */	/* a with acute accent */
	{'\'','e', 0xE9},	/* 023 */	/* e with acute accent */
	{'\'','i', 0xED},	/* 024 */	/* i with acute accent */
	{'\'','o', 0xF3},	/* 025 */	/* o with acute accent */
	{'\'','u', 0xFA},	/* 026 */	/* u with acute accent */
	{'\'','y', 0xFD},	/* 027 */	/* y with acute accent */
	{'*', 'A', 0xC5},	/* 028 */	/* A with ring */
	{'*', '^', 0xB0},	/* 029 */	/* degree */
	{'*', 'a', 0xE5},	/* 030 */	/* a with ring */
	{'+', '-', 0xB1},	/* 031 */	/* plus/minus */
	{',', ',', 0xB8},	/* 032 */	/* cedilla */
	{',', '-', 0xAC},	/* 033 */	/* not sign */
	{',', 'C', 0xC7},	/* 034 */	/* C with cedilla */
	{',', 'c', 0xE7},	/* 035 */	/* c with cedilla */
	{'-', '-', 0xAD},	/* 036 */	/* soft hyphen */
	{'-', ':', 0xF7},	/* 037 */	/* division sign */
	{'-', 'A', 0xAA},	/* 038 */	/* feminine superior numeral */
	{'-', 'D', 0xD0},	/* 039 */	/* Upper-case eth */
	{'-', 'L', 0xA3},	/* 040 */	/* pounds sterling */
	{'-', 'Y', 0xA5},	/* 041 */	/* yen */
	{'-', '^', 0xAF},	/* 042 */	/* macron */
	{'-', 'a', 0xAA},	/* 043 */	/* feminine superior numeral */
	{'-', 'd', 0xF0},	/* 044 */	/* Lower-case eth */
	{'-', 'l', 0xA3},	/* 045 */	/* pounds sterling */
	{'-', 'y', 0xA5},	/* 046 */	/* yen */
	{'-', '|', 0xAC},	/* 047 */	/* not sign */
	{'.', '^', 0xB7},	/* 048 */	/* centered dot */
	{'/', 'C', 0xA2},	/* 049 */	/* cent sign */
	{'/', 'O', 0xD8},	/* 050 */	/* O with slash */
	{'/', 'c', 0xA2},	/* 051 */	/* cent sign */
	{'/', 'o', 0xF8},	/* 052 */	/* o with slash */
	{'/', 'u', 0xB5},	/* 053 */	/* mu */
	{'0', 'X', 0xA4},	/* 054 */	/* currency symbol */
	{'0', '^', 0xB0},	/* 055 */	/* degree */
	{'0', 'x', 0xA4},	/* 056 */	/* currency symbol */
	{'1', '2', 0xBD},	/* 057 */	/* 1/2 */
	{'1', '4', 0xBC},	/* 058 */	/* 1/4 */
	{'1', '^', 0xB9},	/* 059 */	/* superior '1' */
	{'2', '^', 0xB2},	/* 060 */	/* superior '2' */
	{'3', '4', 0xBE},	/* 061 */	/* 3/4 */
	{'3', '^', 0xB3},	/* 062 */	/* superior '3' */
	{'<', '<', 0xAB},	/* 063 */	/* left guillemot */
	{'>', '>', 0xBB},	/* 064 */	/* right guillemot */
	{'?', '?', 0xBF},	/* 065 */	/* inverted ? */
	{'A', 'E', 0xC6},	/* 066 */	/* AE dipthong */
	{'A', '^', 0xC2},	/* 067 */	/* A with circumflex accent */
	{'A', '`', 0xC0},	/* 068 */	/* A with grave accent */
	{'A', '~', 0xC3},	/* 069 */	/* A with tilde */
	{'C', 'O', 0xA9},	/* 060 */	/* copyright */
	{'E', '^', 0xCA},	/* 071 */	/* E with circumflex accent */
	{'E', '`', 0xC8},	/* 072 */	/* E with grave accent */
	{'H', 'T', 0xDE},	/* 073 */	/* Upper-case thorn */
	{'I', '^', 0xCE},	/* 074 */	/* I with circumflex accent */
	{'I', '`', 0xCC},	/* 075 */	/* I with grave accent */
	{'N', '~', 0xD1},	/* 076 */	/* N with tilde */
	{'O', 'R', 0xAE},	/* 077 */	/* registered */
	{'O', 'S', 0xA7},	/* 078 */	/* section mark */
	{'O', 'X', 0xA4},	/* 079 */	/* currency symbol */
	{'O', '^', 0xD4},	/* 080 */	/* O with circumflex accent */
	{'O', '_', 0xBA},	/* 081 */	/* masculine superior numeral */
	{'O', '`', 0xD2},	/* 082 */	/* O with grave accent */
	{'O', '~', 0xD5},	/* 083 */	/* O with tilde */
	{'P', '|', 0xDE},	/* 084 */	/* Upper-case thorn */
	{'U', '^', 0xDB},	/* 085 */	/* U with circumflex accent */
	{'U', '`', 0xD9},	/* 086 */	/* U with grave accent */
	{'\\','\\',0xB4},	/* 087 */	/* acute accent */
	{'^', 'a', 0xE2},	/* 088 */	/* a with circumflex accent */
	{'^', 'e', 0xEA},	/* 089 */	/* e with circumflex accent */
	{'^', 'i', 0xEE},	/* 090 */	/* i with circumflex accent */
	{'^', 'o', 0xF4},	/* 091 */	/* o with circumflex accent */
	{'^', 'u', 0xFB},	/* 092 */	/* u with circumflex accent */
	{'_', 'o', 0xBA},	/* 093 */	/* masculine superior numeral */
	{'`', 'a', 0xE0},	/* 094 */	/* a with grave accent */
	{'`', 'e', 0xE8},	/* 095 */	/* e with grave accent */
	{'`', 'i', 0xEC},	/* 096 */	/* i with grave accent */
	{'`', 'o', 0xF2},	/* 097 */	/* o with grave accent */
	{'`', 'u', 0xF9},	/* 098 */	/* u with grave accent */
	{'a', 'e', 0xE6},	/* 099 */	/* ae dipthong */
	{'a', '~', 0xE3},	/* 100 */	/* a with tilde */
	{'c', 'o', 0xA9},	/* 101 */	/* copyright */
	{'h', 't', 0xFE},	/* 102 */	/* Lower-case thorn */
	{'n', '~', 0xF1},	/* 103 */	/* n with tilde */
	{'o', 'r', 0xAE},	/* 104 */	/* registered */
	{'o', 's', 0xA7},	/* 105 */	/* section mark */
	{'o', 'x', 0xA4},	/* 106 */	/* currency symbol */
	{'o', '~', 0xF5},	/* 107 */	/* o with tilde */
	{'p', '|', 0xFE},	/* 108 */	/* Lower-case thorn */
	{'s', 's', 0xDF},	/* 109 */	/* German double-s */
	{'x', 'x', 0xD7},	/* 110 */	/* multiplication sign */
	{'|', '|', 0xA6},	/* 111 */	/* broken bar */

	{0, 0, 0},			/* end of table */
};

/*
 * Floating Accent Sequence Table
 */

/* FA + ASCII character => ISO character */
struct fltaccent_sequence_t kb_fltaccent_table[] = {

	{FA_UMLAUT, 'A', 0xC4},		/* A with umlaut */
	{FA_UMLAUT, 'E', 0xCB},		/* E with umlaut */
	{FA_UMLAUT, 'I', 0xCF},		/* I with umlaut */
	{FA_UMLAUT, 'O', 0xD6},		/* O with umlaut */
	{FA_UMLAUT, 'U', 0xDC},		/* U with umlaut */
	{FA_UMLAUT, 'a', 0xE4},		/* a with umlaut */
	{FA_UMLAUT, 'e', 0xEB},		/* e with umlaut */
	{FA_UMLAUT, 'i', 0xEF},		/* i with umlaut */
	{FA_UMLAUT, 'o', 0xF6},		/* o with umlaut */
	{FA_UMLAUT, 'u', 0xFC},		/* u with umlaut */
	{FA_UMLAUT, 'y', 0xFC},		/* y with umlaut */

	{FA_CFLEX, 'A', 0xC2},		/* A with circumflex */
	{FA_CFLEX, 'E', 0xCA},		/* E with circumflex */
	{FA_CFLEX, 'I', 0xCE},		/* I with circumflex */
	{FA_CFLEX, 'O', 0xD4},		/* O with circumflex */
	{FA_CFLEX, 'U', 0xDB},		/* U with circumflex */
	{FA_CFLEX, 'a', 0xE2},		/* a with circumflex */
	{FA_CFLEX, 'e', 0xEA},		/* e with circumflex */
	{FA_CFLEX, 'i', 0xEE},		/* i with circumflex */
	{FA_CFLEX, 'o', 0xF4},		/* o with circumflex */
	{FA_CFLEX, 'u', 0xFB},		/* u with circumflex */

	{FA_TILDE, 'A', 0xC3},		/* A with tilde */
	{FA_TILDE, 'N', 0xD1},		/* N with tilde */
	{FA_TILDE, 'O', 0xD5},		/* O with tilde */
	{FA_TILDE, 'a', 0xE3},		/* a with tilde */
	{FA_TILDE, 'n', 0xF1},		/* n with tilde */
	{FA_TILDE, 'o', 0xF5},		/* o with tilde */

	{FA_CEDILLA, 'C', 0xC7},	/* C with cedilla */
	{FA_CEDILLA, 'c', 0xE7},	/* c with cedilla */

	{FA_ACUTE, 'A', 0xC1},		/* A with acute accent */
	{FA_ACUTE, 'E', 0xC9},		/* E with acute accent */
	{FA_ACUTE, 'I', 0xCD},		/* I with acute accent */
	{FA_ACUTE, 'O', 0xD3},		/* O with acute accent */
	{FA_ACUTE, 'U', 0xDA},		/* U with acute accent */
	{FA_ACUTE, 'a', 0xE1},		/* a with acute accent */
	{FA_ACUTE, 'e', 0xE9},		/* e with acute accent */
	{FA_ACUTE, 'i', 0xED},		/* i with acute accent */
	{FA_ACUTE, 'o', 0xF3},		/* o with acute accent */
	{FA_ACUTE, 'u', 0xFA},		/* u with acute accent */
	{FA_ACUTE, 'y', 0xFD},		/* y with acute accent */

	{FA_GRAVE, 'A', 0xC0},		/* A with grave accent */
	{FA_GRAVE, 'E', 0xC8},		/* E with grave accent */
	{FA_GRAVE, 'I', 0xCC},		/* I with grave accent */
	{FA_GRAVE, 'O', 0xD2},		/* O with grave accent */
	{FA_GRAVE, 'U', 0xD9},		/* U with grave accent */
	{FA_GRAVE, 'a', 0xE0},		/* a with grave accent */
	{FA_GRAVE, 'e', 0xE8},		/* e with grave accent */
	{FA_GRAVE, 'i', 0xEC},		/* i with grave accent */
	{FA_GRAVE, 'o', 0xF2},		/* o with grave accent */
	{FA_GRAVE, 'u', 0xF9},		/* u with grave accent */

	{0, 0, 0},			/* end of table */
};

/*
 * Num Lock Table
 */

/* Num Lock:  pad key entry & 0x1F => ASCII character */
u_char kb_numlock_table[] = {
	'=',
	'/',
	'*',
	'-',
	',',

	'7',
	'8',
	'9',
	'+',

	'4',
	'5',
	'6',

	'1',
	'2',
	'3',

	'0',
	'.',
	'\n',	/* Enter */
};

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright  (c) 1986 AT&T
 *	All Rights Reserved
 */
#ident	"@(#)working.c	1.6	97/12/03 SMI"       /* SVr4.0 1.4 */

#include	<curses.h>
#include	"wish.h"
#include	"vt.h"
#include	"vtdefs.h"

/* set in if_init.c */
extern int	Work_col;
extern char	*Work_msg;

/*
 * puts up or removes "Working" message on status line at "Work_col"
 */

void
working(flag)
bool	flag;
{
/* new */
	WINDOW		*win;

	win = VT_array[ STATUS_WIN ].win;
	if (flag)
		mvwaddstr(win, 0, Work_col, Work_msg);
	else {
		wmove(win, 0, Work_col);
		wclrtoeol(win);		/* assumes right-most! */
	}
	wnoutrefresh( win );
	if ( flag )
		doupdate();
}
#pragma ident	"@(#)filbuf.c	1.13	98/01/06 SMI"  /* from S5R2 2.1 */


/*LINTLIBRARY*/
#include <stdio.h>

extern _findbuf();
extern int read();
extern int fflush();

int
__filbuf(iop)
register FILE *iop;
{
	return (_filbuf(iop));
}

int
_filbuf(iop)
register FILE *iop;
{

	if ( !(iop->_flag & _IOREAD) )
		if (iop->_flag & _IORW)
			iop->_flag |= _IOREAD;
		else
			return(EOF);

	if (iop->_flag&(_IOSTRG|_IOEOF))
		return(EOF);

	if (iop->_base == NULL)  /* get buffer if we don't have one */
		_findbuf(iop);

	if (iop == stdin) {
		if (stdout->_flag&_IOLBF)
			(void) fflush(stdout);
		if (stderr->_flag&_IOLBF)
			(void) fflush(stderr);
	}

	iop->_ptr = iop->_base;
	iop->_cnt = read(fileno(iop), (char *)iop->_base,
	    (unsigned)((iop->_flag & _IONBF) ? 1 : iop->_bufsiz ));
	if (--iop->_cnt >= 0)		/* success */
		return (*iop->_ptr++);
	if (iop->_cnt != -1)		/* error */
		iop->_flag |= _IOERR;
	else {				/* end-of-file */
		iop->_flag |= _IOEOF;
		if (iop->_flag & _IORW)
			iop->_flag &= ~_IOREAD;
	}
	iop->_cnt = 0;
	return (EOF);
}

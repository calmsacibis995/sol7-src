/*	@(#)stdiom.h 1.4 88/02/08 SMI; from S5R2 1.1	*/

/*	@(#)stdiom.h 1.3 94/08/10 */

/*
	The following macros improve performance of the stdio by reducing the
	number of calls to _bufsync and _wrtchk.  _BUFSYNC has the same
	effect as _bufsync, and _WRTCHK has the same effect as _wrtchk,
	but often these functions have no effect, and in those cases the
	macros avoid the expense of calling the functions.  */

#define	_BUFSYNC(iop)	if ((iop->_base + iop->_bufsiz) - iop->_ptr <   \
			    (iop->_cnt < 0 ? 0 : iop->_cnt))  \
				_bufsync(iop)
#define	_WRTCHK(iop)	((((iop->_flag & (_IOWRT | _IOEOF)) != _IOWRT) || \
				(iop->_base == NULL) || \
				(iop->_ptr == iop->_base && iop->_cnt == 0 && \
					!(iop->_flag & (_IONBF | _IOLBF)))) \
			? _wrtchk(iop) : 0)
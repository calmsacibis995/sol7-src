	.ident	"@(#)link.s	1.8	97/01/06 SMI"

	.file	"link.s"

	.text

	.globl	__link
	.globl	__cerror

_fgdef_(`__link'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$LINK,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	xorl	%eax,%eax
	ret

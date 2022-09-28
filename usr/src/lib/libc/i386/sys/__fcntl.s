
	.ident	"@(#)__fcntl.s 1.3	96/12/20 SMI"

	.file	"__fcntl.s"

	.text

	.globl  __fcntl
	.globl	__cerror

 _fgdef_(`__fcntl'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$FCNTL,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	cmpb	$ERESTART,%al
	je 	__fcntl
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)

noerror:
	ret

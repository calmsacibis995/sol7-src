	.ident	"@(#)kill.s	1.8	97/05/01 SMI"

	.file	"kill.s"

	.text

	.globl	__cerror

_fwpdef_(`_kill', `_libc_kill'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$KILL,%eax
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

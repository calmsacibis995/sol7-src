	.ident	"@(#)resolvepath.s	1.1	97/04/30 SMI"


	.file	"resolvepath.s"

	.text

	.globl  __cerror

_fwdef_(`resolvepath'):
	MCOUNT
	movl	$RESOLVEPATH,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret

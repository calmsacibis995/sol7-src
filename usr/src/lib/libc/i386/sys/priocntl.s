	.ident	"@(#)priocntlset.s	1.8	97/01/16 SMI"

/ i = priocntlset();
/ returns number of classes or pid of selected LWP

	.file	"priocntl.s"

	.text

	.globl  __cerror
	.globl	__priocntlset

_fgdef_(`__priocntlset'):
	MCOUNT
	movl	$PRIOCNTLSET,%eax
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

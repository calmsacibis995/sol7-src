	.ident	"@(#)ntp_adjtime.s	1.1	96/10/10 SMI"


	.file	"ntp_adjtime.s"

	.text

	.globl	__cerror

_fwdef_(`ntp_adjtime'):
	MCOUNT			/ subroutine entry counter if profiling
	movl	$NTP_ADJTIME,%eax
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

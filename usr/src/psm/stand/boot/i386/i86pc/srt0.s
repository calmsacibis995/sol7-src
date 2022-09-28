/*
 * Copyright (c) 1986-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident "@(#)srt0.s	1.21	98/02/11 SMI"

/*
 * srt0.s - standalone startup code
 */

#include <sys/asm_linkage.h>
#include <sys/bootdef.h>
#include <sys/reg.h>

#if defined(lint)

#include <sys/types.h>
#include <sys/bootlink.h>
#include <sys/bootconf.h>
#include <sys/dev_info.h>
#include <sys/bootp2s.h>

extern int main(void *cookie, char **argv, int argc);
extern void bsetup(void);
extern int handle21();
extern void a20enable(void);

struct gate_desc {
	unsigned long  g_off;           /* offset */
	unsigned short g_sel;           /* selector */
	unsigned char  g_wcount;        /* word count */
	unsigned char  g_type;          /* type of gate and access rights */
};
extern struct gate_desc slbidt[];
extern int yydebug;
struct int_pb ic;
struct int_pb32 ic32;	/* for 32 bit registers int call */
char _end[1];
struct bootops *bop;
struct pri_to_secboot *realp;
int Oldstyleboot;
int BootDev, BootDevType;
ulong old21vec, i21cnt;
uint bpt_loc, bpd_loc, cr0mask;
int pp_table;

/*ARGSUSED*/
void
start(void *cookie, char **argv, int argc)
{
	a20enable();
	bsetup();
	(void) main(cookie, argv, argc);
	start(cookie, argv, argc);
	(void) handle21(0);
	slbidt[0] = slbidt[1];
	yydebug = 0xfeedface;
}

void *
memcpy(void *s1, void *s2, size_t n)
{
	return(n ? s1 : s2);
}

void wait100ms(void) { return; }
int dofar(ulong fptr) { return((int)fptr); }
int doint_asm(int ival, struct real_regs *rr) { return((int)rr->ds+ival); }
ulong dofar_stack;
int dofar_with_stack(ulong ptr) { return((int)(dofar_stack-ptr)); }
caddr_t caller(void) { return(0); }
ulong cursp;
void getesp(void) { cursp = cursp; }
void int21chute(void) { return; }
void comeback_with_stack(void) { return; }
void start_paging(void) { return; }
void pagestop(void) { return; }
void pagestart(void) { return; }
/*ARGSUSED*/
void dregister(long dr0, long dr7) { return; }

#else /* !defined(lint) */


/* some macros used for debugging... */
/*#define	FLOWDEBUG		/* enable BIOS prints to show control flow */

#define GLOBAL_LONG(name)\
	.globl	name;\
name:;\
	.long	0

#define	RMPUTS(str)\
	data16;\
	call	.RMPUTS_ESTR.__LINE__;\
	.string str;\
	.string "\000";\
.RMPUTS_ESTR.__LINE__:;\
	data16;\
	popl	%esi;\
.RMPUTS_TOP.__LINE__:;\
	data16;\
	xorl	%eax,%eax;\
	addr16;\
	movb	%cs:(%esi),%al;\
	andb	%al,%al;\
	jz	.RMPUTS_BOT.__LINE__;\
	data16;\
	andl	$0x7f,%eax;\
	data16;\
	orl	$0x0e00,%eax;\
	data16;\
	movl	$1,%ebx;\
	int	$0x10;\
	data16;\
	incl	%esi;\
	jmp	.RMPUTS_TOP.__LINE__;\
.RMPUTS_BOT.__LINE__:

#define	_RMPUTREG(shval)\
	data16;\
	movl	%esi,%eax;\
	data16;\
	sarl	shval,%eax;\
	data16;\
	andl	$0xf,%eax;\
	data16;\
	movl	$pp_hextable,%ecx;\
	addr16;\
	movb	%cs:(%ecx,%eax),%al;\
	data16;\
	andl	$0x7f,%eax;\
	data16;\
	orl	$0x0e00,%eax;\
	data16;\
	movl	$1,%ebx;\
	int	$0x10;

#define RMPUT_ESI_TOPHALF\
	_RMPUTREG($28)\
	_RMPUTREG($24)\
	_RMPUTREG($20)\
	_RMPUTREG($16)

#define RMPUT_ESI_BOTHALF\
	_RMPUTREG($12)\
	_RMPUTREG($8)\
	_RMPUTREG($4)\
	_RMPUTREG($0)

#define RMPUT_ESI\
	RMPUT_ESI_TOPHALF\
	RMPUT_ESI_BOTHALF

#define	RMPUT_MEMLOC(loc)\
	addr16;\
	data16;\
	movl	loc,%esi;\
	RMPUT_ESI


	/ XXX NOTE: change anything between _start and _begin and you die!
/	_start(secboot_mem_loc, spc, spt, bps, entry_ds_si, act_part)
	.globl	_start
_start:
	/ we begin here in real mode and quickly switch to protected mode.
	/ to help make this assembly language a little easier to read, the
	/ convention is to indent the real mode code two extra tabstops and
	/ add the data16 and addr16 prefix pseudo-ops in those two tabstops.
        		cli
			jmp	_begin
/
/	Divot is for saving any patch information necessary due to
/	inetboot backward compatibity issues.
/
	.globl	divot
divot:
	.value	0
	.long	0
	.long	0

/
/	the following code provides backward compatibility with the
/	"xpcimach" module that was delivered in an update to solaris 2.4.
/	that module wants to do some bios interrupts so it jumps to
/	the hard-wired address 0x8327 which is where the "doint" routine
/	used to live in the 2.4 version of ufsboot (i'm not making this up).
/	before doing this, the xpcimach module first checks to see if the
/	first four bytes of the routine at 0x8327 appear to be the doint
/	code by comparing them with 0x55, 0x8b, 0xec, and 0x56 (i'm still
/	not making this up).  if those values are there, then xpcimach
/	assumes it can fill in the global "ic" data structure at memory
/	location 0x80fc (really, why would i lie?) and jump to 0x8327.
/
/	okay, pretty gross.  but we can top that.  we want to continue to
/	support that xpcimach module, so we've put code here that will pass
/	its test (i.e. the first four bytes match those found in the 2.4
/	ufsboot binary) and we've reserved the area where xpcimach thinks
/	the "ic" struct lives.  then, when xpcimach jumps to 0x8327,
/	we do the necessary things to allow our newer version of doint
/	to do the job.
/
/	note that on most systems this code never gets executed.  we just
/	jump around it.  the real second level boot starts at _begin.
/
/	it would be a BAD IDEA to try moving this code somewhere else!
/

	/ skip to 0x80fc, where the old "ic" buffer is expected to live
	/ XXX NOTE: we subtract 4 because of an assembler bug.  it is
	/ XXX NOTE: confused by the jmp instruction above.
	.set	.,_start+0xfc-4

	/	80fc:	you are here...

	/ here is the "ic" buffer expected by xpcimach
old_ic:
	.2byte	0	/ intval
	.2byte	0	/ ax
	.2byte	0	/ bx
	.2byte	0	/ cx
	.2byte	0	/ dx
	.2byte	0	/ bp
	.2byte	0	/ es
	.2byte	0	/ si
	.2byte	0	/ di
	.2byte	0	/ ds

	/	8110:	you are here...

	/ skip until we get to 0x8327
	/ XXX NOTE: we subtract 4 because of an assembler bug.  it is
	/ XXX NOTE: confused by the jmp instruction above.
	.set	.,_start+0x327-4

	/	8327:	you are here...

	/ the bytes expected here: 0x55, 0x8b, 0xec, and 0x56
	/ are these opcodes:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%esi

	/ copy the struct ic stuff stored by xpcimach into the real struct ic
	movl	old_ic,%esi
	movl	%esi,ic
	movl	old_ic+4,%esi
	movl	%esi,ic+4
	movl	old_ic+8,%esi
	movl	%esi,ic+8
	movl	old_ic+12,%esi
	movl	%esi,ic+12
	movl	old_ic+16,%esi
	movl	%esi,ic+16

	/ let the real doint function do its thing
	call	doint

	/ restore stack and return to caller
	popl	%esi
	popl	%ebp
	ret

	/ here endeth the gross ugly hack for xpcimach

	/ XXX NOTE: another assembler problem...
	/ XXX NOTE: the jmp instruction right after the _start label
	/ XXX NOTE: is confusing the assembler.  it generates an offset
	/ XXX NOTE: that is two bytes before the label _begin.  so we
	/ XXX NOTE: put some nops here so it will just "fall through..."
	nop
	nop
	nop
	nop
	.globl	_begin
_begin:
	/ this "addr16/data16" prefix stuff is a real pain.  "data16" is a
	/ toggle that switches the mode back and forth from 16/32 bit operands.
	/ Our native state is 16-bit operands, so we need to "data16" prefix
	/ instructions that will use longword operands. (see the xorl/movl
	/ instructions below)
	data16;		xorl	%eax, %eax
			movw	%ax, %ds
			movw	%ax, %fs
			movw	%ax, %gs
	data16;		movl	%eax, %ebx
	data16;		movl	%eax, %ecx
	data16;		movl	%eax, %esi

/ only DL contents are guaranteed!
///			andw	$LOW_REGMASK, %dx

		addr16;	movw	%dx, BootDev	/ Boot dev is coming in in DL
	data16;		movl	%eax, %edx	/saved BootDev, more known state

	/ save es in dx because of changes in goprot
			movw    %es,  %dx
			movw	%ax, %es	/all regs now in known state.

	data16;		movl	$SECBOOT_STACKLOC, %ebx
			movw	%ax, %ss
	data16;		movl	%ebx, %esp

/
/	We may have been patched to work with older bootparams.  If so, these 
/	bootparams specified an address of 0x35000 as the starting address
/	after RPL download.  To maintain compatibility with these bootparams,
/	a 'jmp _start' was placed in the boot image to immediately redirect
/	the inetboot to the proper starting point, _start.  Our job now is
/	to restore the original contents of the boot image at 0x35000 before
/	someone needs to execute them for real.
/
		addr16;	movw	divot, %ax
			testw	$0, %ax
			je	nodivot

	data16;		leal	divot, %esi
	data16;		movl	2(%esi), %eax
	data16;		movl	%eax, 0x35000
	data16;		movl	6(%esi), %eax
	data16;		movl	%eax, 0x35004

	.globl	nodivot
nodivot:
	data16;		xorl	%eax, %eax
	data16;		movl	%eax, %esi

	/ assume this srt0 code is loaded in first 64k

	/ go prot assumes that real mode CS is 0
	data16;		call    goprot

	/ now in protected mode...

	/ %edi is preserved past goprot
	/ old %es is in %dx
	/ structure pointer is in %es:%di
	/ save it in realp
	/ segment:offset -> physical addr = seg shifted left 4 + offset
	xorl	 %eax, %eax
	shll    $4, %edx	/ old es
	movw    %di, %ax
	addl    %edx, %eax
	movl    %eax, realp

	/ enable addr bit 20 so we can put the arena at 1Mb
	call	a20enable

/	relocate all but srt0 to high memory
	movl	$_endsrt0, %esi	/ start copying at end of srt0
	movl	$edata, %eax	/ calculate number of longs
	subl	$SECBOOT_RELOCADDR, %eax
	shrl	$2, %eax
	incl	%eax
	movl	%eax, %ecx
	cld
	movl	$SECBOOT_RELOCADDR, %edi
	rep
	movsl

/	zero out the bss
	movl	$edata, %edi		/ set start address at end of data seg
	movl	$end, %eax		/ get long word count
	subl	%edi, %eax
	shrl	$2, %eax
	incl	%eax			/ add 1 word count for truncation
	movl	%eax, %ecx
	xorl	%eax, %eax		/ set target value to zero
	rep
	stosl

/	set up protected mode interrupt descriptor table
	movl	$slbidt, %eax	/* pointer to IDT */
	movl	$IDTLIM, %ecx	/* size of IDT */
	call	munge_table	/* rearranges/builds table in place */

	lidt    standIDTptr

	call	bsetup

	call	main

/	no return here
	sti
	hlt


/	----------------------------------------------------
/ Enter protected mode.
/
/ We must set up the GDTR
/
/ When we enter this routine, 	ss = ds = cs = "codebase", 
/	when we leave,  	ss = B_GDT(0x08),
/				cs = C_GDT(0x10).
/				ds = es = fs = gs = B_GDT(0x08).
/
/ Trashes %ax, %bx and sets segment registers as above. 
/ CAUTION - If other than ax and bx get used, check all callers
/           to see that register(s) are saved around the call.
/
	.globl	goprot
goprot:

	data16;		popl	%ebx	/ get return %eip, for later use

	/ load the GDTR

	addr16;	data16;	lgdt	GDTptr

			mov	%cr0, %eax

	/ set protect mode and possibly
	/ page mode based on cr0mask.
	/ cr0mask is set for paging
	/ in page_on()
	addr16;	data16;	orl	cr0mask, %eax
			mov	%eax, %cr0 

			jmp	qflush		/ flush the prefetch queue

/ 	Set up the segment registers, so we can continue like before;
/ 	if everything works properly, this shouldn't change anything.
/ 	Note that we're still in 16 bit operand and address mode, here, 
/ 	and we will continue to be until the new %cs is established. 

qflush:
	data16;		mov	$B_GDT, %eax	/ big flat data descriptor
			movw	%ax, %ds
			movw	%ax, %es
			movw    %ax, %fs
			movw    %ax, %gs
			movw	%ax, %ss		/ don't need to set %sp

/ 	Now, set up %cs by fiddling with the return stack and doing an lret

	data16;		pushl	$C_GDT			/ push %cs

	data16;		pushl	%ebx			/ push %eip

	data16;		lret

/	----------------------------------------------------
/ 	Re-enter real mode.
/ 
/ 	We assume that we are executing code in a segment that
/ 	has a limit of 64k. Thus, the CS register limit should
/ 	be set up appropriately for real mode already. 
/ 	Set up %ss, %ds, %es, %fs, and %gs with a selector that
/ 	points to a descriptor containing the following values
/
/	Limit = 64k
/	Byte Granular 	( G = 0 )
/	Expand up	( E = 0 )
/	Writable	( W = 1 )
/	Present		( P = 1 )
/	Base = any value

	.globl	goreal
goreal:

/ 	Transfer control to a 16 bit code segment

	ljmp	$C16GDT, $set16cs
set16cs:			/ 16 bit addresses and operands 

	/ need to have all segment regs sane
	/ before we can enter real mode
	data16;		movl	$D_GDT, %eax
			movw	%ax, %es
			movw	%ax, %ds

			mov	%cr0, %eax

	/ clear the protection and
	/ paging bits
	/ jump should clear prefetch q
	data16;		and 	$NOPROTMASK, %eax

			mov	%eax, %cr0

/	We do a long jump here, to reestablish %cs is real mode.
/	It appears that this has to be a ljmp as opposed to
/       a lret probably due to the way Intel fixed errata #25
/       on the A2 step. This leads to self modifying code.

farjump:

			ljmp	$0x0, $restorecs

/ 	Now we've returned to real mode, so everything is as it 
/	should be. Set up the segment registers and so on.
/	The stack pointer can stay where it was, since we have fiddled
/	the segments to be compatible.

restorecs:

	/ flush tlb
			mov	%cr3, %eax
			mov	%eax, %cr3

			movw	%cs, %ax
			movw	%ax, %ss
			movw	%ax, %ds
			movw	%ax, %es
			movw	%ax, %fs
			movw	%ax, %gs

	/ return to whence we came; it was a 32 bit call
	data16;		ret

/
/	start_paging	Turn the x86's paging mode on.
/
.globl	start_paging
start_paging:
	/ Turn paging on
	movl	bpd_loc, %eax
	movl	%eax, %cr3

	movl	%cr0, %eax
	orl	$[CR0_PG|PROTMASK], %eax/ set page and protected mode
	andl	$-1![CR0_EM], %eax	/ turn off emulation bit
	movl	%eax, %cr0 

	jmp	page_flush		/ flush the prefetch queue
page_flush:
	nop
	nop
	ret

/	Data definitions
	.align	4
	.globl	secboot_mem_loc
	.globl	act_part
	.globl	entry_ds_si
	.globl	cr0mask
	.globl	spc
	.globl	spt
	.globl	bps
	.globl  bpd_loc 
	.globl  bpt_loc 
	.globl  ic
	.globl	ic32
	.globl  realp
ic:
	.long   0
	.long   0
	.long   0
	.long   0
	.long   0
ic32:
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
secboot_mem_loc:
	.long	0
act_part:
	.long	0
entry_ds_si:
	.long	0
bpd_loc:		/boot page directory location
	.long	0
bpt_loc:		/boot page table location
	.long	0
cr0mask:
	.long	PROTMASK
save_esp2:
	.long	0
save_esp:
	.long	0
realp:	
	.long	0

standGDTptr:	
standGDTlimit:
	.value	0
standGDTbase:
	.long	0

	.long	IDTLIM		/* size of interrupt descriptor table */
	.set	IDTLIM, [8\*256-1]	/* IDTSZ = 256 (for now....) */

bioIDTptr:	
bioIDTlimit:
	.value	0x3ff	/ size of bios idt
bioIDTbase:
	.long	0

/ 	In-memory stand-alone program IDT pointer 

standIDTptr:	
standIDTlimit:
	.value	IDTLIM
standIDTbase:
	.long	slbidt

spc:
	.value	0
spt:
	.value	0
bps:
	.value	0
stand_cs:
	.value	0
stand_ds:
	.value	0
stand_es:
	.value	0
stand_fs:
	.value	0
stand_gs:
	.value	0
stand_ss:
	.value	0
	.globl	dofar_stack
dofar_stack:
	.long	0	/ used by dofar_with_stack
dofar_ostack:
	.long	0	/ used by dofar_with_stack
dofar_stand_cs:
	.value	0
dofar_stand_ds:
	.value	0
dofar_stand_es:
	.value	0
dofar_stand_fs:
	.value	0
dofar_stand_gs:
	.value	0
dofar_stand_ss:
	.value	0
dofar_save_esp:
	.long	0
dofar_standGDTptr:	
dofar_standGDTlimit:
	.value	0
dofar_standGDTbase:
	.long	0
dofar_standIDTptr:	
dofar_standIDTlimit:
	.value	IDTLIM
dofar_standIDTbase:
	.long	slbidt
.globl	child_up
child_up:
	.value	0
variable:
	.value	0
.globl	Oldstyleboot
Oldstyleboot:
	.long	0x0
.globl	BootDev
BootDev:
	.long	0x80
.globl	BootDevType
BootDevType:
	.long	0x13

	.string	"Copyright (c) 1986-1995, Sun Microsystems, Inc."

.align 4
/NB new

/ May have to throw this away later
.globl	romp
romp:   .long   0
.globl	bootsvcs
bootsvcs:   .long   0

	.align	4
/	----------------------------------------------------
/ 	The GDTs for protected mode operation
/
/	All 32 bit GDTs can reference the entire 4GB address space.

	.globl	flatdesc

GDTstart:
nulldesc:			/ offset = 0x0

	.value	0x0	
	.value	0x0	
	.byte	0x0	
	.byte	0x0	
	.byte	0x0	
	.byte	0x0	

flatdesc:			/ offset = 0x08    B_GDT

	.value	0xFFFF		/ segment limit 0..15
	.value	0x0000		/ segment base 0..15
	.byte	0x0		/ segment base 16..23; set for 0K
	.byte	0x92		/ flags; A=0, Type=001, DPL=00, P=1
				/        Present expand down
	.byte	0xCF		/ flags; Limit (16..19)=1111, AVL=0, G=1, B=1
	.byte	0x0		/ segment base 24..32

codedesc:			/ offset = 0x10    C_GDT

	.value	0xFFFF		/ segment limit 0..15
	.value	0x0000		/ segment base 0..15
	.byte	0x0		/ segment base 16..23; set for 0k
	.byte	0x9E		/ flags; A=0, Type=111, DPL=00, P=1
	.byte	0xCF		/ flags; Limit (16..19)=1111, AVL=0, G=1, D=1
	.byte	0x0		/ segment base 24..32

code16desc:			/ offset = 0x18    C16GDT

	.value	0xFFFF		/ segment limit 0..15
	.value	0x000		/ segment base 0..15
	.byte	0x0		/ segment base 16..23; set for 0k
	.byte	0x9E		/ flags; A=0, Type=111, DPL=00, P=1
	.byte	0x0F		/ flags; Limit (16..19)=1111, AVL=0, G=0, D=0
	.byte	0x0		/ segment base 24..32

datadesc:			/ offset = 0x20    D_GDT

	.value	0xFFFF		/ segment limit 0..15
	.value	0x0000		/ segment base 0..15
	.byte	0x0		/ segment base 16..23; set for 0K
	.byte	0x92		/ flags; A=0, Type=001, DPL=00, P=1
				/        Present expand down
	.byte	0x4F		/ flags; Limit (16..19)=1111, AVL=0, G=1, B=1
	.byte	0x0		/ segment base 24..32


/ 	In-memory GDT pointer for the lgdt call

	.globl GDTptr
	.globl gdtlimit
	.globl gdtbase

GDTptr:	
gdtlimit:
	.value	0x28		/ size of GDT table
gdtbase:
	.long	GDTstart

/ ic elements and offsets
/intval ax bx cx dx bp es si di ds
/0      2  4  6  8  10 12 14 16 18

	.globl	putchar_notused
putchar_notused:
	push	%ebp			/ save stack
	mov	%esp,%ebp
	pushl	%ebx
	pushl   %esi

	leal	ic, %esi		/ address int_pb struct ic
	movw	$1, 4(%esi)		/ page zero to bx
	movb	8(%ebp), %al		/ char to al
	movb	$14, %ah		/ teletype putchar
	movw    %ax, 2(%esi)		/ to ax
	movw	$0x10, 0(%esi)		/ to intval

	call	doint

	popl	%esi
	popl	%ebx
	pop	%ebp			/ restore frame pointer

	ret


/	--------------------------------------------
/ 	Call BIOS wait routine to wait for 100 msecond; programming the interval
/	timer directly does not seem to be reliable.
/ 	- decreased resolution to cut down on overhead of mode switching
/

	.globl	wait100ms
wait100ms:
	push	%ebp			/ C entry
	mov	%esp,%ebp
	push	%edi
	push	%esi
	push	%ebx

	leal    ic, %esi

	movw	$0x01, 6(%esi)		/ -> cx
	movw	$0x86a0, 8(%esi)	/ -> dx
	movw	$0x8600, 2(%esi)	/ ->ax setup for bios wait
	movw	$0x15, (%esi)           / -> intval:	BIOS utility function

	call	doint

	mov	$0, %eax

	pop	%ebx
	pop	%esi			/ C exit
	pop	%edi
	pop	%ebp

	ret


/	--------------------------------------------
/	memcpy(dest, src, cnt): works in exactly the same fashion as the 
/	libc memcpy; addresses are physaddr's.
	.globl	memcpy
memcpy:
	pushl	%edi
	pushl	%esi
	pushl	%ebx

	cld
	movl	16(%esp), %edi	/ %edi = dest address
	movl	20(%esp), %esi	/ %esi = source address
	movl	24(%esp), %ecx	/ %ecx = length of string
	movl	%edi, %eax	/ return value from the call

	movl	%ecx, %ebx	/ %ebx = number of bytes to move
	shrl	$2, %ecx	/ %ecx = number of words to move
	rep ; smovl		/ move the words

	movl	%ebx, %ecx	/ %ecx = number of bytes to move
	andl	$0x3, %ecx	/ %ecx = number of bytes left to move
	rep ; smovb		/ move the bytes

	popl	%ebx
	popl	%esi
	popl	%edi
	ret

/
/ doint_asm  --  Do a BIOS int call.
/	Called as doint(int intnum, struct real_regs *rp)
/
	.globl	doint_asm
doint_asm:
	pushl	%ebp
	movl	%esp,%ebp
	subl	$32,%esp
	pushl	%esi
	pushl	%edi
	pushl	%ebx
	pushl   %ecx
	pushl   %edx

	/
	/ Self-modifying code here that inserts proper interrupt for
	/ later.
	/
	movl	8(%ebp),%eax
	movb	%al,newintcode+1

	/
	/ Save regs pointer for later use
	/
	movl	12(%ebp),%esi

	/
	/ Jump here for P5 94 byte instruction prefetch queue
	/
	jmp	.doint_asm_qflush

.doint_asm_qflush:

	/
	/ If we are not using a stack in memory below 640K, switch
	/ to one.  Otherwise, maintain the status quo.
	/
	/ As I write this, I believe I can do the compare this way
	/ only as long as we use a big flat descriptor that has all
	/ the segments mapped to begin at address 0.  If that were not
	/ true, how would I get the effective address for the stack?
	/
	movl	%esp,%eax
	cmpl	$TOP_RMMEM,%eax
#ifdef notdef
XXX DEBUG XXX
	jbe	havelowstack
XXX DEBUG XXX always switch stacks for now...
#endif

	movl	%esp,%eax
	movl	%eax,-4(%ebp)
	movw    %ss,%ax
	movw    %ax,-10(%ebp)
	movl	$SBOOT_NEWINTSTACKLOC,%eax
	movl	%eax,%esp
	jmp	donelowstack

havelowstack:
	movl	$-1,-4(%ebp)

donelowstack:
	/
	/ Switch interrupt descriptor tables.
	/
	sidt    -26(%ebp)
	lidt	bioIDTptr

	/
	/ Save segment registers of caller.
	/
	movw	%cs,-12(%ebp)
	movw	%ds,-14(%ebp)
	movw    %es,-16(%ebp)
	movw	%fs,-18(%ebp)
	movw    %gs,-20(%ebp)

	/
	/ Save caller's gdt.
	/
	sgdt	-32(%ebp)

	/
	/ Save frame pointer on stack.
	/
	pushl	%ebp

	call	goreal

	/
	/ NOW back in REAL MODE
	/
	/ Clear the upper (extended) half of all registers,
	/ so that only low bits will be set while we are in
	/ real mode.  Having stray high bits on causes strange
	/ and unpredictable failures to occur in real mode.
	/
		addr16;	mov	36(%esi),%eax
			push	%eax	/ save input ds until just before int

	data16;		xorl	%eax,%eax
		addr16;	mov	(%esi),%eax	

	data16;		xorl	%ebx,%ebx
		addr16;	mov	12(%esi),%ebx

	data16;		xorl	%ecx,%ecx
		addr16;	mov	8(%esi),%ecx

	data16;		xorl	%edx,%edx
		addr16;	mov	4(%esi),%edx

	data16;		xorl	%ebp,%ebp
		addr16;	mov	16(%esi),%ebp

		addr16;	movw	38(%esi),%es

	data16;		xorl	%edi,%edi
		addr16;	mov	24(%esi),%edi

		addr16;	mov	20(%esi),%esi	/ fetch int call si value
	data16;		andl	$0xFFFF,%esi

			sti

			pop	%ds		/ set effective ds
	
newintcode:
			int	$0x10		/ do BIOS call

			cli
			pushf			/ save carry for later
			push	%ebp		/ save returned bp
			push	%esi		/ save returned si
			push	%ds		/ real mode - stack 2-bytes word
			push	%es
			push	%ebx
			push	%eax
			movw	%cs, %eax
			movw	%eax, %ds	/ restore entry ds, es
			movw	%eax, %es

	data16;		call	goprot		/ protect mode

	/
	/ NOW back in PROTECTED MODE.
	/

	movl	14(%esp),%ebp	/ get previous base pointer
	movl	12(%ebp),%esi	/ re-get regs ptr

	popw	%ax
	movw	%ax, (%esi)	/ register ax
	popw	%bx
	movw	%bx, 12(%esi)	/ register bx
	movw	%cx, 8(%esi)
	movw	%dx, 4(%esi)
	popw	%ax		/ register es
	movw	%ax, 38(%esi)
	popw	%ax		/ register ds
	movw	%ax, 36(%esi)
	movw	%di, 24(%esi)   / register di
	popw	%ax		/ get saved returned si
	movw	%ax, 20(%esi)
	popw	%ax		/ get saved returned bp
	movw	%ax, 16(%esi)
	popw	%ax		/ get flags
	movw	%ax, 44(%esi)
	pushw	%ax
	xorl	%eax, %eax	/ initialize return to zero
	popfw			/ get carry
	jnc	newdixt
	inc	%eax		/ return != 0 for carry set
	.globl	newdixt
newdixt:
	popl	%ebp

	/
	/ set up to restore the caller's cs
	/
	xorl	%ebx,%ebx
	movw	-12(%ebp),%bx
	pushl	%ebx
	pushl   $.doint_asm_flush

	/
	/ Interrupt descriptor table, then global 
	/ descriptor table.
	/
	lidt	-26(%ebp)
	lgdt    -32(%ebp)

	/
	/ Restore original code segment.
	/
	lret     

.doint_asm_flush:
	cli

	cmpl	$-1,-4(%ebp)
	je	donerestorestack

	/
	/ Restore caller's stack.
	/
	movw	-10(%ebp),%bx
	movw	%bx,%ss

	/
	/ Restore caller's stack pointer.
	/
	movl	-4(%ebp), %ebx
	movl	%ebx, %esp

donerestorestack:
	/
	/ Restore other segment registers of caller.
	/
	movw    -16(%ebp),%es
	movw    -18(%ebp),%fs
	movw    -20(%ebp),%gs
	movw    -14(%ebp),%ds

	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%edi
	popl	%esi
	addl	$32,%esp
	popl	%ebp
	ret

/
/	Dofar(addr)
/		Call real-mode subroutine pointed to by addr.  This version
/		not re-entrant because it uses global ic structure, and also
/		some global storage shared with doint.
/
	.globl	dofar
dofar:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%esi
	pushl	%edi
	pushl	%ebx
	pushl   %ecx
	pushl   %edx
	leal    ic,%esi
	movl	8(%ebp),%eax
	movl	%eax,%edi

	/ jump here for P5 94 byte instruction prefetch queue
	jmp	.dofar_qflush
.dofar_qflush:

	/ save caller's stack pointer and insert ours
	movl	%esp,%eax
	movl	%eax,dofar_save_esp
	movw    %ss,%ax
	movw    %ax,dofar_stand_ss
	movl	$SBOOT_FARSTACKLOC,%eax
	movl	%eax,%esp

	/ switch interrupt descriptor tables
	sidt    dofar_standIDTptr	
	lidt	bioIDTptr

	/ save segment registers of caller
	movw	%cs,dofar_stand_cs
	movw	%ds,dofar_stand_ds
	movw    %es,dofar_stand_es
	movw    %fs,dofar_stand_fs
	movw    %gs,dofar_stand_gs

	/ save caller's gdt
	sgdt    dofar_standGDTptr

	call	goreal		/ get real

	/ real mode...
//
// Clear the upper (extended) half of all registers, so that only low bits
// will be set while we are in real mode.  Having stray high bits on causes
// strange and unpredictable failures to occur in real mode.
//
// Also set up stack and registers for far call! We actually do the call
// by setting up a stack with two return addresses, and then back into the
// call by doing a far return.
//
	data16;		movl	$comeback, %eax
			push	%cs
			push	%eax

	data16;		movl	%edi, %eax
	data16;		shrl	$16, %eax
			push	%eax
			push	%edi

	data16;		xorl	%eax, %eax

		addr16; mov	18(%esi),%eax	/ setup registers for the call	
			push	%eax		/ save input ds

	data16;		xorl	%eax,%eax
		addr16; mov	2(%esi),%eax	

	data16;		xorl	%ebx,%ebx
		addr16; mov	4(%esi),%ebx

	data16;		xorl	%ecx,%ecx
		addr16; mov	6(%esi),%ecx

	data16;		xorl	%edx,%edx
		addr16; mov	8(%esi),%edx

	data16;		xorl	%ebp,%ebp
		addr16; mov	10(%esi),%ebp

		addr16; movw	12(%esi),%es

	data16;		xorl	%edi,%edi
		addr16; mov	16(%esi),%edi

		addr16; mov	14(%esi),%esi	/ fetch int call si value
	data16;		andl	$0xFFFF,%esi

			sti

			pop	%ds		/ set effective ds
callit:
			lret
comeback:
			cli

#ifdef	FLOWDEBUG
	/* print a '}' for debugging */
	data16;		push	%eax
	data16;		push	%ebx
	data16;		movl	$0x0e7d,%eax
	data16;		movl	$1,%ebx
			int	$0x10
	data16;		pop	%ebx
	data16;		pop	%eax
#endif	/* FLOWDEBUG */


			push	%esi		/ save returned esi
			push	%ds		/ real mode - stack 2-bytes word
			push	%es

			pushw	$0
			pop	%ds
			push	%cs
			pop	%es

	data16;		xorl	%esi,%esi

	data16;	addr16; leal	ic,%esi

		addr16; movw	%ax,2(%esi)	/ register ax
		addr16; movw	%bx,4(%esi)	/ register bx
		addr16; movw	%cx,6(%esi)
		addr16;	movw	%dx,8(%esi)
		addr16; movw	%bp,10(%esi)
			popw	%ax		/ register es
		addr16; movw	%ax,12(%esi)
			popw	%ax		/ register ds
		addr16; movw	%ax,18(%esi)
		addr16; movw	%di,16(%esi)   / register di
			popw	%ax		/ get saved returned esi
		addr16; movw	%ax,14(%esi)

	data16;		call	goprot		/ protect mode

	/ Back in protected mode...

	xorl	%eax,%eax
	leal    ic,%esi
	movw	2(%esi),%ax	/ return value

	/ set up to restore the caller's cs
	xorl	%ebx,%ebx
	movw	dofar_stand_cs,%bx
	pushl	%ebx
	pushl   $.dofar_flush

	/ interrupt descriptor table
	lidt    dofar_standIDTptr	

	/ restore caller's gdt
	lgdt    dofar_standGDTptr

	/ restore cs
	lret     
.dofar_flush:

	cli	/paranoia?

	/ restore caller's stack
	movw	dofar_stand_ss,%bx
	movw	%bx,%ss
	/ restore caller's stack pointer
	movl	dofar_save_esp,%ebx
	movl	%ebx,%esp

	/ restore other segment registers of caller
	movw    dofar_stand_es,%es
	movw    dofar_stand_fs,%fs
	movw    dofar_stand_gs,%gs
	movw    dofar_stand_ds,%ds

	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%edi
	popl	%esi
	popl	%ebp
	ret

/
/	Dofar_with_stack(addr)
/		Call real-mode subroutine pointed to by addr.  This version
/		not re-entrant because it uses global ic structure, and also
/		some global storage shared with doint.
/
	.globl	dofar_with_stack
dofar_with_stack:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%esi
	pushl	%edi
	pushl	%ebx
	pushl   %ecx
	pushl   %edx
	leal    ic,%esi
	movl	8(%ebp),%eax
	movl	%eax,%edi

	/ jump here for P5 94 byte instruction prefetch queue
	jmp	.dofar_with_stack_qflush
.dofar_with_stack_qflush:

	/ save caller's stack pointer and insert ours
	movl	%esp,%eax
	movl	%eax,dofar_save_esp
	movw    %ss,%ax
	movw    %ax,dofar_stand_ss
	movl	$SBOOT_FARSTACKLOC,%eax
	movl	%eax,%esp

	/ switch interrupt descriptor tables
	sidt    dofar_standIDTptr	
	lidt	bioIDTptr

	/ save segment registers of caller
	movw	%cs,dofar_stand_cs
	movw	%ds,dofar_stand_ds
	movw    %es,dofar_stand_es
	movw    %fs,dofar_stand_fs
	movw    %gs,dofar_stand_gs

	/ save caller's gdt
	sgdt    dofar_standGDTptr

	call	goreal		/ get real

	/ real mode...
//
// Clear the upper (extended) half of all registers, so that only low bits
// will be set while we are in real mode.  Having stray high bits on causes
// strange and unpredictable failures to occur in real mode.
//
// Also set up stack and registers for far call! We actually do the call
// by setting up a stack with two return addresses, and then back into the
// call by doing a far return.
//
	data16;		movl	%edi, %eax
	data16;		shrl	$16, %eax

	data16;		xorl	%ebx,%ebx
	data16;		xorl	%ecx,%ecx
		addr16;	movl	dofar_stack+2,%ebx	/ get new ss
	data16;		shll	$4,%ebx			/ build a 32 bit ptr
		addr16;	movl	dofar_stack,%ecx	/ with new sp
	data16;		addl	%ecx,%ebx

		addr16;	movl	%eax,-2(%ebx)	/ "push" new cs
		addr16;	movl	%edi,-4(%ebx)	/ "push" new ip

	data16;		xorl	%eax, %eax

		addr16; mov	18(%esi),%eax	/ setup registers for the call	
			push	%eax		/ save input ds

	data16;		xorl	%eax,%eax
		addr16; mov	2(%esi),%eax	

	data16;		xorl	%ebx,%ebx
		addr16; mov	4(%esi),%ebx

	data16;		xorl	%ecx,%ecx
		addr16; mov	6(%esi),%ecx

	data16;		xorl	%edx,%edx
		addr16; mov	8(%esi),%edx

	data16;		xorl	%ebp,%ebp
		addr16; mov	10(%esi),%ebp

		addr16; movw	12(%esi),%es

	data16;		xorl	%edi,%edi
		addr16; mov	16(%esi),%edi

		addr16; mov	14(%esi),%esi	/ fetch int call si value
	data16;		andl	$0xFFFF,%esi


			pop	%ds		/ set effective ds
		addr16;	movw	%ss,%cs:dofar_ostack+2
		addr16;	movl	%esp,%cs:dofar_ostack
		addr16;	movw	%cs:dofar_stack+2,%ss
		addr16;	movl	%cs:dofar_stack,%esp
			subl	$4,%esp		/ point at our pushed cs:ip
			sti
callit_with_stack:
			lret
			.globl	comeback_with_stack
comeback_with_stack:
			cli
		addr16;	movw	%cs:dofar_ostack+2,%ss
		addr16;	movl	%cs:dofar_ostack,%esp

#ifdef	FLOWDEBUG
	/* print a '}' for debugging */
	data16;		push	%eax
	data16;		push	%ebx
	data16;		movl	$0x0e7d,%eax
	data16;		movl	$1,%ebx
			int	$0x10
	data16;		pop	%ebx
	data16;		pop	%eax
#endif	/* FLOWDEBUG */


			push	%esi		/ save returned esi
			push	%ds		/ real mode - stack 2-bytes word
			push	%es

			pushw	$0
			pop	%ds
			push	%cs
			pop	%es

	data16;		xorl	%esi,%esi

	data16;	addr16; leal	ic,%esi

		addr16; movw	%ax,2(%esi)	/ register ax
		addr16; movw	%bx,4(%esi)	/ register bx
		addr16; movw	%cx,6(%esi)
		addr16;	movw	%dx,8(%esi)
		addr16; movw	%bp,10(%esi)
			popw	%ax		/ register es
		addr16; movw	%ax,12(%esi)
			popw	%ax		/ register ds
		addr16; movw	%ax,18(%esi)
		addr16; movw	%di,16(%esi)   / register di
			popw	%ax		/ get saved returned esi
		addr16; movw	%ax,14(%esi)

	data16;		call	goprot		/ protect mode

	/ Back in protected mode...

	xorl	%eax,%eax
	leal    ic,%esi
	movw	2(%esi),%ax	/ return value

	/ set up to restore the caller's cs
	xorl	%ebx,%ebx
	movw	dofar_stand_cs,%bx
	pushl	%ebx
	pushl   $.dofar_with_stack_flush

	/ interrupt descriptor table
	lidt    dofar_standIDTptr	

	/ restore caller's gdt
	lgdt    dofar_standGDTptr

	/ restore cs
	lret     
.dofar_with_stack_flush:

	cli	/paranoia?

	/ restore caller's stack
	movw	dofar_stand_ss,%bx
	movw	%bx,%ss
	/ restore caller's stack pointer
	movl	dofar_save_esp,%ebx
	movl	%ebx,%esp

	/ restore other segment registers of caller
	movw    dofar_stand_es,%es
	movw    dofar_stand_fs,%fs
	movw    dofar_stand_gs,%gs
	movw    dofar_stand_ds,%ds

	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%edi
	popl	%esi
	popl	%ebp
	ret

/
/	getXXX routines - load register XXX into a well known global variable.
/
	.globl cursp
cursp:	.long  0
	.globl curbp
curbp:	.long  0
	.globl curbx
curbx:	.long  0

	.globl getesp
getesp:
	movl	%esp,cursp
	ret
	.globl getebp
getebp:
	movl	%ebp,curbp
	ret
	.globl getebx
getebx:
	movl	%ebx,curbx
	ret
/
/	return the address of the caller - the value will
/	be within the bounds of the caller where it called you
/
	.globl caller
caller:
	movl	4(%ebp),%eax
	ret

/
/	int21chute -- Interrupt chute to protected mode.
/		Transfer processing of int 21 in real mode to our
/		protected mode handler.
/
	.globl	i21cnt
i21cnt:
	.long  0
	.globl	old21vec
old21vec:
	.long  0
i21_handled:
	.long  0
	.globl	i21_eax
i21_eax:
	.long  0
	.globl	i21_ds
i21_ds:
	.value  0

	.align 4
	.globl	i21_stack
i21_stack: 			/ int21 reentrant register stack
	.long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0
	.long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0
	.long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0
	.long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0

	.long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0
	.long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0
	.long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0
	.long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0

	.long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0
	.long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0
	.long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0
	.long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0; .long 0

	.globl	i21_base
i21_base:			/ current base of int21 reentrant stack
	.long  i21_stack

	.globl int21chute
int21chute:
	/ this routine starts in real-mode
			cli
	/ save ds on stack for a few instructions
			push	%ds
	/ copy cs to ds so we can save regs to our data area
			push	%cs
			pop	%ds

	/ save regs in struct real_regs for handle21()
	data16; addr16;	movl	%eax,i21_eax		/ tmp storage of eax

	data16; addr16; movl	i21_base,%eax		/ load base
	data16;		addl	$4,%eax
	data16; addr16;	movl	%edx,(%eax)		/ edx

	data16; addr16; movl	i21_base,%eax		/ load base
	data16; addr16;	movl	i21_eax,%edx		/ 
	data16; addr16;	movl	%edx,(%eax)		/ eax

	data16; addr16; movl	i21_base,%eax		/ load base
	data16;		addl	$4,%eax
	data16; addr16;	movl	(%eax),%edx		/ restore edx

	data16; addr16; movl	i21_base,%eax		/ load base
	data16;		addl	$8,%eax
	data16; addr16; movl	%ecx,(%eax)		/ ecx
	data16;		addl	$4,%eax
	data16; addr16; movl	%ebx,(%eax)		/ ebx
	data16;		addl	$4,%eax
	data16; addr16; movl	%ebp,(%eax)		/ ebp
	data16;		addl	$4,%eax
	data16; addr16; movl	%esi,(%eax)		/ esi
	data16;		addl	$4,%eax
	data16; addr16; movl	%edi,(%eax)		/ edi

	data16; addr16; movl	i21_base,%eax		/ load base
	data16;		addl	$34,%eax
		addr16;	movw	%ss,(%eax)		/ ss

	data16; addr16; movl	i21_base,%eax		/ load base
	data16;		addl	$38,%eax
		addr16;	movw	%es,(%eax)		/ es

	data16; addr16; movl	i21_base,%eax		/ load base
	data16;		addl	$36,%eax
			pop	%es		/ grab ds saved on stack above
		addr16;	movw	%es,(%eax)		/ ds

	data16; addr16; movl	i21_base,%eax		/ load base
	data16;		addl	$40,%eax
		addr16;	movw	%fs,(%eax)		/ fs
	data16;		addl	$2,%eax
		addr16;	movw	%gs,(%eax)		/ gs

	data16; addr16; movl	i21_base,%eax		/ load base
	data16;		addl	$48,%eax
			pop	%es		/ grab ip saved by int 21
		addr16;	movw	%es,(%eax)		/ ip

	data16; addr16; movl	i21_base,%eax		/ load base
	data16;		addl	$32,%eax
			pop	%es		/ grab cs saved by int 21
		addr16;	movw	%es,(%eax)		/ cs

	data16; addr16; movl	i21_base,%eax		/ load base
	data16;		addl	$44,%eax
			pop	%es		/ grab flags saved by int 21
		addr16;	movw	%es,(%eax)		/ fl

	data16; addr16; movl	i21_base,%eax		/ load base
	data16;		addl	$28,%eax
	data16; addr16; movl	%esp,(%eax)	/ save sp of stack now

	data16; addr16; movl	i21_base,%eax		/ load base
	data16; addr16;	movl	%eax,i21_eax	/ stow real_reg addr
	data16;		addl	$64,%eax	/ increment i21 base
	data16; addr16;	movl	%eax,i21_base

#ifdef	FLOWDEBUG
	/* spew a '<' using the BIOS: entry into int21chute */
	data16;		movl	$0x0e3c,%eax
	data16;		movl	$1,%ebx
			int	$0x10
#endif	/* FLOWDEBUG */

	/ set up idt for protected mode
	data16; addr16; lidt    standIDTptr	

	/ switch to protected mode
	data16;		xorl	%eax,%eax
			movw	%ss,%ax
	data16;		shll	$4,%eax
	data16;		addl	%eax,%esp
	data16;		xorl	%eax,%eax
			movw	%ax,%ss
	data16;		call	goprot

	/ protected mode: let handle21() do its thing
	/ can't do "pushl $i21_base" because as makes it a cmpsb
	movl	i21_eax,%eax
	pushl	%eax
	call	handle21
	addl	$4,%esp
	movl	%eax,i21_handled

	/ switch back to real-mode
	call	goreal

	/ return to real-mode idt
	data16; addr16; lidt	bioIDTptr

	data16; addr16;	movl	i21_base,%eax		/ restore old int1_base
	data16;		subl	$64,%eax
	data16; addr16;	movl	%eax,i21_base

	data16; addr16;	movl	i21_base,%eax		/ load base
	data16;		addl	$34,%eax
		addr16;	movw	(%eax),%ss

	data16; addr16;	movl	i21_base,%eax		/ load base
	data16;		addl	$28,%eax
	data16; addr16; movl	(%eax),%esp

#ifdef	FLOWDEBUG
	/* spew a '>' using the BIOS: exit from int21chute */
	data16;		movl	$0x0e3e,%eax
	data16;		movl	$1,%ebx
			int	$0x10
#endif	/* FLOWDEBUG */

	/ pull the registers back in from struct real_regs with handle21's mods
	data16; addr16;	movl	i21_base,%edx		/ load base
	data16; addr16;	movl	(%edx),%eax
	data16; addr16;	movl	%eax,i21_eax		/ tmp storage of eax

	data16; addr16;	movl	i21_base,%eax		/ load base
	data16;		addl	$36,%eax
		addr16;	movw	(%eax),%edx
		addr16;	movw	%edx,i21_ds		/ tmp storage of ds

	data16; addr16;	movl	i21_base,%eax		/ load base
	data16;		addl	$4,%eax
	data16; addr16; movl	(%eax),%edx
	data16;		addl	$4,%eax
	data16; addr16; movl	(%eax),%ecx
	data16;		addl	$4,%eax
	data16; addr16; movl	(%eax),%ebx
	data16;		addl	$4,%eax
	data16; addr16; movl	(%eax),%ebp
	data16;		addl	$4,%eax
	data16; addr16; movl	(%eax),%esi
	data16;		addl	$4,%eax
	data16; addr16; movl	(%eax),%edi

	data16; addr16;	movl	i21_base,%eax		/ load base
	data16;		addl	$38,%eax
		addr16;	movw	(%eax),%es
	data16;		addl	$2,%eax
		addr16;	movw	(%eax),%fs
	data16;		addl	$2,%eax
		addr16;	movw	(%eax),%gs

	data16; addr16;	movl	i21_base,%eax		/ load base
	data16;		addl	$44,%eax
		addr16;	movl	(%eax),%eax
			pushl	%eax		/ push flags for next iret

	data16; addr16;	movl	i21_base,%eax	/ load base
	data16;		addl	$32,%eax
		addr16;	movw	(%eax),%eax
			pushl	%eax		/ push cs for next iret

	data16; addr16;	movl	i21_base,%eax	/ load base
	data16;		addl	$48,%eax
		addr16;	movl	(%eax),%eax
			pushl	%eax		/ push ip for next iret

	/ everything restored except eax and ds
	/ see if we need to "chain" to the next int 21 handler
	data16; addr16;	movl	i21_handled,%eax
	data16;		andl	%eax,%eax
			jz	i21_not_handled

			sti

	/ restore the last two regs: eax and ds

	data16; addr16;	movl	i21_eax,%eax
		addr16;	movw	i21_ds,%ds

	/ all done
			iret

i21_not_handled:

#ifdef	FLOWDEBUG
	/* spew a '=' using the BIOS: "not handled" exit from int21chute */
	data16;		movl	$0x0e3d,%eax
	data16;		movl	$1,%ebx
			int	$0x10
	/ restore bx trashed by BIOS call
	data16; addr16;	movl	i21_base,%eax	/ load base
	data16;		addl 	$12,%eax
	data16; addr16; movl	(%eax),%ebx
#endif	/* FLOWDEBUG */

	/ put next int 21 handler's vector on stack for lret below
	data16;	addr16; movl	old21vec,%eax
	data16;		pushl	%eax

			sti
	/ restore the last two regs: eax and ds
	data16; addr16; movl	i21_eax,%eax
		addr16;	movw	i21_ds,%ds

	/ all done
			lret

int_0:
	int	$0x0

/ Called as doint15_32bit()
/ We can't use doint because we need to use 32 bits registers instead of 16
/
/		where: ic32 is global struct int_pb32 {
/					uint eax,ebx,ecx,edx,esi,edi;
/					     0   4   8   12, 16, 20
/					ushort bp,es,ds;
/					       24 26 28
/					}
/ caller has our 4 valid descriptors at the boottom of his GDT
/
	.globl	doint15_32bit
doint15_32bit:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi
	pushl	%edi
	pushl	%ebx
	pushl   %ecx
	pushl   %edx
	leal    ic32, %esi
	/ jump here for P5 94 byte instruction prefetch queue
	jmp	.doint15_qflush
.doint15_qflush:

	/ save caller's stack pointer and insert ours
	movl	%esp, %eax
	movl	%eax, save_esp
	movw    %ss, %ax
	movw    %ax, stand_ss
	movl	$SBOOT_INTSTACKLOC, %eax
	movl	%eax, %esp

	/ switch interrupt descriptor tables
	sidt    standIDTptr	
	lidt	bioIDTptr

	/ save segment registers of caller
	movw	%cs, stand_cs
	movw	%ds, stand_ds
	movw    %es, stand_es
	movw    %fs, stand_fs
	movw    %gs, stand_gs

	/ save caller's gdt
	sgdt    standGDTptr

	call	goreal		/ get real
/ clear the upper (extended) half of all registers, so that only low bits
/ will be set while we are in real mode.  Having stray high bits on causes
/ strange and unpredictable failures to occur in real mode.
	addr16
	mov	28(%esi),%eax	/ setup registers for the call	
	push	%eax		/ save input ds

	addr16
	data16
	mov	(%esi),%eax	

	addr16
	data16
	mov	4(%esi),%ebx

	addr16
	data16
	mov	8(%esi),%ecx

	addr16
	data16
	mov	12(%esi),%edx

	data16
	xorl	%ebp,%ebp
	addr16
	mov	24(%esi),%ebp

	addr16
	movw	26(%esi),%es

	data16
	xorl	%edi,%edi
	addr16
	data16
	mov	20(%esi),%edi

	addr16
	data16
	mov	16(%esi),%esi	/ fetch int call si value

	sti

	pop	%ds		/ set effective ds
	int	$0x15		/ do BIOS int 15 call
	cli
	pushf			/ save carry for later
	data16
	push	%esi		/ save returned esi
	push	%ds		/ real mode - stack 2-bytes word
	push	%es
	data16
	push	%ebx
	data16
	push	%eax
	movw	%cs, %eax
	movw	%eax, %ds	/ restore entry ds, es
	movw	%eax, %es
	data16
	call	goprot		/ protect mode
	leal    ic32, %esi 	/ re-get int_pb ptr
	popl	%eax
	movl	%eax, (%esi)	/ register eax
	popl	%ebx
	movl	%ebx, 4(%esi)	/ register ebx
	movl	%ecx, 8(%esi)	/ register ecx
	movl	%edx,12(%esi)	/ register edx
	movl	%edi,20(%esi)   / register edi
	movw	%bp, 24(%esi)	/ register bp
	popw	%ax		/ register es
	movw	%ax, 26(%esi)
	popw	%ax		/ register ds
	movw	%ax, 28(%esi)
	popl	%eax		/ get saved returned esi
	movl	%eax,16(%esi)
	xorl	%eax, %eax	/ initalize return to zero
	popfw			/ get carry
	jnc	int15_ret	
	inc	%eax		/ return != 0 for carry set

	.globl 	int15_ret
int15_ret:
	/ set up to restore the caller's cs
	xorl	%ebx, %ebx
	movw	stand_cs, %bx
	pushl	%ebx
	pushl   $.doint15_flush

	/ interrupt descriptor table
	lidt    standIDTptr	

	/ restore caller's gdt
	lgdt    standGDTptr

	/ restore stand_cs
	lret     
.doint15_flush:

	cli	/paranoia?

	/restore caller's stack
	movw	stand_ss, %bx
	movw	%bx, %ss
	/ restore caller's stack pointer
	movl	save_esp, %ebx
	movl	%ebx, %esp

	/ restore other segment registers of caller
	movw    stand_es, 	%es
	movw    stand_fs, 	%fs
	movw    stand_gs, 	%gs
	movw    stand_ds, 	%ds

	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%edi
	popl	%esi
	popl	%ebp
	ret

/*
 *  exitto is called from main() and does 1 things
 *	It then jumps directly to the just-loaded standalone.
 *	There is NO RETURN from exitto(). ????
 */

.globl bop
bop:
	.long	0

#endif /* !defined(lint) */

#if defined(lint)

/* ARGSUSED */
void
exitto(int (*entrypoint)())
{}

#else	/* lint */

	ENTRY(exitto)
	/.globl exitto
/exitto:
	push	%ebp			/ save stack
	mov	%esp,%ebp
	pushal				/ protect secondary boot

	movl	%esp, %eax
	movl	%eax, save_esp2

	/holds address of ebvec structure
	movl	$elfbootvec, %eax
	pushl   (%eax)

	/holds address of bootops structure
	movl	$bop, %eax
	movl	(%eax), %ebx
	pushl   %ebx

	/ no debug vector
	pushl	$0

	/holds address of array of pointers to functions
	movl	$sysp, %eax
	movl	(%eax), %ecx
	pushl   %ecx

	movl	8(%ebp), %eax		
	call   *%eax

	movl	save_esp2, %eax
	movl	%eax, %esp

	popal
	pop	%ebp			/ restore frame pointer

	ret
	SET_SIZE(exitto)
#endif

#if !defined(lint)
/*
 *	protpanic -- called when an unexpected trap occurs
 */
	.text
	.align	4
	.globl	pp_table
pp_table:

	GLOBAL_LONG(pp_gs)
	GLOBAL_LONG(pp_fs)
	GLOBAL_LONG(pp_es)
	GLOBAL_LONG(pp_ds)
	GLOBAL_LONG(pp_edi)
	GLOBAL_LONG(pp_esi)
	GLOBAL_LONG(pp_ebp)
	GLOBAL_LONG(pp_esp)
	GLOBAL_LONG(pp_ebx)
	GLOBAL_LONG(pp_edx)
	GLOBAL_LONG(pp_ecx)
	GLOBAL_LONG(pp_eax)
	GLOBAL_LONG(pp_trapno)
	GLOBAL_LONG(pp_err)
	GLOBAL_LONG(pp_eip)
	GLOBAL_LONG(pp_cs)
	GLOBAL_LONG(pp_efl)
	GLOBAL_LONG(pp_uesp)
	GLOBAL_LONG(pp_ss)

	.globl	pp_hextable
pp_hextable:
	.string	"0123456789abcdef"

	.globl	protpanic
protpanic:
	cli

	/* save registers in safe place */
	movw	%gs,pp_gs
	movw	%fs,pp_fs
	movw	%es,pp_es
	movw	%ds,pp_ds
	movw	%ss,pp_ss
	movl	%edi,pp_edi
	movl	%esi,pp_esi
	movl	%ebp,pp_ebp
	movl	%esp,pp_esp
	movl	%ebx,pp_ebx
	movl	%edx,pp_edx
	movl	%ecx,pp_ecx
	movl	%eax,pp_eax
	popl	%eax
	movl	%eax,pp_err
	popl	%eax
	movl	%eax,pp_eip
	popl	%eax
	movl	%eax,pp_cs

	/* go to safe stack */
	movl	$SBOOT_INTSTACKLOC, %eax
	movl	%eax, %esp

	/* return to real-mode idt */
	lidt	bioIDTptr

	/ inline of goreal...
	ljmp	$C16GDT, $pp_set16cs
pp_set16cs:			/ 16 bit addresses and operands 
	data16;		movl	$D_GDT, %eax
			movw	%ax, %es
			movw	%ax, %ds
			mov	%cr0, %eax
	data16;		and 	$NOPROTMASK, %eax
			mov	%eax, %cr0
			ljmp	$0x0, $pp_restorecs
pp_restorecs:
			mov	%cr3, %eax
			mov	%eax, %cr3
			movw	%cs, %ax
			movw	%ax, %ss
			movw	%ax, %ds
			movw	%ax, %es
			movw	%ax, %fs
			movw	%ax, %gs

			RMPUTS("boot panic:\n\rtrp ");
			RMPUT_MEMLOC(pp_trapno)
			RMPUTS(" err ");
			RMPUT_MEMLOC(pp_err)
			RMPUTS(" eip ");
			RMPUT_MEMLOC(pp_eip)
			RMPUTS("  cs ");
			RMPUT_MEMLOC(pp_cs)
			RMPUTS("\n\resp ");
			RMPUT_MEMLOC(pp_esp);
			RMPUTS("  ss ");
			RMPUT_MEMLOC(pp_ss);
			RMPUTS("  ds ");
			RMPUT_MEMLOC(pp_ds);
			RMPUTS("  es ");
			RMPUT_MEMLOC(pp_es);
			RMPUTS("  fs ");
			RMPUT_MEMLOC(pp_fs);
			RMPUTS("  gs ");
			RMPUT_MEMLOC(pp_gs);
			RMPUTS("\n\reax ");
			RMPUT_MEMLOC(pp_eax);
			RMPUTS(" ebx ");
			RMPUT_MEMLOC(pp_ebx);
			RMPUTS(" ecx ");
			RMPUT_MEMLOC(pp_ecx);
			RMPUTS(" edx ");
			RMPUT_MEMLOC(pp_edx);
			RMPUTS("\n\resi ");
			RMPUT_MEMLOC(pp_esi);
			RMPUTS(" edi ");
			RMPUT_MEMLOC(pp_edi);
			RMPUTS(" ebp ");
			RMPUT_MEMLOC(pp_ebp);
			RMPUTS("\n\rcr0 ");
			movl	%cr0,%esi
			RMPUT_ESI
			RMPUTS(" cr2 ");
			movl	%cr2,%esi
			RMPUT_ESI
			RMPUTS(" cr3 ");
			movl	%cr3,%esi
			RMPUT_ESI
			RMPUTS("\n\rgdtr ");
			addr16
			data16
			sgdt	pp_eax
			addr16
			data16
			movl	pp_eax,%esi
			RMPUT_ESI
			addr16
			data16
			movl	pp_eax+4,%esi
			RMPUT_ESI_TOPHALF
			RMPUTS(" idtr ");
			addr16
			data16
			sgdt	pp_eax
			addr16
			data16
			movl	pp_eax,%esi
			RMPUT_ESI
			addr16
			data16
			movl	pp_eax+4,%esi
			RMPUT_ESI_TOPHALF
			RMPUTS("\n\r");
			sti

			/* try jumping to debugger */

	data16; addr16;	movl	$0xFE00, %eax
	data16; addr16;	movl	$0xFF, %ebx
			int	$0x21

			/* hang forever... */
			jmp	.

/* ********************************************************************* */
/* */
/*	munge_table: */
/*		This procedure will 'munge' a descriptor table to */
/*		change it from initialized format to runtime format. */
/* */
/*		Assumes: */
/*			%eax -- contains the base address of table. */
/*			%ecx -- contains size of table. */
/* */
/* ********************************************************************* */
munge_table:

	addl	%eax, %ecx	/* compute end of IDT array */
	movl	%eax, %esi	/* beginning of IDT */

moretable:
	cmpl	%esi, %ecx
	jl	donetable	/* Have we done every descriptor?? */

	movl	%esi, %ebx	/*long-vector/*short-selector/*char-rsrvd/*char-type */
			
	movb	7(%ebx), %al	/* Find the byte containing the type field */
	testb	$0x10, %al	/* See if this descriptor is a segment */
	jne	notagate
	testb	$0x04, %al	/* See if this destriptor is a gate */
	je	notagate
				/* Rearrange a gate descriptor. */
	movl	4(%ebx), %edx	/* Selector, type lifted out. */
	movw	2(%ebx), %ax	/* Grab Offset 16..31 */
	movl	%edx, 2(%ebx)	/* Put back Selector, type */
	movw	%ax, 6(%ebx)	/* Offset 16..31 now in right place */
	jmp	descdone

notagate:			/* Rearrange a non gate descriptor. */
	movw	4(%ebx), %dx	/* Limit 0..15 lifted out */
	movw	6(%ebx), %ax	/* acc1, acc2 lifted out */
	movb	%ah, 5(%ebx)	/* acc2 put back */
	movw	2(%ebx), %ax	/* 16-23, 24-31 picked up */
	movb	%al, 7(%ebx)	/* 24-31 put back */
	movb	%ah, 4(%ebx)	/* 16-23 put back */
	movw	(%ebx), %ax	/* base 0-15 picked up */
	movw	%ax, 2(%ebx)	/* base 0-15 put back */
	movw	%dx, (%ebx)	/* lim 0-15 put back */

descdone:
	addl	$8, %esi	/* Go for the next descriptor */
	jmp	moretable

donetable:
	ret

	.globl	pagestart
pagestart:			/* turn on paging - see dosemul.c */
	movl	%cr3, %eax
	movl	%eax, %cr3
	nop
	nop
	movl	%cr0, %eax
	orl	cr0mask, %eax
	movl	%eax, %cr0 
	jmp ptflush
ptflush:
	nop
	nop
	ret

	.globl	pagestop
pagestop:			/* turn off paging - see dosemul.c */
	movl	%cr3, %eax
	movl	%eax, %cr3
	nop
	nop
	movl	%cr0, %eax
	andl	$-1![CR0_PG], %eax
	movl	%eax, %cr0 
	jmp ppflush
ppflush:
	nop
	nop
	ret

/	--------------------------------------------
/	dregister(dr0, dr7): absolutely primitive load debug registers
/	This is currently only works once. The debug
/	trap is treated as a panic. This should be replaced by
/	a subset of kadb functionality asap for boot-panic debugging.
/
	.globl	dregister
dregister:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%edi
	pushl	%esi

	movl	8(%ebp), %edi	/ %edi = dr0
	movl	%edi, %dr0
	movl	12(%ebp), %esi	/ %esi = dr7
	movl	%esi, %dr7

	popl	%esi
	popl	%edi
	leave
	ret

#endif /* !defined(lint) */

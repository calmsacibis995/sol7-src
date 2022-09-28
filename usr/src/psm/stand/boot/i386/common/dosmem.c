/*
 * Copyright (c) 1997, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ident "@(#)dosmem.c	1.8	97/11/24 SMI"

#include <sys/types.h>
#include <sys/ramfile.h>
#include <sys/doserr.h>
#include <sys/dosemul.h>
#include <sys/param.h>
#include <sys/booti386.h>
#include <sys/salib.h>
#include <sys/promif.h>

extern int int21debug;
extern int ldmemdebug;
extern int rm_resize(caddr_t vad, size_t oldsz, size_t newsz);
extern void rm_free(caddr_t, size_t);
extern caddr_t rm_malloc(size_t size, u_int align, caddr_t virt);
extern long rm_maxblock();

/*
 * Global for handling memory requests from real mode modules.
 */
static dmcl_t *DOSmemreqs;

dmcl_t *
findmemreq(int seg, dmcl_t **prev)
{
	int found = 0;
	dmcl_t *f, *p;

	*prev = p = (dmcl_t *)NULL;
	f = DOSmemreqs;
	while (f) {
		if (f->seg == seg) {
			found = 1;
			break;
		}
		p = f;
		f = f->next;
	}
	if (found) {
		*prev = p;
		return (f);
	} else
		return ((dmcl_t *)NULL);
}

void
delmemreq(dmcl_t *db, dmcl_t *pb)
{
	if (db) {
		/*
		 * Remove block from global request list
		 */
		if (pb)
			pb->next = db->next;
		else
			DOSmemreqs = db->next;
	}
}

void
addmemreq(dmcl_t *mc)
{
	if (!DOSmemreqs)
		DOSmemreqs = mc;
	else {
		dmcl_t *f;
		for (f = DOSmemreqs; f->next; f = f->next);
		f->next = mc;
	}
}

void
markpars(int seg, int size)
{
	dmcl_t *nb;

	if ((nb = (dmcl_t *)bkmem_alloc(sizeof (dmcl_t))) == NULL) {
		prom_panic("no memory for tracking low memory!");
	}

	nb->next = (dmcl_t *)NULL;
	nb->size = size;
	nb->seg = seg;
	nb->addr = (void *)mk_ea(seg, 0);

	addmemreq(nb);
}

void
unmarkpars(int seg)
{
	dmcl_t *fb, *pb;

	if ((fb = findmemreq(seg, &pb)) != (dmcl_t *)NULL) {
		delmemreq(fb, pb);
		(void) bkmem_free((caddr_t)fb, sizeof (*fb));
	} else {
		printf("WARNING: Request to free ");
		printf("non-allocated segment (%x).\n", seg);
	}
}

int
parmarklkupsize(int seg)
{
	dmcl_t *fb, *pb;

	return (
	    ((fb = findmemreq(seg, &pb)) == (dmcl_t *)NULL) ? -1 : fb->size);
}

void
dosallocpars(struct real_regs *rp)
{
	caddr_t newblk;
	ulong needed = BX(rp) * PARASIZE;

	if (int21debug || ldmemdebug)
		printf("alloc-paras: need 0x%x bytes:", needed);

	/*
	 * See if caller is trying to find largest available block
	 */
	if (needed == 0xffff0) {
		AX(rp) = DOSERR_INSUFFICIENT_MEMORY;
		BX(rp) = rm_maxblock() >> 4;
		SET_CARRY(rp);
		return;
	}
	if ((newblk = rm_malloc(needed, 0, 0)) == NULL) {
		if (int21debug || ldmemdebug)
			printf("(failed->rm_mem)");
		AX(rp) = DOSERR_INSUFFICIENT_MEMORY;
		BX(rp) = 0;
		SET_CARRY(rp);
	} else {
		if (int21debug || ldmemdebug)
			printf("New blk seg 0x%x, 0x%x bytes. ",
			    segpart((ulong)newblk), needed);
		markpars(segpart((ulong)newblk), needed);
		AX(rp) = segpart((ulong)newblk);
		CLEAR_CARRY(rp);
	}
}

void
dosfreepars(struct real_regs *rp)
{
	dmcl_t *fc, *pc;

	if (int21debug || ldmemdebug)
		printf("dosfreepars @ %x", rp->es);

	if (fc = findmemreq(rp->es, &pc)) {
		if (ldmemdebug)
			printf("FREE@0x%x, size = 0x%x\n", fc->addr, fc->size);
		rm_free(fc->addr, fc->size);
		if (ldmemdebug)
			printf("FREE@0x%x, size = 0x%x\n", fc->addr, fc->size);
		unmarkpars(rp->es);
		CLEAR_CARRY(rp);
	} else {
		if (int21debug)
			printf("(failed->seg not alloced)");
		AX(rp) = DOSERR_MEMBLK_ADDR_BAD;
		SET_CARRY(rp);
	}
}

void
dosreallocpars(struct real_regs *rp)
{
	dmcl_t *fb, *dc;
	int seg = rp->es;
	int ns;

	SET_CARRY(rp);	/* Assume failure */
	ns = BX(rp)*PARASIZE;

	if (int21debug || ldmemdebug)
		printf("Resize memory: new size %x, seg %x ", ns, seg);

	/*
	 * See if caller is trying to find largest available block
	 */
	if (ns == 0xffff0) {
		AX(rp) = DOSERR_INSUFFICIENT_MEMORY;
		BX(rp) = rm_maxblock() >> 4;
		SET_CARRY(rp);
		return;
	}
	if (fb = findmemreq(seg, &dc)) {
		if (ns <= fb->size) {
			if (int21debug || ldmemdebug)
				printf("(shrink req ok)");
			(void) rm_resize((caddr_t)(fb->seg * PARASIZE),
				fb->size, ns);
			fb->size = ns;
			CLEAR_CARRY(rp);
		} else {
			if (rm_resize((caddr_t)(fb->seg * PARASIZE),
			    fb->size, ns) < 0) {
				if (int21debug || ldmemdebug)
					printf("(rm_resize fail)");
				AX(rp) = DOSERR_INSUFFICIENT_MEMORY;
				BX(rp) = 0;
			} else {
				fb->size = ns;
				CLEAR_CARRY(rp);
				if (int21debug)
					printf("(resize succeed)");
				if (ldmemdebug)
					printf("RESZ@0x%x, new size = 0x%x. ",
					    fb->seg*PARASIZE, ns);
			}
		}
	} else {
		if (int21debug)
			printf("(fail)");
		AX(rp) = DOSERR_MEMBLK_ADDR_BAD;
		BX(rp) = 0;
	}
}

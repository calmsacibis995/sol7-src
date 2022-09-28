/*
 * Copyright (c) 1993 - 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PRT_XXX_H
#define	_PRT_XXX_H

#pragma ident	"@(#)prt_xxx.h	1.5	98/01/24 SMI"

#include <sys/pctypes.h>
#include <sys/pcmcia.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Printing functions, as many as there are different structures
 */
void obio_print();
void sbus_print();
void pcmcia_print();

/*
 * This is hardcoded. You may add entries here and in init_priv_data().
 * Of course, do it at your own risk.
 */
struct di_priv_format ppd_format[] = {
	{	/*
		 * obio format: applies to most of nexus drivers
		 * We include all nexus drivers in on297:
		 *   -- any driver that defines non-NULL bus_ops
		 *   -- any driver that calls scsi_hba_init()
		 * Exceptions:
		 *   -- pseudo is a nexus driver, but we know it
		 *	has no parent private data
		 *
		 * Are there more nexus drivers?
		 */
		"bootbus central dma ebus eisa esp fas fhc glm iommu isa"
		" isp lebuffer ledma mc obio pci pci_pci pln qec rootnex"
		" sf soc socal xbox",
		sizeof (struct ddi_parent_private_data),

		sizeof (struct regspec),		/* first pointer */
		offsetof(struct ddi_parent_private_data, par_reg),
		offsetof(struct ddi_parent_private_data, par_nreg),

		sizeof (struct intrspec),		/* second pointer */
		offsetof(struct ddi_parent_private_data, par_intr),
		offsetof(struct ddi_parent_private_data, par_nintr),

		sizeof (struct rangespec),		/* third pointer */
		offsetof(struct ddi_parent_private_data, par_rng),
		offsetof(struct ddi_parent_private_data, par_nrng),

		0, 0, 0,	/* no more pointers */
		0, 0, 0
	},

	{	/* pcmcia format */
		"pcic stp4020",
		sizeof (struct pcmcia_parent_private),

		sizeof (struct pcm_regs),		/* first pointer */
		offsetof(struct pcmcia_parent_private, ppd_reg),
		offsetof(struct pcmcia_parent_private, ppd_nreg),

		sizeof (struct intrspec),		/* second pointer */
		offsetof(struct pcmcia_parent_private, ppd_intrspec),
		offsetof(struct pcmcia_parent_private, ppd_intr),

		0, 0, 0,	/* no more pointers */
		0, 0, 0,
		0, 0, 0
	},

	{	/* sbus format--it's different on sun4u!! */
		"sbus",
		sizeof (struct ddi_parent_private_data),

		sizeof (struct regspec),		/* first pointer */
		offsetof(struct ddi_parent_private_data, par_reg),
		offsetof(struct ddi_parent_private_data, par_nreg),

		sizeof (struct intrspec),		/* second pointer */
		offsetof(struct ddi_parent_private_data, par_intr),
		offsetof(struct ddi_parent_private_data, par_nintr),

		sizeof (struct rangespec),		/* third pointer */
		offsetof(struct ddi_parent_private_data, par_rng),
		offsetof(struct ddi_parent_private_data, par_nrng),

		0, 0, 0,	/* no more pointers */
		0, 0, 0
	}
};

struct priv_data {
	char *drv_name;		/* parent name */
	void (*pd_print)();	/* print function */
} prt_priv_data[] = {
	{ ppd_format[0].drv_name, obio_print},
	{ ppd_format[1].drv_name, pcmcia_print},
	{ ppd_format[2].drv_name, sbus_print}
};

int nprt_priv_data = sizeof (prt_priv_data)/sizeof (struct priv_data);

#ifdef	__cplusplus
}
#endif

#endif	/* _PRT_XXX_H */

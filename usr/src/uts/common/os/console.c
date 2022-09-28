/*
 * Copyright (c) 1989-1993, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)console.c	1.20	97/11/22 SMI"

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/vnode.h>
#include <sys/console.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/esunddi.h>

#define	MINLINES	10
#define	MAXLINES	48
#define	LOSCREENLINES	34
#define	HISCREENLINES	48

#define	MINCOLS		10
#define	MAXCOLS		120
#define	LOSCREENCOLS	80
#define	HISCREENCOLS	120

vnode_t *ltemvp = NULL;
dev_t ltemdev;

/*
 * Gets the number of rows and columns (in char's) and the
 * width and height (in pixels) of the console.
 */
void
console_get_size(ushort *r, ushort *c, ushort *x, ushort *y)
{
	u_char *data;
	u_char *p;
	static char *cols = "screen-#columns";
	static char *rows = "screen-#rows";
	static char *width = "screen-width";
	static char *height = "screen-height";
	u_int len;
	int	rel_needed = 0;
	dev_info_t *dip;
	dev_t dev;

	/*
	 * If we have loaded the console IO stuff, then ask for the screen
	 * size properties from the layered terminal emulator.  Else ask for
	 * them from the root node, which will eventually fall through to the
	 * options node and get them from the prom.
	 */
	if (ltemvp == NULL) {
		dip = ddi_root_node();
		dev = DDI_DEV_T_ANY;
	} else {
		dip = e_ddi_get_dev_info(ltemdev, VCHR);
		dev = ltemdev;
		rel_needed = 1;
	}

	/*
	 * If we have not initialized a console yet and don't have a root
	 * node (ie. we have not initialized the DDI yet) return our default
	 * size for the screen.
	 */
	if (dip == NULL) {
		*r = LOSCREENLINES;
		*c = LOSCREENCOLS;
		*x = *y = 0;
		return;
	}

	*c = 0;
	/*
	 * Get the number of columns
	 */
	if (ddi_prop_lookup_byte_array(dev, dip, 0, cols, &data, &len) ==
	    DDI_PROP_SUCCESS) {
		p = data;
		data[len] = '\0';
		*c = stoi((char **)&p);
		ddi_prop_free(data);
	}

	if (*c < MINCOLS)
		*c = LOSCREENCOLS;
	else if (*c > MAXCOLS)
		*c = HISCREENCOLS;

	*r = 0;
	/*
	 * Get the number of rows
	 */
	if (ddi_prop_lookup_byte_array(dev, dip, 0, rows, &data, &len) ==
	    DDI_PROP_SUCCESS) {
		p = data;
		data[len] = '\0';
		*r = stoi((char **)&p);
		ddi_prop_free(data);
	}

	if (*r < MINLINES)
		*r = LOSCREENLINES;
	else if (*r > MAXLINES)
		*r = HISCREENLINES;

	*x = 0;
	/*
	 * Get the number of pixels wide
	 */
	if (ddi_prop_lookup_byte_array(dev, dip, 0, width, &data, &len) ==
	    DDI_PROP_SUCCESS) {
		p = data;
		data[len] = '\0';
		*x = stoi((char **)&p);
		ddi_prop_free(data);
	}

	*y = 0;
	/*
	 * Get the number of pixels high
	 */
	if (ddi_prop_lookup_byte_array(dev, dip, 0, height, &data, &len) ==
	    DDI_PROP_SUCCESS) {
		p = data;
		data[len] = '\0';
		*y = stoi((char **)&p);
		ddi_prop_free(data);
	}

	if (rel_needed)
		ddi_rele_driver(getmajor(ltemdev));

}

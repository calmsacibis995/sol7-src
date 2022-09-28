/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)prom_node.c	1.5	96/12/10 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/prom_emul.h>
#include <sys/obpdefs.h>

#if !defined(KADB) && !defined(I386BOOT)

extern prom_node_t *top_prom_node;
extern struct prom_prop *get_prom_prop(prom_node_t *pnp, char *name);
extern void prom_fatal_error(const char *);

/*
 * Routines for walking the PROMs devinfo tree
 */

/*
 * Return the root nodeid.
 * Calling prom_nextnode(0) returns the root nodeid.
 */
dnode_t
prom_rootnode(void)
{
	static dnode_t rootnode;

	return (rootnode ? rootnode : (rootnode = prom_nextnode(OBP_NONODE)));
}

static dnode_t
prom_topnode()
{
	return ((dnode_t)top_prom_node);
}


static int
find_node(prom_node_t *pnp, prom_node_t *pnp2)
{
	if (pnp == pnp2)
		return (1);
	if (pnp) {
		if (find_node(pnp->pn_child, pnp2))
			return (1);
		if (find_node(pnp->pn_sibling, pnp2))
			return (1);
	}
	return (0);
}

/*
 * Called to validate a prom node pointer, since that is what we hand
 * to the user for nodeid's.  Returns true if a valid pointer.
 */
static int
prom_checkptr(prom_node_handle_t handle)
{
	if (top_prom_node == NULL) {
		/* not yet copied tree */
		return (0);
	}
	if (handle != 0) {
		if (!find_node(prom_topnode(), handle))
			return (0);
	}
	/* node id 0 means the top */
	return (1);
}

dnode_t
prom_findnode_byname(dnode_t id, char *name)
{
	prom_node_t *pnp;
	dnode_t npnp;
	struct prom_prop *propp;

	pnp = (prom_node_t *)id;
	if (pnp == NULL)
		return (OBP_NONODE);
	if ((propp = get_prom_prop(pnp, "name")) != NULL)
		if (prom_strncmp(name, propp->pp_val, propp->pp_len) == 0)
			return ((dnode_t)pnp);
	if ((npnp = prom_findnode_byname(pnp->pn_child, name)) != OBP_NONODE)
		return (npnp);
	if ((npnp = prom_findnode_byname(pnp->pn_sibling, name)) != OBP_NONODE)
		return (npnp);
	return (OBP_NONODE);
}

dnode_t
prom_chosennode(void)
{
	static dnode_t chosen;
	dnode_t	node;

	if (chosen)
		return (chosen);

	node = prom_findnode_byname(prom_rootnode(), "chosen");
	return (chosen = node);
}

dnode_t
prom_optionsnode(void)
{
	static dnode_t options;
	dnode_t	node;

	if (options)
		return (options);

	node = prom_findnode_byname(prom_rootnode(), "options");
	return (options = node);
}

dnode_t
prom_nextnode(dnode_t nodeid)
{
	prom_node_t *pnp = (prom_node_t *)nodeid;

	if (!prom_checkptr(pnp))
		return (OBP_NONODE);
	if (pnp == NULL)
		return (prom_topnode());
	if (pnp->pn_sibling == NULL)
		return (OBP_NONODE);
	return ((dnode_t)pnp->pn_sibling);
}

dnode_t
prom_childnode(register dnode_t nodeid)
{
	prom_node_t *pnp = (prom_node_t *)nodeid;

	if (!prom_checkptr(pnp))
		return (OBP_NONODE);
	if (pnp == NULL)
		return (OBP_NONODE);
	if (pnp->pn_child == NULL)
		return (OBP_NONODE);
	return ((dnode_t)pnp->pn_child);
}

/*
 * Gets a token from a prom pathname, collecting evertyhing till a non-comma
 * seperator is found.
 */
static char *
prom_gettoken(char *tp, char *token)
{
	for (;;) {
		tp = prom_path_gettoken(tp, token);
		token += prom_strlen(token);
		if (*tp == ',') {
			*token++ = *tp++;
			*token = '\0';
			continue;
		}
		break;
	}
	return (tp);
}

/*
 * Get node id of node in prom tree that path identifies
 */
dnode_t
prom_finddevice(char *path)
{
	char name[OBP_MAXPROPNAME];
	char addr[OBP_MAXPROPNAME];
	char pname[OBP_MAXPROPNAME];
	char paddr[OBP_MAXPROPNAME];
	char *tp;
	dnode_t np, device;

	tp = path;
	np = prom_rootnode();
	device = OBP_BADNODE;
	if (*tp++ != '/')
		goto done;
	for (;;) {
		tp = prom_gettoken(tp, name);
		if (*name == '\0')
			break;
		if (*tp == '@') {
			tp++;
			tp = prom_gettoken(tp, addr);
		} else {
			addr[0] = '\0';
		}
		if ((np = prom_childnode(np)) == OBP_NONODE)
			break;
		while (np != OBP_NONODE) {
			if (prom_getprop(np, OBP_NAME, pname) < 0)
				goto done;
			if (prom_getprop(np, "unit-address", paddr) < 0)
				paddr[0] = '\0';
			if (prom_strcmp(name, pname) == 0 &&
				prom_strcmp(addr, paddr) == 0)
				break;
			np = prom_nextnode(np);
		}
		if (np == OBP_NONODE)
			break;
		if (*tp == '\0') {
			device = np;
			break;
		} else {
			tp++;
		}
	}
done:
	return (device);
}
#endif

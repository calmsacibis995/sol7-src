/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)tsort.c	1.16	97/05/01 SMI"

/*
 * Utilities to handle shared object dependency graph.
 *
 * The algorithms used in this file are taken from the following book:
 *	Algorithms in C
 *		Robert Sedgewick
 *		Addison-Wesley Publishing company
 *		ISBN 0-201-51425-7
 * 	From the following chapters:
 *		Chapter 29 Elementary Graph Algorithms
 *		Chapter 32 Directed Graph
 */
#include	"_synonyms.h"

#include	<sys/types.h>
#include	<stdarg.h>
#include	<stdio.h>
#include	<dlfcn.h>
#include	<signal.h>
#include	<locale.h>
#include	<libintl.h>
#include	"_rtld.h"
#include	"msg.h"
#include	"profile.h"

/*
 * Data structures and macro definitions for topologically
 * sort the dependencies.
 */
typedef struct sort_stack Sort_stack;
typedef struct sort_info Sort_info;

/*
 * Stack to keep strongly connected components.
 */
struct sort_stack {
	Rt_map **stack_objs;
	int stack_sp;
	int stack_max_depth;
};

/*
 * Return information used for the function visiti).
 */
struct sort_info {
	Rt_map **info_ret;
	int info_idx;
	int info_idx_p0;	/* For initfirst */
	int info_idx_p1;	/* For finifirst */
	List info_scc;
};

/*
 * Visit nodes breadth first.
 *	(called if LD_BREADTH is in effect.)
 */
static int
bvisit(Rt_map *node, Lm_list *list, Sort_info *sort_info, int flag)
{
	Rt_map *tnode;
	Rt_map **ret = sort_info->info_ret;

	/*
	 * This is for .init. (Reverse breadth-first.)
	 */
	if (flag == RT_SORT_REV) {
		if (NEXT(node))
			(void) bvisit((Rt_map *)NEXT(node),
				list, sort_info, flag);
		/*
		 * Check if this object belongs to the original object's
		 * linked map.
		 */
		if (list == node->rt_list)
			ret[(sort_info->info_idx)++] = node;
		return (1);
	}

	/*
	 * This is for .fini. (Breadth-first.)
	 */
	for (tnode = node->rt_list->lm_head;
	    tnode;
	    tnode = (Rt_map *)NEXT(tnode)) {
		/*
		 * Check if this object belongs to the original object's
		 * linked map.
		 */
		if (list == tnode->rt_list)
			ret[(sort_info->info_idx)++] = tnode;
	}
	return (1);
}

/*
 * Visit nodes depth first, topologically.
 */
static int
visit(Rt_map *node, Lm_list *list, Sort_info *sort_info)
{
	Listnode *lnp;
	Rt_map *parent;
	Rt_map **ret = sort_info->info_ret;

	node->rt_sortval = 1;

	for (LIST_TRAVERSE(&PARENTS(node), lnp, parent)) {
		if (parent->rt_sortval == 0)
			(void) visit(parent, list, sort_info);
	}
	/*
	 * Check if this object belongs to the original object's
	 * linked map.
	 */
	if (list == node->rt_list) {
		if (FLAGS(node) & FLG_RT_INITFRST)
			ret[(sort_info->info_idx_p0)++] = node;
		else if (FLAGS(node) & FLG_RT_FINIFRST)
			ret[(sort_info->info_idx_p1)++] = node;
		else
			ret[(sort_info->info_idx)++] = node;
	}
	return (1);
}

/*
 * Visit nodes depth first, reverse topologically
 * 	When the reverse topologically sorting is performed,
 *	the cyclic dependency needs to be calculated.
 *	All the strongly connected components of the graph
 *	are calculated. (See chapter 32.) When there is only
 *	one element in the strongly connected components,
 *	it means that this element is not part of a loop.
 */
static int
rvisit(Rt_map *node, Lm_list *list,
	Sort_stack *stack, int *id, Sort_info *sort_info)
{
	int m, min;
	Listnode *lnp;
	Rt_map *child;
	Rt_map **snode = stack->stack_objs;
	int depth = stack->stack_max_depth;
	int is_trace = 0;
	Rt_map **ret = sort_info->info_ret;

	if (tracing && (rtld_flags & RT_FL_INIT))
		is_trace = 1;

	node->rt_sortval = ++(*id);
	min = *id;
	snode[(stack->stack_sp)++] = node;

	for (LIST_TRAVERSE(&DEPENDS(node), lnp, child)) {
		if (child->rt_sortval == 0) {
			m = rvisit(child, list, stack, id, sort_info);
			if (m == 0)
				return (0);
		} else
			m = child->rt_sortval;
		if (m < min)
			min = m;
	}

	if (min == node->rt_sortval) {
	    List *scc;

	    if (is_trace != 0) {
		if ((scc = (List *)calloc(1, sizeof (List))) == NULL)
			return (0);
		if (list_append(&sort_info->info_scc, scc) ==
		    (Listnode *)NULL)
			return (0);
	    }

	    while (snode[stack->stack_sp] != node) {
		Rt_map *tnode;
		tnode = snode[--(stack->stack_sp)];
		/*
		 * Check if this object belongs to the original object's
		 * linked map.
		 */
		if (list == node->rt_list) {
			if (FLAGS(tnode) & FLG_RT_INITFRST)
				ret[(sort_info->info_idx_p0)++] = tnode;
			else
				ret[(sort_info->info_idx)++] = tnode;
		}
		snode[stack->stack_sp]->rt_sortval = depth;
		/*
		 * If tracing is on, then save the SCC.
		 */
		if (is_trace) {
			if (list_append(scc, tnode) ==
			    (Listnode *)NULL)
				return (0);
		}
	    }
	}
	return (min);
}

/*
 * Find the corresponding Scc structure
 */
static List *
find_scc(Sort_info *sort_info, Rt_map *lm)
{
	Listnode *lnp1, *lnp2;
	List *scc;
	Rt_map *nl;

	for (LIST_TRAVERSE(&(sort_info->info_scc), lnp1, scc)) {
		for (LIST_TRAVERSE(scc, lnp2, nl)) {
			if (nl == lm)
				return (scc);
		}
	}
	return ((List *)NULL);
}

/*
 * Print out the .init dependency information
 */
static void
print_init_info(Sort_info *sort_info)
{
	int idx;
	List *scc;
	Rt_map *lm;

	(void) printf(MSG_ORIG(MSG_STR_NL));
	idx = 0;
	while ((lm = sort_info->info_ret[idx]) != NULL) {
		if ((INIT(lm) == 0) || (FLAGS(lm) & FLG_RT_ISMAIN)) {
			idx++;
			continue;
		}
		if ((rtld_flags & RT_FL_BREADTH) != 0) {
			(void) printf(MSG_INTL(MSG_LDD_INIT_FMT_01),
			NAME(lm));
			idx++;
			continue;
		}
		scc = find_scc(sort_info, lm);
		if (scc->head == scc->tail)
			(void) printf(MSG_INTL(MSG_LDD_INIT_FMT_01),
			NAME(lm));
		else {
			Listnode *lnp, *lnp2;
			Rt_map *pnode, *nl;

			(void) printf(MSG_INTL(MSG_LDD_INIT_FMT_02), NAME(lm));
			for (LIST_TRAVERSE(&PARENTS(lm), lnp, pnode)) {
			    for (LIST_TRAVERSE(scc, lnp2, nl)) {
				if (nl == pnode) {
				    (void) printf(MSG_ORIG(MSG_LDD_FMT_FILE),
					NAME(pnode));
				}
			    }
			}
			(void) printf(MSG_ORIG(MSG_LDD_FMT_END));
		}
		idx++;
	}
}

/*
 * Sort the dependency
 */
Rt_map **
tsort(Rt_map *lm, int flag)
{
	Listnode *lnp1, *lnp2;
	Rt_map *node;
	int num_objs, num_initfirst, num_finifirst, id = 0;
	int tmp;
	Sort_stack node_stack = {0, 0, 0};
	Sort_info sort_info =
		{0,		/* info_ret */
		0,		/* info_idx */
		0,		/* info_idx_p0 */
		0,		/* info_idx_p1 */
		{0, 0}};	/* info_scc */

	PRF_MCOUNT(118, tsort);

	/*
	 * Allocate memory for return value
	 */
	if ((num_objs = lm->rt_list->lm_numobj) == 0)
		return ((Rt_map **)NULL);
	if ((sort_info.info_ret =
	    (Rt_map **) calloc(num_objs + 1, sizeof (Rt_map *))) == NULL)
		return ((Rt_map **)S_ERROR);
	if ((flag == RT_SORT_REV) && !(rtld_flags & RT_FL_BREADTH)) {
		for (tmp = 0; tmp < num_objs; tmp++)
			(sort_info.info_ret)[tmp] = (Rt_map *) RT_SORT_UNUSED;
	}

	/*
	 * Sort the dependency
	 */
	if (flag == RT_SORT_REV) {
		if ((node_stack.stack_objs = (Rt_map **)
		    calloc(num_objs + 1, sizeof (Rt_map *))) ==
		    (Rt_map **) NULL)
			return ((Rt_map **)S_ERROR);
		node_stack.stack_max_depth = num_objs + 1;
	}

	/*
	 * If LD_BREADTH is defined, don't do topological sorting.
	 * Just do the bvisit(), and call print_init_info() if needed.
	 * No cleaning required, so just return here.
	 */

	/*
	 * bvisit()/visit()/rvisit() all pass the linkmap where the
	 * original object started. While in these functions,
	 * if any object which does not belong to the original link
	 * map, do not add it to the output sorted list.
	 */
	if ((rtld_flags & RT_FL_BREADTH)) {
		(void) bvisit(lm, lm->rt_list, &sort_info, flag);
		/*
		 * If tracing is on, print out the sorted dependency
		 */
		if (tracing && (rtld_flags & RT_FL_INIT))
			print_init_info(&sort_info);
		return (sort_info.info_ret);
	}

	/*
	 * When I'm here, topological sorting is required.
	 */
	num_initfirst = lm->rt_list->lm_numinitfirst;
	num_finifirst = lm->rt_list->lm_numfinifirst;
	node = lm->rt_list->lm_head;
	while (node != NULL) {
		node->rt_sortval = 0;
		node = (Rt_map *) NEXT(node);
	}
	if (flag == RT_SORT_REV) {
		/*
		 * I want to do the INITFIRT objects first. so,,
		 */
		sort_info.info_idx = num_initfirst;
		sort_info.info_idx_p0 = 0;
	} else {
		/*
		 * I want to do the INITFIRT objects at the end. so,,
		 */
		sort_info.info_idx_p1 = 0;
		sort_info.info_idx = num_finifirst;
		sort_info.info_idx_p0 = num_objs - num_initfirst;
	}
	for (node = lm; node != NULL; node = (Rt_map *) NEXT(node)) {
	    if (node->rt_sortval == 0) {
		if (flag == RT_SORT_REV) {
		    if (rvisit(node, lm->rt_list,
			&node_stack, &id, &sort_info) == 0)
			return ((Rt_map **)S_ERROR);
		} else
		    (void) visit(node, lm->rt_list, &sort_info);
	    }
	}

	/*
	 * If tracing is on, print out the sorted dependency
	 */
	if (tracing && (rtld_flags & RT_FL_INIT))
	    print_init_info(&sort_info);

	/*
	 *	Clean stack info.
	 */
	if (node_stack.stack_objs)
		free(node_stack.stack_objs);

	/*
	 * Clean SCC info
	 */
	lnp1 = sort_info.info_scc.head;
	while (lnp1 != NULL) {
		Listnode *f1;
		List *scc = (List *)lnp1->data;

		lnp2 = scc->head;
		while (lnp2 != NULL) {
			Listnode *f2;

			f2 = lnp2;
			lnp2 = lnp2->next;
			free(f2);
		}

		f1 = lnp1;
		lnp1 = lnp1->next;
		free(f1->data);
		free(f1);
	}
	return (sort_info.info_ret);
}

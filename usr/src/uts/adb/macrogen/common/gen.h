/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 */
#pragma ident   "@(#)gen.h 1.2     98/01/09 SMI"


/* A dimension_t tells the number of elements in one dimension of an array. */
typedef struct dimension_ dimension_t;
struct dimension_ {
	int		 num_elements;	/* # elements in this array dimension */
	dimension_t	*next;		/* info about further dimensions */
	dimension_t	*prev;		/*   in a doubly-linked list     */
};

extern void gen_struct_begin(
	char *,		/* struct name */
	char *,		/* prefix for members */
	char *,		/* file (macro name) to put the macro in */
	int		/* size of struct, in bytes */
);

extern void gen_struct_member(
	char *,		/* label */
	int,		/* offset, in bits */
	dimension_t *,	/* array info (most major dimension first) */
	int,		/* size of element, in bits */
	char *		/* format */
);

extern void gen_struct_end(
);

/*
 * Copyright (c) 1990 - 1998 Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident "@(#)pdevinfo.c	1.37	98/01/22 SMI"

/*
 * For machines that support the openprom, fetch and print the list
 * of devices that the kernel has fetched from the prom or conjured up.
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <strings.h>
#include <unistd.h>
#include <stropts.h>
#include <sys/utsname.h>
#include <sys/sunddi.h>
#include <sys/openpromio.h>
#include <libdevinfo.h>

/*
 * function declarations
 */
static void walk_driver(di_node_t root_node);
static int dump_devs(di_node_t node, void *arg);
static int dump_prop_list(char *name, int ilev, di_node_t node,
	di_node_t (*nxtprop)(di_node_t, di_prop_t));
static void dump_node(int id, int level);
static int _error(char *opt_noperror, ...);

void indent_to_level(int ilev);

extern void init_priv_data(struct di_priv_data *);
extern void dump_priv_data(int ilev, di_node_t node);

/*
 * local data
 */
static char *indent_string = "    ";

extern int verbose;
extern int drv_name;
extern int pseudodevs;
extern char *progname;
extern char *promdev;
extern struct utsname uts_buf;

#ifdef	DEBUG
extern int bydriver;
extern int forceload;
extern char *drivername;
extern int vdebug_flag;
#endif	/* DEBUG */

#ifdef	DEBUG
#define	dprintf	if (vdebug_flag) fprintf
#else
#define	dprintf	if (0) printf
#endif	/* DEBUG */

void
prtconf_devinfo()
{
	struct di_priv_data fetch;
	di_node_t root_node;
	uint_t flag;

#ifdef	DEBUG
	if (vdebug_flag) di_enable_debug();
	dprintf(stderr, "verbosemode %s\n", verbose ? "on" : "off");
#endif	/* DEBUG */

	/* determine what info we need to get from kernel */
	flag = DINFOSUBTREE;

	if (verbose) {
		flag |= (DINFOPROP | DINFOPRIVDATA);
#ifdef	DEBUG
		if (forceload)
			flag |= DINFOFORCE;
#endif	/* DEBUG */
		init_priv_data(&fetch);
		root_node = di_init_impl("/", flag, &fetch);
	} else
		root_node = di_init("/", flag);

	if (root_node == DI_NODE_NIL)
		exit(_error("di_init() failed."));

	/*
	 * ...and walk all nodes to report them out...
	 */
#ifdef	DEBUG
	if (bydriver)
		walk_driver(root_node);
	else
#endif	/* DEBUG */
		di_walk_node(root_node, DI_WALK_CLDFIRST, NULL, dump_devs);

	di_fini(root_node);
}

/*
 * utility routines
 */

#ifdef	DEBUG
static void
walk_driver(di_node_t root)
{
	di_node_t node;

	node = di_drv_first_node(drivername, root);

	while (node != DI_NODE_NIL) {
		dump_devs(node, NULL);
		node = di_drv_next_node(node);
	}
}
#endif	/* DEBUG */

/*
 * print out information about this node, returns appropriate code.
 *   NOTE arg is a dummy used to match node_callback() interface
 */

static int
dump_devs(di_node_t node, void *arg)
{
	int ilev = 0, rval;		/* indentation level */
	di_node_t tmp;
	char *driver_name;

#ifdef DEBUG
	char *path = di_devfs_path(node);
	dprintf(stderr, "Dump node %s\n", path);
	di_devfs_path_free(path);

	if (! bydriver) {
#endif	/* DEBUG */
	/* figure out indentation level */
		tmp = node;
		while ((tmp = di_parent_node(tmp)) != DI_NODE_NIL)
			ilev++;
#ifdef DEBUG
	} else
		ilev = 1;
#endif	/* DEBUG */

	indent_to_level(ilev);

	(void) printf("%s", di_node_name(node));

	/*
	 * if this node does not have an instance number or is the
	 * root node (1229946), we don't print an instance number
	 *
	 * NOTE ilev = 0 for root node
	 */
	if ((di_instance(node) >= 0) && ilev)
		(void) printf(", instance #%d", di_instance(node));

	if (drv_name) {
	/*
	 * XXX Don't print driver name for root because old prtconf
	 *	can't figure it out.
	 */
		driver_name = di_driver_name(node);
		if (ilev && (driver_name != NULL))
			(void) printf(" (driver name: %s)", driver_name);
	} else if (di_state(node) & DI_DRIVER_DETACHED)
		(void) printf(" (driver not attached)");

	(void) printf("\n");

	if (verbose)  {
		rval = dump_prop_list("System", ilev+1, node, di_prop_sys_next);
		if (rval)
			dump_prop_list(NULL, ilev+1, node, di_prop_global_next);
		else
			dump_prop_list("System software", ilev+1, node,
				di_prop_global_next);
		(void) dump_prop_list("Driver", ilev+1, node, di_prop_drv_next);
		(void) dump_prop_list("Hardware", ilev+1, node,
				di_prop_hw_next);
		dump_priv_data(ilev+1, node);
	}

	if (!pseudodevs && (strcmp(di_node_name(node), "pseudo") == 0))
		return (DI_WALK_PRUNECHILD);
	else
		return (DI_WALK_CONTINUE);
}

void
indent_to_level(int ilev)
{
	int i;

	for (i = 0; i < ilev; i++)
		(void) printf(indent_string);
}

/*
 * Returns 0 if nothing is printed, 1 otherwise
 */
static int
dump_prop_list(char *name, int ilev, di_node_t node, di_node_t (*nxtprop)())
{
	int prop_len, i;
	uchar_t *prop_data;
	di_prop_t prop, next;

	if ((next = nxtprop(node, DI_PROP_NIL)) == DI_PROP_NIL)
		return (0);

	if (name != NULL)  {
		indent_to_level(ilev);
		(void) printf("%s properties:\n", name);
	}

	while (next != DI_PROP_NIL) {
		prop = next;
		next = nxtprop(node, prop);

		/*
		 * get prop length and value:
		 * private interface--always success
		 */
		prop_len = di_prop_rawdata(prop, &prop_data);

		indent_to_level(ilev +1);
		(void) printf("name <%s> length <%d>",
			di_prop_name(prop), prop_len);

		if (di_prop_type(prop) == DDI_PROP_UNDEF_IT) {
			(void) printf(" -- Undefined.\n");
			continue;
		}

		if (prop_len == 0)  {
			(void) printf(" -- <no value>.\n");
			continue;
		}

		(void) putchar('\n');
		indent_to_level(ilev +1);
		(void) printf("    value <0x");
		for (i = 0; i < prop_len; ++i)  {
			unsigned char byte;

			byte = (unsigned char)prop_data[i];
			(void) printf("%2.2x", byte);
		}
		(void) printf(">.\n");
	}

	return (1);
}


/* _error([no_perror, ] fmt [, arg ...]) */
int
_error(char *opt_noperror, ...)
{
	int saved_errno;
	va_list ap;
	int no_perror = 0;
	char *fmt;
	extern int _doprnt();

	saved_errno = errno;

	if (progname)
		(void) fprintf(stderr, "%s: ", progname);

	va_start(ap, opt_noperror);
	if (opt_noperror == NULL) {
		no_perror = 1;
		fmt = va_arg(ap, char *);
	} else
		fmt = opt_noperror;
	(void) _doprnt(fmt, ap, stderr);
	va_end(ap);

	if (no_perror)
		(void) fprintf(stderr, "\n");
	else {
		(void) fprintf(stderr, ": ");
		errno = saved_errno;
		perror("");
	}

	return (-1);
}


/*
 * The rest of the routines handle printing the raw prom devinfo (-p option).
 *
 * 128 is the size of the largest (currently) property name
 * 16k - MAXNAMESZ - sizeof (int) is the size of the largest
 * (currently) property value that is allowed.
 * the sizeof (u_int) is from struct openpromio
 */

#define	MAXNAMESZ	128
#define	MAXVALSIZE	(16384 - MAXNAMESZ - sizeof (u_int))
#define	BUFSIZE		(MAXNAMESZ + MAXVALSIZE + sizeof (u_int))
typedef union {
	char buf[BUFSIZE];
	struct openpromio opp;
} Oppbuf;

static void dump_node(), print_one(), promclose(), walk();
static int child(), getpropval(), next(), unprintable(), promopen();

static int prom_fd;

static int
is_openprom()
{
	Oppbuf	oppbuf;
	struct openpromio *opp = &(oppbuf.opp);
	unsigned int i;

	opp->oprom_size = MAXVALSIZE;
	if (ioctl(prom_fd, OPROMGETCONS, opp) < 0)
		exit(_error("OPROMGETCONS"));

	i = (unsigned int)((unsigned char)opp->oprom_array[0]);
	return ((i & OPROMCONS_OPENPROM) == OPROMCONS_OPENPROM);
}

static char *badarchmsg =
	"System architecture does not support this option of this command.\n";

int
do_prominfo()
{
	if (promopen(O_RDONLY))  {
		exit(_error("openeepr device open failed"));
	}

	if (is_openprom() == 0)  {
		(void) fprintf(stderr, badarchmsg);
		return (1);
	}

	if (next(0) == 0)
		return (1);
	walk(next(0), 0);
	promclose();
	return (0);
}

static void
walk(id, level)
int id, level;
{
	int curnode;

	dump_node(id, level);
	if (curnode = child(id))
		walk(curnode, level+1);
	if (curnode = next(id))
		walk(curnode, level);
}

/*
 * Print all properties and values
 */
static void
dump_node(int id, int level)
{
	Oppbuf	oppbuf;
	struct openpromio *opp = &(oppbuf.opp);
	int i = level;

	while (i--)
		(void) printf(indent_string);
	(void) printf("Node");
	if (!verbose) {
		print_one("name", level);
		(void) putchar('\n');
		return;
	}
	(void) printf(" %#08x\n", id);

	/* get first prop by asking for null string */
	bzero(oppbuf.buf, BUFSIZE);
	for (;;) {
		/*
		 * get next property name
		 */
		opp->oprom_size = MAXNAMESZ;

		if (ioctl(prom_fd, OPROMNXTPROP, opp) < 0)
			exit(_error("OPROMNXTPROP"));

		if (opp->oprom_size == 0) {
			break;
		}
		print_one(opp->oprom_array, level+1);
	}
	(void) putchar('\n');
}

/*
 * certain 'known' property names may contain 'composite' strings.
 * Handle them here, and print them as 'string1' + 'string2' ...
 */
static int
print_composite_string(char *var, struct openpromio *opp)
{
	char *p, *q;
	char *firstp;

	if ((strcmp(var, "version") != 0) &&
	    (strcmp(var, "compatible") != 0))
		return (0);	/* Not a known composite string */

	/*
	 * Verify that each string in the composite string is non-NULL,
	 * is within the bounds of the property length, and contains
	 * printable characters or white space. Otherwise let the
	 * caller deal with it.
	 */
	for (firstp = p = opp->oprom_array;
	    p < (opp->oprom_array + opp->oprom_size);
	    p += strlen(p) + 1) {
		if (strlen(p) == 0)
			return (0);		/* NULL string */
		for (q = p; *q; q++) {
			if (!(isascii(*q) && (isprint(*q) || isspace(*q))))
				return (0);	/* Not printable or space */
		}
		if (q > (firstp + opp->oprom_size))
			return (0);		/* Out of bounds */
	}

	for (firstp = p = opp->oprom_array;
	    p < (opp->oprom_array + opp->oprom_size);
	    p += strlen(p) + 1) {
		if (p == firstp)
			(void) printf("'%s'", p);
		else
			(void) printf(" + '%s'", p);
	}
	putchar('\n');
	return (1);
}

/*
 * Print one property and its value.
 */
static void
print_one(var, level)
char	*var;
int	level;
{
	Oppbuf	oppbuf;
	struct openpromio *opp = &(oppbuf.opp);
	int i;

	while (verbose && level--)
		(void) printf(indent_string);
	if (verbose)
		(void) printf("%s: ", var);
	(void) strcpy(opp->oprom_array, var);
	if (getpropval(opp) || opp->oprom_size == -1) {
		(void) printf("data not available.\n");
		return;
	}

	if (verbose)
		if (print_composite_string(var, opp))
			return;

	if (verbose && unprintable(opp)) {
#ifdef i386
		int endswap;

		/*
		 * Due to backwards compatibility constraints x86 int
		 * properties are not in big-endian (ieee 1275) byte order.
		 * If we have a property that is a multiple of 4 bytes,
		 * let's assume it is an array of ints and print the bytes
		 * in little endian order to make things look nicer for
		 * the user.
		 */
		endswap = (opp->oprom_size % 4) == 0;
		(void) printf(" ");
		for (i = 0; i < opp->oprom_size; ++i) {
			if (i && (i % 4 == 0))
				(void) putchar('.');
			if (!endswap)
				(void) printf("%02x",
				    opp->oprom_array[i] & 0xff);
			else
				(void) printf("%02x",
				    opp->oprom_array[i + (3 - 2 * (i % 4))] &
					0xff);
		}
		(void) putchar('\n');
#else
		(void) printf(" ");
		for (i = 0; i < opp->oprom_size; ++i) {
			if (i && (i % 4 == 0))
				(void) putchar('.');
			(void) printf("%02x",
			    opp->oprom_array[i] & 0xff);
		}
		(void) putchar('\n');
#endif	/* i386 */
	} else if (verbose) {
		(void) printf(" '%s'\n", opp->oprom_array);
	} else if (strcmp(var, "name") == 0)
		(void) printf(" '%s'", opp->oprom_array);
}

static int
unprintable(opp)
struct openpromio *opp;
{
	int i;

	/*
	 * Is this just a zero?
	 */
	if (opp->oprom_size == 0 || opp->oprom_array[0] == '\0')
		return (1);
	/*
	 * If any character is unprintable, or if a null appears
	 * anywhere except at the end of a string, the whole
	 * property is "unprintable".
	 */
	for (i = 0; i < opp->oprom_size; ++i) {
		if (opp->oprom_array[i] == '\0')
			return (i != (opp->oprom_size - 1));
		if (!isascii(opp->oprom_array[i]) ||
		    iscntrl(opp->oprom_array[i]))
			return (1);
	}
	return (0);
}

static int
promopen(oflag)
int oflag;
{
	for (;;)  {
		if ((prom_fd = open(promdev, oflag)) < 0)  {
			if (errno == EAGAIN)   {
				(void) sleep(5);
				continue;
			}
			if (errno == ENXIO)
				return (-1);
			exit(_error("cannot open %s", promdev));
		} else
			return (0);
	}
}

static void
promclose()
{
	if (close(prom_fd) < 0)
		exit(_error("close error on %s", promdev));
}

static
getpropval(opp)
struct openpromio *opp;
{
	opp->oprom_size = MAXVALSIZE;

	if (ioctl(prom_fd, OPROMGETPROP, opp) < 0)
		return (_error("OPROMGETPROP"));
	return (0);
}

static int
next(id)
int id;
{
	Oppbuf	oppbuf;
	struct openpromio *opp = &(oppbuf.opp);
	int *ip = (int *)(opp->oprom_array);

	bzero(oppbuf.buf, BUFSIZE);
	opp->oprom_size = MAXVALSIZE;
	*ip = id;
	if (ioctl(prom_fd, OPROMNEXT, opp) < 0)
		return (_error("OPROMNEXT"));
	return (*(int *)opp->oprom_array);
}

static int
child(id)
int id;
{
	Oppbuf	oppbuf;
	struct openpromio *opp = &(oppbuf.opp);
	int *ip = (int *)(opp->oprom_array);

	bzero(oppbuf.buf, BUFSIZE);
	opp->oprom_size = MAXVALSIZE;
	*ip = id;
	if (ioctl(prom_fd, OPROMCHILD, opp) < 0)
		return (_error("OPROMCHILD"));
	return (*(int *)opp->oprom_array);
}

#ifdef i386
/*
 * Just return with a status of 1 which indicates that no separate
 * frame buffer from the console.
 * This fixes bug 3001499.
 */
int
do_fbname()
{
	return (1);
}
#else
/*
 * Get and print the name of the frame buffer device.
 */
int
do_fbname()
{
	Oppbuf	oppbuf;
	struct openpromio *opp = &(oppbuf.opp);
	unsigned int i;

	if (promopen(O_RDONLY))  {
		(void) fprintf(stderr, "Cannot open openprom device\n");
		return (1);
	}

	opp->oprom_size = MAXVALSIZE;
	if (ioctl(prom_fd, OPROMGETCONS, opp) < 0)
		exit(_error("OPROMGETCONS"));

	i = (unsigned int)((unsigned char)opp->oprom_array[0]);
	if ((i & OPROMCONS_STDOUT_IS_FB) == 0)  {
		(void) fprintf(stderr,
			"Console output device is not a framebuffer\n");
		return (1);
	}

	opp->oprom_size = MAXVALSIZE;
	if (ioctl(prom_fd, OPROMGETFBNAME, opp) < 0)
		exit(_error("OPROMGETFBNAME"));

	(void) printf("%s\n", opp->oprom_array);
	promclose();
	return (0);
}
#endif /* i386 */

/*
 * Get and print the PROM version.
 */
int
do_promversion(void)
{
	Oppbuf	oppbuf;
	struct openpromio *opp = &(oppbuf.opp);

	if (promopen(O_RDONLY))  {
		(void) fprintf(stderr, "Cannot open openprom device\n");
		return (1);
	}

	opp->oprom_size = MAXVALSIZE;
	if (ioctl(prom_fd, OPROMGETVERSION, opp) < 0)
		exit(_error("OPROMGETVERSION"));

	(void) printf("%s\n", opp->oprom_array);
	promclose();
	return (0);
}

int
do_prom_version64(void)
{
#ifdef	sparc
	Oppbuf	oppbuf;
	struct openpromio *opp = &(oppbuf.opp);
	struct openprom_opr64 *opr = (struct openprom_opr64 *)opp->oprom_array;

	static const char msg[] =
		"Warning: Down-rev firmware detected %s ...\n\n"
		"\tPlease upgrade to at least the following version:\n"
		"\t\t%s\n\n";

	static const char msg2[] = "on one or more CPU boards";

	if (promopen(O_RDONLY))  {
		(void) fprintf(stderr, "Cannot open openprom device\n");
		return (-1);
	}

	opp->oprom_size = MAXVALSIZE;
	if (ioctl(prom_fd, OPROMREADY64, opp) < 0)
		exit(_error("OPROMREADY64"));

#if 0
	(void) printf("do_prom_version:\n");
	(void) printf("opr->return_code: %d\n", opr->return_code);
	(void) printf("opr->nodeid: %#08x\n", opr->nodeid);
	(void) printf("OPR Message follows: ...\n%s\n", opr->message);
#endif

	if (opr->return_code == 0)
		return (0);

	(void) printf(msg, opr->return_code == 1 ? "" : msg2, opr->message);

	promclose();
	return (opr->return_code);
#else
	return (0);
#endif
}

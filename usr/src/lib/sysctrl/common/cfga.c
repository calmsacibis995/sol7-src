/*
 * Copyright (c) 1996-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)cfga.c 1.6     98/01/23     SMI"

#include <stddef.h>
#include <locale.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>
#include <langinfo.h>
#include <time.h>
#include <varargs.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/dditypes.h>
#include <sys/modctl.h>
#include <sys/obpdefs.h>
#include <sys/fhc.h>
#include <sys/sysctrl.h>
#include <sys/openpromio.h>
#ifdef	SIM
#include <sys/stat.h>
#endif
#include <config_admin.h>

#ifdef	DEBUG
#define	DBG	printf
#define	DBG1	printf
#define	DBG3	printf
#define	DBG4	printf
#else
#define	DBG(a, b)
#define	DBG1(a)
#define	DBG3(a, b, c)
#define	DBG4(a, b, c, d)
#endif

#define	BD_CPU			1
#define	BD_MEM			2
#define	BD_IO_2SBUS		3
#define	BD_IO_SBUS_FFB		4
#define	BD_IO_PCI		5
#define	BD_DISK			6
#define	BD_UNKNOWN		7
#define	CMD_GETSTAT		8
#define	CMD_LIST		9
#define	CMD_CONNECT		10
#define	CMD_DISCONNECT		11
#define	CMD_CONFIGURE		12
#define	CMD_UNCONFIGURE		13
#define	CMD_QUIESCE		14
#define	CMD_INSERT		15
#define	CMD_REMOVE		16
#define	OPT_ENABLE		17
#define	OPT_DISABLE		18
#define	ERR_PROM_OPEN		19
#define	ERR_PROM_GETPROP	20
#define	ERR_PROM_SETPROP	21
#define	ERR_TRANS		22
#define	ERR_OPT_UNKNOWN		23
#define	ERR_OPT_ILL		24
#define	ERR_OPT_NONE		25
#define	ERR_AP_ILL		26
#define	ERR_DISABLED		27
#define	DIAG_FORCE		28
#define	DIAG_TRANS_OK		29
#define	DIAG_FAILED		30
#define	DIAG_WAS_ENABLED	31
#define	DIAG_WAS_DISABLED	32
#define	DIAG_WILL_ENABLE	33
#define	DIAG_WILL_DISABLE	34
#define	HELP_QUIESCE		35
#define	HELP_INSERT		36
#define	HELP_REMOVE		37
#define	HELP_ENABLE		38
#define	HELP_DISABLE		39
#define	HELP_UNKNOWN		40
#define	ASK_CONNECT		41
#define	STR_BD			42
#define	STR_COL			43
#define	SYSC_COOLING		44
#define	SYSC_POWER		45
#define	SYSC_PRECHARGE		46
#define	SYSC_BUSY		47
#define	SYSC_INTRANS		48
#define	SYSC_UTHREAD		49
#define	SYSC_KTHREAD		50
#define	SYSC_DEV_ATTACH		51
#define	SYSC_DEV_DETACH		52
#define	SYSC_COPY		53
#define	SYSC_NDI_ATTACH		54
#define	SYSC_NDI_DETACH		55
#define	SYSC_CORE_RESOURCE	56
#define	SYSC_OSTATE		57
#define	SYSC_RSTATE		58
#define	SYSC_PROM		59
#define	SYSC_NOMEM		60
#define	SYSC_NOTSUP		61
#define	SYSC_HOTPLUG		62
#define	SYSC_HW_COMPAT		63
#define	SYSC_NON_DR_PROM	64
#define	SYSC_SOFTSP		66
#define	SYSC_SUSPEND		66
#define	SYSC_RESUME		67
#define	SYSC_UNKNOWN		68
#define	SYSC_DEVSTR		69

/*
 * The string table contains all the strings used by the platform
 * library.  The comment next to each string specifies whether the
 * string should be internationalized (y) or not (n).
 * Note that there are calls to gettext() with strings other than
 * the ones below, they are marked by the li18 symbol.
 */
static char *
cfga_strs[] = {
	/*   */ NULL,
	/* n */ "cpu/mem   ",
	/* n */ "mem       ",
	/* n */ "dual-sbus ",
	/* n */ "sbus-upa  ",
	/* n */ "dual-pci  ",
	/* n */ "disk      ",
	/* n */ "unknown   ",
	/* n */ "getstat",
	/* n */ "list",
	/* n */ "connect",
	/* n */ "disconnect",
	/* n */ "configure",
	/* n */ "unconfigure",
	/* n */ "quiesce-test",
	/* n */ "insert-test",
	/* n */ "remove-test",
	/* n */ "enable-at-boot",
	/* n */ "disable-at-boot",
	/* n */ "prom open",
	/* n */ "prom getprop",
	/* n */ "prom setprop",
	/* y */ "illegal transition",
	/* y */ "unknown option: ",
	/* y */ "illegal option: ",
	/* y */ "no options allowed",
	/* y */ "illegal attachment point: ",
	/* y */ "board is disabled: must override with ",
	/* n */ "[-f][-o enable-at-boot]",
	/* y */ "transition succeeded but ",
	/* y */ " failed: ",
	/* y */ "was already enabled at boot time",
	/* y */ "was already disabled at boot time",
	/* y */ "will be enabled at boot time",
	/* y */ "will be disabled at boot time",
	/* n */ "\t-x quiesce-test ap_id [ap_id...]",
	/* n */ "\t-x insert-test  ap_id [ap_id...]",
	/* n */ "\t-x remove-test  ap_id [ap_id...]",
	/* n */ "\t-o enable-at-boot  -c {connect, configure} ap_id [ap_id...]",
	/* n */ "\t-o disable-at-boot -c {connect, configure} ap_id [ap_id...]",
	/* y */ "\tunknown command or option: ",
	/* y */
"system will be temporarily suspended to connect a board: proceed",
	/* y */ "board ",
	/* y */ ": ",
	/* y */ "not enough cooling for a new board",
	/* y */ "not enough power for a new board",
	/* y */ "not enough precharge power for a new board",
	/* y */ "configuration operation already in progress on this system",
	/* y */ "configuration operation already in progress on this board",
	/* y */ "could not suspend user process: ",
	/* y */ "could not suspend system processes",
	/* y */ "device did not attach",
	/* y */ "device did not detach",
	/* y */ "could not copy data to/from system",
	/* y */ "nexus error during attach",
	/* y */ "nexus error during detach",
	/* y */ "attempt to core system resource",
	/* y */ "invalid occupant state",
	/* y */ "invalid receptacle state",
	/* y */ "firmware operation error",
	/* y */ "not enough memory",
	/* y */ "command not supported",
	/* y */ "hotplug feature unavailable on this system",
	/* y */ "board does not support dynamic reconfiguration",
	/* y */ "firmware does not support dynamic reconfiguration",
	/* y */ "invalid system state data",
	/* y */ "system suspend error",
	/* y */ "system resume error",
	/* y */ "unknown system error",
	/*   */ NULL
};

#define	cfga_str(i)		cfga_strs[(i)]

#ifdef	SYSC_ERR
#define	cfga_eid(a, b)		(((a) << 8) + (b))

/*
 *
 *	Translation table for mapping from an <errno,sysc_err>
 *	pair to an error string.
 *
 *
 *	SYSC_COOLING,		EAGAIN,  SYSC_ERR_COOLING
 *	SYSC_POWER,		EAGAIN,  SYSC_ERR_POWER
 *	SYSC_PRECHARGE,		EAGAIN,  SYSC_ERR_PRECHARGE
 *	SYSC_BUSY,		EBUSY,   SYSC_ERR_DEFAULT
 *	SYSC_INTRANS,		EBUSY,   SYSC_ERR_INTRANS
 *	SYSC_KTHREAD,		EBUSY,   SYSC_ERR_KTHREAD
 *	SYSC_DEV_ATTACH,	EBUSY,   SYSC_ERR_NDI_ATTACH
 *	SYSC_DEV_DETACH,	EBUSY,   SYSC_ERR_NDI_DETACH
 *	SYSC_COPY,		EFAULT,  SYSC_ERR_DEFAULT
 *	SYSC_NDI_ATTACH,	EFAULT,  SYSC_ERR_NDI_ATTACH
 *	SYSC_NDI_DETACH,	EFAULT,  SYSC_ERR_NDI_DETACH
 *	SYSC_CORE_RESOURCE,	EINVAL,  SYSC_ERR_CORE_RESOURCE
 *	SYSC_OSTATE,		EINVAL,  SYSC_ERR_OSTATE
 *	SYSC_RSTATE,		EINVAL,  SYSC_ERR_RSTATE
 *	SYSC_PROM,		EIO,     SYSC_ERR_PROM
 *	SYSC_NOMEM,		ENOMEM,  SYSC_ERR_DEFAULT
 *	SYSC_NOMEM,		ENOMEM,  SYSC_ERR_DR_INIT
 *	SYSC_NOMEM,		ENOMEM,  SYSC_ERR_NDI_ATTACH
 *	SYSC_NOMEM,		ENOMEM,  SYSC_ERR_NDI_DETACH
 *	SYSC_NOTSUPP,		ENOTSUP, SYSC_ERR_DEFAULT
 *	SYSC_HOTPLUG,		ENOTSUP, SYSC_ERR_HOTPLUG
 *	SYSC_HW_COMPAT,		ENOTSUP, SYSC_ERR_HW_COMPAT
 *	SYSC_NON_DR_PROM,	ENOTSUP, SYSC_ERR_NON_DR_PROM
 *	SYSC_NOTSUPP,		ENOTTY,  SYSC_ERR_DEFAULT
 *	SYSC_SOFTSP,		ENXIO,   SYSC_ERR_DEFAULT
 *	SYSC_SUSPEND,		ENXIO,   SYSC_ERR_SUSPEND
 *	SYSC_RESUME,		ENXIO,   SYSC_ERR_RESUME
 *	0			EPERM,   SYSC_ERR_DEFAULT
 *	SYSC_UTHREAD,		ESRCH,   SYSC_ERR_UTHREAD
 */
static int
cfga_sid(int err, int scerr)
{
	switch (cfga_eid(err, scerr)) {
	case cfga_eid(EAGAIN, SYSC_ERR_COOLING):
		return (SYSC_COOLING);
	case cfga_eid(EAGAIN, SYSC_ERR_POWER):
		return (SYSC_POWER);
	case cfga_eid(EAGAIN, SYSC_ERR_PRECHARGE):
		return (SYSC_PRECHARGE);
	case cfga_eid(EBUSY, SYSC_ERR_DEFAULT):
		return (SYSC_BUSY);
	case cfga_eid(EBUSY, SYSC_ERR_INTRANS):
		return (SYSC_INTRANS);
	case cfga_eid(EBUSY, SYSC_ERR_KTHREAD):
		return (SYSC_KTHREAD);
	case cfga_eid(EBUSY, SYSC_ERR_NDI_ATTACH):
		return (SYSC_DEV_ATTACH);
	case cfga_eid(EBUSY, SYSC_ERR_NDI_DETACH):
		return (SYSC_DEV_DETACH);
	case cfga_eid(EFAULT, SYSC_ERR_DEFAULT):
		return (SYSC_COPY);
	case cfga_eid(EFAULT, SYSC_ERR_NDI_ATTACH):
		return (SYSC_NDI_ATTACH);
	case cfga_eid(EFAULT, SYSC_ERR_NDI_DETACH):
		return (SYSC_NDI_DETACH);
	case cfga_eid(EINVAL, SYSC_ERR_CORE_RESOURCE):
		return (SYSC_CORE_RESOURCE);
	case cfga_eid(EINVAL, SYSC_ERR_OSTATE):
		return (SYSC_OSTATE);
	case cfga_eid(EINVAL, SYSC_ERR_RSTATE):
		return (SYSC_RSTATE);
	case cfga_eid(EIO, SYSC_ERR_PROM):
		return (SYSC_PROM);
	case cfga_eid(ENOMEM, SYSC_ERR_DEFAULT):
		return (SYSC_NOMEM);
	case cfga_eid(ENOMEM, SYSC_ERR_DR_INIT):
		return (SYSC_NOMEM);
	case cfga_eid(ENOMEM, SYSC_ERR_NDI_ATTACH):
		return (SYSC_NOMEM);
	case cfga_eid(ENOMEM, SYSC_ERR_NDI_DETACH):
		return (SYSC_NOMEM);
	case cfga_eid(ENOTSUP, SYSC_ERR_DEFAULT):
		return (SYSC_NOTSUP);
	case cfga_eid(ENOTSUP, SYSC_ERR_HOTPLUG):
		return (SYSC_HOTPLUG);
	case cfga_eid(ENOTSUP, SYSC_ERR_HW_COMPAT):
		return (SYSC_HW_COMPAT);
	case cfga_eid(ENOTSUP, SYSC_ERR_NON_DR_PROM):
		return (SYSC_NON_DR_PROM);
	case cfga_eid(ENOTTY, SYSC_ERR_DEFAULT):
		return (SYSC_NOTSUP);
	case cfga_eid(ENXIO, SYSC_ERR_DEFAULT):
		return (SYSC_SOFTSP);
	case cfga_eid(ENXIO, SYSC_ERR_SUSPEND):
		return (SYSC_SUSPEND);
	case cfga_eid(ENXIO, SYSC_ERR_RESUME):
		return (SYSC_RESUME);
	case cfga_eid(EPERM, SYSC_ERR_DEFAULT):
		return (0);
	case cfga_eid(ESRCH, SYSC_ERR_UTHREAD):
		return (SYSC_UTHREAD);
	default:
		break;
	}

	return (SYSC_UNKNOWN);
}
#endif

static sysc_cfga_cmd_t sysc_cmd;

/*
 * cfga_err() accepts a variable number of message IDs and constructs
 * a corresponding error string which is returned via the errstring argument.
 * cfga_err() calls gettext() to internationalize proper messages.
 */
static void
cfga_err(char **errstring, ...)
{
	int a;
	int i;
	int n;
	int len;
	int flen;
	char *p;
	char *q;
	char *s[32];
	char *failed;
	va_list ap;

	va_start(ap);

	failed = gettext(cfga_str(DIAG_FAILED));
	flen = strlen(failed);

	for (n = len = 0; (a = va_arg(ap, int)) != 0; n++) {

		switch (a) {
		case ERR_PROM_OPEN:
		case ERR_PROM_GETPROP:
		case ERR_PROM_SETPROP:
		case CMD_GETSTAT:
		case CMD_LIST:
		case CMD_CONNECT:
		case CMD_DISCONNECT:
		case CMD_CONFIGURE:
		case CMD_UNCONFIGURE:
		case CMD_QUIESCE:
		case CMD_INSERT:
		case CMD_REMOVE:
			p =  cfga_str(a);
			len += (strlen(p) + flen);
			s[n] = p;
			s[++n] = failed;

			DBG("<%s>", p);
			DBG("<%s>", failed);
			break;

		case OPT_ENABLE:
		case OPT_DISABLE:
			p = gettext(cfga_str(DIAG_TRANS_OK));
			q = cfga_str(a);
			len += (strlen(p) + strlen(q) + flen);
			s[n] = p;
			s[++n] = q;
			s[++n] = failed;

			DBG("<%s>", p);
			DBG("<%s>", q);
			DBG("<%s>", failed);
			break;

		case ERR_OPT_ILL:
		case ERR_AP_ILL:
		case ERR_OPT_UNKNOWN:
			p =  gettext(cfga_str(a));
			q = va_arg(ap, char *);
			len += (strlen(p) + strlen(q));
			s[n] = p;
			s[++n] = q;

			DBG("<%s>", p);
			DBG("<%s>", q);
			break;

		case ERR_TRANS:
		case ERR_OPT_NONE:
		case ERR_DISABLED:
			p =  gettext(cfga_str(a));
			len += strlen(p);
			s[n] = p;

			DBG("<%s>", p);
			break;

		case DIAG_FORCE:
		default:
			p =  cfga_str(a);
			len += strlen(p);
			s[n] = p;

			DBG("<%s>", p);
			break;
		}
	}

	DBG1("\n");
	va_end(ap);

	if (errno) {
#ifdef	SYSC_ERR
		i = cfga_sid(errno, (int)sysc_cmd.errtype);

		DBG4("cfga_sid(%d,%d)=%d\n", errno, sysc_cmd.errtype, i);

		if (i == SYSC_UNKNOWN)
			p = gettext(strerror(errno));	/* li18 */
		else
			p = gettext(cfga_str(i));
#else
		if ((p = strerror(errno)) == NULL)
			p = gettext("Unknown error");
		else
			p = gettext(p);
#endif
		len += strlen(p);
		s[n++] = p;
		p = cfga_str(SYSC_DEVSTR);
		if (p && p[0]) {
			q = cfga_str(STR_COL);

			len += strlen(q);
			s[n++] = q;
			len += strlen(p);
			s[n++] = p;
		}
	}

	if ((p = (char *)calloc(len, 1)) == NULL)
		return;

	for (i = 0; i < n; i++)
		strcat(p, s[i]);

	*errstring = p;
#ifdef	SIM_MSG
	printf("%s\n", *errstring);
	free(*errstring);
#endif
}

/*
 * This routine accepts a variable number of message IDs and constructs
 * a corresponding error string which is printed via the message print routine
 * argument.  The HELP_UNKNOWN message ID has an argument string (the unknown
 * help topic) that follows.
 */
static void
cfga_msg(struct cfga_msg *msgp, ...)
{
	int a;
	int i;
	int n;
	int len;
	char *p;
	char *s[32];
	va_list ap;

	va_start(ap);

	for (n = len = 0; (a = va_arg(ap, int)) != 0; n++) {
		DBG("<%d>", a);
		p =  gettext(cfga_str(a));
		len += strlen(p);
		s[n] = p;
		if (a == HELP_UNKNOWN) {
			p = va_arg(ap, char *);
			len += strlen(p);
			s[++n] = p;
		}
	}

	va_end(ap);

	if ((p = (char *)calloc(len + 1, 1)) == NULL)
		return;

	for (i = 0; i < n; i++)
		strcat(p, s[i]);
	strcat(p, "\n");

#ifdef	SIM_MSG
	printf("%s", p);
	free(p);
#else
	(*msgp->message_routine)(NULL, p);
#endif
}

static sysc_cfga_stat_t *
sysc_stat(const char *ap_id, int *fdp)
{
	int fd;
	static sysc_cfga_stat_t sc_list[MAX_BOARDS];


	if ((fd = open(ap_id, O_RDWR, 0)) == -1)
		return (NULL);
	else if (ioctl(fd, SYSC_CFGA_CMD_GETSTATUS, sc_list) == -1) {
		close(fd);
		return (NULL);
	} else if (fdp)
		*fdp = fd;

	return (sc_list);
}

/*
 * This code implementes the simulation of the ioctls that transition state.
 * The GETSTAT ioctl is not simulated.  In this way a snapshot of the system
 * state is read and manipulated by the simulation routines.  It is basically
 * a useful debugging tool.
 */
#ifdef	SIM
static int sim_idx;
static int sim_fd = -1;
static int sim_size = MAX_BOARDS * sizeof (sysc_cfga_stat_t);
static sysc_cfga_stat_t sim_sc_list[MAX_BOARDS];

static sysc_cfga_stat_t *
sim_sysc_stat(const char *ap_id, int *fdp)
{
	int fd;
	struct stat buf;

	if (sim_fd != -1)
		return (sim_sc_list);

	if ((sim_fd = open("/tmp/cfga_simdata", O_RDWR|O_CREAT)) == -1) {
		perror("sim_open");
		exit(1);
	} else if (fstat(sim_fd, &buf) == -1) {
		perror("sim_stat");
		exit(1);
	}

	if (buf.st_size) {
		if (buf.st_size != sim_size) {
			perror("sim_size");
			exit(1);
		} else if (read(sim_fd, sim_sc_list, sim_size) == -1) {
			perror("sim_read");
			exit(1);
		}
	} else if ((fd = open(ap_id, O_RDWR, 0)) == -1)
		return (NULL);
	else if (ioctl(fd, SYSC_CFGA_CMD_GETSTATUS, sim_sc_list) == -1) {
		close(fd);
		return (NULL);
	} else if (fdp)
		*fdp = fd;

	return (sim_sc_list);
}

static int
sim_open(char *a, int b, int c)
{
	printf("sim_open(%s)\n", a);

	if (strcmp(a, "/dev/openprom") == 0)
		return (open(a, b, c));
	return (0);
}

static int
sim_close(int a) { return (0); }

static int
sim_ioctl(int fd, int cmd, void *a)
{
	extern int prom_fd;

	printf("sim_ioctl(%d)\n", sim_idx);

	switch (cmd) {
	case SYSC_CFGA_CMD_CONNECT:
		sim_sc_list[sim_idx].rstate = SYSC_CFGA_RSTATE_CONNECTED;
		break;
	case SYSC_CFGA_CMD_CONFIGURE:
		sim_sc_list[sim_idx].ostate = SYSC_CFGA_OSTATE_CONFIGURED;
		break;
	case SYSC_CFGA_CMD_UNCONFIGURE:
		sim_sc_list[sim_idx].ostate = SYSC_CFGA_OSTATE_UNCONFIGURED;
		break;
	case SYSC_CFGA_CMD_DISCONNECT:
		sim_sc_list[sim_idx].rstate = SYSC_CFGA_RSTATE_DISCONNECTED;
		break;
	case SYSC_CFGA_CMD_QUIESCE_TEST:
	case SYSC_CFGA_CMD_TEST:
		return (0);
	case OPROMGETOPT:
		return (ioctl(prom_fd, OPROMGETOPT, a));
	case OPROMSETOPT:
		return (ioctl(prom_fd, OPROMSETOPT, a));
	}

	if (lseek(sim_fd, SEEK_SET, 0) == -1) {
		perror("sim_seek");
		exit(1);
	}
	if (write(sim_fd, sim_sc_list, sim_size) == -1) {
		perror("sim_write");
		exit(1);
	}

	return (0);
}

#define	open(a, b, c)	sim_open((char *)(a), (int)(b), (int)(c))
#define	close(a)	sim_close(a)
#define	ioctl(a, b, c)	sim_ioctl((int)(a), (int)(b), (void *)(c))
#define	sysc_stat(a, b)	sim_sysc_stat(a, b)
#endif	SIM

static int prom_fd;
static char *promdev = "/dev/openprom";
static char *dlprop = "disabled-board-list";

#define	BUFSIZE		128

typedef union {
	char buf[BUFSIZE];
	struct openpromio opp;
} oppbuf_t;

static int
prom_get_prop(char *var, char **val)
{
	static oppbuf_t oppbuf;
	struct openpromio *opp = &(oppbuf.opp);

	(void) strncpy(opp->oprom_array, var, OBP_MAXPROPNAME);
	opp->oprom_array[OBP_MAXPROPNAME + 1] = '\0';
	opp->oprom_size = BUFSIZE;

	DBG3("getprop(%s, %d)\n", opp->oprom_array, opp->oprom_size);

	if (ioctl(prom_fd, OPROMGETOPT, opp) < 0)
		return (ERR_PROM_GETPROP);
	else if (opp->oprom_size > 0)
		*val = opp->oprom_array;
	else
		*val = NULL;

	return (0);
}

static cfga_err_t
prom_set_prop(char *var, char *val)
{
	oppbuf_t oppbuf;
	struct openpromio *opp = &(oppbuf.opp);
	int varlen = strlen(var) + 1;
	int vallen = strlen(val);

	DBG("prom_set_prop(%s)\n", val);

	(void) strcpy(opp->oprom_array, var);
	(void) strcpy(opp->oprom_array + varlen, val);
	opp->oprom_size = varlen + vallen;

	if (ioctl(prom_fd, OPROMSETOPT, opp) < 0)
		return (ERR_PROM_SETPROP);

	return (0);
}

static int
dlist_find(int board, char **dlist, int *disabled)
{
	int i;
	int err;
	char *p;
	char *dl;
	char b[2];

	if ((prom_fd = open(promdev, O_RDWR, 0)) < 0)
		return (ERR_PROM_OPEN);
	else if (err = prom_get_prop(dlprop, dlist))
		return (err);

	b[1] = 0;
	*disabled = 0;

	if ((dl = *dlist) != NULL) {
		int len = strlen(dl);

		for (i = 0; i < len; i++) {
			int bd;

			b[0] = dl[i];
			bd = strtol(b, &p, 16);

			if (p != b && bd == board)
				(*disabled)++;
		}
	}

	return (0);
}

static int
do_opt(int board, int disable, char *dlist, struct cfga_msg *msgp)
{
	int i, j, n;
	int err;
	int found;
	int update;
	char *p;
	char b[2];
	char ndlist[64];

	b[1] = 0;
	ndlist[0] = 0;
	j = 0;
	found = 0;
	update = 0;

	if (dlist) {
		int len = strlen(dlist);

		for (i = 0; i < len; i++) {
			int bd;

			b[0] = dlist[i];
			bd = strtol(b, &p, 16);

			if (p != b && bd == board) {
				found++;
				if (disable)
					cfga_msg(msgp, STR_BD,
						DIAG_WAS_DISABLED, 0);
				else {
					cfga_msg(msgp, STR_BD,
						DIAG_WILL_ENABLE, 0);
					update++;
					continue;
				}
			}
			ndlist[j++] = dlist[i];
		}
		ndlist[j] = 0;
	}

	if (!found)
		if (disable) {
			cfga_msg(msgp, STR_BD, DIAG_WILL_DISABLE, 0);
			p = &ndlist[j];
			n = sprintf(p, "%d", board);
			p[n] = 0;
			update++;
		} else
			cfga_msg(msgp, STR_BD, DIAG_WAS_ENABLED, 0);

	if (update)
		err = prom_set_prop(dlprop, ndlist);
	else
		err = 0;

	(void) close(prom_fd);

	return (err);
}

static int
ap_idx(const char *ap_id)
{
	int id;
	char *s;
	static char *slot = "slot";

	DBG("ap_idx(%s)\n", ap_id);

	if ((s = strstr(ap_id, slot)) == NULL)
		return (-1);
	else {
		int n;

		s += strlen(slot);
		n = strlen(s);

		DBG3("ap_idx: s=%s, n=%d\n", s, n);

		switch (n) {
		case 2:
			if (!isdigit(s[1]))
				return (-1);
		/* FALLTHROUGH */
		case 1:
			if (!isdigit(s[0]))
				return (-1);
			break;
		default:
			return (-1);
		}
	}

	if ((id = atoi(s)) > MAX_BOARDS)
		return (-1);

	DBG3("ap_idx(%s)=%d\n", s, id);

	return (id);
}

/*ARGSUSED*/
cfga_err_t
cfga_change_state(
	cfga_cmd_t state_change_cmd,
	const char *ap_id,
	const char *options,
	struct cfga_confirm *confp,
	struct cfga_msg *msgp,
	char **errstring,
	cfga_flags_t flags)
{
	int fd;
	int idx;
	int err;
	int force;
	int opterr;
	int disable;
	int disabled;
	cfga_err_t rc;
	sysc_cfga_stat_t *ss;
	sysc_cfga_cmd_t *sc;
	sysc_cfga_rstate_t rs;
	sysc_cfga_ostate_t os;
	char *dlist;
	char outputstr[SYSC_OUTPUT_LEN];

	if (errstring != NULL)
		*errstring = NULL;

	rc = CFGA_ERROR;

	if (options) {
		switch (state_change_cmd) {
		case CFGA_CMD_DISCONNECT:
			cfga_err(errstring, CMD_DISCONNECT, ERR_OPT_NONE, 0);
			return (CFGA_ERROR);
		case CFGA_CMD_UNCONFIGURE:
			cfga_err(errstring, CMD_UNCONFIGURE, ERR_OPT_NONE, 0);
			return (CFGA_ERROR);
		}
		disable = 0;
		if (strcmp(options, cfga_str(OPT_DISABLE)) == 0)
			disable++;
		else if (strcmp(options, cfga_str(OPT_ENABLE)) != 0) {
			cfga_err(errstring, ERR_OPT_ILL, options, 0);
			return (rc);
		}
	}

	if ((idx = ap_idx(ap_id)) == -1) {
		cfga_err(errstring, ERR_AP_ILL, ap_id, 0);
		return (rc);
	} else if ((ss = sysc_stat(ap_id, &fd)) == NULL) {
		cfga_err(errstring, CMD_GETSTAT, 0);
		return (rc);
	}
#ifdef	SIM
	sim_idx = idx;
#endif
	/*
	 * We disallow connecting on the disabled list unless
	 * either the FORCE flag or the enable-at-boot option
	 * is set. The check is made further below
	 */
	if (opterr = dlist_find(idx, &dlist, &disabled)) {
		err = disable ? OPT_DISABLE : OPT_ENABLE;
		cfga_err(errstring, err, opterr, 0);
		return (rc);
	} else
		force = flags & CFGA_FLAG_FORCE;

	rs = ss[idx].rstate;
	os = ss[idx].ostate;

	memset((void *)outputstr, 0, sizeof (outputstr));
	sc = &sysc_cmd;
	sc->outputstr = outputstr;
	cfga_str(SYSC_DEVSTR) = outputstr;

	switch (state_change_cmd) {
	case CFGA_CMD_CONNECT:
		if (rs != SYSC_CFGA_RSTATE_DISCONNECTED)
			cfga_err(errstring, ERR_TRANS, 0);
		else if (disabled && !(force || (options && !disable)))
			cfga_err(errstring, CMD_CONNECT,
				ERR_DISABLED, DIAG_FORCE, 0);
		else if (!(*confp->confirm)(NULL, cfga_str(ASK_CONNECT)))
			return (CFGA_NACK);
		else if (ioctl(fd, SYSC_CFGA_CMD_CONNECT, sc) == -1)
			cfga_err(errstring, CMD_CONNECT, 0);
		else if (options && (opterr = do_opt(idx, disable,
		    dlist, msgp))) {
			err = disable ? OPT_DISABLE : OPT_ENABLE;
			cfga_err(errstring, err, opterr, 0);
		} else
			rc = CFGA_OK;
		break;

	case CFGA_CMD_DISCONNECT:
		if ((os == SYSC_CFGA_OSTATE_CONFIGURED) &&
		    (ioctl(fd, SYSC_CFGA_CMD_UNCONFIGURE, sc) == -1)) {
			cfga_err(errstring, CMD_UNCONFIGURE, 0);
			return (CFGA_ERROR);
		}
		if (rs == SYSC_CFGA_RSTATE_CONNECTED) {
			if (ioctl(fd, SYSC_CFGA_CMD_DISCONNECT, sc) == -1)
				cfga_err(errstring, CMD_DISCONNECT, 0);
			else
				rc = CFGA_OK;
		} else
			cfga_err(errstring, ERR_TRANS, 0);
		break;

	case CFGA_CMD_CONFIGURE:
		if (rs == SYSC_CFGA_RSTATE_DISCONNECTED)
			if (disabled && !(force || (options && !disable))) {
				cfga_err(errstring, CMD_CONFIGURE,
					ERR_DISABLED, DIAG_FORCE, 0);
				return (CFGA_ERROR);
			} else if (!(*confp->confirm)(NULL,
			    cfga_str(ASK_CONNECT)))
				return (CFGA_NACK);
			else if (ioctl(fd, SYSC_CFGA_CMD_CONNECT, sc) == -1) {
				cfga_err(errstring, CMD_CONNECT, 0);
				return (CFGA_ERROR);
			}
		if (os == SYSC_CFGA_OSTATE_UNCONFIGURED) {
			if (ioctl(fd, SYSC_CFGA_CMD_CONFIGURE, sc) == -1)
				cfga_err(errstring, CMD_CONFIGURE, 0);
			else if (options &&
				(opterr = do_opt(idx, disable, dlist, msgp))) {
				err = disable ? OPT_DISABLE : OPT_ENABLE;
				cfga_err(errstring, err, opterr, 0);
			} else
				rc = CFGA_OK;
		} else
			cfga_err(errstring, ERR_TRANS, 0);
		break;

	case CFGA_CMD_UNCONFIGURE:
		if (os != SYSC_CFGA_OSTATE_CONFIGURED)
			cfga_err(errstring, ERR_TRANS, 0);
		else if (ioctl(fd, SYSC_CFGA_CMD_UNCONFIGURE, sc) == -1)
			cfga_err(errstring, CMD_UNCONFIGURE, 0);
		else
			rc = CFGA_OK;
		break;

	default:
		rc = CFGA_OPNOTSUPP;
		break;
	}

	return (rc);
}

/*ARGSUSED*/
cfga_err_t
cfga_private_func(
	const char *function,
	const char *ap_id,
	const char *options,
	struct cfga_confirm *confp,
	struct cfga_msg *msgp,
	char **errstring,
	cfga_flags_t flags)
{
	int fd;
	int cmd;
	int err;
	cfga_err_t rc;
	char outputstr[SYSC_OUTPUT_LEN];

	if (errstring != NULL)
		*errstring = NULL;

	if (strcmp(function, cfga_str(CMD_QUIESCE)) == 0) {
		cmd = SYSC_CFGA_CMD_QUIESCE_TEST;
		err = CMD_QUIESCE;
	} else if (strcmp(function, cfga_str(CMD_INSERT)) == 0) {
		cmd = SYSC_CFGA_CMD_TEST;
		err = CMD_INSERT;
	} else if (strcmp(function, cfga_str(CMD_REMOVE)) == 0) {
		cmd = SYSC_CFGA_CMD_TEST;
		err = CMD_REMOVE;
	} else {
		cfga_err(errstring, ERR_OPT_UNKNOWN, (char *)function, 0);
		return (CFGA_ERROR);
	}

	memset((void *)outputstr, 0, sizeof (outputstr));
	sysc_cmd.outputstr = outputstr;
	cfga_str(SYSC_DEVSTR) = outputstr;

	rc = CFGA_ERROR;

	if (ap_idx(ap_id) == -1)
		cfga_err(errstring, ERR_AP_ILL, ap_id, 0);
	else if (((fd = open(ap_id, O_RDWR, 0)) == -1) ||
	    (ioctl(fd, cmd, &sysc_cmd) == -1))
		cfga_err(errstring, err, 0);
	else
		rc = CFGA_OK;

	return (rc);
}


/*ARGSUSED*/
cfga_err_t
cfga_test(
	int num_ap_ids,
	char *const *ap_ids,
	const char *options,
	struct cfga_msg *msgp,
	char **errstring,
	cfga_flags_t flags)
{
	if (errstring != NULL)
		*errstring = NULL;

	return (CFGA_NOTSUPP);
}

static cfga_stat_t
rstate_cvt(sysc_cfga_rstate_t rs)
{
	cfga_stat_t cs;

	switch (rs) {
	case SYSC_CFGA_RSTATE_EMPTY:
		cs = CFGA_STAT_EMPTY;
		break;
	case SYSC_CFGA_RSTATE_DISCONNECTED:
		cs = CFGA_STAT_DISCONNECTED;
		break;
	case SYSC_CFGA_RSTATE_CONNECTED:
		cs = CFGA_STAT_CONNECTED;
		break;
	default:
		cs = CFGA_STAT_NONE;
		break;
	}

	return (cs);
}

static cfga_stat_t
ostate_cvt(sysc_cfga_ostate_t os)
{
	cfga_stat_t cs;

	switch (os) {
	case SYSC_CFGA_OSTATE_UNCONFIGURED:
		cs = CFGA_STAT_UNCONFIGURED;
		break;
	case SYSC_CFGA_OSTATE_CONFIGURED:
		cs = CFGA_STAT_CONFIGURED;
		break;
	default:
		cs = CFGA_STAT_NONE;
		break;
	}

	return (cs);
}

static cfga_cond_t
cond_cvt(sysc_cfga_cond_t sc)
{
	cfga_cond_t cc;

	switch (sc) {
	case SYSC_CFGA_COND_OK:
		cc = CFGA_COND_OK;
		break;
	case SYSC_CFGA_COND_FAILING:
		cc = CFGA_COND_FAILING;
		break;
	case SYSC_CFGA_COND_FAILED:
		cc = CFGA_COND_FAILED;
		break;
	case SYSC_CFGA_COND_UNUSABLE:
		cc = CFGA_COND_UNUSABLE;
		break;
	case SYSC_CFGA_COND_UNKNOWN:
	default:
		cc = CFGA_COND_UNKNOWN;
		break;
	}

	return (cc);
}

static char *
type_str(enum board_type type)
{
	char *type_str;

	switch (type) {
	case MEM_BOARD:
		type_str = cfga_str(BD_MEM);
		break;
	case CPU_BOARD:
		type_str = cfga_str(BD_CPU);
		break;
	case IO_2SBUS_BOARD:
		type_str = cfga_str(BD_IO_2SBUS);
		break;
	case IO_SBUS_FFB_BOARD:
		type_str = cfga_str(BD_IO_SBUS_FFB);
		break;
	case IO_PCI_BOARD:
		type_str = cfga_str(BD_IO_PCI);
		break;
	case DISK_BOARD:
		type_str = cfga_str(BD_DISK);
		break;
	case UNKNOWN_BOARD:
	default:
		type_str = cfga_str(BD_UNKNOWN);
		break;
	}
	return (type_str);
}

static void
info_set(sysc_cfga_stat_t *sc, cfga_info_t info)
{
	int i;
	struct cpu_info *cpu;
	union bd_un *bd = &sc->bd;

	*info = NULL;

	switch (sc->type) {
	case CPU_BOARD:
		if (sc->rstate == SYSC_CFGA_RSTATE_DISCONNECTED)
			break;
		else
			cpu = bd->cpu;
		for (i = 0; i < 2; i++, cpu++) {
			if (cpu->cpu_speed > 1) {
				info += sprintf(info, "CPU %d: ", i);
				info += sprintf(info, "%3d MHz",
					cpu->cpu_speed);
			}
			if (cpu->cache_size)
				info += sprintf(info, " %0.1fM ",
					(float)cpu->cache_size /
					(float)(1024 * 1024));
		}
		break;
	case IO_SBUS_FFB_BOARD:
		switch (bd->io2.ffb_size) {
		case FFB_SINGLE:
			info += sprintf(info, "Single buffered FFB");
			break;
		case FFB_DOUBLE:
			info += sprintf(info, "Double buffered FFB");
			break;
		case FFB_NOT_FOUND:
			info += sprintf(info, "No FFB installed");
			break;
		default:
			info += sprintf(info, "Illegal FFB size");
			break;
		}
		break;
	case DISK_BOARD:
		for (i = 0; i < 2; i++)
			if (bd->dsk.disk_pres[i])
				info += sprintf(info, " Target: %2d   ",
						bd->dsk.disk_id[i], 0);
			else
				info += sprintf(info, " no disk      ");
		break;
	}
}

static void
sysc_cvt(sysc_cfga_stat_t *sc, cfga_stat_data_t *cs)
{
	strcpy(cs->ap_type, type_str(sc->type));
	cs->ap_r_state = rstate_cvt(sc->rstate);
	cs->ap_o_state = ostate_cvt(sc->ostate);
	cs->ap_cond = cond_cvt(sc->condition);
	cs->ap_busy = (cfga_busy_t)sc->in_transition;
	cs->ap_status_time = sc->last_change;
	info_set(sc, cs->ap_info);
	cs->ap_log_id[0] = NULL;
	cs->ap_phys_id[0] = NULL;
}

/*ARGSUSED*/
cfga_err_t
cfga_list(
	const char *ap_id,
	cfga_stat_data_t **ap_list,
	int *nlist,
	const char *options,
	char **errstring)
{
	int i;
	cfga_err_t rc;
	sysc_cfga_stat_t *sc;
	cfga_stat_data_t *cs;

	if (errstring != NULL)
		*errstring = NULL;

	rc = CFGA_ERROR;

	if (ap_idx(ap_id) == -1)
		cfga_err(errstring, ERR_AP_ILL, ap_id, 0);
	else if ((sc = sysc_stat(ap_id, NULL)) == NULL)
		cfga_err(errstring, CMD_LIST, 0);
	else if (!(cs = (cfga_stat_data_t *)malloc(MAX_BOARDS * sizeof (*cs))))
		cfga_err(errstring, CMD_LIST, 0);
	else {
		*ap_list = cs;

		for (*nlist = 0, i = 0; i < MAX_BOARDS; i++, sc++) {
			if (sc->board == -1)
				continue;
			sysc_cvt(sc, cs++);
			(*nlist)++;
		}

		rc = CFGA_OK;
	}

	return (rc);
}

/*ARGSUSED*/
cfga_err_t
cfga_stat(
	const char *ap_id,
	struct cfga_stat_data *cs,
	const char *options,
	char **errstring)
{
	cfga_err_t rc;
	int idx;
	sysc_cfga_stat_t *sc;

	if (errstring != NULL)
		*errstring = NULL;

	rc = CFGA_ERROR;

	if ((idx = ap_idx(ap_id)) == -1)
		cfga_err(errstring, ERR_AP_ILL, ap_id, 0);
	else if ((sc = sysc_stat(ap_id, NULL)) == NULL)
		cfga_err(errstring, CMD_GETSTAT, 0);
	else {
		sysc_cvt(sc + idx, cs);
		rc = CFGA_OK;
	}

	return (rc);
}

/*ARGSUSED*/
cfga_err_t
cfga_help(struct cfga_msg *msgp, const char *options, cfga_flags_t flags)
{
	int help = 0;

	if (options) {
		if (strcmp(options, cfga_str(OPT_DISABLE)) == 0)
			help = HELP_DISABLE;
		else if (strcmp(options, cfga_str(OPT_ENABLE)) == 0)
			help = HELP_ENABLE;
		else if (strcmp(options, cfga_str(CMD_INSERT)) == 0)
			help = HELP_INSERT;
		else if (strcmp(options, cfga_str(CMD_REMOVE)) == 0)
			help = HELP_REMOVE;
		else if (strcmp(options, cfga_str(CMD_QUIESCE)) == 0)
			help = HELP_QUIESCE;
		else
			help = HELP_UNKNOWN;
	}

	if (help)  {
		if (help == HELP_UNKNOWN)
			cfga_msg(msgp, help, options, 0);
		else
			cfga_msg(msgp, help, 0);
	} else {
		cfga_msg(msgp, HELP_DISABLE, 0);
		cfga_msg(msgp, HELP_ENABLE, 0);
		cfga_msg(msgp, HELP_INSERT, 0);
		cfga_msg(msgp, HELP_REMOVE, 0);
		cfga_msg(msgp, HELP_QUIESCE, 0);
	}

	return (CFGA_OK);
}

/*ARGSUSED*/
int
cfga_ap_id_cmp(const cfga_ap_log_id_t ap_id1, const cfga_ap_log_id_t ap_id2)
{
	return (strcmp(ap_id1, ap_id2));
}

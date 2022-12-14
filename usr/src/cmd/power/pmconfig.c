/*
 * Copyright (c) 1994-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)pmconfig.c	1.19	98/01/20 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/pm.h>
#ifdef sparc
#include <sys/mnttab.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/cpr.h>
#include <sys/openpromio.h>
#endif
#include "powerd.h"

typedef enum {
	NAME, NUMBER, EOL, TIME, EOFTOK, ERROR
} token_t;

static token_t	lex(FILE *, char *);
static void	find_eol(FILE *);
static void	process_device(FILE *, int fd, char *);
static int	other_valid_tokens(char *);
static int	is_default_device(char *);
static int	launch(char *, char *[]);
#ifdef sparc
static void	process_statefile(FILE *, char *);
static int	parse_statefile_path(char *, struct cprconfig *);
static int	cfcheck(struct cprconfig *, char *, char *, int);
#endif

#define	MAXNAMELEN	256
#define	ERR_MSG_LEN	MAXNAMELEN+64
#define	CONF_FILE	"/etc/power.conf"
#define	MNTTAB		"/etc/mnttab"
#define	USAGE		"Usage:\t%s [ -r ]\n"
#define	OPENPROM	"/dev/openprom"
#define	PROM_BUFSZ	1024

static int	lineno;
static char	*me;		/* name of this program */

int
main(int argc, char *argv[])
{
	FILE		*file;
	char		buf[MAXNAMELEN];
	int		fd;
	token_t		token;
	key_t		key;
	int		shmid;
	pwr_info_t	*daemon = (pwr_info_t *)-1;
#ifdef sparc
	static int	found_statefile;

	static char	*dup_msg =
			"%s (line %d): Ignoring redundant statefile entry.\n";
#endif
	me = argv[0];
	if (argc > 2) {
		(void) fprintf(stderr, USAGE, me);
		exit(1);
	}

	/*
	 * Load platform specifc module (if present)
	 */
	if ((fd = open("/dev/pmc", O_RDONLY)) != -1)
		(void) close(fd);

	if ((fd = open("/dev/pm", O_RDWR)) == -1) {
		(void) fprintf(stderr, "%s: Can't open \"/dev/pm\"\n", me);
		exit(1);
	}
	if ((key = ftok("/dev/pm", 'P')) < 0) {
		(void) fprintf(stderr, "%s: Unable to access /dev/pm", me);
		(void) close(fd);
		exit(1);
	}
	shmid = shmget(key, (int)sizeof (pwr_info_t), SHM_RDONLY);
	if (shmid != -1)
		daemon = (pwr_info_t *)shmat(shmid, NULL, SHM_RDONLY);
	if (argc == 2 && strcmp(argv[1], "-r") != 0) {
		(void) fprintf(stderr, USAGE, me);
		exit(1);
	}
	if (ioctl(fd, PM_REM_DEVICES, 0) < 0) {
		(void) fprintf(stderr,
		    "%s: Unable to remove power managed devices\n", me);
		(void) close(fd);
		exit(1);
	}
	if (argc == 1) {
		if (!(file = fopen(CONF_FILE, "r"))) {
			(void) fprintf(stderr,
			    "%s: Can't open \"%s\"\n", me, CONF_FILE);
			(void) close(fd);
			exit(1);
		}

		/*
		 * Line by line parsing
		 */
		lineno = 1;
		while ((token = lex(file, buf)) != EOFTOK) {
			if (token == EOL)
				continue;
			if (token != NAME) {	/* Want a name first */
				(void) fprintf(stderr,
				    "%s (line %d): Must begin with a name\n",
				    me, lineno);
				find_eol(file);
				continue;
			}
			if (other_valid_tokens(buf)) {
				find_eol(file);
				continue;
#ifdef sparc
			} else
				if (strcmp(buf, "statefile") == 0) {
					if (++found_statefile > 1)
						(void) fprintf(stderr,
							dup_msg, me, lineno);
					else
						process_statefile(file, buf);
					find_eol(file);
#endif
				} else
					process_device(file, fd, buf);

		}
		(void) fclose(file);
		if (daemon == (pwr_info_t *)-1) {
			if (launch("/usr/lib/power/powerd", NULL) == -1) {
				(void) fprintf(stderr,
				    "%s: Failed to start powerd: ", me);
				perror(NULL);
			}

		} else if (sigsend(P_PID, daemon->pd_pid, SIGHUP)) {
			if (errno == ESRCH) {
				if (launch("/usr/lib/power/powerd",
				    NULL) == -1) {
					(void) fprintf(stderr,
					    "%s: Failed to start powerd: ", me);
					perror(NULL);
				}
			} else {
				(void) fprintf(stderr, "%s: Error sending"
					" signal to powerd: ", me);
				perror(NULL);
			}
		}
	}
	if (daemon != (pwr_info_t *)-1)
		(void) shmdt((void *)daemon);
	(void) close(fd);
	return (0);
}

#ifdef sparc
/*
 * Parse the "statefile" line.  The 2nd argument is the path selected
 * by the user to contain the cpr statefile.  Verify that either:
 * -it resides on a writeable, local, UFS filesystem and that if it
 *  already exists and is not a directory, or:
 * -it is a character special file upon which no file system is mounted
 *
 * Then create the hidden config file in root used
 * by both the cpr kernel module and the booter program.
 */
static void
process_statefile(FILE *file, char *buf)
{
	typedef union {
		char	buf[PROM_BUFSZ];
		struct	openpromio opp;
	} Oppbuf;
	Oppbuf oppbuf;
	int	fdprom;
	struct openpromio *opp = &(oppbuf.opp);
	int	fd;
	static struct cprconfig cf;
	char err_buf[ERR_MSG_LEN];

	if (uadmin(A_FREEZE, AD_CHECK, 0) != 0) {
		if (errno != ENOTSUP) {
			fprintf(stderr, "%s: ", me);
			perror("uadmin(A_FREEZE, A_CHECK, 0)");
			return;
		}
		return;
	}
	if (lex(file, buf) != NAME || *buf != '/') {
		fprintf(stderr, "%s: %s line (%d) \"statefile\" requires "
		    "pathname argument.\n", me, CONF_FILE, lineno);

		return;
	}

	if ((fdprom = open(OPENPROM, O_RDONLY)) == -1) {
		sprintf(err_buf, "%s: %s line (%d) Cannot open %s",
		    me, CONF_FILE, lineno, OPENPROM);
		perror(err_buf);
		return;
	}

	if (parse_statefile_path(buf, &cf))
	    return;

	/*
	 * Convert the device special file representing the filesystem
	 * containing the cpr statefile to a full device path.
	 */
	opp->oprom_size = PROM_BUFSZ;
	strcpy(opp->oprom_array, cf.cf_devfs);

	if (ioctl(fdprom, OPROMDEV2PROMNAME, opp) == -1) {
		sprintf(err_buf, "%s: %s line (%d) failed to convert "
			"mount point %s to prom name",
			me, CONF_FILE, lineno, opp->oprom_array);
		perror(err_buf);
		return;
	}
	strcpy(cf.cf_dev_prom, opp->oprom_array);
	cf.cf_magic = CPR_CONFIG_MAGIC;

	/*
	 * Might not need to do anything, so only complain if
	 * what we would write would be different from what is
	 *  there
	 */
	if ((fd = open(CPR_CONFIG, O_CREAT | O_WRONLY, 0600)) == -1) {
		int eno = errno;
		if (cfcheck(&cf, me, err_buf, lineno) == -1) {
			errno = eno;
			sprintf(err_buf, "%s: %s line (%d) Can't open %s",
			    me, CONF_FILE, lineno, CPR_CONFIG);
			perror(err_buf);
			return;
		} else {
			/*
			 * The write is a no-op, so skip it
			 */
			return;
		}

	}

	if (write(fd, &cf, sizeof (cf)) != sizeof (cf)) {
		sprintf(err_buf, "%s: %s line (%d) error writing %s.",
		    me, CONF_FILE, lineno, CPR_CONFIG);
		perror(err_buf);
		return;
	}
}

/*
 * Parse the statefile path and populate the cprconfig structure with
 * the results.
 */
int
parse_statefile_path(char *path, struct cprconfig *cf)
{
	FILE	*fp;
	static struct mnttab mtab_ref = { NULL, NULL, "ufs", NULL, NULL };
	struct	mnttab mtab;
	struct stat st;
	char	*slash;
	int	longest = 0;
	int	error;
	char	err_buf[ERR_MSG_LEN];
	dev_t	dev;

	slash = strrchr(path, '/'); /* Caller guarantees initial slash */

	/*
	 * First check for the character special device
	 */
	if (stat(path, &st) != -1 && (st.st_mode & S_IFMT) == S_IFBLK) {
		cf->cf_type = CFT_SPEC;
		strcpy(cf->cf_devfs, path);
		dev = st.st_rdev;
	} else if ((st.st_mode & S_IFMT) == S_IFCHR) {
		fprintf(stderr, "%s: %s line (%d) -- statefile \"%s\" is "
		    "a character special file. Only block special or "
		    "regular files are supported.\n", me, CONF_FILE, lineno,
		    path);
		return (-1);
	} else {
		cf->cf_type = CFT_UFS;

		/*
		 * Make sure penultimate component of path is a directory.  Only
		 * need to make this test for non-root.
		 */
		if (slash != path) {
			*slash = '\0';		/* Put the slash back later! */
			if (stat(path, &st) == -1 ||
			    (st.st_mode & S_IFMT) != S_IFDIR) {
				fprintf(stderr, "%s: %s line (%d) "
				    "statefile directory %s not found.\n",
				    me, CONF_FILE, lineno, path);
				*slash = '/';
				return (-1);
			}
			*slash = '/';
		}

		/*
		 * If the path refers to existing file, it must be a regular
		 * file.
		 */
		if ((error =
		    stat(path, &st)) == 0 && (st.st_mode & S_IFMT) != S_IFREG) {
			fprintf(stderr, "%s: %s line (%d) -- "
			    "statefile \"%s\" is not a regular file.\n",
			    me, CONF_FILE, lineno, path);
			return (-1);
		}

		/*
		 * If unable to stat the full path, "not found" is the
		 * only excuse.
		 */
		if (error == -1 && errno != ENOENT) {
			sprintf(err_buf, "%s: %s line (%d) -- "
			    "cannot access statefile \"%s\"",
			    me, CONF_FILE, lineno, path);
			perror(err_buf);
			return (-1);
		}
	}

	if ((fp = fopen(MNTTAB, "r")) == NULL) {
		sprintf(err_buf, "%s: %s line (%d) "
		    " error opening %s", me, CONF_FILE, lineno, MNTTAB);
		perror(err_buf);

		return (-1);
	}

	/*
	 * Read all the ufs entries in mnttab.  For UFS statefile, try to
	 * match each mountpoint path with the start of the full path.  The
	 * longest such match is the mountpoint of the statefile fs.
	 * For SPEC statefile, look up the mount point and see if it is the
	 * same device as the statefile, (not allowed).
	 */
	while ((error = getmntany(fp, &mtab, &mtab_ref)) != -1) {
		int	len;

		if (error > 0) {
			fprintf(stderr, "%s: %s line "
			    "(%d) error reading %s\n",
			    me, CONF_FILE, lineno, MNTTAB);
			longest = -1;
			break;
		}

		switch (cf->cf_type) {
		case CFT_UFS:
			len = strlen(mtab.mnt_mountp);
			if (strncmp(mtab.mnt_mountp, path, len) == 0 &&
				path[len == 1 ? 0: len ] == '/')
				if (len > longest) {
					longest = len;
					strcpy(cf->cf_fs, mtab.mnt_mountp);
					strcpy(cf->cf_devfs, mtab.mnt_special);
					strcpy(cf->cf_path,
					    longest == 1 ?
					    path + 1 : path + longest + 1);
				}
			break;
		case CFT_SPEC:
			if (stat(mtab.mnt_special, &st) != -1 &&
			    (st.st_rdev == dev)) {
				/* fs mounted on statefile! */
				fprintf(stderr, "%s: %s line (%d) -- "
				    "statefile \"%s\" has a file system mounted"
				    " on it.\n", me, CONF_FILE, lineno, path);
				return (-1);
			}
		}
	}
	fclose(fp);

	return (longest == -1 ? longest : 0);
}

static int
cfcheck(struct cprconfig *cfp, char *me, char *err_buf, int lineno)
{
	static struct cprconfig cf;
	int fd;

	if ((fd = open(CPR_CONFIG, O_RDONLY, 0600)) == -1) {
		sprintf(err_buf, "%s: %s line (%d) Can't open %s",
		    me, CONF_FILE, lineno, CPR_CONFIG);
		perror(err_buf);
		return (-1);
	}

	if (read(fd, &cf, sizeof (cf)) != sizeof (cf)) {
		sprintf(err_buf, "%s: %s line (%d) error reading %s.",
		    me, CONF_FILE, lineno, CPR_CONFIG);
		perror(err_buf);
		return (-1);
	}
	if (cf.cf_magic != cfp->cf_magic || cf.cf_type != cfp->cf_type) {
		sprintf(err_buf, "%s: %s line (%d) bad magic or wrong type in "
		    "%s.", me, CONF_FILE, lineno, CPR_CONFIG);
		return (-1);
	}
	if (strcmp(cf.cf_path, cfp->cf_path) != 0) {
		sprintf(err_buf, "%s: %s line (%d) expected cf_path '%s', "
		    "found cf_path '%s' in %s.",
		    me, CONF_FILE, lineno, cf.cf_path, cfp->cf_path,
		    CPR_CONFIG);
		return (-1);
	}
	if (strcmp(cf.cf_fs, cfp->cf_fs) != 0) {
		sprintf(err_buf, "%s: %s line (%d) expected cf_fs '%s', "
		    "found cf_fs '%s' in %s.",
		    me, CONF_FILE, lineno, cf.cf_fs, cfp->cf_fs,
		    CPR_CONFIG);
		return (-1);
	}
	if (strcmp(cf.cf_devfs, cfp->cf_devfs) != 0) {
		sprintf(err_buf, "%s: %s line (%d) expected cf_devfs '%s', "
		    "found cf_devfs '%s' in %s.",
		    me, CONF_FILE, lineno, cf.cf_devfs, cfp->cf_devfs,
		    CPR_CONFIG);
		return (-1);
	}
	if (strcmp(cf.cf_dev_prom, cfp->cf_dev_prom) != 0) {
		sprintf(err_buf, "%s: %s line (%d) expected cf_dev_prom '%s', "
		    "found cf_dev_prom '%s' in %s.",
		    me, CONF_FILE, lineno, cf.cf_dev_prom, cfp->cf_dev_prom,
		    CPR_CONFIG);
		return (-1);
	}
	return (0);
}
#endif

static void
process_device(FILE *file, int fd, char *buf1)
{
	char		*ch, buf2[MAXNAMELEN];
	pm_request	req;
	token_t		token;
	int		value, ret, cmpt, dep;
	int		neg = 1;

	req.who = buf1;
	req.dependent = buf2;
	ret = -1;

	for (cmpt = 0; (token = lex(file, buf2)) == NUMBER; cmpt++) {
		value = 0;
		ch = buf2;
		if (*ch == '-') {
			neg = -1;
			ch++;
		}
		while (*ch != '\0')
			value = value * 10 + *ch++ - '0';
		req.select = cmpt;
		req.level = value * neg;
		if ((ret = ioctl(fd, PM_SET_THRESHOLD, &req)) < 0)
			break;
	}
	if (ret < 0) {
		if (token != NUMBER && cmpt == 0) {
			(void) fprintf(stderr, "%s (line %d): ", me, lineno);
			(void) fprintf(stderr, "Can't find threshold value\n");
		} else if (errno == EINVAL) {
			(void) fprintf(stderr, "%s (line %d): ", me, lineno);
			(void) fprintf(stderr, "Threshold value error\n");
		} else {
			/*
			 * Don't complain about the default devices,
			 * as the user can't be expected to do anything
			 * about their absence
			 */
			if (errno != ENODEV || !is_default_device(req.who)) {
				(void) fprintf(stderr, "%s (line %d): ", me,
				    lineno);
				(void) fprintf(stderr, "\"%s\" ", req.who);
				perror(NULL);
			}
		}
		find_eol(file);
		return;
	}
	for (dep = 0; token != EOL && token != EOFTOK; dep++) {
		if (token != NAME) {
			(void) fprintf(stderr, "%s (line %d): "
			    "Unrecognizable dependent name \"%s\"\n",
			    me, lineno, buf2);
		} else if ((ret = ioctl(fd, PM_ADD_DEP, &req)) < 0) {
			/*
			 * Don't complain about the default devices,
			 * as the user can't be expected to do anything
			 * about their absence
			 */
			if (errno != ENODEV || !is_default_device(buf2)) {
				(void) fprintf(stderr, "%s (line %d): \"%s\" ",
				    me, lineno, buf2);
				perror(NULL);
			}
		}
		token = lex(file, buf2);
	}
}

static int
other_valid_tokens(char *name)
{
	return ((strcmp(name, "autoshutdown") == 0 ||
		strcmp(name, "autoshutdown-cmd") == 0 ||
		strcmp(name, "power-button-cmd") == 0 ||
		strcmp(name, "low-battery-cmd") == 0 ||
		strcmp(name, "ttychars") == 0 ||
		strcmp(name, "loadaverage") == 0 ||
		strcmp(name, "diskreads") == 0 ||
		strcmp(name, "nfsreqs") == 0 ||
		strcmp(name, "idlecheck") == 0) ? 1 : 0);
}

#define	isspace(ch)	((ch) == ' ' || (ch) == '\t')
#define	iseol(ch)	((ch) == '\n' || (ch) == '\r' || (ch) == '\f')
#define	isdelimiter(ch)	(isspace(ch) || iseol(ch))

static token_t
lex(FILE *file, char *cp)
{
	register int	ch;
	register int	neg = 0;

	while ((ch  = getc(file)) != EOF) {
		if (isspace(ch))
			continue;
		if (ch == '\\') {
			ch = (char)getc(file);
			if (iseol(ch)) {
				lineno++;
				continue;
			} else {
				(void) ungetc(ch, file);
				ch = '\\';
				break;
			}
		}
		break;
	}

	*cp++ = ch;
	if (ch == EOF)
		return (EOFTOK);
	if (iseol(ch)) {
		lineno++;
		return (EOL);
	}
	if (ch == '#') {
		find_eol(file);
		return (EOL);
	}
	if ((ch >= '0' && ch <= '9') || ch == '-') {
		if (ch == '-')
			neg = 1;
		while ((ch = (char)getc(file)) >= '0' && ch <= '9')
			*cp++ = ch;
		if (isdelimiter(ch) || ch == EOF) {
			*cp = '\0';
			(void) ungetc(ch, file);
			return (NUMBER);
		}
		if (ch == ':' && !neg) {
			*cp++ = ch;
			while ((ch = (char)getc(file)) >= '0' && ch <= '9')
				*cp++ = ch;
			if (isdelimiter(ch) || ch == EOF) {
				*cp = '\0';
				(void) ungetc(ch, file);
				return (TIME);
			}
		}
		while ((ch = (char)getc(file)) != EOF && !isdelimiter(ch));
		(void) ungetc(ch, file);
		return (ERROR);
	}
	while ((ch = (char)getc(file)) != EOF && !isdelimiter(ch))
		*cp++ = ch;
	*cp = '\0';
	(void) ungetc(ch, file);
	return (NAME);
}

static void
find_eol(FILE *file)
{
	register int ch, last = '\0';

	do {
		while ((ch = (char)getc(file)) != EOF && !iseol(ch))
			last = ch;
		lineno++;
	} while (last == '\\' && ch != EOF);
}

/*
 * We ship with certain devices named in the power.conf file.  If the named
 * device does not exist it will generally be beyond the power of the user
 * to do anything about it (e.g. x86 and ppc keyboard/mouse drivers don't
 * support PM yet).
 * So we filter out these, sincd we *do* want to complain about any that the
 * user has added.
 */
static char *default_devices[] = {
"/dev/kbd",
"/dev/mouse",
"/dev/fb",
0
};

static int
is_default_device(char *name)
{
	char **dp = default_devices;

	while (*dp) {
		if (strcmp(*dp++, name) == 0)
			return (1);
	}
	return (0);
}

/*
 * Launch the specified program and wait for it to complete.  We use this
 * to launch powerd since it's faster than system(3s) and we don't need
 * to use any shell syntax.
 */

static int
launch(char *file, char *argv[])
{
	char *fallback_argv[2];
	pid_t pid, p;
	int status;

	if (argv == NULL) {
		fallback_argv[0] = file;
		fallback_argv[1] = NULL;
		argv = fallback_argv;
	}

	if ((pid = vfork()) == 0) {
		(void) execv(file, argv);
		_exit(127);
		/*NOTREACHED*/
	} else if (pid == -1)
		return (-1);

	do {
		p = waitpid(pid, &status, 0);
	} while (p == -1 && errno == EINTR);

	return ((p == -1) ? -1 : status);
}

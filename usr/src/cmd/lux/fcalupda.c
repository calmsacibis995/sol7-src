/*
 * Copyright 1997 Sun Microsystems, Inc. All rights reserved.
 */

#pragma ident   "@(#)fcalupdate.c 1.8     97/08/06 SMI"

#include <fcntl.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <siginfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/exec.h>
#include <sys/exechdr.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socalreg.h>
#include <sys/socalio.h>
#include <sys/time.h>
#include <nl_types.h>

#include "luxdef.h"

#define	WWN_SIZE		16
#define	FEPROM_SIZE		256*1024
#define	FEPROM_MAX_PROGRAM	25
#define	FEPROM_MAX_ERASE	1000

#define	FEPROM_READ_MEMORY	0x00
#define	FEPROM_ERASE		0x20
#define	FEPROM_ERASE_VERIFY	0xa0
#define	FEPROM_PROGRAM		0x40
#define	FEPROM_PROGRAM_VERIFY	0xc0
#define	FEPROM_RESET		0xff

#define	FOUND			0
#define	NOT_FOUND		1

#define	PROM_SIZ		0x20010
/*
 * The next define is to work around a problem with sbusmem driver not
 * able to round up mmap() requests that are not around page boundaries.
 */
#define	PROM_SIZ_ROUNDED	0x22000
#define	SAMPLE_SIZ		0x100

#define	REG_OFFSET		0x20000

#define	FEPROM_WWN_OFFSET	0x3fe00

#define	FEPROM_SUN_WWN		0x50200200

#define	ONBOARD_SOCAL		"SUNW,socal@d"

static u_char	buffer[FEPROM_SIZE];
static char	soc_name[] = "SUNW,socal";
static char	sbus_list[128][PATH_MAX];
static char	sbussoc_list[128][PATH_MAX];
static char	bootpath[PATH_MAX];
static char	version[MAXNAMELEN];

static u_int	getsbuslist(void);
static void	load_file(char *, caddr_t, volatile socal_reg_t *);
static void	usec_delay(int);
static void	getbootdev(unsigned int);
static int	warn(void);
static int	findversion(int, u_char *);
static int	write_feprom(u_char *, u_char *, volatile socal_reg_t *);
static int	feprom_erase(volatile u_char *, volatile socal_reg_t *);

static struct exec exec;

static char *warnstring =
"WARNING!! This program will update the Fcode in this FC100/S Sbus Card.\n";
static char *warnstring1 =
"This may take a few (5) minutes. Please be patient.\n";
static char *warnstring2 = "Do you wish to continue ? (y/n) ";

extern	nl_catd	l_catd;

void
fcal_update(unsigned int verbose, char *file)
/*ARGSUSED*/
{
	int fd;
	caddr_t addr;
	u_int i;
	u_int fflag = 0;
	u_int vflag = 0;
	u_int numslots;
	volatile socal_reg_t *regs;
	char *slotname, socal[MAXNAMELEN], strings_buf[MAXNAMELEN];

	if (!file)
		vflag++;
	else {
		fflag++;
		(void) sprintf(strings_buf,
			"strings %s | fgrep SUNW,socal > /dev/null", file);
		if (system(strings_buf) != 0) {
			(void) fprintf(stderr,
		MSGSTR(-1, "Error: %s is not a FC100/S Fcode file\n"), file);
			return;
		}
	}

	/*
	 * Get count of, and names of SBus slots using the SBus memory
	 * interface.
	 */
	(void) getbootdev(verbose);
	if (getenv("_LUX_D_DEBUG") != NULL) {
		(void) fprintf(stdout, "  Bootpath: %s\n", bootpath);
	}

	numslots = getsbuslist();
	(void) fprintf(stdout,
	MSGSTR(-1, "  Found Path to %d FC100/S Cards\n"), numslots);

	for (i = 0; i < numslots; i++) {

		/*
		 * Open SBus memory for this slot.
		 */
		slotname = &sbus_list[i][0];
		if (fflag && (strcmp(slotname, bootpath) == 0)) {
			(void) fprintf(stderr, " Ignoring %s (bootpath)\n",
					slotname);
			continue;
		}

		(void) sprintf(socal, "%s:0", &sbussoc_list[i][0]);

		if ((fd = open(socal, O_RDWR)) < 0) {
			(void) sprintf(socal, "%s:1", &sbussoc_list[i][0]);
			if ((fd = open(socal, O_RDWR)) < 0) {
				(void) fprintf(stderr,
					MSGSTR(-1, "Could not open %s\n"),
					&sbussoc_list[i][0]);
				(void) fprintf(stderr,
					MSGSTR(-1, "Ignoring %s\n"),
					&sbussoc_list[i][0]);
				continue;
			}
		}

		if (verbose)
			(void) fprintf(stdout, "  Opening %s\n", slotname);

		fd = open(slotname, O_RDWR);

		if (fd < 0) {
			perror("open of slotname");
			continue;
		}

		/*
		 * Mmap that SBus memory into my memory space.
		 */
		addr = mmap((caddr_t)0, PROM_SIZ_ROUNDED, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);

		if (addr == MAP_FAILED) {
			perror("mmap1");
			(void) close(fd);
			continue;
		}

		if ((int)addr == -1) {
			perror("mmap");
			(void) close(fd);
			continue;
		}

		regs = (socal_reg_t *)((int)addr + REG_OFFSET);

		(void) fprintf(stdout,
		MSGSTR(-1, "  Device: %s\n"), &sbussoc_list[i][0]);
		/*
		 * Load the New FCode
		 */
		if (fflag) {
			if (!warn())
				load_file(file, addr, regs);
		} else if (vflag) {
			if (findversion(i, (u_char *)&version[0]) == FOUND) {
				(void) fprintf(stdout,
				MSGSTR(-1, "  Detected FC100/S Version: %s\n"),
					version);
			}
		}

		if (munmap(addr, PROM_SIZ) == -1) {
			perror("munmap");
		}

		(void) close(fd);

	}
	(void) fprintf(stdout, MSGSTR(-1, "  Complete\n"));
}

static int
findversion(int index, u_char *version)
/*ARGSUSED*/
{
	int fd;
	struct socal_fm_version	*buffer;
	char	socal[MAXNAMELEN];
	char	prom_ver[100];
	char	mcode_ver[100];

	(void) sprintf(socal, "%s:0", &sbussoc_list[index][0]);

	if ((fd = open(socal, O_RDWR)) < 0) {
		(void) sprintf(socal, "%s:1", &sbussoc_list[index][0]);
		if ((fd = open(socal, O_RDWR)) < 0) {
			(void) fprintf(stderr, "Could not open %s\n",
						&sbussoc_list[index][0]);
			return (NOT_FOUND);
		}
	}

	if ((buffer = (struct socal_fm_version *)malloc(
				sizeof (struct socal_fm_version))) == NULL) {
		(void) fprintf(stderr, "malloc failed \n");
		return (NOT_FOUND);
	}

	buffer->fcode_ver = (char *)version;
	buffer->mcode_ver = mcode_ver;
	buffer->prom_ver = prom_ver;
	buffer->fcode_ver_len = MAXNAMELEN - 1;
	buffer->mcode_ver_len = 100;
	buffer->prom_ver_len = 100;

	if (ioctl(fd, FCIO_FCODE_MCODE_VERSION, buffer) < 0) {
		(void) fprintf(stderr,
			"fcal_s_download: could not get fcode version.\n");
		return (NOT_FOUND);
	}

	version[buffer->fcode_ver_len] = '\0';

	free(buffer);

	return (FOUND);
}
/*
 * program an FEprom with data from 'source_address'.
 *	program the FEprom with zeroes,
 *	erase it,
 *	program it with the real data.
 */
static int
feprom_program(u_char *source_address, u_char *dest_address,
	volatile socal_reg_t *regs)
{
	int i;

	(void) fprintf(stdout, "Filling with zeroes...\n");
	if (!write_feprom((u_char *)0, dest_address, regs)) {
		(void) fprintf(stderr, "FEprom at 0x%x: zero fill failed\n",
					(int)dest_address);
		return (0);
	}

	(void) fprintf(stdout, "Erasing...\n");
	for (i = 0; i < FEPROM_MAX_ERASE; i++) {
		if (feprom_erase(dest_address, regs))
			break;
	}

	if (i >= FEPROM_MAX_ERASE) {
		(void) fprintf(stderr, "FEprom at 0x%x: failed to erase\n",
			(int)dest_address);
		return (0);
	} else if (i > 0) {
		(void) fprintf(stderr, "FEprom erased after %d attempt%s\n",
			i, (i == 1 ? "" : "s"));
	}

	(void) fprintf(stdout, "Programming...\n");
	if (!(write_feprom(source_address, dest_address, regs))) {
		(void) fprintf(stderr, "FEprom at 0x%x: write failed\n",
				(int)dest_address);
		return (0);
	}

	/* select the zeroth bank at end so we can read it */
	regs->socal_cr.w &= ~(0x30000);
	(void) fprintf(stdout, "Programming done\n");
	return (1);
}

/*
 * program an FEprom one byte at a time using hot electron injection.
 */
static int
write_feprom(u_char *source_address, u_char *dest_address,
	volatile socal_reg_t *regs)
{
	int pulse, i;
	u_char *s = source_address;
	volatile u_char *d;

	for (i = 0; i < FEPROM_SIZE; i++, s++) {

		if ((i & 0xffff) == 0) {
			(void) fprintf(stdout, "selecting bank %d\n", i>>16);
			regs->socal_cr.w &= ~(0x30000);
			regs->socal_cr.w |= i & 0x30000;
		}

		d = dest_address + (i & 0xffff);

		for (pulse = 0; pulse < FEPROM_MAX_PROGRAM; pulse++) {
			*d = FEPROM_PROGRAM;
			*d = source_address ? *s : 0;
			usec_delay(50);
			*d = FEPROM_PROGRAM_VERIFY;
			usec_delay(30);
			if (*d == (source_address ? *s : 0))
					break;
		}

		if (pulse >= FEPROM_MAX_PROGRAM) {
			*dest_address = FEPROM_RESET;
			return (0);
		}
	}

	*dest_address = FEPROM_RESET;
	return (1);
}

/*
 * erase an FEprom using Fowler-Nordheim tunneling.
 */
static int
feprom_erase(volatile u_char *dest_address, volatile socal_reg_t *regs)
{
	int i;
	volatile u_char *d = dest_address;

	*d = FEPROM_ERASE;
	usec_delay(50);
	*d = FEPROM_ERASE;

	usec_delay(10000); /* wait 10ms while FEprom erases */

	for (i = 0; i < FEPROM_SIZE; i++) {

		if ((i & 0xffff) == 0) {
			regs->socal_cr.w &= ~(0x30000);
			regs->socal_cr.w |= i & 0x30000;
		}

		d = dest_address + (i & 0xffff);

		*d = FEPROM_ERASE_VERIFY;
		usec_delay(50);
		if (*d != 0xff) {
			*dest_address = FEPROM_RESET;
			return (0);
		}
	}
	*dest_address = FEPROM_RESET;
	return (1);
}

static void
usec_delay(int s)
{
	hrtime_t now, then;

	now = gethrtime();
	then = now + s*1000;
	do {
		now = gethrtime();
	} while (now < then);
}

static u_int
getsbuslist(void)
{
	int len, fgret, k = 0;
	char *sp, *sp1, cmd[MAXNAMELEN], file[100];
	FILE *ifile;

	(void) sprintf(file, "/tmp/lux.%d", (int)getpid());
	(void) sprintf(cmd,
		"find /devices -name SUNW,socal* -print > %s 2>&1", file);
	if (system(cmd) != 0) {
		goto end;
	}

	/*
	 * get FILE structure for pipe used for reading
	 */
	if ((ifile = fopen(file, "r")) == NULL) {
		perror("fopen");
		goto end;
	}

	for (;;) {
		char	buffer[100];

		fgret = fscanf(ifile, "%s", sbussoc_list[k]);
		if (fgret == (int)EOF)
			break;
		else if (fgret == NULL)
			break;
		if (strstr(sbussoc_list[k], ONBOARD_SOCAL))
			continue;
		if ((sp = strstr(sbussoc_list[k], soc_name)) == NULL)
			continue;
		sp1 = sp;
		/* Need to avoid the nodes for the individual ports */
		while ((*sp1 != '\0') && (*sp1 != '/')) {
			if (*sp1 == ':')
				break;
			sp1++;
		}
		if (*sp1 == ':')
			continue;
		len = strlen(sbussoc_list[k]) - strlen(sp);
		(void) strncpy(buffer, sbussoc_list[k], len);
		buffer[len] = '\0';
		sp += strlen("SUNW,socal@");
		(void) sprintf(sbus_list[k], "%ssbusmem@%c,0:slot%c",
				buffer, sp[0], sp[0]);
		k++;
	}

end:
	(void) unlink(file);
	return (k);
}

static void
getbootdev(unsigned int verbose)
{
	char *df = "df /";
	FILE *ptr;
	char *p, *p1;
	char bootdev[PATH_MAX];
	char buf[BUFSIZ];
	int foundroot = 0;


	if ((ptr = popen(df, "r")) != NULL) {
		while (fgets(buf, BUFSIZ, ptr) != NULL) {
			if (p = strstr(buf, "/dev/dsk/")) {
				(void) memset((char *)&bootdev[0], 0,
					PATH_MAX);
				p1 = p;
				while (*p1 != '\0') {
					if (!isalnum(*p1) && (*p1 != '/'))
						*p1 = ' ';
					p1++;
				}
				(void) sscanf(p, "%s", bootdev);
				foundroot = 1;
			}
		}
		if (!foundroot) {
			if (verbose)
				(void) fprintf(stderr, "root is not on a "
						"local disk!\n");
			(void) memset((char *)&bootpath[0], 0, PATH_MAX);
			return;
		}
		(void) pclose(ptr);
		if (bootdev[0]) {
			char *ls;
			char *p1;
			char *p2 = NULL;
			char *sbusmem = "/sbusmem@";
			char *slot = ",0:slot";

			ls = (char *)malloc(PATH_MAX);
			(void) memset((char *)ls, NULL, PATH_MAX);
			(void) strcpy(ls, "ls -l ");
			(void) strcat(ls, bootdev);
			if ((ptr = popen(ls, "r")) != NULL) {
				while (fgets(buf, BUFSIZ, ptr) != NULL) {
					if (p = strstr(buf, "/devices")) {
					    if (p1 = strstr(buf, "sbus")) {
						while (*p1 != '/')
							p1++;
						p2 = strstr(p1, "@");
						++p2;
						*p1 = '\0';
					    } else {
						if (p1 = strstr(buf,
						    soc_name)) {
							p2 = strstr(p1, "@");
							++p2;
							--p1;
							*p1 = '\0';
						}
					    }
					}
				}
				(void) pclose(ptr);
			}
			(void) memset((char *)&bootdev[0], 0, PATH_MAX);
			(void) sscanf(p, "%s", bootdev);
			(void) memset((char *)&bootpath[0], 0, PATH_MAX);
			(void) strcat(bootpath, bootdev);
			(void) strcat(bootpath, sbusmem);
			if (p2) {
				(void) strncat(bootpath, p2, 1);
				(void) strcat(bootpath, slot);
				(void) strncat(bootpath, p2, 1);
			}
		}
	}
}

/*
 * This function reads "size" bytes from the FC100/S PROM.
 * source_address: PROM address
 * dest_address:   local memeory
 * offset:         Location in PROM to start reading from.
 */
static void
feprom_read(u_char *source_address, u_char *dest_address,
		int offset, int size, volatile socal_reg_t *regs)
{
u_char  *s = source_address;
u_char  *d = dest_address;
int	i = offset;

	if (getenv("_LUX_D_DEBUG") != NULL) {
		(void) fprintf(stdout,
			"  feprom_read: selecting bank %d\n",
			(i&0xf0000)>>16);
		if (size <= 8) {
			(void) fprintf(stdout, "  Data read: ");
		}
	}
	regs->socal_cr.w = i & 0xf0000;
	s = source_address + (i & 0xffff);
	*s = FEPROM_READ_MEMORY;
	usec_delay(6);
	for (; s < source_address + (i & 0xffff) + size; d++, s++) {
		*d = *s;
		if ((getenv("_LUX_D_DEBUG") != NULL) &&
			(size <= 8)) {
			(void) fprintf(stdout, "0x%x ", *d);
		}
	}
	if ((getenv("_LUX_D_DEBUG") != NULL) &&
		(size <= 8)) {
		(void) fprintf(stdout, "\n  From offset: 0x%x\n",
			offset);
	}
}


static void
load_file(char *file, caddr_t prom, volatile socal_reg_t *regs)
{
u_int	wwn_d8, wwn_lo;
u_int	wwn_hi;
int ffd = open(file, 0);

	wwn_hi = FEPROM_SUN_WWN;

	if (ffd < 0) {
		perror("open of file");
		exit(1);
	}
	(void) fprintf(stdout, "Loading FCode: %s\n", file);

	if (read(ffd, &exec, sizeof (exec)) != sizeof (exec)) {
		perror("read exec");
		exit(1);
	}

	if (exec.a_trsize || exec.a_drsize) {
		(void) fprintf(stderr, "%s: is relocatable\n", file);
		exit(1);
	}

	if (exec.a_data || exec.a_bss) {
		(void) fprintf(stderr, "%s: has data or bss\n", file);
		exit(1);
	}

	if (exec.a_machtype != M_SPARC) {
		(void) fprintf(stderr, "%s: not for SPARC\n", file);
		exit(1);
	}

	(void) fprintf(stdout, "Loading 0x%x bytes from %s at offset 0x%x\n",
		(int)exec.a_text, file, 0);

	if (read(ffd, &buffer, exec.a_text) != exec.a_text) {
		perror("read");
		exit(1);
	}

	(void) close(ffd);

	feprom_read((u_char *)prom, (u_char *)&wwn_d8,
		FEPROM_WWN_OFFSET, 4, regs);
	feprom_read((u_char *)prom, (u_char *)&wwn_lo,
		FEPROM_WWN_OFFSET + 4, 4, regs);
	wwn_hi |= wwn_d8 & 0x0f; /* only last digit is interesting */
	if (getenv("_LUX_D_DEBUG") != NULL) {
		(void) fprintf(stdout,
			"  load_file: Writing WWN hi:0x%x lo:0x%x "
			"to the FC100/S PROM\n", wwn_hi, wwn_lo);
	}
	/* put wwn into buffer location */
	bcopy((const void *)&wwn_hi,
		(void *)&buffer[FEPROM_WWN_OFFSET],
		sizeof (wwn_hi));
	bcopy((const void *)&wwn_lo,
		(void *)&buffer[FEPROM_WWN_OFFSET + 4],
		sizeof (wwn_lo));
	bcopy((const void *)&wwn_hi,
		(void *)&buffer[FEPROM_WWN_OFFSET + 8],
		sizeof (wwn_hi));
	bcopy((const void *)&wwn_lo,
		(void *)&buffer[FEPROM_WWN_OFFSET + 0xc],
		sizeof (wwn_lo));

	(void) feprom_program((u_char *)buffer, (u_char *)prom, regs);
}

static int
warn(void)
{
	char input[1024];

	input[0] = '\0';

	(void) fprintf(stderr, "%s", warnstring);
	(void) fprintf(stderr, "%s", warnstring1);
loop1:
	(void) fprintf(stderr, "%s", warnstring2);

	(void) gets(input);

	if ((strcmp(input, "y") == 0) || (strcmp(input, "yes") == 0)) {
		return (FOUND);
	} else if ((strcmp(input, "n") == 0) || (strcmp(input, "no") == 0)) {
		(void) fprintf(stderr, "Not Downloading FCode\n");
		return (NOT_FOUND);
	} else {
		(void) fprintf(stderr, "Invalid input\n");
		goto loop1;
	}
}

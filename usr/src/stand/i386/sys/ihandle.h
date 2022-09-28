/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _I386_SYS_IHANDLE_H
#define	_I386_SYS_IHANDLE_H

#pragma ident	"@(#)ihandle.h	1.10	97/08/11 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	DEVT_DSK 0x13	/* .. .. Disk device	*/
#define	DEVT_SER 0x14	/* .. .. Pseudo-Serial device	*/
#define	DEVT_NET 0xFB 	/* .. .. Network device		*/

struct ihandle			/* I/O device descriptor */
{
	unsigned char    type;		/* .. Device type code:	*/

	unsigned char    unit;		/* .. Device number	*/
	unsigned short   usecnt;    /* .. Device open count	*/

	char    fstype[8];	/* .. File system type code	*/
	char	*pathnm;    /* .. Device name pointer		*/
	caddr_t cachep;	/* .. Pointer to cache area		*/
	unsigned long    cfirst;	/* .. First block in cache	*/

	union {				/* Device dependent section:	*/
		struct {			/* .. Disks:	*/
			void *alt; /* .. .. Alternate sector info	*/
			long  siz;	/* .. .. Partition size		*/
			daddr_t  par;	/* .. .. Partition offset	*/
			unsigned int   cyl; /* .. .. Number of cylinders */
			unsigned long  bas; /* .. .. Base sector	*/
			unsigned short bps; /* .. .. Bytes per sector */
			unsigned short spt; /* .. .. Sectors per track */
			unsigned short spc; /* .. .. Sectors per cylinder */
			unsigned long  csize; /* .. .. Device cache size */
			short num;	/* .. .. Partition (slice) no.	*/
			/*
			 * We set flop_change_line to 1 if the device is
			 * a floppy AND has change line read capability
			 */
			short flop_change_line;
		} disk;
	} dev;
};

#define	MAXDEVOPENS	6	/* Max number of open devices		*/

extern struct ihandle *open_devices[];
#define	devp(fd) (open_devices[fd])

extern int find_net(int *, int, char *);
extern int open_net(struct ihandle *);
extern void close_net(struct ihandle *);
extern int read_net(struct ihandle *, caddr_t, u_int, u_int);
extern int write_net(struct ihandle *, caddr_t, u_int, u_int);

extern int find_disk(int *, int, char *);
extern int open_disk(struct ihandle *);
extern void close_disk(struct ihandle *);
extern int read_disk(struct ihandle *, caddr_t, u_int, u_int);
extern int write_disk(struct ihandle *, caddr_t, u_int, u_int);

#ifdef	__cplusplus
}
#endif

#endif /* _I386_SYS_IHANDLE_H */

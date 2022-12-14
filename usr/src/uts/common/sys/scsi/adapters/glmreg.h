/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SCSI_ADAPTERS_GLMREG_H
#define	_SYS_SCSI_ADAPTERS_GLMREG_H

#pragma ident	"@(#)glmreg.h	1.20	97/12/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

enum glm53c8xxregs {		/* To access NCR 53C8xx registers */
NREG_SCNTL0 =	0x00,	NREG_SCNTL1,	NREG_SCNTL2,	NREG_SCNTL3,
NREG_SCID,		NREG_SXFER,	NREG_SDID,	NREG_GPREG,
NREG_SFBR,		NREG_SOCL,	NREG_SSID,	NREG_SBCL,
NREG_DSTAT,		NREG_SSTAT0,	NREG_SSTAT1,	NREG_SSTAT2,
NREG_DSA,
NREG_ISTAT =	0x14,
NREG_CTEST0 =	0x18,	NREG_CTEST1,	NREG_CTEST2,	NREG_CTEST3,
NREG_TEMP,
NREG_DFIFO =	0x20,	NREG_CTEST4,	NREG_CTEST5,	NREG_CTEST6,
NREG_DBC,						NREG_DCMD = 0x27,
NREG_DNAD,
NREG_DSP =	0x2c,
NREG_DSPS =	0x30,
NREG_SCRATCHA =	0x34,
NREG_SCRATCHA0 = 0x34,	NREG_SCRATCHA1,	NREG_SCRATCHA2, NREG_SCRATCHA3,
NREG_DMODE,		NREG_DIEN,	NREG_DWT,	NREG_DCNTL,
NREG_ADDER,

NREG_SIEN0 =	0x40,	NREG_SIEN1,	NREG_SIST0,	NREG_SIST1,
NREG_SLPAR,		NREG_RESERVED,	NREG_MACNTL,	NREG_GPCNTL,
NREG_STIME0,		NREG_STIME1,	NREG_RESPID,
NREG_STEST0 = 0x4c, 	NREG_STEST1,	NREG_STEST2,	NREG_STEST3,
NREG_SIDL,
NREG_SODL = 0x54,
NREG_SBDL = 0x58,
NREG_SCRATCHB = 0x5c,
NREG_SCRATCHB0 = 0x5c,	NREG_SCRATCHB1,	NREG_SCRATCHB2, NREG_SCRATCHB3
};

/*
 * These bits are used to decode DMA/chip errors.
 */
#define	dstatbits	\
"\020\010DMA-FIFO-empty\
\07master-data-parity-error\
\06bus-fault\
\05aborted\
\04single-step-interrupt\
\03SCRIPTS-interrupt-instruction\
\02reserved\
\01illegal-instruction"

/*
 * Device ids.
 */
#define	GLM_53c810	0x1
#define	GLM_53c825	0x3
#define	GLM_53c875	0xf

/*
 * Revisons.
 */
#define	REV1	0x1
#define	REV2	0x2
#define	REV3	0x3
#define	REV4	0x4
#define	GLM_REV(glm)	(uchar_t)(glm->g_revid & 0xf)

/*
 * default hostid.
 */
#define	DEFAULT_HOSTID	7

/*
 * Default Synchronous offset.
 * (max # of allowable outstanding REQ)
 */
#define	GLM_875_OFFSET	16
#define	GLM_825_OFFSET	8
#define	GLM_810_OFFSET	8

#define	SYNC_OFFSET(glm) \
	(glm->g_devid == GLM_53c810 ? 8 : 16)

/*
 * Sync periods.
 */
#define	DEFAULT_SYNC_PERIOD		200	/* 5.0 MB/s */
#define	DEFAULT_FASTSYNC_PERIOD		100	/* 10.0 MB/s */
#define	DEFAULT_FAST20SYNC_PERIOD	50	/* 20.0 MB/s */

/*
 * This yields nanoseconds per input clock tick
 */
#define	CONVERT_PERIOD(time)	(((time)<<2)/10)

/*
 * Max/Min number of clock cycles for synchronous period
 */
#define	MAX_TP			11
#define	MIN_TP			4
#define	MAX_CLOCK_DIVISOR	3
#define	MIN_CLOCK_DIVISOR	1

#define	MAX_SYNC_PERIOD(glm) \
	((glm->g_speriod * MAX_CLOCK_DIVISOR * MAX_TP) / 10)
#define	MIN_SYNC_PERIOD(glm) \
	((glm->g_speriod * MIN_CLOCK_DIVISOR * MIN_TP) / 10)

/*
 * Config space.
 */
#define	GLM_LATENCY_TIMER	0x40

/*
 * defines for onboard 4k SRAM
 */
#define	GLM_HBA_DSA_ADDR_OFFSET	0xffc

/*
 * Bit definitions for the ISTAT (Interrupt Status) register
 */
#define	NB_ISTAT_ABRT		0x80	/* abort operation */
#define	NB_ISTAT_SRST		0x40	/* software reset */
#define	NB_ISTAT_SEMA		0x10	/* Semaphore bit */
#define	NB_ISTAT_SIGP		0x20	/* signal process */

/*
 * Bit definitions for the DSTAT (DMA Status) register
 */
#define	NB_DSTAT_DFE	0x80	/* DMA FIFO empty */
#define	NB_DSTAT_MDPE	0x40	/* master data parity error */
#define	NB_DSTAT_BF	0x20	/* bus fault */
#define	NB_DSTAT_ABRT	0x10	/* aborted */
#define	NB_DSTAT_SSI 	0x08	/* SCRIPT step interrupt */
#define	NB_DSTAT_SIR	0x04	/* SCRIPT interrupt instruction */
#define	NB_DSTAT_RES	0x02	/* reserved */
#define	NB_DSTAT_IID	0x01	/* illegal instruction detected */

/*
 * Just the unexpected fatal DSTAT errors
*/
#define	NB_DSTAT_ERRORS		(NB_DSTAT_MDPE | NB_DSTAT_BF | NB_DSTAT_ABRT \
				| NB_DSTAT_SSI | NB_DSTAT_IID)

/*
 * Bit definitions for the SIST0 (SCSI Interrupt Status Zero) register
 */
#define	NB_SIST0_MA	0x80	/* initiator: Phase Mismatch, or */
				/* target: ATN/ active */
#define	NB_SIST0_CMP	0x40	/* function complete */
#define	NB_SIST0_SEL	0x20	/* selected */
#define	NB_SIST0_RSL	0x10	/* reselected */
#define	NB_SIST0_SGE	0x08	/* SCSI gross error */
#define	NB_SIST0_UDC	0x04	/* unexpected disconnect */
#define	NB_SIST0_RST	0x02	/* SCSI RST/ (reset) received */
#define	NB_SIST0_PAR	0x01	/* parity error */

/*
 * Bit definitions for the SIST1 (SCSI Interrupt Status One) register
 */
#define	NB_SIST1_STO	0x04	/* selection or reselection time-out */
#define	NB_SIST1_GEN	0x02	/* general purpose timer expired */
#define	NB_SIST1_HTH	0x01	/* handshake-to-handshake timer expired */

/*
 * Miscellaneous other bits that have to be fiddled
 */
#define	NB_SCNTL0_EPC		0x08	/* enable parity checking */
#define	NB_SCNTL0_AAP		0x02	/* Assert ATN on Parity error */

#define	NB_SCNTL1_CON		0x10	/* connected */
#define	NB_SCNTL1_RST		0x08	/* assert scsi reset signal */

#define	NB_SCID_RRE		0x40	/* enable response to reselection */
#define	NB_SCID_ENC		0x0f	/* encoded chip scsi id */

#define	NB_GPREG_GPIO3		0x08	/* low if differential board. */

#define	NB_SSID_VAL		0x80	/* scsi id valid bit */
#define	NB_SSID_ENCID		0x0f	/* encoded destination scsi id */

#define	NB_SSTAT0_ILF		0x80	/* scsi input data latch full */
#define	NB_SSTAT0_ORF		0x40	/* scsi output data register full */
#define	NB_SSTAT0_OLF		0x20	/* scsi output data latch full */

#define	NB_SSTAT2_ILF1		0x80	/* scsi input data latch1 full. */
#define	NB_SSTAT2_ORF1		0x40	/* scsi output data register1 full */
#define	NB_SSTAT2_OLF1		0x20	/* scsi output data latch1 full */

#define	NB_SSTAT1_FF		0xf0	/* scsi fifo flags */
#define	NB_SSTAT1_PHASE		0x07	/* current scsi phase */

#define	NB_CTEST2_DDIR		0x80	/* data transfer direction */
#define	NB_CTEST2_DIF		0x20	/* SCSI Differential Mode */

#define	NB_CTEST3_VMASK		0xf0	/* chip revision level */
#define	NB_CTEST3_VERSION	0x10	/* expected chip revision level */
#define	NB_CTEST3_CLF		0x04	/* clear dma fifo */

#define	NB_CTEST4_MPEE		0x08	/* master parity error enable */

#define	NB_CTEST5_DFS		0x20	/* Sets dma fifo size to 536 bytes. */
#define	NB_CTEST5_BL2		0x04	/* Used w/DMODE reg for burst size. */

#define	NB_DIEN_MDPE		0x40	/* master data parity error */
#define	NB_DIEN_BF		0x20	/* bus fault */
#define	NB_DIEN_ABRT		0x10	/* aborted */
#define	NB_DIEN_SSI		0x08	/* SCRIPT step interrupt */
#define	NB_DIEN_SIR		0x04	/* SCRIPT interrupt instruction */
#define	NB_DIEN_IID		0x01	/* Illegal instruction detected */

#define	NB_DCNTL_COM		0x01	/* 53c700 compatibility */

#define	NB_SIEN0_MA		0x80	/* Initiator: Phase Mismatch, or */
					/* Target: ATN/ active */
#define	NB_SIEN0_CMP		0x40	/* function complete */
#define	NB_SIEN0_SEL		0x20	/* selected */
#define	NB_SIEN0_RSL		0x10	/* reselected */
#define	NB_SIEN0_SGE		0x08	/* SCSI gross error */
#define	NB_SIEN0_UDC		0x04	/* unexpected disconnect */
#define	NB_SIEN0_RST		0x02	/* SCSI reset condition */
#define	NB_SIEN0_PAR		0x01	/* SCSI parity error */

#define	NB_SIEN1_STO		0x04	/* selection or reselection time-out */
#define	NB_SIEN1_GEN		0x02	/* general purpose timer expired */
#define	NB_SIEN1_HTH		0x01	/* handshake-to-handshake timer */
					/* expired */
#define	NB_STIME0_SEL		0x0f	/* selection time-out bits */
#define	NB_STIME0_204		0x0c	/* 204.8 ms */
#define	NB_STIME0_409		0x0d	/* 409.6 ms */

#define	NB_STEST3_CSF		0x02	/* clear SCSI FIFO */
#define	NB_STEST3_DSI		0x10	/* disable single initiator response */
#define	NB_STEST3_HSC		0x20	/* Halt SCSI Clock */
#define	NB_STEST3_TE		0x80	/* tolerANT enable */

#define	NB_SCNTL1_EXC		0x80	/* extra clock cycle of data setup */

#define	NB_SCNTL3_FAST20	0x80	/* FAST-20 Enabled. */
#define	NB_SCNTL3_SCF		0x70	/* synch. clock conversion factor */
#define	NB_SCNTL3_SCF1		0x10	/* SCLK / 1 */
#define	NB_SCNTL3_SCF15		0x20	/* SCLK / 1.5 */
#define	NB_SCNTL3_SCF2		0x30	/* SCLK / 2 */
#define	NB_SCNTL3_SCF3		0x00	/* SCLK / 3 */
#define	NB_SCNTL3_SCF4		0x05	/* SCLK / 4 */
#define	NB_SCNTL3_CCF		0x07	/* clock conversion factor */
#define	NB_SCNTL3_CCF1		0x01	/* SCLK / 1 */
#define	NB_SCNTL3_CCF15		0x02	/* SCLK / 1.5 */
#define	NB_SCNTL3_CCF2		0x03	/* SCLK / 2 */
#define	NB_SCNTL3_CCF3		0x00	/* SCLK / 3 */
#define	NB_SCNTL3_CCF4		0x05	/* SCLK / 4 */
#define	NB_SCNTL3_EWS		0x08	/* Enable wide scsi bit. */

#define	NB_CTEST4_BDIS		0x80	/* burst disable */

#define	NB_DMODE_BOF		0x02	/* Burst Op Code Fetch Enable */
#define	NB_DMODE_ERL		0x08	/* Enable Read Line */
#define	NB_DMODE_BL		0x40	/* burst length */
#define	NB_825_DMODE_BL		0xc0	/* burst length for 53c825 */

#define	NB_DCNTL_IRQM		0x08	/* IRQ mode */
#define	NB_DCNTL_CLSE		0x80	/* Cache Line Size Enable mode */
#define	NB_DCNTL_PFEN		0x20	/* Prefetch enable mode */

#define	NB_STEST1_SCLK		0x80	/* disable external SCSI clock */
#define	NB_STEST1_DBLEN		0x08	/* SCLK Double Enable */
#define	NB_STEST1_DBLSEL	0x04	/* SCLK Double Select */

#define	NB_STEST2_EXT		0x02	/* extend SREQ/SACK filtering */
#define	NB_STEST2_ROF		0x40	/* Reset SCSI Offset */


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCSI_ADAPTERS_GLMREG_H */

/*
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)glm.c	1.104	97/12/18 SMI"

/*
 * glm - Symbios 53c810a, 53c825 and 53c875 SCSI Processor HBA driver.
 */
#if defined(lint) && !defined(DEBUG)
#define	DEBUG 1
#define	GLM_DEBUG
#endif

/*
 * standard header files.
 */
#include <sys/note.h>
#include <sys/scsi/scsi.h>
#include <sys/pci.h>
#include <sys/file.h>

/*
 * private header files.
 */
#include <sys/scsi/adapters/glmvar.h>
#include <sys/scsi/adapters/glmreg.h>
#include <sys/scsi/adapters/reset_notify.h>

/*
 * autoconfiguration data and routines.
 */
static int glm_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
    void **result);
static int glm_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int glm_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);

static void glm_table_init(glm_t *glm, glm_unit_t *unit);
static void glm_tear_down_unit_dsa(struct glm *glm, int targ, int lun);
static int glm_create_unit_dsa(struct glm *glm, int targ, int lun);

static int glm_config_space_init(struct glm *glm);
static int glm_hba_init(glm_t *glm);
static void glm_hba_fini(glm_t *glm);
static int glm_script_alloc(glm_t *glm);
static void glm_script_free(struct glm *glm);
static void glm_cfg_fini(glm_t *glm_blkp);
static int glm_script_offset(int func);
static int glm_memory_script_init(glm_t *glm);

/*
 * SCSA function prototypes with some helper functions for DMA.
 */
static int glm_scsi_start(struct scsi_address *ap, struct scsi_pkt *pkt);
static int glm_scsi_reset(struct scsi_address *ap, int level);
static int glm_scsi_abort(struct scsi_address *ap, struct scsi_pkt *pkt);
static int glm_capchk(char *cap, int tgtonly, int *cidxp);
static int glm_scsi_getcap(struct scsi_address *ap, char *cap, int tgtonly);
static int glm_scsi_setcap(struct scsi_address *ap, char *cap, int value,
    int tgtonly);
static void glm_scsi_dmafree(struct scsi_address *ap, struct scsi_pkt *pktp);
static struct scsi_pkt *glm_scsi_init_pkt(struct scsi_address *ap,
    struct scsi_pkt *pkt, struct buf *bp, int cmdlen, int statuslen,
	int tgtlen, int flags, int (*callback)(), caddr_t arg);

static void glm_scsi_sync_pkt(struct scsi_address *ap, struct scsi_pkt *pktp);
static void glm_scsi_destroy_pkt(struct scsi_address *ap,
    struct scsi_pkt *pkt);
static int glm_scsi_tgt_init(dev_info_t *hba_dip, dev_info_t *tgt_dip,
    scsi_hba_tran_t *hba_tran, struct scsi_device *sd);
static int glm_scsi_tgt_probe(struct scsi_device *sd,
    int (*callback)());
static void glm_scsi_tgt_free(dev_info_t *hba_dip, dev_info_t *tgt_dip,
    scsi_hba_tran_t *hba_tran, struct scsi_device *sd);
static int glm_scsi_reset_notify(struct scsi_address *ap, int flag,
    void (*callback)(caddr_t), caddr_t arg);

/*
 * internal function prototypes.
 */
static int glm_dr_detach(dev_info_t *dev);
static glm_unit_t *glm_unit_init(glm_t *glm, int target, int lun);

static int glm_accept_pkt(struct glm *glm, struct glm_scsi_cmd *sp);
static int glm_prepare_pkt(struct glm *glm, struct glm_scsi_cmd *cmd);
static void glm_sg_update(glm_unit_t *unit, uchar_t index, ulong_t remain);
static ulong_t glm_sg_residual(glm_unit_t *unit, struct glm_scsi_cmd *cmd);
static void glm_queue_pkt(glm_t *glm, glm_unit_t *unit, ncmd_t *cmd);
static int glm_send_abort_msg(struct scsi_address *ap, glm_t *glm,
    glm_unit_t *unit);
static int glm_send_dev_reset_msg(struct scsi_address *ap, glm_t *glm);
static int glm_do_scsi_reset(struct scsi_address *ap, int level);
static int glm_do_scsi_abort(struct scsi_address *ap, struct scsi_pkt *pkt);
static int glm_abort_cmd(struct glm *glm, struct glm_unit *unit,
    struct glm_scsi_cmd *cmd);
static void glm_chkstatus(glm_t *glm, glm_unit_t *unit,
    struct glm_scsi_cmd *cmd);
static void glm_handle_qfull(struct glm *glm, struct glm_scsi_cmd *cmd);
static void glm_restart_cmd(void *);
static void glm_remove_cmd(struct glm *glm, struct glm_unit *unit,
    struct glm_scsi_cmd *cmd);
static void glm_pollret(glm_t *glm, ncmd_t *poll_cmdp);
static void glm_flush_hba(struct glm *glm);
static void glm_flush_target(struct glm *glm, ushort target, uchar_t reason,
    uint_t stat);
static void glm_flush_lun(glm_t *glm, glm_unit_t *unit, uchar_t reason,
    uint_t stat);
static void glm_set_pkt_reason(struct glm *glm, struct glm_scsi_cmd *cmd,
    uchar_t reason, uint_t stat);
static void glm_mark_packets(struct glm *glm, struct glm_unit *unit,
    u_char reason, u_int stat);
static void glm_flush_waitQ(struct glm *glm, struct glm_unit *unit);
static void glm_flush_tagQ(struct glm *glm, struct glm_unit *unit);
static void glm_process_intr(glm_t *glm, uchar_t istat);
static ulong_t glm_decide(glm_t *glm, ulong_t action);
static ulong_t glm_ccb_decide(glm_t *glm, glm_unit_t *unit, ulong_t action);
static int glm_wait_intr(glm_t *glm);

static int glm_pkt_alloc_extern(glm_t *glm, ncmd_t *cmd,
    int cmdlen, int tgtlen, int statuslen, int kf);
static void glm_pkt_destroy_extern(glm_t *glm, ncmd_t *cmd);

static void glm_watch(void *arg);
static void glm_watchsubr(register struct glm *glm);
static void glm_cmd_timeout(struct glm *glm, struct glm_unit *unit);
static void glm_sync_wide_backoff(struct glm *glm, struct glm_unit *unit);
static void glm_force_renegotiation(struct glm *glm, int target);

static int glm_kmem_cache_constructor(void *buf, void *cdrarg, int kmflags);
static void glm_kmem_cache_destructor(void *buf, void *cdrarg);

static uint_t glm_intr(caddr_t arg);
static void glm_start_next(struct glm *glm);
static int glm_start_cmd(struct glm *glm, glm_unit_t *unit,
	struct glm_scsi_cmd *cmd);
static void glm_wait_for_reselect(glm_t *glm, ulong_t action);
static void glm_restart_current(glm_t *glm, ulong_t action);
static void glm_restart_hba(glm_t *glm, ulong_t action);
static void glm_queue_target(glm_t *glm, glm_unit_t *unit);
static void glm_queue_target_lun(glm_t *glm, ushort_t target);
static glm_unit_t *glm_get_target(glm_t *glm);
static ulong_t glm_check_intcode(glm_t *glm, glm_unit_t *unit, ulong_t action);
static ulong_t glm_parity_check(struct glm *glm, struct glm_unit *unit);
static void glm_addfq(glm_t	*glm, glm_unit_t *unit);
static void glm_addbq(glm_t	*glm, glm_unit_t *unit);
static void glm_doneq_add(glm_t *glm, ncmd_t *cmdp);
static ncmd_t *glm_doneq_rm(glm_t *glm);
static void glm_doneq_empty(glm_t *glm);
static void glm_waitq_add(glm_unit_t *unit, ncmd_t *cmdp);
static void glm_waitq_add_lifo(glm_unit_t *unit, ncmd_t *cmdp);
static ncmd_t *glm_waitq_rm(glm_unit_t *unit);
static void glm_waitq_delete(glm_unit_t *unit, ncmd_t *cmdp);

static void glm_syncio_state(glm_t *glm, glm_unit_t *unit, uchar_t state,
    uchar_t sxfer, uchar_t sscfX10);
static void glm_syncio_disable(glm_t *glm);
static void glm_syncio_reset_target(glm_t *glm, int target);
static void glm_syncio_reset(glm_t *glm, glm_unit_t *unit);
static void glm_syncio_msg_init(glm_t *glm, glm_unit_t *unit);
static int glm_syncio_enable(glm_t *glm, glm_unit_t *unit);
static int glm_syncio_respond(glm_t *glm, glm_unit_t *unit);
static ulong_t glm_syncio_decide(glm_t *glm, glm_unit_t *unit, ulong_t action);
static void glm_start_watch_reset_delay();
static void glm_setup_bus_reset_delay(struct glm *glm);
static void glm_watch_reset_delay(void *arg);
static int glm_watch_reset_delay_subr(register struct glm *glm);

static int glm_max_sync_divisor(glm_t *glm, int syncioperiod,
    uchar_t *sxferp, uchar_t *sscfX10p);
static int glm_period_round(glm_t *glm, int syncioperiod);
static void glm_max_sync_rate_init(glm_t *glm);

static int glm_create_arq_pkt(struct glm_unit *unit, struct scsi_address *ap);
static int glm_delete_arq_pkt(struct glm_unit *unit, struct scsi_address *ap);
static void glm_complete_arq_pkt(struct scsi_pkt *pkt);
static int glm_handle_sts_chk(struct glm *glm, struct glm_unit *unit,
    struct glm_scsi_cmd *sp);

static void glm_set_throttles(struct glm *glm, int slot, int n, int what);
static void glm_set_all_lun_throttles(struct glm *glm, int target, int what);
static void glm_full_throttle(struct glm *glm, int target, int lun);

static void glm_make_wdtr(struct glm *glm, struct glm_unit *unit, uchar_t wide);
static void glm_set_wide_scntl3(struct glm *glm, struct glm_unit *unit,
    uchar_t width);

static void glm_update_props(struct glm *glm, int tgt);
static void glm_update_this_prop(struct glm *glm, char *property, int value);
static int glm_alloc_active_slots(struct glm *glm, struct glm_unit *unit,
    int flag);

static void glm_dump_cmd(struct glm *glm, struct glm_scsi_cmd *cmd);
static void glm_log(struct glm *glm, int level, char *fmt, ...);
#ifdef GLM_DEBUG
static void glm_printf(char *fmt, ...);
#endif

static void	glm53c87x_reset(glm_t *glm);
static void	glm53c87x_init(glm_t *glm);
static void	glm53c87x_enable(glm_t *glm);
static void	glm53c87x_disable(glm_t *glm);
static uchar_t	glm53c87x_get_istat(glm_t *glm);
static void	glm53c87x_halt(glm_t *glm);
static void	glm53c87x_check_error(glm_unit_t *unit, struct scsi_pkt *pktp);
static ulong_t	glm53c87x_dma_status(glm_t *glm);
static ulong_t	glm53c87x_scsi_status(glm_t *glm);
static int	glm53c87x_save_byte_count(glm_t *glm, glm_unit_t *unit);
static int	glm53c87x_get_target(struct glm *glm, uchar_t *tp);
static void	glm53c87x_set_syncio(glm_t *glm, glm_unit_t *unit);
static void	glm53c87x_setup_script(glm_t *glm, glm_unit_t *unit);
static void	glm53c87x_bus_reset(glm_t *glm);


static ddi_dma_attr_t glm_dma_attrs = {
	DMA_ATTR_V0,	/* attribute layout version		*/
	0x0ull,		/* address low - should be 0 (longlong)	*/
	0xffffffffull,	/* address high - 32-bit max range	*/
	0x00ffffffull,	/* count max - max DMA object size	*/
	4,		/* allocation alignment requirements	*/
	0x78,		/* burstsizes - binary encoded values	*/
	1,		/* minxfer - gran. of DMA engine	*/
	0x00ffffffull,	/* maxxfer - gran. of DMA engine	*/
	0xffffffffull,	/* max segment size (DMA boundary)	*/
	GLM_MAX_DMA_SEGS, /* scatter/gather list length		*/
	512,		/* granularity - device transfer size	*/
	0		/* flags, set to 0			*/
};

static ddi_device_acc_attr_t dev_attr = {
	DDI_DEVICE_ATTR_V0,
	DDI_STRUCTURE_LE_ACC,
	DDI_STRICTORDER_ACC
};

/*
 * Hotplug support
 * Leaf ops (hotplug controls for target devices)
 */
static int glm_open(dev_t *, int, int, cred_t *);
static int glm_close(dev_t, int, int, cred_t *);
static int glm_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);

static struct cb_ops glm_cb_ops = {
	glm_open,
	glm_close,
	nodev,		/* strategy */
	nodev,		/* print */
	nodev,		/* dump */
	nodev,		/* read */
	nodev,		/* write */
	glm_ioctl,
	nodev,		/* devmap */
	nodev,		/* mmap */
	nodev,		/* segmap */
	nochpoll,	/* poll */
	ddi_prop_op,	/* prop_op */
	NULL,
	D_NEW | D_MP
};

static struct dev_ops glm_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	glm_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	glm_attach,		/* attach */
	glm_detach,		/* detach */
	nodev,			/* reset */
	&glm_cb_ops,		/* driver operations */
	NULL,			/* bus operations */
	ddi_power		/* power */
};

char _depends_on[] = "misc/scsi";

static struct modldrv modldrv = {
	&mod_driverops,	/* Type of module. This one is a driver */
	"GLM SCSI HBA Driver 1.104.",   /* Name of the module. */
	&glm_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1, &modldrv, NULL
};

/*
 * Local static data
 */
#if defined(GLM_DEBUG)
static ulong_t	glm_debug_flags = 0x0;
#endif	/* defined(GLM_DEBUG) */

static kmutex_t 	glm_global_mutex;
static int		glm_global_init = 0;
static void		*glm_state;		/* soft	state ptr */
static krwlock_t	glm_global_rwlock;
static struct glm	*glm_head, *glm_tail;

static kmutex_t		glm_log_mutex;
static char		glm_log_buf[256];
_NOTE(MUTEX_PROTECTS_DATA(glm_log_mutex, glm_log_buf))

/*
 * Use one copy of scripts for memory scripts.	Point all hba's that
 * use memory scripts to this one location.
 */
static uint_t glm_scripts[NSS_FUNCS];
static uint_t glm_do_list_end;
static uint_t glm_di_list_end;

static struct glm *glm_head, *glm_tail;
static clock_t glm_scsi_watchdog_tick;
static clock_t glm_tick;
static timeout_id_t glm_reset_watch;
static timeout_id_t glm_timeout_id;

/*
 * tunables
 */
static uchar_t	glm_default_offset = GLM_875_OFFSET;
static uint_t	glm_selection_timeout = NB_STIME0_204;

/*
 * Include the output of the NASM program. NASM is a program
 * which takes the scr.ss file and turns it into a series of
 * C data arrays and initializers.
 */
#include <sys/scsi/adapters/scr.out>

static ddi_acc_handle_t glm_script_acc_handle;
static ddi_dma_handle_t glm_script_dma_handle;

static size_t glm_script_size = sizeof (SCRIPT);

/*
 * warlock directives
 */
_NOTE(SCHEME_PROTECTS_DATA("unique per pkt", scsi_pkt \
	glm_scsi_cmd NcrTableIndirect buf))

#ifdef GLM_DEBUG
#define	GLM_TEST
static int glm_no_sync_wide_backoff;
static int glm_test_arq_enable, glm_test_arq;
static int glm_rtest, glm_rtest_type;
static int glm_atest, glm_atest_type;
static int glm_ptest;
static int glm_test_stop;
static int glm_test_instance;
static int glm_test_untagged;
static int glm_enable_untagged;
static int glm_test_timeouts;

void debug_enter(char *);
static void glm_test_reset(struct glm *glm, struct glm_unit *unit);
static void glm_test_abort(struct glm *glm, struct glm_unit *unit);
static int glm_hbaq_check(struct glm *glm, struct glm_unit *unit);
#endif

/*
 * Notes:
 *	- scsi_hba_init(9F) initializes SCSI HBA modules
 *	- must call scsi_hba_fini(9F) if modload() fails
 */
int
_init(void)
{
	int status;
	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);

	NDBG0(("_init"));

	status = ddi_soft_state_init(&glm_state, sizeof (struct glm),
		GLM_INITIAL_SOFT_SPACE);
	if (status != 0) {
		return (status);
	}

	if ((status = scsi_hba_init(&modlinkage)) != 0) {
		ddi_soft_state_fini(&glm_state);
		return (status);
	}

	mutex_init(&glm_global_mutex, NULL, MUTEX_DRIVER, NULL);
	rw_init(&glm_global_rwlock, NULL, RW_DRIVER, NULL);
	mutex_init(&glm_log_mutex, NULL, MUTEX_DRIVER, NULL);

	if ((status = mod_install(&modlinkage)) != 0) {
		mutex_destroy(&glm_log_mutex);
		rw_destroy(&glm_global_rwlock);
		mutex_destroy(&glm_global_mutex);
		ddi_soft_state_fini(&glm_state);
		scsi_hba_fini(&modlinkage);
	}

	return (status);
}

/*
 * Notes:
 *	- scsi_hba_fini(9F) uninitializes SCSI HBA modules
 */
int
_fini(void)
{
	int status;
	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);

	NDBG0(("_fini"));

	if ((status = mod_remove(&modlinkage)) == 0) {
		ddi_soft_state_fini(&glm_state);
		scsi_hba_fini(&modlinkage);
		mutex_destroy(&glm_global_mutex);
		rw_destroy(&glm_global_rwlock);
		mutex_destroy(&glm_log_mutex);
	}
	return (status);
}

/*
 * The loadable-module _info(9E) entry point
 */
int
_info(struct modinfo *modinfop)
{
	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);
	NDBG0(("glm _info"));

	return (mod_info(&modlinkage, modinfop));
}

/*ARGSUSED*/
static int
glm_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	glm_t *glm;
	int instance, error;

	NDBG0(("glm glm_info: dip=%x infocmd=%x", dip, infocmd));

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		instance = getminor((dev_t)arg);
		glm = (glm_t *)ddi_get_soft_state(glm_state, instance);
		if (glm == NULL) {
			error = DDI_FAILURE;
			break;
		}
		*result = (void *)glm->g_dip;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *) getminor((dev_t)arg);
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/*
 * Notes:
 *	Set up all device state and allocate data structures,
 *	mutexes, condition variables, etc. for device operation.
 *	Add interrupts needed.
 *	Return DDI_SUCCESS if device is ready, else return DDI_FAILURE.
 */
static int
glm_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	register glm_t		*glm = NULL;
	char			*prop_template = "target%d-scsi-options";
	char			prop_str[32];
	int			instance, i, id;
	char			buf[64];
	char			intr_added = 0;
	char			script_alloc = 0;
	char			map_setup = 0;
	char			hba_init = 0;
	char			hba_attach_setup = 0;
	char			mutex_init_done = 0;
	char			minor_node_created = 0;
	scsi_hba_tran_t		*hba_tran;

	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);

	switch (cmd) {
	case DDI_ATTACH:
		break;

	case DDI_PM_RESUME:
	case DDI_RESUME:
		hba_tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
		if (!hba_tran) {
			return (DDI_FAILURE);
		}

		glm = TRAN2GLM(hba_tran);

		if (!glm) {
			return (DDI_FAILURE);
		}

		/*
		 * Reset hardware and softc to "no outstanding commands"
		 * Note	that a check condition can result on first command
		 * to a	target.
		 */
		mutex_enter(&glm->g_mutex);

		/*
		 * glm_config_space_init will re-enable the correct
		 * values in config space.
		 */
		if (glm_config_space_init(glm) == FALSE) {
			mutex_exit(&glm->g_mutex);
			return (DDI_FAILURE);
		}

		if (glm_script_alloc(glm) != DDI_SUCCESS) {
			mutex_exit(&glm->g_mutex);
			return (DDI_FAILURE);
		}

		glm->g_suspended = 0;

		/*
		 * reset/init the chip and enable the interrupts
		 * and the interrupt handler
		 */
		GLM_RESET(glm);
		GLM_INIT(glm);
		GLM_ENABLE_INTR(glm);

		glm_syncio_reset(glm, NULL);
		glm->g_wide_enabled = glm->g_wide_known = 0;

		/* start requests, if possible */
		glm_restart_hba(glm, 0);

		mutex_exit(&glm->g_mutex);
		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);

	}

	instance = ddi_get_instance(dip);

	if (ddi_intr_hilevel(dip, 0)) {
		/*
		 * Interrupt number '0' is a high-level interrupt.
		 * At this point you either add a special interrupt
		 * handler that triggers a soft interrupt at a lower level,
		 * or - more simply and appropriately here - you just
		 * fail the attach.
		 */
		glm_log(NULL, CE_WARN,
		    "glm%d: Device is using a hilevel intr", instance);
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.attach.4001]");
		goto fail;
	}

	/*
	 * Allocate softc information.
	 */
	if (ddi_soft_state_zalloc(glm_state, instance) != DDI_SUCCESS) {
		glm_log(NULL, CE_WARN,
		    "glm%d: cannot allocate soft state", instance);
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.attach.4002]");
		goto fail;
	}

	glm = (struct glm *)ddi_get_soft_state(glm_state, instance);

	if (glm == NULL) {
		glm_log(NULL, CE_WARN,
		    "glm%d: cannot get soft state", instance);
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.attach.4003]");
		goto fail;
	}

	/* Allocate a transport structure */
	hba_tran = glm->g_tran = scsi_hba_tran_alloc(dip, SCSI_HBA_CANSLEEP);
	ASSERT(glm->g_tran != NULL);

	glm->g_dip = dip;
	glm->g_instance = instance;

	/*
	 * set host ID
	 */
	glm->g_glmid = DEFAULT_HOSTID;
	id = ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0, "initiator-id", -1);
	if (id == -1) {
		id = ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0,
		    "scsi-initiator-id", -1);
	}
	if (id != DEFAULT_HOSTID && id >= 0 && id < NTARGETS_WIDE) {
		glm_log(glm, CE_NOTE, "?initiator SCSI ID now %d\n", id);
		glm->g_glmid = (uchar_t)id;
	}

	/*
	 * Setup configuration space
	 */
	if (glm_config_space_init(glm) == FALSE) {
		/* no special clean up required for this function */
		glm_log(glm, CE_WARN, "glm_config_space_init failed");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.attach.4004]");
		goto fail;
	}

	/*
	 * map in the GLM's operating registers.
	 */
	if (ddi_regs_map_setup(dip, MEM_SPACE, &glm->g_devaddr,
	    0, 0, &dev_attr, &glm->g_datap) != DDI_SUCCESS) {
		glm_log(glm, CE_WARN, "map setup failed");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.attach.4005]");
		goto fail;
	}
	map_setup++;

	if (glm_hba_init(glm) == DDI_FAILURE) {
		glm_log(glm, CE_WARN, "glm_hba_init failed");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.attach.4006]");
		goto fail;
	}
	hba_init++;

	if (glm_script_alloc(glm) != DDI_SUCCESS) {
		glm_log(glm, CE_WARN, "glm_script_alloc failed");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.attach.4007]");
		goto fail;
	}
	script_alloc++;

	/*
	 * Get iblock_cookie to initialize mutexes used in the
	 * interrupt handler.
	 */
	if (ddi_get_iblock_cookie(dip, 0, &glm->g_iblock) != DDI_SUCCESS) {
		glm_log(glm, CE_WARN, "get iblock cookie failed");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.attach.4008]");
		goto fail;
	}

	mutex_init(&glm->g_mutex, NULL, MUTEX_DRIVER, glm->g_iblock);
	cv_init(&glm->g_cv, NULL, CV_DRIVER, NULL);
	mutex_init_done++;

	/*
	 * Now register the interrupt handler.
	 */
	if (ddi_add_intr(dip, 0, &glm->g_iblock,
	    (ddi_idevice_cookie_t *)0, glm_intr, (caddr_t)glm)) {
		glm_log(glm, CE_WARN, "adding interrupt failed");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.attach.4009]");
		goto fail;
	}
	intr_added++;

	/*
	 * initialize SCSI HBA transport structure
	 */
	hba_tran->tran_hba_private	= glm;
	hba_tran->tran_tgt_private	= NULL;

	hba_tran->tran_tgt_init		= glm_scsi_tgt_init;
	hba_tran->tran_tgt_probe	= glm_scsi_tgt_probe;
	hba_tran->tran_tgt_free		= glm_scsi_tgt_free;

	hba_tran->tran_start		= glm_scsi_start;
	hba_tran->tran_reset		= glm_scsi_reset;
	hba_tran->tran_abort		= glm_scsi_abort;
	hba_tran->tran_getcap		= glm_scsi_getcap;
	hba_tran->tran_setcap		= glm_scsi_setcap;
	hba_tran->tran_init_pkt		= glm_scsi_init_pkt;
	hba_tran->tran_destroy_pkt	= glm_scsi_destroy_pkt;

	hba_tran->tran_dmafree		= glm_scsi_dmafree;
	hba_tran->tran_sync_pkt		= glm_scsi_sync_pkt;
	hba_tran->tran_reset_notify	= glm_scsi_reset_notify;
	hba_tran->tran_get_bus_addr	= NULL;
	hba_tran->tran_get_name		= NULL;

	if (scsi_hba_attach_setup(dip, &glm_dma_attrs,
	    hba_tran, 0) != DDI_SUCCESS) {
		glm_log(glm, CE_WARN, "hba attach setup failed");
		goto fail;
	}
	hba_attach_setup++;

	/*
	 * disable wide for all targets
	 * (will be enabled by target driver if required)
	 * sync is enabled by default
	 */
	glm->g_nowide =	glm->g_notag = ALL_TARGETS;

	/*
	 * initialize the qfull retry counts
	 */
	for (i = 0; i < NTARGETS_WIDE; i++) {
		glm->g_qfull_retries[i] = QFULL_RETRIES;
		glm->g_qfull_retry_interval[i] =
		    drv_usectohz(QFULL_RETRY_INTERVAL * 1000);
	}

	/*
	 * create kmem cache for packets
	 */
	(void) sprintf(buf, "glm%d_cache", instance);
	glm->g_kmem_cache = kmem_cache_create(buf,
		sizeof (struct glm_scsi_cmd) +
			sizeof (struct scsi_pkt), 8,
		glm_kmem_cache_constructor, glm_kmem_cache_destructor,
		NULL, (void *)glm, NULL, 0);

	if (glm->g_kmem_cache == NULL) {
		glm_log(glm, CE_WARN, "creating kmem cache failed");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.attach.4010]");
		goto fail;
	}

	if (ddi_create_minor_node(dip, "devctl", S_IFCHR, instance,
	    DDI_NT_NEXUS, 0) != DDI_SUCCESS) {
		glm_log(glm, CE_WARN, "creating minor node failed");
		goto fail;
	}
	minor_node_created++;

	/*
	 * if scsi-options property exists, use it.
	 */
	glm->g_scsi_options = ddi_prop_get_int(DDI_DEV_T_ANY,
	    dip, 0, "scsi-options", DEFAULT_SCSI_OPTIONS);

	if ((glm->g_scsi_options & SCSI_OPTIONS_SYNC) == 0) {
		glm_syncio_disable(glm);
	}

	if ((glm->g_scsi_options & SCSI_OPTIONS_WIDE) == 0) {
		glm->g_nowide = ALL_TARGETS;
	}

	/*
	 * if target<n>-scsi-options property exists, use it;
	 * otherwise use the g_scsi_options
	 */
	for (i = 0; i < NTARGETS_WIDE; i++) {
		(void) sprintf(prop_str, prop_template, i);
		glm->g_target_scsi_options[i] = ddi_prop_get_int(
			DDI_DEV_T_ANY, dip, 0, prop_str, -1);

		if (glm->g_target_scsi_options[i] != -1) {
			glm_log(glm, CE_NOTE, "?target%x-scsi-options=0x%x\n",
			    i, glm->g_target_scsi_options[i]);
			glm->g_target_scsi_options_defined |= (1 << i);
		} else {
			glm->g_target_scsi_options[i] = glm->g_scsi_options;
		}
		if (((glm->g_target_scsi_options[i] &
		    SCSI_OPTIONS_DR) == 0) &&
		    (glm->g_target_scsi_options[i] & SCSI_OPTIONS_TAG)) {
			glm->g_target_scsi_options[i] &= ~SCSI_OPTIONS_TAG;
			glm_log(glm, CE_WARN,
			    "Target %d: disabled TQ since disconnects "
			    "are disabled", i);
		}

		if (glm->g_devid == GLM_53c810) {
			glm->g_target_scsi_options[i] &= ~SCSI_OPTIONS_WIDE;
		}

		/*
		 * glm driver only support FAST20 on Rev 2 or greater parts.
		 * This is the first chip with the SCLK doubler, so a 40 Mhz
		 * clock to the SCLK pin can be doubled to 80 Mhz internally.
		 *
		 * Otherwise, we disable FAST20 for all targets.
		 */
		if (glm->g_devid != GLM_53c875 ||
		    (glm->g_devid == GLM_53c875 && (GLM_REV(glm) < REV2))) {
			glm->g_target_scsi_options[i] &= ~SCSI_OPTIONS_FAST20;
		}
	}

	glm->g_scsi_tag_age_limit =
	    ddi_prop_get_int(DDI_DEV_T_ANY, dip, 0, "scsi-tag-age-limit",
		DEFAULT_TAG_AGE_LIMIT);

	glm->g_scsi_reset_delay	= ddi_prop_get_int(DDI_DEV_T_ANY,
	    dip, 0, "scsi-reset-delay",	SCSI_DEFAULT_RESET_DELAY);
	if (glm->g_scsi_reset_delay == 0) {
		glm_log(glm, CE_NOTE,
			"scsi_reset_delay of 0 is not recommended,"
			" resetting to SCSI_DEFAULT_RESET_DELAY\n");
		glm->g_scsi_reset_delay = SCSI_DEFAULT_RESET_DELAY;
	}

	/*
	 * create power	management property
	 * All components are created idle.
	 */
	if (pm_create_components(dip, 1) == DDI_SUCCESS) {
		pm_set_normal_power(dip, 0, 1);
	} else {
		glm_log(glm, CE_WARN, "creating pm component failed");
		goto fail;
	}

	/*
	 * at this point, we are not going to fail the attach
	 *
	 * used for glm_watch
	 */
	rw_enter(&glm_global_rwlock, RW_WRITER);
	if (glm_head == NULL) {
		glm_head = glm;
	} else {
		glm_tail->g_next = glm;
	}
	glm_tail = glm;
	rw_exit(&glm_global_rwlock);

	mutex_enter(&glm_global_mutex);
	if (glm_scsi_watchdog_tick == 0) {
		glm_scsi_watchdog_tick = ddi_prop_get_int(DDI_DEV_T_ANY,
		    dip, 0, "scsi-watchdog-tick", DEFAULT_WD_TICK);

		glm_tick = drv_usectohz((clock_t)
		    glm_scsi_watchdog_tick * 1000000);

		glm_timeout_id = timeout(glm_watch, NULL, glm_tick);
	}
	mutex_exit(&glm_global_mutex);

	/*
	 * reset and initilize the chip.
	 */
	GLM_RESET(glm);
	GLM_INIT(glm);

	/* Print message of HBA present */
	ddi_report_dev(dip);

	/* enable the interrupts and the interrupt handler */
	GLM_ENABLE_INTR(glm);

	return (DDI_SUCCESS);

fail:
	glm_log(glm, CE_WARN, "attach failed");
	cmn_err(CE_WARN, "!ID[SUNWpd.glm.attach.4011]");
	if (glm) {

		/* deallocate in reverse order */
		if (glm->g_kmem_cache) {
			kmem_cache_destroy(glm->g_kmem_cache);
		}
		if (minor_node_created) {
			ddi_remove_minor_node(dip, NULL);
		}
		if (hba_attach_setup) {
			(void) scsi_hba_detach(dip);
		}
		if (intr_added) {
			ddi_remove_intr(dip, 0, glm->g_iblock);
		}
		if (mutex_init_done) {
			mutex_destroy(&glm->g_mutex);
			cv_destroy(&glm->g_cv);
		}
		if (hba_init) {
			glm_hba_fini(glm);
		}
		if (script_alloc) {
			glm_script_free(glm);
		}
		if (map_setup) {
			glm_cfg_fini(glm);
		}
		if (glm->g_tran) {
			scsi_hba_tran_free(glm->g_tran);
		}
		ddi_soft_state_free(glm_state, instance);
		ddi_prop_remove_all(dip);
	}
	return (DDI_FAILURE);
}

/*
 * detach(9E).	Remove all device allocations and system resources;
 * disable device interrupts.
 * Return DDI_SUCCESS if done; DDI_FAILURE if there's a problem.
 */
static int
glm_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	register glm_t	*glm;
	scsi_hba_tran_t *tran;

	/* CONSTCOND */
	ASSERT(NO_COMPETING_THREADS);
	NDBG0(("glm_detach: dip=%x cmd=%x", devi, cmd));

	switch (cmd) {
	case DDI_DETACH:
		return (glm_dr_detach(devi));

	case DDI_SUSPEND:
	case DDI_PM_SUSPEND:
		tran = (scsi_hba_tran_t *)ddi_get_driver_private(devi);
		if (!tran) {
			return (DDI_SUCCESS);
		}
		glm = TRAN2GLM(tran);
		if (!glm) {
			return (DDI_SUCCESS);
		}

		if (cmd != DDI_SUSPEND) {
			glm->g_suspended = 1;
		}

		mutex_enter(&glm->g_mutex);

		GLM_BUS_RESET(glm);

		glm->g_wide_known = glm->g_wide_enabled = 0;
		glm_syncio_reset(glm, NULL);

		/* Disable HBA interrupts in hardware */
		GLM_DISABLE_INTR(glm);

		glm_script_free(glm);

		mutex_exit(&glm->g_mutex);

		return (DDI_SUCCESS);

	default:
		return (DDI_FAILURE);
	}
	/* NOTREACHED */
}


static int
glm_dr_detach(dev_info_t *dip)
{
	register struct glm *glm, *g;
	scsi_hba_tran_t *tran;
	struct glm_unit *unit;
	int i, j;

	NDBG0(("glm_dr_detach: dip=%x", dip));

	if (!(tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip))) {
		return (DDI_FAILURE);
	}
	glm = TRAN2GLM(tran);
	if (!glm) {
		return (DDI_FAILURE);
	}

	GLM_BUS_RESET(glm);
	GLM_DISABLE_INTR(glm);

	ddi_remove_intr(dip, (u_int) 0, glm->g_iblock);

	scsi_hba_reset_notify_tear_down(glm->g_reset_notify_listf);

	/*
	 * Remove device instance from the global linked list
	 */
	rw_enter(&glm_global_rwlock, RW_WRITER);
	if (glm_head == glm) {
		g = glm_head = glm->g_next;
	} else {
		for (g = glm_head; g != NULL; g = g->g_next) {
			if (g->g_next == glm) {
				g->g_next = glm->g_next;
				break;
			}
		}
		if (g == NULL) {
			cmn_err(CE_WARN, "!ID[SUNWpd.glm.detach.4012]");
			glm_log(glm, CE_PANIC, "Not in softc list!");
		}
	}

	if (glm_tail == glm) {
		glm_tail = g;
	}
	rw_exit(&glm_global_rwlock);

	if (glm->g_quiesce_timeid) {
		(void) untimeout(glm->g_quiesce_timeid);
	}

	/*
	 * last glm? ... if active, CANCEL watch threads.
	 */
	mutex_enter(&glm_global_mutex);
	if (glm_head == NULL) {
		mutex_exit(&glm_global_mutex);
		(void) untimeout(glm_timeout_id);
		mutex_enter(&glm_global_mutex);
		glm_timeout_id = 0;

		if (glm_reset_watch) {
			(void) untimeout(glm_reset_watch);
			glm_reset_watch = 0;
		}
	}
	mutex_exit(&glm_global_mutex);

	/*
	 * Delete ARQ pkt, nt_active, unit and DSA structures.
	 */
	for (i = 0; i < NTARGETS_WIDE; i++) {
		for (j = 0; j < NLUNS_PER_TARGET; j++) {
			if ((unit = NTL2UNITP(glm, i, j)) != NULL) {
				struct scsi_address sa;
				struct nt_slots *active = unit->nt_active;
				sa.a_hba_tran = NULL;
				sa.a_target = (u_short)i;
				sa.a_lun = (u_char)j;
				(void) glm_delete_arq_pkt(unit, &sa);
				if (active) {
					kmem_free((caddr_t)active,
					    active->nt_size);
					unit->nt_active = NULL;
				}
				glm_tear_down_unit_dsa(glm, i, j);
			}
		}
	}

	/* deallocate everything that was allocated in glm_attach */
	kmem_cache_destroy(glm->g_kmem_cache);
	ddi_remove_minor_node(dip, NULL);
	(void) scsi_hba_detach(dip);
	ddi_remove_intr(dip, 0, glm->g_iblock);
	mutex_destroy(&glm->g_mutex);
	cv_destroy(&glm->g_cv);
	glm_hba_fini(glm);
	glm_script_free(glm);
	glm_cfg_fini(glm);
	scsi_hba_tran_free(glm->g_tran);
	ddi_soft_state_free(glm_state, ddi_get_instance(dip));
	ddi_prop_remove_all(dip);

	return (DDI_SUCCESS);
}

/*
 * Initialize configuration space and figure out which
 * chip and revison of the chip the glm driver is using.
 */
static int
glm_config_space_init(struct glm *glm)
{
	ushort_t cmdreg;

	NDBG0(("glm_config_space_init"));

	/*
	 * map in configuration space.
	 */
	if (ddi_regs_map_setup(glm->g_dip, CONFIG_SPACE, &glm->g_conf_addr,
	    0, 0, &dev_attr, &glm->g_conf_handle) != DDI_SUCCESS) {
		glm_log(glm, CE_WARN, "cannot map configuration space.",
		    glm->g_instance);
		return (FALSE);
	}

	/*
	 * Get the chip device id:
	 *	1 - 53c810
	 *	3 - 53c825
	 *	f - 53c875
	 */
	glm->g_devid = ddi_get16(glm->g_conf_handle,
			(uint16_t *)(glm->g_conf_addr + PCI_CONF_DEVID));

	/*
	 * Get the chip revision.
	 */
	glm->g_revid = ddi_get8(glm->g_conf_handle,
			(uint8_t *)(glm->g_conf_addr + PCI_CONF_REVID));

	/*
	 * Each chip has different capabilities, disable certain
	 * features depending on which chip is found.
	 */
	if ((glm->g_devid == GLM_53c810) &&
		glm->g_revid & 0x10) {
		glm->g_sync_offset = SYNC_OFFSET(glm);
		glm_log(glm, CE_NOTE, "?Rev. %d Symbios 53c810A found.\n",
			GLM_REV(glm));
	} else if (glm->g_devid == GLM_53c825) {
		glm->g_options |= GLM_OPT_WIDE_BUS;
		glm->g_sync_offset = SYNC_OFFSET(glm);
		glm_log(glm, CE_NOTE, "?Rev. %d Symbios 53c825 found.\n",
		    GLM_REV(glm));
	} else if (glm->g_devid == GLM_53c875) {
		glm->g_options |= (GLM_OPT_WIDE_BUS | GLM_OPT_ONBOARD_RAM);
		glm->g_sync_offset = glm_default_offset;

		glm_log(glm, CE_NOTE, "?Rev. %d Symbios 53c875 found.\n",
		    GLM_REV(glm));

		/*
		 * Now locate the address of the SCRIPTS ram.  This
		 * address offset is needed by the SCRIPTS processor.
		 */
		glm->g_ram_base_addr = ddi_get32(glm->g_conf_handle,
		    (uint32_t *)(glm->g_conf_addr + PCI_CONF_BASE2));
	} else {
		/*
		 * Free the configuration registers and fail.
		 */
		glm_log(glm, CE_WARN, "Symbios PCI device (1000,%x) "
			"not supported.", glm->g_devid);
		ddi_regs_map_free(&glm->g_conf_handle);
		return (FALSE);
	}

	/*
	 * Set the command register to the needed values.
	 */
	cmdreg = ddi_get16(glm->g_conf_handle,
			(uint16_t *)(glm->g_conf_addr + PCI_CONF_COMM));
	cmdreg |= (PCI_COMM_ME | PCI_COMM_SERR_ENABLE |
			PCI_COMM_PARITY_DETECT | PCI_COMM_MAE);
	cmdreg &= ~PCI_COMM_IO;
	ddi_put16(glm->g_conf_handle,
			(uint16_t *)(glm->g_conf_addr + PCI_CONF_COMM), cmdreg);

	/*
	 * Set the latency timer to 0x40 as specified by the upa -> pci
	 * bridge chip design team.  This may be done by the sparc pci
	 * bus nexus driver, but the driver should make sure the latency
	 * timer is correct for performance reasons.
	 */
	ddi_put8(glm->g_conf_handle,
		(uint8_t *)(glm->g_conf_addr + PCI_CONF_LATENCY_TIMER),
		GLM_LATENCY_TIMER);

	/*
	 * Free the configuration space mapping, no longer needed.
	 */
	ddi_regs_map_free(&glm->g_conf_handle);
	return (TRUE);
}

/*
 * Initialize the Table Indirect pointers for each target, lun
 */
static glm_unit_t *
glm_unit_init(glm_t *glm, int target, int lun)
{
	size_t alloc_len;
	ddi_dma_attr_t unit_dma_attrs;
	u_int ncookie;
	struct glm_dsa *dsap;
	ddi_dma_cookie_t cookie;
	ddi_dma_handle_t dma_handle;
	ddi_acc_handle_t accessp;
	glm_unit_t *unit;

	NDBG0(("glm_unit_init: target=%x, lun=%x", target, lun));

	/*
	 * dynamically create a customized dma attribute structure
	 * that describes the GLM's per-target structures.
	 */
	unit_dma_attrs = glm_dma_attrs;
	unit_dma_attrs.dma_attr_sgllen		= 1;
	unit_dma_attrs.dma_attr_granular	= sizeof (struct glm_dsa);

	/*
	 * allocate a per-target structure upon demand,
	 * in a platform-independent manner.
	 */
	if (ddi_dma_alloc_handle(glm->g_dip, &unit_dma_attrs,
	    DDI_DMA_SLEEP, NULL, &dma_handle) != DDI_SUCCESS) {
		glm_log(glm, CE_WARN,
		    "(%d,%d): unable to allocate dma handle.", target, lun);
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.unit_init.4013]");
		return (NULL);
	}

	if (ddi_dma_mem_alloc(dma_handle, sizeof (struct glm_dsa),
	    &dev_attr, DDI_DMA_CONSISTENT, DDI_DMA_SLEEP,
	    NULL, (caddr_t *)&dsap, &alloc_len, &accessp) != DDI_SUCCESS) {
		ddi_dma_free_handle(&dma_handle);
		glm_log(glm, CE_WARN,
		    "(%d,%d): unable to allocate per-target structure.",
			target, lun);
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.unit_init.4014]");
		return (NULL);
	}

	if (ddi_dma_addr_bind_handle(dma_handle, NULL, (caddr_t)dsap,
	    alloc_len, DDI_DMA_RDWR | DDI_DMA_CONSISTENT, DDI_DMA_SLEEP,
	    NULL, &cookie, &ncookie) != DDI_DMA_MAPPED) {
		(void) ddi_dma_mem_free(&accessp);
		ddi_dma_free_handle(&dma_handle);
		glm_log(glm, CE_WARN, "(%d,%d): unable to bind DMA resources.",
		    target, lun);
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.unit_init.4015]");
		return (NULL);
	}

	unit = kmem_zalloc(sizeof (glm_unit_t), KM_SLEEP);
	ASSERT(unit != NULL);
	unit->nt_refcnt++;

	/* store pointer to per-target structure in HBA's array */
	NTL2UNITP(glm, target, lun) = unit;

	unit->nt_dsap	= dsap;
	unit->nt_dma_p	= dma_handle;
	unit->nt_target	= (ushort)target;
	unit->nt_lun	= (ushort)lun;
	unit->nt_accessp = accessp;
	unit->nt_dsa_addr = cookie.dmac_address;
	if (glm->g_reset_delay[target]) {
		unit->nt_throttle = HOLD_THROTTLE;
	} else {
		unit->nt_throttle = MAX_THROTTLE;
	}
	unit->nt_waitqtail = &unit->nt_waitq;

	return (unit);
}

/*
 * Initialize the Table Indirect pointers for each unit
 */
static void
glm_table_init(glm_t *glm, glm_unit_t *unit)
{
	struct glm_dsa *dsap;
	ddi_acc_handle_t accessp;
	uint32_t tbl_addr = unit->nt_dsa_addr;
	ushort_t target = unit->nt_target;

	NDBG0(("glm_table_init: unit=%x", unit));


	/* clear the table */
	dsap = unit->nt_dsap;
	bzero((caddr_t)dsap, sizeof (*dsap));

	/*
	 * initialize the sharable data structure between host and hba
	 *
	 * perform all byte assignments
	 */
	dsap->nt_selectparm.nt_sdid = target;
	dsap->nt_selectparm.nt_scntl3 = glm->g_scntl3;
	glm->g_dsa->g_reselectparm[target].g_sdid = target;
	glm->g_dsa->g_reselectparm[target].g_scntl3 = glm->g_scntl3;

	accessp = unit->nt_accessp;

	/* perform multi-bytes assignments */
	ddi_put32(accessp, (uint32_t *)&dsap->nt_cmd.count,
	    sizeof (dsap->nt_cdb));
	ddi_put32(accessp, (uint32_t *)&dsap->nt_cmd.address,
	    EFF_ADDR(tbl_addr, (uint32_t)
		((uintptr_t)&dsap->nt_cdb - (uintptr_t)dsap)));

	ddi_put32(accessp, (uint32_t *)&dsap->nt_sendmsg.count,
	    sizeof (dsap->nt_msgoutbuf));
	ddi_put32(accessp, (uint32_t *)&dsap->nt_sendmsg.address,
	    EFF_ADDR(tbl_addr, (uint32_t)
		((uintptr_t)&dsap->nt_msgoutbuf - (uintptr_t)dsap)));

	ddi_put32(accessp, (uint32_t *)&dsap->nt_rcvmsg.count,
	    sizeof (dsap->nt_msginbuf));
	ddi_put32(accessp, (uint32_t *)&dsap->nt_rcvmsg.address,
	    EFF_ADDR(tbl_addr, (uint32_t)
		((uintptr_t)&dsap->nt_msginbuf - (uintptr_t)dsap)));

	ddi_put32(accessp, (uint32_t *)&dsap->nt_status.count,
	    sizeof (dsap->nt_statbuf));
	ddi_put32(accessp, (uint32_t *)&dsap->nt_status.address,
	    EFF_ADDR(tbl_addr, (uint32_t)
		((uintptr_t)&dsap->nt_statbuf - (uintptr_t)dsap)));

	ddi_put32(accessp, (uint32_t *)&dsap->nt_extmsg.count,
	    sizeof (dsap->nt_extmsgbuf));
	ddi_put32(accessp, (uint32_t *)&dsap->nt_extmsg.address,
	    EFF_ADDR(tbl_addr, (uint32_t)
		((uintptr_t)&dsap->nt_extmsgbuf - (uintptr_t)dsap)));

	ddi_put32(accessp, (uint32_t *)&dsap->nt_syncin.count,
	    sizeof (dsap->nt_syncibuf));
	ddi_put32(accessp, (uint32_t *)&dsap->nt_syncin.address,
	    EFF_ADDR(tbl_addr, (uint32_t)
		((uintptr_t)&dsap->nt_syncibuf - (uintptr_t)dsap)));

	ddi_put32(accessp, (uint32_t *)&dsap->nt_errmsg.count,
	    sizeof (dsap->nt_errmsgbuf));
	ddi_put32(accessp, (uint32_t *)&dsap->nt_errmsg.address,
	    EFF_ADDR(tbl_addr, (uint32_t)
		((uintptr_t)&dsap->nt_errmsgbuf - (uintptr_t)dsap)));

	ddi_put32(accessp, (uint32_t *)&dsap->nt_widein.count,
	    sizeof (dsap->nt_wideibuf));
	ddi_put32(accessp, (uint32_t *)&dsap->nt_widein.address,
	    EFF_ADDR(tbl_addr, (uint32_t)
		((uintptr_t)&dsap->nt_wideibuf - (uintptr_t)dsap)));
}


/*
 * glm_hba_init()
 *
 *	Set up this HBA's copy of the SCRIPT and initialize
 *	each of it's target/luns.
 */
static int
glm_hba_init(glm_t *glm)
{
	size_t alloc_len;
	struct glm_hba_dsa *dsap;
	u_int ncookie;
	ddi_dma_cookie_t cookie;
	ddi_dma_attr_t hba_dma_attrs;
	ddi_acc_handle_t handle;
	uint32_t tbl_addr;

	NDBG0(("glm_hba_init"));

	glm->g_state = NSTATE_IDLE;

	/*
	 * Initialize the empty FIFO completion queue
	 */
	glm->g_donetail = &glm->g_doneq;

	/*
	 * Set syncio for hba to be reject, i.e. we never send a
	 * sdtr or wdtr to ourself.
	 */
	glm->g_syncstate[glm->g_glmid] = NSYNC_SDTR_REJECT;

	/*
	 * dynamically create a customized dma attribute structure
	 * that describes the GLM's per-hba structures.
	 */
	hba_dma_attrs = glm_dma_attrs;
	hba_dma_attrs.dma_attr_align = 0x100;
	hba_dma_attrs.dma_attr_sgllen = 1;
	hba_dma_attrs.dma_attr_granular	= sizeof (struct glm_hba_dsa);

	/*
	 * allocate a per-target structure upon demand,
	 * in a platform-independent manner.
	 */
	if (ddi_dma_alloc_handle(glm->g_dip, &hba_dma_attrs,
	    DDI_DMA_SLEEP, NULL, &glm->g_dsa_dma_h) != DDI_SUCCESS) {
		glm_log(glm, CE_WARN, "Unable to allocate hba dma handle.");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.hba_init.4016]");
		return (DDI_FAILURE);
	}

	if (ddi_dma_mem_alloc(glm->g_dsa_dma_h, sizeof (struct glm_hba_dsa),
	    &dev_attr, DDI_DMA_CONSISTENT, DDI_DMA_SLEEP, NULL,
	    (caddr_t *)&dsap, &alloc_len, &glm->g_dsa_acc_h) != DDI_SUCCESS) {
		ddi_dma_free_handle(&glm->g_dsa_dma_h);
		glm_log(glm, CE_WARN, "Unable to allocate hba DSA structure.");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.hba_init.4017]");
		return (DDI_FAILURE);
	}

	if (ddi_dma_addr_bind_handle(glm->g_dsa_dma_h, NULL, (caddr_t)dsap,
	    alloc_len, DDI_DMA_RDWR | DDI_DMA_CONSISTENT, DDI_DMA_SLEEP,
	    NULL, &cookie, &ncookie) != DDI_DMA_MAPPED) {
		(void) ddi_dma_mem_free(&glm->g_dsa_acc_h);
		ddi_dma_free_handle(&glm->g_dsa_dma_h);
		glm_log(glm, CE_WARN, "Unable to bind DMA resources.");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.hba_init.4018]");
		return (DDI_FAILURE);
	}

	bzero((caddr_t)dsap, sizeof (*dsap));
	tbl_addr = glm->g_dsa_addr = cookie.dmac_address;
	glm->g_dsa = dsap;
	handle = glm->g_dsa_acc_h;

	/*
	 * Initialize hba DSA table.
	 */
	ddi_put32(handle, (uint32_t *)&dsap->g_rcvmsg.count,
	    sizeof (dsap->g_msginbuf));
	ddi_put32(handle, (uint32_t *)&dsap->g_rcvmsg.address,
	    EFF_ADDR(tbl_addr, (uint32_t)
		((uintptr_t)&dsap->g_msginbuf - (uintptr_t)dsap)));

	ddi_put32(handle, (uint32_t *)&dsap->g_errmsg.count,
	    sizeof (dsap->g_errmsgbuf));
	ddi_put32(handle, (uint32_t *)&dsap->g_errmsg.address,
	    EFF_ADDR(tbl_addr, (uint32_t)
		((uintptr_t)&dsap->g_errmsgbuf - (uintptr_t)dsap)));

	ddi_put32(handle, (uint32_t *)&dsap->g_moremsgin.count,
	    sizeof (dsap->g_moremsginbuf));
	ddi_put32(handle, (uint32_t *)&dsap->g_moremsgin.address,
	    EFF_ADDR(tbl_addr, (uint32_t)
		((uintptr_t)&dsap->g_moremsginbuf) - (uintptr_t)dsap));

	ddi_put32(handle, (uint32_t *)&dsap->g_tagmsg.count,
	    sizeof (dsap->g_taginbuf));
	ddi_put32(handle, (uint32_t *)&dsap->g_tagmsg.address,
	    EFF_ADDR(tbl_addr, (uint32_t)
		((uintptr_t)&dsap->g_taginbuf - (uintptr_t)dsap)));

	/*
	 * Check for differential.
	 */
	if ((ddi_get8(glm->g_datap,
	    (uint8_t *)(glm->g_devaddr + NREG_GPREG)) & NB_GPREG_GPIO3) == 0) {
		ClrSetBits(glm->g_devaddr + NREG_STEST2, 0, NB_CTEST2_DIF);
	}

	return (DDI_SUCCESS);
}

static void
glm_hba_fini(glm_t *glm)
{
	NDBG0(("glm_hba_fini"));
	(void) ddi_dma_unbind_handle(glm->g_dsa_dma_h);
	(void) ddi_dma_mem_free(&glm->g_dsa_acc_h);
	ddi_dma_free_handle(&glm->g_dsa_dma_h);
}

static int
glm_script_alloc(glm_t *glm)
{
	int k;
	uint32_t offset;

	NDBG0(("glm_script_alloc"));

	/*
	 * If the glm is on a 875, download the script to the onboard
	 * 4k scripts ram.  Otherwise, use memory scripts.
	 *
	 * In the case of memory scripts, use only one copy.  Point all
	 * memory based glm's to this copy of memory scripts.
	 */
	if (glm->g_options & GLM_OPT_ONBOARD_RAM) {
		/*
		 * Now map in the 4k SCRIPTS RAM for use by the CPU/driver.
		 */
		if (ddi_regs_map_setup(glm->g_dip, BASE_REG2,
		    &glm->g_scripts_ram, 0, 4096, &dev_attr,
					&glm->g_ram_handle) != DDI_SUCCESS) {
			    return (DDI_FAILURE);
		}

		/*
		 * The reset bit in the ISTAT register can not be set
		 * if we want to write to the 4k scripts ram.
		 */
		ddi_put8(glm->g_datap, (uint8_t *)(glm->g_devaddr + NREG_ISTAT),
		    (uint8_t)~NB_ISTAT_SRST);

		/*
		 * Copy the scripts code into the local 4k RAM.
		 */
		ddi_rep_put32(glm->g_ram_handle, (uint32_t *)SCRIPT,
		    (uint32_t *)glm->g_scripts_ram, (glm_script_size >> 2),
			DDI_DEV_AUTOINCR);

		/*
		 * patch resel SCRIPTS with hba dsa addr.
		 *
		 * First, patch the script extern variable with the
		 * location in scripts ram that the DSA address exists.
		 * Second, patch that address with the DSA addr.
		 */
		for (k = 0; k < (sizeof (E_glm_hba_dsa_addr_Used)/4); k++) {
			offset = (E_glm_hba_dsa_addr_Used[k] << 2);
			ddi_put32(glm->g_ram_handle,
			    (uint32_t *)&glm->g_scripts_ram[offset],
				(uint32_t)(glm->g_ram_base_addr +
				    GLM_HBA_DSA_ADDR_OFFSET));
		}

		ddi_put32(glm->g_ram_handle,
		    (uint32_t *)(glm->g_scripts_ram +
			GLM_HBA_DSA_ADDR_OFFSET), glm->g_dsa_addr);

		/*
		 * Free the 4k SRAM mapping.
		 */
		ddi_regs_map_free(&glm->g_ram_handle);

		/*
		 * These are the script entry offsets.
		 */
		for (k = 0; k < NSS_FUNCS; k++)
			glm->g_glm_scripts[k] =
			    (glm->g_ram_base_addr + glm_script_offset(k));

		glm->g_do_list_end = (glm->g_ram_base_addr + Ent_do_list_end);
		glm->g_di_list_end = (glm->g_ram_base_addr + Ent_di_list_end);

	} else {
		/*
		 * Memory scripts are initialized once.
		 */
		if (!glm_global_init) {
			if (glm_memory_script_init(glm) == FALSE) {
				return (DDI_FAILURE);
			}
		}

		/*
		 * Point this hba to the memory scripts.
		 */
		for (k = 0; k < NSS_FUNCS; k++)
			glm->g_glm_scripts[k] = glm_scripts[k];

		glm->g_do_list_end = glm_do_list_end;
		glm->g_di_list_end = glm_di_list_end;

		glm_global_init++;
	}

	return (DDI_SUCCESS);
}

/*
 * Free the memory scripts if this is a 53c825 and this is
 * the last 53c825 HBA that depends on these scripts.
 */
static void
glm_script_free(struct glm *glm)
{
	NDBG0(("glm_script_free"));

	mutex_enter(&glm_global_mutex);
	if ((glm->g_options & GLM_OPT_ONBOARD_RAM) == 0) {
		glm_global_init--;
		if (glm_global_init == 0) {
			(void) ddi_dma_unbind_handle(glm_script_dma_handle);
			(void) ddi_dma_mem_free(&glm_script_acc_handle);
			ddi_dma_free_handle(&glm_script_dma_handle);
		}
	}
	mutex_exit(&glm_global_mutex);
}

static void
glm_cfg_fini(glm_t *glm)
{
	NDBG0(("glm_cfg_fini"));
	ddi_regs_map_free(&glm->g_datap);
}

/*
 * Offsets of SCRIPT routines.
 */
static int
glm_script_offset(int func)
{
	NDBG0(("glm_script_offset: func=%x", func));

	switch (func) {
	case NSS_STARTUP:	/* select a target and start a request */
		return (Ent_start_up);
	case NSS_CONTINUE:	/* continue with current target (no select) */
		return (Ent_continue);
	case NSS_WAIT4RESELECT:	/* wait for reselect */
		return (Ent_resel_m);
	case NSS_CLEAR_ACK:
		return (Ent_clear_ack);
	case NSS_EXT_MSG_OUT:
		return (Ent_ext_msg_out);
	case NSS_ERR_MSG:
		return (Ent_errmsg);
	case NSS_BUS_DEV_RESET:
		return (Ent_dev_reset);
	case NSS_ABORT:
		return (Ent_abort);
	case NSS_EXT_MSG_IN:
		return (Ent_ext_msg_in);
	default:
		return (0);
	}
	/*NOTREACHED*/
}

/*
 * glm_memory_script_init()
 */
static int
glm_memory_script_init(glm_t *glm)
{
	caddr_t		memp;
	int		func;
	size_t		alloc_len;
	u_int		ncookie;
	ddi_dma_cookie_t cookie;
	ddi_dma_attr_t	script_dma_attrs;
	int k;
	uint32_t offset;


	NDBG0(("glm_memory_script_init"));

	/*
	 * dynamically create a customized dma attribute structure
	 * that describes the GLM's per-target structures.
	 */
	script_dma_attrs = glm_dma_attrs;
	script_dma_attrs.dma_attr_sgllen	= 1;
	script_dma_attrs.dma_attr_granular	= 0x1000;

	if (ddi_dma_alloc_handle(glm->g_dip, &script_dma_attrs,
	    DDI_DMA_DONTWAIT, NULL, &glm_script_dma_handle) != DDI_SUCCESS) {
		glm_log(glm, CE_WARN, "unable to allocate dma handle.");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.memory_script_init.4019]");
		return (FALSE);
	}

	if (ddi_dma_mem_alloc(glm_script_dma_handle, 0x1000,
	    &dev_attr, DDI_DMA_STREAMING, DDI_DMA_DONTWAIT,
	    NULL, &memp, &alloc_len, &glm_script_acc_handle) != DDI_SUCCESS) {
		glm_log(glm, CE_WARN, "unable to allocate script memory.");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.memory_script_init.4020]");
		return (FALSE);
	}

	if (ddi_dma_addr_bind_handle(glm_script_dma_handle, NULL, memp,
	    alloc_len, DDI_DMA_READ | DDI_DMA_STREAMING, DDI_DMA_DONTWAIT, NULL,
	    &cookie, &ncookie) != DDI_DMA_MAPPED) {
		glm_log(glm, CE_WARN, "unable to allocate script DMA.");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.memory_script_init.4021]");
		return (FALSE);
	}

	/* copy the script into the buffer we just allocated */
	ddi_rep_put32(glm_script_acc_handle, (uint32_t *)SCRIPT,
	    (uint32_t *)memp, glm_script_size >> 2, DDI_DEV_AUTOINCR);

	for (func = 0; func < NSS_FUNCS; func++)
		glm_scripts[func] =
		    cookie.dmac_address + glm_script_offset(func);

	glm_do_list_end = (cookie.dmac_address + Ent_do_list_end);

	glm_di_list_end = (cookie.dmac_address + Ent_di_list_end);

	/*
	 * patch resel SCRIPTS with hba dsa addr.
	 *
	 * First, patch the script extern variable with the
	 * location in scripts ram that the DSA address exists.
	 * Second, patch that address with the DSA addr.
	 */
	for (k = 0; k < (sizeof (E_glm_hba_dsa_addr_Used)/4); k++) {
		offset = (E_glm_hba_dsa_addr_Used[k] << 2);
		ddi_put32(glm_script_dma_handle,
			(uint32_t *)&(memp[offset]),
		(cookie.dmac_address + GLM_HBA_DSA_ADDR_OFFSET));
	}

	ddi_put32(glm_script_dma_handle,
		(uint32_t *)(memp +
		GLM_HBA_DSA_ADDR_OFFSET),
		(uint32_t)glm->g_dsa_addr);

	return (TRUE);
}


/*
 * tran_tgt_init(9E) - target device instance initialization
 */
/*ARGSUSED*/
static int
glm_scsi_tgt_init(dev_info_t *hba_dip, dev_info_t *tgt_dip,
    scsi_hba_tran_t *hba_tran, struct scsi_device *sd)
{
	/*
	 * At this point, the scsi_device structure already exists
	 * and has been initialized.
	 *
	 * Use this function to allocate target-private data structures,
	 * if needed by this HBA.  Add revised flow-control and queue
	 * properties for child here, if desired and if you can tell they
	 * support tagged queueing by now.
	 */
	glm_t *glm;
	int targ = sd->sd_address.a_target;
	int lun = sd->sd_address.a_lun;
	int rval = DDI_FAILURE;
	glm = SDEV2GLM(sd);

	NDBG0(("glm_scsi_tgt_init: hbadip=%x tgtdip=%x tgt=%x lun=%x",
		hba_dip, tgt_dip, targ, lun));

	mutex_enter(&glm->g_mutex);

	/*
	 * If this HBA hardware does not support wide,
	 * return DDI_FAILURE for the high targets.
	 */
	if (targ >= NTARGETS && targ < NTARGETS_WIDE &&
		((glm->g_options & GLM_OPT_WIDE_BUS) == 0)) {
		mutex_exit(&glm->g_mutex);
		return (rval);
	}

	if (targ < 0 || targ >= NTARGETS_WIDE ||
	    lun < 0 || lun >= NLUNS_PER_TARGET) {
		NDBG0(("%s%d: %s%d bad address <%d,%d>",
			ddi_get_name(hba_dip), ddi_get_instance(hba_dip),
			ddi_get_name(tgt_dip), ddi_get_instance(tgt_dip),
			targ, lun));
		mutex_exit(&glm->g_mutex);
		return (rval);
	}

	rval = glm_create_unit_dsa(glm, targ, lun);

	mutex_exit(&glm->g_mutex);
	return (rval);
}

static int
glm_create_unit_dsa(struct glm *glm, int targ, int lun)
{
	glm_unit_t *unit;
	ASSERT(mutex_owned(&glm->g_mutex));

	/*
	 * Has this target already been initialized?
	 * Note that targets are probed multiple times for st, sd, etc.
	 */
	if ((unit = NTL2UNITP(glm, targ, lun)) != NULL) {
		unit->nt_refcnt++;
		return (DDI_SUCCESS);
	}

	/*
	 * allocate and initialize a unit structure
	 */
	unit = glm_unit_init(glm, targ, lun);
	if (unit == NULL) {
		return (DDI_FAILURE);
	}

	glm_table_init(glm, unit);

	if (glm_alloc_active_slots(glm, unit, KM_SLEEP)) {
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*
 * tran_tgt_probe(9E) - target device probing
 */
static int
glm_scsi_tgt_probe(struct scsi_device *sd, int (*callback)())
{
	dev_info_t dip = ddi_get_parent(sd->sd_dev);
	int rval = SCSIPROBE_FAILURE;
	scsi_hba_tran_t *tran;
	struct glm *glm;
	int tgt = sd->sd_address.a_target;

	NDBG0(("glm_scsi_tgt_probe: tgt=%x lun=%x",
		tgt, sd->sd_address.a_lun));

	tran = (scsi_hba_tran_t *)ddi_get_driver_private(dip);
	ASSERT(tran != NULL);
	glm = TRAN2GLM(tran);

	/*
	 * renegotiate because not all targets will return a
	 * check condition on inquiry
	 */
	mutex_enter(&glm->g_mutex);
	glm_force_renegotiation(glm, tgt);
	mutex_exit(&glm->g_mutex);
	rval = scsi_hba_probe(sd, callback);

	/*
	 * the scsi-options precedence is:
	 *	target-scsi-options		highest
	 *	device-type-scsi-options
	 *	per bus scsi-options
	 *	global scsi-options		lowest
	 */
	mutex_enter(&glm->g_mutex);
	if ((rval == SCSIPROBE_EXISTS) &&
	    ((glm->g_target_scsi_options_defined & (1 << tgt)) == 0)) {
		int options;

		options = scsi_get_device_type_scsi_options(dip, sd, -1);
		if (options != -1) {
			glm->g_target_scsi_options[tgt] = options;
			glm_log(glm, CE_NOTE,
				"?target%x-scsi-options = 0x%x\n", tgt,
				glm->g_target_scsi_options[tgt]);
			glm_force_renegotiation(glm, tgt);
		}
	}
	mutex_exit(&glm->g_mutex);

	return (rval);
}

/*
 * tran_tgt_free(9E) - target device instance deallocation
 */
/*ARGSUSED*/
static void
glm_scsi_tgt_free(dev_info_t *hba_dip, dev_info_t *tgt_dip,
    scsi_hba_tran_t *hba_tran, struct scsi_device *sd)
{
	glm_t		*glm	= TRAN2GLM(hba_tran);
	int		targ	= sd->sd_address.a_target;
	int		lun	= sd->sd_address.a_lun;

	NDBG0(("glm_scsi_tgt_free: hbadip=%x tgtdip=%x tgt=%x lun=%x",
		hba_dip, tgt_dip, targ, lun));

	mutex_enter(&glm->g_mutex);
	if (targ != glm->g_glmid) {
		glm_tear_down_unit_dsa(glm, targ, lun);
	}
	mutex_exit(&glm->g_mutex);
}

/*
 * free unit and dsa structure memory.
 */
static void
glm_tear_down_unit_dsa(struct glm *glm, int targ, int lun)
{
	glm_unit_t *unit;
	struct scsi_address sa;
	struct nt_slots *active;
	register struct glm_scsi_cmd *arqsp;
	register struct arq_private_data *arq_data;

	unit = NTL2UNITP(glm, targ, lun);
	if (unit == NULL) {
		return;
	}
	ASSERT(unit->nt_refcnt > 0);
	ASSERT(unit->nt_target != glm->g_glmid);

	/*
	 * If there are cmds to run on the waitq, keep this target's
	 * unit and dsa structures around.  Only tear down these
	 * structures when there is nothing else to run for this
	 * target.
	 */
	if ((unit->nt_waitq != NULL) || (unit->nt_ncmdp != NULL) ||
	    unit->nt_tcmds) {
		return;
	}

	/*
	 * If there is a saved sp, there must be a ARQ
	 * pkt in progress.
	 */
	if (unit->nt_arq_pkt) {
		arqsp = unit->nt_arq_pkt;
		arq_data = (struct arq_private_data *)
				arqsp->cmd_pkt->pkt_private;
		if (arq_data->arq_save_sp != NULL) {
			return;
		}
	}

	/*
	 * Decrement reference count to per-target info, and
	 * if we're finished with this target, release the
	 * per-target info.
	 */
	if (--(unit->nt_refcnt) == 0) {
		/*
		 * delete ark pkt.
		 */
		if (unit->nt_arq_pkt) {
			sa.a_hba_tran = NULL;
			sa.a_target = (u_short)unit->nt_target;
			sa.a_lun = (u_char)unit->nt_lun;
			(void) glm_delete_arq_pkt(unit, &sa);
		}

		/*
		 * remove active slot(s).
		 */
		active = unit->nt_active;
		if (active) {
			kmem_free((caddr_t)active, active->nt_size);
			unit->nt_active = NULL;
		}

		(void) ddi_dma_unbind_handle(unit->nt_dma_p);
		(void) ddi_dma_mem_free(&unit->nt_accessp);
		ddi_dma_free_handle(&unit->nt_dma_p);
		kmem_free(unit, sizeof (glm_unit_t));
		NTL2UNITP(glm, targ, lun) = NULL;
	}
}

/*
 * scsi_pkt handling
 *
 * Visible to the external world via the transport structure.
 */

/*
 * Notes:
 *	- transport the command to the addressed SCSI target/lun device
 *	- normal operation is to schedule the command to be transported,
 *	  and return TRAN_ACCEPT if this is successful.
 *	- if NO_INTR, tran_start must poll device for command completion
 */
/*ARGSUSED*/
static int
glm_scsi_start(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	glm_t *glm = PKT2GLM(pkt);
	struct glm_scsi_cmd *cmd = PKT2CMD(pkt);
	register int rval;

	NDBG1(("glm_scsi_start: target=%x pkt=%x", ap->a_target, pkt));

	/*
	 * prepare the pkt before taking mutex.
	 */
	rval = glm_prepare_pkt(glm, cmd);
	if (rval != TRAN_ACCEPT) {
		return (rval);
	}

	/*
	 * Send the command to target/lun, however your HBA requires it.
	 * If busy, return TRAN_BUSY; if there's some other formatting error
	 * in the packet, return TRAN_BADPKT; otherwise, fall through to the
	 * return of TRAN_ACCEPT.
	 *
	 * Remember that access to shared resources, including the glm_t
	 * data structure and the HBA hardware registers, must be protected
	 * with mutexes, here and everywhere.
	 *
	 * Also remember that at interrupt time, you'll get an argument
	 * to the interrupt handler which is a pointer to your glm_t
	 * structure; you'll have to remember which commands are outstanding
	 * and which scsi_pkt is the currently-running command so the
	 * interrupt handler can refer to the pkt to set completion
	 * status, call the target driver back through pkt_comp, etc.
	 */
	mutex_enter(&glm->g_mutex);
	cmd->cmd_type = NRQ_NORMAL_CMD;
	rval = glm_accept_pkt(glm, cmd);
	mutex_exit(&glm->g_mutex);

	return (rval);
}

static int
glm_accept_pkt(struct glm *glm, struct glm_scsi_cmd *cmd)
{
	struct scsi_pkt *pkt = CMD2PKT(cmd);
	struct glm_unit *unit = PKT2GLMUNITP(pkt);
	int rval = TRAN_ACCEPT;

	NDBG1(("glm_accept_pkt: cmd=%x", cmd));

	ASSERT(mutex_owned(&glm->g_mutex));

	if ((cmd->cmd_flags & CFLAG_PREPARED) == 0) {
		rval = glm_prepare_pkt(glm, cmd);
		if (rval != TRAN_ACCEPT) {
			cmd->cmd_flags &= ~CFLAG_TRANFLAG;
			return (rval);
		}
	}

	/*
	 * Create unit and dsa structures on the fly if this target
	 * was never inited/probed by tran_tgt_init(9E)/tran_tgt_probe(9E)
	 */
	if (unit == NULL) {
		if (glm_create_unit_dsa(glm, Tgt(cmd), Lun(cmd)) ==
								DDI_FAILURE) {
			return (TRAN_BADPKT);
		}
		unit = PKT2GLMUNITP(pkt);
	}

	/*
	 * tagged targets should have NTAGS slots.
	 */
	if (TAGGED(unit->nt_target) &&
	    unit->nt_active->nt_n_slots != NTAGS) {
		(void) glm_alloc_active_slots(glm, unit, KM_SLEEP);
	}

	/*
	 * reset the throttle if we were draining
	 */
	if ((unit->nt_tcmds == 0) &&
	    (unit->nt_throttle == DRAIN_THROTTLE)) {
		NDBG23(("reset throttle\n"));
		ASSERT(glm->g_reset_delay[Tgt(cmd)] == 0);
		glm_full_throttle(glm, unit->nt_target, unit->nt_lun);
	}

	if ((unit->nt_throttle > HOLD_THROTTLE) &&
	    (unit->nt_throttle > unit->nt_tcmds) &&
	    (unit->nt_waitq == NULL) &&
	    ((cmd->cmd_pkt_flags & FLAG_NOINTR) == 0) &&
	    (glm->g_state == NSTATE_IDLE)) {
		(void) glm_start_cmd(glm, unit, cmd);
	} else {
		glm_queue_pkt(glm, unit, cmd);

		/*
		 * if NO_INTR flag set, tran_start(9E) must poll
		 * device for command completion.
		 */
		if (cmd->cmd_pkt_flags & FLAG_NOINTR) {
			glm_pollret(glm, cmd);
		}
	}

	return (rval);
}

/*
 * allocate a tag byte and XXX check for tag aging
 */
static char glm_tag_lookup[] =
	{0, MSG_HEAD_QTAG, MSG_ORDERED_QTAG, 0, MSG_SIMPLE_QTAG};

static int
glm_alloc_tag(register struct glm *glm, register struct glm_unit *unit,
    register struct glm_scsi_cmd *cmd)
{
	register struct nt_slots *tag_slots;
	register int tag;

	ASSERT(mutex_owned(&glm->g_mutex));
	ASSERT(unit != NULL);

	tag_slots = unit->nt_active;
	ASSERT(tag_slots->nt_n_slots == NTAGS);

alloc_tag:
	tag = (unit->nt_active->nt_tags)++;
	if (unit->nt_active->nt_tags >= NTAGS) {
		/*
		 * we reserve tag 0 for non-tagged cmds
		 */
		unit->nt_active->nt_tags = 1;
	}

	/* Validate tag, should never fail. */
	if (tag_slots->nt_slot[tag] == 0) {
		/*
		 * Store assigned tag and tag queue type.
		 * Note, in case of multiple choice, default to simple queue.
		 */
		ASSERT(tag < NTAGS);
		cmd->cmd_tag[1] = (u_char)tag;
		cmd->cmd_tag[0] = glm_tag_lookup[((cmd->cmd_pkt_flags &
			FLAG_TAGMASK) >> 12)];
		tag_slots->nt_slot[tag] = cmd;
		unit->nt_tcmds++;
		return (0);
	} else {
		register int age, i;

		/*
		 * Check tag age.  If timeouts enabled and
		 * tag age greater than 1, print warning msg.
		 * If timeouts enabled and tag age greater than
		 * age limit, begin draining tag que to check for
		 * lost tag cmd.
		 */
		age = tag_slots->nt_slot[tag]->cmd_age++;
		if (age >= glm->g_scsi_tag_age_limit &&
		    tag_slots->nt_slot[tag]->cmd_pkt->pkt_time) {
			NDBG22(("tag %d in use, age= %d", tag, age));
			NDBG22(("draining tag queue"));
			if (glm->g_reset_delay[Tgt(cmd)] == 0) {
				unit->nt_throttle = DRAIN_THROTTLE;
			}
		}

		/* If tag in use, scan until a free one is found. */
		for (i = 1; i < NTAGS; i++) {
			tag = unit->nt_active->nt_tags;
			if (!tag_slots->nt_slot[tag]) {
				NDBG22(("found free tag %d\n", tag));
				break;
			}
			if (++(unit->nt_active->nt_tags) >= NTAGS) {
				/*
				 * we reserve tag 0 for non-tagged cmds
				 */
				unit->nt_active->nt_tags = 1;
			}
			NDBG22(("found in use tag %d\n", tag));
		}

		/*
		 * If no free tags, we're in serious trouble.
		 * the target driver submitted more than 255
		 * requests
		 */
		if (tag_slots->nt_slot[tag]) {
			NDBG22(("target %d: All tags in use!!!\n",
			    unit->nt_target));
			goto fail;
		}
		goto alloc_tag;
	}

fail:
	return (-1);
}

static void
glm_queue_pkt(glm_t *glm, glm_unit_t *unit, ncmd_t *cmd)
{
	NDBG1(("glm_queue_pkt: unit=%x cmd=%x", unit, cmd));

	/*
	 * Add this pkt to the target's work queue
	 */
	if (cmd->cmd_pkt_flags & FLAG_HEAD) {
		glm_waitq_add_lifo(unit, cmd);
	} else {
		glm_waitq_add(unit, cmd);
	}

	/*
	 * If this target isn't active stick it on the hba's work queue
	 */
	if ((unit->nt_state & NPT_STATE_QUEUED) == 0) {
		glm_queue_target(glm, unit);
	}
}

/*
 * prepare the pkt:
 * the pkt may have been resubmitted or just reused so
 * initialize some fields and do some checks.
 */
static int
glm_prepare_pkt(register struct glm *glm, register struct glm_scsi_cmd *cmd)
{
	register struct scsi_pkt *pkt = CMD2PKT(cmd);

	NDBG1(("glm_prepare_pkt: cmd=%x", cmd));

	/*
	 * Reinitialize some fields that need it; the packet may
	 * have been resubmitted
	 */
	pkt->pkt_reason = CMD_CMPLT;
	pkt->pkt_state	= 0;
	pkt->pkt_statistics = 0;
	pkt->pkt_resid = 0;
	cmd->cmd_age = 0;
	cmd->cmd_pkt_flags = pkt->pkt_flags;

	/*
	 * zero status byte.
	 */
	*(pkt->pkt_scbp) = 0;

	if (cmd->cmd_flags & CFLAG_DMAVALID) {
		pkt->pkt_resid = cmd->cmd_dmacount;

		/*
		 * consistent packets need to be sync'ed first
		 * (only for data going out)
		 */
		if (cmd->cmd_flags & (CFLAG_CMDIOPB | CFLAG_DMASEND)) {
			(void) ddi_dma_sync(cmd->cmd_dmahandle, 0, 0,
			    DDI_DMA_SYNC_FORDEV);
		}
	}

#ifdef GLM_TEST
#ifndef __lock_lint
	if (glm_test_untagged > 0) {
		struct glm_unit *unit = NTL2UNITP(glm, Tgt(cmd), Lun(cmd));
		if (TAGGED(Tgt(cmd)) && (unit != NULL)) {
			cmd->cmd_pkt_flags &= ~FLAG_TAGMASK;
			cmd->cmd_pkt_flags &= ~FLAG_NODISCON;
			glm_log(glm, CE_NOTE,
			    "starting untagged cmd, target=%d,"
			    " tcmds=%d, cmd=%x, throttle=%d\n",
			    Tgt(cmd), unit->nt_tcmds, cmd,
			    unit->nt_throttle);
			glm_test_untagged = -10;
		}
	}
#endif
#endif

#ifdef GLM_DEBUG
	if ((pkt->pkt_comp == NULL) &&
	    ((pkt->pkt_flags & FLAG_NOINTR) == 0)) {
		NDBG1(("packet with pkt_comp == 0"));
		return (TRAN_BADPKT);
	}
#endif

	if ((glm->g_target_scsi_options[Tgt(cmd)] & SCSI_OPTIONS_DR) == 0) {
		cmd->cmd_pkt_flags |= FLAG_NODISCON;
	}

	cmd->cmd_flags =
	    (cmd->cmd_flags & ~(CFLAG_TRANFLAG | CFLAG_CMD_REMOVED)) |
		CFLAG_PREPARED | CFLAG_IN_TRANSPORT;

	return (TRAN_ACCEPT);
}

/*
 * tran_init_pkt(9E) - allocate scsi_pkt(9S) for command
 *
 * One of three possibilities:
 *	- allocate scsi_pkt
 *	- allocate scsi_pkt and DMA resources
 *	- allocate DMA resources to an already-allocated pkt
 */
static struct scsi_pkt *
glm_scsi_init_pkt(struct scsi_address *ap, struct scsi_pkt *pkt,
    struct buf *bp, int cmdlen, int statuslen, int tgtlen, int flags,
    int (*callback)(), caddr_t arg)
{
	register struct glm_scsi_cmd *cmd, *new_cmd;
	struct glm *glm = ADDR2GLM(ap);
	register int failure = 1;
	int kf =  (callback == SLEEP_FUNC)? KM_SLEEP: KM_NOSLEEP;

	ASSERT(callback == NULL_FUNC || callback == SLEEP_FUNC);
#ifdef GLM_TEST_EXTRN_ALLOC
	statuslen *= 100; tgtlen *= 4;
#endif
	NDBG3(("glm_scsi_init_pkt:\n"
	"\ttgt=%x in=%x bp=%x clen=%x slen=%x tlen=%x flags=%x",
	ap->a_target, pkt, bp, cmdlen, statuslen, tgtlen, flags));

	/*
	 * Allocate the new packet.
	 */
	if (pkt == NULL) {
		ddi_dma_handle_t	*save_dma_handle;

		cmd = kmem_cache_alloc(glm->g_kmem_cache, kf);

		if (cmd) {
			save_dma_handle = cmd->cmd_dmahandle;
			bzero((caddr_t)cmd, sizeof (*cmd) +
				sizeof (struct scsi_pkt));
			cmd->cmd_dmahandle = save_dma_handle;

			pkt = (struct scsi_pkt *)((u_char *)cmd +
			    sizeof (struct glm_scsi_cmd));
			pkt->pkt_ha_private	= (opaque_t)cmd;
			pkt->pkt_address	= *ap;
			pkt->pkt_private = (opaque_t)cmd->cmd_pkt_private;
			pkt->pkt_scbp	= (opaque_t)&cmd->cmd_scb;
			pkt->pkt_cdbp	= (opaque_t)&cmd->cmd_cdb;
			cmd->cmd_pkt		= (struct scsi_pkt *)pkt;
			cmd->cmd_cdblen 	= (uchar_t)cmdlen;
			cmd->cmd_scblen		= (u_char)statuslen;
			failure = 0;
		}

		if (failure || (cmdlen > sizeof (cmd->cmd_cdb)) ||
		    (tgtlen > PKT_PRIV_LEN) ||
		    (statuslen > EXTCMDS_STATUS_SIZE)) {
			if (failure == 0) {
				/*
				 * if extern alloc fails, all will be
				 * deallocated, including cmd
				 */
				failure = glm_pkt_alloc_extern(glm, cmd,
				    cmdlen, tgtlen, statuslen, kf);
			}
			if (failure) {
				/*
				 * if extern allocation fails, it will
				 * deallocate the new pkt as well
				 */
				return (NULL);
			}
		}
		new_cmd = cmd;

	} else {
		cmd = PKT2CMD(pkt);
		new_cmd = NULL;
	}

	/*
	 * DMA resource allocation.  This version assumes your
	 * HBA has some sort of bus-mastering or onboard DMA capability, with a
	 * scatter-gather list of length GLM_MAX_DMA_SEGS, as given in the
	 * ddi_dma_attr_t structure and passed to scsi_impl_dmaget.
	 */
	if (bp && (bp->b_bcount != 0) &&
		(cmd->cmd_flags & CFLAG_DMAVALID) == 0) {

		int cnt, rval, dma_flags;
		glmti_t *dmap;		/* ptr to the S/G list */

		/*
		 * Set up DMA memory and position to the next DMA segment.
		 * Information will be in scsi_cmd on return; most usefully,
		 * in cmd->cmd_dmaseg.
		 */
		ASSERT(cmd->cmd_dmahandle != NULL);

		if (bp->b_flags & B_READ) {
			dma_flags = DDI_DMA_READ;
			cmd->cmd_flags &= ~CFLAG_DMASEND;
		} else {
			dma_flags = DDI_DMA_WRITE;
			cmd->cmd_flags |= CFLAG_DMASEND;
		}
		if (flags & PKT_CONSISTENT) {
			cmd->cmd_flags |= CFLAG_CMDIOPB;
			dma_flags |= DDI_DMA_CONSISTENT;
		}

		rval = ddi_dma_buf_bind_handle(cmd->cmd_dmahandle, bp,
				dma_flags, callback, arg,
				&cmd->cmd_cookie, &cmd->cmd_cookiec);

		if (rval && (rval != DDI_DMA_MAPPED)) {
			switch (rval) {
			case DDI_DMA_NORESOURCES:
				bioerror(bp, 0);
				break;
			case DDI_DMA_BADATTR:
			case DDI_DMA_NOMAPPING:
				bioerror(bp, EFAULT);
				break;
			case DDI_DMA_TOOBIG:
			default:
				bioerror(bp, EINVAL);
				break;
			}
			cmd->cmd_flags &= ~CFLAG_DMAVALID;
			if (new_cmd) {
				glm_scsi_destroy_pkt(ap, pkt);
			}
			return ((struct scsi_pkt *)NULL);
		}
		cmd->cmd_flags |= CFLAG_DMAVALID;
		ASSERT(cmd->cmd_cookiec > 0);


		/*
		 * Always use scatter-gather transfer
		 * Use the loop below to store physical addresses of
		 * DMA segments, from the DMA cookies, into your HBA's
		 * scatter-gather list.
		 */
		dmap = cmd->cmd_sg;

		ASSERT(cmd->cmd_cookie.dmac_size != 0);

		/*
		 * store the first segment into the S/G list
		 */
		dmap->count = cmd->cmd_cookie.dmac_size;
		dmap->address = cmd->cmd_cookie.dmac_address;

		cmd->cmd_dmacount += cmd->cmd_cookie.dmac_size;

		/*
		 * We already stored the first DMA scatter gather segment,
		 * start at 1 if we need to store more.
		 */
		for (cnt = 1; cnt < cmd->cmd_cookiec; cnt++) {
			/*
			 * Get next DMA cookie
			 */
			ddi_dma_nextcookie(cmd->cmd_dmahandle,
				&cmd->cmd_cookie);
			dmap++;

			cmd->cmd_dmacount += cmd->cmd_cookie.dmac_size;

			/*
			 * store the segment parms into the S/G list
			 */
			dmap->count = cmd->cmd_cookie.dmac_size;
			dmap->address = cmd->cmd_cookie.dmac_address;
		}
		NDBG16(("glm_dmaget: cmd_dmacount=%d.", cmd->cmd_dmacount));
	}
	return (pkt);
}

/*
 * tran_destroy_pkt(9E) - scsi_pkt(9s) deallocation
 *
 * Notes:
 *	- also frees DMA resources if allocated
 *	- implicit DMA synchonization
 */
static void
glm_scsi_destroy_pkt(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	register struct glm_scsi_cmd *cmd = PKT2CMD(pkt);
	struct glm *glm = ADDR2GLM(ap);

	NDBG3(("glm_scsi_destroy_pkt: target=%x pkt=%x",
		ap->a_target, pkt));

	if (cmd->cmd_flags & CFLAG_DMAVALID) {
		(void) ddi_dma_unbind_handle(cmd->cmd_dmahandle);
		cmd->cmd_flags &= ~CFLAG_DMAVALID;
	}

	if ((cmd->cmd_flags &
	    (CFLAG_FREE | CFLAG_CDBEXTERN | CFLAG_PRIVEXTERN |
	    CFLAG_SCBEXTERN)) == 0) {
		cmd->cmd_flags = CFLAG_FREE;
		kmem_cache_free(glm->g_kmem_cache, (void *)cmd);
	} else {
		glm_pkt_destroy_extern(glm, cmd);
	}
}

/*
 * kmem cache constructor and destructor:
 * When constructing, we bzero the cmd and allocate the dma handle
 * When destructing, just free the dma handle
 */
static int
glm_kmem_cache_constructor(void *buf, void *cdrarg, int kmflags)
{
	struct glm_scsi_cmd *cmd = buf;
	struct glm *glm  = cdrarg;
	int  (*callback)(caddr_t) = (kmflags & KM_SLEEP) ? DDI_DMA_SLEEP:
				DDI_DMA_DONTWAIT;

	NDBG4(("glm_kmem_cache_constructor"));

	bzero(buf, sizeof (*cmd) + sizeof (struct scsi_pkt));

	/*
	 * allocate a dma handle
	 */
	if ((ddi_dma_alloc_handle(glm->g_dip, &glm_dma_attrs, callback,
	    NULL, &cmd->cmd_dmahandle)) != DDI_SUCCESS) {
		return (-1);
	}
	return (0);
}

/*ARGSUSED*/
static void
glm_kmem_cache_destructor(void *buf, void *cdrarg)
{
	struct glm_scsi_cmd *cmd = buf;

	NDBG4(("glm_kmem_cache_destructor"));

	if (cmd->cmd_dmahandle) {
		ddi_dma_free_handle(&cmd->cmd_dmahandle);
	}
}

/*
 * allocate and deallocate external pkt space (ie. not part of glm_scsi_cmd)
 * for non-standard length cdb, pkt_private, status areas
 * if allocation fails, then deallocate all external space and the pkt
 */
/* ARGSUSED */
static int
glm_pkt_alloc_extern(glm_t *glm, ncmd_t *cmd,
    int cmdlen, int tgtlen, int statuslen, int kf)
{
	register caddr_t cdbp, scbp, tgt;
	int failure = 0;

	NDBG3(("glm_pkt_alloc_extern: "
	"cmd=%x cmdlen=%x tgtlen=%x statuslen=%x kf=%x",
	cmd, cmdlen, tgtlen, statuslen, kf));

	tgt = cdbp = scbp = NULL;
	cmd->cmd_scblen		= (uchar_t)statuslen;
	cmd->cmd_privlen	= (uchar_t)tgtlen;

	if (cmdlen > sizeof (cmd->cmd_cdb)) {
		if ((cdbp = kmem_zalloc((size_t)cmdlen, kf)) == NULL) {
			failure++;
		} else {
			cmd->cmd_pkt->pkt_cdbp = (opaque_t)cdbp;
			cmd->cmd_flags |= CFLAG_CDBEXTERN;
		}
	}
	if (tgtlen > PKT_PRIV_LEN) {
		if ((tgt = kmem_zalloc(tgtlen, kf)) == NULL) {
			failure++;
		} else {
			cmd->cmd_flags |= CFLAG_PRIVEXTERN;
			cmd->cmd_pkt->pkt_private = tgt;
		}
	}
	if (statuslen > EXTCMDS_STATUS_SIZE) {
		if ((scbp = kmem_zalloc((size_t)statuslen, kf)) == NULL) {
			failure++;
		} else {
			cmd->cmd_flags |= CFLAG_SCBEXTERN;
			cmd->cmd_pkt->pkt_scbp = (opaque_t)scbp;
		}
	}
	if (failure) {
		glm_pkt_destroy_extern(glm, cmd);
	}
	return (failure);
}

/*
 * deallocate external pkt space and deallocate the pkt
 */
static void
glm_pkt_destroy_extern(glm_t *glm, ncmd_t *cmd)
{

	NDBG3(("glm_pkt_destroy_extern: cmd=%x", cmd));

	if (cmd->cmd_flags & CFLAG_FREE) {
		cmn_err(CE_WARN,
		    "!ID[SUNWpd.glm.pkt_destroy_extern.4022]");
		glm_log(glm, CE_PANIC,
		    "glm_pkt_destroy_extern: freeing free packet");
		_NOTE(NOT_REACHED)
		/* NOTREACHED */
	}
	if (cmd->cmd_flags & CFLAG_CDBEXTERN) {
		kmem_free((caddr_t)cmd->cmd_pkt->pkt_cdbp,
		    (size_t)cmd->cmd_cdblen);
	}
	if (cmd->cmd_flags & CFLAG_SCBEXTERN) {
		kmem_free((caddr_t)cmd->cmd_pkt->pkt_scbp,
		    (size_t)cmd->cmd_scblen);
	}
	if (cmd->cmd_flags & CFLAG_PRIVEXTERN) {
		kmem_free((caddr_t)cmd->cmd_pkt->pkt_private,
		    (size_t)cmd->cmd_privlen);
	}
	cmd->cmd_flags = CFLAG_FREE;
	kmem_cache_free(glm->g_kmem_cache, (void *)cmd);
}

/*
 * tran_sync_pkt(9E) - explicit DMA synchronization
 */
/*ARGSUSED*/
static void
glm_scsi_sync_pkt(struct scsi_address *ap, register struct scsi_pkt *pktp)
{
	register struct glm_scsi_cmd *cmdp = PKT2CMD(pktp);

	NDBG3(("glm_scsi_sync_pkt: target=%x, pkt=%x",
		ap->a_target, pktp));

	if (cmdp->cmd_dmahandle) {
		(void) ddi_dma_sync(cmdp->cmd_dmahandle, 0, 0,
		    (cmdp->cmd_flags & CFLAG_DMASEND) ?
		    DDI_DMA_SYNC_FORDEV : DDI_DMA_SYNC_FORCPU);
	}
}

/*
 * tran_dmafree(9E) - deallocate DMA resources allocated for command
 */
/*ARGSUSED*/
static void
glm_scsi_dmafree(register struct scsi_address *ap,
    register struct scsi_pkt *pktp)
{
	register struct glm_scsi_cmd *cmd = PKT2CMD(pktp);

	NDBG3(("glm_scsi_dmafree: target=%x pkt=%x", ap->a_target, pktp));

	if (cmd->cmd_flags & CFLAG_DMAVALID) {
		(void) ddi_dma_unbind_handle(cmd->cmd_dmahandle);
		cmd->cmd_flags ^= CFLAG_DMAVALID;
	}
}

/*
 * Interrupt handling
 * Utility routine.  Poll for status of a command sent to HBA
 * without interrupts (a FLAG_NOINTR command).
 */
static void
glm_pollret(glm_t *glm, ncmd_t *poll_cmdp)
{
	int got_it = FALSE;
	int limit = 0;
	int i;

	NDBG5(("glm_pollret: cmdp=0x%x", poll_cmdp));

	/*
	 * ensure we are not accessing a target too quickly
	 * after a reset. the throttles get set back later
	 * by the reset delay watch; hopefully, we don't go
	 * thru this loop more than once
	 */
	if (glm->g_reset_delay[Tgt(poll_cmdp)]) {
		drv_usecwait(glm->g_scsi_reset_delay * 1000);
		for (i = 0; i < NTARGETS_WIDE; i++) {
			if (glm->g_reset_delay[i]) {
				int s = 0;
				glm->g_reset_delay[i] = 0;
				for (; s < NLUNS_PER_TARGET; s++) {
					glm_full_throttle(glm, i, s);
				}
				glm_queue_target_lun(glm, i);
			}
		}
	}

	/*
	 * Wait, using drv_usecwait(), long enough for the command to
	 * reasonably return from the target if the target isn't
	 * "dead".  A polled command may well be sent from scsi_poll, and
	 * there are retries built in to scsi_poll if the transport
	 * accepted the packet (TRAN_ACCEPT).  scsi_poll waits 1 second
	 * and retries the transport up to scsi_poll_busycnt times
	 * (currently 60) if
	 * 1. pkt_reason is CMD_INCOMPLETE and pkt_state is 0, or
	 * 2. pkt_reason is CMD_CMPLT and *pkt_scbp has STATUS_BUSY
	 *
	 * limit the waiting to avoid a hang in the event that the
	 * cmd never gets started but we are still receiving interrupts
	 */
	for (limit = 0; limit < GLM_POLL_TIME; limit++) {
		if (glm_wait_intr(glm) == FALSE) {
			NDBG5(("glm_pollret: command incomplete"));
			break;
		}
		glm_doneq_empty(glm);
		if (poll_cmdp->cmd_flags & CFLAG_COMPLETED) {
			got_it = TRUE;
			break;
		}
		drv_usecwait(100);
	}

	NDBG5(("glm_pollret: break"));

	if (!got_it) {
		glm_unit_t *unit = PKT2GLMUNITP(CMD2PKT(poll_cmdp));

		NDBG5(("glm_pollret: command incomplete"));

		/*
		 * this isn't supposed to happen, the hba must be wedged
		 * Mark this cmd as a timeout.
		 */
		glm_set_pkt_reason(glm, poll_cmdp, CMD_TIMEOUT,
			(STAT_TIMEOUT|STAT_ABORTED));

		if (poll_cmdp->cmd_queued == FALSE) {

			NDBG5(("glm_pollret: not on waitq"));

			/*
			 * it must be the active request
			 * reset the bus.
			 */
			GLM_BUS_RESET(glm);

			/* wait for the interrupt to clean this up */
			while (!(poll_cmdp->cmd_flags & CFLAG_COMPLETED)) {
				if (glm_wait_intr(glm) == FALSE) {
					/* we are hosed */
					glm_log(glm, CE_PANIC,
					"timeout on bus reset interrupt");
				}
			}
			poll_cmdp->cmd_pkt->pkt_state |=
			    (STATE_GOT_BUS|STATE_GOT_TARGET|STATE_SENT_CMD);
		} else {

			/* find and remove it from the waitq */
			NDBG5(("glm_pollret: delete from waitq"));
			glm_waitq_delete(unit, poll_cmdp);
		}

	}
	NDBG5(("glm_pollret: done"));

	/*
	 * check doneq again
	 */
	glm_doneq_empty(glm);
}

static int
glm_wait_intr(glm_t *glm)
{
	int cnt;
	uchar_t	istat;

	NDBG5(("glm_wait_intr"));

	/* keep trying for at least GLM_POLL_TIME/10000 seconds */
	for (cnt = 0; cnt < GLM_POLL_TIME; cnt += 1) {
		istat = GLM_GET_ISTAT(glm);
		/*
		 * loop GLM_POLL_TIME times but wait at least 100 usec
		 * each time around the loop
		 */
		if (istat & (NB_ISTAT_DIP | NB_ISTAT_SIP)) {
			glm->g_polled_intr = 1;
			NDBG17(("glm_wait_intr: istat=0x%x", istat));
			/* process this interrupt */
			glm_process_intr(glm, istat);
			return (TRUE);
		}
		drv_usecwait(100);
	}
	NDBG5(("glm_wait_intr: FAILED with istat=0x%x", istat));
	return (FALSE);
}

static void
glm_process_intr(glm_t *glm, uchar_t istat)
{
	ulong_t		action = 0;

	NDBG1(("glm_process_intr: g_state=0x%x istat=0x%x",
	    glm->g_state, istat));

	/*
	 * Always clear sigp bit if it might be set
	 */
	if (glm->g_state == NSTATE_WAIT_RESEL) {
		GLM_RESET_SIGP(glm);
	}

	/*
	 * Analyze DMA interrupts
	 */
	if (istat & NB_ISTAT_DIP) {
		action |= GLM_DMA_STATUS(glm);
	}

	/*
	 * Analyze SCSI errors and check for phase mismatch
	 */
	if (istat & NB_ISTAT_SIP) {
		action |= GLM_SCSI_STATUS(glm);
	}

	/*
	 * If no errors, no action, just restart the HBA
	 */
	if (action != 0) {
		action = glm_decide(glm, action);
	}

	/*
	 * Restart the current, or start a new, queue item
	 */
	glm_restart_hba(glm, action);
}

/*
 * glm interrupt handler
 *
 * Read the istat register first.  Check to see if a scsi interrupt
 * or dma interrupt is pending.  If that is true, handle those conditions
 * else, return DDI_INTR_UNCLAIMED.
 */
static uint_t
glm_intr(caddr_t arg)
{
	glm_t *glm = (glm_t *)arg;
	uchar_t istat;

	NDBG1(("glm_intr"));

	mutex_enter(&glm->g_mutex);

	/*
	 * Read the istat register.
	 */
	if ((istat = INTPENDING(glm)) != 0) {
		do {
			/*
			 * clear the next interrupt status from the hardware
			 */
			glm_process_intr(glm, istat);

			/*
			 * run the completion routines of all the
			 * completed commands
			 */
			glm_doneq_empty(glm);

		} while ((istat = INTPENDING(glm)) != 0);
	} else {
		if (glm->g_polled_intr) {
			glm->g_polled_intr = 0;
			mutex_exit(&glm->g_mutex);
			return (DDI_INTR_CLAIMED);
		}
		mutex_exit(&glm->g_mutex);
		return (DDI_INTR_UNCLAIMED);
	}

	NDBG1(("glm_intr complete"));
	mutex_exit(&glm->g_mutex);
	return (DDI_INTR_CLAIMED);
}

/*
 * Called from glm_pollret when an interrupt is pending on the
 * HBA, or from the interrupt service routine glm_intr.
 * Read status back from your HBA, determining why the interrupt
 * happened.  If it's because of a completed command, update the
 * command packet that relates (you'll have some HBA-specific
 * information about tying the interrupt to the command, but it
 * will lead you back to the scsi_pkt that caused the command
 * processing and then the interrupt).
 * If the command has completed normally,
 *  1. set the SCSI status byte into *pktp->pkt_scbp
 *  2. set pktp->pkt_reason to an appropriate CMD_ value
 *  3. set pktp->pkt_resid to the amount of data not transferred
 *  4. set pktp->pkt_state's bits appropriately, according to the
 *	information you now have; things like bus arbitration,
 *	selection, command sent, data transfer, status back, ARQ
 *	done
 */
static void
glm_chkstatus(glm_t *glm, glm_unit_t *unit, struct glm_scsi_cmd *cmd)
{
	struct scsi_pkt *pkt = CMD2PKT(cmd);
	struct scsi_status *status;

	NDBG1(("glm_chkstatus: unit=%x devaddr=0x%x cmd=%x",
		unit, glm->g_devaddr, cmd));

	/*
	 * Get status from target.
	 */
	*(pkt->pkt_scbp) = unit->nt_dsap->nt_statbuf[0];
	status = (struct scsi_status *)pkt->pkt_scbp;

#ifdef GLM_TEST
	if ((glm_test_instance == glm->g_instance) &&
	    (glm_test_arq_enable && (glm_test_arq++ > 10000))) {
		glm_test_arq = 0;
		status->sts_chk = 1;
	}
#endif

	if (status->sts_chk) {
		glm_force_renegotiation(glm, unit->nt_target);
	}

	/*
	 * back off sync/wide if there were parity errors
	 */
	if (pkt->pkt_statistics & STAT_PERR) {
		glm_sync_wide_backoff(glm, unit);
	} else {
#ifdef GLM_TEST
		if ((glm_test_instance == glm->g_instance) &&
		    (glm_ptest & (1<<pkt->pkt_address.a_target))) {
			glm_sync_wide_backoff(glm, unit);
			glm_ptest = 0;
		}
#endif
	}

	/*
	 * The active logical unit just completed an operation,
	 * pass the status back to the requestor.
	 */
	if (unit->nt_goterror) {
		/* interpret the error status */
		NDBG1(("glm_chkstatus: got error"));
		GLM_CHECK_ERROR(glm, unit, pkt);
	} else {
		NDBG1(("glm_chkstatus: okay"));

		/*
		 * Get residual byte count from the S/G DMA list.
		 * sync data for consistent memory xfers
		 */
		if (cmd->cmd_flags & CFLAG_DMAVALID) {
			if (cmd->cmd_flags & CFLAG_CMDIOPB &&
			    cmd->cmd_dmahandle != NULL) {
				(void) ddi_dma_sync(cmd->cmd_dmahandle, 0,
				    (uint_t)0, DDI_DMA_SYNC_FORCPU);
			}
			if (cmd->cmd_cur_cookie != 0) {
				pkt->pkt_resid = glm_sg_residual(unit, cmd);
				NDBG1(("glm_chkstatus: resid=%x dmacount=%x\n",
					pkt->pkt_resid, cmd->cmd_dmacount));
			} else {
				pkt->pkt_resid = 0;
			}
			pkt->pkt_state |= STATE_XFERRED_DATA;

			/*
			 * no data xfer.
			 */
			if (pkt->pkt_resid == cmd->cmd_dmacount) {
				pkt->pkt_state &= ~STATE_XFERRED_DATA;
			}
		}

		/*
		 * XXX- Is there a more accurate way?
		 */
		pkt->pkt_state |= (STATE_GOT_BUS | STATE_GOT_TARGET
				| STATE_SENT_CMD
				| STATE_GOT_STATUS);
	}

	/*
	 * start an autorequest sense if there was a check condition.
	 */
	if (status->sts_chk) {
		if (glm_handle_sts_chk(glm, unit, cmd)) {
			/*
			 * we can't start an arq because one is
			 * already in progress. the target is
			 * probably confused
			 */
			GLM_BUS_RESET(glm);
			return;
		}
	} else if ((*(char *)status & STATUS_MASK) ==
					STATUS_QFULL) {
			glm_handle_qfull(glm, cmd);
	}

	NDBG1(("glm_chkstatus: pkt=0x%x done", pkt));
}

/*
 * handle qfull condition
 */
static void
glm_handle_qfull(struct glm *glm, struct glm_scsi_cmd *cmd)
{
	struct glm_unit *unit = PKT2GLMUNITP(CMD2PKT(cmd));
	register int target = unit->nt_target;

	if ((++cmd->cmd_qfull_retries > glm->g_qfull_retries[target]) ||
	    (glm->g_qfull_retries[target] == 0)) {
		/*
		 * We have exhausted the retries on QFULL, or,
		 * the target driver has indicated that it
		 * wants to handle QFULL itself by setting
		 * qfull-retries capability to 0. In either case
		 * we want the target driver's QFULL handling
		 * to kick in. We do this by having pkt_reason
		 * as CMD_CMPLT and pkt_scbp as STATUS_QFULL.
		 */
		glm_set_all_lun_throttles(glm, target, DRAIN_THROTTLE);
	} else {
		if (glm->g_reset_delay[Tgt(cmd)] == 0) {
			unit->nt_throttle =
			    max((unit->nt_tcmds - 2), 0);
		}
		cmd->cmd_pkt->pkt_flags |= FLAG_HEAD;
		cmd->cmd_flags &= ~(CFLAG_TRANFLAG | CFLAG_CMD_REMOVED);
		cmd->cmd_flags |= CFLAG_QFULL_STATUS;
		(void) glm_accept_pkt(glm, cmd);

		/*
		 * when target gives queue full status with no commands
		 * outstanding (nt_tcmds == 0), throttle is set to 0
		 * (HOLD_THROTTLE), and the queue full handling start
		 * (see psarc/1994/313); if there are commands outstanding,
		 * throttle is set to (nt_tcmds - 2)
		 */
		if (unit->nt_throttle == HOLD_THROTTLE) {
			/*
			 * By setting throttle to QFULL_THROTTLE, we
			 * avoid submitting new commands and in
			 * glm_restart_cmd find out slots which need
			 * their throttles to be cleared.
			 */
			glm_set_all_lun_throttles(glm, target, QFULL_THROTTLE);
			if (glm->g_restart_cmd_timeid == 0) {
				glm->g_restart_cmd_timeid =
				    timeout(glm_restart_cmd, glm,
				    glm->g_qfull_retry_interval[target]);
			}
		}
	}
}

/*
 * invoked from timeout() to restart qfull cmds with throttle == 0
 */
static void
glm_restart_cmd(void *arg)
{
	struct glm *glm = arg;
	int i;
	struct glm_unit *unit;

	mutex_enter(&glm->g_mutex);

	glm->g_restart_cmd_timeid = 0;

	for (i = 0; i < N_GLM_UNITS; i += NLUNS_PER_TARGET) {
		if ((unit = glm->g_units[i]) == NULL) {
			continue;
		}
		if (glm->g_reset_delay[i/NLUNS_PER_TARGET] == 0) {
			if (unit->nt_throttle == QFULL_THROTTLE) {
				glm_set_all_lun_throttles(glm,
				    unit->nt_target, MAX_THROTTLE);
				glm_queue_target(glm, unit);
			}
		}
	}
	mutex_exit(&glm->g_mutex);
}

/*
 * Some event or combination of events has occurred. Decide which
 * one takes precedence and do the appropiate HBA function and then
 * the appropriate end of request function.
 */
static ulong_t
glm_decide(glm_t *glm, ulong_t action)
{
	struct glm_unit *unit = glm->g_current;

	NDBG1(("glm_decide: action=%x", action));

	/*
	 * If multiple errors occurred do the action for
	 * the most severe error.
	 */

	/*
	 * If we received a SIR interrupt, process here.
	 */
	if (action & NACTION_CHK_INTCODE) {
		action = glm_check_intcode(glm, unit, action);
		/*
		 * If we processed a reselection, g_current could
		 * have changed.
		 */
		unit = glm->g_current;
	}

	/* if sync i/o negotiation in progress, determine new syncio state */
	if (glm->g_state == NSTATE_ACTIVE &&
	    (action & (NACTION_GOT_BUS_RESET | NACTION_DO_BUS_RESET)) == 0) {
		if (action & NACTION_SDTR) {
			action = glm_syncio_decide(glm, unit, action);
		}
	}

	if (action & NACTION_GOT_BUS_RESET) {
		/*
		 * clear all requests waiting for reconnection.
		 */
		glm_flush_hba(glm);

#ifdef GLM_TEST
		if (glm_test_stop) {
			debug_enter("glm_decide: bus reset");
		}
#endif
	}

	if (action & NACTION_SIOP_REINIT) {
		GLM_RESET(glm);
		GLM_INIT(glm);
		GLM_ENABLE_INTR(glm);
		/* the reset clears the byte counters so can't do save */
		action &= ~NACTION_SAVE_BCNT;
		NDBG1(("HBA reset: devaddr=0x%x", glm->g_devaddr));
	}

	if (action & NACTION_CLEAR_CHIP) {
		/* Reset scsi offset. */
		RESET_SCSI_OFFSET(glm);

		/* Clear the DMA FIFO pointers */
		CLEAR_DMA(glm);

		/* Clear the SCSI FIFO pointers */
		CLEAR_SCSI_FIFO(glm);
	}

	if (action & NACTION_SIOP_HALT) {
		GLM_HALT(glm);
		NDBG1(("HBA halt: devaddr=0x%x", glm->g_devaddr));
	}

	if (action & NACTION_DO_BUS_RESET) {
		GLM_BUS_RESET(glm);
		(void) glm_wait_intr(glm);

		/* clear invalid actions, if any */
		action &= NACTION_DONE | NACTION_ERR | NACTION_DO_BUS_RESET |
		    NACTION_BUS_FREE;

		NDBG22(("bus reset: devaddr=0x%x", glm->g_devaddr));
	}

	if (action & NACTION_SAVE_BCNT) {
		/*
		 * Save the state of the data transfer scatter/gather
		 * for possible later reselect/reconnect.
		 */
		if (!GLM_SAVE_BYTE_COUNT(glm, unit)) {
			/* if this isn't an interrupt during a S/G dma */
			/* then the target changed phase when it shouldn't */
			NDBG1(("glm_decide: phase mismatch: devaddr=0x%x",
			    glm->g_devaddr));
		}
	}

	/*
	 * Check to see if the current request has completed.
	 * If the HBA isn't active it can't be done, we're probably
	 * just waiting for reselection and now need to reconnect to
	 * a new target.
	 */
	if (glm->g_state == NSTATE_ACTIVE) {
		action = glm_ccb_decide(glm, unit, action);
	}
	return (action);
}

static ulong_t
glm_ccb_decide(glm_t *glm, glm_unit_t *unit, ulong_t action)
{
	register struct glm_scsi_cmd *cmd;
	int target = unit->nt_target;

	NDBG1(("glm_ccb_decide: unit=%x action=%x", unit, action));

	if (action & NACTION_ERR) {
		/* error completion, save all the errors seen for later */
		unit->nt_goterror = TRUE;
	} else if ((action & NACTION_DONE) == 0) {
		/* the target's state hasn't changed */
		return (action);
	}

	/*
	 * If this target has more requests and there is no reset
	 * delay pending, then requeue it fifo order
	 */
	if ((unit->nt_waitq != NULL) &&
	    (glm->g_reset_delay[target] == 0) &&
	    ((unit->nt_state & NPT_STATE_QUEUED) == 0)) {
		glm_addbq(glm, unit);
	}

	/*
	 * The unit is now idle.
	 */
	unit->nt_state &= ~NPT_STATE_ACTIVE;

	/*
	 * If no active request then just return.  This happens with
	 * a proxy cmd like Device Reset.  NINT_DEV_RESET and
	 * glm_flush_target clean up the proxy cmd and proxy cmds do not
	 * have completion routine.
	 */
	if ((cmd = unit->nt_ncmdp) == NULL) {
		goto exit;
	}

	/*
	 * If the proxy cmd did not get cleaned up in NINT_DEV_RESET,
	 * it probably failed.  Do not try to remove proxy cmd in
	 * nt_slot[] since it will not be there.
	 */
	if ((cmd->cmd_flags & CFLAG_CMDPROXY) == 0) {
		ASSERT(cmd == unit->nt_active->nt_slot[cmd->cmd_tag[1]]);
	}
	glm_remove_cmd(glm, unit, cmd);

	/* post the completion status into the scsi packet */
	ASSERT(cmd != NULL && unit != NULL);
	glm_chkstatus(glm, unit, cmd);

	/*
	 * See if an ARQ was fired off.  If so, don't complete
	 * this pkt, glm_complete_arq_pkt will do that.
	 */
	if (cmd->cmd_flags & CFLAG_ARQ_IN_PROGRESS) {
		action |= NACTION_ARQ;
	} else if (cmd->cmd_flags & CFLAG_QFULL_STATUS) {
		/*
		 * The target returned QFULL, do not add tihs
		 * pkt to the doneq since the hba will retry
		 * this cmd.
		 *
		 * The pkt has already been resubmitted in
		 * glm_handle_qfull().  Remove this cmd_flag here.
		 */
		cmd->cmd_flags &= ~CFLAG_QFULL_STATUS;
	} else {
		/*
		 * Add the completed request to end of the done queue
		 */
		cmd->cmd_flags |= CFLAG_FINISHED;
		glm_doneq_add(glm, cmd);
	}
exit:
	glm->g_current = NULL;
	glm->g_state = NSTATE_IDLE;

	NDBG1(("glm_ccb_decide: end."));
	return (action);
}

static void
glm_remove_cmd(struct glm *glm, struct glm_unit *unit,
    struct glm_scsi_cmd *cmd)
{
	register int tag = cmd->cmd_tag[1];
	register struct nt_slots *tag_slots = unit->nt_active;

	ASSERT(cmd != NULL);
	ASSERT(cmd->cmd_queued == FALSE);

	/*
	 * clean up the tagged and untagged case.
	 */
	if (cmd == tag_slots->nt_slot[tag]) {
		tag_slots->nt_slot[tag] = NULL;
		ASSERT((cmd->cmd_flags & CFLAG_CMD_REMOVED) == 0);
		cmd->cmd_flags |= CFLAG_CMD_REMOVED;
		/*
		 * could be the current cmd.
		 */
		if (unit->nt_ncmdp == cmd) {
			unit->nt_ncmdp = NULL;
		}
		unit->nt_tcmds--;
	}

	/*
	 * clean up any proxy cmd that doesn't occupy
	 * a slot in nt_slot.
	 */
	if (unit->nt_ncmdp && (unit->nt_ncmdp == cmd) &&
	    (cmd->cmd_flags & CFLAG_CMDPROXY)) {
		ASSERT((cmd->cmd_flags & CFLAG_CMD_REMOVED) == 0);
		cmd->cmd_flags |= CFLAG_CMD_REMOVED;
		unit->nt_ncmdp = NULL;
	}

	if (cmd->cmd_flags & CFLAG_CMDDISC) {
		cmd->cmd_flags &= ~CFLAG_CMDDISC;
		glm->g_ndisc--;
	}

	/*
	 * Figure out what to set tag Q timeout for...
	 *
	 * Optimize: If we have duplicate's of same timeout
	 * we're using, then we'll use it again until we run
	 * out of duplicates.  This should be the normal case
	 * for block and raw I/O.
	 * If no duplicates, we have to scan through tag que and
	 * find the longest timeout value and use it.  This is
	 * going to take a while...
	 */
	if (cmd->cmd_pkt->pkt_time == tag_slots->nt_timebase) {
		if (--(tag_slots->nt_dups) <= 0) {
			if (unit->nt_tcmds) {
				register struct glm_scsi_cmd *ssp;
				register uint_t n = 0;
				register ushort_t t = tag_slots->nt_n_slots;
				register ushort_t i;
				/*
				 * This crude check assumes we don't do
				 * this too often which seems reasonable
				 * for block and raw I/O.
				 */
				for (i = 0; i < t; i++) {
					ssp = tag_slots->nt_slot[i];
					if (ssp &&
					    (ssp->cmd_pkt->pkt_time > n)) {
						n = ssp->cmd_pkt->pkt_time;
						tag_slots->nt_dups = 1;
					} else if (ssp &&
					    (ssp->cmd_pkt->pkt_time == n)) {
						tag_slots->nt_dups++;
					}
				}
				tag_slots->nt_timebase = n;
			} else {
				tag_slots->nt_dups = 0;
				tag_slots->nt_timebase = 0;
			}
		}
	}
	tag_slots->nt_timeout = tag_slots->nt_timebase;

	ASSERT(cmd != unit->nt_active->nt_slot[cmd->cmd_tag[1]]);
}

static ulong_t
glm_check_intcode(glm_t *glm, glm_unit_t *unit, ulong_t action)
{
	struct glm_scsi_cmd *cmd;
	struct scsi_pkt *pkt;
	glm_unit_t *re_unit;
	ulong_t intcode;
	char *errmsg;
	uchar_t width;
	ushort_t tag = 0;
	int index;

	NDBG1(("glm_check_intcode: unit=%x action=%x", unit, action));

	if (action & (NACTION_GOT_BUS_RESET | NACTION_GOT_BUS_RESET
			| NACTION_SIOP_HALT
			| NACTION_SIOP_REINIT | NACTION_BUS_FREE
			| NACTION_DONE | NACTION_ERR)) {
		return (action);
	}

	/* SCRIPT interrupt instruction */
	/* Get the interrupt vector number */
	intcode = GLM_GET_INTCODE(glm);

	NDBG1(("glm_check_intcode: intcode=%x", intcode));

	switch (intcode) {
	default:
		break;

	case NINT_OK:
		if (ddi_get8(glm->g_datap, (uint8_t *)(glm->g_devaddr +
		    NREG_SCRATCHA0)) == 0) {
			/* the dma list has completed */
			cmd = glm->g_current->nt_ncmdp;
			cmd->cmd_cur_cookie = 0;
		}
		return (NACTION_DONE | action);

	case NINT_SDP_MSG:
		/* Save Data Pointers msg */
		NDBG1(("\t\tintcode SDP"));
		if (ddi_get8(glm->g_datap, (uint8_t *)(glm->g_devaddr +
		    NREG_SCRATCHA0)) == 0) {
			/* the dma list has completed */
			cmd = glm->g_current->nt_ncmdp;
			cmd->cmd_cur_cookie = 0;
		}
		cmd = glm->g_current->nt_ncmdp;
		cmd->cmd_flags |= CFLAG_RESTORE_PTRS;
		cmd->cmd_saved_cookie = cmd->cmd_cur_cookie;
		index = cmd->cmd_cur_cookie ?
		    (cmd->cmd_cookiec - cmd->cmd_cur_cookie) : 0;
		cmd->cmd_saved_addr = unit->nt_dsap->nt_data[index].address;
		cmd->cmd_saved_count = unit->nt_dsap->nt_data[index].count;
		return (NACTION_ACK | action);

	case NINT_DISC:
		/* remove this target from the top of queue */
		NDBG1(("\t\tintcode DISC"));

		cmd = glm->g_current->nt_ncmdp;
		glm->g_current->nt_ncmdp = NULL;
		pkt = CMD2PKT(cmd);
		cmd->cmd_flags |= CFLAG_CMDDISC;
		pkt->pkt_statistics |= STAT_DISCON;

		if (ddi_get8(glm->g_datap, (uint8_t *)(glm->g_devaddr +
		    NREG_SCRATCHA0)) == 0) {
			/* the dma list has completed */
			cmd->cmd_cur_cookie = 0;
		}

		glm->g_ndisc++;
		unit->nt_state &= ~NPT_STATE_ACTIVE;
		glm->g_state = NSTATE_IDLE;
		glm->g_current = NULL;
		cmd->cmd_flags |= CFLAG_RESTORE_PTRS;

		/*
		 * Some disks do not send a save data ptr on disconnect
		 * if all data has been xferred.  Therefore, do not
		 * restore pointers on reconnect.
		 */
		if ((cmd->cmd_flags & CFLAG_DMAVALID) &&
		    (cmd->cmd_cur_cookie == 0) &&
		    (cmd->cmd_cur_cookie != cmd->cmd_saved_cookie)) {
			cmd->cmd_flags &= ~CFLAG_RESTORE_PTRS;
		}

		return (action);

	case NINT_RP_MSG:
		/* Restore Data Pointers */
		NDBG1(("\t\tintcode RP"));
		ASSERT(glm->g_current == unit);
		ASSERT(unit->nt_ncmdp != NULL);
		cmd = glm->g_current->nt_ncmdp;
		cmd->cmd_cur_cookie = cmd->cmd_saved_cookie;
		index = cmd->cmd_cur_cookie ?
		    (cmd->cmd_cookiec - cmd->cmd_cur_cookie) : 0;
		unit->nt_dsap->nt_data[index].address = cmd->cmd_saved_addr;
		unit->nt_dsap->nt_data[index].count = cmd->cmd_saved_count;
		return (NACTION_ACK | action);

	case NINT_RESEL:
		/* reselected by a disconnected target */
		NDBG1(("\t\tintcode RESEL"));
		/*
		 * One of two situations:
		 */
		switch (glm->g_state) {
		case NSTATE_ACTIVE:
		{
			/*
			 * If glm was trying to initiate a sdtr or wdtr and
			 * got preempted, undo this now.
			 */
			if (glm->g_syncstate[unit->nt_target] ==
							NSYNC_SDTR_SENT) {
				glm->g_syncstate[unit->nt_target] =
					NSYNC_SDTR_NOTDONE;
			}
			if (glm->g_wdtr_sent) {
				glm->g_wide_known &= ~(1<<unit->nt_target);
				glm->g_wdtr_sent = 0;
			}

			/*
			 * First, Remove cmd from nt_slots since it was
			 * preempted.  Then, re-queue this cmd/unit.
			 */
			cmd = unit->nt_ncmdp;

			/*
			 * remove cmd.
			 */
			ASSERT((cmd->cmd_flags & CFLAG_CMD_REMOVED) == 0);
			glm_remove_cmd(glm, unit, cmd);
			cmd->cmd_flags &= ~CFLAG_CMD_REMOVED;

			ASSERT(unit->nt_state & NPT_STATE_ACTIVE);
			unit->nt_state &= ~NPT_STATE_ACTIVE;

			/*
			 * re-queue this cmd.
			 */
			glm_waitq_add_lifo(unit, cmd);
			if ((unit->nt_state & NPT_STATE_QUEUED) == 0) {
				glm_addfq(glm, unit);
			}

			/*
			 * Pretend we were waiting for reselection
			 */
			glm->g_state = NSTATE_WAIT_RESEL;

			break;
		}
		case NSTATE_WAIT_RESEL:
			/*
			 * The DSA register has the address of the hba's
			 * dsa structure.  grab the incomming bytes and
			 * reconnect.
			 */
			break;

		default:
			/* should never happen */
			NDBG1(("\t\tintcode RESEL botched"));
			return (NACTION_DO_BUS_RESET | action);
		}

		/*
		 * Get target structure of device that wants to reconnect
		 */
		if ((re_unit = glm_get_target(glm)) == NULL) {
			glm_log(glm, CE_WARN, "invalid reselection");
			cmn_err(CE_WARN,
			    "!ID[SUNWpd.glm.check_intcode.6001]");
			return (NACTION_DO_BUS_RESET | action);
		}

		/*
		 * A target disconnected without notifying us.
		 *
		 * There could have been a parity error recieved on
		 * the scsi bus <msg-in, disconnect msg>.  In this
		 * case, the hba would have assert ATN and sent out
		 * a MSG_MSG_PARITY msg.  The target could have
		 * accepted this msg and gone bus free.  The target
		 * should have tried to resend the msg-in disconnect
		 * msg.  However, since the target disconnected, the
		 * the hba and target are not in agreement about the
		 * the target's state.
		 *
		 * Reset the bus.
		 */
		if (re_unit->nt_state & NPT_STATE_ACTIVE) {
			glm_log(glm, CE_WARN,
			    "invalid reselection (%d.%d)", re_unit->nt_target,
				re_unit->nt_lun);
			cmn_err(CE_WARN,
			    "!ID[SUNWpd.glm.check_intcode.6002]");
			return (NACTION_DO_BUS_RESET | action);
		}

		unit = re_unit;

		/*
		 * If tag queueing in use, pick up the tag byte.
		 */
		if (TAGGED(unit->nt_target) && unit->nt_tcmds &&
		    (unit->nt_active->nt_slot[0] == NULL)) {
			ASSERT(glm->g_dsa->g_taginbuf[0] != 0);
			tag = glm->g_dsa->g_taginbuf[0];
			glm->g_dsa->g_taginbuf[0] = 0;
		}

		/*
		 * The target is reconnecting with a cmd that
		 * we don't know about.  This could happen if a cmd
		 * that we think is a disconnect cmd timeout suddenly
		 * reconnects and tries to finish.
		 *
		 * The cmd could have been aborted software-wise, but
		 * this cmd is reconnecting from the target before the
		 * abort-msg has has gone out on the bus.
		 */
		if (unit->nt_active->nt_slot[tag] == NULL) {
			glm_log(glm, CE_WARN,
			    "invalid reselection (%d.%d)", re_unit->nt_target,
				re_unit->nt_lun);
			return (NACTION_DO_BUS_RESET | action);
		}

		/*
		 * tag == 0 if not using tq.
		 */
		unit->nt_ncmdp = cmd = unit->nt_active->nt_slot[tag];

		/*
		 * One less outstanding disconnected target
		 */
		glm->g_ndisc--;
		cmd->cmd_flags &= ~CFLAG_CMDDISC;

		/*
		 * Implicit restore data pointers
		 */
		if (unit->nt_ncmdp->cmd_flags & CFLAG_RESTORE_PTRS) {
			struct glm_dsa *dsap = unit->nt_dsap;
			cmd->cmd_cur_cookie = cmd->cmd_saved_cookie;
			index = cmd->cmd_cur_cookie ?
			    (cmd->cmd_cookiec - cmd->cmd_cur_cookie) : 0;
			dsap->nt_data[index].address = cmd->cmd_saved_addr;
			dsap->nt_data[index].count = cmd->cmd_saved_count;
		}

		/*
		 * Make this target the active target.
		 */
		glm->g_current = unit;
		glm->g_state = NSTATE_ACTIVE;
		glm->g_wdtr_sent = 0;

		switch (glm->g_dsa->g_moremsginbuf[0]) {
		case MSG_SIMPLE_QTAG:
		case MSG_HEAD_QTAG:
		case MSG_ORDERED_QTAG:
			ASSERT(tag != 0);
			action |= NACTION_ACK;
			break;
		case MSG_EXTENDED:
			unit->nt_state |= NPT_STATE_ACTIVE;
			action |= NACTION_EXT_MSG_IN;
			break;
		default:
			action |= NACTION_ACK;
			break;
		}
		glm->g_dsa->g_moremsginbuf[0] = 0;
		return (action);

	case NINT_SIGPROC:
		/* Give up waiting, start up another target */
		if (glm->g_state != NSTATE_WAIT_RESEL) {
			/* big trouble, bus reset time ? */
			return (NACTION_DO_BUS_RESET | NACTION_ERR | action);
		}
		NDBG1(("%s", (GLM_GET_ISTAT(glm) & NB_ISTAT_CON)
				? "glm: connected after sigproc"
				: ""));
		glm->g_state = NSTATE_IDLE;
		return (action);

	case NINT_SDTR:
		if (glm->g_state != NSTATE_ACTIVE) {
			/* reset the bus */
			return (NACTION_DO_BUS_RESET);

		}
		switch (NSYNCSTATE(glm, unit)) {
		default:
			/* reset the bus */
			NDBG1(("\t\tintcode SDTR state botch"));
			return (NACTION_DO_BUS_RESET);

		case NSYNC_SDTR_REJECT:
			/*
			 * glm is not doing sdtr, however, the disk initiated
			 * a sdtr message, respond with async (per the scsi
			 * spec).
			 */
			NDBG31(("\t\tintcode SDTR reject"));
			break;

		case NSYNC_SDTR_DONE:
			/* target wants to renegotiate */
			NDBG31(("\t\tintcode SDTR done, renegotiating"));
			glm_syncio_reset(glm, unit);
			NSYNCSTATE(glm, unit) = NSYNC_SDTR_RCVD;
			break;

		case NSYNC_SDTR_NOTDONE:
			/* target initiated negotiation */
			NDBG31(("\t\tintcode SDTR notdone"));
			glm_syncio_reset(glm, unit);
			NSYNCSTATE(glm, unit) = NSYNC_SDTR_RCVD;
			break;

		case NSYNC_SDTR_SENT:
			/* target responded to my negotiation */
			NDBG31(("\t\tintcode SDTR sent"));
			break;
		}
		return (NACTION_SDTR | action);

	case NINT_NEG_REJECT:
		NDBG31(("\t\tintcode NEG_REJECT "));

		/*
		 * A sdtr or wdtr responce was rejected.  We need to
		 * figure out what the driver was negotiating and
		 * either disable wide or disable sync.
		 */

		/*
		 * If target rejected WDTR, revert to narrow.
		 */
		if (unit->nt_dsap->nt_sendmsg.count > 1 &&
		    unit->nt_dsap->nt_msgoutbuf[2] == MSG_WIDE_DATA_XFER) {
			glm_set_wide_scntl3(glm, unit, 0);
			glm->g_wdtr_sent = 0;
		}

		/*
		 * If target rejected SDTR:
		 * Set all LUNs on this target to async i/o
		 */
		if (unit->nt_dsap->nt_sendmsg.count > 1 &&
		    unit->nt_dsap->nt_msgoutbuf[2] == MSG_SYNCHRONOUS) {
			glm_syncio_state(glm, unit, NSYNC_SDTR_DONE, 0, 0);
		}
		return (NACTION_ACK | action);

	case NINT_WDTR:
		NDBG31(("\t\tintcode NINT_WDTR"));
		/*
		 * Get the byte sent back by the target.
		 */
		width = (unit->nt_dsap->nt_wideibuf[1] & 0xff);
		ASSERT(width <= 1);

		/*
		 * If we negotiated wide (or narrow), sync negotiation
		 * was lost.  Re-negotiate sync after wdtr.
		 */
		if ((++(glm->g_wdtr_sent)) & 1) {
			/*
			 * send back a wdtr responce if this is
			 * a wide bus, otherwise reject the wdtr
			 * message.
			 */
			if (glm->g_options & GLM_OPT_WIDE_BUS) {
				unit->nt_dsap->nt_sendmsg.count = 0;
				glm_make_wdtr(glm, unit, width);
				action |= NACTION_EXT_MSG_OUT;
			} else {
				action |= NACTION_MSG_REJECT;
			}
		} else {
			glm_set_wide_scntl3(glm, unit, width);
			glm->g_wdtr_sent = 0;
			if (NSYNCSTATE(glm, unit) != NSYNC_SDTR_REJECT) {
				action |= NACTION_SDTR;
			} else {
				action |= NACTION_ACK;
			}
		}
		glm->g_props_update |= (1<<unit->nt_target);
		glm_syncio_reset(glm, unit);

		return (action);

	case NINT_MSGREJ:
		NDBG31(("\t\tintcode NINT_MSGREJ"));
		if (glm->g_state != NSTATE_ACTIVE) {
			/* reset the bus */
			return (NACTION_DO_BUS_RESET);
		}

		if (NSYNCSTATE(glm, unit) == NSYNC_SDTR_SENT) {
			/* the target can't handle sync i/o */
			glm_syncio_state(glm, unit, NSYNC_SDTR_DONE, 0, 0);
			return (NACTION_ACK | action);
		}

		if (unit->nt_dsap->nt_sendmsg.count > 1 &&
		    unit->nt_dsap->nt_msgoutbuf[3] == MSG_WIDE_DATA_XFER) {
			glm_set_wide_scntl3(glm, unit, 0);
			glm->g_wdtr_sent = 0;
			return (NACTION_ACK | action);
		}

		return (NACTION_ERR);

	case NINT_IWR:
	{
		/*
		 * If the ignore wide residue msg is received and there is
		 * more data to xfer, fix our data/count pointers.
		 */
		int index = ddi_get8(glm->g_datap,
		    (uint8_t *)(glm->g_devaddr + NREG_SCRATCHA0));
		int residue = unit->nt_dsap->nt_msginbuf[0];
		struct glm_dsa *dsap = unit->nt_dsap;

		NDBG22(("\t\tintcode NINT_IWR"));

		ASSERT(glm->g_current->nt_ncmdp != NULL);
		cmd = glm->g_current->nt_ncmdp;

		/*
		 * if this is the last xfer and the count was even then
		 * update the pointers
		 */
		if ((index == 0) && (residue == 1) &&
		((dsap->nt_data[index].count & 1) == 0)) {
			glmti_t	*sgp;
			sgp = &dsap->nt_data[index];
			sgp->address += sgp->count - 1;
			sgp->count = 1;
			glm->g_current->nt_ncmdp->cmd_cur_cookie = 1;
			ddi_put8(glm->g_datap, (uint8_t *)(glm->g_devaddr +
					NREG_SCRATCHA0), 1);
		/*
		 * if there is more data to be transferred, fix the address
		 * and count of the current segment
		 */
		} else if ((index != 0) && (residue == 1)) {
			glmti_t	*sgp;

			/*
			 * we do not need to check on even byte xfer
			 * count here
			 */
			index = cmd->cmd_cookiec - index;
			sgp = &dsap->nt_data[index];
			sgp->address--;
			sgp->count++;
		}
		return (NACTION_ACK | action);
	}

	case NINT_DEV_RESET:
		NDBG22(("\t\tintcode NINT_DEV_RESET"));

		ASSERT(unit->nt_ncmdp != NULL);
		cmd = unit->nt_ncmdp;

		if (cmd->cmd_flags & CFLAG_CMDPROXY) {
			NDBG22(("proxy cmd completed"));
			cmd->cmd_cdb[GLM_PROXY_RESULT] = TRUE;
		}

		glm_remove_cmd(glm, unit, cmd);
		cmd->cmd_flags |= CFLAG_FINISHED;
		glm_doneq_add(glm, cmd);

		switch (cmd->cmd_type) {
		case NRQ_DEV_RESET:
			/*
			 * clear requests for all the LUNs on this device
			 */
			glm_flush_target(glm, unit->nt_target,
			    CMD_RESET, STAT_DEV_RESET);
			NDBG22((
			"glm_check_intcode: bus device reset completed"));
			break;
		case NRQ_ABORT:
			glm_flush_lun(glm, unit, CMD_ABORTED, STAT_ABORTED);
			NDBG23(("glm_check_intcode: abort completed"));
			break;
		default:
			glm_log(glm, CE_WARN,
			    "invalid interrupt for device reset or abort");
			/*
			 * XXXX not sure what to return here?
			 */
			return (NACTION_DONE | NACTION_CLEAR_CHIP | action);
		}
		return (NACTION_DONE | NACTION_CLEAR_CHIP | action);
	}

	/*
	 * All of the interrupt codes handled above are the of
	 * the "expected" non-error type. The following interrupt
	 * codes are for unusual errors detected by the SCRIPT.
	 * For now treat all of them the same, mark the request
	 * as failed due to an error and then reset the SCSI bus.
	 */
handle_error:

	/*
	 * Some of these errors should be getting BUS DEVICE RESET
	 * rather than bus_reset.
	 */
	switch (intcode) {
	case NINT_SELECTED:
		errmsg = "got selected";
		break;
	case NINT_UNS_MSG:
		cmn_err(CE_WARN, "!ID[SUNWpd.check_intcode.6003]");
		errmsg = "got an unsupported message";
		break;
	case NINT_ILI_PHASE:
		errmsg = "got incorrect phase";
		break;
	case NINT_UNS_EXTMSG:
		cmn_err(CE_WARN, "!ID[SUNWpd.check_intcode.6004]");
		errmsg = "got unsupported extended message";
		break;
	case NINT_MSGIN:
		/*
		 * If the target dropped the bus during reselection,
		 * the dsa register has the address of the hba.  Use
		 * the SSID reg and msg-in phase to try to determine
		 * which target dropped the bus.
		 */
		if ((unit = glm_get_target(glm)) == NULL) {
			glm_log(glm, CE_WARN, "invalid reselection");
			cmn_err(CE_WARN, "!ID[SUNWpd.check_intcode.6005]");
			return (NACTION_DO_BUS_RESET | action);
		}
		errmsg = "Message-In was expected";
		break;
	case NINT_MSGREJ:
		errmsg = "got unexpected Message Reject";
		break;
	case NINT_REJECT:
		errmsg = "unable to send Message Reject";
		break;
	case NINT_TOOMUCHDATA:
		cmn_err(CE_WARN, "!ID[SUNWpd.check_intcode.6006]");
		errmsg = "data overrun: got too much data from target";
		break;

	default:
		cmn_err(CE_WARN, "!ID[SUNWpd.check_intcode.6007]");
		glm_log(glm, CE_WARN, "invalid intcode=%lu", intcode);
		errmsg = "default";
		break;
	}

bus_reset:
	glm_log(glm, CE_WARN, "Resetting scsi bus, %s from (%d,%d)",
		errmsg, unit->nt_target, unit->nt_lun);

	glm_sync_wide_backoff(glm, unit);
	return (NACTION_DO_BUS_RESET | NACTION_ERR);
}

/*
 * figure out the recovery for a parity interrupt or a SGE interrupt.
 */
static ulong_t
glm_parity_check(struct glm *glm, struct glm_unit *unit)
{
	ushort_t phase;
	ulong_t action = NACTION_ERR;
	struct glm_scsi_cmd *cmd;

	ASSERT(unit->nt_ncmdp != NULL);
	cmd = unit->nt_ncmdp;

	NDBG31(("glm_parity_check: cmd=%x", cmd));

	/*
	 * Get the phase of the parity error.
	 */
	phase = (ddi_get8(glm->g_datap,
			(uint8_t *)(glm->g_devaddr + NREG_DCMD)) & 0x7);

	switch (phase) {
	case NSOP_MSGIN:
		glm_log(glm, CE_WARN,
		    "SCSI bus MESSAGE IN phase parity error");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.parity_check.6008]");
		glm_set_pkt_reason(glm, cmd, CMD_CMPLT, STAT_PERR);
		action = NACTION_MSG_PARITY;
		break;
	case NSOP_MSGOUT:
		action = (NACTION_CLEAR_CHIP |
		    NACTION_DO_BUS_RESET | NACTION_ERR);
		break;
	case NSOP_COMMAND:
		action = (NACTION_INITIATOR_ERROR);
		break;
	case NSOP_STATUS:
		glm_set_pkt_reason(glm, cmd, CMD_TRAN_ERR, STAT_PERR);
		glm_log(glm, CE_WARN, "SCSI bus STATUS phase parity error");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.parity_check.6009]");
		action = NACTION_INITIATOR_ERROR;
		break;
	case NSOP_DATAIN:
		glm_set_pkt_reason(glm, cmd, CMD_TRAN_ERR, STAT_PERR);
		glm_log(glm, CE_WARN, "SCSI bus DATA IN phase parity error");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.parity_check.6010]");
		action = (NACTION_SAVE_BCNT | NACTION_INITIATOR_ERROR);
		break;
	case NSOP_DATAOUT:
		glm_set_pkt_reason(glm, cmd, CMD_TRAN_ERR, STAT_PERR);
		glm_log(glm, CE_WARN, "SCSI bus DATA OUT phase parity error");
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.parity_check.6011]");
		action = (NACTION_SAVE_BCNT | NACTION_INITIATOR_ERROR);
		break;
	default:
		break;
	}
	return (action);
}

/*
 * start a fresh request from the top of the device queue
 */
static void
glm_start_next(struct glm *glm)
{
	glm_unit_t *unit;
	struct glm_scsi_cmd *cmd;

	NDBG1(("glm_start_next: glm=0x%x", glm));

again:
	GLM_RMQ(glm, unit);
	if (unit) {
		/*
		 * If all cmds drained from the Tag Q, back to full
		 * throttle and start submitting new cmds again.
		 */
		if (unit->nt_throttle == DRAIN_THROTTLE &&
		    unit->nt_tcmds == 0) {
			glm_full_throttle(glm, unit->nt_target, unit->nt_lun);
		}

		/*
		 * If there is a reset delay for this target and
		 * it happens to be queued on the hba work list, get
		 * the next target to run.  This target will be queued
		 * up later when the reset delay expires.
		 */
		if (unit->nt_throttle > unit->nt_tcmds) {
			GLM_WAITQ_RM(unit, cmd);
			if (cmd) {
				if (glm_start_cmd(glm, unit, cmd) == TRUE) {
					/*
					 * If this is a tagged target and
					 * there is more work to do,
					 * re-queue this target.
					 */
					if (TAGGED(unit->nt_target) &&
					    unit->nt_waitq != NULL) {
						glm_addbq(glm, unit);
					}
					return;
				}
			}
		}
		goto again;
	}

	/* no devs waiting for the hba, wait for disco-ed dev */
	glm_wait_for_reselect(glm, 0);
}

static int
glm_start_cmd(struct glm *glm, glm_unit_t *unit, struct glm_scsi_cmd *cmd)
{
	struct glm_dsa	*dsap;
	struct scsi_pkt *pktp = CMD2PKT(cmd);
	register int n;
	register struct nt_slots *slots = unit->nt_active;
	ushort_t target = unit->nt_target;
	ushort_t tshift = (1<<target);
	uint_t i = 0;

	NDBG1(("glm_start_cmd: cmd=%x\n", cmd));

	ASSERT(glm->g_state == NSTATE_IDLE);
	ASSERT(glm->g_reset_delay[Tgt(cmd)] == 0);

	unit->nt_goterror = FALSE;
	unit->nt_dma_status = 0;
	unit->nt_status0 = 0;
	unit->nt_status1 = 0;
	glm->g_wdtr_sent = 0;
	dsap = unit->nt_dsap;
	dsap->nt_statbuf[0] = 0;
	dsap->nt_errmsgbuf[0] = (uchar_t)MSG_NOP;

	/*
	 * if a non-tagged cmd is submitted to an active tagged target
	 * then drain before submitting this cmd; SCSI-2 allows RQSENSE
	 * to be untagged
	 */
	if (((cmd->cmd_pkt_flags & FLAG_TAGMASK) == 0) &&
	    TAGGED(target) && unit->nt_tcmds &&
	    ((cmd->cmd_flags & CFLAG_CMDPROXY) == 0) &&
	    (*(cmd->cmd_pkt->pkt_cdbp) != SCMD_REQUEST_SENSE)) {
		if ((cmd->cmd_pkt_flags & FLAG_NOINTR) == 0) {

			NDBG23(("untagged cmd, start draining\n"));

			if (glm->g_reset_delay[target] == 0) {
				unit->nt_throttle = DRAIN_THROTTLE;
			}
			glm_waitq_add_lifo(unit, cmd);
		}
		return (FALSE);
	}

	if (TAGGED(target) && (cmd->cmd_pkt_flags & FLAG_TAGMASK)) {
		if (glm_alloc_tag(glm, unit, cmd)) {
			glm_waitq_add_lifo(unit, cmd);
			return (FALSE);
		}
	} else {
		/*
		 * tag slot 0 is reserved for non-tagged cmds
		 * and should be empty because we have drained
		 */
		if ((cmd->cmd_flags & CFLAG_CMDPROXY) == 0) {
			ASSERT(unit->nt_active->nt_slot[0] == NULL);
			unit->nt_active->nt_slot[0] = cmd;
			cmd->cmd_tag[1] = 0;
			if (*(cmd->cmd_pkt->pkt_cdbp) != SCMD_REQUEST_SENSE) {
				ASSERT(unit->nt_tcmds == 0);
			}
			unit->nt_tcmds++;

			/*
			 * don't start any other cmd until this one is finished.
			 * the throttle is reset later in glm_watchsubr()
			 */
			unit->nt_throttle = 1;
		}
	}

	/*
	 * Attach this target to the hba and make it active
	 */
	glm->g_current = unit;
	glm->g_state = NSTATE_ACTIVE;
	unit->nt_state |= NPT_STATE_ACTIVE;
	unit->nt_ncmdp = cmd;

	/*
	 * check to see if target is allowed to disconnect
	 */
	if (cmd->cmd_pkt_flags & FLAG_NODISCON) {
		dsap->nt_msgoutbuf[i++] = (MSG_IDENTIFY | unit->nt_lun);
	} else {
		dsap->nt_msgoutbuf[i++] =
		    (MSG_DR_IDENTIFY | unit->nt_lun);
	}

	/*
	 * Assign tag byte to this cmd.
	 * (proxy msg's don't have tag flag set)
	 */
	if (cmd->cmd_pkt_flags & FLAG_TAGMASK) {
		ASSERT((cmd->cmd_pkt_flags & FLAG_NODISCON) == 0);
		dsap->nt_msgoutbuf[i++] = cmd->cmd_tag[0];
		dsap->nt_msgoutbuf[i++] = cmd->cmd_tag[1];
	}

	/*
	 * Single identify msg.
	 */
	dsap->nt_sendmsg.count = i;

	switch (cmd->cmd_type) {
	case NRQ_NORMAL_CMD:

		NDBG1(("glm_start_cmd: normal"));

		/*
		 * save the cdb length.
		 */
		dsap->nt_cmd.count = cmd->cmd_cdblen;

		/*
		 * Copy the CDB to our DSA structure for table
		 * indirect scripts access.
		 */
		(void) ddi_rep_put8(unit->nt_accessp, (uint8_t *)pktp->pkt_cdbp,
		    dsap->nt_cdb, cmd->cmd_cdblen, DDI_DEV_AUTOINCR);

		/*
		 * setup the Scatter/Gather DMA list for this request
		 */
		if (cmd->cmd_cookiec > 0) {
			uint_t cookiec;
			uint_t index;
			ASSERT(cmd->cmd_flags & CFLAG_DMAVALID);
			ASSERT(cmd->cmd_cookiec <= GLM_MAX_DMA_SEGS);

			/*
			 * Save the number of entries in the DMA
			 * Scatter/Gather list
			 */
			cookiec = cmd->cmd_cur_cookie = cmd->cmd_cookiec;

			NDBG1(("glm_start_cmd: cookiec=%x", cookiec));

			while (cookiec--) {
				(void) ddi_rep_put32(unit->nt_accessp,
				    (uint32_t *)&cmd->cmd_sg[cookiec],
				    (uint32_t *)&dsap->nt_data[cookiec],
				    2, DDI_DEV_AUTOINCR);
			}

			/*
			 * Save the data pointers for disconnects/reselects.
			 */
			cmd->cmd_saved_cookie = cmd->cmd_cur_cookie;
			index = (cmd->cmd_cookiec - 1);
			cmd->cmd_saved_addr =
			    unit->nt_dsap->nt_data[index].address;
			cmd->cmd_saved_count =
			    unit->nt_dsap->nt_data[index].count;
		}

		if (((glm->g_wide_known | glm->g_nowide) & tshift) &&
		    (NSYNCSTATE(glm, unit) != NSYNC_SDTR_NOTDONE)) {
startup:
			GLM_SETUP_SCRIPT(glm, unit);
			GLM_START_SCRIPT(glm, NSS_STARTUP);
			/*
			 * Start timeout.
			 */
#ifdef GLM_TEST
			/*
			 * Temporarily set timebase = 0;  needed for
			 * timeout torture test.
			 */
			if (glm_test_timeouts) {
				slots->nt_timebase = 0;
			}
#endif
			n = pktp->pkt_time - slots->nt_timebase;

			if (n == 0) {
				(slots->nt_dups)++;
				slots->nt_timeout = slots->nt_timebase;
			} else if (n > 0) {
				slots->nt_timeout =
				    slots->nt_timebase = pktp->pkt_time;
				slots->nt_dups = 1;
			}
#ifdef GLM_TEST
			/*
			 * Set back to a number higher than
			 * glm_scsi_watchdog_tick
			 * so timeouts will happen in glm_watchsubr
			 */
			if (glm_test_timeouts) {
				slots->nt_timebase = 60;
			}
#endif
			break;
		}

		if (((glm->g_wide_known | glm->g_nowide) & tshift) == 0) {
			glm_make_wdtr(glm, unit, GLM_XFER_WIDTH);
		} else if (NSYNCSTATE(glm, unit) == NSYNC_SDTR_NOTDONE) {
			NDBG31(("glm_start_cmd: try syncio"));
			/* haven't yet tried syncio on this target */
			glm_syncio_msg_init(glm, unit);
			glm->g_syncstate[unit->nt_target] = NSYNC_SDTR_SENT;
		}
		goto startup;

	case NRQ_DEV_RESET:
		NDBG22(("glm_start_cmd: bus device reset"));
		/* reset the msg out length for single message byte */
		dsap->nt_msgoutbuf[i++] = MSG_DEVICE_RESET;
		dsap->nt_sendmsg.count = i;
		/* no command buffer */
		goto bus_dev_reset;

	case NRQ_ABORT:
		NDBG23(("glm_start_cmd: abort"));
		/* reset the msg out length for two single */
		/* byte messages */
		dsap->nt_msgoutbuf[i++] = MSG_ABORT;
		dsap->nt_sendmsg.count = i;

bus_dev_reset:
		/* no command buffer */
		dsap->nt_cmd.count = 0;
		GLM_SETUP_SCRIPT(glm, unit);
		GLM_START_SCRIPT(glm, NSS_BUS_DEV_RESET);
		break;

	default:
		glm_log(glm, CE_PANIC,
		    "invalid queue entry cmd=0x%p", cmd);
		/* NOTREACHED */
	}

	return (TRUE);
}

static void
glm_wait_for_reselect(glm_t *glm, ulong_t action)
{
	NDBG1(("glm_wait_for_reselect: action=%x", action));

	/*
	 * The hba's dsa structure will take care of reconnections,
	 * so NULL out g_current.  It will get set again during a
	 * valid reselection.
	 */
	glm->g_current = NULL;
	glm->g_state = NSTATE_WAIT_RESEL;
	glm->g_dsa->g_errmsgbuf[0] = (uchar_t)MSG_NOP;

	action &= NACTION_ABORT | NACTION_MSG_REJECT | NACTION_MSG_PARITY |
			NACTION_INITIATOR_ERROR;

	/*
	 * Put hba's dsa structure address in DSA reg.
	 */
	if (action == 0 && glm->g_ndisc != 0) {
		/* wait for any disconnected targets */
		GLM_START_SCRIPT(glm, NSS_WAIT4RESELECT);
		NDBG19(("glm_wait_for_reselect: WAIT"));
		return;
	}

	if (action & NACTION_ABORT) {
		/* abort an invalid reconnect */
		glm->g_dsa->g_errmsgbuf[0] = (uchar_t)MSG_ABORT;
		GLM_START_SCRIPT(glm, NSS_ABORT);
		return;
	}

	if (action & NACTION_MSG_REJECT) {
		/* target sent me bad msg, send msg reject */
		glm->g_dsa->g_errmsgbuf[0] = (uchar_t)MSG_REJECT;
		GLM_START_SCRIPT(glm, NSS_ERR_MSG);
		NDBG19(("glm_wait_for_reselect: Message Reject"));
		return;
	}

	if (action & NACTION_MSG_PARITY) {
		/* got a parity error during message in phase */
		glm->g_dsa->g_errmsgbuf[0] = (uchar_t)MSG_MSG_PARITY;
		GLM_START_SCRIPT(glm, NSS_ERR_MSG);
		NDBG19(("glm_wait_for_reselect: Message Parity Error"));
		return;
	}

	if (action & NACTION_INITIATOR_ERROR) {
		/* catchall for other errors */
		glm->g_dsa->g_errmsgbuf[0] = (uchar_t)MSG_INITIATOR_ERROR;
		GLM_START_SCRIPT(glm, NSS_ERR_MSG);
		NDBG1(("glm_wait_for_reselect: Initiator Detected Error"));
		return;
	}

	/* no disconnected targets go idle */
	glm->g_current = NULL;
	glm->g_state = NSTATE_IDLE;
	NDBG1(("glm_wait_for_reselect: IDLE"));
}

/*
 * How the hba continues depends on whether sync i/o
 * negotiation was in progress and if so how far along.
 * Or there might be an error message to be sent out.
 */
static void
glm_restart_current(glm_t *glm, ulong_t action)
{
	glm_unit_t	*unit = glm->g_current;
	struct glm_dsa	*dsap;

	NDBG1(("glm_restart_current: action=%x", action));

	if (unit == NULL) {
		/* the current request just finished, do the next one */
		glm_start_next(glm);
		return;
	}

	dsap = unit->nt_dsap;
	dsap->nt_errmsgbuf[0] = (uchar_t)MSG_NOP;

	if (unit->nt_state & NPT_STATE_ACTIVE) {
		NDBG1(("glm_restart_current: active"));

		action &= (NACTION_ACK | NACTION_EXT_MSG_OUT |
			    NACTION_MSG_REJECT | NACTION_MSG_PARITY |
			    NACTION_INITIATOR_ERROR | NACTION_ARQ |
			    NACTION_EXT_MSG_IN);

		if (action == 0) {
			/* continue the script on the currently active target */
			GLM_START_SCRIPT(glm, NSS_CONTINUE);
			goto done;
		}

		if (action & NACTION_ACK) {
			/* just ack the last byte and continue */
			GLM_START_SCRIPT(glm, NSS_CLEAR_ACK);
			goto done;
		}

		if (action & NACTION_EXT_MSG_OUT) {
			/* send my SDTR message */
			GLM_START_SCRIPT(glm, NSS_EXT_MSG_OUT);
			goto done;
		}

		if (action & NACTION_EXT_MSG_IN) {
			/* receive extended message */
			GLM_SETUP_SCRIPT(glm, unit);
			GLM_START_SCRIPT(glm, NSS_EXT_MSG_IN);
			goto done;
		}

		if (action & NACTION_MSG_REJECT) {
			/* target sent me bad msg, send msg reject */
			dsap->nt_errmsgbuf[0] = (uchar_t)MSG_REJECT;
			GLM_START_SCRIPT(glm, NSS_ERR_MSG);
			goto done;
		}

		if (action & NACTION_MSG_PARITY) {
			/* got a parity error during message in phase */
			dsap->nt_errmsgbuf[0] = (uchar_t)MSG_MSG_PARITY;
			GLM_START_SCRIPT(glm, NSS_ERR_MSG);
			goto done;
		}

		if (action & NACTION_INITIATOR_ERROR) {
			/* catchall for other errors */
			dsap->nt_errmsgbuf[0] = (uchar_t)
				MSG_INITIATOR_ERROR;
			GLM_START_SCRIPT(glm, NSS_ERR_MSG);
			goto done;
		}
	} else if ((unit->nt_state & NPT_STATE_ACTIVE) == 0) {
		NDBG1(("glm_restart_current: idle"));
		/*
		 * a target wants to reconnect so make
		 * it the currently active target
		 */
		GLM_SETUP_SCRIPT(glm, unit);
		GLM_START_SCRIPT(glm, NSS_CLEAR_ACK);
	}
done:
	unit->nt_state |= NPT_STATE_ACTIVE;
	NDBG1(("glm_restart_current: okay"));
}

static void
glm_restart_hba(glm_t *glm, ulong_t action)
{
	NDBG1(("glm_restart_hba"));

	/*
	 * run the target at the front of the queue unless we're
	 * just waiting for a reconnect. In which case just use
	 * the first target's data structure since it's handy.
	 */
	switch (glm->g_state) {
	case NSTATE_ACTIVE:
		NDBG1(("glm_restart_hba: ACTIVE"));
		glm_restart_current(glm, action);
		break;

	case NSTATE_WAIT_RESEL:
		NDBG1(("glm_restart_hba: WAIT"));
		glm_wait_for_reselect(glm, action);
		break;

	case NSTATE_IDLE:
		NDBG1(("glm_restart_hba: IDLE"));
		/* start whatever's on the top of the queue */
		glm_start_next(glm);
		break;
	}
}

/*
 * Save the scatter/gather current-index and number-completed
 * values so when the target reconnects we can restart the
 * data in/out move instruction at the proper point. Also, if the
 * disconnect happened within a segment there's a fixup value
 * for the partially completed data in/out move instruction.
 */
static void
glm_sg_update(glm_unit_t *unit, uchar_t index, ulong_t remain)
{
	glmti_t	*sgp;
	struct glm_dsa *dsap = unit->nt_dsap;
	struct glm_scsi_cmd *cmd = unit->nt_ncmdp;

	NDBG17(("glm_sg_update: unit=%x index=%x remain=%x",
		unit, index, remain));
	/*
	 * Record the number of segments left to do.
	 */
	cmd->cmd_cur_cookie = index;

	/*
	 * If interrupted between segments then don't adjust S/G table
	 */
	if (remain == 0) {
		/*
		 * Must have just completed the current segment when
		 * the interrupt occurred, restart at the next segment.
		 */
		cmd->cmd_cur_cookie--;
		return;
	}

	/*
	 * index is zero based, so to index into the
	 * scatter/gather list subtract one.
	 */
	index = (cmd->cmd_cookiec - index);

	/*
	 * Fixup the Table Indirect entry for this segment.
	 */
	sgp = &dsap->nt_data[index];

	sgp->address += (sgp->count - remain);
	sgp->count = remain;

	NDBG17(("glm_sg_update: remain=%d", remain));
	NDBG17(("Total number of bytes to transfer was %d", sgp->count));
	NDBG17(("at address=0x%x", sgp->address));
}

/*
 * Determine if the command completed with any bytes leftover
 * in the Scatter/Gather DMA list.
 */
static ulong_t
glm_sg_residual(glm_unit_t *unit, struct glm_scsi_cmd *cmd)
{
	glmti_t	*sgp;
	ulong_t	 residual = 0;
	int	 index;

	/*
	 * Get the current index into the sg table.
	 */
	index = (GLM_MAX_DMA_SEGS - cmd->cmd_cur_cookie);

	NDBG17(("glm_sg_residual: unit=%x index=%d", unit, index));

	sgp = &unit->nt_dsap->nt_data[index];

	for (; index < GLM_MAX_DMA_SEGS; index++, sgp++)
		residual += sgp->count;

	NDBG17(("glm_sg_residual: residual=%d", residual));

	return (residual);
}

static void
glm_queue_target(glm_t *glm, glm_unit_t *unit)
{
	NDBG1(("glm_queue_target"));

	glm_addbq(glm, unit);

	switch (glm->g_state) {
	case NSTATE_IDLE:
		/* the device is idle, start first queue entry now */
		glm_restart_hba(glm, 0);
		break;
	case NSTATE_ACTIVE:
		/* queue the target and return without doing anything */
		break;
	case NSTATE_WAIT_RESEL:
		/*
		 * If we're waiting for reselection of a disconnected target
		 * then set the Signal Process bit in the ISTAT register and
		 * return. The interrupt routine restarts the queue.
		 */
		GLM_SET_SIGP(glm);
		break;
	}
}

static glm_unit_t *
glm_get_target(glm_t *glm)
{
	uchar_t target, lun;

	NDBG1(("glm_get_target"));

	/*
	 * Get the LUN from the IDENTIFY message byte
	 */
	lun = glm->g_dsa->g_msginbuf[0];

	if (IS_IDENTIFY_MSG(lun) == FALSE)
		return (NULL);

	lun = lun & MSG_LUNRTN;

	/*
	 * Get the target from the HBA's id register
	 */
	if (GLM_GET_TARGET(glm, &target))
		return (NTL2UNITP(glm, target, lun));

	return (NULL);
}

/* add unit to the front of queue */
static void
glm_addfq(glm_t	*glm, glm_unit_t *unit)
{
	NDBG7(("glm_addfq: unit=%x", unit));

	if (unit->nt_state & NPT_STATE_QUEUED) {
#ifdef GLM_DEBUG
		if (glm_hbaq_check(glm, unit) == FALSE) {
			glm_log(glm, CE_WARN,
			    "glm_addfq: not queued, but claims it is.\n");
		}
#endif
		return;
	}

	/* See if it's already in the queue */
	if (unit->nt_linkp != NULL || unit == glm->g_backp ||
	    unit == glm->g_forwp)
		glm_log(glm, CE_PANIC, "glm_addfq: queue botch");

	if ((unit->nt_linkp = glm->g_forwp) == NULL)
		glm->g_backp = unit;
	glm->g_forwp = unit;

	unit->nt_state |= NPT_STATE_QUEUED;
}

/* add unit to the back of queue */
static void
glm_addbq(glm_t	*glm, glm_unit_t *unit)
{
	NDBG7(("glm_addbq: unit=%x", unit));

	/*
	 * The target is already queued, just return.
	 */
	if (unit->nt_state & NPT_STATE_QUEUED) {
#ifdef GLM_DEBUG
		if (glm_hbaq_check(glm, unit) == FALSE) {
			glm_log(glm, CE_WARN,
			    "glm_addbq: not queued, but claims it is.\n");
		}
#endif
		return;
	}

	unit->nt_linkp = NULL;

	if (glm->g_forwp == NULL)
		glm->g_forwp = unit;
	else
		glm->g_backp->nt_linkp = unit;

	glm->g_backp = unit;

	unit->nt_state |= NPT_STATE_QUEUED;
}

#ifdef GLM_DEBUG
static int
glm_hbaq_check(struct glm *glm, struct glm_unit *unit)
{
	struct glm_unit *up = glm->g_forwp;
	int rval = FALSE;
	int cnt = 0;
	int tot_cnt = 0;

	while (up) {
		if (up == unit) {
			rval = TRUE;
			cnt++;
		}
		up = up->nt_linkp;
		tot_cnt++;
	}
	if (cnt > 1) {
		glm_log(glm, CE_WARN,
		    "total queued=%d: target %d- on hba q %d times.\n",
			tot_cnt, unit->nt_target, cnt);
	}
	return (rval);
}
#endif

/*
 * These routines manipulate the queue of commands that
 * are waiting for their completion routines to be called.
 * The queue is usually in FIFO order but on an MP system
 * it's possible for the completion routines to get out
 * of order. If that's a problem you need to add a global
 * mutex around the code that calls the completion routine
 * in the interrupt handler.
 */
static void
glm_doneq_add(struct glm *glm, struct glm_scsi_cmd *cmd)
{
	struct scsi_pkt *pkt = CMD2PKT(cmd);

	NDBG7(("glm_doneq_add: cmd=%x", cmd));

	/*
	 * If this target did not respond to selection, remove the
	 * unit/dsa memory resources.  If the target comes back
	 * later, we will build these structures on the fly.
	 *
	 * Also, don't remove HBA's unit/dsa structure if the hba
	 * was probed.
	 */
	if (pkt->pkt_reason == CMD_INCOMPLETE &&
	    (Tgt(cmd) != glm->g_glmid)) {
		glm_tear_down_unit_dsa(glm, Tgt(cmd), Lun(cmd));
	}

#ifdef GLM_DEBUG
	/*
	 * If this cmd ran and completed, make sure its marked
	 * as finished and removed.
	 */
	if (cmd->cmd_flags & CFLAG_FINISHED) {
		ASSERT(cmd->cmd_flags & CFLAG_CMD_REMOVED);
	}
#endif

	ASSERT((cmd->cmd_flags & CFLAG_COMPLETED) == 0);
	cmd->cmd_linkp = NULL;
	*glm->g_donetail = cmd;
	glm->g_donetail = &cmd->cmd_linkp;
	cmd->cmd_flags |= CFLAG_COMPLETED;
	cmd->cmd_flags &= ~CFLAG_IN_TRANSPORT;
}

static ncmd_t *
glm_doneq_rm(glm_t *glm)
{
	ncmd_t	*cmdp;
	NDBG7(("glm_doneq_rm"));

	/* pop one off the done queue */
	if ((cmdp = glm->g_doneq) != NULL) {
		/* if the queue is now empty fix the tail pointer */
		if ((glm->g_doneq = cmdp->cmd_linkp) == NULL)
			glm->g_donetail = &glm->g_doneq;
		cmdp->cmd_linkp = NULL;
	}
	return (cmdp);
}

static void
glm_doneq_empty(glm_t *glm)
{
	if (glm->g_doneq) {
		/*
		 * run the completion routines of all the
		 * completed commands
		 */
		ncmd_t *cmd;
		struct scsi_pkt *pkt;
		while ((cmd = glm_doneq_rm(glm)) != NULL) {
			/* run this command's completion routine */
			pkt = CMD2PKT(cmd);
			mutex_exit(&glm->g_mutex);
			if (pkt->pkt_comp)
				(*pkt->pkt_comp)(pkt);
			mutex_enter(&glm->g_mutex);
		}
	}
}

/*
 * These routines manipulate the target's queue of pending requests
 */
static void
glm_waitq_add(glm_unit_t *unit, ncmd_t *cmdp)
{
	NDBG7(("glm_waitq_add: cmd=%x", cmdp));

	cmdp->cmd_queued = TRUE;
	cmdp->cmd_linkp = NULL;
	*(unit->nt_waitqtail) = cmdp;
	unit->nt_waitqtail = &cmdp->cmd_linkp;
}

static void
glm_waitq_add_lifo(glm_unit_t *unit, ncmd_t *cmdp)
{
	NDBG7(("glm_waitq_add_lifo: cmd=%x", cmdp));

	cmdp->cmd_queued = TRUE;
	if ((cmdp->cmd_linkp = unit->nt_waitq) == NULL) {
		unit->nt_waitqtail = &cmdp->cmd_linkp;
	}
	unit->nt_waitq = cmdp;
}

static ncmd_t *
glm_waitq_rm(glm_unit_t *unit)
{
	ncmd_t *cmdp;
	NDBG7(("glm_waitq_rm: unit=%x", unit));

	GLM_WAITQ_RM(unit, cmdp);

	NDBG7(("glm_waitq_rm: unit=0x%x cmdp=0x%x", unit, cmdp));

	return (cmdp);
}

/*
 * remove specified cmd from the middle of the unit's wait queue.
 */
static void
glm_waitq_delete(glm_unit_t *unit, ncmd_t *cmdp)
{
	ncmd_t	*prevp = unit->nt_waitq;

	NDBG7(("glm_waitq_delete: unit=%x cmd=%x", unit, cmdp));

	if (prevp == cmdp) {
		if ((unit->nt_waitq = cmdp->cmd_linkp) == NULL)
			unit->nt_waitqtail = &unit->nt_waitq;

		cmdp->cmd_linkp = NULL;
		cmdp->cmd_queued = FALSE;
		NDBG7(("glm_waitq_delete: unit=0x%x cmdp=0x%x",
		unit, cmdp));
		return;
	}

	while (prevp != NULL) {
		if (prevp->cmd_linkp == cmdp) {
			if ((prevp->cmd_linkp = cmdp->cmd_linkp) == NULL)
				unit->nt_waitqtail = &prevp->cmd_linkp;

			cmdp->cmd_linkp = NULL;
			cmdp->cmd_queued = FALSE;
			NDBG7(("glm_waitq_delete: unit=0x%x cmdp=0x%x",
				unit, cmdp));
			return;
		}
		prevp = prevp->cmd_linkp;
	}
	cmn_err(CE_PANIC, "glm: glm_waitq_delete: queue botch");
}

static void
glm_hbaq_delete(struct glm *glm, struct glm_unit *unit)
{
	struct glm_unit *up = glm->g_forwp;
	int cnt = 0;

	if (unit == glm->g_forwp) {
		cnt++;
		if ((glm->g_forwp = unit->nt_linkp) == NULL) {
			glm->g_backp = NULL;
		}
		unit->nt_state &= ~NPT_STATE_QUEUED;
		unit->nt_linkp = NULL;
	}
	while (up != NULL) {
		if (up->nt_linkp == unit) {
			ASSERT(cnt == 0);
			cnt++;
			if ((up->nt_linkp = unit->nt_linkp) == NULL) {
				glm->g_backp = up;
			}
			unit->nt_linkp = NULL;
			unit->nt_state &= ~NPT_STATE_QUEUED;
		}
		up = up->nt_linkp;
	}
	if (cnt > 1) {
		glm_log(glm, CE_NOTE,
		    "glm_hbaq_delete: target %d- was on hbaq %d times.\n",
			unit->nt_target, cnt);
	}
}

/*
 * synchronous xfer & negotiation handling
 *
 * establish a new sync i/o state for all the luns on a target
 */
static void
glm_syncio_state(glm_t *glm, glm_unit_t *unit, uchar_t state, uchar_t sxfer,
	uchar_t sscfX10)
{
	uint16_t target = unit->nt_target;
	uint16_t lun;
	uint8_t scntl3 = 0;

	NDBG31(("glm_syncio_state: unit=%x state=%x sxfer=%x sscfX10=%x",
		unit, state, sxfer, sscfX10));

	/*
	 * Change state of all LUNs on this target.  We may be responding
	 * to a target initiated sdtr and we may not be using syncio.
	 * We don't want to change state of the target, but we do need
	 * to respond to the target's requestion for sdtr per the scsi spec.
	 */
	if (NSYNCSTATE(glm, unit) != NSYNC_SDTR_REJECT) {
		NSYNCSTATE(glm, unit) = state;
	}

	/*
	 * Set sync i/o clock divisor in SCNTL3 registers
	 */
	if (sxfer != 0) {
		switch (sscfX10) {
		case 10:
			scntl3 = (NB_SCNTL3_SCF1 | glm->g_scntl3);
			break;

		case 15:
			scntl3 = (NB_SCNTL3_SCF15 | glm->g_scntl3);
			break;

		case 20:
			scntl3 = (NB_SCNTL3_SCF2 | glm->g_scntl3);
			break;

		case 30:
			scntl3 = (NB_SCNTL3_SCF3 | glm->g_scntl3);
			break;
		}
	}

	for (lun = 0; lun < NLUNS_PER_TARGET; lun++) {
		/* store new sync i/o parms in each per-target-struct */
		if ((unit = NTL2UNITP(glm, target, lun)) != NULL) {
			unit->nt_dsap->nt_selectparm.nt_sxfer = sxfer;
			unit->nt_sscfX10 = sscfX10;
			unit->nt_dsap->nt_selectparm.nt_scntl3 |= scntl3;
		}
	}

	glm->g_dsa->g_reselectparm[target].g_sxfer = sxfer;
	glm->g_dsa->g_reselectparm[target].g_scntl3 |= scntl3;
}

static void
glm_syncio_disable(glm_t *glm)
{
	ushort	 target;

	NDBG31(("glm_syncio_disable: devaddr=0x%x", glm->g_devaddr));

	for (target = 0; target < NTARGETS_WIDE; target++)
		glm->g_syncstate[target] = NSYNC_SDTR_REJECT;
}

static void
glm_syncio_reset_target(glm_t *glm, int target)
{
	glm_unit_t *unit;
	ushort_t lun;

	NDBG31(("glm_syncio_reset_target: target=%x", target));

	/* check if sync i/o negotiation disabled on this target */
	if (target == glm->g_glmid)
		glm->g_syncstate[target] = NSYNC_SDTR_REJECT;
	else if (glm->g_syncstate[target] != NSYNC_SDTR_REJECT)
		glm->g_syncstate[target] = NSYNC_SDTR_NOTDONE;

	for (lun = 0; lun < NLUNS_PER_TARGET; lun++) {
		if ((unit = NTL2UNITP(glm, target, lun)) != NULL) {
			/* byte assignment */
			unit->nt_dsap->nt_selectparm.nt_sxfer = 0;
		}
	}
	glm->g_dsa->g_reselectparm[target].g_sxfer = 0;
}

static void
glm_syncio_reset(glm_t *glm, glm_unit_t *unit)
{
	ushort	target;

	NDBG31(("glm_syncio_reset: devaddr=0x%x", glm->g_devaddr));

	if (unit != NULL) {
		/* only reset the luns on this target */
		glm_syncio_reset_target(glm, unit->nt_target);
		return;
	}

	/* set the max offset to zero to disable sync i/o */
	for (target = 0; target < NTARGETS_WIDE; target++) {
		glm_syncio_reset_target(glm, target);
	}
}

static void
glm_syncio_msg_init(glm_t *glm, glm_unit_t *unit)
{
	struct glm_dsa *dsap;
	uint_t msgcount;
	uchar_t target = unit->nt_target;
	uchar_t period = (glm->g_hba_period/4);
	uchar_t offset;

	NDBG31(("glm_syncio_msg_init: unit=%x", unit));

	dsap = unit->nt_dsap;
	msgcount = dsap->nt_sendmsg.count;

	/*
	 * Use target's period not the period of the hba if
	 * this target experienced a sync backoff.
	 */
	if (glm->g_backoff & (1<<unit->nt_target)) {
		period = (glm->g_minperiod[unit->nt_target]/4);
	}

	/*
	 * sanity check of period and offset
	 */
	if (glm->g_target_scsi_options[target] & SCSI_OPTIONS_FAST20) {
		if (period < (uchar_t)(DEFAULT_FAST20SYNC_PERIOD/4)) {
			period = (uchar_t)(DEFAULT_FAST20SYNC_PERIOD/4);
		}
	} else if (glm->g_target_scsi_options[target] & SCSI_OPTIONS_FAST) {
		if (period < (uchar_t)(DEFAULT_FASTSYNC_PERIOD/4)) {
			period = (uchar_t)(DEFAULT_FASTSYNC_PERIOD/4);
		}
	} else {
		if (period < (uchar_t)(DEFAULT_SYNC_PERIOD/4)) {
			period = (uchar_t)(DEFAULT_SYNC_PERIOD/4);
		}
	}

	if (glm->g_target_scsi_options[target] & SCSI_OPTIONS_SYNC) {
		offset = glm->g_sync_offset;
	} else {
		offset = 0;
	}

	dsap->nt_msgoutbuf[msgcount++] = (uchar_t)MSG_EXTENDED;
	dsap->nt_msgoutbuf[msgcount++] = (uchar_t)3;
	dsap->nt_msgoutbuf[msgcount++] = (uchar_t)MSG_SYNCHRONOUS;
	dsap->nt_msgoutbuf[msgcount++] = period;
	dsap->nt_msgoutbuf[msgcount++] = offset;

	dsap->nt_sendmsg.count = msgcount;
}

/*
 * glm sent a sdtr to a target and the target responded.  Find out
 * the offset and sync period and enable sync scsi if needed.
 *
 * called from: glm_syncio_decide.
 */
static int
glm_syncio_enable(glm_t *glm, glm_unit_t *unit)
{
	uchar_t sxfer;
	uchar_t sscfX10;
	int time_ns;
	uchar_t offset;
	struct glm_dsa *dsap;

	NDBG31(("glm_syncio_enable: unit=%x"));

	dsap = unit->nt_dsap;

	/*
	 * units for transfer period factor are 4 nsec.
	 *
	 * These two values are the sync period and offset
	 * the target responded with.
	 */
	time_ns = (dsap->nt_syncibuf[1] * 4);
	offset = dsap->nt_syncibuf[2];

	/*
	 * IF this target is FAST-20 capable use the round
	 * value of 50 ns.
	 */
	if (dsap->nt_syncibuf[1] == (DEFAULT_FAST20SYNC_PERIOD/4)) {
		time_ns = DEFAULT_FAST20SYNC_PERIOD;
	}

	/*
	 * If the target returns a zero offset, go async.
	 */
	if (offset == 0) {
		glm_syncio_state(glm, unit, NSYNC_SDTR_DONE, 0, 0);
		return (TRUE);
	}

	/*
	 * Check the period returned by the target.  Target shouldn't
	 * try to decrease my period
	 */
	if ((time_ns < CONVERT_PERIOD(glm->g_speriod)) ||
	    !glm_max_sync_divisor(glm, time_ns, &sxfer, &sscfX10)) {
		NDBG31(("glm_syncio_enable: invalid period: %d", time_ns));
		return (FALSE);
	}

	/*
	 * check the offset returned by the target.
	 */
	if (offset > glm->g_sync_offset) {
		NDBG31(("glm_syncio_enable: invalid offset=%d", offset));
		return (FALSE);
	}

	/* encode the divisor and offset values */
	sxfer = (((sxfer - 4) << 5) + offset);

	unit->nt_fastscsi = (time_ns < 200) ? TRUE : FALSE;

	/*
	 * Enable FAST-20 timing in the chip for this target.
	 */
	if (time_ns == DEFAULT_FAST20SYNC_PERIOD) {
		unit->nt_dsap->nt_selectparm.nt_scntl3 |= NB_SCNTL3_FAST20;
		glm->g_dsa->g_reselectparm[unit->nt_target].g_scntl3 |=
		    NB_SCNTL3_FAST20;
	}

	/* set the max offset and clock divisor for all LUNs on this target */
	NDBG31(("glm_syncio_enable: target=%d sxfer=%x, sscfX10=%d",
		unit->nt_target, sxfer, sscfX10));

	glm->g_minperiod[unit->nt_target] = time_ns;

	glm_syncio_state(glm, unit, NSYNC_SDTR_DONE, sxfer, sscfX10);
	glm->g_props_update |= (1<<unit->nt_target);
	return (TRUE);
}


/*
 * The target started the synchronous i/o negotiation sequence by
 * sending me a SDTR message. Look at the target's parms and the
 * HBA's defaults and decide on the appropriate comprise. Send the
 * larger of the two transfer periods and the smaller of the two offsets.
 */
static int
glm_syncio_respond(glm_t *glm, glm_unit_t *unit)
{
	uchar_t	sxfer;
	uchar_t	sscfX10;
	int	time_ns;
	uchar_t offset;
	struct glm_dsa *dsap;
	ushort_t targ = unit->nt_target;

	NDBG31(("glm_syncio_respond: unit=%x", unit));

	dsap = unit->nt_dsap;

	/*
	 * Use the smallest offset
	 */
	offset = dsap->nt_syncibuf[2];

	if ((glm->g_syncstate[targ] == NSYNC_SDTR_REJECT) ||
	    ((glm->g_target_scsi_options[targ] & SCSI_OPTIONS_SYNC) == 0)) {
		offset = 0;
	}

	if (offset > glm->g_sync_offset)
		offset = glm->g_sync_offset;

	/*
	 * units for transfer period factor are 4 nsec.
	 */
	time_ns = (dsap->nt_syncibuf[1] * 4);

	/*
	 * If target is FAST-20 capable use the round value
	 * of 50 instead of 48.
	 */
	if (dsap->nt_syncibuf[1] == (DEFAULT_FAST20SYNC_PERIOD/4)) {
		time_ns = DEFAULT_FAST20SYNC_PERIOD;
	}

	if (glm->g_backoff & (1<<targ)) {
		time_ns = glm->g_minperiod[targ];
	}

	/*
	 * Use largest time period.
	 */
	if (time_ns < glm->g_hba_period) {
		time_ns = glm->g_hba_period;
	}

	/*
	 * Target has requested a sync period slower than
	 * our max.  respond with our max sync rate.
	 */
	if (time_ns > MAX_SYNC_PERIOD(glm)) {
		time_ns = MAX_SYNC_PERIOD(glm);
	}

	if (!glm_max_sync_divisor(glm, time_ns, &sxfer, &sscfX10)) {
		NDBG31(("glm_syncio_respond: invalid period: %d,%d",
		    time_ns, offset));
		return (FALSE);
	}

	sxfer = (((sxfer - 4) << 5) + offset);

	/* set the max offset and clock divisor for all LUNs on this target */
	glm_syncio_state(glm, unit, NSYNC_SDTR_DONE, sxfer, sscfX10);

	/* report to target the adjusted period */
	if ((time_ns = glm_period_round(glm, time_ns)) == -1) {
		NDBG31(("glm_syncio_respond: round failed time=%d",
		    time_ns));
		return (FALSE);
	}

	glm->g_minperiod[targ] = time_ns;

	dsap->nt_msgoutbuf[0] = 0x01;
	dsap->nt_msgoutbuf[1] = 0x03;
	dsap->nt_msgoutbuf[2] = 0x01;
	dsap->nt_msgoutbuf[3] = (time_ns / 4);
	dsap->nt_msgoutbuf[4] = (uchar_t)offset;
	dsap->nt_sendmsg.count = 5;

	unit->nt_fastscsi = (time_ns < 200) ? TRUE : FALSE;

	/*
	 * Enable FAST-20 bit.
	 */
	if (time_ns == DEFAULT_FAST20SYNC_PERIOD) {
		unit->nt_dsap->nt_selectparm.nt_scntl3 |= NB_SCNTL3_FAST20;
		glm->g_dsa->g_reselectparm[targ].g_scntl3 |=
		    NB_SCNTL3_FAST20;
	}

	glm->g_props_update |= (1<<targ);

	return (TRUE);
}

static ulong_t
glm_syncio_decide(glm_t *glm, glm_unit_t *unit, ulong_t action)
{
	NDBG31(("glm_syncio_decide: unit=%x action=%x", unit, action));

	if (action & (NACTION_SIOP_HALT | NACTION_SIOP_REINIT
			| NACTION_BUS_FREE)) {
		/* set all LUNs on this target to renegotiate syncio */
		glm_syncio_reset(glm, unit);
		return (action);
	}

	if (action & (NACTION_DONE | NACTION_ERR)) {
		/* the target finished without responding to SDTR */
		/* set all LUN's on this target to async mode */
		glm_syncio_state(glm, unit, NSYNC_SDTR_DONE, 0, 0);
		return (action);
	}

	if (action & (NACTION_MSG_PARITY | NACTION_INITIATOR_ERROR)) {
		/* allow target to try to do error recovery */
		return (action);
	}

	if ((action & NACTION_SDTR) == 0) {
		return (action);
	}

	/* if got good SDTR response, enable sync i/o */
	switch (NSYNCSTATE(glm, unit)) {
	case NSYNC_SDTR_SENT:
		if (glm_syncio_enable(glm, unit)) {
			/* reprogram the sxfer register */
			GLM_SET_SYNCIO(glm, unit);
			action |= NACTION_ACK;
		} else {
			/*
			 * The target sent us bogus sync msg.
			 * reject this msg. Disallow sync.
			 */
			NSYNCSTATE(glm, unit) = NSYNC_SDTR_DONE;
			action |= NACTION_MSG_REJECT;
		}
		return (action);

	case NSYNC_SDTR_RCVD:
	case NSYNC_SDTR_REJECT:
		/*
		 * if target initiated SDTR handshake, send sdtr.
		 */
		if (glm_syncio_respond(glm, unit)) {
			/* reprogram the sxfer register */
			GLM_SET_SYNCIO(glm, unit);
			return (NACTION_EXT_MSG_OUT | action);
		}
		break;

	case NSYNC_SDTR_NOTDONE:
		unit->nt_dsap->nt_sendmsg.count = 0;
		glm_syncio_msg_init(glm, unit);
		glm_syncio_state(glm, unit, NSYNC_SDTR_SENT, 0, 0);
		return (NACTION_EXT_MSG_OUT | action);
	}

	/* target and hba counldn't agree on sync i/o parms, so */
	/* set all LUN's on this target to async mode until */
	/* next bus reset */
	glm_syncio_state(glm, unit, NSYNC_SDTR_DONE, 0, 0);
	return (NACTION_MSG_REJECT | action);
}

/*
 * The chip uses a two stage divisor chain. The first stage can
 * divide by 1, 1.5, 2, 3, or 4.  The second stage can divide by values
 * from 4 to 11 (inclusive). If the board is using a 40MHz clock this
 * allows sync i/o rates  from 10 MB/sec to 1.212 MB/sec. The following
 * table factors a desired overall divisor into the appropriate values
 * for each of the two stages.	The first and third columns of this
 * table are scaled by a factor of 10 to handle the 1.5 case without
 * using floating point numbers.
 */
typedef	struct glm_divisor_table {
	int	divisorX10;	/* divisor times 10 */
	uchar_t	sxfer;		/* sxfer period divisor */
	uchar_t	sscfX10;	/* synchronous scsi clock divisor times 10 */
} ndt_t;

static	ndt_t	DivisorTable[] = {
	{ 40,	4,	10 },
	{ 50,	5,	10 },
	{ 60,	4,	15 },
	{ 70,	7,	10 },
	{ 75,	5,	15 },
	{ 80,	4,	20 },
	{ 90,	6,	15 },
	{ 100,	5,	20 },
	{ 105,	7,	15 },
	{ 110,	11,	10 },
	{ 120,	8,	15 },
	{ 135,	9,	15 },
	{ 140,	7,	20 },
	{ 150,	10,	15 },
	{ 160,	8,	20 },
	{ 165,	11,	15 },
	{ 180,	9,	20 },
	{ 200,	10,	20 },
	{ 210,	7,	30 },
	{ 220,	11,	20 },
	{ 240,	8,	30 },
	{ 270,	9,	30 },
	{ 300,	10,	30 },
	{ 330,	11,	30 },
	0
};

/*
 * Find the clock divisor which gives a period that is at least
 * as long as syncioperiod. If an divisor can't be found that
 * gives the exactly desired syncioperiod then the divisor which
 * results in the next longer valid period will be returned.
 *
 * In the above divisor lookup table the periods and divisors
 * are scaled by a factor of ten to handle the .5 fractional values.
 * I could have just scaled everything by a factor of two but I
 * think x10 is easier to understand and easier to setup.
 */
static int
glm_max_sync_divisor(glm_t *glm, int syncioperiod, uchar_t *sxferp,
    uchar_t *sscfX10p)
{
	ndt_t *dtp;
	int divX100;

	NDBG31(("glm_max_sync_divisor: period=%x sxferp=%x sscfX10p=%x",
		syncioperiod, *sxferp, *sscfX10p));

	divX100 = (syncioperiod * 100);
	divX100 /= glm->g_speriod;

	for (dtp = DivisorTable; dtp->divisorX10 != 0; dtp++) {
		if (dtp->divisorX10 >= divX100) {
			goto got_it;
		}
	}
	NDBG31(("glm_max_sync_divisor: period=%x", syncioperiod));
	return (FALSE);

got_it:
	*sxferp = dtp->sxfer;
	*sscfX10p = dtp->sscfX10;
	NDBG31(("glm_max_sync_divisor gotit: period=%x sxferp=%x sscfX10p=%x",
		syncioperiod, *sxferp, *sscfX10p));
	return (TRUE);
}

static int
glm_period_round(glm_t *glm, int syncioperiod)
{
	int	clkperiod;
	uchar_t	sxfer;
	uchar_t	sscfX10;
	int	tmp;

	NDBG31(("glm_period_round: period=%x", syncioperiod));

	if (glm_max_sync_divisor(glm, syncioperiod, &sxfer, &sscfX10)) {
		clkperiod = glm->g_speriod;

		switch (sscfX10) {
		case 10:
			/* times 1 */
			tmp = (clkperiod * sxfer);
			return (tmp/10);
		case 15:
			/* times 1.5 */
			tmp = (15 * clkperiod * sxfer);
			return ((tmp + 5) / 100);
		case 20:
			/* times 2 */
			tmp = (2 * clkperiod * sxfer);
			return (tmp/10);
		case 30:
			/* times 3 */
			tmp = (3 * clkperiod * sxfer);
			return (tmp/10);
		}
	}
	return (-1);
}

/*
 * Determine frequency of the HBA's clock chip and determine what
 * rate to use for synchronous i/o on each target. Because of the
 * way the chip's divisor chain works it's only possible to achieve
 * timings that are integral multiples of the clocks fundamental
 * period.
 */
static void
glm_max_sync_rate_init(glm_t *glm)
{
	int i;
	static char *prop_cfreq = "clock-frequency";
	int period;

	NDBG31(("glm_max_sync_rate_init"));

	/*
	 * Determine clock frequency of attached Symbios chip.
	 */
	i = ddi_prop_get_int(DDI_DEV_T_ANY, glm->g_dip,
		DDI_PROP_DONTPASS, prop_cfreq, (40 * MEG));
	glm->g_sclock = (i/(MEG));

	/*
	 * If we have a Rev 2 (or greater) 875, double the clock
	 * and enable FAST-20.
	 */
	if (glm->g_devid == GLM_53c875 && (GLM_REV(glm) > REV1) &&
	    ((glm->g_scsi_options & SCSI_OPTIONS_FAST20) != 0)) {
		ClrSetBits(glm->g_devaddr + NREG_STEST1, 0, NB_STEST1_DBLEN);
		drv_usecwait(20);
		ClrSetBits(glm->g_devaddr + NREG_STEST3, 0, NB_STEST3_HSC);
		ClrSetBits(glm->g_devaddr + NREG_STEST1, 0, NB_STEST1_DBLSEL);
		ClrSetBits(glm->g_devaddr + NREG_STEST3, NB_STEST3_HSC, 0);
		glm->g_sclock *= 2;
	}

	/*
	 * calculate the fundamental period in nanoseconds.
	 *
	 * FAST SCSI = 250. (25ns * 10)
	 * FAST-20 = 125. (12.5ns * 10)
	 *
	 * This is needed so that for FAST-20 timings, we don't
	 * lose the .5.
	 */
	glm->g_speriod = (10000 / glm->g_sclock);

	/*
	 * Round max sync rate to the closest value the hba's
	 * divisor chain can produce.
	 *
	 * equation for CONVERT_PERIOD:
	 *
	 * For FAST SCSI: ((250 << 2) / 10) = 100ns.
	 * For FAST-20	: ((125 << 2) / 10) =  50ns.
	 */
	if ((glm->g_hba_period = period =
	    glm_period_round(glm, CONVERT_PERIOD(glm->g_speriod))) <= 0) {
		glm_syncio_disable(glm);
		return;
	}

	/*
	 * Set each target to the correct period.
	 */
	for (i = 0; i < NTARGETS_WIDE; i++) {
		if (glm->g_target_scsi_options[i] & SCSI_OPTIONS_FAST20 &&
		    period == DEFAULT_FAST20SYNC_PERIOD) {
			glm->g_minperiod[i] = DEFAULT_FAST20SYNC_PERIOD;
		} else if (glm->g_target_scsi_options[i] & SCSI_OPTIONS_FAST) {
			glm->g_minperiod[i] = DEFAULT_FASTSYNC_PERIOD;
		} else {
			glm->g_minperiod[i] = DEFAULT_SYNC_PERIOD;
		}
	}
}

static void
glm_sync_wide_backoff(struct glm *glm, struct glm_unit *unit)
{
	uchar_t target = unit->nt_target;
	ushort_t tshift = (1<<target);

	NDBG31(("glm_sync_wide_backoff: unit=%x", unit));

#ifdef GLM_TEST
	if (glm_no_sync_wide_backoff) {
		return;
	}
#endif
	/*
	 * if this not the first time then disable wide or this
	 * is the first time and sync is already disabled.
	 */
	if (glm->g_backoff & tshift ||
	    (unit->nt_dsap->nt_selectparm.nt_sxfer & 0x1f) == 0) {
		if ((glm->g_nowide & tshift) == 0) {
			glm_log(glm, CE_WARN,
			    "Target %d disabled wide SCSI mode",
				target);
			cmn_err(CE_WARN,
			    "!ID[SUNWpd.glm.sync_wide_backoff.6012]");
		}
		/*
		 * do not reset the bit in g_nowide because that
		 * would not force a renegotiation of wide
		 * and do not change any register value yet because
		 * we may have reconnects before the renegotiations
		 */
		glm->g_target_scsi_options[target] &= ~SCSI_OPTIONS_WIDE;
	}

	if ((unit->nt_dsap->nt_selectparm.nt_sxfer & 0x1f) != 0) {
		if (glm->g_backoff & tshift &&
		    (unit->nt_dsap->nt_selectparm.nt_sxfer & 0x1f)) {
			glm_log(glm, CE_WARN,
			    "Target %d reverting to async. mode", target);
			cmn_err(CE_WARN,
			    "!ID[SUNWpd.glm.sync_wide_backoff.6013]");
			glm->g_target_scsi_options[target] &=
				~(SCSI_OPTIONS_SYNC | SCSI_OPTIONS_FAST);
		} else {
			int period = glm->g_minperiod[target];

			/*
			 * backoff sync 100%.
			 */
			period = (period * 2);

			/*
			 * Backing off sync on slow devices when the 875
			 * is using FAST-20 timings can generate sync
			 * periods that are greater than our max sync.
			 * Adjust up to our max sync.
			 */
			if (period > MAX_SYNC_PERIOD(glm)) {
				period = MAX_SYNC_PERIOD(glm);
			}

			period = glm_period_round(glm, period);

			glm->g_target_scsi_options[target] &=
				~SCSI_OPTIONS_FAST20;

			glm->g_minperiod[target] = period;

			glm_log(glm, CE_WARN,
			    "Target %d reducing sync. transfer rate", target);
			cmn_err(CE_WARN,
			    "!ID[SUNWpd.glm.sync_wide_backoff.6014]");
		}
	}
	glm->g_backoff |= tshift;
	glm->g_props_update |= (1<<target);
	glm_force_renegotiation(glm, target);
}

static void
glm_force_renegotiation(struct glm *glm, int target)
{
	register ushort_t tshift = (1<<target);

	NDBG31(("glm_force_renegotiation: target=%x", target));

	if (glm->g_syncstate[target] != NSYNC_SDTR_REJECT) {
		glm->g_syncstate[target] = NSYNC_SDTR_NOTDONE;
	}
	glm->g_wide_known &= ~tshift;
	glm->g_wide_enabled &= ~tshift;
}

/*
 * wide data xfer negotiation handling
 */
static void
glm_make_wdtr(struct glm *glm, struct glm_unit *unit, uchar_t width)
{
	struct glm_dsa *dsap;
	uint_t msgcount;

	NDBG31(("glm_make_wdtr: unit=%x width=%x", unit, width));

	dsap = unit->nt_dsap;
	msgcount = dsap->nt_sendmsg.count;

	if (((glm->g_target_scsi_options[unit->nt_target] &
	    SCSI_OPTIONS_WIDE) == 0) ||
		(glm->g_nowide & (1<<unit->nt_target))) {
			glm->g_nowide |= (1<<unit->nt_target);
			width = 0;
	}

	width = min(GLM_XFER_WIDTH, width);

	dsap->nt_msgoutbuf[msgcount++] = (uchar_t)MSG_EXTENDED;
	dsap->nt_msgoutbuf[msgcount++] = (uchar_t)2;
	dsap->nt_msgoutbuf[msgcount++] = (uchar_t)MSG_WIDE_DATA_XFER;
	dsap->nt_msgoutbuf[msgcount++] = (uchar_t)width;

	dsap->nt_sendmsg.count = msgcount;

	/*
	 * increment wdtr flag, odd value indicates that we initiated
	 * the negotiation.
	 */
	glm->g_wdtr_sent++;

	/*
	 * the target may reject the optional wide message so
	 * to avoid negotiating on every cmd, set wide known here
	 */
	glm->g_wide_known |= (1<<unit->nt_target);

	glm_set_wide_scntl3(glm, unit, width);
}


static void
glm_set_wide_scntl3(struct glm *glm, struct glm_unit *unit, uchar_t width)
{
	NDBG31(("glm_set_wide_scntl3: unit=%x width=%x", unit, width));

	ASSERT(width <= 1);
	switch (width) {
	case 0:
		unit->nt_dsap->nt_selectparm.nt_scntl3 &= ~NB_SCNTL3_EWS;
		glm->g_dsa->g_reselectparm[unit->nt_target].g_scntl3 &=
		    ~NB_SCNTL3_EWS;
		break;
	case 1:
		/*
		 * The scntl3:NB_SCNTL3_EWS bit controls wide.
		 */
		unit->nt_dsap->nt_selectparm.nt_scntl3 |= NB_SCNTL3_EWS;
		glm->g_dsa->g_reselectparm[unit->nt_target].g_scntl3 |=
		    NB_SCNTL3_EWS;
		glm->g_wide_enabled |= (1<<unit->nt_target);
		break;
	}
	ddi_put8(glm->g_datap, (uint8_t *)(glm->g_devaddr + NREG_SCNTL3),
		unit->nt_dsap->nt_selectparm.nt_scntl3);
}

/*
 * 87x chip handling
 */
static void
glm53c87x_reset(glm_t *glm)
{
	caddr_t	devaddr = glm->g_devaddr;
	ddi_acc_handle_t datap = glm->g_datap;

	NDBG22(("glm53c87x_reset: devaddr=0x%x", devaddr));

	/* Reset the 53c87x chip */
	(void) ddi_put8(datap, (uint8_t *)(devaddr + NREG_ISTAT),
		NB_ISTAT_SRST);

	/* wait a tick and then turn the reset bit off */
	drv_usecwait(100);
	(void) ddi_put8(datap, (uint8_t *)(devaddr + NREG_ISTAT), 0);

	/* clear any pending SCSI interrupts */
	(void) ddi_get8(datap, (uint8_t *)(devaddr + NREG_SIST0));

	/* need short delay before reading SIST1 */
	(void) ddi_get32(datap, (uint32_t *)(devaddr + NREG_SCRATCHA));

	(void) ddi_get32(datap, (uint32_t *)(devaddr + NREG_SCRATCHA));

	(void) ddi_get8(datap, (uint8_t *)(devaddr + NREG_SIST1));

	/* need short delay before reading DSTAT */
	(void) ddi_get32(datap, (uint32_t *)(devaddr + NREG_SCRATCHA));
	(void) ddi_get32(datap, (uint32_t *)(devaddr + NREG_SCRATCHA));

	/* clear any pending DMA interrupts */
	(void) ddi_get8(datap, (uint8_t *)(devaddr + NREG_DSTAT));

	NDBG1(("NCR53c87x: Software reset completed"));
}

static void
glm53c87x_init(glm_t *glm)
{
	caddr_t	devaddr = glm->g_devaddr;
	ddi_acc_handle_t datap = glm->g_datap;
	int	i;

	NDBG0(("glm53c87x_init: devaddr=0x%x", devaddr));

	/* Enable Parity checking and generation */
	ClrSetBits(devaddr + NREG_SCNTL0, 0,
		(NB_SCNTL0_EPC | NB_SCNTL0_AAP));

	/* disable extra clock cycle of data setup so that */
	/* the hba can do 10MB/sec fast scsi */
	ClrSetBits(devaddr + NREG_SCNTL1, NB_SCNTL1_EXC, 0);

	/* Set the HBA's SCSI id, and enable reselects */
	ClrSetBits(devaddr + NREG_SCID, NB_SCID_ENC,
	    ((glm->g_glmid & NB_SCID_ENC) | NB_SCID_RRE));

	/* Disable auto switching */
	ClrSetBits(devaddr + NREG_DCNTL, 0, NB_DCNTL_COM);

	/* set the selection time-out value. */
	ClrSetBits(devaddr + NREG_STIME0, NB_STIME0_SEL,
				(uchar_t)glm_selection_timeout);

	/* Set the scsi id bit to match the HBA's idmask */
	ddi_put16(datap, (uint16_t *)(devaddr + NREG_RESPID),
		(1 << glm->g_glmid));

	/* disable SCSI-1 single initiator option */
	/* enable TolerANT (active negation) */
	ClrSetBits(devaddr + NREG_STEST3, 0, (NB_STEST3_TE | NB_STEST3_DSI));

	/* setup the minimum transfer period (i.e. max transfer rate) */
	/* for synchronous i/o for each of the targets */
	glm_max_sync_rate_init(glm);

	/* set the scsi core divisor */
	if (glm->g_sclock <= 25) {
		glm->g_scntl3 = NB_SCNTL3_CCF1;
	} else if (glm->g_sclock < 38) {
		glm->g_scntl3 = NB_SCNTL3_CCF15;
	} else if (glm->g_sclock <= 50) {
		glm->g_scntl3 = NB_SCNTL3_CCF2;
	} else if (glm->g_sclock <= 75) {
		glm->g_scntl3 = NB_SCNTL3_CCF3;
	} else if (glm->g_sclock <= 80) {
		glm->g_scntl3 = NB_SCNTL3_CCF4;
	}
	ddi_put8(datap, (uint8_t *)(devaddr + NREG_SCNTL3), glm->g_scntl3);

	/*
	 * If this device is the 875, enable scripts prefetching,
	 * PCI read line cmd, large DMA fifo and Cache Line Size Enable.
	 */
	if (glm->g_devid == GLM_53c810) {
		uchar_t dcntl;
		dcntl = ddi_get8(datap, (uint8_t *)(devaddr + NREG_DCNTL));
		dcntl |= NB_DCNTL_CLSE;

		dcntl |= NB_DCNTL_PFEN;
		ddi_put8(datap, (uint8_t *)(devaddr + NREG_DCNTL), dcntl);

		ClrSetBits(devaddr + NREG_DMODE, 0,
			(0x80 | NB_DMODE_ERL | NB_DMODE_BOF));

		/*
		 * 810 does not have fcode, reset the bus here.
		 */
		GLM_BUS_RESET(glm);
		glm_set_throttles(glm, 0, N_GLM_UNITS, HOLD_THROTTLE);
		for (i = 0; i < NTARGETS_WIDE; i++) {
			glm->g_reset_delay[i] = glm->g_scsi_reset_delay;
		}
		glm_start_watch_reset_delay();
	} else if (glm->g_devid == GLM_53c875) {
		uchar_t	dcntl;
		dcntl = ddi_get8(datap, (uint8_t *)(devaddr + NREG_DCNTL));
		dcntl |= NB_DCNTL_CLSE;

		switch (GLM_REV(glm)) {
		case REV1:
		case REV2:
		case REV3:
			break;
		case REV4:
			dcntl |= NB_DCNTL_PFEN;
			break;
		default:
			break;
		}
		ddi_put8(datap, (uint8_t *)(devaddr + NREG_DCNTL), dcntl);

		/*
		 * Set dmode reg for 875
		 */
		ClrSetBits(devaddr + NREG_DMODE, 0,
		    (NB_DMODE_BL | NB_DMODE_ERL | NB_DMODE_BOF));

		/*
		 * This bit along with bits 7&8 of the dmode register are
		 * are used to determine burst size.
		 *
		 * Also enable use of larger dma fifo in the 875.
		 */
		ClrSetBits(devaddr + NREG_CTEST5, 0,
		    (NB_CTEST5_DFS | NB_CTEST5_BL2));
	} else {
		ClrSetBits(devaddr + NREG_DMODE, 0,
		    (NB_825_DMODE_BL | NB_DMODE_ERL));
	}

	NDBG0(("glm53c87x_init: devaddr=0x%x completed", devaddr));
}

static void
glm53c87x_enable(glm_t *glm)
{
	caddr_t	devaddr = glm->g_devaddr;

	NDBG0(("glm53c87x_enable"));

	/* enable all fatal interrupts, disable all non-fatal interrupts */
	ClrSetBits(devaddr + NREG_SIEN0, (NB_SIEN0_CMP | NB_SIEN0_SEL |
	    NB_SIEN0_RSL), (NB_SIEN0_MA | NB_SIEN0_SGE | NB_SIEN0_UDC |
	    NB_SIEN0_RST | NB_SIEN0_PAR));

	/* enable all fatal interrupts, disable all non-fatal interrupts */
	ClrSetBits(devaddr + NREG_SIEN1, (NB_SIEN1_GEN | NB_SIEN1_HTH),
	    NB_SIEN1_STO);

	/* enable all valid except SCRIPT Step Interrupt */
	ClrSetBits(devaddr + NREG_DIEN, NB_DIEN_SSI, (NB_DIEN_MDPE |
	    NB_DIEN_BF | NB_DIEN_ABRT | NB_DIEN_SIR | NB_DIEN_IID));

	/* enable master parity error detection logic */
	ClrSetBits(devaddr + NREG_CTEST4, 0, NB_CTEST4_MPEE);
}

static void
glm53c87x_disable(glm_t *glm)
{
	caddr_t	devaddr = glm->g_devaddr;

	NDBG0(("glm53c87x_disable"));

	/* disable all SCSI interrrupts */
	ClrSetBits(devaddr + NREG_SIEN0, (NB_SIEN0_MA | NB_SIEN0_CMP |
	    NB_SIEN0_SEL | NB_SIEN0_RSL | NB_SIEN0_SGE | NB_SIEN0_UDC |
	    NB_SIEN0_RST | NB_SIEN0_PAR), 0);

	ClrSetBits(devaddr + NREG_SIEN1, (NB_SIEN1_GEN | NB_SIEN1_HTH |
	    NB_SIEN1_STO), 0);

	/* disable all DMA interrupts */
	ClrSetBits(devaddr + NREG_DIEN, (NB_DIEN_MDPE | NB_DIEN_BF |
	    NB_DIEN_ABRT | NB_DIEN_SSI | NB_DIEN_SIR | NB_DIEN_IID), 0);

	/* disable master parity error detection */
	ClrSetBits(devaddr + NREG_CTEST4, NB_CTEST4_MPEE, 0);
}

static uchar_t
glm53c87x_get_istat(glm_t *glm)
{
	NDBG1(("glm53c87x_get_istat"));

	return (ddi_get8(glm->g_datap,
			(uint8_t *)(glm->g_devaddr + NREG_ISTAT)));
}

static void
glm53c87x_halt(glm_t *glm)
{
	caddr_t	devaddr = glm->g_devaddr;
	uchar_t	first_time = TRUE;
	int	loopcnt;
	uchar_t	istat;
	uchar_t	dstat;

	NDBG1(("glm53c87x_halt"));

	/* turn on the abort bit */
	istat = NB_ISTAT_ABRT;
	ddi_put8(glm->g_datap, (uint8_t *)(devaddr + NREG_ISTAT), istat);

	/* wait for and clear all pending interrupts */
	for (;;) {

		/* wait up to 1 sec. for a DMA or SCSI interrupt */
		for (loopcnt = 0; loopcnt < 1000; loopcnt++) {
			istat = glm53c87x_get_istat(glm);
			if (istat & (NB_ISTAT_SIP | NB_ISTAT_DIP))
				goto got_it;

			/* wait 1 millisecond */
			drv_usecwait(1000);
		}
		NDBG10(("glm53c87x_halt: 0x%x: can't halt", devaddr));
		return;

	got_it:
		/* if there's a SCSI interrupt pending clear it and loop */
		if (istat & NB_ISTAT_SIP) {
			/* reset the sip latch registers */
			(void) ddi_get8(glm->g_datap,
				(uint8_t *)(devaddr + NREG_SIST0));

			/* need short delay before reading SIST1 */
			(void) ddi_get32(glm->g_datap,
				(uint32_t *)(devaddr + NREG_SCRATCHA));
			(void) ddi_get32(glm->g_datap,
				(uint32_t *)(devaddr + NREG_SCRATCHA));

			(void) ddi_get8(glm->g_datap,
				(uint8_t *)(devaddr + NREG_SIST1));
			continue;
		}

		if (first_time) {
			/* reset the abort bit before reading DSTAT */
			ddi_put8(glm->g_datap,
				(uint8_t *)(devaddr + NREG_ISTAT), 0);
			first_time = FALSE;
		}
		/* read the DMA status register */
		dstat = ddi_get8(glm->g_datap,
				(uint8_t *)(devaddr + NREG_DSTAT));
		if (dstat & NB_DSTAT_ABRT) {
			/* got the abort interrupt */
			NDBG10(("glm53c87x_halt: devaddr=0x%x: okay",
			    devaddr));
			return;
		}
		/* must have been some other pending interrupt */
		drv_usecwait(1000);
	}
}

/*
 * Utility routine; check for error in execution of command in ccb,
 * handle it.
 */
static void
glm53c87x_check_error(glm_unit_t *unit, struct scsi_pkt *pktp)
{
	NDBG1(("glm53c87x_check_error: pkt=%x", pktp));

	/* store the default error results in packet */
	pktp->pkt_state |= STATE_GOT_BUS;

	if (unit->nt_status0 == 0 && unit->nt_status1 == 0 &&
	    unit->nt_dma_status == 0) {
		NDBG1(("glm53c87x_check_error: A"));
		pktp->pkt_statistics |= STAT_BUS_RESET;
		pktp->pkt_reason = CMD_RESET;
		return;
	}

	if (unit->nt_status1 & NB_SIST1_STO) {
		NDBG1(("glm53c87x_check_error: B"));
		pktp->pkt_state |= STATE_GOT_BUS;
	}
	if (unit->nt_status0 & NB_SIST0_UDC) {
		NDBG1(("glm53c87x_check_error: C"));
		pktp->pkt_state |= (STATE_GOT_BUS | STATE_GOT_TARGET);
		pktp->pkt_statistics = 0;
	}
	if (unit->nt_status0 & NB_SIST0_RST) {
		NDBG1(("glm53c87x_check_error: D"));
		pktp->pkt_state |= STATE_GOT_BUS;
		pktp->pkt_statistics |= STAT_BUS_RESET;
	}
	if (unit->nt_status0 & NB_SIST0_PAR) {
		NDBG1(("glm53c87x_check_error: E"));
		pktp->pkt_statistics |= STAT_PERR;
	}
	if (unit->nt_dma_status & NB_DSTAT_ABRT) {
		NDBG1(("glm53c87x_check_error: F"));
		pktp->pkt_statistics |= STAT_ABORTED;
	}


	/* Determine the appropriate error reason */

	/* watch out, on the 8xx chip the STO bit was moved */
	if (unit->nt_status1 & NB_SIST1_STO) {
		NDBG1(("glm53c87x_check_error: G"));
		pktp->pkt_reason = CMD_INCOMPLETE;
	} else if (unit->nt_status0 & NB_SIST0_UDC) {
		NDBG1(("glm53c87x_check_error: H"));
		pktp->pkt_reason = CMD_UNX_BUS_FREE;
	} else if (unit->nt_status0 & NB_SIST0_RST) {
		NDBG1(("glm53c87x_check_error: I"));
		pktp->pkt_reason = CMD_RESET;
	} else if (unit->nt_status0 & NB_SIST0_PAR) {
		pktp->pkt_reason = CMD_TRAN_ERR;
	} else {
		NDBG1(("glm53c87x_check_error: J"));
		pktp->pkt_reason = CMD_INCOMPLETE;
	}
}

/*
 * for SCSI or DMA errors I need to figure out reasonable error
 * recoveries for all combinations of (hba state, scsi bus state,
 * error type). The possible error recovery actions are (in the
 * order of least to most drastic):
 *
 *	1. send message parity error to target
 *	2. send abort
 *	3. send abort tag
 *	4. send initiator detected error
 *	5. send bus device reset
 *	6. bus reset
 */
static ulong_t
glm53c87x_dma_status(glm_t *glm)
{
	glm_unit_t	*unit;
	caddr_t devaddr = glm->g_devaddr;
	ulong_t	 action = 0;
	uchar_t	 dstat;

	/* read DMA interrupt status register, and clear the register */
	dstat = ddi_get8(glm->g_datap, (uint8_t *)(devaddr + NREG_DSTAT));

	NDBG21(("glm53c87x_dma_status: devaddr=0x%x dstat=0x%x",
	    devaddr, dstat));

	/*
	 * DMA errors leave the HBA connected to the SCSI bus.
	 * Need to clear the bus and reset the chip.
	 */
	switch (glm->g_state) {
	case NSTATE_IDLE:
		/* this shouldn't happen */
		glm_log(glm, CE_WARN, "Unexpected DMA state: IDLE. dstat=%b",
		    dstat, dstatbits);
		action = (NACTION_DO_BUS_RESET | NACTION_SIOP_REINIT);
		break;

	case NSTATE_ACTIVE:
		unit = glm->g_current;
		unit->nt_dma_status |= dstat;
		if (dstat & NB_DSTAT_ERRORS) {
			glm_log(glm, CE_WARN,
			    "Unexpected DMA state: ACTIVE. dstat=%b",
				dstat, dstatbits);
			action = (NACTION_SIOP_REINIT | NACTION_DO_BUS_RESET |
			    NACTION_ERR);
		} else if (dstat & NB_DSTAT_SIR) {
			/* SCRIPT software interrupt */
			action |= NACTION_CHK_INTCODE;
		}
		break;

	case NSTATE_WAIT_RESEL:
		if (dstat & NB_DSTAT_ERRORS) {
			glm_log(glm, CE_WARN,
			    "Unexpected DMA state: WAIT. dstat=%b",
				dstat, dstatbits);
			action = NACTION_SIOP_REINIT;
		} else if (dstat & NB_DSTAT_SIR) {
			/* SCRIPT software interrupt */
			action |= NACTION_CHK_INTCODE;
		}
		break;
	}
	return (action);
}

static ulong_t
glm53c87x_scsi_status(glm_t *glm)
{
	glm_unit_t	*unit;
	caddr_t	 devaddr = glm->g_devaddr;
	ulong_t	 action = 0;
	uchar_t	 sist0;
	uchar_t	 sist1;
	uchar_t	 scntl1;

	NDBG1(("glm53c87x_scsi_status"));

	/* read SCSI interrupt status register, and clear the register */
	sist0 = ddi_get8(glm->g_datap, (uint8_t *)(devaddr + NREG_SIST0));

	(void) ddi_get32(glm->g_datap,
			(uint32_t *)(devaddr + NREG_SCRATCHA));

	(void) ddi_get32(glm->g_datap,
			(uint32_t *)(devaddr + NREG_SCRATCHA));

	sist1 = ddi_get8(glm->g_datap, (uint8_t *)(devaddr + NREG_SIST1));

	NDBG1(("glm53c87x_scsi_status: devaddr=0x%x sist0=0x%x sist1=0x%x",
	    devaddr, sist0, sist1));

	/*
	 * the scsi timeout, unexpected disconnect, and bus reset
	 * interrupts leave the bus in the bus free state ???
	 *
	 * the scsi gross error and parity error interrupts leave
	 * the bus still connected ???
	 */
	switch (glm->g_state) {
	case NSTATE_IDLE:
		if ((sist0 & (NB_SIST0_SGE | NB_SIST0_PAR | NB_SIST0_UDC)) ||
		    (sist1 & NB_SIST1_STO)) {
			/* shouldn't happen, clear chip */
			action = (NACTION_CLEAR_CHIP | NACTION_DO_BUS_RESET);
		}

		if (sist0 & NB_SIST0_RST) {
			action = (NACTION_CLEAR_CHIP | NACTION_GOT_BUS_RESET);
		}
		break;

	case NSTATE_ACTIVE:
		unit = glm->g_current;
		unit->nt_status0 |= sist0;
		unit->nt_status1 |= sist1;

		/*
		 * If phase mismatch, then figure out the residual for
		 * the current dma scatter/gather segment
		 */
		if (sist0 & NB_SIST0_MA) {
			action = NACTION_SAVE_BCNT;
		}

		/*
		 * Parity error.  Determine the phase and action.
		 */
		if (sist0 & NB_SIST0_PAR) {
			action = glm_parity_check(glm, unit);
		}

		if (sist0 & NB_SIST0_SGE) {
			/* attempt recovery if selection done and connected */
			if (ddi_get8(glm->g_datap,
				(uint8_t *)(devaddr + NREG_SCNTL1))
						& NB_SCNTL1_CON) {
				action = glm_parity_check(glm, unit);
			} else {
				action = NACTION_ERR;
			}
		}

		/*
		 * The target dropped the bus.
		 */
		if (sist0 & NB_SIST0_UDC) {
			action = (NACTION_SAVE_BCNT | NACTION_ERR |
			    NACTION_CLEAR_CHIP);
		}

		/*
		 * selection timeout.
		 * make sure we negotiate when this target comes
		 * on line later on
		 */
		if (sist1 & NB_SIST1_STO) {
			/* bus is now idle */
			action = (NACTION_SAVE_BCNT | NACTION_ERR |
			    NACTION_CLEAR_CHIP);
			glm_force_renegotiation(glm, unit->nt_target);
			glm->g_wdtr_sent = 0;
		}

		if (sist0 & NB_SIST0_RST) {
			/* bus is now idle */
			action = (NACTION_CLEAR_CHIP | NACTION_GOT_BUS_RESET |
				NACTION_ERR);
		}
		break;

	case NSTATE_WAIT_RESEL:
		if (sist0 & NB_SIST0_PAR) {
			/* attempt recovery if reconnected */
			scntl1 = ddi_get8(glm->g_datap,
				(uint8_t *)(glm->g_devaddr + NREG_SCNTL1));

			if (scntl1 & NB_SCNTL1_CON) {
				action = NACTION_MSG_PARITY;
			} else {
				/* don't respond */
				action = NACTION_BUS_FREE;
			}
		}

		if (sist0 & NB_SIST0_UDC) {
			/* target reselected then disconnected, ignore it */
			action = (NACTION_BUS_FREE | NACTION_CLEAR_CHIP);
		}

		if ((sist0 & NB_SIST0_SGE) || (sist1 & NB_SIST1_STO)) {
			/* shouldn't happen, clear chip and bus reset */
			action = (NACTION_CLEAR_CHIP | NACTION_DO_BUS_RESET);
		}

		if (sist0 & NB_SIST0_RST) {
			/* got bus reset, restart the wait for reselect */
			action = (NACTION_CLEAR_CHIP | NACTION_GOT_BUS_RESET |
				NACTION_BUS_FREE);
		}
		break;
	}
	NDBG1(("glm53c87x_scsi_status: action=%x", action));
	return (action);
}

/*
 * If the phase-mismatch which preceeds the Save Data Pointers
 * occurs within in a Scatter/Gather segment there's a residual
 * byte count that needs to be computed and remembered. It's
 * possible for a Disconnect message to occur without a preceeding
 * Save Data Pointers message, so at this point we only compute
 * the residual without fixing up the S/G pointers.
 */
static int
glm53c87x_save_byte_count(glm_t *glm, glm_unit_t *unit)
{
	caddr_t	devaddr = glm->g_devaddr;
	uint_t	dsp;
	int	index;
	ulong_t	remain;
	uchar_t	opcode;
	ushort_t tmp;
	uchar_t	sstat0;
	uchar_t	sstat2;
	int	rc;
	ushort_t dfifo;
	ushort_t ctest5;
	ulong_t	dbc;

	NDBG17(("glm53c87x_save_byte_count devaddr=0x%x", devaddr));

	/*
	 * Only need to do this for phase mismatch interrupts
	 * during actual data in or data out move instructions.
	 */
	if ((unit->nt_ncmdp->cmd_flags & CFLAG_DMAVALID) == 0) {
		/* since no data requested must not be S/G dma */
		rc = FALSE;
		goto out;
	}

	/*
	 * fetch the instruction pointer and back it up
	 * to the actual interrupted instruction.
	 */
	dsp = ddi_get32(glm->g_datap, (uint32_t *)(devaddr + NREG_DSP));
	dsp -= 8;

	/* check for MOVE DATA_OUT or MOVE DATA_IN instruction */
	opcode = ddi_get8(glm->g_datap, (uint8_t *)(devaddr + NREG_DCMD));

	if (opcode == (NSOP_MOVE | NSOP_DATAOUT)) {
		/* move data out */
		index = glm->g_do_list_end - dsp;

	} else if (opcode == (NSOP_MOVE | NSOP_DATAIN)) {
		/* move data in */
		index = glm->g_di_list_end - dsp;

	} else {
		/* not doing data dma so nothing to update */
		NDBG17(("glm53c87x_save_byte_count: %x not a move opcode",
		    opcode));
		rc = FALSE;
		goto out;
	}

	/*
	 * convert byte index into S/G DMA list index
	 */
	index = (index/8);

	if (index < 0 || index > GLM_MAX_DMA_SEGS) {
		/* it's out of dma list range, must have been some other move */
		NDBG17((
		    "glm53c87x_save_byte_count: devaddr=0x%x 0x%x not dma",
		    devaddr, index));
		rc = FALSE;
		goto out;
	}

	/*
	 * 875 has a larger fifo, so the math is a little different.
	 */
	if (glm->g_devid == GLM_53c875) {
		/* read the dbc register. */
		dbc = (ddi_get32(glm->g_datap,
			(uint32_t *)(devaddr + NREG_DBC)) & 0xffffff);

		ctest5 = ((ddi_get8(glm->g_datap,
			(uint8_t *)(devaddr + NREG_CTEST5)) & 0x3) << 8);

		dfifo = (ctest5 |
		    ddi_get8(glm->g_datap, (uint8_t *)(devaddr + NREG_DFIFO)));

		/* actual number left untransferred. */
		remain = dbc + ((dfifo - (dbc & 0x3ff)) & 0x3ff);
	} else {

		/* get the residual from the byte count register */
		dbc =  ddi_get32(glm->g_datap,
			(uint32_t *)(devaddr + NREG_DBC)) & 0xffffff;

		/* number of bytes stuck in the DMA FIFO */
		dfifo = (ddi_get8(glm->g_datap,
			(uint8_t *)(devaddr + NREG_DFIFO)) & 0x7f);

		/* actual number left untransferred. */
		remain = dbc + ((dfifo - (dbc & 0x7f)) & 0x7f);
	}

	/*
	 * Add one if there's a byte stuck in the SCSI fifo
	 */
	tmp = ddi_get8(glm->g_datap, (uint8_t *)(devaddr + NREG_CTEST2));

	if (tmp & NB_CTEST2_DDIR) {
		/* transfer was incoming (SCSI -> host bus) */
		sstat0 = ddi_get8(glm->g_datap,
			(uint8_t *)(devaddr + NREG_SSTAT0));

		sstat2 = ddi_get8(glm->g_datap,
			(uint8_t *)(devaddr + NREG_SSTAT2));

		if (sstat0 & NB_SSTAT0_ILF)
			remain++;	/* Add 1 if SIDL reg is full */

		/* Wide byte left async. */
		if (sstat2 & NB_SSTAT2_ILF1)
			remain++;

		/* check for synchronous i/o */
		if (unit->nt_dsap->nt_selectparm.nt_sxfer != 0) {
			tmp = ddi_get8(glm->g_datap,
				(uint8_t *)(devaddr + NREG_SSTAT1));
			remain += (tmp >> 4) & 0xf;
		}
	} else {
		/* transfer was outgoing (host -> SCSI bus) */
		sstat0 = ddi_get8(glm->g_datap,
				(uint8_t *)(devaddr + NREG_SSTAT0));

		sstat2 = ddi_get8(glm->g_datap,
				(uint8_t *)(devaddr + NREG_SSTAT2));

		if (sstat0 & NB_SSTAT0_OLF)
			remain++;	/* Add 1 if data is in SODL reg. */

		/* Check data for wide byte left. */
		if (sstat2 & NB_SSTAT2_OLF1)
			remain++;

		/* check for synchronous i/o */
		if ((unit->nt_dsap->nt_selectparm.nt_sxfer != 0) &&
		    (sstat0 & NB_SSTAT0_ORF)) {
			remain++;	/* Add 1 if data is in SODR */
			/* Check for Wide byte left. */
			if (sstat2 & NB_SSTAT2_ORF1)
				remain++;
		}
	}

	/* update the S/G pointers and indexes */
	glm_sg_update(unit, index, remain);
	rc = TRUE;

out:
	/* Clear the DMA FIFO pointers */
	CLEAR_DMA(glm);

	/* Clear the SCSI FIFO pointers */
	CLEAR_SCSI_FIFO(glm);

	NDBG17(("glm53c87x_save_byte_count: devaddr=0x%x index=%d remain=%d",
	    devaddr, index, remain));
	return (rc);
}

static int
glm53c87x_get_target(struct glm *glm, uchar_t *tp)
{
	caddr_t devaddr = glm->g_devaddr;
	uchar_t id;

	/*
	 * get the id byte received from the reselecting target
	 */
	id = ddi_get8(glm->g_datap, (uint8_t *)(devaddr + NREG_SSID));

	NDBG1(("glm53c87x_get_target: devaddr=0x%x lcrc=0x%x", devaddr, id));

	/* is it valid? */
	if (id & NB_SSID_VAL) {
		/* mask off extraneous bits */
		id &= NB_SSID_ENCID;
		NDBG1(("glm53c87x_get_target: ID %d reselected", id));
		*tp = id;
		return (TRUE);
	}
	glm_log(glm, CE_WARN,
	    "glm53c87x_get_target: invalid reselect id %d", id);
	cmn_err(CE_WARN, "!ID[SUNWpd.glm.87x_get_target.6015]");
	return (FALSE);
}

static void
glm53c87x_set_syncio(glm_t *glm, glm_unit_t *unit)
{
	caddr_t	devaddr = glm->g_devaddr;
	uchar_t	sxfer = unit->nt_dsap->nt_selectparm.nt_sxfer;
	uchar_t	scntl3 = 0;

	NDBG31(("glm53c87x_set_syncio: unit=%x", unit));

	/* Set SXFER register */
	ddi_put8(glm->g_datap, (uint8_t *)(devaddr + NREG_SXFER), sxfer);

	/* Set sync i/o clock divisor in SCNTL3 registers */
	if (sxfer != 0) {
		switch (unit->nt_sscfX10) {
		case 10:
			scntl3 = NB_SCNTL3_SCF1 | glm->g_scntl3;
			break;

		case 15:
			scntl3 = NB_SCNTL3_SCF15 | glm->g_scntl3;
			break;

		case 20:
			scntl3 = NB_SCNTL3_SCF2 | glm->g_scntl3;
			break;

		case 30:
			scntl3 = NB_SCNTL3_SCF3 | glm->g_scntl3;
			break;
		}
		unit->nt_dsap->nt_selectparm.nt_scntl3 |= scntl3;
	}

	ddi_put8(glm->g_datap, (uint8_t *)(devaddr + NREG_SCNTL3),
	    unit->nt_dsap->nt_selectparm.nt_scntl3);

	/* set extended filtering if not Fast-SCSI (i.e., < 5MB/sec) */
	if (sxfer == 0 || unit->nt_fastscsi == FALSE) {
		ddi_put8(glm->g_datap, (uint8_t *)(devaddr + NREG_STEST2),
		    NB_STEST2_EXT);
	} else {
		ddi_put8(glm->g_datap, (uint8_t *)(devaddr + NREG_STEST2), 0);
	}
}

static void
glm53c87x_setup_script(glm_t *glm, glm_unit_t *unit)
{
	caddr_t	devaddr = glm->g_devaddr;
	struct glm_scsi_cmd *cmd = unit->nt_ncmdp;

	uchar_t nleft = cmd->cmd_cur_cookie;

	NDBG1(("glm53c87x_setup_script: devaddr=0x%x", devaddr));

	/* Set the Data Structure address register to */
	/* the physical address of the active table */
	ddi_put32(glm->g_datap,
	    (uint32_t *)(devaddr + NREG_DSA), unit->nt_dsa_addr);

	/*
	 * Setup scratcha0 as the number of segments left to do
	 */
	ddi_put8(glm->g_datap,
	    (uint8_t *)(glm->g_devaddr + NREG_SCRATCHA0), nleft);

	NDBG1(("glm53c87x_setup_script: devaddr=0x%x okay", devaddr));
}

static void
glm53c87x_bus_reset(glm_t *glm)
{
	caddr_t	devaddr = glm->g_devaddr;

	NDBG22(("glm53c87x_bus_reset"));

	/* Reset the scsi bus */
	ClrSetBits(devaddr + NREG_SCNTL1, 0, NB_SCNTL1_RST);

	/* Wait at least 25 microsecond */
	drv_usecwait((clock_t)25);

	/* Turn off the bit to complete the reset */
	ClrSetBits(devaddr + NREG_SCNTL1, NB_SCNTL1_RST, 0);
}


/*
 * device and bus reset handling
 *
 * Notes:
 *	- RESET_ALL:	reset the SCSI bus
 *	- RESET_TARGET:	reset the target specified in scsi_address
 */
static int
glm_scsi_reset(struct scsi_address *ap, int level)
{
	glm_t *glm = ADDR2GLM(ap);
	int rval;

	NDBG22(("glm_scsi_reset: target=%x level=%x",
		ap->a_target, level));

	mutex_enter(&glm->g_mutex);
	ASSERT(!glm->g_suspended);
	rval = glm_do_scsi_reset(ap, level);
	mutex_exit(&glm->g_mutex);
	return (rval);
}

static int
glm_do_scsi_reset(struct scsi_address *ap, int level)
{
	glm_t *glm = ADDR2GLM(ap);
	int rval = FALSE;

	NDBG22(("glm_do_scsi_reset: target=%x level=%x",
			ap->a_target, level));

	switch (level) {

	case RESET_ALL:
		/*
		 * Reset the SCSI bus, kill all commands in progress
		 * (remove them from lists, etc.)  Make sure you
		 * wait the specified time for the reset to settle,
		 * if your hardware doesn't do that for you somehow.
		 */
		GLM_BUS_RESET(glm);
		rval = TRUE;
		break;

	case RESET_TARGET:
		/*
		 * Issue a Bus Device Reset message to the target/lun
		 * specified in ap;
		 */
		if (ADDR2GLMUNITP(ap) == NULL) {
			return (FALSE);
		}

		/*
		 * the flushing is done when NINT_DEV_RESET has been received
		 */
		rval = glm_send_dev_reset_msg(ap, glm);
		break;
	}
	return (rval);
}

/* ARGSUSED */
static void
glm_reset_bus(struct glm *glm)
{
	struct scsi_address ap;

	NDBG28(("glm_reset_bus"));

	mutex_enter(&glm->g_mutex);
	ap.a_hba_tran = glm->g_tran;
	ap.a_target = 0;
	ap.a_lun = 0;
	(void) glm_do_scsi_reset(&ap, RESET_ALL);
	mutex_exit(&glm->g_mutex);
}

static int
glm_scsi_reset_notify(struct scsi_address *ap, int flag,
	void (*callback)(caddr_t), caddr_t arg)
{
	struct glm *glm = ADDR2GLM(ap);

	NDBG22(("glm_scsi_reset_notify: tgt=%x", ap->a_target));

	return (scsi_hba_reset_notify_setup(ap, flag, callback, arg,
		&glm->g_mutex, &glm->g_reset_notify_listf));
}

static int
glm_send_dev_reset_msg(struct scsi_address *ap, glm_t *glm)
{
	auto struct glm_scsi_cmd local_cmd;
	auto struct scsi_pkt local_pkt;
	struct glm_scsi_cmd *cmd = &local_cmd;
	struct scsi_pkt *pkt = &local_pkt;
	int rval = FALSE;

	NDBG22(("glm_send_dev_reset_msg: tgt=%x", ap->a_target));

	bzero((caddr_t)cmd, sizeof (*cmd));
	bzero((caddr_t)pkt, sizeof (*pkt));

	pkt->pkt_address	= *ap;
	pkt->pkt_cdbp		= (opaque_t)&cmd->cmd_cdb[0];
	pkt->pkt_scbp		= (opaque_t)&cmd->cmd_scb;
	pkt->pkt_ha_private	= (opaque_t)cmd;
	pkt->pkt_flags		= (FLAG_NOINTR | FLAG_HEAD);
	cmd->cmd_pkt		= pkt;
	cmd->cmd_scblen		= 1;
	cmd->cmd_flags		= CFLAG_CMDPROXY;
	cmd->cmd_type		= NRQ_DEV_RESET;
	cmd->cmd_cdb[GLM_PROXY_TYPE] = GLM_PROXY_SNDMSG;
	cmd->cmd_cdb[GLM_PROXY_RESULT] = FALSE;

	/*
	 * Send proxy cmd.
	 */
	if ((glm_accept_pkt(glm, cmd) == TRAN_ACCEPT) &&
	    (pkt->pkt_reason == CMD_CMPLT) &&
	    (cmd->cmd_cdb[GLM_PROXY_RESULT] == TRUE)) {
		rval = TRUE;
	}

	NDBG22(("glm_send_dev_reset_msg: rval=%x", rval));

#ifdef GLM_TEST
	if (rval && glm_test_stop) {
		debug_enter("glm_send_dev_reset_msg");
	}
#endif
	return (rval);
}

static void
glm_set_throttles(struct glm *glm, int slot, int n, int what)
{
	register int i;
	struct glm_unit *unit;

	NDBG25(("glm_set_throttles: slot=%x, n=%x, what=%x",
		slot, n, what));

	/*
	 * if the bus is draining/quiesced, no changes to the throttles
	 * are allowed. Not allowing change of throttles during draining
	 * limits error recovery but will reduce draining time
	 *
	 * all throttles should have been set to HOLD_THROTTLE
	 */
	if (glm->g_softstate & (GLM_SS_QUIESCED | GLM_SS_DRAINING)) {
		return;
	}

	ASSERT((n == 1) || (n == N_GLM_UNITS) || (n == NLUNS_PER_TARGET));
	ASSERT((slot + n) <= N_GLM_UNITS);
	if (n == NLUNS_PER_TARGET) {
		slot &= ~(NLUNS_PER_TARGET - 1);
	}

	for (i = slot; i < (slot + n); i++) {
		if ((unit = glm->g_units[i]) == NULL) {
			continue;
		}

		if (what == HOLD_THROTTLE) {
			unit->nt_throttle = HOLD_THROTTLE;
		} else if ((glm->g_reset_delay[i/NLUNS_PER_TARGET]) == 0) {
			unit->nt_throttle = what;
		}
	}
}

static void
glm_set_all_lun_throttles(struct glm *glm, int target, int what)
{
	/*
	 * passed in is the target, slot will be lun 0.
	 */
	int slot = (target * NLUNS_PER_TARGET);

	/*
	 * glm_set_throttle will adjust slot starting at LUN 0
	 */
	glm_set_throttles(glm, slot, NLUNS_PER_TARGET, what);
}

static void
glm_full_throttle(struct glm *glm, int target, int lun)
{
	int slot = ((target * NLUNS_PER_TARGET) | lun);
	glm_set_throttles(glm, slot, 1, MAX_THROTTLE);
}

static void
glm_flush_lun(glm_t *glm, glm_unit_t *unit, uchar_t reason,
    uint_t stat)
{
	NDBG25(("glm_flush_lun: unit=%x reason=%x stat=%x",
		unit, reason, stat));

	glm_mark_packets(glm, unit, reason, stat);
	glm_flush_waitQ(glm, unit);
	glm_flush_tagQ(glm, unit);
	if (unit->nt_state & NPT_STATE_QUEUED) {
		glm_hbaq_delete(glm, unit);
	}

	/*
	 * reset timeouts
	 */
	if (unit->nt_active) {
		unit->nt_active->nt_timebase = 0;
		unit->nt_active->nt_timeout = 0;
		unit->nt_active->nt_dups = 0;
	}

	ASSERT((unit->nt_state & NPT_STATE_QUEUED) == 0);

	unit->nt_state &= ~NPT_STATE_ACTIVE;
	unit->nt_linkp = NULL;
	unit->nt_ncmdp = NULL;
}

/*
 * Flush all the commands for all the LUNs on the specified target device.
 */
static void
glm_flush_target(glm_t *glm, ushort_t target, uchar_t reason,
    uint_t stat)
{
	struct glm_unit *unit;
	ushort_t lun;

	NDBG25(("glm_flush_target: target=%x", target));

	/*
	 * Completed Bus Device Reset, clean up
	 */
	for (lun = 0; lun < NLUNS_PER_TARGET; lun++) {
		unit = NTL2UNITP(glm, target, lun);
		if (unit == NULL) {
			continue;
		}

		/*
		 * flush the cmds from this target.
		 */
		glm_flush_lun(glm, unit, reason, stat);
		ASSERT(unit->nt_state == 0);
	}

#ifdef GLM_DEBUG
	if (glm_hbaq_check(glm, unit) == TRUE) {
		glm_log(glm, CE_WARN, "target (%d.%d) still queued.\n",
			unit->nt_target, unit->nt_lun);
	}
#endif

	/*
	 * if we are not in panic set up a reset delay for this target
	 */
	if (!ddi_in_panic()) {
		glm_set_all_lun_throttles(glm, target, HOLD_THROTTLE);
		glm->g_reset_delay[target] = glm->g_scsi_reset_delay;
		glm_start_watch_reset_delay();
	} else {
		drv_usecwait(glm->g_scsi_reset_delay * 1000);
	}

	glm_force_renegotiation(glm, target);
}

/*
 * Called after a SCSI Bus Reset to find and flush all
 * the outstanding scsi requests.
 */
static void
glm_flush_hba(struct glm *glm)
{
	struct glm_unit *unit;
	int slot;

	NDBG25(("glm_flush_hba"));

	/*
	 * renegotiate wide and sync for all targets.
	 */
	glm->g_wide_known = glm->g_wide_enabled = 0;
	glm_syncio_reset(glm, NULL);

	/*
	 * for each slot, flush all cmds- waiting and/or outstanding.
	 */
	for (slot = 0; slot < N_GLM_UNITS; slot++) {
		if ((unit = glm->g_units[slot]) == NULL) {
			continue;
		}
		glm_flush_lun(glm, unit, CMD_RESET, STAT_BUS_RESET);
	}

	/*
	 * The current unit, and link list of unit for the hba
	 * to run have been flushed.
	 */
	glm->g_current = NULL;
	ASSERT((glm->g_forwp == NULL) && (glm->g_backp == NULL));
	glm->g_forwp = glm->g_backp = NULL;

	/*
	 * Now mark the hba as idle.
	 */
	glm->g_state = NSTATE_IDLE;

	/*
	 * perform the reset notification callbacks that are registered.
	 */
	(void) scsi_hba_reset_notify_callback(&glm->g_mutex,
		&glm->g_reset_notify_listf);

	/*
	 * setup the reset delay
	 */
	if (!ddi_in_panic()) {
		glm_setup_bus_reset_delay(glm);
	} else {
		drv_usecwait(glm->g_scsi_reset_delay * 1000);
	}
}

/*
 * mark all packets with new reason and update statistics
 */
static void
glm_mark_packets(struct glm *glm, struct glm_unit *unit,
    u_char reason, u_int stat)
{
	struct glm_scsi_cmd *sp = unit->nt_waitq;

	NDBG25(("glm_mark_packets: unit=%x reason=%x stat=%x",
		unit, reason, stat));

	/*
	 * First set pkt_reason, pkt_statistics for the waitq.
	 */
	while (sp != 0) {
		glm_set_pkt_reason(glm, sp, reason, STAT_ABORTED);
		sp = sp->cmd_linkp;
	}
	if (unit->nt_tcmds) {
		int n = 0;
		u_short tag;

		for (tag = 0; tag < unit->nt_active->nt_n_slots; tag++) {
			if ((sp = unit->nt_active->nt_slot[tag]) != 0) {
				glm_set_pkt_reason(glm, sp, reason, stat);
				n++;
			}
		}
		ASSERT(unit->nt_tcmds == n);
	}

	/*
	 * There may be a proxy cmd.
	 */
	if ((sp = unit->nt_ncmdp) != NULL) {
		if (sp->cmd_flags & CFLAG_CMDPROXY) {
			glm_set_pkt_reason(glm, sp, reason, stat);
		}
	}
}

/*
 * put the active cmd and waitq on the doneq.
 */
static void
glm_flush_waitQ(struct glm *glm, struct glm_unit *unit)
{
	struct glm_scsi_cmd *sp;

	NDBG25(("glm_flush_waitQ: unit=%x", unit));

	/*
	 * Flush the waitq.
	 */
	while ((sp = glm_waitq_rm(unit)) != NULL) {
		glm_doneq_add(glm, sp);
	}

	/*
	 * Flush the proxy cmd.
	 */
	if ((sp = unit->nt_ncmdp) != NULL) {
		if (sp->cmd_flags & CFLAG_CMDPROXY) {
			glm_doneq_add(glm, sp);
		}
	}

#ifdef GLM_DEBUG
	if (unit->nt_state & NPT_STATE_QUEUED) {
		if (glm_hbaq_check(glm, unit) == FALSE) {
			glm_log(glm, CE_WARN,
			    "glm_flush_waitQ: someone is not correct.\n");
		}

	}
#endif
}

/*
 * cleanup the tag queue
 * preserve some order by starting with the oldest tag
 */
static void
glm_flush_tagQ(struct glm *glm, struct glm_unit *unit)
{
	u_short tag, starttag;
	struct glm_scsi_cmd *sp;
	struct nt_slots *tagque = unit->nt_active;

	if (tagque == NULL) {
		return;
	}

	tag = starttag = unit->nt_active->nt_tags;

	do {
		if ((sp = tagque->nt_slot[tag]) != 0) {
			glm_remove_cmd(glm, unit, sp);
			glm_doneq_add(glm, sp);
		}
		tag = ((u_short)(tag + 1)) %
		    (u_short)unit->nt_active->nt_n_slots;
	} while (tag != starttag);

	ASSERT(unit->nt_tcmds == 0);
}

/*
 * set pkt_reason and OR in pkt_statistics flag
 */
/*ARGSUSED*/
static void
glm_set_pkt_reason(struct glm *glm, struct glm_scsi_cmd *cmd, uchar_t reason,
    uint_t stat)
{
	NDBG25(("glm_set_pkt_reason: cmd=%x reason=%x stat=%x",
		cmd, reason, stat));

	if (cmd) {
		if (cmd->cmd_pkt->pkt_reason == CMD_CMPLT) {
			cmd->cmd_pkt->pkt_reason = reason;
		}
		cmd->cmd_pkt->pkt_statistics |= stat;
	}
}

static void
glm_start_watch_reset_delay()
{
	NDBG22(("glm_start_watch_reset_delay"));

	mutex_enter(&glm_global_mutex);
	if (glm_reset_watch == 0) {
		glm_reset_watch = timeout(glm_watch_reset_delay, NULL,
		    drv_usectohz((clock_t)
			GLM_WATCH_RESET_DELAY_TICK * 1000));
	}
	ASSERT(glm_reset_watch != 0);
	mutex_exit(&glm_global_mutex);
}

static void
glm_setup_bus_reset_delay(struct glm *glm)
{
	int i;

	NDBG22(("glm_setup_bus_reset_delay"));

	glm_set_throttles(glm, 0, N_GLM_UNITS, HOLD_THROTTLE);
	for (i = 0; i < NTARGETS_WIDE; i++) {
		glm->g_reset_delay[i] = glm->g_scsi_reset_delay;
	}
	glm_start_watch_reset_delay();
}

/*
 * glm_watch_reset_delay(_subr) is invoked by timeout() and checks every
 * glm instance for active reset delays
 */
/*ARGSUSED*/
static void
glm_watch_reset_delay(void *arg)
{
	struct glm *glm;
	int not_done = 0;

	NDBG22(("glm_watch_reset_delay"));

	mutex_enter(&glm_global_mutex);
	glm_reset_watch = 0;
	mutex_exit(&glm_global_mutex);
	for (glm = glm_head; glm != (struct glm *)NULL; glm = glm->g_next) {
		if (glm->g_tran == 0) {
			continue;
		}
		mutex_enter(&glm->g_mutex);
		not_done += glm_watch_reset_delay_subr(glm);
		mutex_exit(&glm->g_mutex);
	}
	if (not_done) {
		glm_start_watch_reset_delay();
	}
}

static int
glm_watch_reset_delay_subr(register struct glm *glm)
{
	register short slot, s;
	int done = 0;

	NDBG22(("glm_watch_reset_delay_subr: glm=%x", glm));

	for (slot = 0; slot < N_GLM_UNITS; slot += NLUNS_PER_TARGET) {
		s = slot/NLUNS_PER_TARGET;
		if (glm->g_reset_delay[s] != 0) {
			glm->g_reset_delay[s] -= GLM_WATCH_RESET_DELAY_TICK;
			if (glm->g_reset_delay[s] <= 0) {
				glm->g_reset_delay[s] = 0;
				glm_set_all_lun_throttles(glm, s,
				    MAX_THROTTLE);
				glm_queue_target_lun(glm, s);
			} else {
				done = -1;
			}
		}
	}
	return (done);
}

/*
 * queue all target/lun with work after reset delay.
 */
static void
glm_queue_target_lun(glm_t *glm, ushort_t target)
{
	ushort_t lun;
	glm_unit_t *unit;

	NDBG22(("glm_queue_target_lun: target=%x", target));

	for (lun = 0; lun < NLUNS_PER_TARGET; lun++) {
		unit = NTL2UNITP(glm, target, lun);
		if (unit == NULL) {
			continue;
		}
		/*
		 * If there are pkts to run, queue this target/lun.
		 */
		if (unit->nt_waitq != NULL) {
			glm_queue_target(glm, unit);
		}
	}
}

#ifdef GLM_TEST
static void
glm_test_reset(struct glm *glm, struct glm_unit *unit)
{
	struct scsi_address ap;
	ushort target = unit->nt_target;

	if (glm_rtest & (1<<target)) {
		ap.a_hba_tran = glm->g_tran;
		ap.a_target = target;
		ap.a_lun = 0;

		NDBG22(("glm_test_reset: glm_rtest=%x glm_rtest_type=%x",
			glm_rtest, glm_rtest_type));

		switch (glm_rtest_type) {
		case 0:
			if (glm_do_scsi_reset(&ap, RESET_TARGET)) {
				glm_rtest = 0;
			}
			break;
		case 1:
			if (glm_do_scsi_reset(&ap, RESET_ALL)) {
				glm_rtest = 0;
			}
			break;
		}
		if (glm_rtest == 0) {
			NDBG22(("glm_test_reset success"));
		}
	}
}
#endif

/*
 * abort handling:
 *
 * Notes:
 *	- if pkt is not NULL, abort just that command
 *	- if pkt is NULL, abort all outstanding commands for target
 */
/*ARGSUSED*/
static int
glm_scsi_abort(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	register struct glm *glm = ADDR2GLM(ap);
	register int rval;

	NDBG23(("glm_scsi_abort: target=%d.%d", ap->a_target, ap->a_lun));

	mutex_enter(&glm->g_mutex);
	ASSERT(!glm->g_suspended);
	rval = glm_do_scsi_abort(ap, pkt);
	mutex_exit(&glm->g_mutex);
	return (rval);
}

static int
glm_do_scsi_abort(struct scsi_address *ap, struct scsi_pkt *pkt)
{
	struct glm *glm = ADDR2GLM(ap);
	struct glm_unit *unit;
	struct glm_scsi_cmd *sp = NULL;
	int rval = FALSE;

	ASSERT(mutex_owned(&glm->g_mutex));

	/*
	 * Abort the command pktp on the target/lun in ap.  If pktp is
	 * NULL, abort all outstanding commands on that target/lun.
	 * If you can abort them, return 1, else return 0.
	 * Each packet that's aborted should be sent back to the target
	 * driver through the callback routine, with pkt_reason set to
	 * CMD_ABORTED.
	 *
	 * abort cmd pktp on HBA hardware; clean out of outstanding
	 * command lists, etc.
	 */
	if ((unit = ADDR2GLMUNITP(ap)) == NULL) {
		return (rval);
	}

	if (pkt != NULL) {
		/* abort the specified packet */
		sp = PKT2CMD(pkt);

		if (sp->cmd_queued) {
			NDBG23(("glm_do_scsi_abort: queued sp=%x aborted",
					sp));
			glm_waitq_delete(unit, sp);
			glm_set_pkt_reason(glm, sp, CMD_ABORTED, STAT_ABORTED);
			glm_doneq_add(glm, sp);
			rval = TRUE;
			goto done;
		}

		/*
		 * the pkt may be the active packet. if not, the packet
		 * has already been returned or may be on the doneq
		 */
		if (sp != unit->nt_active->nt_slot[sp->cmd_tag[1]]) {
			rval = TRUE;
			goto done;
		}
	}

	/*
	 * Abort one active pkt or all the packets for a particular
	 * LUN, even if no packets are queued/outstanding
	 * If it's done then it's probably already on
	 * the done queue. If it's active we can't abort.
	 */
	rval = glm_send_abort_msg(ap, glm, unit);

#ifdef GLM_TEST
	if (rval && glm_test_stop) {
		debug_enter("glm_do_scsi_abort");
	}
#endif

done:
	glm_doneq_empty(glm);

	return (rval);
}

/*ARGSUSED*/
static int
glm_send_abort_msg(struct scsi_address *ap, glm_t *glm, glm_unit_t *unit)
{
	auto struct glm_scsi_cmd local_cmd;
	auto struct scsi_pkt local_pkt;
	struct glm_scsi_cmd *cmd = &local_cmd;
	struct scsi_pkt *pkt = &local_pkt;
	int rval = FALSE;

	NDBG23(("glm_send_abort_msg: tgt=%x", ap->a_target));

	bzero((caddr_t)cmd, sizeof (*cmd));
	bzero((caddr_t)pkt, sizeof (*pkt));

	pkt->pkt_address	= *ap;
	pkt->pkt_cdbp		= (opaque_t)&cmd->cmd_cdb[0];
	pkt->pkt_scbp		= (opaque_t)&cmd->cmd_scb;
	pkt->pkt_ha_private	= (opaque_t)cmd;
	pkt->pkt_flags		= (FLAG_NOINTR | FLAG_HEAD);
	cmd->cmd_pkt		= pkt;
	cmd->cmd_scblen		= 1;
	cmd->cmd_flags		= CFLAG_CMDPROXY;
	cmd->cmd_type		= NRQ_ABORT;
	cmd->cmd_cdb[GLM_PROXY_TYPE] = GLM_PROXY_SNDMSG;
	cmd->cmd_cdb[GLM_PROXY_RESULT] = FALSE;

	/*
	 * Send proxy cmd.
	 */
	if ((glm_accept_pkt(glm, cmd) == TRAN_ACCEPT) &&
	    (pkt->pkt_reason == CMD_CMPLT) &&
	    (cmd->cmd_cdb[GLM_PROXY_RESULT] == TRUE)) {
		rval = TRUE;
	}

	NDBG23(("glm_send_abort_msg: rval=%x", rval));
#ifdef GLM_TEST
	if (rval && glm_test_stop) {
		debug_enter("glm_send_abort_msg");
	}
#endif
	return (rval);
}

#ifdef GLM_TEST
static void
glm_test_abort(struct glm *glm, struct glm_unit *unit)
{
	struct scsi_address ap;
	ushort target = unit->nt_target;
	int rval = FALSE;
	struct glm_scsi_cmd *cmd;

	ASSERT(mutex_owned(&glm->g_mutex));

	if (glm_atest & (1<<target)) {
		ap.a_hba_tran = glm->g_tran;
		ap.a_target = target;
		ap.a_lun = 0;

		NDBG23(("glm_test_abort: glm_atest=%x glm_atest_type=%x",
			glm_atest, glm_atest_type));

		switch (glm_atest_type) {
		case 0:
			/* aborting specific queued cmd (head) */
			if (unit->nt_waitq) {
				cmd =  unit->nt_waitq;
				rval = glm_do_scsi_abort(&ap, cmd->cmd_pkt);
			}
			break;
		case 1:
			/* aborting specific queued cmd (2nd) */
			if (unit->nt_waitq && unit->nt_waitq->cmd_linkp) {
				cmd = unit->nt_waitq->cmd_linkp;
				rval = glm_do_scsi_abort(&ap, cmd->cmd_pkt);
			}
			break;
		case 2:
		{
			int tag;
			struct nt_slots *tag_slots = unit->nt_active;

			/* aborting specific disconnected cmd */
			if (((unit->nt_state & NPT_STATE_ACTIVE) == 0) &&
			    unit->nt_tcmds != 0) {
				/*
				 * find the oldest tag.
				 */
				for (tag = NTAGS-1; tag >= 0; tag--) {
				    if ((cmd = tag_slots->nt_slot[tag]) != 0) {
					if (cmd->cmd_flags & CFLAG_CMDDISC) {
					    break;
					}
				    }
				}
				if (cmd) {
				    rval = glm_do_scsi_abort(&ap, cmd->cmd_pkt);
				}
			}
			break;
		}
		case 3:
			/* aborting all queued requests */
			if (unit->nt_waitq || unit->nt_tcmds > 0) {
				rval = glm_do_scsi_abort(&ap, NULL);
			}
			break;
		case 4:
			/* aborting disconnected cmd */
			if ((unit->nt_state & NPT_STATE_ACTIVE) == 0) {
				rval = glm_do_scsi_abort(&ap, NULL);
			}
			break;
		}
		if (rval) {
			glm_atest = 0;
			NDBG23(("glm_test_abort success"));
		}
	}
}
#endif

/*
 * capability handling:
 * (*tran_getcap).  Get the capability named, and return its value.
 */
static int
glm_scsi_getcap(struct scsi_address *ap, char *cap, int tgtonly)
{
	register struct glm *glm = ADDR2GLM(ap);
	register struct glm_unit *unit;
	register ushort_t tshift = (1<<ap->a_target);
	int ckey;
	int rval = FALSE;

	NDBG24(("glm_scsi_getcap: target=%x, cap=%s tgtonly=%x",
		ap->a_target, cap, tgtonly));

	mutex_enter(&glm->g_mutex);

	if ((unit = ADDR2GLMUNITP(ap)) == NULL) {
		mutex_exit(&glm->g_mutex);
		return (rval);
	}

	if ((glm_capchk(cap, tgtonly, &ckey)) != TRUE) {
		mutex_exit(&glm->g_mutex);
		return (UNDEFINED);
	}

	switch (ckey) {
	case SCSI_CAP_DMA_MAX:
		rval = 1<<30;
		break;
	case SCSI_CAP_DISCONNECT:
		if (tgtonly &&
		    (glm->g_target_scsi_options[ap->a_target] &
				SCSI_OPTIONS_DR)) {
			rval = TRUE;
		}
		break;
	case SCSI_CAP_WIDE_XFER:
		if (tgtonly && ((glm->g_nowide & tshift) == 0)) {
			rval = TRUE;
		}
		break;
	case SCSI_CAP_SYNCHRONOUS:
		if (tgtonly &&
		    (glm->g_target_scsi_options[ap->a_target] &
				SCSI_OPTIONS_SYNC)) {
			rval = TRUE;
		}
		break;
	case SCSI_CAP_ARQ:
		if (tgtonly && unit->nt_arq_pkt) {
			rval = TRUE;
		}
		break;
	case SCSI_CAP_INITIATOR_ID:
		rval = glm->g_glmid;
		break;
	case SCSI_CAP_MSG_OUT:
	case SCSI_CAP_PARITY:
	case SCSI_CAP_UNTAGGED_QING:
		rval = TRUE;
		break;
	case SCSI_CAP_TAGGED_QING:
		if (tgtonly && ((glm->g_notag & tshift) == 0)) {
			rval = TRUE;
		}
		break;
	case SCSI_CAP_RESET_NOTIFICATION:
		rval = TRUE;
		break;
	case SCSI_CAP_LINKED_CMDS:
		rval = FALSE;
		break;
	case SCSI_CAP_QFULL_RETRIES:
		rval = glm->g_qfull_retries[ap->a_target];
		break;
	case SCSI_CAP_QFULL_RETRY_INTERVAL:
		rval = drv_hztousec(
			glm->g_qfull_retry_interval[ap->a_target]) /
			1000;
		break;
	default:
		rval = UNDEFINED;
		break;
	}

	NDBG24(("glm_scsi_getcap: %s, rval=%x", cap, rval));

	mutex_exit(&glm->g_mutex);
	return (rval);
}

/*
 * (*tran_setcap).  Set the capability named to the value given.
 */
static int
glm_scsi_setcap(struct scsi_address *ap, char *cap, int value, int tgtonly)
{
	register struct glm *glm = ADDR2GLM(ap);
	register struct glm_unit *unit;
	int ckey;
	register int target = ap->a_target;
	register ushort_t tshift = (1<<target);
	int rval = FALSE;

	NDBG24(("glm_scsi_setcap: target=%x, cap=%s value=%x tgtonly=%x",
		ap->a_target, cap, value, tgtonly));

	if (!tgtonly) {
		return (rval);
	}

	mutex_enter(&glm->g_mutex);

	if ((unit = ADDR2GLMUNITP(ap)) == NULL) {
		mutex_exit(&glm->g_mutex);
		return (rval);
	}

	if ((glm_capchk(cap, tgtonly, &ckey)) != TRUE) {
		mutex_exit(&glm->g_mutex);
		return (UNDEFINED);
	}

	switch (ckey) {
	case SCSI_CAP_DMA_MAX:
	case SCSI_CAP_MSG_OUT:
	case SCSI_CAP_PARITY:
	case SCSI_CAP_INITIATOR_ID:
	case SCSI_CAP_LINKED_CMDS:
	case SCSI_CAP_UNTAGGED_QING:
	case SCSI_CAP_RESET_NOTIFICATION:
		/*
		 * None of these are settable via
		 * the capability interface.
		 */
		break;
	case SCSI_CAP_DISCONNECT:
		if (value)
			glm->g_target_scsi_options[ap->a_target] |=
					SCSI_OPTIONS_DR;
		else
			glm->g_target_scsi_options[ap->a_target] &=
					~SCSI_OPTIONS_DR;
		break;
	case SCSI_CAP_WIDE_XFER:
		if (value) {
			if (glm->g_target_scsi_options[target] &
			    SCSI_OPTIONS_WIDE) {
				glm->g_nowide &= ~tshift;
			}
		} else {
			glm->g_nowide |= tshift;
			glm->g_target_scsi_options[target] &=
					~SCSI_OPTIONS_WIDE;
		}
		glm->g_props_update |= (1<<target);
		glm_force_renegotiation(glm, target);
		rval = TRUE;
		break;
	case SCSI_CAP_SYNCHRONOUS:
		if (value) {
			glm->g_target_scsi_options[ap->a_target] |=
				SCSI_OPTIONS_SYNC;
		} else {
			glm->g_target_scsi_options[ap->a_target] &=
				~SCSI_OPTIONS_SYNC;
		}
		glm->g_props_update |= (1<<target);
		glm_force_renegotiation(glm, target);
		rval = TRUE;
		break;
	case SCSI_CAP_ARQ:
		if (value) {
			if (glm_create_arq_pkt(unit, ap)) {
				break;
			}
		} else {
			if (glm_delete_arq_pkt(unit, ap)) {
				break;
			}
		}
		rval = TRUE;
		break;
	case SCSI_CAP_TAGGED_QING:
	{
		u_short old_notag = glm->g_notag;

		if (value) {
			if (glm->g_target_scsi_options[target] &
			    SCSI_OPTIONS_TAG) {
				NDBG9(("target %d: TQ enabled",
				    target));
				glm->g_notag &= ~tshift;
			} else {
				break;
			}
		} else {
			NDBG9(("target %d: TQ disabled",
			    target));
			glm->g_notag |= tshift;
		}

		if (value && glm_alloc_active_slots(glm, unit, KM_NOSLEEP)) {
			glm->g_notag = old_notag;
			break;
		}

		glm_set_all_lun_throttles(glm, target, MAX_THROTTLE);

		rval = TRUE;
		break;
	}
	case SCSI_CAP_QFULL_RETRIES:
		glm->g_qfull_retries[ap->a_target] = (uchar_t)value;
		rval = TRUE;
		break;
	case SCSI_CAP_QFULL_RETRY_INTERVAL:
		glm->g_qfull_retry_interval[ap->a_target] =
			drv_usectohz(value * 1000);
		rval = TRUE;
		break;
	default:
		rval = UNDEFINED;
		break;
	}
	mutex_exit(&glm->g_mutex);
	return (rval);
}

/*
 * Utility routine for glm_ifsetcap/ifgetcap
 */
/*ARGSUSED*/
static int
glm_capchk(char *cap, int tgtonly, int *cidxp)
{
	NDBG24(("glm_capchk: cap=%s", cap));

	if (!cap)
		return (FALSE);

	*cidxp = scsi_hba_lookup_capstr(cap);
	return (TRUE);
}

/*
 * property management
 * glm_update_props:
 * create/update sync/wide/TQ/scsi-options properties for this target
 */
static void
glm_update_props(struct glm *glm, int tgt)
{
	char property[32];
	int wide_enabled, tq_enabled;
	uint_t xfer_rate = 0;
	struct glm_unit *unit = glm->g_units[TL2INDEX(tgt, 0)];

	if (unit == NULL) {
		return;
	}

	NDBG2(("glm_update_props: tgt=%x", tgt));

	wide_enabled = ((glm->g_wide_enabled & (1<<tgt))? 1 : 0);

	if ((unit->nt_dsap->nt_selectparm.nt_sxfer & 0x1f) != 0) {
		xfer_rate = ((1000 * 1000)/glm->g_minperiod[tgt]);
		xfer_rate *= ((wide_enabled)? 2 : 1);
	} else {
		xfer_rate = 500;
		xfer_rate *= ((wide_enabled)? 2 : 1);
	}

	(void) sprintf(property, "target%x-sync-speed", tgt);
	glm_update_this_prop(glm, property, xfer_rate);

	(void) sprintf(property, "target%x-wide", tgt);
	glm_update_this_prop(glm, property, wide_enabled);

	(void) sprintf(property, "target%x-TQ", tgt);
	tq_enabled = ((glm->g_notag & (1<<tgt)) ? 0 : 1);
	glm_update_this_prop(glm, property, tq_enabled);
}

static void
glm_update_this_prop(struct glm *glm, char *property, int value)
{
	dev_info_t *dip = glm->g_dip;

	NDBG2(("glm_update_this_prop: prop=%s, value=%x", property, value));

	if (ddi_prop_update_int(DDI_DEV_T_NONE, dip,
	    property, value) != DDI_PROP_SUCCESS) {
		NDBG2(("cannot modify/create %s property.", property));
	}
}

static int
glm_alloc_active_slots(struct glm *glm, struct glm_unit *unit, int flag)
{
	int target = unit->nt_target;
	struct nt_slots *old_active = unit->nt_active;
	struct nt_slots *new_active;
	ushort_t size;
	int rval = -1;

	ASSERT(unit != NULL);

	if (unit->nt_tcmds) {
		NDBG9(("cannot change size of active slots array"));
		return (rval);
	}

	size = ((NOTAG(target)) ? GLM_NT_SLOT_SIZE : GLM_NT_SLOTS_SIZE_TQ);
	new_active = (struct nt_slots *)kmem_zalloc(size, flag);
	if (new_active == NULL) {
		NDBG1(("new active alloc failed\n"));
	} else {
		unit->nt_active = new_active;
		unit->nt_active->nt_n_slots = (NOTAG(target) ? 1 : NTAGS);
		unit->nt_active->nt_size = size;
		/*
		 * reserve tag 0 for non-tagged cmds to tagged targets
		 */
		if (TAGGED(target)) {
			unit->nt_active->nt_tags = 1;
		}
		if (old_active) {
			kmem_free((caddr_t)old_active, old_active->nt_size);
		}
		rval = 0;
	}

	/*
	 * Set the throttle.
	 */
	if (unit->nt_throttle != HOLD_THROTTLE) {
		unit->nt_throttle = ((NOTAG(target)) ? 1 : MAX_THROTTLE);
	}

	return (rval);
}

/*
 * Error logging, printing, and debug print routines.
 */
static char *glm_label = "glm";

static void
glm_log(struct glm *glm, int level, char *fmt, ...)
{
	dev_info_t *dev;
	va_list ap;

	if (glm) {
		dev = glm->g_dip;
	} else {
		dev = 0;
	}

	mutex_enter(&glm_log_mutex);

	va_start(ap, fmt);
	(void) vsprintf(glm_log_buf, fmt, ap);
	va_end(ap);

	if (level == CE_CONT) {
		scsi_log(dev, glm_label, level, "%s\n", glm_log_buf);
	} else {
		scsi_log(dev, glm_label, level, "%s", glm_log_buf);
	}

	mutex_exit(&glm_log_mutex);
}

#ifdef GLM_DEBUG
/*VARARGS1*/
static void
glm_printf(char *fmt, ...)
{
	dev_info_t *dev = 0;
	va_list	ap;

	mutex_enter(&glm_log_mutex);

	va_start(ap, fmt);
	(void) vsprintf(glm_log_buf, fmt, ap);
	va_end(ap);

#ifdef PROM_PRINTF
	prom_printf("%s:\t%s\n", glm_label, glm_log_buf);
#else
	scsi_log(dev, glm_label, SCSI_DEBUG, "%s\n", glm_log_buf);
#endif
	mutex_exit(&glm_log_mutex);
}
#endif

/*ARGSUSED*/
static void
glm_dump_cmd(struct glm *glm, struct glm_scsi_cmd *cmd)
{
	register i;
	register uchar_t *cp = (uchar_t *)cmd->cmd_pkt->pkt_cdbp;
	auto char buf[128];

	buf[0] = '\0';
	glm_log(glm, CE_NOTE, "?Cmd (0x%x) dump for Target %d Lun %d:\n", cmd,
	    Tgt(cmd), Lun(cmd));
	(void) sprintf(&buf[0], "\tcdb=[");
	for (i = 0; i < (int)cmd->cmd_cdblen; i++) {
		(void) sprintf(&buf[strlen(buf)], " 0x%x", *cp++);
	}
	(void) sprintf(&buf[strlen(buf)], " ]");
	glm_log(glm, CE_NOTE, "?%s\n", buf);
	glm_log(glm, CE_NOTE,
	    "?pkt_flags=0x%x pkt_statistics=0x%x pkt_state=0x%x\n",
	    cmd->cmd_pkt->pkt_flags, cmd->cmd_pkt->pkt_statistics,
	    cmd->cmd_pkt->pkt_state);
	glm_log(glm, CE_NOTE, "?pkt_scbp=0x%x cmd_flags=0x%x\n",
	    *(cmd->cmd_pkt->pkt_scbp), cmd->cmd_flags);
}

/*
 * timeout handling
 */
/*ARGSUSED*/
static void
glm_watch(void *arg)
{
	struct glm *glm;

	NDBG30(("glm_watch"));

	rw_enter(&glm_global_rwlock, RW_READER);
	for (glm = glm_head; glm != (struct glm *)NULL; glm = glm->g_next) {
		mutex_enter(&glm->g_mutex);

		/*
		 * For now, always call glm_watchsubr.
		 */
		glm_watchsubr(glm);

		if (glm->g_props_update) {
			int i;
			for (i = 0; i < NTARGETS_WIDE; i++) {
				if (glm->g_props_update & (1<<i)) {
					glm_update_props(glm, i);
				}
			}
			glm->g_props_update = 0;
		}
		mutex_exit(&glm->g_mutex);
	}
	rw_exit(&glm_global_rwlock);

	mutex_enter(&glm_global_mutex);
	glm_timeout_id = timeout(glm_watch, NULL, glm_tick);
	mutex_exit(&glm_global_mutex);
}

static void
glm_watchsubr(register struct glm *glm)
{
	struct glm_unit **unitp, *unit;
	register int cnt;
	register struct nt_slots *tag_slots;

	NDBG30(("glm_watchsubr: glm=%x\n", glm));

	/*
	 * we check in reverse order because during torture testing
	 * we really don't want to torture the root disk at 0 too often
	 */
	unitp = &glm->g_units[(N_GLM_UNITS-1)];

	for (cnt = (N_GLM_UNITS-1); cnt >= 0; cnt--) {
		if ((unit = *unitp--) == NULL) {
			continue;
		}

		if ((unit->nt_throttle > HOLD_THROTTLE) &&
		    (unit->nt_active &&
		    (unit->nt_active->nt_slot[0] == NULL))) {
			glm_full_throttle(glm, unit->nt_target, unit->nt_lun);
			if (unit->nt_waitq != NULL) {
				glm_queue_target(glm, unit);
			}
		}

#ifdef GLM_DEBUG
		if (unit->nt_state & NPT_STATE_QUEUED) {
			if (glm_hbaq_check(glm, unit) != TRUE) {
				glm_log(glm, CE_NOTE,
				    "target %d, botched state (%x).\n",
					unit->nt_target, unit->nt_state);
			}
		}
#endif

#ifdef GLM_TEST
		if (glm_enable_untagged) {
			glm_test_untagged++;
		}
#endif

		tag_slots = unit->nt_active;

		if ((unit->nt_tcmds > 0) && (tag_slots->nt_timebase)) {
			if (tag_slots->nt_timebase <=
			    glm_scsi_watchdog_tick) {
				tag_slots->nt_timebase +=
				    glm_scsi_watchdog_tick;
				continue;
			}

			tag_slots->nt_timeout -= glm_scsi_watchdog_tick;

			if (tag_slots->nt_timeout < 0) {
				glm_cmd_timeout(glm, unit);
				return;
			}
			if ((tag_slots->nt_timeout) <=
			    glm_scsi_watchdog_tick) {
				NDBG23(("pending timeout on (%d.%d)\n",
				    unit->nt_target, unit->nt_lun));
				glm_set_throttles(glm, 0, N_GLM_UNITS,
				    DRAIN_THROTTLE);
			}
		}

#ifdef GLM_TEST
		if (glm->g_instance == glm_test_instance) {
			glm_test_reset(glm, unit);
			glm_test_abort(glm, unit);
		}
#endif
	}
}

/*
 * timeout recovery
 */
static void
glm_cmd_timeout(struct glm *glm, struct glm_unit *unit)
{
	int i, n, tag, ncmds;
	struct glm_scsi_cmd *sp = NULL;
	struct glm_scsi_cmd *ssp;

	/*
	 * set throttle back; no more draining necessary
	 */
	glm_full_throttle(glm, unit->nt_target, unit->nt_lun);

	if (NOTAG(unit->nt_target)) {
		sp = unit->nt_active->nt_slot[0];
	}

	/*
	 * If the scripts processor is active and there is no interrupt
	 * pending for next second then the current sp must be stuck;
	 * switch to current unit and sp.
	 */
	NDBG29(("glm_cmd_timeout: g_state=%x, unit=%x, current=%x",
		glm->g_state, unit, glm->g_current));

	if (glm->g_state == NSTATE_ACTIVE) {
		for (i = 0; (i < 10000) && (INTPENDING(glm) == 0); i++) {
			drv_usecwait(100);
		}
		if (INTPENDING(glm) == 0) {
			ASSERT(glm->g_current != NULL);
			unit = glm->g_current;
			sp = unit->nt_ncmdp;
		}
		NDBG29(("glm_cmd_timeout: new unit=%x", unit));
	}

#ifdef GLM_DEBUG
	/*
	 * See if we have a connected cmd timeout.
	 * if the hba is currently working on a cmd (g_current != NULL)
	 * and the cmd in g_current->nt_ncmp equal to a cmd in the unit
	 * active g_slot, this cmd is connected and timed out.
	 *
	 * This is used mainly for reset torture testing (sd_io_time=1).
	 */
	if (TAGGED(unit->nt_target) && glm->g_current &&
	    (glm->g_current == unit)) {
		if (glm->g_current->nt_ncmdp != NULL) {
			ssp = glm->g_current->nt_ncmdp;
			if (unit->nt_active->nt_slot[ssp->cmd_tag[1]] != NULL) {
				sp = unit->nt_active->nt_slot[ssp->cmd_tag[1]];
				if (sp != ssp) {
					sp = NULL;
				}
			}
		}
	}
#endif

	/*
	 * update all outstanding pkts for this unit.
	 */
	n = unit->nt_active->nt_n_slots;
	for (ncmds = tag = 0; tag < n; tag++) {
		ssp = unit->nt_active->nt_slot[tag];
		if (ssp && ssp->cmd_pkt->pkt_time) {
			glm_set_pkt_reason(glm, ssp, CMD_TIMEOUT,
				STAT_TIMEOUT | STAT_ABORTED);
			ssp->cmd_pkt->pkt_state |= (STATE_GOT_BUS |
			    STATE_GOT_TARGET | STATE_SENT_CMD);
			glm_dump_cmd(glm, ssp);
			ncmds++;
		}
	}

	/*
	 * no timed-out cmds here?
	 */
	if (ncmds == 0) {
		return;
	}

	if (sp) {
		/*
		 * dump all we know about this timeout
		 */
		if (sp->cmd_flags & CFLAG_CMDDISC) {
			glm_log(glm, CE_WARN,
			    "Disconnected command timeout for Target %d.%d",
				unit->nt_target, unit->nt_lun);
			cmn_err(CE_WARN, "!ID[SUNWpd.glm.cmd_timeout.6016]");
		} else {
			ASSERT(unit == glm->g_current);
			glm_log(glm, CE_WARN,
			    "Connected command timeout for Target %d.%d",
				unit->nt_target, unit->nt_lun);
			cmn_err(CE_WARN, "!ID[SUNWpd.glm.cmd_timeout.6017]");
			/*
			 * connected cmd timeout are usually due to noisy buses.
			 */
			glm_sync_wide_backoff(glm, unit);
		}
	} else {
		glm_log(glm, CE_WARN,
		    "Disconnected tagged cmd(s) (%d) timeout for Target %d.%d",
			unit->nt_tcmds, unit->nt_target, unit->nt_lun);
		cmn_err(CE_WARN, "!ID[SUNWpd.glm.cmd_timeout.6018]");
	}
	(void) glm_abort_cmd(glm, unit, sp);
}

static int
glm_abort_cmd(struct glm *glm, struct glm_unit *unit, struct glm_scsi_cmd *cmd)
{
	struct scsi_address ap;
	ap.a_hba_tran = glm->g_tran;
	ap.a_target = unit->nt_target;
	ap.a_lun = unit->nt_lun;

	NDBG29(("glm_abort_cmd: unit=%x cmd=%x", unit, cmd));

	/*
	 * If the current target is not the target passed in,
	 * try to reset that target.
	 */
	if (glm->g_current == NULL ||
	    ((glm->g_current && (glm->g_current != unit)))) {
		/*
		 * give up on this command
		 */
		NDBG29(("glm_abort_cmd device reset"));
		if (glm_do_scsi_reset(&ap, RESET_TARGET)) {
			return (TRUE);
		}
	}

	/*
	 * if the target won't listen, then a retry is useless
	 * there is also the possibility that the cmd still completed while
	 * we were trying to reset and the target driver may have done a
	 * device reset which has blown away this cmd.
	 * well, we've tried, now pull the chain
	 */
	NDBG29(("glm_abort_cmd bus reset"));
	return (glm_do_scsi_reset(&ap, RESET_ALL));
}

/*
 * auto request sense support
 * create or destroy an auto request sense packet
 */
static int
glm_create_arq_pkt(struct glm_unit *unit, struct scsi_address *ap)
{
	register struct glm_scsi_cmd *rqpktp;
	register struct buf *bp;
	struct arq_private_data *arq_data;

	NDBG8(("glm_create_arq_pkt: target=%x", ap->a_target));

	if (unit->nt_arq_pkt != 0) {
		return (0);
	}

	/*
	 * it would be nicer if we could allow the target driver
	 * to specify the size but this is easier and OK for most
	 * drivers to use SENSE_LENGTH
	 * Allocate a request sense packet.
	 */
	bp = scsi_alloc_consistent_buf(ap, (struct buf *)NULL,
	    SENSE_LENGTH, B_READ, SLEEP_FUNC, NULL);
	rqpktp = PKT2CMD(scsi_init_pkt(ap,
	    NULL, bp, CDB_GROUP0, 1, PKT_PRIV_LEN,
		PKT_CONSISTENT, SLEEP_FUNC, NULL));
	arq_data =
	    (struct arq_private_data *)(rqpktp->cmd_pkt->pkt_private);
	arq_data->arq_save_bp = bp;

	RQ_MAKECOM_G0((CMD2PKT(rqpktp)),
	    FLAG_SENSING | FLAG_HEAD | FLAG_NODISCON,
		(char)SCMD_REQUEST_SENSE, 0, (char)SENSE_LENGTH);
	rqpktp->cmd_flags |= CFLAG_CMDARQ;
	rqpktp->cmd_pkt->pkt_ha_private = rqpktp;
	unit->nt_arq_pkt = rqpktp;

	/*
	 * we need a function ptr here so abort/reset can
	 * defer callbacks; glm_call_pkt_comp() calls
	 * glm_complete_arq_pkt() directly without releasing the lock
	 * However, since we are not calling back directly thru
	 * pkt_comp, don't check this with warlock
	 */
	rqpktp->cmd_pkt->pkt_comp =
		(void (*)(struct scsi_pkt *))glm_complete_arq_pkt;
	return (0);
}

/*ARGSUSED*/
static int
glm_delete_arq_pkt(struct glm_unit *unit, struct scsi_address *ap)
{
	register struct glm_scsi_cmd *rqpktp;

	NDBG8(("glm_delete_arq_pkt: target=%x", ap->a_target));

	/*
	 * if there is still a pkt saved or no rqpkt
	 * then we cannot deallocate or there is nothing to do
	 */
	if ((rqpktp = unit->nt_arq_pkt) != NULL) {
		struct arq_private_data *arq_data =
		    (struct arq_private_data *)(rqpktp->cmd_pkt->pkt_private);
		struct buf *bp = arq_data->arq_save_bp;
		/*
		 * is arq pkt in use?
		 */
		if (arq_data->arq_save_sp) {
			return (-1);
		}

		scsi_destroy_pkt(CMD2PKT(rqpktp));
		scsi_free_consistent_buf(bp);
	}
	return (0);
}

/*
 * complete an arq packet by copying over transport info and the actual
 * request sense data.
 */
static void
glm_complete_arq_pkt(struct scsi_pkt *pkt)
{
	register struct glm *glm;
	register struct glm_unit *unit;
	register struct glm_scsi_cmd *sp;
	register struct scsi_arq_status *arqstat;
	register struct arq_private_data *arq_data;
	register struct glm_scsi_cmd *ssp;
	register struct buf *bp;

	glm = ADDR2GLM(&pkt->pkt_address);

	mutex_enter(&glm->g_mutex);
	unit = PKT2GLMUNITP(pkt);
	sp = pkt->pkt_ha_private;
	arq_data = (struct arq_private_data *)sp->cmd_pkt->pkt_private;
	ssp = arq_data->arq_save_sp;
	bp = arq_data->arq_save_bp;

	NDBG8(("glm_complete_arq_pkt: target=%d pkt=%x, ssp=%x",
	    unit->nt_target, pkt, ssp));

	ASSERT(unit != NULL);
	ASSERT(sp == unit->nt_arq_pkt);
	ASSERT(arq_data->arq_save_sp != NULL);
	ASSERT(ssp->cmd_flags & CFLAG_ARQ_IN_PROGRESS);

	ssp->cmd_flags &= ~CFLAG_ARQ_IN_PROGRESS;
	arqstat = (struct scsi_arq_status *)(ssp->cmd_pkt->pkt_scbp);
	arqstat->sts_rqpkt_status = *((struct scsi_status *)
		(sp->cmd_pkt->pkt_scbp));
	arqstat->sts_rqpkt_reason = sp->cmd_pkt->pkt_reason;
	arqstat->sts_rqpkt_state  = sp->cmd_pkt->pkt_state;
	arqstat->sts_rqpkt_statistics = sp->cmd_pkt->pkt_statistics;
	arqstat->sts_rqpkt_resid  = sp->cmd_pkt->pkt_resid;
	arqstat->sts_sensedata =
	    *((struct scsi_extended_sense *)bp->b_un.b_addr);
	ssp->cmd_pkt->pkt_state |= STATE_ARQ_DONE;
	arq_data->arq_save_sp = NULL;

	/*
	 * ASC=0x47 is parity error
	 */
	if (arqstat->sts_sensedata.es_key == KEY_ABORTED_COMMAND &&
	    arqstat->sts_sensedata.es_add_code == 0x47) {
		glm_sync_wide_backoff(glm, unit);
	}

	/*
	 * complete the saved sp.
	 */
	ASSERT(ssp->cmd_flags & CFLAG_CMD_REMOVED);
	ssp->cmd_flags |= CFLAG_FINISHED;
	glm_doneq_add(glm, ssp);

	mutex_exit(&glm->g_mutex);
}

/*
 * handle check condition and start an arq packet
 */
static int
glm_handle_sts_chk(struct glm *glm, struct glm_unit *unit,
    struct glm_scsi_cmd *sp)
{
	register struct glm_scsi_cmd *arqsp = unit->nt_arq_pkt;
	register struct arq_private_data *arq_data;
	register struct buf *bp;

	NDBG8(("glm_handle_sts_chk: target=%d unit=%x sp=%x",
	    unit->nt_target, unit, sp));

	if ((arqsp == NULL) || (arqsp == sp) ||
	    (sp->cmd_scblen < sizeof (struct scsi_arq_status))) {
		NDBG8(("no arq packet or cannot arq on arq pkt"));
		return (0);
	}

	arq_data = (struct arq_private_data *)arqsp->cmd_pkt->pkt_private;
	bp = arq_data->arq_save_bp;

	ASSERT(sp != unit->nt_active->nt_slot[sp->cmd_tag[1]]);

	if (arq_data->arq_save_sp != NULL) {
		NDBG8(("auto request sense already in progress\n"));
		goto fail;
	}

	arq_data->arq_save_sp = sp;
	bzero((caddr_t)bp->b_un.b_addr, sizeof (struct scsi_extended_sense));

	/*
	 * copy the timeout from the original packet by lack of a better
	 * value
	 * we could take the residue of the timeout but that could cause
	 * premature timeouts perhaps
	 */
	arqsp->cmd_pkt->pkt_time = sp->cmd_pkt->pkt_time;
	arqsp->cmd_flags &= ~(CFLAG_TRANFLAG | CFLAG_CMD_REMOVED);
	arqsp->cmd_type = NRQ_NORMAL_CMD;
	ASSERT(arqsp->cmd_pkt->pkt_comp != NULL);

	sp->cmd_flags |= CFLAG_ARQ_IN_PROGRESS;

	/*
	 * Make sure arq goes out on bus.
	 */
	glm_full_throttle(glm, unit->nt_target, unit->nt_lun);
	(void) glm_accept_pkt(glm, arqsp);
	return (0);
fail:
	glm_set_pkt_reason(glm, sp, CMD_TRAN_ERR, 0);
	glm_log(glm, CE_WARN, "auto request sense failed");
	cmn_err(CE_WARN, "!ID[SUNWpd.glm.handle_sts_check.6019]");
	glm_dump_cmd(glm, sp);
	return (-1);
}

/*
 * Device / Hotplug control
 */
/* ARGSUSED */
static int
glm_quiesce_bus(struct glm *glm)
{
	NDBG28(("glm_quiesce_bus"));
	return (-1);
}

/* ARGSUSED */
static int
glm_unquiesce_bus(struct glm *glm)
{
	NDBG28(("glm_unquiesce_bus"));
	return (-1);
}

/* ARGSUSED3 */
static int
glm_open(dev_t *devp, int flags, int otyp, cred_t *credp)
{
	int instance;
	struct glm *glm;

	NDBG28(("glm_open: devp=%p flags=%x otyp=%x credp=%p",
		devp, flags, otyp, credp));

	if (otyp != OTYP_CHR)
		return (EINVAL);

	instance = getminor(*devp);
	glm = (struct glm *)ddi_get_soft_state(glm_state, instance);
	if (glm == NULL)
		return (ENXIO);

	mutex_enter(&glm->g_mutex);
	if ((flags & FEXCL) && (glm->g_softstate & GLM_SS_ISOPEN)) {
		mutex_exit(&glm->g_mutex);
		return (EBUSY);
	}

	glm->g_softstate |= GLM_SS_ISOPEN;
	mutex_exit(&glm->g_mutex);
	return (0);
}

/* ARGSUSED */
static int
glm_close(dev_t dev, int flag, int otyp, cred_t *credp)
{
	int instance;
	struct glm *glm;

	NDBG28(("glm_close: flag=%x otyp=%x credp=%p", flag, otyp,
		credp));

	if (otyp != OTYP_CHR)
		return (EINVAL);

	instance = getminor(dev);
	glm = (struct glm *)ddi_get_soft_state(glm_state, instance);

	if (glm == NULL)
		return (ENXIO);

	mutex_enter(&glm->g_mutex);
	glm->g_softstate &= ~GLM_SS_ISOPEN;
	mutex_exit(&glm->g_mutex);
	return (0);
}

/*
 * glm_ioctl: devctl hotplug controls
 */
/* ARGSUSED */
static int
glm_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp, int *rvalp)
{
	struct glm *glm;
	dev_info_t *self;
	dev_info_t *child_dip = NULL;
	struct devctl_iocdata *dcp;
	uint_t bus_state;
	int instance;
	int rv = 0;
	int nrv = 0;
	uint_t cmd_flags = 0;

	NDBG28(("glm_ioctl: dev=%x cmd=%x arg=%lx mode=%x credp=%p",
		dev, cmd, arg, mode, credp));

	instance = getminor(dev);
	glm = (struct glm *)ddi_get_soft_state(glm_state, instance);

	if (glm == NULL)
		return (ENXIO);

	self = (dev_info_t *)glm->g_dip;

	/*
	 * read devctl ioctl data
	 */
	if (ndi_dc_allochdl((void *)arg, &dcp) != NDI_SUCCESS)
		return (EFAULT);

	switch (cmd) {
	case DEVCTL_DEVICE_GETSTATE:
		if (ndi_dc_getname(dcp) == NULL ||
		    ndi_dc_getaddr(dcp) == NULL) {
			rv = EINVAL;
			break;
		}

		/*
		 * lookup and hold child device
		 */
		child_dip = ndi_devi_find(self,
		    ndi_dc_getname(dcp), ndi_dc_getaddr(dcp));
		if (child_dip == NULL) {
			rv = ENXIO;
			break;
		}

		if (ndi_dc_return_dev_state(child_dip, dcp) != NDI_SUCCESS)
			rv = EFAULT;
		break;

	case DEVCTL_DEVICE_ONLINE:
		if (ndi_dc_getname(dcp) == NULL ||
		    ndi_dc_getaddr(dcp) == NULL) {
			rv = EINVAL;
			break;
		}

		/*
		 * lookup and hold child device
		 */
		child_dip = ndi_devi_find(self,
		    ndi_dc_getname(dcp), ndi_dc_getaddr(dcp));
		if (child_dip == NULL) {
			rv = ENXIO;
			break;
		}

		if (ndi_devi_online(child_dip, 0) != NDI_SUCCESS)
			rv = EIO;

		break;

	case DEVCTL_DEVICE_OFFLINE:
		if (ndi_dc_getname(dcp) == NULL ||
		    ndi_dc_getaddr(dcp) == NULL) {
			rv = EINVAL;
			break;
		}

		/*
		 * lookup child device
		 */
		child_dip = ndi_devi_find(self,
		    ndi_dc_getname(dcp), ndi_dc_getaddr(dcp));

		if (child_dip == NULL) {
			rv = ENXIO;
			break;
		}

		nrv = ndi_devi_offline(child_dip, cmd_flags);

		if (nrv == NDI_BUSY)
			rv = EBUSY;
		else if (nrv == NDI_FAILURE)
			rv = EIO;
		break;

	case DEVCTL_DEVICE_RESET:
		rv = ENOTSUP;
		break;


	case DEVCTL_BUS_QUIESCE:
		if (ndi_get_bus_state(self, &bus_state) == NDI_SUCCESS)
			if (bus_state == BUS_QUIESCED)
				break;

		if (glm_quiesce_bus(glm) != 0)
			rv = EIO;
		else
			(void) ndi_set_bus_state(self, BUS_QUIESCED);
		break;

	case DEVCTL_BUS_UNQUIESCE:
		if (ndi_get_bus_state(self, &bus_state) == NDI_SUCCESS)
			if (bus_state == BUS_ACTIVE)
				break;

		if (glm_unquiesce_bus(glm) != 0)
			rv = EIO;
		else
			(void) ndi_set_bus_state(self, BUS_ACTIVE);
		break;

	case DEVCTL_BUS_RESET:
	case DEVCTL_BUS_RESETALL:
		/*
		 * reset
		 */
		glm_reset_bus(glm);
		break;

	case DEVCTL_BUS_GETSTATE:
		if (ndi_dc_return_bus_state(self, dcp) != NDI_SUCCESS)
			rv = EFAULT;
		break;

	default:
		rv = ENOTTY;
	}

	ndi_dc_freehdl(dcp);
	return (rv);
}

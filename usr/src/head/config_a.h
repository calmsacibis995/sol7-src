/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_CONFIG_ADMIN_H
#define	_SYS_CONFIG_ADMIN_H

#pragma ident	"@(#)config_admin.h	1.3	98/01/23 SMI"

/*
 * config_admin.h
 *
 * this file supports usage of the interfaces defined in
 * config_admin.3x. which are contained in /usr/lib/libcfgadm.so.1
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Defined constants
 */
#define	CFGA_AP_LOG_ID_LEN	20
#define	CFGA_AP_PHYS_ID_LEN	MAXPATHLEN
#define	CFGA_INFO_LEN		4096
#define	CFGA_TYPE_LEN		12

/*
 * Configuration change state commands
 */
typedef enum {
	CFGA_CMD_NONE = 0,
	CFGA_CMD_LOAD,
	CFGA_CMD_UNLOAD,
	CFGA_CMD_CONNECT,
	CFGA_CMD_DISCONNECT,
	CFGA_CMD_CONFIGURE,
	CFGA_CMD_UNCONFIGURE
} cfga_cmd_t;

/*
 * Configuration states
 */
typedef enum {
	CFGA_STAT_NONE = 0,
	CFGA_STAT_EMPTY,
	CFGA_STAT_DISCONNECTED,
	CFGA_STAT_CONNECTED,
	CFGA_STAT_UNCONFIGURED,
	CFGA_STAT_CONFIGURED
} cfga_stat_t;

/*
 * Configuration conditions
 */
typedef enum {
	CFGA_COND_UNKNOWN = 0,
	CFGA_COND_OK,
	CFGA_COND_FAILING,
	CFGA_COND_FAILED,
	CFGA_COND_UNUSABLE
} cfga_cond_t;

/*
 * Flags
 */
#define	CFGA_FLAG_FORCE		1
#define	CFGA_FLAG_VERBOSE	2

typedef char cfga_ap_log_id_t[CFGA_AP_LOG_ID_LEN];
typedef char cfga_ap_phys_id_t[CFGA_AP_PHYS_ID_LEN];
typedef char cfga_info_t[CFGA_INFO_LEN];
typedef char cfga_type_t[CFGA_TYPE_LEN];
typedef int cfga_flags_t;
typedef int cfga_busy_t;

typedef struct cfga_stat_data {
	cfga_ap_log_id_t ap_log_id;	/* Attachment point logical id */
	cfga_ap_phys_id_t ap_phys_id;	/* Attachment point physical id */
	cfga_stat_t	ap_r_state;	/* Receptacle state */
	cfga_stat_t	ap_o_state;	/* Occupant state */
	cfga_cond_t	ap_cond;	/* Attachment point condition */
	cfga_busy_t	ap_busy;	/* Busy indicators */
	time_t		ap_status_time;	/* Attachment point last change */
	cfga_info_t	ap_info;	/* Miscellaneous information */
	cfga_type_t	ap_type;	/* Occupant type */
} cfga_stat_data_t;

struct cfga_confirm {
	int (*confirm)(void *appdata_ptr, const char *message);
	void *appdata_ptr;
};

struct cfga_msg {
	int (*message_routine)(void *appdata_ptr, const char *message);
	void *appdata_ptr;
};

/*
 * Library function error codes returned by all functions below
 * except config_strerror which is used to decode the error
 * codes.
 */
typedef enum {
	CFGA_OK = 0,
	CFGA_NACK,
	CFGA_NOTSUPP,
	CFGA_OPNOTSUPP,
	CFGA_PRIV,
	CFGA_BUSY,
	CFGA_SYSTEM_BUSY,
	CFGA_DATA_ERROR,
	CFGA_LIB_ERROR,
	CFGA_NO_LIB,
	CFGA_INSUFFICENT_CONDITION,
	CFGA_INVAL,
	CFGA_ERROR
} cfga_err_t;


#if defined(__STDC__)

/*
 * config_admin.3x library interfaces
 */

cfga_err_t config_change_state(int state_change_cmd, int num_ap_ids,
    char *const *ap_ids, const char *options, struct cfga_confirm *confp,
    struct cfga_msg *msgp, char **errstring, cfga_flags_t flags);

cfga_err_t config_private_func(const char *function, int num_ap_ids,
    char *const *ap_ids, const char *options, struct cfga_confirm *confp,
    struct cfga_msg *msgp, char **errstring, cfga_flags_t flags);

cfga_err_t config_test(int num_ap_ids, char *const *ap_ids,
    const char *options, struct cfga_msg *msgp, char **errstring,
    cfga_flags_t flags);

cfga_err_t config_stat(int num_ap_ids, char *const *ap_ids,
    struct cfga_stat_data *buf, const char *options, char **errstring);

cfga_err_t config_list(struct cfga_stat_data **ap_di_list, int *nlist,
    const char *options, char **errstring);

cfga_err_t config_help(int num_ap_ids, char *const *ap_ids,
    struct cfga_msg *msgp, const char *options, cfga_flags_t flags);

const char *config_strerror(int cfgerrnum);

int config_ap_id_cmp(const cfga_ap_log_id_t ap_id1,
    const cfga_ap_log_id_t ap_id2);

void config_unload_libs();

#ifdef CFGA_PLUGIN_LIB
/*
 * Plugin library routine hooks - only to be used by the generic
 * library and plugin libraries (who must define CFGA_PLUGIN_LIB
 * prior to the inclusion of this header).
 */

static cfga_err_t cfga_change_state(int, const char *, const char *,
    struct cfga_confirm *, struct cfga_msg *, char **, cfga_flags_t);
static cfga_err_t cfga_private_func(const char *, const char *, const char *,
    struct cfga_confirm *, struct cfga_msg *, char **, cfga_flags_t);
static cfga_err_t cfga_test(const char *, const char *, struct cfga_msg *,
    char **, cfga_flags_t);
static cfga_err_t cfga_stat(const char *, struct cfga_stat_data *,
    const char *, char **);
static cfga_err_t cfga_list(const char *, struct cfga_stat_data **,
    int *, const char *, char **);
static cfga_err_t cfga_help(struct cfga_msg *, const char *, cfga_flags_t);
static int cfga_ap_id_cmp(const cfga_ap_log_id_t,
    const cfga_ap_log_id_t);

#endif /* CFGA_PLUGIN_LIB */

#else /* !defined __STDC__ */

extern const char *config_strerror();
extern const char *__config_strerror();
extern int config_ap_id_cmp();

#endif /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_CONFIG_ADMIN_H */

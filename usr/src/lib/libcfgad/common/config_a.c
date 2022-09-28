/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)config_admin.c	1.5	98/01/23 SMI"

/*
 * This define is used for 2.6 where the libdevinfo code is
 * not available to detect Attachment points
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <langinfo.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dlfcn.h>
#include <synch.h>
#include <sys/systeminfo.h>
#include <sys/sunddi.h>
#include <libdevinfo.h>

#define	CFGA_PLUGIN_LIB
#include <config_admin.h>

/*
 * global to control library init
 */
static int config_admin_inited;

/* Limit size of sysinfo return */
#define	SYSINFO_LENGTH	256

/*
 * Structure that contains plugin library information.
 */
typedef struct plugin_lib {
	struct	plugin_lib *next;	/* pointer to next */
	void	*handle;		/* handle from dlopen */
	char	libpath[MAXPATHLEN];	/* full pathname to lib */
	char	ap_phys[MAXPATHLEN];	/* physical ap */
	char	ap_logid[MAXPATHLEN];	/* physical ap */
	int	instance;		/* instance of ap controller */
	rwlock_t entry_lock;		/* protects this entry */
					/* function pointers */
	cfga_err_t	(*cfga_change_state_p)();
	cfga_err_t	(*cfga_private_func_p)();
	cfga_err_t	(*cfga_test_p)();
	cfga_err_t	(*cfga_stat_p)();
	cfga_err_t	(*cfga_list_p)();
	cfga_err_t	(*cfga_help_p)();
	int		(*cfga_ap_id_cmp_p)();
} plugin_lib_t;

static plugin_lib_t plugin_list;

/*
 * Library locator data struct - used to pass down through the device
 * tree walking code.
 */
typedef struct lib_locator {
	char	ap_arg[MAXPATHLEN];
	char	ap_logical[MAXPATHLEN];
	char	pathname[MAXPATHLEN];
	char	ap_physical[MAXPATHLEN];
	plugin_lib_t *libp;
} lib_loc_t;

/*
 * linked list of cfga_stat_data structs - used for
 * config_list
 */
typedef struct stat_data_list {
	struct stat_data_list	*next;
	cfga_stat_data_t	stat_data;
} stat_data_list_t;

/*
 * encapsulate config_list args to get them through the tree
 * walking code
 */
typedef struct list_stat {
	int	*count;			/* list args */
	const char *opts;		/* list args */
	char **errstr;			/* list args */
	stat_data_list_t *sdl;
} list_stat_t;

/*
 * Lock to protect list of libraries
 */
rwlock_t plugin_list_lock;

/*
 * Forward declarations
 */

static const char *__config_strerror(int);
static void *config_calloc_check(size_t, size_t, cfga_err_t *);
static cfga_err_t get_next_lib(plugin_lib_t **);
static cfga_err_t resolve_lib_ref(plugin_lib_t *);
static cfga_err_t config_get_lib(char *, lib_loc_t *);
static void config_init();
static int check_ap(di_node_t, di_minor_t, void *);
static int check_ap_phys(di_node_t, di_minor_t, void *);
static void find_all_aps();
static cfga_err_t find_ap(lib_loc_t *);
static cfga_err_t find_ap_phys(lib_loc_t *);
static plugin_lib_t *lib_in_list(char *, int);
static int add_ap_lib(di_node_t, di_minor_t, void *);
static cfga_err_t load_lib(di_node_t, di_minor_t, lib_loc_t *);
static void release_lib(plugin_lib_t *);
extern void bcopy(const void *, void *, size_t);
static cfga_err_t stat_all_aps(struct cfga_stat_data **, int *,
    const char *, char **);
int isalpha(int);

/*
 * Plugin library search path helpers
 */

#define	LIB_PATH_BASE1	"/usr/platform/"
#define	LIB_PATH_BASE2	"/usr/lib/cfgadm/"
#ifdef __sparcv9
#define	LIB_PATH_MIDDLE	"/lib/cfgadm/sparcv9/"
#else
#define	LIB_PATH_MIDDLE	"/lib/cfgadm/"
#endif
#define	LIB_PATH_TAIL	".so.1"
#define	NUM_LIB_NAMES	2

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif


/*
 * Defined constants
 */

#define	MAX_LIB_NAMES 2
#define	MAX_LIB_PATHS 3

#define	IS_PHYSICAL(a)	(strncmp((a), "/", 1) == 0)

/*
 * Public interfaces for libcfgadm, as documented in config_admin.3x
 */

/*
 * config_change_state
 */

cfga_err_t
config_change_state(
	int state_change_cmd,
	int num_ap_ids,
	char *const *ap_id,
	const char *options,
	struct cfga_confirm *confp,
	struct cfga_msg *msgp,
	char **errstring,
	cfga_flags_t flags)
{
	/*
	 * foreach arg -
	 *  load hs library,
	 *  if force
	 *    call cfga_state_change_func
	 *    return status
	 *  else
	 *    call it's cfga_stat
	 *    check condition
	 *    call cfga_state_change_func
	 *    return status
	 */
	int i;
	cfga_stat_data_t stat_buffer;
	char phys_name[MAXPATHLEN];
	lib_loc_t libloc;
	plugin_lib_t *libp;

	cfga_err_t retval = CFGA_OK;

	/*
	 * operate on each ap_id
	 */
	for (i = 0; (i < num_ap_ids) && (retval == CFGA_OK); i++) {
		if ((retval = config_get_lib(ap_id[i], &libloc)) != CFGA_OK)
			break;

		libp = libloc.libp;
		if (flags & CFGA_FLAG_FORCE) {
			errno = 0;
			retval = (*libp->cfga_change_state_p)
			    (state_change_cmd, libloc.ap_physical, options,
			    confp, msgp, errstring, flags);
		} else {
			errno = 0;
			if ((retval = (*libp->cfga_stat_p)(
			    libloc.ap_physical, &stat_buffer, "", errstring))
			    == CFGA_OK) {
				if ((stat_buffer.ap_cond == CFGA_COND_OK) ||
				    (stat_buffer.ap_cond ==
				    CFGA_COND_UNKNOWN)) {
					errno = 0;
					retval =
					    (*libp->cfga_change_state_p)(
					    state_change_cmd,
					    libloc.ap_physical, options,
					    confp, msgp, errstring,
					    flags);
				} else {
					retval = CFGA_INSUFFICENT_CONDITION;
				}
			}
		}
	}
	return (retval);
}

/*
 * config_private_func
 */

cfga_err_t
config_private_func(
	const char *function,
	int num_ap_ids,
	char *const *ap_ids,
	const char *options,
	struct cfga_confirm *confp,
	struct cfga_msg *msgp,
	char **errstring,
	cfga_flags_t flags)
{
	int i;
	char phys_name[MAXPATHLEN];
	lib_loc_t libloc;
	cfga_err_t retval = CFGA_OK;

	/*
	 * operate on each ap_id
	 */
	for (i = 0; (i < num_ap_ids) && (retval == CFGA_OK); i++) {
		if ((retval = config_get_lib(ap_ids[i], &libloc)) != CFGA_OK)
			return (retval);

		errno = 0;
		retval = (*libloc.libp->cfga_private_func_p)(function,
		    libloc.ap_physical, options, confp,  msgp, errstring,
		    flags);
	}
	return (retval);
}


/*
 * config_test
 */

cfga_err_t
config_test(
	int num_ap_ids,
	char *const *ap_ids,
	const char *options,
	struct cfga_msg *msgp,
	char **errstring,
	cfga_flags_t flags)
{
	int i;
	lib_loc_t libloc;
	char phys_name[MAXPATHLEN];
	cfga_err_t retval = CFGA_OK;

	/*
	 * operate on each ap_id
	 */
	for (i = 0; (i < num_ap_ids) && (retval == CFGA_OK); i++) {
		if ((retval = config_get_lib(ap_ids[i], &libloc)) != CFGA_OK)
			return (retval);
		errno = 0;
		retval = (*libloc.libp->cfga_test_p)(libloc.ap_physical,
		    options, msgp, errstring, flags);
	}
	return (retval);
}

/*
 * config_stat
 */

cfga_err_t
config_stat(
	int num_ap_ids,
	char *const *ap_ids,
	struct cfga_stat_data *buf,
	const char *options,
	char **errstring)
{
	int i;
	char phys_name[MAXPATHLEN];
	char log_name[MAXPATHLEN];
	char tmp[MAXPATHLEN];
	lib_loc_t libloc;
	plugin_lib_t *libp;
	char *cp;
	cfga_err_t retval = CFGA_OK;

	/*
	 * operate on each ap_id
	 */
	libloc.libp = (plugin_lib_t *)NULL;
	for (i = 0; i < num_ap_ids; i++, buf++) {
		if ((retval = config_get_lib(ap_ids[i], &libloc)) != CFGA_OK)
			break;

		libp = libloc.libp;
		errno = 0;
		if ((retval = (*libp->cfga_stat_p)(libloc.ap_physical,
		    buf, options, errstring)) != CFGA_OK)
			break;

		(void) strncpy(buf->ap_log_id, libp->ap_logid,
		    CFGA_AP_LOG_ID_LEN);

		(void) strncpy(buf->ap_phys_id, libp->ap_phys,
		    CFGA_AP_PHYS_ID_LEN);
	}
	return (retval);
}

/*
 * config_list
 */

cfga_err_t
config_list(
	struct cfga_stat_data **ap_di_list,
	int *nlist,
	const char *options,
	char **errstring)
{
	char log_name[MAXPATHLEN];
	int nlist_temp;
	cfga_stat_data_t *data_temp, *new_temp;
	cfga_err_t retval = CFGA_OK;
	cfga_err_t calloc_error = CFGA_OK;
	static plugin_lib_t *libp;
	int i;

	config_init();
	retval = stat_all_aps(ap_di_list, nlist, options, errstring);
	if ((retval == CFGA_OK) && (*nlist == 0))
		return (CFGA_NOTSUPP);
	else
		return (retval);

}

/*
 * config_unload_libs
 *
 * Attempts to remove all libs on the plugin list.
 */

void
config_unload_libs()
{
	plugin_lib_t *libp, *libp_next, *libp_last;

	/* write lock the lib list */
	(void) rw_wrlock(&plugin_list_lock);
	/* free up list of libs */
	libp_last = &plugin_list;
	for (libp = plugin_list.next;
	    libp != &plugin_list;
	    libp = libp->next) {
		/* skip entries that are being used */
		if (rw_trywrlock(&libp->entry_lock) != 0) {
			/*
			 * only bump this when skipping over an
			 * entry that is in use.
			 */
			libp_last = libp;
			continue;
		}
		/*
		 * remove the entry and clean it up
		 */
		libp_last->next = libp->next;
		(void) rw_unlock(&libp->entry_lock);
		(void) rwlock_destroy(&libp->entry_lock);
		if (libp->handle != (void *)NULL) {
			(void) dlclose(libp->handle);
			libp->handle = (void *)NULL;
		}
		free(libp);
		/* reset libp back to previous */
		libp = libp_last;
	}
	(void) rw_unlock(&plugin_list_lock);
}

/*
 * config_ap_id_cmp
 */

int
config_ap_id_cmp(
	const cfga_ap_log_id_t ap1,
	const cfga_ap_log_id_t ap2)
{
	lib_loc_t libloc;
	cfga_err_t retval = CFGA_OK;

	if ((retval = config_get_lib((char *)ap1, &libloc)) != CFGA_OK)
		return (strncmp(ap1, ap2, MAXPATHLEN));

	return ((*libloc.libp->cfga_ap_id_cmp_p)(ap1, ap2));
}

/*
 * config_strerror
 */

const char *
config_strerror(
	int cfgerrnum)
{
	const char *ep;

	ep = __config_strerror(cfgerrnum);

	return ((ep != NULL) ? dgettext(TEXT_DOMAIN, ep) : NULL);
}

/*
 * config_help
 */

cfga_err_t
config_help(
	int num_ap_ids,
	char *const *ap_ids,
	struct cfga_msg *msgp,
	const char *options,
	cfga_flags_t flags)
{
	int i;
	lib_loc_t libloc;
	cfga_err_t retval = CFGA_OK;

	if (num_ap_ids != 0) {
		/*
		 * operate on each ap_id
		 */
		for (i = 0; (i < num_ap_ids) && (retval == CFGA_OK); i++) {
			if ((retval = config_get_lib(ap_ids[i], &libloc)) !=
			    CFGA_OK)
				return (retval);

			errno = 0;
			retval = (*libloc.libp->cfga_help_p)(msgp, options,
			    flags);
			release_lib(libloc.libp);
		}
	}
	return (retval);
}

/*
 * Private support routines for the public interfaces
 */

/*
 * config-init - initializes the library
 */
static void config_init()
{
	if (config_admin_inited == 0) {
		rwlock_init(&plugin_list_lock, NULL, NULL);
		plugin_list.next = &plugin_list;
		config_admin_inited++;
	}
}


static const char *
__config_strerror(
	int cfgerrnum)
{
	const char *ep;

	switch (cfgerrnum) {
	case CFGA_OK:
		ep = "Configuration operation succeeded";
		break;
	case CFGA_NACK:
		ep = "Configuration operation cancelled";
		break;
	case CFGA_NOTSUPP:
		ep = "Configuration administration not supported";
		break;
	case CFGA_OPNOTSUPP:
		ep = "Configuration operation not supported";
		break;
	case CFGA_PRIV:
		ep = "Insuficient privileges";
		break;
	case CFGA_BUSY:
		ep = "Component system is busy, try again";
		break;
	case CFGA_SYSTEM_BUSY:
		ep = "System is busy, try again";
		break;
	case CFGA_DATA_ERROR:
		ep = "Data error";
		break;
	case CFGA_LIB_ERROR:
		ep = "Library error";
		break;
	case CFGA_NO_LIB:
		ep = "No Library found";
		break;
	case CFGA_INSUFFICENT_CONDITION:
		ep = "Insufficient condition";
		break;
	case CFGA_ERROR:
		ep = "Hardware specific failure";
		break;
	default:
		ep = NULL;
		break;
	}
	return (ep);
}

/*
 * make_logical_name - make a logical name from the node
 */
static cfga_err_t
make_logical_name(
	di_node_t node,
	di_minor_t minor,
	void *arg)
{
	char inst[MAXPATHLEN];
	char tmp[MAXPATHLEN];
	int instance;
	plugin_lib_t *libp;
	libp = (plugin_lib_t *)arg;

	(void) strncpy(tmp, di_driver_name(node),
	    MAXPATHLEN);
	instance = di_instance(node);
	(void) sprintf(inst, "%d:", instance);
	(void) strncat(tmp, inst, MAXPATHLEN);
	(void) strncat(tmp, di_minor_name(minor),
	    MAXPATHLEN);
	(void) strncpy(libp->ap_logid, tmp, CFGA_AP_LOG_ID_LEN);
	return (CFGA_OK);
}

/*
 * resolve_lib_ref - relocate to use plugin lib
 */
static cfga_err_t
resolve_lib_ref(
	plugin_lib_t *libp)
{
	void *sym;
	void *lib_handlep = libp->handle;

	if ((sym = dlsym(lib_handlep, "cfga_change_state")) == NULL) {
		perror("dlsym: cfga_change_state");
		return (CFGA_LIB_ERROR);
	} else
		libp->cfga_change_state_p = (cfga_err_t (*)(int, const char *,
		    const char *, struct cfga_confirm *,
		    struct cfga_msg *, char **, cfga_flags_t)) sym;

	if ((sym = dlsym(lib_handlep, "cfga_private_func")) == NULL) {
		perror("dlsym: cfga_private_func");
		return (CFGA_LIB_ERROR);
	} else
		libp->cfga_private_func_p = (cfga_err_t (*)(const char *,
		    const char *, const char *, struct cfga_confirm *,
		    struct cfga_msg *, char **, cfga_flags_t))sym;

	if ((sym = dlsym(lib_handlep, "cfga_test")) == NULL) {
		perror("dlsym: cfga_test");
		return (CFGA_LIB_ERROR);
	} else
		libp->cfga_test_p = (cfga_err_t (*)(const char *, const char *,
		    struct cfga_msg *, char **, cfga_flags_t))sym;

	if ((sym = dlsym(lib_handlep, "cfga_stat")) == NULL) {
		perror("dlsym: cfga_stat");
		return (CFGA_LIB_ERROR);
	} else
		libp->cfga_stat_p = (cfga_err_t (*)(const char *,
		    struct cfga_stat_data *, const char *,
		    char **))sym;

	if ((sym = dlsym(lib_handlep, "cfga_list")) == NULL) {
		perror("dlsym: cfga_list");
		return (CFGA_LIB_ERROR);
	} else
		libp->cfga_list_p = (cfga_err_t (*)(struct cfga_stat_data **,
		    int *, const char *, char **))sym;

	if ((sym = dlsym(lib_handlep, "cfga_help")) == NULL) {
		perror("dlsym: cfga_help");
		return (CFGA_LIB_ERROR);
	} else
		libp->cfga_help_p = (cfga_err_t (*)(struct cfga_msg *,
		    const char *, cfga_flags_t))sym;

	if ((sym = dlsym(lib_handlep, "cfga_ap_id_cmp")) == NULL) {
		perror("dlsym: cfga_ap_id_cmp");
		return (CFGA_LIB_ERROR);
	} else
		libp->cfga_ap_id_cmp_p = (int (*)(const
		    cfga_ap_log_id_t, const cfga_ap_log_id_t))sym;

	return (CFGA_OK);
}

/*
 * config_calloc_check - perform allocation, check result and
 * set error indicator
 */
static void *
config_calloc_check(
	size_t nelem,
	size_t elsize,
	cfga_err_t *error)
{
	void *p;
	static char alloc_fail[] =
"%s: memory allocation failed (%d*%d bytes)\n";

	p = calloc(nelem, elsize);
	if (p == NULL) {
		(void) fprintf(stderr, gettext(alloc_fail), "config_admin",
		    nelem, elsize);
		*error = CFGA_LIB_ERROR;
	} else {
		*error = CFGA_OK;
	}

	return (p);
}

/*
 * get_next_lib - move on to the next plugin library
 * this routine supports the config_list function which
 * calls all plugin cfga_list routines.
 */

static cfga_err_t
get_next_lib(plugin_lib_t **libpp)
{
	cfga_err_t ret;
	lib_loc_t libloc;
	libloc.libp;

	/*
	 * call init routine (a nop if not the first call)
	 */
	config_init();
	/*
	 * If first time through rescan to fix up lib list,
	 * set libp to first lib.
	 * Otherwise, unlock current lib entry used and set lib
	 * to the next.
	 */
	if (*libpp == (plugin_lib_t *)NULL) {
		find_all_aps();
		(void) rw_rdlock(&plugin_list_lock);
		libloc.libp = plugin_list.next;
	} else {
		(void) rw_rdlock(&plugin_list_lock);
		(void) rw_unlock(&(*libpp)->entry_lock);
		libloc.libp = (*libpp)->next;
	}

	/*
	 * If our libp is pointing to the base we have
	 * finished with the list. Unlock and return.
	 * Also, since the find_all_aps leaves a read lock on
	 * each entry (so that the lib's stick around throughout
	 * a list operation) we need to clean up those
	 * references.
	 */
	if (libloc.libp == &plugin_list) {
		for (libloc.libp = plugin_list.next;
		    libloc.libp != &plugin_list;
		    libloc.libp = libloc.libp->next)
			(void) rw_unlock(&libloc.libp->entry_lock);
		(void) rw_unlock(&plugin_list_lock);
		*libpp = (plugin_lib_t *)NULL;
		return (CFGA_NO_LIB);
	}

	/*
	 * May need to open the lib and setup the references
	 */
	if (libloc.libp->handle == (void *) NULL)
		libloc.libp->handle = dlopen(libloc.libp->libpath, RTLD_NOW);
	if (libloc.libp->handle != (void *) NULL) {
		if (resolve_lib_ref(libloc.libp) == CFGA_OK) {
			(void) rw_rdlock(&libloc.libp->entry_lock);
			(void) rw_unlock(&plugin_list_lock);
			*libpp = libloc.libp;
			return (CFGA_OK);
		}
	}
	(void) rw_unlock(&plugin_list_lock);
	*libpp = (plugin_lib_t *)NULL;
	return (CFGA_NO_LIB);
}

/*
 * config_get_lib - given an ap_id find the library name
 */
static cfga_err_t
config_get_lib(
	char *ap_id,
	lib_loc_t *lib_loc_p)
{
	cfga_err_t	ret = CFGA_OK;

	/*
	 * call init routine (a nop if not the first call)
	 */
	config_init();

	lib_loc_p->libp = (plugin_lib_t *)NULL;
	(void) strncpy(lib_loc_p->ap_arg, ap_id, MAXPATHLEN);
	/*
	 * deal with ap_id, which can be a logical name, type or
	 * a physical pathname.
	 */
	if (IS_PHYSICAL(ap_id)) {
		/*
		 * walk the tree and find this ap_id.
		 */
		if ((ret = find_ap_phys(lib_loc_p)) != CFGA_OK)
			return (ret);
	} else {
		/*
		 * walk the tree and find this ap_id.
		 */
		if ((ret = find_ap(lib_loc_p)) != CFGA_OK)
			return (ret);
	}
	return (CFGA_OK);
}

/*
 * load_lib - Given a library pathname, create a entry for it
 * in the library list, if one does not already exist, and read
 * lock it to keep it there.
 */
static cfga_err_t
load_lib(
	di_node_t node,
	di_minor_t minor,
	lib_loc_t *libloc_p)
{
	plugin_lib_t *libp, *list_libp;
	cfga_err_t calloc_error = CFGA_OK;

	/*
	 * Allocate a plugin_lib struct in case the library
	 * is not currently in the plugin_lib list.
	 */
	libp = (plugin_lib_t *)config_calloc_check(1,
	    sizeof (plugin_lib_t), &calloc_error);

	/*
	 * write lock the library list
	 */
	(void) rw_wrlock(&plugin_list_lock);

	/*
	 * see if lib exist in list, if so, nuke alloc'ed
	 * one and use the existing one.
	 */
	list_libp = lib_in_list(libloc_p->pathname, -1);
	if (list_libp != (plugin_lib_t *)NULL) {
		if (calloc_error == CFGA_OK)
			free(libp);
		libp = list_libp;
		(void) rw_rdlock(&libp->entry_lock);
	} else {
		/*
		 * Initialize and link in new libp
		 */
		if (calloc_error != CFGA_OK) {
			(void) rw_unlock(&plugin_list_lock);
			return (CFGA_NO_LIB);
		}
		rwlock_init(&libp->entry_lock, NULL, NULL);
		(void) rw_rdlock(&libp->entry_lock);
		/*
		 * get to end of list
		 */
		list_libp = plugin_list.next;
		while (list_libp->next != &plugin_list)
			list_libp = list_libp->next;
		/*
		 * link in new entry
		 */
		libp->next = list_libp->next;
		list_libp->next = libp;
		libp->instance = di_instance(node);
		(void) strncpy(libp->libpath, libloc_p->pathname, MAXPATHLEN);
		(void) strncpy(libp->ap_phys, "/devices", MAXPATHLEN);
		(void) strncat(libp->ap_phys, di_devfs_path(node), MAXPATHLEN);
		(void) strncat(libp->ap_phys, ":", MAXPATHLEN);
		(void) strncat(libp->ap_phys, di_minor_name(minor),
		    MAXPATHLEN);
		libp->handle = (void *) NULL;
	}
	/*
	 * record physical node name in the libloc
	 * struct
	 */
	(void) strncpy(libloc_p->ap_physical, "/devices", MAXPATHLEN);
	(void) strncat(libloc_p->ap_physical, di_devfs_path(node), MAXPATHLEN);
	(void) strncat(libloc_p->ap_physical, ":", MAXPATHLEN);
	(void) strncat(libloc_p->ap_physical, di_minor_name(minor),
	    MAXPATHLEN);
	(void) make_logical_name(node, minor, libp);

	/*
	 * ensure that the lib is open and linked in
	 */
	if (libp->handle == (void *) NULL)
		libp->handle = dlopen(libp->libpath, RTLD_NOW);
	if (libp->handle != (void *) NULL) {
		if (resolve_lib_ref(libp) == CFGA_OK) {
			(void) rw_rdlock(&libp->entry_lock);
			(void) rw_unlock(&plugin_list_lock);
			libloc_p->libp = libp;
			return (CFGA_OK);
		} else {
			(void) dlclose(libp->handle);
			libp->handle = (void *)NULL;
		}
	}
	(void) rw_unlock(&plugin_list_lock);
	return (CFGA_NO_LIB);
}

/*
 * release_lib - releases the hold on a library in the plugin list.
 */
static void
release_lib(
	plugin_lib_t *libp)
{
	(void) rw_unlock(&libp->entry_lock);
}

/*
 * find_lib - Given an attachment point node find it's library
 */
static cfga_err_t
find_lib(
	di_node_t node,
	di_minor_t minor,
	char *pathname)
{
	char lib[MAXPATHLEN];
	char name[NUM_LIB_NAMES][MAXPATHLEN];
	struct stat lib_stat;
	void *dlhandle;
	static char machine_name[SYSINFO_LENGTH];
	static char arch_name[SYSINFO_LENGTH];
	int i;

	/* Make sure pathname is null if we fail */
	(void) strncpy(pathname, "", MAXPATHLEN);

	/*
	 * Initialize machine name and arch name
	 */
	if (strncmp("", machine_name, MAXPATHLEN) == 0) {
		if (sysinfo(SI_MACHINE, machine_name, SYSINFO_LENGTH) == -1) {
			return (CFGA_ERROR);
		}
		if (sysinfo(SI_ARCHITECTURE, arch_name, SYSINFO_LENGTH) == -1) {
			return (CFGA_ERROR);
		}
	}

	/*
	 * Initialize possible library tags, nodename and driver name.
	 */
	(void) strncpy(&name[0][0], (char *)di_node_name(node), MAXPATHLEN);
	(void) strncpy(&name[1][0], (char *)di_driver_name(node), MAXPATHLEN);

	/*
	 * Try using node name, then use driver name if necessary
	 */
	for (i = 0; i < NUM_LIB_NAMES; i++) {

		lib[0] = NULL;
		/*
		 * Try path based upon machine name
		 */
		(void) strncpy(lib, LIB_PATH_BASE1, MAXPATHLEN);
		(void) strncat(lib, machine_name, SYSINFO_LENGTH);
		(void) strncat(lib, LIB_PATH_MIDDLE, MAXPATHLEN);
		(void) strncat(lib, name[i], MAXPATHLEN);
		(void) strncat(lib, LIB_PATH_TAIL, MAXPATHLEN);

		if (stat(lib, &lib_stat) == 0) {
			/* file exists, is it a lib */
			dlhandle = dlopen(lib, RTLD_LAZY);
			if (dlhandle != (void *)NULL) {
				/* we got one! */
				(void) strncpy(pathname, lib, MAXPATHLEN);
				(void) dlclose(dlhandle);
				return (CFGA_OK);
			}
		}

		/*
		 * Try path based upon arch name
		 */
		(void) strncpy(lib, LIB_PATH_BASE1, MAXPATHLEN);
		(void) strncat(lib, arch_name, SYSINFO_LENGTH);
		(void) strncat(lib, LIB_PATH_MIDDLE, MAXPATHLEN);
		(void) strncat(lib, name[i], MAXPATHLEN);
		(void) strncat(lib, LIB_PATH_TAIL, MAXPATHLEN);

		if (stat(lib, &lib_stat) == 0) {
			/* file exists, is it a lib */
			dlhandle = dlopen(lib, RTLD_LAZY);
			if (dlhandle != (void *)NULL) {
				/* we got one! */
				(void) strncpy(pathname, lib, MAXPATHLEN);
				(void) dlclose(dlhandle);
				return (CFGA_OK);
			}
		}

		/*
		 * Try generic location
		 */
		(void) strncpy(lib, LIB_PATH_BASE2, MAXPATHLEN);
		(void) strncat(lib, name[i], MAXPATHLEN);
		(void) strncat(lib, LIB_PATH_TAIL, MAXPATHLEN);

		if (stat(lib, &lib_stat) == 0) {
			/* file exists, is it a lib */
			dlhandle = dlopen(lib, RTLD_LAZY);
			if (dlhandle != (void *)NULL) {
				/* we got one! */
				(void) strncpy(pathname, lib, MAXPATHLEN);
				(void) dlclose(dlhandle);
				return (CFGA_OK);
			}
		}
	}
	return (CFGA_NO_LIB);
}

/*
 * find_all_aps - locate all attachment points and generate
 * plugin_lib entries for each library.
 */

static void
find_all_aps()
{
	di_node_t root;

	/*
	 * begin walk of device tree
	 */
	if ((root = di_init("/", DINFOCPYALL)) == DI_NODE_NIL) {
		fprintf(stderr, "find_dev: di_init failed\n");
		exit(1);
	}
	di_walk_minor(root, "ddi_ctl:attachment_point",
	    DI_CHECK_ALIAS|DI_CHECK_INTERNAL_PATH,
	    NULL, add_ap_lib);
	di_fini(root);
}

/*
 * find_ap - locate a particular attachment point
 */

static cfga_err_t
find_ap(
	lib_loc_t *libloc_p)
{
	di_node_t root;

	/*
	 * begin walk of device tree
	 */
	if ((root = di_init("/", DINFOCPYALL)) == DI_NODE_NIL) {
		fprintf(stderr, "find_dev: di_init failed\n");
		exit(1);
	}
	di_walk_minor(root, "ddi_ctl:attachment_point",
	    DI_CHECK_ALIAS|DI_CHECK_INTERNAL_PATH,
	    libloc_p, check_ap);
	di_fini(root);
	if (libloc_p->libp != (plugin_lib_t *)NULL)
		return (CFGA_OK);
	else
		return (CFGA_NO_LIB);
}

/*
 * find_ap_phys - locate a particular attachment point
 */

static cfga_err_t
find_ap_phys(
	lib_loc_t *libloc_p)
{
	di_node_t root;

	/*
	 * begin walk of device tree
	 */
	if ((root = di_init("/", DINFOCPYALL)) == DI_NODE_NIL) {
		fprintf(stderr, "find_dev: di_init failed\n");
		exit(1);
	}
	di_walk_minor(root, "ddi_ctl:attachment_point",
	    DI_CHECK_ALIAS|DI_CHECK_INTERNAL_PATH,
	    libloc_p, check_ap_phys);
	di_fini(root);
	if (libloc_p->libp != (plugin_lib_t *)NULL)
		return (CFGA_OK);
	else
		return (CFGA_NO_LIB);
}

/*
 * check_ap - called for each attachment point found
 *
 * This is used in cases where a particular attachment point
 * or type of attachment point is specified via a logical name.
 * Not used for physical names or in the list case with no
 * ap's specified.
 */
int isdigit(int);

static int
check_ap(
	di_node_t node,
	di_minor_t minor,
	void *arg)
{
	char *cp;
	char aptype[MAXPATHLEN];
	char recep_id[MAXPATHLEN];
	char node_minor[MAXPATHLEN];
	char inst[MAXPATHLEN];
	char inst2[MAXPATHLEN];
	char tmp[MAXPATHLEN];
	cfga_err_t ret;
	lib_loc_t *libloc_p;
	char *ap_id;

	libloc_p = (lib_loc_t *)arg;
	ap_id = libloc_p->ap_arg;
	/*
	 * convert a logical ap_id to an ap_type, for
	 * comparison with the nodename and driver name
	 */
	(void) strncpy(aptype, ap_id, MAXPATHLEN);
	if ((cp = strchr(aptype, ':')) != NULL) {
		*cp = NULL;
		/* get instance and shorted the aptype accordingly */
		cp--;
		while (isdigit(*cp))
			cp--;
		cp++;
		(void) strncpy(inst, cp, MAXPATHLEN);
		*cp = NULL;
	} else {
		(void) strncpy(inst, "", strlen(""));
	}

	(void) sprintf(inst2, "%d", di_instance(node));

	if ((cp = strchr(ap_id, ':')) != NULL)
		(void) strncpy(recep_id, cp+1, MAXPATHLEN);
	else
		(void) strcpy(recep_id, "");

	(void) strncpy(tmp, di_minor_name(minor), MAXPATHLEN);
	(void) strncpy(node_minor, tmp, MAXPATHLEN);

	/*
	 * If the type matches the nodename or the drivername
	 * try and find a lib for it, the load it.
	 * On any failure we continue the walk.
	 */
	if (((strncmp(aptype, di_node_name(node), strlen(aptype)) == 0) ||
	    (strncmp(aptype, di_driver_name(node), strlen(aptype)) == 0)) &&
	    (strncmp(recep_id, node_minor, strlen(recep_id)) == 0) &&
	    (strncmp(inst, inst2, strlen(inst)) == 0)) {
		if ((ret = find_lib(node, minor, libloc_p->pathname))
		    != CFGA_OK)
			return (DI_WALK_CONTINUE);
		if ((ret = load_lib(node, minor, libloc_p)) != CFGA_OK)
			return (DI_WALK_CONTINUE);
		return (DI_WALK_TERMINATE);
	} else {
		return (DI_WALK_CONTINUE);
	}
}

/*
 * check_ap_phys - called for each attachment point found
 *
 * This is used in cases where a particular attachment point
 * is specified via a physical name. If the name matches then
 * we try and find and load the library for it.
 */

static int
check_ap_phys(
	di_node_t node,
	di_minor_t minor,
	void *arg)
{
	cfga_err_t ret;
	lib_loc_t *libloc_p;
	char *ap_id;
	char phys_name[MAXPATHLEN];

	libloc_p = (lib_loc_t *)arg;
	ap_id = libloc_p->ap_arg;

	(void) strncpy(phys_name, "/devices", MAXPATHLEN);
	(void) strncat(phys_name, di_devfs_path(node), MAXPATHLEN);
	(void) strncat(phys_name, ":", MAXPATHLEN);
	(void) strncat(phys_name, di_minor_name(minor),
	    MAXPATHLEN);
	if (strncmp(phys_name, ap_id, MAXPATHLEN) == 0) {
		if ((ret = find_lib(node, minor, libloc_p->pathname)) !=
		    CFGA_OK)
			return (DI_WALK_CONTINUE);
		if ((ret = load_lib(node, minor, libloc_p)) != CFGA_OK)
			return (DI_WALK_CONTINUE);
		return (DI_WALK_TERMINATE);
	} else {
		return (DI_WALK_CONTINUE);
	}
}

/*
 * lib_in_list
 *
 * See if library, as specified by the full pathname and controller
 * instance number is already represented in the plugin library list.
 * If the instance number is -1 it is ignored.
 */
static plugin_lib_t *
lib_in_list(char *libpath,
	    int inst)
{
	plugin_lib_t *libp;

	for (libp = plugin_list.next; libp != &plugin_list; libp = libp->next) {
		if (strncmp(libpath, libp->libpath, MAXPATHLEN) == 0) {
			if ((inst == -1) || (inst == libp->instance)) {
				return (libp);
			}
		}
	}
	return ((plugin_lib_t *)NULL);
}

/*
 * add_ap_lib - process an attachment point node
 *
 * Used for the 'list all attachment points' case.
 *
 * For each attachment point, try and locate a plugin library for it.
 * If a library is found and not currently in the plugin library list,
 * allocate a plugin lib element to the list.
 *
 * New entry is left read locked to prevent it's deletion from the
 * list via another thread. Will be released at the end of the
 * list traversal.
 */
static int
add_ap_lib(
	di_node_t node,
	di_minor_t minor,
	void *arg)
{
	plugin_lib_t *libp, *listp;
	char libpath[MAXPATHLEN];
	cfga_err_t calloc_error = CFGA_OK;

	/*
	 * try and find a lib for this node
	 */
	if (find_lib(node, minor, libpath) != CFGA_OK)
		return (DI_WALK_CONTINUE);

	/*
	 * We found a library. Allocate a plugin_lib
	 * struct for it, lock the list and add it in.
	 */
	libp = (plugin_lib_t *)config_calloc_check(1,
	    sizeof (plugin_lib_t), &calloc_error);

	if (calloc_error != CFGA_OK)
		return (DI_WALK_CONTINUE);

	/*
	 * write lock the library list
	 */
	(void) rw_wrlock(&plugin_list_lock);

	/*
	 * Each lib goes in list once only, per instance of the
	 * controller.
	 */
	libp->instance = di_instance(node);
	if ((listp = lib_in_list(libpath, libp->instance)) !=
	    (plugin_lib_t *)NULL) {
		free(libp);
		(void) rw_rdlock(&listp->entry_lock);
		(void) rw_unlock(&plugin_list_lock);
		return (DI_WALK_CONTINUE);
	} else {
		/*
		 * initialize and link in the new element
		 */
		rwlock_init(&libp->entry_lock, NULL, NULL);
		(void) rw_rdlock(&libp->entry_lock);
		/*
		 * get to end of list
		 */
		listp = plugin_list.next;
		while (listp->next != &plugin_list)
			listp = listp->next;
		/*
		 * link in new entry
		 */
		libp->next = listp->next;
		listp->next = libp;
		(void) strncpy(libp->libpath, libpath, MAXPATHLEN);
		(void) strncpy(libp->ap_phys, "/devices", MAXPATHLEN);
		(void) strncat(libp->ap_phys, di_devfs_path(node), MAXPATHLEN);
		(void) strncat(libp->ap_phys, ":", MAXPATHLEN);
		(void) strncat(libp->ap_phys, di_minor_name(minor),
		    MAXPATHLEN);
		libp->handle = (void *) NULL;
		/*
		 * unlock the library list
		 */
		(void) rw_unlock(&plugin_list_lock);
		return (DI_WALK_CONTINUE);
	}
}

/*
 * stat_ap - Routine to stat an attachment point as part of
 * a config_list opertion.
 */
static int
stat_ap(
	di_node_t node,
	di_minor_t minor,
	void *arg)
{
	char libpath[MAXPATHLEN];
	lib_loc_t lib_loc;
	plugin_lib_t *local_libp;
	lib_loc_t *libloc_p = &lib_loc;
	cfga_err_t ret;
	stat_data_list_t *slp, *slp2;
	list_stat_t *lstat = (list_stat_t *)arg;
	cfga_err_t calloc_error = CFGA_OK;

	/*
	 * try and find a lib for this node
	 */
	if (find_lib(node, minor, libpath) != CFGA_OK)
		return (DI_WALK_CONTINUE);
	/*
	 * We found a library. Allocate a plugin_lib
	 * struct for it
	 */
	local_libp = (plugin_lib_t *)config_calloc_check(1,
	    sizeof (plugin_lib_t), &calloc_error);

	if (calloc_error != CFGA_OK)
		return (DI_WALK_CONTINUE);

	libloc_p->libp = local_libp;
	libloc_p->libp->instance = di_instance(node);

	(void) strncpy(libloc_p->libp->libpath, libpath, MAXPATHLEN);
	(void) strncpy(libloc_p->pathname, libpath, MAXPATHLEN);
	(void) strncpy(libloc_p->libp->ap_phys, "/devices", MAXPATHLEN);
	(void) strncat(libloc_p->libp->ap_phys, di_devfs_path(node),
	    MAXPATHLEN);
	(void) strncat(libloc_p->libp->ap_phys, ":", MAXPATHLEN);
	(void) strncat(libloc_p->libp->ap_phys, di_minor_name(minor),
	    MAXPATHLEN);

	if ((ret = load_lib(node, minor, libloc_p)) != CFGA_OK) {
		free(libloc_p->libp);
		return (DI_WALK_CONTINUE);
	}

	/*
	 * handle case where the lib is found in the list
	 */
	if (libloc_p->libp != local_libp) {
		free(local_libp);
	}
	/*
	 * allocate stat data buffer and list element
	 */
	slp = (stat_data_list_t *)config_calloc_check(1,
	    sizeof (stat_data_list_t), &calloc_error);

	if (calloc_error != CFGA_OK)
		return (DI_WALK_CONTINUE);

	/*
	 * Do the stat
	 */
	errno = 0;
	if ((ret = (*(libloc_p->libp->cfga_stat_p))(libloc_p->ap_physical,
	    &slp->stat_data, lstat->opts, lstat->errstr)) != CFGA_OK) {
		free(slp);
		return (DI_WALK_TERMINATE);
	}
	slp->next = (stat_data_list_t *)NULL;
	/*
	 * set up the logical and physical id's
	 */
	(void) make_logical_name(node, minor, libloc_p->libp);
	(void) strncpy(slp->stat_data.ap_log_id, libloc_p->libp->ap_logid,
	    CFGA_AP_LOG_ID_LEN);
	(void) strncpy(slp->stat_data.ap_phys_id, libloc_p->ap_physical,
	    CFGA_AP_PHYS_ID_LEN);
	/*
	 * link it in
	 */
	if ((slp2 = lstat->sdl) == (stat_data_list_t *)NULL) {
		lstat->sdl = slp;
	} else {
		while (slp2->next != (stat_data_list_t *)NULL)
			slp2 = slp2->next;
		slp2->next = slp;
	}
	/* keep count */
	(*lstat->count)++;
	(void) dlclose(libloc_p->libp->handle);
	libloc_p->libp->handle = (void *)NULL;
	/* done */
	return (DI_WALK_CONTINUE);
}

/*
 * stat_all_aps - walk the device tree and stat all attachment points.
 */
static cfga_err_t
stat_all_aps(
	cfga_stat_data_t **ap_di_list,
	int *nlist,
	const char *options,
	char **errstring)
{
	di_node_t root;
	list_stat_t lstat;
	int i;
	stat_data_list_t *slp, *slp2;
	cfga_stat_data_t *csdp;
	cfga_err_t calloc_error = CFGA_OK;

	lstat.count = nlist;
	lstat.opts = options;
	lstat.errstr = errstring;
	lstat.sdl = (stat_data_list_t *)NULL;

	/*
	 * begin walk of device tree
	 */
	if ((root = di_init("/", DINFOCPYALL)) == DI_NODE_NIL) {
		fprintf(stderr, "find_dev: di_init failed\n");
		exit(1);
	}
	di_walk_minor(root, "ddi_ctl:attachment_point",
	    DI_CHECK_ALIAS|DI_CHECK_INTERNAL_PATH,
	    &lstat, stat_ap);
	di_fini(root);

	/*
	 * allocate the array
	 */
	csdp = (cfga_stat_data_t *)config_calloc_check(*nlist,
	    sizeof (cfga_stat_data_t), &calloc_error);
	if (calloc_error == CFGA_OK) {
		/*
		 * copy the elements into the array
		 */
		slp = lstat.sdl;
		*ap_di_list = csdp;
		for (i = 0; i < *nlist; i++) {
			*csdp = slp->stat_data;
			csdp++;
			slp = slp->next;
		}
	} else {
		*ap_di_list = (cfga_stat_data_t *)NULL;
	}

	/* clean up */
	slp = lstat.sdl;
	while (slp != (stat_data_list_t *)NULL) {
		slp2 = slp->next;
		free(slp);
		slp = slp2;
	}
	if (calloc_error == CFGA_OK)
		return (CFGA_OK);
	else
		return (CFGA_ERROR);
}

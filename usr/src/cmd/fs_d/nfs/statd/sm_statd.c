/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sm_statd.c	1.36	97/08/26 SMI"	/* SVr4.0 1.2	*/
/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 *
 *
 *  		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	(c) 1986-1989,1994-1996  Sun Microsystems, Inc.
 *  	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *			All rights reserved.
 */
/*
 * sm_statd.c consists of routines used for the intermediate
 * statd implementation(3.2 rpc.statd);
 * it creates an entry in "current" directory for each site that it monitors;
 * after crash and recovery, it moves all entries in "current"
 * to "backup" directory, and notifies the corresponding statd of its recovery.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/param.h>
#include <dirent.h>
#include <rpc/rpc.h>
#include <rpcsvc/sm_inter.h>
#include <errno.h>
#include <memory.h>
#include <signal.h>
#include <synch.h>
#include <thread.h>
#include "sm_statd.h"


int LOCAL_STATE;

sm_hash_t	mon_table[MAX_HASHSIZE];
static sm_hash_t	record_table[MAX_HASHSIZE];
static sm_hash_t	recov_q;

static name_entry *find_name(name_entry **namepp, char *name);
static name_entry *insert_name(name_entry **namepp, char *name,
				int need_alloc);
static void delete_name(name_entry **namepp, char *name);
static void remove_name(char *name, int op, int startup);
static int statd_call_statd(char *name);
static void pr_name(char *name, int flag);
static void *thr_statd_init();
static void *sm_try();
static void *thr_call_statd(void *);
static void remove_single_name(char *name, char *dir1, char *dir2);

/*
 * called when statd first comes up; it searches /etc/sm to gather
 * all entries to notify its own failure
 */
void
statd_init()
{
	struct dirent *dirp;
	DIR 	*dp;
	char from[MAXPATHLEN], to[MAXPATHLEN];
	FILE *fp, *fp_tmp;
	int i, tmp_state;
	char state_file[MAXPATHLEN+SM_MAXPATHLEN];

	if (debug)
		(void) printf("enter statd_init\n");

	if ((mkdir(STATD_HOME, SM_DIRECTORY_MODE)) == -1) {
		if (errno != EEXIST) {
			syslog(LOG_ERR, "statd: mkdir current %m\n");
			exit(1);
		}
	}
	/*
	 * First try to open the file.  If that fails, try to create it.
	 * If that fails, give up.
	 */
	if ((fp = fopen(STATE, "r+")) == (FILE *)NULL)
		if ((fp = fopen(STATE, "w+")) == (FILE *)NULL) {
			syslog(LOG_ERR, "statd: fopen(stat file) error\n");
			exit(1);
		} else
			(void) chmod(STATE, 0644);
	if ((fscanf(fp, "%d", &LOCAL_STATE)) == EOF) {
		if (debug >= 2)
			(void) printf("empty file\n");
		LOCAL_STATE = 0;
	}

	/*
	 * Scan alternate paths for largest "state" number
	 */
	for (i = 0; i < pathix; i++) {
		(void) sprintf(state_file, "%s/statmon/state", path_name[i]);
		if ((fp_tmp = fopen(state_file, "r+")) == (FILE *)NULL) {
			if ((fp_tmp = fopen(state_file, "w+"))
				== (FILE *)NULL) {
				if (debug)
				    syslog(LOG_ERR,
					"statd: %s: fopen failed\n",
					state_file);
				continue;
			} else
				(void) chmod(state_file, 0644);
		}
		if ((fscanf(fp_tmp, "%d", &tmp_state)) == EOF) {
			if (debug)
			    syslog(LOG_ERR,
				"statd: %s: file empty\n", state_file);
			(void) fclose(fp_tmp);
			continue;
		}
		if (tmp_state > LOCAL_STATE) {
			LOCAL_STATE = tmp_state;
			if (debug)
				(void) printf("Update LOCAL STATE: %d\n",
						tmp_state);
		}
		(void) fclose(fp_tmp);
	}

	LOCAL_STATE = ((LOCAL_STATE%2) == 0) ? LOCAL_STATE+1 : LOCAL_STATE+2;

	/* IF local state overflows, reset to value 1 */
	if (LOCAL_STATE < 0) {
		LOCAL_STATE = 1;
	}

	/* Copy the LOCAL_STATE value back to all stat files */
	if (fseek(fp, 0, 0) == -1) {
		syslog(LOG_ERR, "statd: fseek failed\n");
		exit(1);
	}

	(void) fprintf(fp, "%-10d", LOCAL_STATE);
	(void) fflush(fp);
	if (fsync(fileno(fp)) == -1) {
		syslog(LOG_ERR, "statd: fsync failed\n");
		exit(1);
	}
	(void) fclose(fp);

	for (i = 0; i < pathix; i++) {
		(void) sprintf(state_file, "%s/statmon/state", path_name[i]);
		if ((fp_tmp = fopen(state_file, "r+")) == (FILE *)NULL) {
			if ((fp_tmp = fopen(state_file, "w+"))
				== (FILE *)NULL) {
				syslog(LOG_ERR,
				    "statd: %s: fopen failed\n", state_file);
				continue;
			} else
				(void) chmod(state_file, 0644);
		}
		(void) fprintf(fp_tmp, "%-10d", LOCAL_STATE);
		(void) fflush(fp_tmp);
		if (fsync(fileno(fp_tmp)) == -1) {
			syslog(LOG_ERR,
			    "statd: %s: fsync failed\n", state_file);
			(void) fclose(fp_tmp);
			exit(1);
		}
		(void) fclose(fp_tmp);
	}

	if (debug)
		(void) printf("local state = %d\n", LOCAL_STATE);

	if ((mkdir(CURRENT, SM_DIRECTORY_MODE)) == -1) {
		if (errno != EEXIST) {
			syslog(LOG_ERR, "statd: mkdir current, error %m\n");
			exit(1);
		}
	}
	if ((mkdir(BACKUP, SM_DIRECTORY_MODE)) == -1) {
		if (errno != EEXIST) {
			syslog(LOG_ERR, "statd: mkdir backup, error %m\n");
			exit(1);
		}
	}

	/* get all entries in CURRENT into BACKUP */
	if ((dp = opendir(CURRENT)) == (DIR *)NULL) {
		syslog(LOG_ERR, "statd: open current directory, error %m\n");
		exit(1);
	}

	for (dirp = readdir(dp); dirp != (struct dirent *)NULL;
		dirp = readdir(dp)) {
		if (strcmp(dirp->d_name, ".") != 0 &&
			strcmp(dirp->d_name, "..") != 0) {
		/* rename all entries from CURRENT to BACKUP */
			(void) sprintf(from, "%s/%s", CURRENT, dirp->d_name);
			(void) sprintf(to, "%s/%s", BACKUP, dirp->d_name);
			/* If unable to create file, do not unlink from */
			if (create_file(to) == 0) {
				if (unlink(from) == -1) {
					syslog(LOG_ERR,
					"statd_init: unlink of %s, error %m",
						from);
					continue;
				}
			}
		}
	}

	(void) closedir(dp);

	/* Contact hosts' statd */
	if (thr_create(NULL, NULL, thr_statd_init, NULL, THR_DETACHED, 0)) {
		syslog(LOG_ERR,
		"statd: unable to create thread for thr_statd_init\n");
		exit(1);
	}
}

/*
 * Work thread which contacts hosts' statd.
 */
void *
thr_statd_init()
{
	struct dirent *dirp;
	DIR 	*dp;
	int num_threads;
	int num_join;
	int i;
	char buf[MAXPATHLEN+SM_MAXPATHLEN];

	/* Go thru backup directory and contact hosts */
	if ((dp = opendir(BACKUP)) == (DIR *)NULL) {
		syslog(LOG_ERR, "statd: open backup directory, error %m\n");
		exit(1);
	}

	/*
	 * Create "UNDETACHED" threads for each name in backup directory
	 * to initiate statd_call_statd.
	 * NOTE: These threads are the only undetached threads in this
	 * program and thus, the thread id is not needed to join the threads.
	 */
	num_threads = 0;
	for (dirp = readdir(dp); dirp != (struct dirent *)NULL;
		dirp = readdir(dp)) {
		if (strcmp(dirp->d_name, ".") != 0 &&
			strcmp(dirp->d_name, "..") != 0) {
			char *name;

			/*
			 * If the num_threads has exceeded, wait until
			 * a certain amount of threads have finished.
			 * Currently, 10% of threads created should be joined.
			 */
			if (num_threads > MAX_THR) {
				num_join = num_threads/PERCENT_MINJOIN;
				for (i = 0; i < num_join; i++)
					thr_join(0, 0, 0);
				num_threads -= num_join;
			}

			/*
			 * If can't alloc name then print error msg and
			 * continue to next item on list.
			 */
			if ((name = (char *) strdup(dirp->d_name)) ==
			    (char *)NULL) {
				syslog(LOG_ERR,
			"statd: unable to allocate space for name %s\n",
					dirp->d_name);
					continue;
			}

			/* Create a thread to do a statd_call_statd for name */
			if (thr_create(NULL, NULL, thr_call_statd,
						(void *) name, 0, 0)) {
				syslog(LOG_ERR,
		"statd: unable to create thr_call_statd() for name %s.\n",
					dirp->d_name);
				free(name);
				continue;
				}
			num_threads++;
		}

	}
	(void) closedir(dp);

	/*
	 * Join the other threads created above before creating thread
	 * to process items in recovery table.
	 */
	for (i = 0; i < num_threads; i++) {
		thr_join(0, 0, 0);
	}

	/*
	 * Need to only copy /var/statmon/sm.bak to alternate paths, since
	 * the only hosts in /var/statmon/sm should be the ones currently
	 * being monitored and already should be in alternate paths as part
	 * of insert_mon().
	 */
	for (i = 0; i < pathix; i++) {
		(void) sprintf(buf, "%s/statmon/sm.bak", path_name[i]);
		if ((mkdir(buf, SM_DIRECTORY_MODE)) == -1) {
			if (errno != EEXIST)
				syslog(LOG_ERR, "statd: mkdir %s error %m\n",
					buf);
			else
				copydir_from_to(BACKUP, buf);
		} else
			copydir_from_to(BACKUP, buf);
	}


	/*
	 * Reset the die and in_crash variable and signal other threads
	 * that have issued an sm_crash and are waiting.
	 */
	mutex_lock(&crash_lock);
	die = 0;
	in_crash = 0;
	mutex_unlock(&crash_lock);
	cond_broadcast(&crash_finish);

	if (debug)
		(void) printf("Creating thread for sm_try\n");

	/* Continue to notify statd on hosts that were unreachable. */
	if (thr_create(NULL, NULL, sm_try, NULL, THR_DETACHED, 0))
		syslog(LOG_ERR,
			"statd: unable to create thread for sm_try().\n");
	thr_exit((void *) 0);
}

/*
 * Work thread to make call to statd_call_statd.
 */
void *
thr_call_statd(namep)
void *namep;
{
	char *name = (char *) namep;

	/*
	 * If statd of name is unreachable, add name to recovery table
	 * otherwise if statd_call_statd was successful, remove from backup.
	 */
	if (statd_call_statd(name) != 0) {
		mutex_lock(&recov_q.lock);
		(void) insert_name(&recov_q.sm_recovhdp, name, 0);
		mutex_unlock(&recov_q.lock);

		if (debug)
			pr_name(name, 0);

	} else { /* remove from BACKUP directory */
		remove_name(name, 1, 1);
		free(name);
	}
	thr_exit((void *) 0);
}

/*
 * Notifies the statd of host specified by name to indicate that
 * state has changed for this server.
 */
static int
statd_call_statd(name)
	char *name;
{
	enum clnt_stat clnt_stat;
	struct timeval tottimeout;
	CLIENT *clnt;
	stat_chge ntf;
	int i;
	int rc = 0;

	ntf.mon_name = hostname;
	ntf.state = LOCAL_STATE;
	if (debug)
		(void) printf("statd_call_statd at %s\n", name);
	if ((clnt = create_client(name, SM_PROG, SM_VERS)) ==
		(CLIENT *) NULL) {
			return (-1);
	}

	tottimeout.tv_sec = SM_RPC_TIMEOUT;
	tottimeout.tv_usec = 0;

	/* Perform notification to client */
	clnt_stat = clnt_call(clnt, SM_NOTIFY, xdr_stat_chge, (char *)&ntf,
	    xdr_void, NULL, tottimeout);
	if (debug) {
		(void) printf("clnt_stat=%s(%d)\n",
			clnt_sperrno(clnt_stat), clnt_stat);
	}
	if (clnt_stat != (int) RPC_SUCCESS) {
		syslog(LOG_WARNING,
			"statd: cannot talk to statd at %s, %s(%d)\n",
			name, clnt_sperrno(clnt_stat), clnt_stat);
		rc = -1;
	}

	/* For HA systems and multi-homed hosts */
	ntf.state = LOCAL_STATE;
	for (i = 0; i < addrix; i++) {
		ntf.mon_name = host_name[i];
		if (debug)
			(void) printf("statd_call_statd at %s\n", name);
		clnt_stat = clnt_call(clnt, SM_NOTIFY, xdr_stat_chge,
					(char *) &ntf, xdr_void, NULL,
					tottimeout);
		if (clnt_stat != (int) RPC_SUCCESS) {
			syslog(LOG_WARNING,
			    "statd: cannot talk to statd at %s, %s(%d)\n",
			    name, clnt_sperrno(clnt_stat), clnt_stat);
			rc = -1;
		}
	}
	clnt_destroy(clnt);
	return (rc);
}

/*
 * Continues to contact hosts in recovery table that were unreachable.
 * NOTE:  There should only be one sm_try thread executing and
 * thus locks are not needed for recovery table. Die is only cleared
 * after all the hosts has at least been contacted once.  The reader/writer
 * lock ensures to finish this code before an sm_crash is started.  Die
 * variable will signal it.
 */
void *
sm_try()
{
	name_entry *nl, *next;
	timestruc_t	wtime;
	int delay = 0;

	rw_rdlock(&thr_rwlock);
	if (mutex_trylock(&sm_trylock))
		goto out;
	mutex_lock(&crash_lock);
	(void) memset(&wtime, 0, sizeof (timestruc_t));

	while (!die) {
		wtime.tv_sec = time((time_t *)NULL) + delay;
		wtime.tv_nsec = 0;
		/*
		 * Wait until signalled to wakeup or time expired.
		 * If signalled to be awoken, then a crash has occurred
		 * or otherwise time expired.
		 */
		if (cond_timedwait(&retrywait, &crash_lock, &wtime) == 0) {
			break;
		}

		/* Exit loop if queue is empty */
		if ((next = recov_q.sm_recovhdp) == NULL)
			break;

		mutex_unlock(&crash_lock);

		while (((nl = next) != (name_entry *)NULL) && (!die)) {
			next = next->nxt;
			if (statd_call_statd(nl->name) == 0) {
				/* remove name from BACKUP */
				remove_name(nl->name, 1, 0);
				mutex_lock(&recov_q.lock);
				/* remove entry from recovery_q */
				delete_name(&recov_q.sm_recovhdp, nl->name);
				mutex_unlock(&recov_q.lock);
			} else {
				/*
				 * Print message only once since unreachable
				 * host can be contacted forever.
				 */
				if (delay == 0)
					syslog(LOG_WARNING,
					"statd: host %s is not responding\n",
						nl->name);
			}
		}
		/*
		 * Increment the amount of delay before restarting again.
		 * The amount of delay should not exceed the MAX_DELAYTIME.
		 */
		if (delay <= MAX_DELAYTIME)
			delay += INC_DELAYTIME;
		mutex_lock(&crash_lock);
	}

	mutex_unlock(&crash_lock);
	mutex_unlock(&sm_trylock);
out:
	rw_unlock(&thr_rwlock);
	if (debug)
		(void) printf("EXITING sm_try\n");
	thr_exit((void *) 0);
}

/*
 * Malloc's space and returns the ptr to malloc'ed space. NULL if unsuccessful.
 */
char *
xmalloc(len)
	unsigned len;
{
	char *new;

	if ((new = malloc(len)) == 0) {
		syslog(LOG_ERR, "statd: malloc, error %m\n");
		return ((char *)NULL);
	} else {
		(void) memset(new, 0, len);
		return (new);
	}
}

/*
 * the following two routines are very similar to
 * insert_mon and delete_mon in sm_proc.c, except the structture
 * is different
 */
static name_entry *
insert_name(namepp, name, need_alloc)
	name_entry **namepp;
	char *name;
	int need_alloc;
{
	name_entry *new;

	new = (name_entry *) xmalloc(sizeof (name_entry));
	if (new == (name_entry *) NULL)
		return (NULL);

	/* Allocate name when needed which is only when adding to record_t */
	if (need_alloc) {
		if ((new->name = strdup(name)) == (char *) NULL) {
			syslog(LOG_ERR, "statd: strdup, error %m\n");
			free(new);
			return (NULL);
		}
	} else
		new->name = name;

	new->nxt = *namepp;
	if (new->nxt != (name_entry *)NULL)
		new->nxt->prev = new;

	new->prev = (name_entry *) NULL;

	*namepp = new;
	return (new);
}

/*
 * Deletes name from specified list (namepp).
 */
static void
delete_name(namepp, name)
	name_entry **namepp;
	char *name;
{
	name_entry *nl;

	nl = *namepp;
	while (nl != (name_entry *)NULL) {
		if (str_cmp_unqual_hostname(nl->name, name) == 0) {
			if (nl->prev != (name_entry *)NULL)
				nl->prev->nxt = nl->nxt;
			else
				*namepp = nl->nxt;
			if (nl->nxt != (name_entry *)NULL)
				nl->nxt->prev = nl->prev;
			free(nl->name);
			free(nl);
			return;
		}
		nl = nl->nxt;
	}
}

/*
 * Finds name from specified list (namep).
 */
static name_entry *
find_name(namep, name)
	name_entry **namep;
	char *name;
{
	name_entry *nl;

	nl = *namep;

	while (nl != (name_entry *)NULL) {
		if (str_cmp_unqual_hostname(nl->name, name) == 0) {
			return (nl);
		}
		nl = nl->nxt;
	}
	return ((name_entry *)NULL);
}

/*
 * Creates a file.
 */
int
create_file(name)
	char *name;
{
	int fd;

	if ((fd = open(name, O_CREAT, S_IWUSR)) == -1) {
		syslog(LOG_ERR, "statd: open of %s, error %m\n", name);
		return (1);
	} else {
		if (debug >= 2)
			(void) printf("%s is created\n", name);
		if (close(fd)) {
			syslog(LOG_ERR, "statd: close, error %m\n");
			return (1);
		}

	}
	return (0);
}

/*
 * Deletes the file specified by name.
 */
void
delete_file(name)
	char *name;
{
	if (debug >= 2)
		(void) printf("Remove monitor entry %s\n", name);
	if (unlink(name) == -1) {
		if (errno != ENOENT)
			syslog(LOG_ERR, "statd: unlink of %s, error %m", name);
	}
}

/*
 * remove the name from the specified directory
 * op = 0: CURRENT
 * op = 1: BACKUP
 */
static void
remove_name(char *name, int op, int startup)
{
	int i;
	char *alt_dir;
	char *queue;

	if (op == 0) {
		alt_dir = "statmon/sm";
		queue = CURRENT;
	} else {
		alt_dir = "statmon/sm.bak";
		queue = BACKUP;
	}

	remove_single_name(name, queue, NULL);
	/*
	 * At startup, entries have not yet been copied to alternate
	 * directories and thus do not need to be removed.
	 */
	if (startup == 0) {
		for (i = 0; i < pathix; i++) {
			remove_single_name(name, path_name[i], alt_dir);
		}
	}
}

/*
 * Remove the name from the specified directory, which is dir1/dir2 or
 * dir1, depending on whether dir2 is NULL.
 */
static void
remove_single_name(char *name, char *dir1, char *dir2)
{
	char path[MAXPATHLEN+MAXNAMELEN+SM_MAXPATHLEN];

	if (strlen(name) + strlen(dir1) + (dir2 != NULL ? strlen(dir2) : 0)
			+ 3 > MAXPATHLEN) {
		if (dir2 != NULL)
			syslog(LOG_ERR,
				"statd: pathname too long: %s/%s/%s\n",
						dir1, dir2, name);
		else
			syslog(LOG_ERR,
				"statd: pathname too long: %s/%s\n",
						dir1, name);

		return;
	}

	(void) strcpy(path, dir1);
	(void) strcat(path, "/");
	if (dir2 != NULL) {
		(void) strcat(path, dir2);
		(void) strcat(path, "/");
	}
	(void) strcat(path, name);
	delete_file(path);
}


/*
 * Manage the cache of hostnames.  An entry for each host that has recently
 * locked a file is kept.  There is an in-ram table (rec_table) and an empty
 * file in the file system name space (/var/statmon/sm/<name>).  This
 * routine adds (deletes) the name to (from) the in-ram table and the entry
 * to (from) the file system name space.
 *
 * If op == 1 then the name is added to the queue otherwise the name is
 * deleted.
 */
void
record_name(name, op)
	char *name;
	int op;
{
	name_entry *nl;
	int i;
	char path[MAXPATHLEN+MAXNAMELEN+SM_MAXPATHLEN];
	name_entry **record_q;
	unsigned int hash;

	/*
	 * These names are supposed to be just host names, not paths or
	 * other arbitrary files.
	 * manipulating the empty pathname unlinks CURRENT,
	 * manipulating files with '/' would allow you to create and unlink
	 * files all over the system; LOG_AUTH, it's a security thing.
	 * Don't remove the directories . and ..
	 */
	if (name == NULL)
		return;

	if (name[0] == '\0' || strchr(name, '/') != NULL ||
			strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
		syslog(LOG_ERR|LOG_AUTH, "statd: attempt to %s \"%s/%s\"",
			op == 1 ? "create" : "remove", CURRENT, name);
		return;
	}

	SMHASH(name, hash);
	if (debug) {
		if (op == 1)
			(void) printf("inserting %s at hash %d,\n",
			name, hash);
		else
			(void) printf("deleting %s at hash %d\n", name, hash);
		pr_name(name, 1);
	}


	if (op == 1) { /* insert */
		mutex_lock(&record_table[hash].lock);
		record_q = &record_table[hash].sm_rechdp;
		if ((nl = find_name(record_q, name)) == (name_entry *)NULL) {

			int	path_len;

			if ((nl = insert_name(record_q, name, 1)) !=
			    (name_entry *) NULL)
				nl->count++;
			mutex_unlock(&record_table[hash].lock);
			/* make an entry in current directory */

			path_len = strlen(CURRENT) + strlen(name) + 2;
			if (path_len > MAXPATHLEN) {
				syslog(LOG_ERR,
					"statd: pathname too long: %s/%s\n",
						CURRENT, name);
				return;
			}
			(void) strcpy(path, CURRENT);
			(void) strcat(path, "/");
			(void) strcat(path, name);
			(void) create_file(path);
			if (debug) {
				(void) printf("After insert_name\n");
				pr_name(name, 1);
			}
			/* make an entry in alternate paths */
			for (i = 0; i < pathix; i++) {
				path_len = strlen(path_name[i]) +
							strlen("/statmon/sm/") +
							strlen(name) + 1;

				if (path_len > MAXPATHLEN) {
					syslog(LOG_ERR,
				"statd: pathname too long: %s/statmon/sm/%s\n",
							path_name[i], name);
					continue;
				}
				(void) strcpy(path, path_name[i]);
				(void) strcat(path, "/statmon/sm/");
				(void) strcat(path, name);
				(void) create_file(path);
			}
			return;
		}
		nl->count++;
		mutex_unlock(&record_table[hash].lock);

	} else { /* delete */
		mutex_lock(&record_table[hash].lock);
		record_q = &record_table[hash].sm_rechdp;
		if ((nl = find_name(record_q, name)) == (name_entry *)NULL) {
			mutex_unlock(&record_table[hash].lock);
			return;
		}
		nl->count--;
		if (nl->count == 0) {
			delete_name(record_q, name);
			mutex_unlock(&record_table[hash].lock);
			/* remove this entry from current directory */
			remove_name(name, 0, 0);
		} else
			mutex_unlock(&record_table[hash].lock);
		if (debug) {
			(void) printf("After delete_name \n");
			pr_name(name, 1);
		}
	}
}

/*
 * SM_CRASH - simulate a crash of statd.
 */
void
sm_crash()
{
	name_entry *nl, *next;
	mon_entry *nl_monp, *mon_next;
	int k;
	my_id *nl_idp;

	for (k = 0; k < MAX_HASHSIZE; k++) {
		mutex_lock(&mon_table[k].lock);
		if ((mon_next = mon_table[k].sm_monhdp) ==
		    (mon_entry *) NULL) {
			mutex_unlock(&mon_table[k].lock);
			continue;
		} else {
			while ((nl_monp = mon_next) != (mon_entry *)NULL) {
				mon_next = mon_next->nxt;
				nl_idp = &nl_monp->id.mon_id.my_id;
				free(nl_monp->id.mon_id.mon_name);
				free(nl_idp->my_name);
				free(nl_monp);
			}
			mon_table[k].sm_monhdp = (mon_entry *)NULL;
		}
		mutex_unlock(&mon_table[k].lock);
	}

	/* Clean up entries in  record table */
	for (k = 0; k < MAX_HASHSIZE; k++) {
		mutex_lock(&record_table[k].lock);
		if ((next = record_table[k].sm_rechdp) ==
		    (name_entry *) NULL) {
			mutex_unlock(&record_table[k].lock);
			continue;
		} else {
			while ((nl = next) != (name_entry *)NULL) {
				next = next->nxt;
				free(nl->name);
				free(nl);
			}
			record_table[k].sm_rechdp = (name_entry *)NULL;
		}
		mutex_unlock(&record_table[k].lock);
	}

	/* Clean up entries in recovery table */
	mutex_lock(&recov_q.lock);
	if ((next = recov_q.sm_recovhdp) != (name_entry *)NULL) {
		while ((nl = next) != (name_entry *)NULL) {
			next = next->nxt;
			free(nl->name);
			free(nl);
		}
		recov_q.sm_recovhdp = (name_entry *)NULL;
	}
	mutex_unlock(&recov_q.lock);
	statd_init();
}

/*
 * Initialize the hash tables: mon_table, record_table, recov_q and
 * locks.
 */
void
sm_inithash()
{
	int k;

	if (debug)
		(void) printf("Initializing hash tables\n");

	for (k = 0; k < MAX_HASHSIZE; k++) {
		mon_table[k].sm_monhdp = (mon_entry *)NULL;
		record_table[k].sm_rechdp = (name_entry *)NULL;
		mutex_init(&mon_table[k].lock, USYNC_THREAD, NULL);
		mutex_init(&record_table[k].lock, USYNC_THREAD, NULL);
	}
	mutex_init(&recov_q.lock, USYNC_THREAD, NULL);
	recov_q.sm_recovhdp = (name_entry *)NULL;

}

/*
 * Prints out list in record_table if flag is 1 otherwise
 * prints out each list in recov_q specified by name.
 */
static void
pr_name(name, flag)
	char *name;
	int flag;
{
	name_entry *nl;
	unsigned int hash;

	if (!debug)
		return;
	if (flag) {
		SMHASH(name, hash);
		(void) printf("*****record_q: ");
		mutex_lock(&record_table[hash].lock);
		nl = record_table[hash].sm_rechdp;
		while (nl != (name_entry *)NULL) {
			(void) printf("(%x), ", (int) nl);
			nl = nl->nxt;
		}
		mutex_unlock(&record_table[hash].lock);
	} else {
		(void) printf("*****recovery_q: ");
		mutex_lock(&recov_q.lock);
		nl = recov_q.sm_recovhdp;
		while (nl != (name_entry *)NULL) {
			(void) printf("(%x), ", (int) nl);
			nl = nl->nxt;
		}
		mutex_unlock(&recov_q.lock);

	}
	(void) printf("\n");
}

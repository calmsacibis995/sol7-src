/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright (c) 1996, 1997 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */
#pragma ident   "@(#)logging.c 1.1     97/12/03 SMI"

#include "port_before.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include <isc/logging.h>

#include "port_after.h"

#ifdef VSPRINTF_CHAR
# define VSPRINTF(x) strlen(vsprintf/**/x)
#else
# define VSPRINTF(x) ((size_t)vsprintf x)
#endif

#include "logging_p.h"

static const int syslog_priority[] = { LOG_DEBUG, LOG_INFO, LOG_NOTICE,
				       LOG_WARNING, LOG_ERR, LOG_CRIT };

static const char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
				"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static char *level_text[] = { "info: ", "notice: ", "warning: ", "error: ",
			      "critical: " };

static void
version_rename(log_channel chan) {
	unsigned int ver;
	char old_name[PATH_MAX+1];
	char new_name[PATH_MAX+1];
	
	ver = chan->out.file.versions;
	if (ver < 1)
		return;
	if (ver > LOG_MAX_VERSIONS)
		ver = LOG_MAX_VERSIONS;
	/*
	 * Need to have room for '.nn' (XXX assumes LOG_MAX_VERSIONS < 100)
	 */
	if (strlen(chan->out.file.name) > (PATH_MAX-3))
		return;
	for (ver--; ver > 0; ver--) {
		sprintf(old_name, "%s.%d", chan->out.file.name, ver-1);
		sprintf(new_name, "%s.%d", chan->out.file.name, ver);
		(void)rename(old_name, new_name);
	}
	sprintf(new_name, "%s.0", chan->out.file.name);
	(void)rename(chan->out.file.name, new_name);
}

FILE *
log_open_stream(log_channel chan) {
	FILE *stream;
	int fd, flags;
	struct stat sb;
	int regular;

	if (chan == NULL || chan->type != log_file) {
		errno = EINVAL;
		return (NULL);
	}
	
	/*
	 * Don't open already open streams
	 */
	if (chan->out.file.stream != NULL)
		return (chan->out.file.stream);

	if (stat(chan->out.file.name, &sb) < 0) {
		if (errno != ENOENT) {
			syslog(LOG_ERR,
			       "log_open_stream: stat of %s failed: %s",
			       chan->out.file.name, strerror(errno));
			chan->flags |= LOG_CHANNEL_BROKEN;
			return (NULL);
		}
		regular = 1;
	} else
		regular = (sb.st_mode & S_IFREG);

	if (chan->out.file.versions) {
		if (regular)
			version_rename(chan);
		else {
			syslog(LOG_ERR,
       "log_open_stream: want versions but %s isn't a regular file",
			       chan->out.file.name);
			chan->flags |= LOG_CHANNEL_BROKEN;
			errno = EINVAL;
			return (NULL);
		}
	}

	flags = O_WRONLY|O_CREAT|O_APPEND;

	if (chan->flags & LOG_TRUNCATE) {
		if (regular) {
			(void)unlink(chan->out.file.name);
			flags |= O_EXCL;
		} else {
			syslog(LOG_ERR,
       "log_open_stream: want truncation but %s isn't a regular file",
			       chan->out.file.name);
			chan->flags |= LOG_CHANNEL_BROKEN;
			errno = EINVAL;
			return (NULL);
		}
	}

	fd = open(chan->out.file.name, flags,
		  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	if (fd < 0) {
		syslog(LOG_ERR, "log_open_stream: open(%s) failed: %s",
		       chan->out.file.name, strerror(errno));
		chan->flags |= LOG_CHANNEL_BROKEN;
		return (NULL);
	}
	stream = fdopen(fd, "a");
	if (stream == NULL) {
		syslog(LOG_ERR, "log_open_stream: fdopen() failed");
		chan->flags |= LOG_CHANNEL_BROKEN;
		return (NULL);
	}

	chan->out.file.stream = stream;
	return (stream);
}

int
log_close_stream(log_channel chan) {
	FILE *stream;

	if (chan == NULL || chan->type != log_file) {
		errno = EINVAL;
		return (0);
	}
	stream = chan->out.file.stream;
	chan->out.file.stream = NULL;
	if (stream != NULL && fclose(stream) == EOF)
		return (-1);
	return (0);
}

FILE *
log_get_stream(log_channel chan) {
	if (chan == NULL || chan->type != log_file) {
		errno = EINVAL;
		return (NULL);
	}
	return (chan->out.file.stream);
}

char *
log_get_filename(log_channel chan) {
	if (chan == NULL || chan->type != log_file) {
		errno = EINVAL;
		return (NULL);
	}
	return (chan->out.file.name);
}

void
log_vwrite(log_context lc, int category, int level, const char *format, 
	   va_list args) {
	log_channel_list lcl;
	int pri, debugging, did_vsprintf = 0;
	int original_category;
	FILE *stream;
	log_context_p lcp;
	int chan_level;
	log_channel chan;
	struct timeval tv;
	struct tm *local_tm;
	char *category_name;
	char *level_str;
	char time_buf[256];
	char level_buf[256];

	lcp = lc.opaque;

	assert(lcp != NULL);

	debugging = (lcp->flags & LOG_OPTION_DEBUG);

	/*
	 * If not debugging, short circuit debugging messages very early.
	 */
	if (level > 0 && !debugging)
		return;

	if (category < 0 || category > lcp->num_categories)
		category = 0;		/* use default */
	original_category = category;
	lcl = lcp->categories[category];
	if (lcl == NULL) {
		category = 0;
		lcl = lcp->categories[0];
	}

	/*
	 * Get the current time and format it.
	 */
	time_buf[0]='\0';
	if (gettimeofday(&tv, NULL) < 0) {
		syslog(LOG_INFO, "gettimeofday failed in log_vwrite()");
	} else {
		local_tm = localtime((time_t *)&tv.tv_sec);
		if (local_tm != NULL) {
			sprintf(time_buf, "%02d-%s-%4d %02d:%02d:%02d.%03d ",
				local_tm->tm_mday, months[local_tm->tm_mon],
				local_tm->tm_year+1900, local_tm->tm_hour,
				local_tm->tm_min, local_tm->tm_sec,
				tv.tv_usec/1000);
		}
	}

	/*
	 * Make a string representation of the current category and level
	 */

	if (lcp->category_names != NULL &&
	    lcp->category_names[original_category] != NULL)
		category_name = lcp->category_names[original_category];
	else
		category_name = "";

	if (level >= log_critical) {
		if (level >= 0) {
			sprintf(level_buf, "debug %d: ", level);
			level_str = level_buf;
		} else
			level_str = level_text[-level-1];
	} else {
		sprintf(level_buf, "level %d: ", level);
		level_str = level_buf;
	}

	/*
	 * Write the message to channels.
	 */
	for ( /* nothing */; lcl != NULL; lcl = lcl->next) {
		chan = lcl->channel;

		if (chan->flags & (LOG_CHANNEL_BROKEN|LOG_CHANNEL_OFF))
			continue;

		/* some channels only log when debugging is on */
		if ((chan->flags & LOG_REQUIRE_DEBUG) && !debugging)
			continue;

		/* some channels use the global level */
		if (chan->flags & LOG_USE_CONTEXT_LEVEL) {
			chan_level = lcp->level;
		} else
			chan_level = chan->level;

		if (level > chan_level)
			continue;

		if (!did_vsprintf) {
			if (VSPRINTF((lcp->buffer, format, args)) >
			    LOG_BUFFER_SIZE) {
				syslog(LOG_CRIT,
				       "memory overrun in log_vwrite()");
				exit(1);
			}
			did_vsprintf = 1;
		}

		switch (chan->type) {
		case log_syslog:
			if (level >= log_critical)
				pri = (level >= 0) ? 0 : -level;
			else
				pri = -log_critical;
			syslog(chan->out.facility|syslog_priority[pri],
			       "%s%s%s%s",
			       (chan->flags & LOG_TIMESTAMP) ?	time_buf : "",
			       (chan->flags & LOG_PRINT_CATEGORY) ?
			       category_name : "",
			       (chan->flags & LOG_PRINT_LEVEL) ?
			       level_str : "",
			       lcp->buffer);
			break;
		case log_file:
			stream = chan->out.file.stream;
			if (stream == NULL) {
				stream = log_open_stream(chan);
				if (stream == NULL)
					break;
			}
			if (chan->out.file.max_size != ULONG_MAX) {
				long pos;
				
				pos = ftell(stream);
				if (pos >= 0 &&
				    pos > chan->out.file.max_size)
					break;
			}
			fprintf(stream, "%s%s%s%s\n", 
				(chan->flags & LOG_TIMESTAMP) ?	time_buf : "",
				(chan->flags & LOG_PRINT_CATEGORY) ?
				category_name : "",
				(chan->flags & LOG_PRINT_LEVEL) ?
				level_str : "",
				lcp->buffer);
			fflush(stream);
			break;
		case log_null:
			break;
		default:
			syslog(LOG_ERR,
			       "unknown channel type in log_vwrite()");
		}
	}
}

void
log_write(log_context lc, int category, int level, const char *format, ...) {
	va_list args;

	va_start(args, format);
	log_vwrite(lc, category, level, format, args);
	va_end(args);
}

/*
 * Functions to create, set, or destroy contexts
 */

int
log_new_context(int num_categories, char **category_names, log_context *lc) {
	log_context_p lcp;

	lcp = (log_context_p)malloc(sizeof (struct log_context_p));
	if (lcp == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	lcp->num_categories = num_categories;
	lcp->category_names = category_names;
	lcp->categories = (log_channel_list *)malloc(num_categories *
						     sizeof(log_channel_list));
	if (lcp->categories == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	memset(lcp->categories, '\0',
	       num_categories * sizeof(log_channel_list));
	lcp->flags = 0U;
	lcp->level = 0;
	lc->opaque = lcp;
	return (0);
}

void
log_free_context(log_context lc) {
	log_context_p lcp;
	log_channel_list lcl, lcl_next;
	log_channel chan;
	int i;

	lcp = lc.opaque;

	assert(lcp != NULL);
	for (i = 0; i < lcp->num_categories; i++)
		for (lcl = lcp->categories[i]; lcl != NULL; lcl = lcl_next) {
			lcl_next = lcl->next;
			chan = lcl->channel;
			(void)log_free_channel(chan);
			free(lcl);
		}
	free(lcp->categories);
	free(lcp);
	lc.opaque = NULL;
}

int
log_add_channel(log_context lc, int category, log_channel chan) {
	log_context_p lcp;
	log_channel_list lcl;

	lcp = lc.opaque;

	if (lcp == NULL || category < 0 || category >= lcp->num_categories) {
		errno = EINVAL;
		return (-1);
	}

	lcl = (log_channel_list)malloc(sizeof (struct log_channel_list));
	if (lcl == NULL) {
		errno = ENOMEM;
		return(-1);
	}
	lcl->channel = chan;
	lcl->next = lcp->categories[category];
	lcp->categories[category] = lcl;
	chan->references++;
	return (0);
}

int
log_remove_channel(log_context lc, int category, log_channel chan) {
	log_context_p lcp;
	log_channel_list lcl, prev_lcl, next_lcl;
	int found = 0;

	lcp = lc.opaque;

	if (lcp == NULL || category < 0 || category >= lcp->num_categories) {
		errno = EINVAL;
		return (-1);
	}

	for (prev_lcl = NULL, lcl = lcp->categories[category];
	     lcl != NULL;
	     lcl = next_lcl) {
		next_lcl = lcl->next;
		if (lcl->channel == chan) {
			log_free_channel(chan);
			if (prev_lcl != NULL)
				prev_lcl->next = next_lcl;
			else
				lcp->categories[category] = next_lcl;
			free(lcl);
			/*
			 * We just set found instead of returning because
			 * the channel might be on the list more than once.
			 */
			found = 1;
		} else
			prev_lcl = lcl;
	}
	if (!found) {
		errno = ENOENT;
		return (-1);
	}
	return (0);
}

int
log_option(log_context lc, int option, int value) {
	log_context_p lcp;
	lcp = lc.opaque;

	if (lcp == NULL) {
		errno = EINVAL;
		return (-1);
	}
	switch (option) {
	case LOG_OPTION_DEBUG:
		if (value)
			lcp->flags |= option;
		else
			lcp->flags &= ~option;
		break;
	case LOG_OPTION_LEVEL:
		lcp->level = value;
		break;
	default:
		errno = EINVAL;
		return (-1);
	}
	return (0);
}

int
log_category_is_active(log_context lc, int category) {
	log_context_p lcp;
	lcp = lc.opaque;

	if (lcp == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if (category >= 0 && category < lcp->num_categories &&
	    lcp->categories[category] != NULL)
		return (1);
	return (0);
}

log_channel
log_new_syslog_channel(unsigned int flags, int level, int facility) {
	log_channel chan;

	chan = (log_channel)malloc(sizeof(struct log_channel));
	if (chan == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	chan->type = log_syslog;
	chan->flags = flags;
	chan->level = level;
	chan->out.facility = facility;
	chan->references = 0;
	return (chan);
}

log_channel
log_new_file_channel(unsigned int flags, int level,
		     char *name, FILE *stream, unsigned int versions,
		     unsigned long max_size) {
	log_channel chan;

	chan = (log_channel)malloc(sizeof(struct log_channel));
	if (chan == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	chan->type = log_file;
	chan->flags = flags;
	chan->level = level;
	chan->out.file.name = name;
	chan->out.file.stream = stream;
	chan->out.file.versions = versions;
	chan->out.file.max_size = max_size;
	chan->references = 0;
	return (chan);
}

log_channel
log_new_null_channel() {
	log_channel chan;

	chan = (log_channel)malloc(sizeof(struct log_channel));
	if (chan == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	chan->type = log_null;
	chan->flags = LOG_CHANNEL_OFF;
	chan->level = log_info;
	chan->references = 0;
	return (chan);
}

int
log_inc_references(log_channel chan) {
	if (chan == NULL) {
		errno = EINVAL;
		return (-1);
	}
	chan->references++;
	return (0);
}

int
log_dec_references(log_channel chan) {
	if (chan == NULL || chan->references <= 0) {
		errno = EINVAL;
		return (-1);
	}
	chan->references--;
	return (0);
}

int
log_free_channel(log_channel chan) {
	if (chan == NULL || chan->references <= 0) {
		errno = EINVAL;
		return (-1);
	}
	chan->references--;
	if (chan->references == 0) {
		if (chan->type == log_file) {
			if ((chan->flags & LOG_CLOSE_STREAM) &&
			    chan->out.file.stream != NULL)
				(void)fclose(chan->out.file.stream);
			if (chan->out.file.name != NULL)
				free(chan->out.file.name);
		}
		free(chan);
	}
	return (0);
}

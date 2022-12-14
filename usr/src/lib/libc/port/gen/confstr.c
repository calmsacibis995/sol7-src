/*
 * Copyright (c) 1994-1998,  by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)confstr.c	1.11	98/01/21 SMI"

/* LINTLIBRARY */

#include "synonyms.h"
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

/* Keep in synch with execvp.c */

typedef struct {
	int	config_value;
	char	*value;
} config;

/*
 * keep these in the same order as in sys/unistd.h
 */
static const config	default_conf[] = {
	/*
	 * Leave _CS_PATH as the first entry.  There is a performance
	 * issue here since exec calls this function asking for CS_PATH.
	 * Also chack out execvp.c if the path must change.  This value
	 * may be hard coded there too...
	 */
	{ _CS_PATH,		"/usr/xpg4/bin:/usr/ccs/bin:/usr/bin"	},
	{ _CS_LFS_CFLAGS,	"-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64" },
	{ _CS_LFS_LDFLAGS,	""					},
	{ _CS_LFS_LIBS,		""					},
	{ _CS_LFS_LINTFLAGS,	"-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64" },
	{ _CS_LFS64_CFLAGS,	"-D_LARGEFILE64_SOURCE"			},
	{ _CS_LFS64_LDFLAGS,	""					},
	{ _CS_LFS64_LIBS,	""					},
	{ _CS_LFS64_LINTFLAGS,	"-D_LARGEFILE64_SOURCE"			},
	{ _CS_XBS5_ILP32_OFF32_CFLAGS,	""				},
	{ _CS_XBS5_ILP32_OFF32_LDFLAGS,	""				},
	{ _CS_XBS5_ILP32_OFF32_LIBS,	""				},
	{ _CS_XBS5_ILP32_OFF32_LINTFLAGS, ""				},
	{ _CS_XBS5_ILP32_OFFBIG_CFLAGS,
"-Xa -Usun -Usparc -Uunix -Ui386 -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64" },
	{ _CS_XBS5_ILP32_OFFBIG_LDFLAGS, ""				},
	{ _CS_XBS5_ILP32_OFFBIG_LIBS,	""				},
	{ _CS_XBS5_ILP32_OFFBIG_LINTFLAGS,
		"-Xa -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64"},
#ifndef __i386
	{ _CS_XBS5_LP64_OFF64_CFLAGS, "-xarch=v9 -D_LARGEFILE64_SOURCE"	},
	{ _CS_XBS5_LP64_OFF64_LDFLAGS,	""				},
	{ _CS_XBS5_LP64_OFF64_LIBS,	""				},
	{ _CS_XBS5_LP64_OFF64_LINTFLAGS, "-xarch=v9 -D_LARGEFILE64_SOURCE" },
	{ _CS_XBS5_LPBIG_OFFBIG_CFLAGS, "-xarch=v9 -D_LARGEFILE64_SOURCE" },
	{ _CS_XBS5_LPBIG_OFFBIG_LDFLAGS, ""				},
	{ _CS_XBS5_LPBIG_OFFBIG_LIBS,	""				},
	{ _CS_XBS5_LPBIG_OFFBIG_LINTFLAGS, "-xarch=v9 -D_LARGEFILE64_SOURCE" }
#endif
};

#define	CS_ENTRY_COUNT (sizeof (default_conf) / sizeof (config))


size_t
confstr(int name, char *buf, size_t length)
{
	size_t			conf_length;
	config			*entry;
	int			i;

	/*
	 * Make sure it is a know configuration parameter
	 */
	entry = (config *)default_conf;
	for (i = 0; i < CS_ENTRY_COUNT; i++) {
		if (name == entry->config_value) {
			/*
			 * Copy out the parameter from our tables.
			 */
			conf_length = strlen(entry->value) + 1;
			if (length != 0) {
				(void) strncpy(buf, entry->value, length);
				buf[length - 1] = '\0';
			}
			return (conf_length);
		}
		entry++;
	}

	/* If the entry was not found in table return an error */
	errno = EINVAL;
	return ((size_t)0);
}

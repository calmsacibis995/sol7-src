/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */
#pragma	ident	"@(#)kobj_stubs.c	1.12	98/01/13 SMI"

#include <sys/kobj.h>
#include <sys/machkobj.h>

/*
 * Stubs for entry points into
 * the stand-alone linker/loader.
 */

/*ARGSUSED*/
void
kobj_load_module(struct modctl *modp, int use_path)
{}

/*ARGSUSED*/
void
kobj_unload_module(struct modctl *modp)
{}

/*ARGSUSED*/
struct _buf *
kobj_open_path(char *name, int use_path)
{
	return (NULL);
}

/*ARGSUSED*/
struct _buf *
kobj_open_file(char *name)
{
	return (NULL);
}

/*ARGSUSED*/
int
kobj_read_file(struct _buf *file, char *buf, unsigned size, unsigned off)
{
	return (-1);
}

/*ARGSUSED*/
void
kobj_close_file(struct _buf *file)
{}

/*ARGSUSED*/
intptr_t
kobj_open(char *filename)
{
	return (-1L);
}

/*ARGSUSED*/
int
kobj_read(intptr_t descr, char *buf, unsigned size, unsigned offset)
{
	return (-1);
}

/*ARGSUSED*/
void
kobj_close(intptr_t descr)
{}

/*ARGSUSED*/
int
kobj_filbuf(struct _buf *f)
{
	return (-1);
}

/*ARGSUSED*/
int
kobj_addrcheck(void *xmp, caddr_t adr)
{
	return (1);
}

/*ARGSUSED*/
uintptr_t
kobj_getelfsym(char *name, void *mp, int *size)
{
	return (0);
}

/*ARGSUSED*/
void
kobj_getmodinfo(void *xmp, struct modinfo *modinfo)
{}

void
kobj_getpagesize()
{}

/*ARGSUSED*/
char *
kobj_getsymname(uintptr_t value, u_long *offset)
{
	return (NULL);
}

/*ARGSUSED*/
uintptr_t
kobj_getsymvalue(char *name, int kernelonly)
{
	return (0);
}

/*ARGSUSED*/
char *
kobj_searchsym(struct module *mp, uintptr_t value, u_long *offset)
{
	return (NULL);
}

/*ARGSUSED*/
uintptr_t
kobj_lookup(void *mod, char *name)
{
	return (0);
}

/*ARGSUSED*/
Sym *
kobj_lookup_all(struct module *mp, char *name, int include_self)
{
	return (NULL);
}

/*ARGSUSED*/
void *
kobj_alloc(size_t size, int flag)
{
	return (NULL);
}

/*ARGSUSED*/
void *
kobj_zalloc(size_t size, int flag)
{
	return (NULL);
}

/*ARGSUSED*/
void
kobj_free(void *address, size_t size)
{}

/*ARGSUSED*/
void
kobj_sync(void)
{}

/*ARGSUSED*/
void
kobj_stat_get(kobj_stat_t *kp)
{}

/*ARGSUSED*/
kobj_free_t *
kobj_create_managed()
{
	return (NULL);
}

/*ARGSUSED*/
void
kobj_manage_mem(caddr_t addr, size_t size, kobj_free_t *list)
{}

/*ARGSUSED*/
caddr_t
kobj_find_managed_mem(size_t size, kobj_free_t *list)
{
	return (NULL);
}

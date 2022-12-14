/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DDI_ISA_H
#define	_SYS_DDI_ISA_H

#pragma ident	"@(#)ddi_isa.h	1.11	97/04/08 SMI"

#include <sys/isa_defs.h>
#include <sys/dditypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

/*
 * These are the data access functions which the platform
 * can choose to define as functions or macro's.
 */

/*
 * DDI interfaces defined as macro's
 */

/*
 * DDI interfaces defined as functions
 */

#ifdef	__STDC__

#ifdef _LP64

uint8_t
ddi_mem_get8(ddi_acc_handle_t handle, uint8_t *host_addr);

uint16_t
ddi_mem_get16(ddi_acc_handle_t handle, uint16_t *host_addr);

uint32_t
ddi_mem_get32(ddi_acc_handle_t handle, uint32_t *host_addr);

uint64_t
ddi_mem_get64(ddi_acc_handle_t handle, uint64_t *host_addr);

void
ddi_mem_rep_get8(ddi_acc_handle_t handle, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags);

void
ddi_mem_rep_get16(ddi_acc_handle_t handle, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags);

void
ddi_mem_rep_get32(ddi_acc_handle_t handle, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags);

void
ddi_mem_rep_get64(ddi_acc_handle_t handle, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags);

void
ddi_mem_put8(ddi_acc_handle_t handle, uint8_t *dev_addr, uint8_t value);

void
ddi_mem_put16(ddi_acc_handle_t handle, uint16_t *dev_addr, uint16_t value);

void
ddi_mem_put32(ddi_acc_handle_t handle, uint32_t *dev_addr, uint32_t value);

void
ddi_mem_put64(ddi_acc_handle_t handle, uint64_t *dev_addr, uint64_t value);

void
ddi_mem_rep_put8(ddi_acc_handle_t handle, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags);

void
ddi_mem_rep_put16(ddi_acc_handle_t handle, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags);

void
ddi_mem_rep_put32(ddi_acc_handle_t handle, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags);

void
ddi_mem_rep_put64(ddi_acc_handle_t handle, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags);

uint8_t
ddi_io_get8(ddi_acc_handle_t handle, int dev_port);

uint16_t
ddi_io_get16(ddi_acc_handle_t handle, int dev_port);

uint32_t
ddi_io_get32(ddi_acc_handle_t handle, int dev_port);

void
ddi_io_rep_get8(ddi_acc_handle_t handle,
	uint8_t *host_addr, int dev_port, size_t repcount);

void
ddi_io_rep_get16(ddi_acc_handle_t handle,
	uint16_t *host_addr, int dev_port, size_t repcount);

void
ddi_io_rep_get32(ddi_acc_handle_t handle,
	uint32_t *host_addr, int dev_port, size_t repcount);

void
ddi_io_put8(ddi_acc_handle_t handle, int dev_port, uint8_t value);

void
ddi_io_put16(ddi_acc_handle_t handle, int dev_port, uint16_t value);

void
ddi_io_put32(ddi_acc_handle_t handle, int dev_port, uint32_t value);

void
ddi_io_rep_put8(ddi_acc_handle_t handle,
	uint8_t *host_addr, int dev_port, size_t repcount);

void
ddi_io_rep_put16(ddi_acc_handle_t handle,
	uint16_t *host_addr, int dev_port, size_t repcount);

void
ddi_io_rep_put32(ddi_acc_handle_t handle,
	uint32_t *host_addr, int dev_port, size_t repcount);


#else /* _ILP32 */

uint8_t
ddi_mem_getb(ddi_acc_handle_t handle, uint8_t *host_addr);
#define	ddi_mem_get8	ddi_mem_getb

uint16_t
ddi_mem_getw(ddi_acc_handle_t handle, uint16_t *host_addr);
#define	ddi_mem_get16	ddi_mem_getw

uint32_t
ddi_mem_getl(ddi_acc_handle_t handle, uint32_t *host_addr);
#define	ddi_mem_get32	ddi_mem_getl

uint64_t
ddi_mem_getll(ddi_acc_handle_t handle, uint64_t *host_addr);
#define	ddi_mem_get64	ddi_mem_getll

void
ddi_mem_rep_getb(ddi_acc_handle_t handle, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags);
#define	ddi_mem_rep_get8	ddi_mem_rep_getb

void
ddi_mem_rep_getw(ddi_acc_handle_t handle, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags);
#define	ddi_mem_rep_get16	ddi_mem_rep_getw

void
ddi_mem_rep_getl(ddi_acc_handle_t handle, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags);
#define	ddi_mem_rep_get32	ddi_mem_rep_getl

void
ddi_mem_rep_getll(ddi_acc_handle_t handle, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags);
#define	ddi_mem_rep_get64	ddi_mem_rep_getll

void
ddi_mem_putb(ddi_acc_handle_t handle, uint8_t *dev_addr, uint8_t value);
#define	ddi_mem_put8	ddi_mem_putb

void
ddi_mem_putw(ddi_acc_handle_t handle, uint16_t *dev_addr, uint16_t value);
#define	ddi_mem_put16	ddi_mem_putw

void
ddi_mem_putl(ddi_acc_handle_t handle, uint32_t *dev_addr, uint32_t value);
#define	ddi_mem_put32	ddi_mem_putl

void
ddi_mem_putll(ddi_acc_handle_t handle, uint64_t *dev_addr, uint64_t value);
#define	ddi_mem_put64	ddi_mem_putll

void
ddi_mem_rep_putb(ddi_acc_handle_t handle, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags);
#define	ddi_mem_rep_put8	ddi_mem_rep_putb

void
ddi_mem_rep_putw(ddi_acc_handle_t handle, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags);
#define	ddi_mem_rep_put16	ddi_mem_rep_putw

void
ddi_mem_rep_putl(ddi_acc_handle_t handle, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags);
#define	ddi_mem_rep_put32	ddi_mem_rep_putl

void
ddi_mem_rep_putll(ddi_acc_handle_t handle, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags);
#define	ddi_mem_rep_put64	ddi_mem_rep_putll

uint8_t
ddi_io_getb(ddi_acc_handle_t handle, int dev_port);
#define	ddi_io_get8	ddi_io_getb

uint16_t
ddi_io_getw(ddi_acc_handle_t handle, int dev_port);
#define	ddi_io_get16	ddi_io_getw

uint32_t
ddi_io_getl(ddi_acc_handle_t handle, int dev_port);
#define	ddi_io_get32	ddi_io_getl

void
ddi_io_rep_getb(ddi_acc_handle_t handle,
	uint8_t *host_addr, int dev_port, size_t repcount);
#define	ddi_io_rep_get8	ddi_io_rep_getb

void
ddi_io_rep_getw(ddi_acc_handle_t handle,
	uint16_t *host_addr, int dev_port, size_t repcount);
#define	ddi_io_rep_get16	ddi_io_rep_getw

void
ddi_io_rep_getl(ddi_acc_handle_t handle,
	uint32_t *host_addr, int dev_port, size_t repcount);
#define	ddi_io_rep_get32	ddi_io_rep_getl

void
ddi_io_putb(ddi_acc_handle_t handle, int dev_port, uint8_t value);
#define	ddi_io_put8	ddi_io_putb

void
ddi_io_putw(ddi_acc_handle_t handle, int dev_port, uint16_t value);
#define	ddi_io_put16	ddi_io_putw

void
ddi_io_putl(ddi_acc_handle_t handle, int dev_port, uint32_t value);
#define	ddi_io_put32	ddi_io_putl

void
ddi_io_rep_putb(ddi_acc_handle_t handle,
	uint8_t *host_addr, int dev_port, size_t repcount);
#define	ddi_io_rep_put8	ddi_io_rep_putb

void
ddi_io_rep_putw(ddi_acc_handle_t handle,
	uint16_t *host_addr, int dev_port, size_t repcount);
#define	ddi_io_rep_put16	ddi_io_rep_putw

void
ddi_io_rep_putl(ddi_acc_handle_t handle,
	uint32_t *host_addr, int dev_port, size_t repcount);
#define	ddi_io_rep_put32	ddi_io_rep_putl

#endif /* _LP64 */


/*
 * ahi_acc_attr flags
 */
#define	DDI_ACCATTR_CONFIG_SPACE	0x1
#define	DDI_ACCATTR_IO_SPACE		0x2
#define	DDI_ACCATTR_CPU_VADDR		0x4

typedef struct ddi_acc_impl {
	ddi_acc_hdl_t	ahi_common;
	ulong_t		ahi_acc_attr;

	uint8_t
		(*ahi_get8)(struct ddi_acc_impl *handle, uint8_t *addr);
	uint16_t
		(*ahi_get16)(struct ddi_acc_impl *handle, uint16_t *addr);
	uint32_t
		(*ahi_get32)(struct ddi_acc_impl *handle, uint32_t *addr);
	uint64_t
		(*ahi_get64)(struct ddi_acc_impl *handle, uint64_t *addr);

	void	(*ahi_put8)(struct ddi_acc_impl *handle, uint8_t *addr,
			uint8_t value);
	void	(*ahi_put16)(struct ddi_acc_impl *handle, uint16_t *addr,
			uint16_t value);
	void	(*ahi_put32)(struct ddi_acc_impl *handle, uint32_t *addr,
			uint32_t value);
	void	(*ahi_put64)(struct ddi_acc_impl *handle, uint64_t *addr,
			uint64_t value);

	void	(*ahi_rep_get8)(struct ddi_acc_impl *handle,
			uint8_t *host_addr, uint8_t *dev_addr,
			size_t repcount, uint_t flags);
	void	(*ahi_rep_get16)(struct ddi_acc_impl *handle,
			uint16_t *host_addr, uint16_t *dev_addr,
			size_t repcount, uint_t flags);
	void	(*ahi_rep_get32)(struct ddi_acc_impl *handle,
			uint32_t *host_addr, uint32_t *dev_addr,
			size_t repcount, uint_t flags);
	void	(*ahi_rep_get64)(struct ddi_acc_impl *handle,
			uint64_t *host_addr, uint64_t *dev_addr,
			size_t repcount, uint_t flags);

	void	(*ahi_rep_put8)(struct ddi_acc_impl *handle,
			uint8_t *host_addr, uint8_t *dev_addr,
			size_t repcount, uint_t flags);
	void	(*ahi_rep_put16)(struct ddi_acc_impl *handle,
			uint16_t *host_addr, uint16_t *dev_addr,
			size_t repcount, uint_t flags);
	void	(*ahi_rep_put32)(struct ddi_acc_impl *handle,
			uint32_t *host_addr, uint32_t *dev_addr,
			size_t repcount, uint_t flags);
	void	(*ahi_rep_put64)(struct ddi_acc_impl *handle,
			uint64_t *host_addr, uint64_t *dev_addr,
			size_t repcount, uint_t flags);

} ddi_acc_impl_t;


/*
 * Input functions to memory mapped IO
 */
uint8_t
i_ddi_vaddr_get8(ddi_acc_impl_t *hdlp, uint8_t *addr);

uint16_t
i_ddi_vaddr_get16(ddi_acc_impl_t *hdlp, uint16_t *addr);

uint32_t
i_ddi_vaddr_get32(ddi_acc_impl_t *hdlp, uint32_t *addr);

uint64_t
i_ddi_vaddr_get64(ddi_acc_impl_t *hdlp, uint64_t *addr);

uint16_t
i_ddi_vaddr_swap_get16(ddi_acc_impl_t *hdlp, uint16_t *addr);

uint32_t
i_ddi_vaddr_swap_get32(ddi_acc_impl_t *hdlp, uint32_t *addr);

uint64_t
i_ddi_vaddr_swap_get64(ddi_acc_impl_t *hdlp, uint64_t *addr);

/*
 * Output functions to memory mapped IO
 */
void
i_ddi_vaddr_put8(ddi_acc_impl_t *hdlp, uint8_t *addr, uint8_t value);

void
i_ddi_vaddr_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value);

void
i_ddi_vaddr_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value);

void
i_ddi_vaddr_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value);

void
i_ddi_vaddr_swap_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value);

void
i_ddi_vaddr_swap_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value);

void
i_ddi_vaddr_swap_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value);

/*
 * Repeated input functions for memory mapped IO
 */
void
i_ddi_vaddr_rep_get8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_vaddr_rep_get16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_vaddr_rep_get32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_vaddr_rep_get64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_vaddr_swap_rep_get16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_vaddr_swap_rep_get32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_vaddr_swap_rep_get64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags);

/*
 * Repeated output functions for memory mapped IO
 */
void
i_ddi_vaddr_rep_put8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_vaddr_rep_put16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_vaddr_rep_put32(ddi_acc_impl_t *hdl, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_vaddr_rep_put64(ddi_acc_impl_t *hdl, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_vaddr_swap_rep_put16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_vaddr_swap_rep_put32(ddi_acc_impl_t *hdl, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_vaddr_swap_rep_put64(ddi_acc_impl_t *hdl, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags);

/*
 * Input functions to IO space
 */
uint8_t
i_ddi_io_get8(ddi_acc_impl_t *hdlp, uint8_t *addr);

uint16_t
i_ddi_io_get16(ddi_acc_impl_t *hdlp, uint16_t *addr);

uint32_t
i_ddi_io_get32(ddi_acc_impl_t *hdlp, uint32_t *addr);

uint64_t
i_ddi_io_get64(ddi_acc_impl_t *hdlp, uint64_t *addr);

uint16_t
i_ddi_io_swap_get16(ddi_acc_impl_t *hdlp, uint16_t *addr);

uint32_t
i_ddi_io_swap_get32(ddi_acc_impl_t *hdlp, uint32_t *addr);

uint64_t
i_ddi_io_swap_get64(ddi_acc_impl_t *hdlp, uint64_t *addr);

uint16_t
i_ddi_io_swap_get16(ddi_acc_impl_t *hdlp, uint16_t *addr);

uint32_t
i_ddi_io_swap_get32(ddi_acc_impl_t *hdlp, uint32_t *addr);

uint64_t
i_ddi_io_swap_get64(ddi_acc_impl_t *hdlp, uint64_t *addr);


/*
 * Output functions to IO space
 */
void
i_ddi_io_put8(ddi_acc_impl_t *hdlp, uint8_t *addr, uint8_t value);

void
i_ddi_io_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value);

void
i_ddi_io_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value);

void
i_ddi_io_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value);

void
i_ddi_io_swap_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value);

void
i_ddi_io_swap_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value);

void
i_ddi_io_swap_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value);

/*
 * Repeated input functions for IO space
 */
void
i_ddi_io_rep_get8(ddi_acc_impl_t *hdlp, uint8_t *host_addr, uint8_t *dev_addr,
	size_t repcount, uint_t flags);

void
i_ddi_io_rep_get16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_io_rep_get32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_io_rep_get64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_io_swap_rep_get8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_io_swap_rep_get16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_io_swap_rep_get32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_io_swap_rep_get64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags);

/*
 * Repeated output functions for IO space
 */
void
i_ddi_io_rep_put8(ddi_acc_impl_t *hdlp, uint8_t *host_addr, uint8_t *dev_addr,
	size_t repcount, uint_t flags);

void
i_ddi_io_rep_put16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_io_rep_put32(ddi_acc_impl_t *hdl, uint32_t *host_addr, uint32_t *dev_addr,
	size_t repcount, uint_t flags);

void
i_ddi_io_rep_put64(ddi_acc_impl_t *hdl, uint64_t *host_addr, uint64_t *dev_addr,
	size_t repcount, uint_t flags);

void
i_ddi_io_swap_rep_put16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_io_swap_rep_put32(ddi_acc_impl_t *hdl, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags);

void
i_ddi_io_swap_rep_put64(ddi_acc_impl_t *hdl, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags);

int
i_ddi_vaddr_get(ddi_acc_impl_t *hdlp, caddr_t host_addr,
	caddr_t dev_addr, size_t wordsz, size_t count, ulong_t dev_addrtype);

int
i_ddi_vaddr_put(ddi_acc_impl_t *hdlp, caddr_t host_addr,
	caddr_t dev_addr, size_t wordsz, size_t count, ulong_t dev_addrtype);

int
i_ddi_io_get(ddi_acc_impl_t *hdlp, caddr_t host_addr,
	caddr_t dev_addr, size_t wordsz, size_t count, ulong_t dev_addrtype);

int
i_ddi_io_put(ddi_acc_impl_t *hdlp, caddr_t host_addr,
	caddr_t dev_addr, size_t wordsz, size_t count, ulong_t dev_addrtype);

uchar_t
i_pci_getb(int b, int d, int f, int r);

ushort_t
i_pci_getw(int b, int d, int f, int r);

ulong_t
i_pci_getl(int b, int d, int f, int r);

void
i_pci_putb(int b, int d, int f, int r, uchar_t v);

void
i_pci_putw(int b, int d, int f, int r, ushort_t v);

void
i_pci_putl(int b, int d, int f, int r, ulong_t v);

#endif	/* __STDC__ */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DDI_ISA_H */

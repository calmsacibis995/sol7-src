#pragma ident	"@(#)xnew.h	1.2	97/04/24 SMI"
#ifndef xnew_INCLUDED
#define xnew_INCLUDED 1

#ifdef SP_NEW_H_MISSING

typedef void (*VFP)();

#ifdef SP_SET_NEW_HANDLER_EXTERN_C
extern "C" 
#endif
void set_new_handler(VFP);

#ifndef SP_DECLARE_PLACEMENT_OPERATOR_NEW
#define SP_DECLARE_PLACEMENT_OPERATOR_NEW
#endif

#else /* not SP_NEW_H_MISSING */

#include <new.h>

#endif /* not SP_NEW_H_MISSING */

#ifdef SP_DECLARE_PLACEMENT_OPERATOR_NEW

inline
void *operator new(size_t, void *p)
{
    return p;
}

#endif /* SP_DECLARE_PLACEMENT_OPERATOR_NEW */

#endif /* not xnew_INCLUDED */

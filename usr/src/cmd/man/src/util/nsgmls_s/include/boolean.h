#pragma ident	"@(#)Boolean.h	1.2	97/04/24 SMI"
// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.

#ifndef Boolean_INCLUDED
#define Boolean_INCLUDED 1

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

#ifdef SP_HAVE_BOOL

typedef bool Boolean;
typedef char PackedBoolean;

#else /* not SP_HAVE_BOOL */

typedef int Boolean;
typedef char PackedBoolean;

#endif /* not SP_HAVE_BOOL */

#ifdef SP_NAMESPACE
}
#endif

#ifndef SP_HAVE_BOOL

typedef int bool;

const int true = 1;
const int false = 0;

#endif /* not SP_HAVE_BOOL */

#endif /* not Boolean_INCLUDED */

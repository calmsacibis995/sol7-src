#pragma ident	"@(#)CharsetRegistry.h	1.2	97/04/24 SMI"
// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.

#ifndef CharsetRegistry_INCLUDED
#define CharsetRegistry_INCLUDED 1
#ifdef __GNUG__
#pragma interface
#endif

#include "Boolean.h"
#include "types.h"
#include "StringC.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

class PublicId;
class CharsetInfo;
class UnivCharsetDesc;

class CharsetRegistry {
public:
  static Boolean findCharset(const PublicId &, const CharsetInfo &,
			     UnivCharsetDesc &);
};

#ifdef SP_NAMESPACE
}
#endif

#endif /* not CharsetRegistry_INCLUDED */

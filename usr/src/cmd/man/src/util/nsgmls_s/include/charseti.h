#pragma ident	"@(#)CharsetInfo.h	1.2	97/04/24 SMI"
// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.

#ifndef CharsetInfo_INCLUDED
#define CharsetInfo_INCLUDED 1
#ifdef __GNUG__
#pragma interface
#endif

#include <limits.h>
#include "UnivCharsetDesc.h"
#include "Boolean.h"
#include "types.h"
#include "StringC.h"
#include "ISet.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

class SP_API CharsetInfo {
public:
  CharsetInfo();
  CharsetInfo(const UnivCharsetDesc &);
  void set(const UnivCharsetDesc &);
  // Use only for characters guaranteed to me in the C basic execution
  // character set and which have been verified to be in this
  // character set.
  Char execToDesc(char) const;
  StringC execToDesc(const char *s) const;
  Boolean descToUniv(WideChar from, UnivChar &to) const;
  Boolean descToUniv(WideChar from, UnivChar &to, WideChar &alsoMax) const;
  // Return 0 for no matches, 1 for 1, 2 for more than 1
  // to gets the first character; toSet gets all the characters
  // if there's more than 1.
  unsigned univToDesc(UnivChar from, WideChar &to, ISet<WideChar> &toSet)
       const;
  unsigned univToDesc(UnivChar from, WideChar &to, ISet<WideChar> &toSet,
		      WideChar &count)
       const;
  void getDescSet(ISet<Char> &) const;
  int digitWeight(Char) const;
  const UnivCharsetDesc &desc() const;
private:
  void init();
  UnivCharsetDesc desc_;
  enum { nSmall = 128 };
  // 0 for no matches, 1 for 1, 2 for more than 1
  unsigned smallUnivValid_[nSmall];
  WideChar smallUnivToDesc_[nSmall];
  PackedBoolean smallDescValid_[nSmall];
  UnivChar smallDescToUniv_[nSmall];
  Char execToDesc_[UCHAR_MAX + 1];
};

inline
unsigned CharsetInfo::univToDesc(UnivChar from, WideChar &to,
				 ISet<WideChar> &toSet)
     const
{
  if (from < nSmall && smallUnivValid_[from] <= 1) {
    if (smallUnivValid_[from]) {
      to = smallUnivToDesc_[from];
      return 1;
    }
    else
      return 0;
  }
  else
    return desc_.univToDesc(from, to, toSet);
}

inline
Boolean CharsetInfo::descToUniv(UnivChar from, WideChar &to) const
{
  if (from < nSmall) {
    if (smallDescValid_[from]) {
      to = smallDescToUniv_[from];
      return 1;
    }
    else
      return 0;
  }
  else
    return desc_.descToUniv(from, to);
}

inline
Char CharsetInfo::execToDesc(char c) const
{
  return execToDesc_[(unsigned char)c];
}

inline
Boolean CharsetInfo::descToUniv(WideChar from, UnivChar &to,
				WideChar &alsoMax) const
{
  return desc_.descToUniv(from, to, alsoMax);
}

inline
unsigned CharsetInfo::univToDesc(UnivChar from, WideChar &to,
				 ISet<WideChar> &toSet, WideChar &count)
     const
{
  return desc_.univToDesc(from, to, toSet, count);
}

inline
const UnivCharsetDesc &CharsetInfo::desc() const
{
  return desc_;
}

#ifdef SP_NAMESPACE
}
#endif

#endif /* not CharsetInfo_INCLUDED */

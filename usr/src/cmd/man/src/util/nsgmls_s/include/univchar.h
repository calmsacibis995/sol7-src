#pragma ident	"@(#)UnivCharsetDesc.h	1.2	97/04/24 SMI"
// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.

#ifndef UnivCharsetDesc_INCLUDED
#define UnivCharsetDesc_INCLUDED 1
#ifdef __GNUG__
#pragma interface
#endif

#include <stddef.h>
#include "types.h"
#include "RangeMap.h"
#include "Boolean.h"
#include "ISet.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

class SP_API UnivCharsetDesc {
public:
  struct SP_API Range {
    WideChar descMin;
    // Note that this is a count, as in the SGML declaration,
    // rather than a maximum.
    unsigned long count;
    UnivChar univMin;
  };
  enum {
    zero = 48,
    A = 65,
    a = 97,
    tab = 9,
    rs = 10,
    re = 13,
    space = 32,
    exclamation = 33,
    lessThan = 60,
    greaterThan = 62
    };
  UnivCharsetDesc();
  UnivCharsetDesc(const Range *, size_t);
  void set(const Range *, size_t);
  Boolean descToUniv(WideChar from, UnivChar &to) const;
  Boolean descToUniv(WideChar from, UnivChar &to, WideChar &alsoMax) const;
  // Return 0 for no matches, 1 for 1, 2 for more than 1
  unsigned univToDesc(UnivChar from, WideChar &to, ISet<WideChar> &toSet)
       const;
  unsigned univToDesc(UnivChar from, WideChar &to, ISet<WideChar> &toSet,
		      WideChar &count)
       const;
  void addRange(WideChar descMin, WideChar descMax, UnivChar univMin);
  void addBaseRange(const UnivCharsetDesc &baseSet,
		    WideChar descMin,
		    WideChar descMax,
		    WideChar baseMin,
		    ISet<WideChar> &baseMissing);
  WideChar maxDesc() const;
private:
  RangeMap<WideChar,UnivChar> descToUniv_;
  friend class UnivCharsetDescIter;
};

class SP_API UnivCharsetDescIter {
public:
  UnivCharsetDescIter(const UnivCharsetDesc &);
  Boolean next(WideChar &descMin, WideChar &descMax, UnivChar &univMin);
private:
  RangeMapIter<WideChar,UnivChar> iter_;
};

inline
Boolean UnivCharsetDesc::descToUniv(WideChar from, UnivChar &to) const
{
  WideChar tem;
  return descToUniv_.map(from, to, tem);
}

inline
Boolean UnivCharsetDesc::descToUniv(WideChar from, UnivChar &to,
				    WideChar &alsoMax) const
{
  return descToUniv_.map(from, to, alsoMax);
}

inline
unsigned UnivCharsetDesc::univToDesc(UnivChar from, WideChar &to,
				     ISet<WideChar> &toSet) const
{
  WideChar tem;
  return descToUniv_.inverseMap(from, to, toSet, tem);
}

inline
unsigned UnivCharsetDesc::univToDesc(UnivChar from, WideChar &to,
				     ISet<WideChar> &toSet,
				     WideChar &count) const
{
  return descToUniv_.inverseMap(from, to, toSet, count);
}

inline
void UnivCharsetDesc::addRange(WideChar descMin,
			       WideChar descMax,
			       UnivChar univMin)
{
  descToUniv_.addRange(descMin, descMax, univMin);
}

inline
UnivCharsetDescIter::UnivCharsetDescIter(const UnivCharsetDesc &desc)
: iter_(desc.descToUniv_)
{
}

inline
Boolean UnivCharsetDescIter::next(WideChar &descMin,
				  WideChar &descMax,
				  UnivChar &univMin)
{
  return iter_.next(descMin, descMax, univMin);
}

#ifdef SP_NAMESPACE
}
#endif

#endif /* not UnivCharsetDesc_INCLUDED */

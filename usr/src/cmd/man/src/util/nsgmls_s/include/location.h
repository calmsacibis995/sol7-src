#pragma ident	"@(#)Location.h	1.2	97/04/24 SMI"
// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.

#ifndef Location_INCLUDED
#define Location_INCLUDED 1
#ifdef __GNUG__
#pragma interface
#endif

#include "types.h"
#include "Boolean.h"
#include "Ptr.h"
#include "Resource.h"
#include "Boolean.h"
#include "Vector.h"
#include "Owner.h"
#include "StringC.h"
#include "rtti.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

class ExternalInfo;
class EntityOrigin;
class InputSourceOrigin;
class Entity;
class EntityDecl;
class Location;
class Markup;
class Text;
class NamedCharRef;

class SP_API Origin : public Resource {
public:
  virtual ~Origin();
  virtual const EntityOrigin *asEntityOrigin() const;
  virtual const InputSourceOrigin *asInputSourceOrigin() const;
  virtual const Location &parent() const = 0;
  virtual Index refLength() const;
  virtual Boolean origChars(const Char *&) const;
  virtual Boolean inBracketedTextOpenDelim() const;
  virtual Boolean inBracketedTextCloseDelim() const;
  virtual Boolean isNumericCharRef(const Markup *&markup) const;
  virtual Boolean isNamedCharRef(Index ind, NamedCharRef &ref) const;
  virtual const EntityDecl *entityDecl() const;
};

class SP_API Location {
public:
  Location();
  Location(Origin *, Index);
  Location(ConstPtr<Origin>, Index);
  void operator+=(Index i) { index_ += i; }
  void operator-=(Index i) { index_ -= i; }
  Index index() const { return index_; }
  const ConstPtr<Origin> &origin() const { return origin_; }
  void clear() { origin_.clear(); }
  void swap(Location &to) {
    origin_.swap(to.origin_);
    Index tem = to.index_;
    to.index_ = index_;
    index_ = tem;
  }
private:
  ConstPtr<Origin> origin_;
  Index index_;
};

class SP_API ExternalInfo {
  RTTI_CLASS
public:
  virtual ~ExternalInfo();
};

class SP_API NamedCharRef {
public:
  enum RefEndType {
    endOmitted,
    endRE,
    endRefc
    };
  NamedCharRef();
  NamedCharRef(Index, RefEndType, const StringC &);
  Index refStartIndex() const;
  RefEndType refEndType() const;
  const StringC &origName() const;
  void set(Index, RefEndType, const Char *, size_t);
private:
  Index refStartIndex_;
  RefEndType refEndType_;
  StringC origName_;
};

struct SP_API InputSourceOriginNamedCharRef {
  Index replacementIndex;
  size_t origNameOffset;
  Index refStartIndex;
  NamedCharRef::RefEndType refEndType;
};

class SP_API InputSourceOrigin : public Origin {
public:
  InputSourceOrigin();
  InputSourceOrigin(const Location &refLocation);
  const Location &parent() const;
  const ExternalInfo *externalInfo() const;
  Offset startOffset(Index ind) const;
  void noteCharRef(Index replacementIndex, const NamedCharRef &);
  Boolean isNamedCharRef(Index ind, NamedCharRef &ref) const;
  void setExternalInfo(ExternalInfo *);
  virtual Boolean defLocation(Offset off, Location &) const;
  virtual InputSourceOrigin *copy() const;
  const InputSourceOrigin *asInputSourceOrigin() const;
  virtual const StringC *entityName() const;
private:
  InputSourceOrigin(const InputSourceOrigin &); // undefined
  void operator=(const InputSourceOrigin &);	// undefined
  size_t nPrecedingCharRefs(Index ind) const;
  Vector<InputSourceOriginNamedCharRef> charRefs_;
  StringC charRefOrigNames_;
  Owner<ExternalInfo> externalInfo_; // 0 for internal entities
  Location refLocation_;	// where referenced from
};

// a delimiter specified in bracketed text

class SP_API BracketOrigin : public Origin {
public:
  enum Position { open, close };
  BracketOrigin(const Location &, Position);
  const Location &parent() const;
  Boolean inBracketedTextOpenDelim() const;
  Boolean inBracketedTextCloseDelim() const;
private:
  Position pos_;
  Location loc_;
};

class SP_API ReplacementOrigin : public Origin {
public:
  ReplacementOrigin(const Location &, Char origChar);
  const Location &parent() const;
  Boolean origChars(const Char *&) const;
private:
  Location loc_;
  Char origChar_;
};

class SP_API MultiReplacementOrigin : public Origin {
public:
  MultiReplacementOrigin(const Location &, StringC &origChars);
  const Location &parent() const;
  Boolean origChars(const Char *&) const;
private:
  Location loc_;
  StringC origChars_;
};

inline
Index NamedCharRef::refStartIndex() const
{
  return refStartIndex_;
}

inline
NamedCharRef::RefEndType NamedCharRef::refEndType() const
{
  return refEndType_;
}

inline
const StringC &NamedCharRef::origName() const
{
  return origName_;
}

inline
const ExternalInfo *InputSourceOrigin::externalInfo() const
{
  return externalInfo_.pointer();
}

#ifdef SP_NAMESPACE
}
#endif

#endif /* not Location_INCLUDED */

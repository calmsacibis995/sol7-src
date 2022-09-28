#pragma ident	"@(#)entmgr_inst.m4	1.2	97/04/24 SMI"
// Copyright (c) 1995 James Clark
// See the file COPYING for copying permission.

#include "splib.h"

#ifdef SP_MANUAL_INST

#define SP_DEFINE_TEMPLATES
#include "Owner.h"
#include "CopyOwner.h"
#include "RangeMap.h"
#include "Ptr.h"
#include "StringOf.h"
#include "StringC.h"
#include "Vector.h"
#include "ISet.h"
#include "ISetIter.h"
#include "XcharMap.h"
#include "SubstTable.h"
#include "StringResource.h"
#undef SP_DEFINE_TEMPLATES

#include "types.h"
#include "Location.h"
#include "Message.h"
#include "NamedResource.h"
#include "EntityManager.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

__instantiate(Ptr<InputSourceOrigin>)
__instantiate(ConstPtr<InputSourceOrigin>)
__instantiate(Ptr<Origin>)
__instantiate(ConstPtr<Origin>)
__instantiate(Ptr<SharedXcharMap<unsigned char> >)
__instantiate(ConstPtr<SharedXcharMap<unsigned char> >)
__instantiate(Ptr<SharedXcharMap<PackedBoolean> >)
__instantiate(ConstPtr<SharedXcharMap<PackedBoolean> >)
__instantiate(Ptr<SharedXcharMap<EquivCode> >)
__instantiate(ConstPtr<SharedXcharMap<EquivCode> >)
__instantiate(Ptr<StringResource<Char> >)
__instantiate(ConstPtr<StringResource<Char> >)
__instantiate(Ptr<NamedResource>)
__instantiate(ConstPtr<NamedResource>)
__instantiate(Ptr<EntityManager>)
__instantiate(ConstPtr<EntityManager>)
__instantiate(Ptr<EntityCatalog>)
__instantiate(ConstPtr<EntityCatalog>)
__instantiate(Owner<MessageArg>)
__instantiate(CopyOwner<MessageArg>)
__instantiate(String<Char>)
__instantiate(Vector<OpenElementInfo>)
__instantiate(Vector<CopyOwner<MessageArg> >)
__instantiate(SharedXcharMap<unsigned char>)
__instantiate(XcharMap<unsigned char>)
__instantiate(`Vector<RangeMapRange<WideChar,UnivChar> >')
__instantiate(`RangeMapIter<WideChar,UnivChar>')
__instantiate(`RangeMap<WideChar,UnivChar>')
__instantiate(Vector<InputSourceOriginNamedCharRef>)
__instantiate(Vector<StringC>)
__instantiate(Vector<String<EquivCode> >)
__instantiate(Owner<ExternalInfo>)
__instantiate(ISet<Char>)
__instantiate(Vector<ISetRange<Char> >)
__instantiate(ISet<WideChar>)
__instantiate(ISetIter<Char>)
__instantiate(ISetIter<WideChar>)
__instantiate(Vector<ISetRange<WideChar> >)
__instantiate(SubstTable<Char>)
__instantiate(SharedXcharMap<PackedBoolean>)
__instantiate(SharedXcharMap<unsigned char>)
__instantiate(SharedXcharMap<EquivCode>)
__instantiate(String<EquivCode>)
__instantiate(String<SyntaxChar>)
__instantiate(XcharMap<PackedBoolean>)
__instantiate(XcharMap<unsigned char>)
__instantiate(XcharMap<EquivCode>)
__instantiate(Vector<char>)
__instantiate(Vector<PackedBoolean>)
__instantiate(SubstTable<Char>)

#ifdef SP_NAMESPACE
}
#endif

#endif /* SP_MANUAL_INST */

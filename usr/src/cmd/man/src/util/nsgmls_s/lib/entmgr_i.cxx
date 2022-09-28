#pragma ident	"@(#)instmac.m4	1.2	97/04/24 SMI"
#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif


#ifdef SP_NAMESPACE
}
#endif
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

#ifdef __DECCXX
#pragma define_template Ptr<InputSourceOrigin>
#else
#ifdef SP_ANSI_CLASS_INST
template class Ptr<InputSourceOrigin>;
#else
typedef Ptr<InputSourceOrigin> Dummy_0;
#endif
#endif
#ifdef __DECCXX
#pragma define_template ConstPtr<InputSourceOrigin>
#else
#ifdef SP_ANSI_CLASS_INST
template class ConstPtr<InputSourceOrigin>;
#else
typedef ConstPtr<InputSourceOrigin> Dummy_1;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Ptr<Origin>
#else
#ifdef SP_ANSI_CLASS_INST
template class Ptr<Origin>;
#else
typedef Ptr<Origin> Dummy_2;
#endif
#endif
#ifdef __DECCXX
#pragma define_template ConstPtr<Origin>
#else
#ifdef SP_ANSI_CLASS_INST
template class ConstPtr<Origin>;
#else
typedef ConstPtr<Origin> Dummy_3;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Ptr<SharedXcharMap<unsigned char> >
#else
#ifdef SP_ANSI_CLASS_INST
template class Ptr<SharedXcharMap<unsigned char> >;
#else
typedef Ptr<SharedXcharMap<unsigned char> > Dummy_4;
#endif
#endif
#ifdef __DECCXX
#pragma define_template ConstPtr<SharedXcharMap<unsigned char> >
#else
#ifdef SP_ANSI_CLASS_INST
template class ConstPtr<SharedXcharMap<unsigned char> >;
#else
typedef ConstPtr<SharedXcharMap<unsigned char> > Dummy_5;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Ptr<SharedXcharMap<PackedBoolean> >
#else
#ifdef SP_ANSI_CLASS_INST
template class Ptr<SharedXcharMap<PackedBoolean> >;
#else
typedef Ptr<SharedXcharMap<PackedBoolean> > Dummy_6;
#endif
#endif
#ifdef __DECCXX
#pragma define_template ConstPtr<SharedXcharMap<PackedBoolean> >
#else
#ifdef SP_ANSI_CLASS_INST
template class ConstPtr<SharedXcharMap<PackedBoolean> >;
#else
typedef ConstPtr<SharedXcharMap<PackedBoolean> > Dummy_7;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Ptr<SharedXcharMap<EquivCode> >
#else
#ifdef SP_ANSI_CLASS_INST
template class Ptr<SharedXcharMap<EquivCode> >;
#else
typedef Ptr<SharedXcharMap<EquivCode> > Dummy_8;
#endif
#endif
#ifdef __DECCXX
#pragma define_template ConstPtr<SharedXcharMap<EquivCode> >
#else
#ifdef SP_ANSI_CLASS_INST
template class ConstPtr<SharedXcharMap<EquivCode> >;
#else
typedef ConstPtr<SharedXcharMap<EquivCode> > Dummy_9;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Ptr<StringResource<Char> >
#else
#ifdef SP_ANSI_CLASS_INST
template class Ptr<StringResource<Char> >;
#else
typedef Ptr<StringResource<Char> > Dummy_10;
#endif
#endif
#ifdef __DECCXX
#pragma define_template ConstPtr<StringResource<Char> >
#else
#ifdef SP_ANSI_CLASS_INST
template class ConstPtr<StringResource<Char> >;
#else
typedef ConstPtr<StringResource<Char> > Dummy_11;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Ptr<NamedResource>
#else
#ifdef SP_ANSI_CLASS_INST
template class Ptr<NamedResource>;
#else
typedef Ptr<NamedResource> Dummy_12;
#endif
#endif
#ifdef __DECCXX
#pragma define_template ConstPtr<NamedResource>
#else
#ifdef SP_ANSI_CLASS_INST
template class ConstPtr<NamedResource>;
#else
typedef ConstPtr<NamedResource> Dummy_13;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Ptr<EntityManager>
#else
#ifdef SP_ANSI_CLASS_INST
template class Ptr<EntityManager>;
#else
typedef Ptr<EntityManager> Dummy_14;
#endif
#endif
#ifdef __DECCXX
#pragma define_template ConstPtr<EntityManager>
#else
#ifdef SP_ANSI_CLASS_INST
template class ConstPtr<EntityManager>;
#else
typedef ConstPtr<EntityManager> Dummy_15;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Ptr<EntityCatalog>
#else
#ifdef SP_ANSI_CLASS_INST
template class Ptr<EntityCatalog>;
#else
typedef Ptr<EntityCatalog> Dummy_16;
#endif
#endif
#ifdef __DECCXX
#pragma define_template ConstPtr<EntityCatalog>
#else
#ifdef SP_ANSI_CLASS_INST
template class ConstPtr<EntityCatalog>;
#else
typedef ConstPtr<EntityCatalog> Dummy_17;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Owner<MessageArg>
#else
#ifdef SP_ANSI_CLASS_INST
template class Owner<MessageArg>;
#else
typedef Owner<MessageArg> Dummy_18;
#endif
#endif
#ifdef __DECCXX
#pragma define_template CopyOwner<MessageArg>
#else
#ifdef SP_ANSI_CLASS_INST
template class CopyOwner<MessageArg>;
#else
typedef CopyOwner<MessageArg> Dummy_19;
#endif
#endif
#ifdef __DECCXX
#pragma define_template String<Char>
#else
#ifdef SP_ANSI_CLASS_INST
template class String<Char>;
#else
typedef String<Char> Dummy_20;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Vector<OpenElementInfo>
#else
#ifdef SP_ANSI_CLASS_INST
template class Vector<OpenElementInfo>;
#else
typedef Vector<OpenElementInfo> Dummy_21;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Vector<CopyOwner<MessageArg> >
#else
#ifdef SP_ANSI_CLASS_INST
template class Vector<CopyOwner<MessageArg> >;
#else
typedef Vector<CopyOwner<MessageArg> > Dummy_22;
#endif
#endif
#ifdef __DECCXX
#pragma define_template SharedXcharMap<unsigned char>
#else
#ifdef SP_ANSI_CLASS_INST
template class SharedXcharMap<unsigned char>;
#else
typedef SharedXcharMap<unsigned char> Dummy_23;
#endif
#endif
#ifdef __DECCXX
#pragma define_template XcharMap<unsigned char>
#else
#ifdef SP_ANSI_CLASS_INST
template class XcharMap<unsigned char>;
#else
typedef XcharMap<unsigned char> Dummy_24;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Vector<RangeMapRange<WideChar,UnivChar> >
#else
#ifdef SP_ANSI_CLASS_INST
template class Vector<RangeMapRange<WideChar,UnivChar> >;
#else
typedef Vector<RangeMapRange<WideChar,UnivChar> > Dummy_25;
#endif
#endif
#ifdef __DECCXX
#pragma define_template RangeMapIter<WideChar,UnivChar>
#else
#ifdef SP_ANSI_CLASS_INST
template class RangeMapIter<WideChar,UnivChar>;
#else
typedef RangeMapIter<WideChar,UnivChar> Dummy_26;
#endif
#endif
#ifdef __DECCXX
#pragma define_template RangeMap<WideChar,UnivChar>
#else
#ifdef SP_ANSI_CLASS_INST
template class RangeMap<WideChar,UnivChar>;
#else
typedef RangeMap<WideChar,UnivChar> Dummy_27;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Vector<InputSourceOriginNamedCharRef>
#else
#ifdef SP_ANSI_CLASS_INST
template class Vector<InputSourceOriginNamedCharRef>;
#else
typedef Vector<InputSourceOriginNamedCharRef> Dummy_28;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Vector<StringC>
#else
#ifdef SP_ANSI_CLASS_INST
template class Vector<StringC>;
#else
typedef Vector<StringC> Dummy_29;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Vector<String<EquivCode> >
#else
#ifdef SP_ANSI_CLASS_INST
template class Vector<String<EquivCode> >;
#else
typedef Vector<String<EquivCode> > Dummy_30;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Owner<ExternalInfo>
#else
#ifdef SP_ANSI_CLASS_INST
template class Owner<ExternalInfo>;
#else
typedef Owner<ExternalInfo> Dummy_31;
#endif
#endif
#ifdef __DECCXX
#pragma define_template ISet<Char>
#else
#ifdef SP_ANSI_CLASS_INST
template class ISet<Char>;
#else
typedef ISet<Char> Dummy_32;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Vector<ISetRange<Char> >
#else
#ifdef SP_ANSI_CLASS_INST
template class Vector<ISetRange<Char> >;
#else
typedef Vector<ISetRange<Char> > Dummy_33;
#endif
#endif
#ifdef __DECCXX
#pragma define_template ISet<WideChar>
#else
#ifdef SP_ANSI_CLASS_INST
template class ISet<WideChar>;
#else
typedef ISet<WideChar> Dummy_34;
#endif
#endif
#ifdef __DECCXX
#pragma define_template ISetIter<Char>
#else
#ifdef SP_ANSI_CLASS_INST
template class ISetIter<Char>;
#else
typedef ISetIter<Char> Dummy_35;
#endif
#endif
#ifdef __DECCXX
#pragma define_template ISetIter<WideChar>
#else
#ifdef SP_ANSI_CLASS_INST
template class ISetIter<WideChar>;
#else
typedef ISetIter<WideChar> Dummy_36;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Vector<ISetRange<WideChar> >
#else
#ifdef SP_ANSI_CLASS_INST
template class Vector<ISetRange<WideChar> >;
#else
typedef Vector<ISetRange<WideChar> > Dummy_37;
#endif
#endif
#ifdef __DECCXX
#pragma define_template SubstTable<Char>
#else
#ifdef SP_ANSI_CLASS_INST
template class SubstTable<Char>;
#else
typedef SubstTable<Char> Dummy_38;
#endif
#endif
#ifdef __DECCXX
#pragma define_template SharedXcharMap<PackedBoolean>
#else
#ifdef SP_ANSI_CLASS_INST
template class SharedXcharMap<PackedBoolean>;
#else
typedef SharedXcharMap<PackedBoolean> Dummy_39;
#endif
#endif
#ifdef __DECCXX
#pragma define_template SharedXcharMap<unsigned char>
#else
#ifdef SP_ANSI_CLASS_INST
template class SharedXcharMap<unsigned char>;
#else
typedef SharedXcharMap<unsigned char> Dummy_40;
#endif
#endif
#ifdef __DECCXX
#pragma define_template SharedXcharMap<EquivCode>
#else
#ifdef SP_ANSI_CLASS_INST
template class SharedXcharMap<EquivCode>;
#else
typedef SharedXcharMap<EquivCode> Dummy_41;
#endif
#endif
#ifdef __DECCXX
#pragma define_template String<EquivCode>
#else
#ifdef SP_ANSI_CLASS_INST
template class String<EquivCode>;
#else
typedef String<EquivCode> Dummy_42;
#endif
#endif
#ifdef __DECCXX
#pragma define_template String<SyntaxChar>
#else
#ifdef SP_ANSI_CLASS_INST
template class String<SyntaxChar>;
#else
typedef String<SyntaxChar> Dummy_43;
#endif
#endif
#ifdef __DECCXX
#pragma define_template XcharMap<PackedBoolean>
#else
#ifdef SP_ANSI_CLASS_INST
template class XcharMap<PackedBoolean>;
#else
typedef XcharMap<PackedBoolean> Dummy_44;
#endif
#endif
#ifdef __DECCXX
#pragma define_template XcharMap<unsigned char>
#else
#ifdef SP_ANSI_CLASS_INST
template class XcharMap<unsigned char>;
#else
typedef XcharMap<unsigned char> Dummy_45;
#endif
#endif
#ifdef __DECCXX
#pragma define_template XcharMap<EquivCode>
#else
#ifdef SP_ANSI_CLASS_INST
template class XcharMap<EquivCode>;
#else
typedef XcharMap<EquivCode> Dummy_46;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Vector<char>
#else
#ifdef SP_ANSI_CLASS_INST
template class Vector<char>;
#else
typedef Vector<char> Dummy_47;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Vector<PackedBoolean>
#else
#ifdef SP_ANSI_CLASS_INST
template class Vector<PackedBoolean>;
#else
typedef Vector<PackedBoolean> Dummy_48;
#endif
#endif
#ifdef __DECCXX
#pragma define_template SubstTable<Char>
#else
#ifdef SP_ANSI_CLASS_INST
template class SubstTable<Char>;
#else
typedef SubstTable<Char> Dummy_49;
#endif
#endif

#ifdef SP_NAMESPACE
}
#endif

#endif /* SP_MANUAL_INST */

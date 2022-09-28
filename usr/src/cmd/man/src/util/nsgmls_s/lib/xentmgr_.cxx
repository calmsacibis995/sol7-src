#pragma ident	"@(#)instmac.m4	1.2	97/04/24 SMI"
#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif


#ifdef SP_NAMESPACE
}
#endif
#pragma ident	"@(#)xentmgr_inst.m4	1.2	97/04/24 SMI"
// Copyright (c) 1994, 1995 James Clark
// See the file COPYING for copying permission.

#include "splib.h"

#ifdef SP_MANUAL_INST

#define SP_DEFINE_TEMPLATES
#include "StringOf.h"
#include "Vector.h"
#include "NCVector.h"
#include "ListIter.h"
#include "IList.h"
#include "List.h"
#include "Owner.h"
#include "OwnerTable.h"
#include "PointerTable.h"
#include "HashTableItemBase.h"
#include "HashTable.h"
#include "Ptr.h"
#undef SP_DEFINE_TEMPLATES

#include "StorageManager.h"
#include "ExtendEntityManager.h"
#include "OffsetOrderedList.h"
#include "CodingSystem.h"
#include "types.h"
#include "StringOf.h"
#include "DescriptorManager.h"
#include "StorageManager.h"
#include "Boolean.h"
#include "RegisteredCodingSystem.h"
#include "StorageObjectPosition.h"
#include "CatalogEntry.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

#ifdef __DECCXX
#pragma define_template String<char>
#else
#ifdef SP_ANSI_CLASS_INST
template class String<char>;
#else
typedef String<char> Dummy_0;
#endif
#endif
#ifdef __DECCXX
#pragma define_template NCVector<Owner<StorageObject> >
#else
#ifdef SP_ANSI_CLASS_INST
template class NCVector<Owner<StorageObject> >;
#else
typedef NCVector<Owner<StorageObject> > Dummy_1;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Vector<StorageObjectSpec>
#else
#ifdef SP_ANSI_CLASS_INST
template class Vector<StorageObjectSpec>;
#else
typedef Vector<StorageObjectSpec> Dummy_2;
#endif
#endif
#ifdef __DECCXX
#pragma define_template NCVector<Owner<OffsetOrderedListBlock> >
#else
#ifdef SP_ANSI_CLASS_INST
template class NCVector<Owner<OffsetOrderedListBlock> >;
#else
typedef NCVector<Owner<OffsetOrderedListBlock> > Dummy_3;
#endif
#endif
#ifdef __DECCXX
#pragma define_template NCVector<StorageObjectPosition>
#else
#ifdef SP_ANSI_CLASS_INST
template class NCVector<StorageObjectPosition>;
#else
typedef NCVector<StorageObjectPosition> Dummy_4;
#endif
#endif
#ifdef __DECCXX
#pragma define_template IList<ListItem<DescriptorUser*> >
#else
#ifdef SP_ANSI_CLASS_INST
template class IList<ListItem<DescriptorUser*> >;
#else
typedef IList<ListItem<DescriptorUser*> > Dummy_5;
#endif
#endif
#ifdef __DECCXX
#pragma define_template List<DescriptorUser*>
#else
#ifdef SP_ANSI_CLASS_INST
template class List<DescriptorUser*>;
#else
typedef List<DescriptorUser*> Dummy_6;
#endif
#endif
#ifdef __DECCXX
#pragma define_template ListIter<DescriptorUser *>
#else
#ifdef SP_ANSI_CLASS_INST
template class ListIter<DescriptorUser *>;
#else
typedef ListIter<DescriptorUser *> Dummy_7;
#endif
#endif
#ifdef __DECCXX
#pragma define_template ListItem<DescriptorUser *>
#else
#ifdef SP_ANSI_CLASS_INST
template class ListItem<DescriptorUser *>;
#else
typedef ListItem<DescriptorUser *> Dummy_8;
#endif
#endif
#ifdef __DECCXX
#pragma define_template IListIter<ListItem<DescriptorUser*> >
#else
#ifdef SP_ANSI_CLASS_INST
template class IListIter<ListItem<DescriptorUser*> >;
#else
typedef IListIter<ListItem<DescriptorUser*> > Dummy_9;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Owner<StorageObject>
#else
#ifdef SP_ANSI_CLASS_INST
template class Owner<StorageObject>;
#else
typedef Owner<StorageObject> Dummy_10;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Owner<Decoder>
#else
#ifdef SP_ANSI_CLASS_INST
template class Owner<Decoder>;
#else
typedef Owner<Decoder> Dummy_11;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Owner<OffsetOrderedListBlock>
#else
#ifdef SP_ANSI_CLASS_INST
template class Owner<OffsetOrderedListBlock>;
#else
typedef Owner<OffsetOrderedListBlock> Dummy_12;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Owner<ExtendEntityManager::CatalogManager>
#else
#ifdef SP_ANSI_CLASS_INST
template class Owner<ExtendEntityManager::CatalogManager>;
#else
typedef Owner<ExtendEntityManager::CatalogManager> Dummy_13;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Owner<StorageManager>
#else
#ifdef SP_ANSI_CLASS_INST
template class Owner<StorageManager>;
#else
typedef Owner<StorageManager> Dummy_14;
#endif
#endif
#ifdef __DECCXX
#pragma define_template NCVector<Owner<StorageManager> >
#else
#ifdef SP_ANSI_CLASS_INST
template class NCVector<Owner<StorageManager> >;
#else
typedef NCVector<Owner<StorageManager> > Dummy_15;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Vector<RegisteredCodingSystem>
#else
#ifdef SP_ANSI_CLASS_INST
template class Vector<RegisteredCodingSystem>;
#else
typedef Vector<RegisteredCodingSystem> Dummy_16;
#endif
#endif
#ifdef __DECCXX
#pragma define_template HashTable<String<Char>,CatalogEntry>
#else
#ifdef SP_ANSI_CLASS_INST
template class HashTable<String<Char>,CatalogEntry>;
#else
typedef HashTable<String<Char>,CatalogEntry> Dummy_17;
#endif
#endif
#ifdef __DECCXX
#pragma define_template HashTableIter<String<Char>,CatalogEntry>
#else
#ifdef SP_ANSI_CLASS_INST
template class HashTableIter<String<Char>,CatalogEntry>;
#else
typedef HashTableIter<String<Char>,CatalogEntry> Dummy_18;
#endif
#endif
#ifdef __DECCXX
#pragma define_template HashTableItem<String<Char>,CatalogEntry>
#else
#ifdef SP_ANSI_CLASS_INST
template class HashTableItem<String<Char>,CatalogEntry>;
#else
typedef HashTableItem<String<Char>,CatalogEntry> Dummy_19;
#endif
#endif
#ifdef __DECCXX
#pragma define_template HashTableItemBase<String<Char> >
#else
#ifdef SP_ANSI_CLASS_INST
template class HashTableItemBase<String<Char> >;
#else
typedef HashTableItemBase<String<Char> > Dummy_20;
#endif
#endif
#ifdef __DECCXX
#pragma define_template OwnerTable<HashTableItemBase<String<Char> >,String<Char>,Hash,HashTableKeyFunction<String<Char> > >
#else
#ifdef SP_ANSI_CLASS_INST
template class OwnerTable<HashTableItemBase<String<Char> >,String<Char>,Hash,HashTableKeyFunction<String<Char> > >;
#else
typedef OwnerTable<HashTableItemBase<String<Char> >,String<Char>,Hash,HashTableKeyFunction<String<Char> > > Dummy_21;
#endif
#endif
#ifdef __DECCXX
#pragma define_template CopyOwnerTable<HashTableItemBase<String<Char> >,String<Char>,Hash,HashTableKeyFunction<String<Char> > >
#else
#ifdef SP_ANSI_CLASS_INST
template class CopyOwnerTable<HashTableItemBase<String<Char> >,String<Char>,Hash,HashTableKeyFunction<String<Char> > >;
#else
typedef CopyOwnerTable<HashTableItemBase<String<Char> >,String<Char>,Hash,HashTableKeyFunction<String<Char> > > Dummy_22;
#endif
#endif
#ifdef __DECCXX
#pragma define_template OwnerTableIter<HashTableItemBase<String<Char> >, String<Char>, Hash, HashTableKeyFunction<String<Char> > >
#else
#ifdef SP_ANSI_CLASS_INST
template class OwnerTableIter<HashTableItemBase<String<Char> >, String<Char>, Hash, HashTableKeyFunction<String<Char> > >;
#else
typedef OwnerTableIter<HashTableItemBase<String<Char> >, String<Char>, Hash, HashTableKeyFunction<String<Char> > > Dummy_23;
#endif
#endif
#ifdef __DECCXX
#pragma define_template PointerTable<HashTableItemBase<String<Char> >*,String<Char>,Hash,HashTableKeyFunction<String<Char> > >
#else
#ifdef SP_ANSI_CLASS_INST
template class PointerTable<HashTableItemBase<String<Char> >*,String<Char>,Hash,HashTableKeyFunction<String<Char> > >;
#else
typedef PointerTable<HashTableItemBase<String<Char> >*,String<Char>,Hash,HashTableKeyFunction<String<Char> > > Dummy_24;
#endif
#endif
#ifdef __DECCXX
#pragma define_template PointerTableIter<HashTableItemBase<String<Char> > *, String<Char>, Hash, HashTableKeyFunction<String<Char> > >
#else
#ifdef SP_ANSI_CLASS_INST
template class PointerTableIter<HashTableItemBase<String<Char> > *, String<Char>, Hash, HashTableKeyFunction<String<Char> > >;
#else
typedef PointerTableIter<HashTableItemBase<String<Char> > *, String<Char>, Hash, HashTableKeyFunction<String<Char> > > Dummy_25;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Vector<HashTableItemBase<String<Char> >*>
#else
#ifdef SP_ANSI_CLASS_INST
template class Vector<HashTableItemBase<String<Char> >*>;
#else
typedef Vector<HashTableItemBase<String<Char> >*> Dummy_26;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Ptr<ExtendEntityManager>
#else
#ifdef SP_ANSI_CLASS_INST
template class Ptr<ExtendEntityManager>;
#else
typedef Ptr<ExtendEntityManager> Dummy_27;
#endif
#endif
#ifdef __DECCXX
#pragma define_template ConstPtr<ExtendEntityManager>
#else
#ifdef SP_ANSI_CLASS_INST
template class ConstPtr<ExtendEntityManager>;
#else
typedef ConstPtr<ExtendEntityManager> Dummy_28;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Vector<ParsedSystemId::Map>
#else
#ifdef SP_ANSI_CLASS_INST
template class Vector<ParsedSystemId::Map>;
#else
typedef Vector<ParsedSystemId::Map> Dummy_29;
#endif
#endif
#ifdef SP_NAMESPACE
}
#endif

#endif /* SP_MANUAL_INST */

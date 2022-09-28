#pragma ident	"@(#)ExtendEntityManager.h	1.2	97/04/24 SMI"
// Copyright (c) 1994, 1995 James Clark
// See the file COPYING for copying permission.

#ifndef ExtendEntityManager_INCLUDED
#define ExtendEntityManager_INCLUDED 1

#ifdef __GNUG__
#pragma interface
#endif

#include "EntityManager.h"
#include "CharsetInfo.h"
#include "types.h"
#include "Boolean.h"
#include "StringC.h"
#include "types.h"
#include "Vector.h"
#include "Location.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

class StorageManager;
class InputCodingSystem;
class Messenger;

struct SP_API StorageObjectSpec {
  StorageObjectSpec();
  StorageManager *storageManager;
  const char *codingSystemName;
  const InputCodingSystem *codingSystem;
  StringC specId;		// specified id
  StringC baseId;		// id that specified id is relative to
  StringC id;			// actual id used (filled in after opening)
  enum Records {
    find,
    cr,
    lf,
    crlf,
    asis
    };
  Records records;
  PackedBoolean notrack;
  PackedBoolean zapEof;		// zap a final Ctrl-Z
  PackedBoolean search;
};


struct SP_API ParsedSystemId : public Vector<StorageObjectSpec> {
  ParsedSystemId();
  void unparse(const CharsetInfo &charset, StringC &result) const;
  struct SP_API Map {
    enum Type {
      catalogDocument,
      catalogPublic
    };
    Type type;
    StringC publicId;
  };
  Vector<Map> maps;
};

struct SP_API StorageObjectLocation {
  const StorageObjectSpec *storageObjectSpec;
  unsigned long lineNumber;
  unsigned long columnNumber;
  unsigned long byteIndex;
  unsigned long storageObjectOffset;
};

class SP_API ExtendEntityManager : public EntityManager {
public:
  class SP_API CatalogManager {
  public:
    virtual ~CatalogManager();
    virtual ConstPtr<EntityCatalog>
      makeCatalog(StringC &systemId,
		  const CharsetInfo &charset,
		  ExtendEntityManager *,
		  Messenger &) const = 0;
    virtual Boolean mapCatalog(ParsedSystemId &systemId,
			       ExtendEntityManager *em,
			       Messenger &mgr) const = 0;
  };
  virtual void registerStorageManager(StorageManager *) = 0;
  virtual void registerCodingSystem(const char *, const InputCodingSystem *)
    = 0;
  virtual void setCatalogManager(CatalogManager *) = 0;
  virtual InputSource *openIfExists(const StringC &sysid,
				    const CharsetInfo &,
				    InputSourceOrigin *,
				    Boolean mayRewind,
				    Messenger &) = 0;
  virtual Boolean expandSystemId(const StringC &,
				 const Location &,
				 Boolean isNdata,
				 const CharsetInfo &,
				 const StringC *mapCatalogPublic,
				 Messenger &,
				 StringC &) = 0;
  virtual Boolean mergeSystemIds(const Vector<StringC> &sysids,
				 Boolean mapCatalogDocument,
				 const CharsetInfo &,
				 Messenger &mgr,
				 StringC &) const = 0;
  virtual Boolean parseSystemId(const StringC &str,
				const CharsetInfo &idCharset,
				Boolean isNdata,
				const StorageObjectSpec *defSpec,
				Messenger &mgr,
				ParsedSystemId &parsedSysid) const = 0;
  static Boolean externalize(const ExternalInfo *,
			     Offset,
			     StorageObjectLocation &);
  static const ParsedSystemId *
    externalInfoParsedSystemId(const ExternalInfo *);
  static ExtendEntityManager *make(StorageManager *,
				   const InputCodingSystem *);
};

#ifdef SP_NAMESPACE
}
#endif

#endif /* not ExtendEntityManager_INCLUDED */

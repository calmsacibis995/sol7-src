#
#pragma ident	"@(#)Makefile.wat	1.2	97/04/24 SMI"
#
# Copyright (c) 1994, 1995 James Clark
# See the file COPYING for copying permission.

# Makefile for Watcom C/C++ 10.5a
CXX=wcl386
srcdir=..
TARGET=dos
LINK_SYSTEM=dos4g
STACK_SIZE=128k
OPTIMIZE=/otx /zp4 /4
SEARCHPATH=$(srcdir)\include;$(srcdir)\lib;$(srcdir)\generic;$(srcdir)\nsgmls;$(srcdir)\spam;$(srcdir)\sgmlnorm;$(srcdir)\spent
CXXFLAGS=/bt=$(TARGET) /zq $(OPTIMIZE) /i=$(SEARCHPATH)
LIBFLAGS=-p=32
LDFLAGS=/l=$(LINK_SYSTEM) /k$(STACK_SIZE)

NSGMLS_OBJS=nsgmls.obj SgmlsEventHandler.obj RastEventHandler.obj &
  StringSet.obj nsgmls_inst.obj
SPAM_OBJS=spam.obj CopyEventHandler.obj MarkupEventHandler.obj spam_inst.obj
SGMLNORM_OBJS=sgmlnorm.obj SGMLGenerator.obj
SPENT_OBJS=spent.obj spent_inst.obj
LIB_OBJS=Allocator.obj &
  ArcEngine.obj &
  Attribute.obj &
  CharsetDecl.obj &
  CharsetInfo.obj &
  CharsetRegistry.obj &
  CmdLineApp.obj &
  CodingSystem.obj &
  ConsoleOutput.obj &
  ContentState.obj &
  ContentToken.obj &
  DescriptorManager.obj &
  Dtd.obj &
  EUCJPCodingSystem.obj &
  ElementType.obj &
  Entity.obj &
  EntityApp.obj &
  EntityCatalog.obj &
  EntityDecl.obj &
  EntityManager.obj &
  ErrnoMessageArg.obj &
  ErrorCountEventHandler.obj &
  Event.obj &
  EventGenerator.obj &
  ExtendEntityManager.obj &
  ExternalId.obj &
  Fixed2CodingSystem.obj &
  GenericEventHandler.obj &
  Group.obj &
  Hash.obj &
  IListBase.obj &
  ISO8859InputCodingSystem.obj &
  Id.obj &
  IdentityCodingSystem.obj &
  InputSource.obj &
  InternalInputSource.obj &
  Link.obj &
  LinkProcess.obj &
  LiteralStorage.obj &
  Location.obj &
  Lpd.obj &
  Markup.obj &
  Message.obj &
  MessageArg.obj &
  MessageEventHandler.obj &
  MessageReporter.obj &
  MessageTable.obj &
  ModeInfo.obj &
  Notation.obj &
  NumericCharRefOrigin.obj &
  OffsetOrderedList.obj &
  OpenElement.obj &
  OutputCharStream.obj &
  OutputState.obj &
  Param.obj &
  Parser.obj &
  ParserApp.obj &
  ParserEventGeneratorKit.obj &
  ParserMessages.obj &
  ParserOptions.obj &
  ParserState.obj &
  Partition.obj &
  PosixStorage.obj &
  Recognizer.obj &
  RewindStorageObject.obj &
  SJISCodingSystem.obj &
  SOEntityCatalog.obj &
  Sd.obj &
  SdText.obj &
  SearchResultMessageArg.obj &
  SgmlParser.obj &
  SGMLApplication.obj &
  ShortReferenceMap.obj &
  StdioStorage.obj &
  StorageManager.obj &
  StringVectorMessageArg.obj &
  Syntax.obj &
  Text.obj &
  TokenMessageArg.obj &
  TranslateInputCodingSystem.obj &
  TrieBuilder.obj &
  TypeId.obj &
  URLStorage.obj &
  UTF8CodingSystem.obj &
  Undo.obj &
  UnicodeCodingSystem.obj &
  UnivCharsetDesc.obj &
  Win32CodingSystem.obj &
  app_inst.obj &
  arc_inst.obj &
  assert.obj &
  entmgr_inst.obj &
  parseAttribute.obj &
  parseCommon.obj &
  parseDecl.obj &
  parseInstance.obj &
  parseMode.obj &
  parseParam.obj &
  parseSd.obj &
  parser_inst.obj &
  xentmgr_inst.obj &

LIB=libsp.lib

all: nsgmls.exe spam.exe sgmlnorm.exe spent.exe

nsgmls.exe: $(NSGMLS_OBJS) $(LIB)
	$(CXX) $(NSGMLS_OBJS) $(LIB) /fe=$@ $(CXXFLAGS) $(LDFLAGS)

spam.exe: $(SPAM_OBJS) $(LIB)
	$(CXX) $(SPAM_OBJS) $(LIB) /fe=$@ $(CXXFLAGS) $(LDFLAGS)

sgmlnorm.exe: $(SGMLNORM_OBJS) $(LIB)
	$(CXX) $(SGMLNORM_OBJS) $(LIB) /fe=$@ $(CXXFLAGS) $(LDFLAGS)

spent.exe: $(SPENT_OBJS) $(LIB)
	$(CXX) $(SPENT_OBJS) $(LIB) /fe=$@ $(CXXFLAGS) $(LDFLAGS)

$(LIB): $(LIB_OBJS)
	wlib $(LIBFLAGS) $(LIB) $?

.cxx.obj: .AUTODEPEND
	$(CXX) $[@ /c $(CXXFLAGS)

.cxx: $(SEARCHPATH)

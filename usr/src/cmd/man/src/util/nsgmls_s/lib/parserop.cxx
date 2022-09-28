#pragma ident	"@(#)ParserOptions.cxx	1.2	97/04/24 SMI"
// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.

#ifdef __GNUG__
#pragma implementation
#endif
#include "splib.h"
#include "ParserOptions.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

ParserOptions::ParserOptions()
: datatag(0),
  omittag(1),
  rank(1),
  shorttag(1),
  linkSimple(1000),
  linkImplicit(1),
  linkExplicit(1),
  concur(0),
  subdoc(99999999),
  formal(1),
  shortref(1),
  errorIdref(1),
  errorSignificant(1),
  errorAfdr(1),
  errorLpdNotation(0),
  warnSgmlDecl(0),
  warnShould(0),
  warnDuplicateEntity(0),
  warnUndefinedElement(0),
  warnDefaultEntityReference(0),
  warnMixedContent(0),
  warnUnclosedTag(0),
  warnNet(0),
  warnEmptyTag(0),
  warnUnusedMap(0),
  warnUnusedParam(0),
  warnNotationSystemId(0)
{
  for (int i = 0; i < nQuantity; i++)
    quantity[i] = 99999999;
  quantity[BSEQLEN] = 960;
  quantity[NORMSEP] = 2;
  quantity[LITLEN] = 24000;
  quantity[PILEN] = 24000;
  quantity[DTEMPLEN] = 24000;
}

#ifdef SP_NAMESPACE
}
#endif

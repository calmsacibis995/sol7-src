#pragma ident	"@(#)CharsetRegistry.cxx	1.2	97/04/24 SMI"
// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.

#ifdef __GNUG__
#pragma implementation
#endif
#include "splib.h"
#include "CharsetRegistry.h"
#include "ExternalId.h"
#include "CharsetInfo.h"
#include "UnivCharsetDesc.h"
#include "StringC.h"
#include "types.h"
#include "macros.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

static UnivCharsetDesc::Range iso646_irv[] = {
  { 0, 128, 0 }
};

static UnivCharsetDesc::Range iso646_C0[] = {
  { 0, 32, 0 },
  { 127, 1, 127 },
};

static struct {
  const char *sequence;
  const UnivCharsetDesc::Range *ranges;
  size_t nRanges;
} table[] = {
  { "ESC 2/5 4/0", iso646_irv, SIZEOF(iso646_irv) },
  { "ESC 2/8 4/0", iso646_irv, SIZEOF(iso646_irv) },
  { "ESC 2/8 4/2", iso646_irv, SIZEOF(iso646_irv) }, // ASCII
  { "ESC 2/1 4/0", iso646_C0, SIZEOF(iso646_C0) },
};

Boolean CharsetRegistry::findCharset(const PublicId &id,
				     const CharsetInfo &charset,
				     UnivCharsetDesc &desc)
{
  PublicId::OwnerType ownerType;
  if (!id.getOwnerType(ownerType) || ownerType != PublicId::ISO)
    return 0;
  StringC sequence;
  if (!id.getDesignatingSequence(sequence))
    return 0;
  // Canonicalize the escape sequence by mapping esc -> ESC,
  // removing leading zeros from escape sequences, and removing
  // initial spaces.
  StringC s;
  size_t i;
  for (i = 0; i < sequence.size(); i++) {
    Char c = sequence[i];
    if (c == charset.execToDesc('e'))
      s += charset.execToDesc('E');
    else if (c == charset.execToDesc('s'))
      s += charset.execToDesc('S');
    else if (c == charset.execToDesc('c'))
      s += charset.execToDesc('C');
    else if (charset.digitWeight(c) >= 0
	     && s.size() > 0
	     && s[s.size() - 1] == charset.execToDesc('0')
	     && (s.size() == 1
		 || charset.digitWeight(s[s.size() - 2]) >= 0))
      s[s.size() - 1] = c;
    else if (c != charset.execToDesc(' ') || s.size() > 0)
      s += c;
  }
  for (i = 0; i < SIZEOF(table); i++)
    if (s == charset.execToDesc(table[i].sequence)) {
      desc.set(table[i].ranges, table[i].nRanges);
      return 1;
    }
  return 0;
}

#ifdef SP_NAMESPACE
}
#endif

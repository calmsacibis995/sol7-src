#pragma ident	"@(#)CodingSystem.h	1.2	97/04/24 SMI"
// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.

#ifndef CodingSystem_INCLUDED
#define CodingSystem_INCLUDED 1

#ifdef __GNUG__
#pragma interface
#endif

#include "types.h"
#include "Boolean.h"
#include "StringC.h"

#include <stddef.h>

class streambuf;

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

class SP_API Decoder {
public:
  Decoder(unsigned minBytesPerChar = 1);
  virtual ~Decoder();
  virtual size_t decode(Char *, const char *, size_t, const char **) = 0;
  virtual Boolean convertOffset(unsigned long &offset) const;
  // Decoder assumes that for every decoded Char there must be at least
  // minBytesPerChar bytes
  unsigned minBytesPerChar() const;
protected:
  unsigned minBytesPerChar_;
};


class SP_API Encoder {
public:
  class SP_API Handler {
  public:
    virtual void handleUnencodable(Char, streambuf *) = 0;
  };
  Encoder();
  virtual ~Encoder();
  virtual void output(const Char *, size_t, streambuf *) = 0;
  // This outputs a byte order mark with Unicode.
  virtual void startFile(streambuf *);
  virtual void output(Char *, size_t, streambuf *);
  void setUnencodableHandler(Handler *);
protected:
  void handleUnencodable(Char, streambuf *);
private:
  Handler *unencodableHandler_;
};

class SP_API InputCodingSystem {
public:
  virtual ~InputCodingSystem();
  virtual Decoder *makeDecoder() const = 0;
  StringC convertIn(const char *) const;
  virtual Boolean isIdentity() const;
};

class SP_API OutputCodingSystem {
public:
  virtual ~OutputCodingSystem();
  virtual Encoder *makeEncoder() const = 0;
  virtual unsigned fixedBytesPerChar() const;
  String<char> convertOut(const StringC &) const;
};

class SP_API CodingSystem : public InputCodingSystem, public OutputCodingSystem {
};

inline
unsigned Decoder::minBytesPerChar() const
{
  return minBytesPerChar_;
}

inline
void Encoder::handleUnencodable(Char c, streambuf *sbufp)
{
  if (unencodableHandler_)
    unencodableHandler_->handleUnencodable(c, sbufp);
}

inline
void Encoder::setUnencodableHandler(Handler *handler)
{
  unencodableHandler_ = handler;
}

#ifdef SP_NAMESPACE
}
#endif

#endif /* not CodingSystem_INCLUDED */

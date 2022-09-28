#pragma ident	"@(#)IdentityCodingSystem.cxx	1.2	97/04/24 SMI"
// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.

#include "splib.h"
#include "IdentityCodingSystem.h"
#include <iostream.h>
#include <limits.h>

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

class IdentityDecoder : public Decoder {
public:
  size_t decode(Char *to, const char *from, size_t fromLen,
		const char **rest);
  Boolean convertOffset(unsigned long &offset) const;
};

class IdentityEncoder : public Encoder {
public:
  IdentityEncoder();
  ~IdentityEncoder();
  void output(Char *, size_t, streambuf *);
  void output(const Char *, size_t, streambuf *);
private:
  void allocBuf(size_t);
  char *buf_;
  size_t bufSize_;
};

IdentityCodingSystem::IdentityCodingSystem()
{
}

Decoder *IdentityCodingSystem::makeDecoder() const
{
  return new IdentityDecoder;
}

Encoder *IdentityCodingSystem::makeEncoder() const
{
  return new IdentityEncoder;
}

Boolean IdentityCodingSystem::isIdentity() const
{
  return 1;
}

size_t IdentityDecoder::decode(Char *to, const char *from, size_t fromLen,
			       const char **rest)
{
  if (sizeof(Char) == sizeof(char) && from == (char *)to) {
    *rest = from + fromLen;
    return fromLen;
  }
  for (size_t n = fromLen; n > 0; n--)
    *to++ = (unsigned char)*from++; // zero extend
  *rest = from;
  return fromLen;
}

Boolean IdentityDecoder::convertOffset(unsigned long &) const
{
  return true;
}

IdentityEncoder::IdentityEncoder()
: buf_(0), bufSize_(0)
{
}

IdentityEncoder::~IdentityEncoder()
{
  delete [] buf_;
}

void IdentityEncoder::allocBuf(size_t n)
{
  if (bufSize_ < n) {
    delete [] buf_;
    buf_ = new char[bufSize_ = n];
  }
}

// FIXME handle errors from streambuf::sputn

void IdentityEncoder::output(Char *s, size_t n, streambuf *sb)
{
  char *p = (char *)s;
  if (sizeof(Char) != sizeof(char)) {
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
      if (s[i] > UCHAR_MAX) {
	sb->sputn(p, j);
	j = 0;
	handleUnencodable(s[i], sb);
      }
      else
	p[j++] = char(s[i]);
    }
    sb->sputn(p, j);
  }
  else
    sb->sputn(p, n);
}

void IdentityEncoder::output(const Char *s, size_t n, streambuf *sb)
{
  if (sizeof(Char) != sizeof(char)) {
    allocBuf(n);
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
      if (s[i] > UCHAR_MAX) {
	sb->sputn(buf_, j);
	j = 0;
	handleUnencodable(s[i], sb);
      }
      else
	buf_[j++] = char(s[i]);
    }
    sb->sputn(buf_, j);
  }
  else
    sb->sputn((const char *)s, n);
}

#ifdef SP_NAMESPACE
}
#endif


//token from muduo lib
#include <cnet/utils/LogStream.h>
#include <algorithm>
#include <limits>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <iostream>

using namespace cnet;
using namespace cnet::detail;

namespace cnet
{
namespace detail
{

const char digits[] = "9876543210123456789";
const char *zero = digits + 9;

const char digitsHex[] = "0123456789ABCDEF";

// Efficient Integer to String Conversions, by Matthew Wilson.
template <typename T>
size_t convert(char buf[], T value)
{
  T i = value;
  char *p = buf;

  do
  {
    int lsd = static_cast<int>(i % 10);
    i /= 10;
    *p++ = zero[lsd];
  } while (i != 0);

  if (value < 0)
  {
    *p++ = '-';
  }
  *p = '\0';
  std::reverse(buf, p);

  return p - buf;
}

size_t convertHex(char buf[], uintptr_t value)
{
  uintptr_t i = value;
  char *p = buf;

  do
  {
    int lsd = static_cast<int>(i % 16);
    i /= 16;
    *p++ = digitsHex[lsd];
  } while (i != 0);

  *p = '\0';
  std::reverse(buf, p);

  return p - buf;
}

template class FixedBuffer<kSmallBuffer>;
template class FixedBuffer<kLargeBuffer>;

} // namespace detail
} // namespace cnet

template <int SIZE>
const char *FixedBuffer<SIZE>::debugString()
{
  *cur_ = '\0';
  return data_;
}

template <int SIZE>
void FixedBuffer<SIZE>::cookieStart()
{
}

template <int SIZE>
void FixedBuffer<SIZE>::cookieEnd()
{
}

void LogStream::staticCheck()
{
  static_assert(kMaxNumericSize - 10 > std::numeric_limits<double>::digits10, "");
  static_assert(kMaxNumericSize - 10 > std::numeric_limits<long double>::digits10, "");
  static_assert(kMaxNumericSize - 10 > std::numeric_limits<long>::digits10, "");
  static_assert(kMaxNumericSize - 10 > std::numeric_limits<long long>::digits10, "");
}

template <typename T>
void LogStream::formatInteger(T v)
{
  if (_exBuffer.empty())
  {
    if (buffer_.avail() >= kMaxNumericSize)
    {
      size_t len = convert(buffer_.current(), v);
      buffer_.add(len);
      return;
    }
    else
    {
      _exBuffer.append(buffer_.data(), buffer_.length());
    }
  }
  auto oldLen = _exBuffer.length();
  _exBuffer.resize(oldLen + kMaxNumericSize);
  size_t len = convert(&_exBuffer[oldLen], v);
  _exBuffer.resize(oldLen + len);
}

LogStream &LogStream::operator<<(short v)
{
  *this << static_cast<int>(v);
  return *this;
}

LogStream &LogStream::operator<<(unsigned short v)
{
  *this << static_cast<unsigned int>(v);
  return *this;
}

LogStream &LogStream::operator<<(int v)
{
  formatInteger(v);
  return *this;
}

LogStream &LogStream::operator<<(unsigned int v)
{
  formatInteger(v);
  return *this;
}

LogStream &LogStream::operator<<(long v)
{
  formatInteger(v);
  return *this;
}

LogStream &LogStream::operator<<(unsigned long v)
{
  formatInteger(v);
  return *this;
}

LogStream &LogStream::operator<<(long long v)
{
  formatInteger(v);
  return *this;
}

LogStream &LogStream::operator<<(unsigned long long v)
{
  formatInteger(v);
  return *this;
}

LogStream &LogStream::operator<<(const void *p)
{
  uintptr_t v = reinterpret_cast<uintptr_t>(p);
  if (_exBuffer.empty())
  {
    if (buffer_.avail() >= kMaxNumericSize)
    {
      char *buf = buffer_.current();
      buf[0] = '0';
      buf[1] = 'x';
      size_t len = convertHex(buf + 2, v);
      buffer_.add(len + 2);
      return *this;
    }
    else
    {
      _exBuffer.append(buffer_.data(), buffer_.length());
    }
  }
  auto oldLen = _exBuffer.length();
  _exBuffer.resize(oldLen + kMaxNumericSize);
  char *buf = &_exBuffer[oldLen];
  buf[0] = '0';
  buf[1] = 'x';
  size_t len = convertHex(buf + 2, v);
  _exBuffer.resize(oldLen + len + 2);
  return *this;
}

// FIXME: replace this with Grisu3 by Florian Loitsch.
LogStream &LogStream::operator<<(double v)
{
  if (_exBuffer.empty())
  {
    if (buffer_.avail() >= kMaxNumericSize)
    {
      int len = snprintf(buffer_.current(), kMaxNumericSize, "%.12g", v);
      buffer_.add(len);
      return *this;
    }
    else
    {
      _exBuffer.append(buffer_.data(), buffer_.length());
    }
  }
  auto oldLen = _exBuffer.length();
  _exBuffer.resize(oldLen + kMaxNumericSize);
  int len = snprintf(&(_exBuffer[oldLen]), kMaxNumericSize, "%.12g", v);
  _exBuffer.resize(oldLen + len);
  return *this;
}

template <typename T>
Fmt::Fmt(const char *fmt, T val)
{
  length_ = snprintf(buf_, sizeof buf_, fmt, val);
  assert(static_cast<size_t>(length_) < sizeof buf_);
}

// Explicit instantiations

template Fmt::Fmt(const char *fmt, char);

template Fmt::Fmt(const char *fmt, short);
template Fmt::Fmt(const char *fmt, unsigned short);
template Fmt::Fmt(const char *fmt, int);
template Fmt::Fmt(const char *fmt, unsigned int);
template Fmt::Fmt(const char *fmt, long);
template Fmt::Fmt(const char *fmt, unsigned long);
template Fmt::Fmt(const char *fmt, long long);
template Fmt::Fmt(const char *fmt, unsigned long long);

template Fmt::Fmt(const char *fmt, float);
template Fmt::Fmt(const char *fmt, double);

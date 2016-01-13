// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_UTILS_H_
#define IPC_IPC_MESSAGE_UTILS_H_

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/containers/small_map.h"
#include "base/files/file.h"
#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/tuple.h"
#include "ipc/ipc_message_start.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_sync_message.h"

#if defined(COMPILER_GCC)
// GCC "helpfully" tries to inline template methods in release mode. Except we
// want the majority of the template junk being expanded once in the
// implementation file (and only provide the definitions in
// ipc_message_utils_impl.h in those files) and exported, instead of expanded
// at every call site. Special note: GCC happily accepts the attribute before
// the method declaration, but only acts on it if it is after.
#if (__GNUC__ * 10000 + __GNUC_MINOR__ * 100) >= 40500
// Starting in gcc 4.5, the noinline no longer implies the concept covered by
// the introduced noclone attribute, which will create specialized versions of
// functions/methods when certain types are constant.
// www.gnu.org/software/gcc/gcc-4.5/changes.html
#define IPC_MSG_NOINLINE  __attribute__((noinline, noclone));
#else
#define IPC_MSG_NOINLINE  __attribute__((noinline));
#endif
#elif defined(COMPILER_MSVC)
// MSVC++ doesn't do this.
#define IPC_MSG_NOINLINE
#else
#error "Please add the noinline property for your new compiler here."
#endif

namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
class NullableString16;
class Time;
class TimeDelta;
class TimeTicks;
struct FileDescriptor;
}

namespace IPC {

struct ChannelHandle;

// -----------------------------------------------------------------------------
// How we send IPC message logs across channels.
struct IPC_EXPORT LogData {
  LogData();
  ~LogData();

  std::string channel;
  int32 routing_id;
  uint32 type;  // "User-defined" message type, from ipc_message.h.
  std::string flags;
  int64 sent;  // Time that the message was sent (i.e. at Send()).
  int64 receive;  // Time before it was dispatched (i.e. before calling
                  // OnMessageReceived).
  int64 dispatch;  // Time after it was dispatched (i.e. after calling
                   // OnMessageReceived).
  std::string message_name;
  std::string params;
};

//-----------------------------------------------------------------------------

// A dummy struct to place first just to allow leading commas for all
// members in the macro-generated constructor initializer lists.
struct NoParams {
};

template <class P>
static inline void WriteParam(Message* m, const P& p) {
  typedef typename SimilarTypeTraits<P>::Type Type;
  ParamTraits<Type>::Write(m, static_cast<const Type& >(p));
}

template <class P>
static inline bool WARN_UNUSED_RESULT ReadParam(const Message* m,
                                                PickleIterator* iter,
                                                P* p) {
  typedef typename SimilarTypeTraits<P>::Type Type;
  return ParamTraits<Type>::Read(m, iter, reinterpret_cast<Type* >(p));
}

template <class P>
static inline void LogParam(const P& p, std::string* l) {
  typedef typename SimilarTypeTraits<P>::Type Type;
  ParamTraits<Type>::Log(static_cast<const Type& >(p), l);
}

// Primitive ParamTraits -------------------------------------------------------

template <>
struct ParamTraits<bool> {
  typedef bool param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteBool(p);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadBool(iter, r);
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<unsigned char> {
  typedef unsigned char param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<unsigned short> {
  typedef unsigned short param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<int> {
  typedef int param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadInt(iter, r);
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<unsigned int> {
  typedef unsigned int param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt(p);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadInt(iter, reinterpret_cast<int*>(r));
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<long> {
  typedef long param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteLongUsingDangerousNonPortableLessPersistableForm(p);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadLong(iter, r);
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<unsigned long> {
  typedef unsigned long param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteLongUsingDangerousNonPortableLessPersistableForm(p);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadLong(iter, reinterpret_cast<long*>(r));
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<long long> {
  typedef long long param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt64(static_cast<int64>(p));
  }
  static bool Read(const Message* m, PickleIterator* iter,
                   param_type* r) {
    return m->ReadInt64(iter, reinterpret_cast<int64*>(r));
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<unsigned long long> {
  typedef unsigned long long param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteInt64(p);
  }
  static bool Read(const Message* m, PickleIterator* iter,
                   param_type* r) {
    return m->ReadInt64(iter, reinterpret_cast<int64*>(r));
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

// Note that the IPC layer doesn't sanitize NaNs and +/- INF values.  Clients
// should be sure to check the sanity of these values after receiving them over
// IPC.
template <>
struct IPC_EXPORT ParamTraits<float> {
  typedef float param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteFloat(p);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return m->ReadFloat(iter, r);
  }
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<double> {
  typedef double param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

// STL ParamTraits -------------------------------------------------------------

template <>
struct ParamTraits<std::string> {
  typedef std::string param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteString(p);
  }
  static bool Read(const Message* m, PickleIterator* iter,
                   param_type* r) {
    return m->ReadString(iter, r);
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<std::wstring> {
  typedef std::wstring param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteWString(p);
  }
  static bool Read(const Message* m, PickleIterator* iter,
                   param_type* r) {
    return m->ReadWString(iter, r);
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};

// If WCHAR_T_IS_UTF16 is defined, then string16 is a std::wstring so we don't
// need this trait.
#if !defined(WCHAR_T_IS_UTF16)
template <>
struct ParamTraits<base::string16> {
  typedef base::string16 param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteString16(p);
  }
  static bool Read(const Message* m, PickleIterator* iter,
                   param_type* r) {
    return m->ReadString16(iter, r);
  }
  IPC_EXPORT static void Log(const param_type& p, std::string* l);
};
#endif

template <>
struct IPC_EXPORT ParamTraits<std::vector<char> > {
  typedef std::vector<char> param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message*, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<std::vector<unsigned char> > {
  typedef std::vector<unsigned char> param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<std::vector<bool> > {
  typedef std::vector<bool> param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <class P>
struct ParamTraits<std::vector<P> > {
  typedef std::vector<P> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.size()));
    for (size_t i = 0; i < p.size(); i++)
      WriteParam(m, p[i]);
  }
  static bool Read(const Message* m, PickleIterator* iter,
                   param_type* r) {
    int size;
    // ReadLength() checks for < 0 itself.
    if (!m->ReadLength(iter, &size))
      return false;
    // Resizing beforehand is not safe, see BUG 1006367 for details.
    if (INT_MAX / sizeof(P) <= static_cast<size_t>(size))
      return false;
    r->resize(size);
    for (int i = 0; i < size; i++) {
      if (!ReadParam(m, iter, &(*r)[i]))
        return false;
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    for (size_t i = 0; i < p.size(); ++i) {
      if (i != 0)
        l->append(" ");
      LogParam((p[i]), l);
    }
  }
};

template <class P>
struct ParamTraits<std::set<P> > {
  typedef std::set<P> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.size()));
    typename param_type::const_iterator iter;
    for (iter = p.begin(); iter != p.end(); ++iter)
      WriteParam(m, *iter);
  }
  static bool Read(const Message* m, PickleIterator* iter,
                   param_type* r) {
    int size;
    if (!m->ReadLength(iter, &size))
      return false;
    for (int i = 0; i < size; ++i) {
      P item;
      if (!ReadParam(m, iter, &item))
        return false;
      r->insert(item);
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<std::set>");
  }
};

template <class K, class V>
struct ParamTraits<std::map<K, V> > {
  typedef std::map<K, V> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.size()));
    typename param_type::const_iterator iter;
    for (iter = p.begin(); iter != p.end(); ++iter) {
      WriteParam(m, iter->first);
      WriteParam(m, iter->second);
    }
  }
  static bool Read(const Message* m, PickleIterator* iter,
                   param_type* r) {
    int size;
    if (!ReadParam(m, iter, &size) || size < 0)
      return false;
    for (int i = 0; i < size; ++i) {
      K k;
      if (!ReadParam(m, iter, &k))
        return false;
      V& value = (*r)[k];
      if (!ReadParam(m, iter, &value))
        return false;
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<std::map>");
  }
};

template <class A, class B>
struct ParamTraits<std::pair<A, B> > {
  typedef std::pair<A, B> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.first);
    WriteParam(m, p.second);
  }
  static bool Read(const Message* m, PickleIterator* iter,
                   param_type* r) {
    return ReadParam(m, iter, &r->first) && ReadParam(m, iter, &r->second);
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.first, l);
    l->append(", ");
    LogParam(p.second, l);
    l->append(")");
  }
};

// Base ParamTraits ------------------------------------------------------------

template <>
struct IPC_EXPORT ParamTraits<base::DictionaryValue> {
  typedef base::DictionaryValue param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

#if defined(OS_POSIX)
// FileDescriptors may be serialised over IPC channels on POSIX. On the
// receiving side, the FileDescriptor is a valid duplicate of the file
// descriptor which was transmitted: *it is not just a copy of the integer like
// HANDLEs on Windows*. The only exception is if the file descriptor is < 0. In
// this case, the receiving end will see a value of -1. *Zero is a valid file
// descriptor*.
//
// The received file descriptor will have the |auto_close| flag set to true. The
// code which handles the message is responsible for taking ownership of it.
// File descriptors are OS resources and must be closed when no longer needed.
//
// When sending a file descriptor, the file descriptor must be valid at the time
// of transmission. Since transmission is not synchronous, one should consider
// dup()ing any file descriptors to be transmitted and setting the |auto_close|
// flag, which causes the file descriptor to be closed after writing.
template<>
struct IPC_EXPORT ParamTraits<base::FileDescriptor> {
  typedef base::FileDescriptor param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};
#endif  // defined(OS_POSIX)

template <>
struct IPC_EXPORT ParamTraits<base::FilePath> {
  typedef base::FilePath param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<base::ListValue> {
  typedef base::ListValue param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<base::NullableString16> {
  typedef base::NullableString16 param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<base::File::Info> {
  typedef base::File::Info param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct SimilarTypeTraits<base::File::Error> {
  typedef int Type;
};

#if defined(OS_WIN)
template <>
struct SimilarTypeTraits<HWND> {
  typedef HANDLE Type;
};
#endif  // defined(OS_WIN)

template <>
struct IPC_EXPORT ParamTraits<base::Time> {
  typedef base::Time param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<base::TimeDelta> {
  typedef base::TimeDelta param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<base::TimeTicks> {
  typedef base::TimeTicks param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<Tuple0> {
  typedef Tuple0 param_type;
  static void Write(Message* m, const param_type& p) {
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
  }
};

template <class A>
struct ParamTraits< Tuple1<A> > {
  typedef Tuple1<A> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return ReadParam(m, iter, &r->a);
  }
  static void Log(const param_type& p, std::string* l) {
    LogParam(p.a, l);
  }
};

template <class A, class B>
struct ParamTraits< Tuple2<A, B> > {
  typedef Tuple2<A, B> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
    WriteParam(m, p.b);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return (ReadParam(m, iter, &r->a) &&
            ReadParam(m, iter, &r->b));
  }
  static void Log(const param_type& p, std::string* l) {
    LogParam(p.a, l);
    l->append(", ");
    LogParam(p.b, l);
  }
};

template <class A, class B, class C>
struct ParamTraits< Tuple3<A, B, C> > {
  typedef Tuple3<A, B, C> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
    WriteParam(m, p.b);
    WriteParam(m, p.c);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return (ReadParam(m, iter, &r->a) &&
            ReadParam(m, iter, &r->b) &&
            ReadParam(m, iter, &r->c));
  }
  static void Log(const param_type& p, std::string* l) {
    LogParam(p.a, l);
    l->append(", ");
    LogParam(p.b, l);
    l->append(", ");
    LogParam(p.c, l);
  }
};

template <class A, class B, class C, class D>
struct ParamTraits< Tuple4<A, B, C, D> > {
  typedef Tuple4<A, B, C, D> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
    WriteParam(m, p.b);
    WriteParam(m, p.c);
    WriteParam(m, p.d);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return (ReadParam(m, iter, &r->a) &&
            ReadParam(m, iter, &r->b) &&
            ReadParam(m, iter, &r->c) &&
            ReadParam(m, iter, &r->d));
  }
  static void Log(const param_type& p, std::string* l) {
    LogParam(p.a, l);
    l->append(", ");
    LogParam(p.b, l);
    l->append(", ");
    LogParam(p.c, l);
    l->append(", ");
    LogParam(p.d, l);
  }
};

template <class A, class B, class C, class D, class E>
struct ParamTraits< Tuple5<A, B, C, D, E> > {
  typedef Tuple5<A, B, C, D, E> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.a);
    WriteParam(m, p.b);
    WriteParam(m, p.c);
    WriteParam(m, p.d);
    WriteParam(m, p.e);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    return (ReadParam(m, iter, &r->a) &&
            ReadParam(m, iter, &r->b) &&
            ReadParam(m, iter, &r->c) &&
            ReadParam(m, iter, &r->d) &&
            ReadParam(m, iter, &r->e));
  }
  static void Log(const param_type& p, std::string* l) {
    LogParam(p.a, l);
    l->append(", ");
    LogParam(p.b, l);
    l->append(", ");
    LogParam(p.c, l);
    l->append(", ");
    LogParam(p.d, l);
    l->append(", ");
    LogParam(p.e, l);
  }
};

template<class P>
struct ParamTraits<ScopedVector<P> > {
  typedef ScopedVector<P> param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.size()));
    for (size_t i = 0; i < p.size(); i++)
      WriteParam(m, *p[i]);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    int size = 0;
    if (!m->ReadLength(iter, &size))
      return false;
    if (INT_MAX/sizeof(P) <= static_cast<size_t>(size))
      return false;
    r->resize(size);
    for (int i = 0; i < size; i++) {
      (*r)[i] = new P();
      if (!ReadParam(m, iter, (*r)[i]))
        return false;
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    for (size_t i = 0; i < p.size(); ++i) {
      if (i != 0)
        l->append(" ");
      LogParam(*p[i], l);
    }
  }
};

template <typename NormalMap,
          int kArraySize,
          typename EqualKey,
          typename MapInit>
struct ParamTraits<base::SmallMap<NormalMap, kArraySize, EqualKey, MapInit> > {
  typedef base::SmallMap<NormalMap, kArraySize, EqualKey, MapInit> param_type;
  typedef typename param_type::key_type K;
  typedef typename param_type::data_type V;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.size()));
    typename param_type::const_iterator iter;
    for (iter = p.begin(); iter != p.end(); ++iter) {
      WriteParam(m, iter->first);
      WriteParam(m, iter->second);
    }
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    int size;
    if (!m->ReadLength(iter, &size))
      return false;
    for (int i = 0; i < size; ++i) {
      K key;
      if (!ReadParam(m, iter, &key))
        return false;
      V& value = (*r)[key];
      if (!ReadParam(m, iter, &value))
        return false;
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<base::SmallMap>");
  }
};

template <class P>
struct ParamTraits<scoped_ptr<P> > {
  typedef scoped_ptr<P> param_type;
  static void Write(Message* m, const param_type& p) {
    bool valid = !!p;
    WriteParam(m, valid);
    if (valid)
      WriteParam(m, *p);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    bool valid = false;
    if (!ReadParam(m, iter, &valid))
      return false;

    if (!valid) {
      r->reset();
      return true;
    }

    param_type temp(new P());
    if (!ReadParam(m, iter, temp.get()))
      return false;

    r->swap(temp);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    if (p)
      LogParam(*p, l);
    else
      l->append("NULL");
  }
};

// IPC types ParamTraits -------------------------------------------------------

// A ChannelHandle is basically a platform-inspecific wrapper around the
// fact that IPC endpoints are handled specially on POSIX.  See above comments
// on FileDescriptor for more background.
template<>
struct IPC_EXPORT ParamTraits<IPC::ChannelHandle> {
  typedef ChannelHandle param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<LogData> {
  typedef LogData param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<Message> {
  static void Write(Message* m, const Message& p);
  static bool Read(const Message* m, PickleIterator* iter, Message* r);
  static void Log(const Message& p, std::string* l);
};

// Windows ParamTraits ---------------------------------------------------------

#if defined(OS_WIN)
template <>
struct IPC_EXPORT ParamTraits<HANDLE> {
  typedef HANDLE param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<LOGFONT> {
  typedef LOGFONT param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct IPC_EXPORT ParamTraits<MSG> {
  typedef MSG param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};
#endif  // defined(OS_WIN)

//-----------------------------------------------------------------------------
// Generic message subclasses

// Used for asynchronous messages.
template <class ParamType>
class MessageSchema {
 public:
  typedef ParamType Param;
  typedef typename TupleTypes<ParamType>::ParamTuple RefParam;

  static void Write(Message* msg, const RefParam& p) IPC_MSG_NOINLINE;
  static bool Read(const Message* msg, Param* p) IPC_MSG_NOINLINE;
};

// defined in ipc_logging.cc
IPC_EXPORT void GenerateLogData(const std::string& channel,
                                const Message& message,
                                LogData* data, bool get_params);


#if defined(IPC_MESSAGE_LOG_ENABLED)
inline void AddOutputParamsToLog(const Message* msg, std::string* l) {
  const std::string& output_params = msg->output_params();
  if (!l->empty() && !output_params.empty())
    l->append(", ");

  l->append(output_params);
}

template <class ReplyParamType>
inline void LogReplyParamsToMessage(const ReplyParamType& reply_params,
                                    const Message* msg) {
  if (msg->received_time() != 0) {
    std::string output_params;
    LogParam(reply_params, &output_params);
    msg->set_output_params(output_params);
  }
}

inline void ConnectMessageAndReply(const Message* msg, Message* reply) {
  if (msg->sent_time()) {
    // Don't log the sync message after dispatch, as we don't have the
    // output parameters at that point.  Instead, save its data and log it
    // with the outgoing reply message when it's sent.
    LogData* data = new LogData;
    GenerateLogData("", *msg, data, true);
    msg->set_dont_log();
    reply->set_sync_log_data(data);
  }
}
#else
inline void AddOutputParamsToLog(const Message* msg, std::string* l) {}

template <class ReplyParamType>
inline void LogReplyParamsToMessage(const ReplyParamType& reply_params,
                                    const Message* msg) {}

inline void ConnectMessageAndReply(const Message* msg, Message* reply) {}
#endif

// This class assumes that its template argument is a RefTuple (a Tuple with
// reference elements). This would go into ipc_message_utils_impl.h, but it is
// also used by chrome_frame.
template <class RefTuple>
class ParamDeserializer : public MessageReplyDeserializer {
 public:
  explicit ParamDeserializer(const RefTuple& out) : out_(out) { }

  bool SerializeOutputParameters(const IPC::Message& msg, PickleIterator iter) {
    return ReadParam(&msg, &iter, &out_);
  }

  RefTuple out_;
};

// Used for synchronous messages.
template <class SendParamType, class ReplyParamType>
class SyncMessageSchema {
 public:
  typedef SendParamType SendParam;
  typedef typename TupleTypes<SendParam>::ParamTuple RefSendParam;
  typedef ReplyParamType ReplyParam;

  static void Write(Message* msg, const RefSendParam& send) IPC_MSG_NOINLINE;
  static bool ReadSendParam(const Message* msg, SendParam* p) IPC_MSG_NOINLINE;
  static bool ReadReplyParam(
      const Message* msg,
      typename TupleTypes<ReplyParam>::ValueTuple* p) IPC_MSG_NOINLINE;

  template<class T, class S, class Method>
  static bool DispatchWithSendParams(bool ok, const SendParam& send_params,
                                     const Message* msg, T* obj, S* sender,
                                     Method func) {
    Message* reply = SyncMessage::GenerateReply(msg);
    if (ok) {
      typename TupleTypes<ReplyParam>::ValueTuple reply_params;
      DispatchToMethod(obj, func, send_params, &reply_params);
      WriteParam(reply, reply_params);
      LogReplyParamsToMessage(reply_params, msg);
    } else {
      NOTREACHED() << "Error deserializing message " << msg->type();
      reply->set_reply_error();
    }
    sender->Send(reply);
    return ok;
  }

  template<class T, class Method>
  static bool DispatchDelayReplyWithSendParams(bool ok,
                                               const SendParam& send_params,
                                               const Message* msg, T* obj,
                                               Method func) {
    Message* reply = SyncMessage::GenerateReply(msg);
    if (ok) {
      Tuple1<Message&> t = MakeRefTuple(*reply);
      ConnectMessageAndReply(msg, reply);
      DispatchToMethod(obj, func, send_params, &t);
    } else {
      NOTREACHED() << "Error deserializing message " << msg->type();
      reply->set_reply_error();
      obj->Send(reply);
    }
    return ok;
  }

  template<typename TA>
  static void WriteReplyParams(Message* reply, TA a) {
    ReplyParam p(a);
    WriteParam(reply, p);
  }

  template<typename TA, typename TB>
  static void WriteReplyParams(Message* reply, TA a, TB b) {
    ReplyParam p(a, b);
    WriteParam(reply, p);
  }

  template<typename TA, typename TB, typename TC>
  static void WriteReplyParams(Message* reply, TA a, TB b, TC c) {
    ReplyParam p(a, b, c);
    WriteParam(reply, p);
  }

  template<typename TA, typename TB, typename TC, typename TD>
  static void WriteReplyParams(Message* reply, TA a, TB b, TC c, TD d) {
    ReplyParam p(a, b, c, d);
    WriteParam(reply, p);
  }

  template<typename TA, typename TB, typename TC, typename TD, typename TE>
  static void WriteReplyParams(Message* reply, TA a, TB b, TC c, TD d, TE e) {
    ReplyParam p(a, b, c, d, e);
    WriteParam(reply, p);
  }
};

}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_UTILS_H_

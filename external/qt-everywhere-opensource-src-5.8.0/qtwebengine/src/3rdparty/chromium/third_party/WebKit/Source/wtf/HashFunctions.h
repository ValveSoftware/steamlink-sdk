/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_HashFunctions_h
#define WTF_HashFunctions_h

#include "wtf/RefPtr.h"
#include "wtf/StdLibExtras.h"
#include <memory>
#include <stdint.h>
#include <type_traits>

namespace WTF {

template <size_t size> struct IntTypes;
template <> struct IntTypes<1> { typedef int8_t SignedType; typedef uint8_t UnsignedType; };
template <> struct IntTypes<2> { typedef int16_t SignedType; typedef uint16_t UnsignedType; };
template <> struct IntTypes<4> { typedef int32_t SignedType; typedef uint32_t UnsignedType; };
template <> struct IntTypes<8> { typedef int64_t SignedType; typedef uint64_t UnsignedType; };

// integer hash function

// Thomas Wang's 32 Bit Mix Function: http://www.cris.com/~Ttwang/tech/inthash.htm
inline unsigned hashInt(uint8_t key8)
{
    unsigned key = key8;
    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key;
}

// Thomas Wang's 32 Bit Mix Function: http://www.cris.com/~Ttwang/tech/inthash.htm
inline unsigned hashInt(uint16_t key16)
{
    unsigned key = key16;
    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key;
}

// Thomas Wang's 32 Bit Mix Function: http://www.cris.com/~Ttwang/tech/inthash.htm
inline unsigned hashInt(uint32_t key)
{
    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key;
}

// Thomas Wang's 64 bit Mix Function: http://www.cris.com/~Ttwang/tech/inthash.htm
inline unsigned hashInt(uint64_t key)
{
    key += ~(key << 32);
    key ^= (key >> 22);
    key += ~(key << 13);
    key ^= (key >> 8);
    key += (key << 3);
    key ^= (key >> 15);
    key += ~(key << 27);
    key ^= (key >> 31);
    return static_cast<unsigned>(key);
}

// Compound integer hash method: http://opendatastructures.org/versions/edition-0.1d/ods-java/node33.html#SECTION00832000000000000000
inline unsigned hashInts(unsigned key1, unsigned key2)
{
    unsigned shortRandom1 = 277951225; // A random 32-bit value.
    unsigned shortRandom2 = 95187966; // A random 32-bit value.
    uint64_t longRandom = 19248658165952622LL; // A random 64-bit value.

    uint64_t product = longRandom * (shortRandom1 * key1 + shortRandom2 * key2);
    unsigned highBits = static_cast<unsigned>(product >> (sizeof(uint64_t) - sizeof(unsigned)));
    return highBits;
}

template <typename T> struct IntHash {
    static unsigned hash(T key) { return hashInt(static_cast<typename IntTypes<sizeof(T)>::UnsignedType>(key)); }
    static bool equal(T a, T b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template <typename T> struct FloatHash {
    typedef typename IntTypes<sizeof(T)>::UnsignedType Bits;
    static unsigned hash(T key)
    {
        return hashInt(bitwise_cast<Bits>(key));
    }
    static bool equal(T a, T b)
    {
        return bitwise_cast<Bits>(a) == bitwise_cast<Bits>(b);
    }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

// pointer identity hash function

template <typename T>
struct PtrHash {
    static unsigned hash(T* key)
    {
#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable: 4244) // work around what seems to be a bug in MSVC's conversion warnings
#endif
        return IntHash<uintptr_t>::hash(reinterpret_cast<uintptr_t>(key));
#if COMPILER(MSVC)
#pragma warning(pop)
#endif
    }
    static bool equal(T* a, T* b) { return a == b; }
    static bool equal(std::nullptr_t, T* b) { return !b; }
    static bool equal(T* a, std::nullptr_t) { return !a; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template <typename T>
struct RefPtrHash : PtrHash<T> {
    using PtrHash<T>::hash;
    static unsigned hash(const RefPtr<T>& key) { return hash(key.get()); }
    static unsigned hash(const PassRefPtr<T>& key) { return hash(key.get()); }
    using PtrHash<T>::equal;
    static bool equal(const RefPtr<T>& a, const RefPtr<T>& b) { return a == b; }
    static bool equal(T* a, const RefPtr<T>& b) { return a == b; }
    static bool equal(const RefPtr<T>& a, T* b) { return a == b; }
    static bool equal(const RefPtr<T>& a, const PassRefPtr<T>& b) { return a == b; }
};

template <typename T>
struct UniquePtrHash : PtrHash<T> {
    using PtrHash<T>::hash;
    static unsigned hash(const std::unique_ptr<T>& key) { return hash(key.get()); }
    static bool equal(const std::unique_ptr<T>& a, const std::unique_ptr<T>& b) { return a == b; }
    static bool equal(const std::unique_ptr<T>& a, const T* b) { return a.get() == b; }
    static bool equal(const T* a, const std::unique_ptr<T>& b) { return a == b.get(); }
};

// Default hash function for each type.
template <typename T>
struct DefaultHash;

// Actual implementation of DefaultHash.
//
// The case of |isIntegral| == false is not implemented. If you see a compile error saying DefaultHashImpl<T, false>
// is not defined, that's because the default hash functions for T are not defined. You need to implement them yourself.
template <typename T, bool isIntegral>
struct DefaultHashImpl;

template <typename T>
struct DefaultHashImpl<T, true> {
    using Hash = IntHash<typename std::make_unsigned<T>::type>;
};

// Canonical implementation of DefaultHash.
template <typename T>
struct DefaultHash : DefaultHashImpl<T, std::is_integral<T>::value> { };

// Specializations of DefaultHash follow.
template <>
struct DefaultHash<float> {
    using Hash = FloatHash<float>;
};
template <>
struct DefaultHash<double> {
    using Hash = FloatHash<double>;
};

// Specializations for pointer types.
template <typename T>
struct DefaultHash<T*> {
    using Hash = PtrHash<T>;
};
template <typename T>
struct DefaultHash<RefPtr<T>> {
    using Hash = RefPtrHash<T>;
};
template <typename T>
struct DefaultHash<std::unique_ptr<T>> {
    using Hash = UniquePtrHash<T>;
};

// Specializations for pairs.

// Generic case (T or U is non-integral):
template <typename T, typename U, bool areBothIntegral>
struct PairHashImpl {
    static unsigned hash(const std::pair<T, U>& p)
    {
        return hashInts(DefaultHash<T>::Hash::hash(p.first), DefaultHash<U>::Hash::hash(p.second));
    }
    static bool equal(const std::pair<T, U>& a, const std::pair<T, U>& b)
    {
        return DefaultHash<T>::Hash::equal(a.first, b.first) && DefaultHash<U>::Hash::equal(a.second, b.second);
    }
    static const bool safeToCompareToEmptyOrDeleted = DefaultHash<T>::Hash::safeToCompareToEmptyOrDeleted
        && DefaultHash<U>::Hash::safeToCompareToEmptyOrDeleted;
};

// Special version for pairs of integrals:
template <typename T, typename U>
struct PairHashImpl<T, U, true> {
    static unsigned hash(const std::pair<T, U>& p) { return hashInts(p.first, p.second); }
    static bool equal(const std::pair<T, U>& a, const std::pair<T, U>& b)
    {
        return PairHashImpl<T, U, false>::equal(a, b); // Refer to the generic version.
    }
    static const bool safeToCompareToEmptyOrDeleted = PairHashImpl<T, U, false>::safeToCompareToEmptyOrDeleted;
};

// Combined version:
template <typename T, typename U>
struct PairHash : PairHashImpl<T, U, std::is_integral<T>::value && std::is_integral<U>::value> { };

template <typename T, typename U>
struct DefaultHash<std::pair<T, U>> {
    using Hash = PairHash<T, U>;
};

} // namespace WTF

using WTF::DefaultHash;
using WTF::IntHash;
using WTF::PtrHash;

#endif // WTF_HashFunctions_h

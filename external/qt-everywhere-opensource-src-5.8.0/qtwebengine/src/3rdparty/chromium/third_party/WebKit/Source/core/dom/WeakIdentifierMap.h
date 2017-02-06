// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WeakIdentifierMap_h
#define WeakIdentifierMap_h

#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"

namespace blink {

// TODO(sof): WeakIdentifierMap<> belongs (out) in wtf/, but
// cannot until GarbageCollected<> can be used from WTF.

template<typename T, typename IdentifierType, bool isGarbageCollected>
class WeakIdentifierMapBase {
    USING_FAST_MALLOC(WeakIdentifierMapBase);
protected:
    using ObjectToIdentifier = HashMap<T*, IdentifierType>;
    using IdentifierToObject = HashMap<IdentifierType, T*>;

    ObjectToIdentifier m_objectToIdentifier;
    IdentifierToObject m_identifierToObject;
};

template<typename T, typename IdentifierType>
class WeakIdentifierMapBase<T, IdentifierType, true> : public GarbageCollected<WeakIdentifierMapBase<T, IdentifierType, true>> {
public:
    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_objectToIdentifier);
        visitor->trace(m_identifierToObject);
    }
protected:
    using ObjectToIdentifier = HeapHashMap<WeakMember<T>, IdentifierType>;
    using IdentifierToObject = HeapHashMap<IdentifierType, WeakMember<T>>;

    ObjectToIdentifier m_objectToIdentifier;
    IdentifierToObject m_identifierToObject;
};

template<typename T, typename IdentifierType = int>
class WeakIdentifierMap final : public WeakIdentifierMapBase<T, IdentifierType, IsGarbageCollectedType<T>::value>  {
public:
    static IdentifierType identifier(T* object)
    {
        IdentifierType result = instance().m_objectToIdentifier.get(object);

        if (WTF::isHashTraitsEmptyValue<HashTraits<IdentifierType>>(result)) {
            result = next();
            instance().put(object, result);
        }
        return result;
    }

    static T* lookup(IdentifierType identifier)
    {
        return instance().m_identifierToObject.get(identifier);
    }

    static void notifyObjectDestroyed(T* object)
    {
        instance().objectDestroyed(object);
    }

private:
    static WeakIdentifierMap<T, IdentifierType>& instance();

    WeakIdentifierMap() { }

    static IdentifierType next()
    {
        static IdentifierType s_lastId = 0;
        return ++s_lastId;
    }

    void put(T* object, IdentifierType identifier)
    {
        DCHECK(object && !this->m_objectToIdentifier.contains(object));
        this->m_objectToIdentifier.set(object, identifier);
        this->m_identifierToObject.set(identifier, object);
    }

    void objectDestroyed(T* object)
    {
        IdentifierType identifier = this->m_objectToIdentifier.take(object);
        if (!WTF::isHashTraitsEmptyValue<HashTraits<IdentifierType>>(identifier))
            this->m_identifierToObject.remove(identifier);
    }
};

#define DECLARE_WEAK_IDENTIFIER_MAP(T, ...) \
    template<> WeakIdentifierMap<T, ##__VA_ARGS__>& WeakIdentifierMap<T, ##__VA_ARGS__>::instance(); \
    extern template class WeakIdentifierMap<T, ##__VA_ARGS__>;

#define DEFINE_WEAK_IDENTIFIER_MAP(T, ...)   \
    template class WeakIdentifierMap<T, ##__VA_ARGS__>; \
    template<> WeakIdentifierMap<T, ##__VA_ARGS__>& WeakIdentifierMap<T, ##__VA_ARGS__>::instance() \
    { \
        using RefType = WeakIdentifierMap<T, ##__VA_ARGS__>; \
        DEFINE_STATIC_LOCAL(RefType, mapInstance, (new WeakIdentifierMap<T, ##__VA_ARGS__>())); \
        return mapInstance; \
    }

} // namespace blink

#endif // WeakIdentifierMap_h

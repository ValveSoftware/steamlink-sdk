/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"
#include "platform/heap/HeapLinkedStack.h"
#include "platform/heap/HeapTerminatedArrayBuilder.h"
#include "platform/heap/ThreadState.h"
#include "platform/heap/Visitor.h"
#include "public/platform/Platform.h"
#include "wtf/HashTraits.h"
#include "wtf/LinkedHashSet.h"

#include <gtest/gtest.h>

namespace WebCore {

class IntWrapper : public GarbageCollectedFinalized<IntWrapper> {
public:
    static IntWrapper* create(int x)
    {
        return new IntWrapper(x);
    }

    virtual ~IntWrapper()
    {
        ++s_destructorCalls;
    }

    static int s_destructorCalls;
    static void trace(Visitor*) { }

    int value() const { return m_x; }

    bool operator==(const IntWrapper& other) const { return other.value() == value(); }

    unsigned hash() { return IntHash<int>::hash(m_x); }

protected:
    IntWrapper(int x) : m_x(x) { }

private:
    IntWrapper();
    int m_x;
};

USED_FROM_MULTIPLE_THREADS(IntWrapper);

class ThreadMarker {
public:
    ThreadMarker() : m_creatingThread(reinterpret_cast<ThreadState*>(0)), m_num(0) { }
    ThreadMarker(unsigned i) : m_creatingThread(ThreadState::current()), m_num(i) { }
    ThreadMarker(WTF::HashTableDeletedValueType deleted) : m_creatingThread(reinterpret_cast<ThreadState*>(-1)), m_num(0) { }
    ~ThreadMarker()
    {
        EXPECT_TRUE((m_creatingThread == ThreadState::current())
            || (m_creatingThread == reinterpret_cast<ThreadState*>(0))
            || (m_creatingThread == reinterpret_cast<ThreadState*>(-1)));
    }
    bool isHashTableDeletedValue() const { return m_creatingThread == reinterpret_cast<ThreadState*>(-1); }
    bool operator==(const ThreadMarker& other) const { return other.m_creatingThread == m_creatingThread && other.m_num == m_num; }
    ThreadState* m_creatingThread;
    unsigned m_num;
};

struct ThreadMarkerHash {
    static unsigned hash(const ThreadMarker& key)
    {
        return static_cast<unsigned>(reinterpret_cast<uintptr_t>(key.m_creatingThread) + key.m_num);
    }

    static bool equal(const ThreadMarker& a, const ThreadMarker& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = false;
};

typedef std::pair<Member<IntWrapper>, WeakMember<IntWrapper> > StrongWeakPair;

struct PairWithWeakHandling : public StrongWeakPair {
    ALLOW_ONLY_INLINE_ALLOCATION();

public:
    // Regular constructor.
    PairWithWeakHandling(IntWrapper* one, IntWrapper* two)
        : StrongWeakPair(one, two)
    {
        ASSERT(one); // We use null first field to indicate empty slots in the hash table.
    }

    // The HashTable (via the HashTrait) calls this constructor with a
    // placement new to mark slots in the hash table as being deleted. We will
    // never call trace or the destructor on these slots. We mark ourselves deleted
    // with a pointer to -1 in the first field.
    PairWithWeakHandling(WTF::HashTableDeletedValueType)
        : StrongWeakPair(reinterpret_cast<IntWrapper*>(-1), nullptr)
    {
    }

    // Used by the HashTable (via the HashTrait) to skip deleted slots in the
    // table. Recognizes objects that were 'constructed' using the above
    // constructor.
    bool isHashTableDeletedValue() const { return first == reinterpret_cast<IntWrapper*>(-1); }

    // Since we don't allocate independent objects of this type, we don't need
    // a regular trace method. Instead, we use a traceInCollection method.
    void traceInCollection(Visitor* visitor, ShouldWeakPointersBeMarkedStrongly strongify)
    {
        visitor->trace(first);
        visitor->traceInCollection(second, strongify);
    }
    // The traceInCollection may not trace the weak members, so it is vital
    // that shouldRemoveFromCollection checks them so the entry can be removed
    // from the collection (otherwise it would contain a dangling pointer).
    bool shouldRemoveFromCollection(Visitor* visitor)
    {
        return !visitor->isAlive(second);
    }
};

}

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<WebCore::ThreadMarker> {
    typedef WebCore::ThreadMarkerHash Hash;
};

// ThreadMarkerHash is the default hash for ThreadMarker
template<> struct HashTraits<WebCore::ThreadMarker> : GenericHashTraits<WebCore::ThreadMarker> {
    static const bool emptyValueIsZero = true;
    static void constructDeletedValue(WebCore::ThreadMarker& slot) { new (NotNull, &slot) WebCore::ThreadMarker(HashTableDeletedValue); }
    static bool isDeletedValue(const WebCore::ThreadMarker& slot) { return slot.isHashTableDeletedValue(); }
};

// The hash algorithm for our custom pair class is just the standard double
// hash for pairs. Note that this means you can't mutate either of the parts of
// the pair while they are in the hash table, as that would change their hash
// code and thus their preferred placement in the table.
template<> struct DefaultHash<WebCore::PairWithWeakHandling> {
    typedef PairHash<WebCore::Member<WebCore::IntWrapper>, WebCore::WeakMember<WebCore::IntWrapper> > Hash;
};

// Custom traits for the pair. These are weakness handling traits, which means
// PairWithWeakHandling must implement the traceInCollection and
// shouldRemoveFromCollection methods. In addition, these traits are concerned
// with the two magic values for the object, that represent empty and deleted
// slots in the hash table. The SimpleClassHashTraits allow empty slots in the
// table to be initialzed with memset to zero, and we use -1 in the first part
// of the pair to represent deleted slots.
template<> struct HashTraits<WebCore::PairWithWeakHandling> : WebCore::WeakHandlingHashTraits<WebCore::PairWithWeakHandling> {
    static const bool needsDestruction = false;
    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const WebCore::PairWithWeakHandling& value) { return !value.first; }
    static void constructDeletedValue(WebCore::PairWithWeakHandling& slot) { new (NotNull, &slot) WebCore::PairWithWeakHandling(HashTableDeletedValue); }
    static bool isDeletedValue(const WebCore::PairWithWeakHandling& value) { return value.isHashTableDeletedValue(); }
};

}

namespace WebCore {

class TestGCScope {
public:
    explicit TestGCScope(ThreadState::StackState state)
        : m_state(ThreadState::current())
        , m_safePointScope(state)
        , m_parkedAllThreads(false)
    {
        m_state->checkThread();
        EXPECT_FALSE(m_state->isInGC());
        if (LIKELY(ThreadState::stopThreads())) {
            m_state->enterGC();
            m_parkedAllThreads = true;
        }
    }

    bool allThreadsParked() { return m_parkedAllThreads; }

    ~TestGCScope()
    {
        // Only cleanup if we parked all threads in which case the GC happened
        // and we need to resume the other threads.
        if (LIKELY(m_parkedAllThreads)) {
            m_state->leaveGC();
            EXPECT_FALSE(m_state->isInGC());
            ThreadState::resumeThreads();
        }
    }

private:
    ThreadState* m_state;
    ThreadState::SafePointScope m_safePointScope;
    bool m_parkedAllThreads; // False if we fail to park all threads
};

static void getHeapStats(HeapStats* stats)
{
    TestGCScope scope(ThreadState::NoHeapPointersOnStack);
    EXPECT_TRUE(scope.allThreadsParked());
    Heap::getStats(stats);
}

#define DEFINE_VISITOR_METHODS(Type)                                       \
    virtual void mark(const Type* object, TraceCallback callback) OVERRIDE \
    {                                                                      \
        if (object)                                                        \
            m_count++;                                                     \
    }                                                                      \
    virtual bool isMarked(const Type*) OVERRIDE { return false; }

class CountingVisitor : public Visitor {
public:
    CountingVisitor()
        : m_count(0)
    {
    }

    virtual void mark(const void* object, TraceCallback) OVERRIDE
    {
        if (object)
            m_count++;
    }

    virtual void mark(HeapObjectHeader* header, TraceCallback callback) OVERRIDE
    {
        ASSERT(header->payload());
        m_count++;
    }

    virtual void mark(FinalizedHeapObjectHeader* header, TraceCallback callback) OVERRIDE
    {
        ASSERT(header->payload());
        m_count++;
    }

    virtual void markConservatively(HeapObjectHeader* header) OVERRIDE
    {
        ASSERT_NOT_REACHED();
    }

    virtual void markConservatively(FinalizedHeapObjectHeader* header) OVERRIDE
    {
        ASSERT_NOT_REACHED();
    }

    virtual void registerWeakMembers(const void*, const void*, WeakPointerCallback) OVERRIDE { }
    virtual void registerWeakCell(void**, WeakPointerCallback) OVERRIDE { }
    virtual bool isMarked(const void*) OVERRIDE { return false; }

    FOR_EACH_TYPED_HEAP(DEFINE_VISITOR_METHODS)

    size_t count() { return m_count; }
    void reset() { m_count = 0; }

private:
    size_t m_count;
};

class SimpleObject : public GarbageCollected<SimpleObject> {
public:
    static SimpleObject* create() { return new SimpleObject(); }
    void trace(Visitor*) { }
    char getPayload(int i) { return payload[i]; }
    // This virtual method is unused but it is here to make sure
    // that this object has a vtable. This object is used
    // as the super class for objects that also have garbage
    // collected mixins and having a virtual here makes sure
    // that adjustment is needed both for marking and for isAlive
    // checks.
    virtual void virtualMethod() { }
protected:
    SimpleObject() { }
    char payload[64];
};

#undef DEFINE_VISITOR_METHODS

class HeapTestSuperClass : public GarbageCollectedFinalized<HeapTestSuperClass> {
public:
    static HeapTestSuperClass* create()
    {
        return new HeapTestSuperClass();
    }

    virtual ~HeapTestSuperClass()
    {
        ++s_destructorCalls;
    }

    static int s_destructorCalls;
    void trace(Visitor*) { }

protected:
    HeapTestSuperClass() { }
};

int HeapTestSuperClass::s_destructorCalls = 0;

class HeapTestOtherSuperClass {
public:
    int payload;
};

static const size_t classMagic = 0xABCDDBCA;

class HeapTestSubClass : public HeapTestOtherSuperClass, public HeapTestSuperClass {
public:
    static HeapTestSubClass* create()
    {
        return new HeapTestSubClass();
    }

    virtual ~HeapTestSubClass()
    {
        EXPECT_EQ(classMagic, m_magic);
        ++s_destructorCalls;
    }

    static int s_destructorCalls;

private:

    HeapTestSubClass() : m_magic(classMagic) { }

    const size_t m_magic;
};

int HeapTestSubClass::s_destructorCalls = 0;

class HeapAllocatedArray : public GarbageCollected<HeapAllocatedArray> {
public:
    HeapAllocatedArray()
    {
        for (int i = 0; i < s_arraySize; ++i) {
            m_array[i] = i % 128;
        }
    }

    int8_t at(size_t i) { return m_array[i]; }
    void trace(Visitor*) { }
private:
    static const int s_arraySize = 1000;
    int8_t m_array[s_arraySize];
};

// Do several GCs to make sure that later GCs don't free up old memory from
// previously run tests in this process.
static void clearOutOldGarbage(HeapStats* heapStats)
{
    while (true) {
        getHeapStats(heapStats);
        size_t used = heapStats->totalObjectSpace();
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        getHeapStats(heapStats);
        if (heapStats->totalObjectSpace() >= used)
            break;
    }
}

class OffHeapInt : public RefCounted<OffHeapInt> {
public:
    static RefPtr<OffHeapInt> create(int x)
    {
        return adoptRef(new OffHeapInt(x));
    }

    virtual ~OffHeapInt()
    {
        ++s_destructorCalls;
    }

    static int s_destructorCalls;

    int value() const { return m_x; }

    bool operator==(const OffHeapInt& other) const { return other.value() == value(); }

    unsigned hash() { return IntHash<int>::hash(m_x); }
    void voidFunction() { }

protected:
    OffHeapInt(int x) : m_x(x) { }

private:
    OffHeapInt();
    int m_x;
};

int IntWrapper::s_destructorCalls = 0;
int OffHeapInt::s_destructorCalls = 0;

class ThreadedTesterBase {
protected:
    static void test(ThreadedTesterBase* tester)
    {
        for (int i = 0; i < numberOfThreads; i++)
            createThread(&threadFunc, tester, "testing thread");
        while (tester->m_threadsToFinish) {
            ThreadState::SafePointScope scope(ThreadState::NoHeapPointersOnStack);
            yield();
        }
        delete tester;
    }

    virtual void runThread() = 0;

protected:
    static const int numberOfThreads = 10;
    static const int gcPerThread = 5;
    static const int numberOfAllocations = 50;

    ThreadedTesterBase() : m_gcCount(0), m_threadsToFinish(numberOfThreads)
    {
    }

    virtual ~ThreadedTesterBase()
    {
    }

    inline bool done() const { return m_gcCount >= numberOfThreads * gcPerThread; }

    volatile int m_gcCount;
    volatile int m_threadsToFinish;

private:
    static void threadFunc(void* data)
    {
        reinterpret_cast<ThreadedTesterBase*>(data)->runThread();
    }
};

class ThreadedHeapTester : public ThreadedTesterBase {
public:
    static void test()
    {
        ThreadedTesterBase::test(new ThreadedHeapTester);
    }

protected:
    virtual void runThread() OVERRIDE
    {
        ThreadState::attach();

        int gcCount = 0;
        while (!done()) {
            ThreadState::current()->safePoint(ThreadState::NoHeapPointersOnStack);
            {
                Persistent<IntWrapper> wrapper;

                typedef Persistent<IntWrapper, GlobalPersistents> GlobalIntWrapperPersistent;
                OwnPtr<GlobalIntWrapperPersistent> globalPersistent = adoptPtr(new GlobalIntWrapperPersistent(IntWrapper::create(0x0ed0cabb)));

                for (int i = 0; i < numberOfAllocations; i++) {
                    wrapper = IntWrapper::create(0x0bbac0de);
                    if (!(i % 10)) {
                        globalPersistent = adoptPtr(new GlobalIntWrapperPersistent(IntWrapper::create(0x0ed0cabb)));
                    }
                    ThreadState::SafePointScope scope(ThreadState::NoHeapPointersOnStack);
                    yield();
                }

                if (gcCount < gcPerThread) {
                    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
                    gcCount++;
                    atomicIncrement(&m_gcCount);
                }

                Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
                EXPECT_EQ(wrapper->value(), 0x0bbac0de);
                EXPECT_EQ((*globalPersistent)->value(), 0x0ed0cabb);
            }
            ThreadState::SafePointScope scope(ThreadState::NoHeapPointersOnStack);
            yield();
        }
        ThreadState::detach();
        atomicDecrement(&m_threadsToFinish);
    }
};

class ThreadedWeaknessTester : public ThreadedTesterBase {
public:
    static void test()
    {
        ThreadedTesterBase::test(new ThreadedWeaknessTester);
    }

private:
    virtual void runThread() OVERRIDE
    {
        ThreadState::attach();

        int gcCount = 0;
        while (!done()) {
            ThreadState::current()->safePoint(ThreadState::NoHeapPointersOnStack);
            {
                Persistent<HeapHashMap<ThreadMarker, WeakMember<IntWrapper> > > weakMap = new HeapHashMap<ThreadMarker, WeakMember<IntWrapper> >;
                PersistentHeapHashMap<ThreadMarker, WeakMember<IntWrapper> > weakMap2;

                for (int i = 0; i < numberOfAllocations; i++) {
                    weakMap->add(static_cast<unsigned>(i), IntWrapper::create(0));
                    weakMap2.add(static_cast<unsigned>(i), IntWrapper::create(0));
                    ThreadState::SafePointScope scope(ThreadState::NoHeapPointersOnStack);
                    yield();
                }

                if (gcCount < gcPerThread) {
                    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
                    gcCount++;
                    atomicIncrement(&m_gcCount);
                }

                Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
                EXPECT_TRUE(weakMap->isEmpty());
                EXPECT_TRUE(weakMap2.isEmpty());
            }
            ThreadState::SafePointScope scope(ThreadState::NoHeapPointersOnStack);
            yield();
        }
        ThreadState::detach();
        atomicDecrement(&m_threadsToFinish);
    }
};

// The accounting for memory includes the memory used by rounding up object
// sizes. This is done in a different way on 32 bit and 64 bit, so we have to
// have some slack in the tests.
template<typename T>
void CheckWithSlack(T expected, T actual, int slack)
{
    EXPECT_LE(expected, actual);
    EXPECT_GE((intptr_t)expected + slack, (intptr_t)actual);
}

class TraceCounter : public GarbageCollectedFinalized<TraceCounter> {
public:
    static TraceCounter* create()
    {
        return new TraceCounter();
    }

    void trace(Visitor*) { m_traceCount++; }

    int traceCount() { return m_traceCount; }

private:
    TraceCounter()
        : m_traceCount(0)
    {
    }

    int m_traceCount;
};

class ClassWithMember : public GarbageCollected<ClassWithMember> {
public:
    static ClassWithMember* create()
    {
        return new ClassWithMember();
    }

    void trace(Visitor* visitor)
    {
        EXPECT_TRUE(visitor->isMarked(this));
        if (!traceCount())
            EXPECT_FALSE(visitor->isMarked(m_traceCounter));
        else
            EXPECT_TRUE(visitor->isMarked(m_traceCounter));

        visitor->trace(m_traceCounter);
    }

    int traceCount() { return m_traceCounter->traceCount(); }

private:
    ClassWithMember()
        : m_traceCounter(TraceCounter::create())
    { }

    Member<TraceCounter> m_traceCounter;
};

class SimpleFinalizedObject : public GarbageCollectedFinalized<SimpleFinalizedObject> {
public:
    static SimpleFinalizedObject* create()
    {
        return new SimpleFinalizedObject();
    }

    ~SimpleFinalizedObject()
    {
        ++s_destructorCalls;
    }

    static int s_destructorCalls;

    void trace(Visitor*) { }

private:
    SimpleFinalizedObject() { }
};

int SimpleFinalizedObject::s_destructorCalls = 0;

class TestTypedHeapClass : public GarbageCollected<TestTypedHeapClass> {
public:
    static TestTypedHeapClass* create()
    {
        return new TestTypedHeapClass();
    }

    void trace(Visitor*) { }

private:
    TestTypedHeapClass() { }
};

class Bar : public GarbageCollectedFinalized<Bar> {
public:
    static Bar* create()
    {
        return new Bar();
    }

    void finalizeGarbageCollectedObject()
    {
        EXPECT_TRUE(m_magic == magic);
        m_magic = 0;
        s_live--;
    }
    bool hasBeenFinalized() const { return !m_magic; }

    virtual void trace(Visitor* visitor) { }
    static unsigned s_live;

protected:
    static const int magic = 1337;
    int m_magic;

    Bar()
        : m_magic(magic)
    {
        s_live++;
    }
};

unsigned Bar::s_live = 0;

class Baz : public GarbageCollected<Baz> {
public:
    static Baz* create(Bar* bar)
    {
        return new Baz(bar);
    }

    void trace(Visitor* visitor)
    {
        visitor->trace(m_bar);
    }

    void clear() { m_bar.release(); }

    // willFinalize is called by FinalizationObserver.
    void willFinalize()
    {
        EXPECT_TRUE(!m_bar->hasBeenFinalized());
    }

private:
    explicit Baz(Bar* bar)
        : m_bar(bar)
    {
    }

    Member<Bar> m_bar;
};

class Foo : public Bar {
public:
    static Foo* create(Bar* bar)
    {
        return new Foo(bar);
    }

    static Foo* create(Foo* foo)
    {
        return new Foo(foo);
    }

    virtual void trace(Visitor* visitor) OVERRIDE
    {
        if (m_pointsToFoo)
            visitor->mark(static_cast<Foo*>(m_bar));
        else
            visitor->mark(m_bar);
    }

private:
    Foo(Bar* bar)
        : Bar()
        , m_bar(bar)
        , m_pointsToFoo(false)
    {
    }

    Foo(Foo* foo)
        : Bar()
        , m_bar(foo)
        , m_pointsToFoo(true)
    {
    }

    Bar* m_bar;
    bool m_pointsToFoo;
};

class Bars : public Bar {
public:
    static Bars* create()
    {
        return new Bars();
    }

    virtual void trace(Visitor* visitor) OVERRIDE
    {
        for (unsigned i = 0; i < m_width; i++)
            visitor->trace(m_bars[i]);
    }

    unsigned getWidth() const
    {
        return m_width;
    }

    static const unsigned width = 7500;
private:
    Bars() : m_width(0)
    {
        for (unsigned i = 0; i < width; i++) {
            m_bars[i] = Bar::create();
            m_width++;
        }
    }

    unsigned m_width;
    Member<Bar> m_bars[width];
};

class ConstructorAllocation : public GarbageCollected<ConstructorAllocation> {
public:
    static ConstructorAllocation* create() { return new ConstructorAllocation(); }

    void trace(Visitor* visitor) { visitor->trace(m_intWrapper); }

private:
    ConstructorAllocation()
    {
        m_intWrapper = IntWrapper::create(42);
    }

    Member<IntWrapper> m_intWrapper;
};

class LargeObject : public GarbageCollectedFinalized<LargeObject> {
public:
    ~LargeObject()
    {
        s_destructorCalls++;
    }
    static LargeObject* create() { return new LargeObject(); }
    char get(size_t i) { return m_data[i]; }
    void set(size_t i, char c) { m_data[i] = c; }
    size_t length() { return s_length; }
    void trace(Visitor* visitor)
    {
        visitor->trace(m_intWrapper);
    }
    static int s_destructorCalls;

private:
    static const size_t s_length = 1024*1024;
    LargeObject()
    {
        m_intWrapper = IntWrapper::create(23);
    }
    Member<IntWrapper> m_intWrapper;
    char m_data[s_length];
};

int LargeObject::s_destructorCalls = 0;

class RefCountedAndGarbageCollected : public RefCountedGarbageCollected<RefCountedAndGarbageCollected> {
public:
    static PassRefPtr<RefCountedAndGarbageCollected> create()
    {
        return adoptRef(new RefCountedAndGarbageCollected());
    }

    ~RefCountedAndGarbageCollected()
    {
        ++s_destructorCalls;
    }

    // These are here with their default implementations so you can break in
    // them in the debugger.
    void ref() { RefCountedGarbageCollected<RefCountedAndGarbageCollected>::ref(); }
    void deref() { RefCountedGarbageCollected<RefCountedAndGarbageCollected>::deref(); }

    void trace(Visitor*) { }

    static int s_destructorCalls;

private:
    RefCountedAndGarbageCollected()
    {
    }
};

int RefCountedAndGarbageCollected::s_destructorCalls = 0;

class RefCountedAndGarbageCollected2 : public HeapTestOtherSuperClass, public RefCountedGarbageCollected<RefCountedAndGarbageCollected2> {
public:
    static RefCountedAndGarbageCollected2* create()
    {
        return adoptRefCountedGarbageCollected(new RefCountedAndGarbageCollected2());
    }

    ~RefCountedAndGarbageCollected2()
    {
        ++s_destructorCalls;
    }

    void trace(Visitor*) { }

    static int s_destructorCalls;

private:
    RefCountedAndGarbageCollected2()
    {
    }
};

int RefCountedAndGarbageCollected2::s_destructorCalls = 0;

#define DEFINE_VISITOR_METHODS(Type)                                       \
    virtual void mark(const Type* object, TraceCallback callback) OVERRIDE \
    {                                                                      \
        mark(object);                                                      \
    }                                                                      \

class RefCountedGarbageCollectedVisitor : public CountingVisitor {
public:
    RefCountedGarbageCollectedVisitor(int expected, void** objects)
        : m_count(0)
        , m_expectedCount(expected)
        , m_expectedObjects(objects)
    {
    }

    void mark(const void* ptr) { markNoTrace(ptr); }

    virtual void markNoTrace(const void* ptr)
    {
        if (!ptr)
            return;
        if (m_count < m_expectedCount)
            EXPECT_TRUE(expectedObject(ptr));
        else
            EXPECT_FALSE(expectedObject(ptr));
        m_count++;
    }

    virtual void mark(const void* ptr, TraceCallback) OVERRIDE
    {
        mark(ptr);
    }

    virtual void mark(HeapObjectHeader* header, TraceCallback callback) OVERRIDE
    {
        mark(header->payload());
    }

    virtual void mark(FinalizedHeapObjectHeader* header, TraceCallback callback) OVERRIDE
    {
        mark(header->payload());
    }

    bool validate() { return m_count >= m_expectedCount; }
    void reset() { m_count = 0; }

    FOR_EACH_TYPED_HEAP(DEFINE_VISITOR_METHODS)

private:
    bool expectedObject(const void* ptr)
    {
        for (int i = 0; i < m_expectedCount; i++) {
            if (m_expectedObjects[i] == ptr)
                return true;
        }
        return false;
    }

    int m_count;
    int m_expectedCount;
    void** m_expectedObjects;
};

#undef DEFINE_VISITOR_METHODS

class Weak : public Bar {
public:
    static Weak* create(Bar* strong, Bar* weak)
    {
        return new Weak(strong, weak);
    }

    virtual void trace(Visitor* visitor) OVERRIDE
    {
        visitor->trace(m_strongBar);
        visitor->registerWeakMembers(this, zapWeakMembers);
    }

    static void zapWeakMembers(Visitor* visitor, void* self)
    {
        reinterpret_cast<Weak*>(self)->zapWeakMembers(visitor);
    }

    bool strongIsThere() { return !!m_strongBar; }
    bool weakIsThere() { return !!m_weakBar; }

private:
    Weak(Bar* strongBar, Bar* weakBar)
        : Bar()
        , m_strongBar(strongBar)
        , m_weakBar(weakBar)
    {
    }

    void zapWeakMembers(Visitor* visitor)
    {
        if (!visitor->isAlive(m_weakBar))
            m_weakBar = 0;
    }

    Member<Bar> m_strongBar;
    Bar* m_weakBar;
};

class WithWeakMember : public Bar {
public:
    static WithWeakMember* create(Bar* strong, Bar* weak)
    {
        return new WithWeakMember(strong, weak);
    }

    virtual void trace(Visitor* visitor) OVERRIDE
    {
        visitor->trace(m_strongBar);
        visitor->trace(m_weakBar);
    }

    bool strongIsThere() { return !!m_strongBar; }
    bool weakIsThere() { return !!m_weakBar; }

private:
    WithWeakMember(Bar* strongBar, Bar* weakBar)
        : Bar()
        , m_strongBar(strongBar)
        , m_weakBar(weakBar)
    {
    }

    Member<Bar> m_strongBar;
    WeakMember<Bar> m_weakBar;
};

class Observable : public GarbageCollectedFinalized<Observable> {
public:
    static Observable* create(Bar* bar) { return new Observable(bar);  }
    ~Observable() { m_wasDestructed = true; }
    void trace(Visitor* visitor) { visitor->trace(m_bar); }

    // willFinalize is called by FinalizationObserver. willFinalize can touch
    // other on-heap objects.
    void willFinalize()
    {
        EXPECT_FALSE(m_wasDestructed);
        EXPECT_FALSE(m_bar->hasBeenFinalized());
    }

private:
    explicit Observable(Bar* bar)
        : m_bar(bar)
        , m_wasDestructed(false)
    {
    }

    Member<Bar> m_bar;
    bool m_wasDestructed;
};

template <typename T> class FinalizationObserver : public GarbageCollected<FinalizationObserver<T> > {
public:
    static FinalizationObserver* create(T* data) { return new FinalizationObserver(data); }
    bool didCallWillFinalize() const { return m_didCallWillFinalize; }

    void trace(Visitor* visitor)
    {
        visitor->registerWeakMembers(this, zapWeakMembers);
    }

private:
    FinalizationObserver(T* data)
        : m_data(data)
        , m_didCallWillFinalize(false)
    {
    }

    static void zapWeakMembers(Visitor* visitor, void* self)
    {
        FinalizationObserver* o = reinterpret_cast<FinalizationObserver*>(self);
        if (o->m_data && !visitor->isAlive(o->m_data)) {
            o->m_data->willFinalize();
            o->m_data = nullptr;
            o->m_didCallWillFinalize = true;
        }
    }

    WeakMember<T> m_data;
    bool m_didCallWillFinalize;
};

class FinalizationObserverWithHashMap {
public:
    typedef HeapHashMap<WeakMember<Observable>, OwnPtr<FinalizationObserverWithHashMap> > ObserverMap;

    explicit FinalizationObserverWithHashMap(Observable& target) : m_target(target) { }
    ~FinalizationObserverWithHashMap()
    {
        m_target.willFinalize();
        s_didCallWillFinalize = true;
    }

    static ObserverMap& observe(Observable& target)
    {
        ObserverMap& map = observers();
        ObserverMap::AddResult result = map.add(&target, nullptr);
        if (result.isNewEntry)
            result.storedValue->value = adoptPtr(new FinalizationObserverWithHashMap(target));
        else
            ASSERT(result.storedValue->value);
        return map;
    }

    static bool s_didCallWillFinalize;

private:
    static ObserverMap& observers()
    {
        DEFINE_STATIC_LOCAL(Persistent<ObserverMap>, observerMap, ());
        if (!observerMap)
            observerMap = new ObserverMap();
        return *observerMap;
    }

    Observable& m_target;
};

bool FinalizationObserverWithHashMap::s_didCallWillFinalize = false;

class SuperClass;

class PointsBack : public RefCountedWillBeGarbageCollectedFinalized<PointsBack> {
public:
    static PassRefPtrWillBeRawPtr<PointsBack> create()
    {
        return adoptRefWillBeNoop(new PointsBack());
    }

    ~PointsBack()
    {
        --s_aliveCount;
    }

    void setBackPointer(SuperClass* backPointer)
    {
        m_backPointer = backPointer;
    }

    SuperClass* backPointer() const { return m_backPointer; }

    void trace(Visitor* visitor)
    {
#if ENABLE_OILPAN
        visitor->trace(m_backPointer);
#endif
    }

    static int s_aliveCount;
private:
    PointsBack() : m_backPointer(nullptr)
    {
        ++s_aliveCount;
    }

    RawPtrWillBeWeakMember<SuperClass> m_backPointer;
};

int PointsBack::s_aliveCount = 0;

class SuperClass : public RefCountedWillBeGarbageCollectedFinalized<SuperClass> {
public:
    static PassRefPtrWillBeRawPtr<SuperClass> create(PassRefPtrWillBeRawPtr<PointsBack> pointsBack)
    {
        return adoptRefWillBeNoop(new SuperClass(pointsBack));
    }

    virtual ~SuperClass()
    {
#if !ENABLE_OILPAN
        m_pointsBack->setBackPointer(0);
#endif
        --s_aliveCount;
    }

    void doStuff(PassRefPtrWillBeRawPtr<SuperClass> targetPass, PointsBack* pointsBack, int superClassCount)
    {
        RefPtrWillBeRawPtr<SuperClass> target = targetPass;
        Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        EXPECT_EQ(pointsBack, target->pointsBack());
        EXPECT_EQ(superClassCount, SuperClass::s_aliveCount);
    }

    virtual void trace(Visitor* visitor)
    {
#if ENABLE_OILPAN
        visitor->trace(m_pointsBack);
#endif
    }

    PointsBack* pointsBack() const { return m_pointsBack.get(); }

    static int s_aliveCount;
protected:
    explicit SuperClass(PassRefPtrWillBeRawPtr<PointsBack> pointsBack)
        : m_pointsBack(pointsBack)
    {
        m_pointsBack->setBackPointer(this);
        ++s_aliveCount;
    }

private:
    RefPtrWillBeMember<PointsBack> m_pointsBack;
};

int SuperClass::s_aliveCount = 0;
class SubData : public NoBaseWillBeGarbageCollectedFinalized<SubData> {
public:
    SubData() { ++s_aliveCount; }
    ~SubData() { --s_aliveCount; }

    void trace(Visitor*) { }

    static int s_aliveCount;
};

int SubData::s_aliveCount = 0;

class SubClass : public SuperClass {
public:
    static PassRefPtrWillBeRawPtr<SubClass> create(PassRefPtrWillBeRawPtr<PointsBack> pointsBack)
    {
        return adoptRefWillBeNoop(new SubClass(pointsBack));
    }

    virtual ~SubClass()
    {
        --s_aliveCount;
    }

    virtual void trace(Visitor* visitor)
    {
#if ENABLE_OILPAN
        SuperClass::trace(visitor);
        visitor->trace(m_data);
#endif
    }

    static int s_aliveCount;
private:
    explicit SubClass(PassRefPtrWillBeRawPtr<PointsBack> pointsBack)
        : SuperClass(pointsBack)
        , m_data(adoptPtrWillBeNoop(new SubData()))
    {
        ++s_aliveCount;
    }

private:
    OwnPtrWillBeMember<SubData> m_data;
};

int SubClass::s_aliveCount = 0;

class TransitionRefCounted : public RefCountedWillBeRefCountedGarbageCollected<TransitionRefCounted> {
public:
    static PassRefPtrWillBeRawPtr<TransitionRefCounted> create()
    {
        return adoptRefWillBeRefCountedGarbageCollected(new TransitionRefCounted());
    }

    ~TransitionRefCounted()
    {
        --s_aliveCount;
    }

    void trace(Visitor* visitor) { }

    static int s_aliveCount;

private:
    TransitionRefCounted()
    {
        ++s_aliveCount;
    }
};

int TransitionRefCounted::s_aliveCount = 0;

class Mixin : public GarbageCollectedMixin {
public:
    virtual void trace(Visitor* visitor) { }

    virtual char getPayload(int i) { return m_padding[i]; }

protected:
    int m_padding[8];
};

class UseMixin : public SimpleObject, public Mixin {
    USING_GARBAGE_COLLECTED_MIXIN(UseMixin)
public:
    static UseMixin* create()
    {
        return new UseMixin();
    }

    static int s_traceCount;
    virtual void trace(Visitor* visitor)
    {
        SimpleObject::trace(visitor);
        Mixin::trace(visitor);
        ++s_traceCount;
    }

private:
    UseMixin()
    {
        s_traceCount = 0;
    }
};

int UseMixin::s_traceCount = 0;

class VectorObject {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    VectorObject()
    {
        m_value = SimpleFinalizedObject::create();
    }

    void trace(Visitor* visitor)
    {
        visitor->trace(m_value);
    }

private:
    Member<SimpleFinalizedObject> m_value;
};

class VectorObjectInheritedTrace : public VectorObject { };

class VectorObjectNoTrace {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    VectorObjectNoTrace()
    {
        m_value = SimpleFinalizedObject::create();
    }

private:
    Member<SimpleFinalizedObject> m_value;
};

class TerminatedArrayItem {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    TerminatedArrayItem(IntWrapper* payload) : m_payload(payload), m_isLast(false) { }

    void trace(Visitor* visitor) { visitor->trace(m_payload); }

    bool isLastInArray() const { return m_isLast; }
    void setLastInArray(bool value) { m_isLast = value; }

    IntWrapper* payload() const { return m_payload; }

private:
    Member<IntWrapper> m_payload;
    bool m_isLast;
};

} // WebCore namespace

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(WebCore::VectorObject);
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(WebCore::VectorObjectInheritedTrace);
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(WebCore::VectorObjectNoTrace);

namespace WebCore {

class OneKiloByteObject : public GarbageCollectedFinalized<OneKiloByteObject> {
public:
    ~OneKiloByteObject() { s_destructorCalls++; }
    char* data() { return m_data; }
    void trace(Visitor* visitor) { }
    static int s_destructorCalls;

private:
    static const size_t s_length = 1024;
    char m_data[s_length];
};

int OneKiloByteObject::s_destructorCalls = 0;

class DynamicallySizedObject : public GarbageCollected<DynamicallySizedObject> {
public:
    static DynamicallySizedObject* create(size_t size)
    {
        void* slot = Heap::allocate<DynamicallySizedObject>(size);
        return new (slot) DynamicallySizedObject();
    }

    void* operator new(std::size_t, void* location)
    {
        return location;
    }

    uint8_t get(int i)
    {
        return *(reinterpret_cast<uint8_t*>(this) + i);
    }

    void trace(Visitor*) { }

private:
    DynamicallySizedObject() { }
};

class FinalizationAllocator : public GarbageCollectedFinalized<FinalizationAllocator> {
public:
    FinalizationAllocator(Persistent<IntWrapper>* wrapper)
        : m_wrapper(wrapper)
    {
    }

    ~FinalizationAllocator()
    {
        for (int i = 0; i < 10; ++i)
            *m_wrapper = IntWrapper::create(42);
        for (int i = 0; i < 512; ++i)
            new OneKiloByteObject();
    }

    void trace(Visitor*) { }

private:
    Persistent<IntWrapper>* m_wrapper;
};

TEST(HeapTest, Transition)
{
    {
        RefPtr<TransitionRefCounted> refCounted = TransitionRefCounted::create();
        EXPECT_EQ(1, TransitionRefCounted::s_aliveCount);
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(1, TransitionRefCounted::s_aliveCount);
    }
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0, TransitionRefCounted::s_aliveCount);

    RefPtrWillBePersistent<PointsBack> pointsBack1 = PointsBack::create();
    RefPtrWillBePersistent<PointsBack> pointsBack2 = PointsBack::create();
    RefPtrWillBePersistent<SuperClass> superClass = SuperClass::create(pointsBack1);
    RefPtrWillBePersistent<SubClass> subClass = SubClass::create(pointsBack2);
    EXPECT_EQ(2, PointsBack::s_aliveCount);
    EXPECT_EQ(2, SuperClass::s_aliveCount);
    EXPECT_EQ(1, SubClass::s_aliveCount);
    EXPECT_EQ(1, SubData::s_aliveCount);

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0, TransitionRefCounted::s_aliveCount);
    EXPECT_EQ(2, PointsBack::s_aliveCount);
    EXPECT_EQ(2, SuperClass::s_aliveCount);
    EXPECT_EQ(1, SubClass::s_aliveCount);
    EXPECT_EQ(1, SubData::s_aliveCount);

    superClass->doStuff(superClass.release(), pointsBack1.get(), 2);
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(2, PointsBack::s_aliveCount);
    EXPECT_EQ(1, SuperClass::s_aliveCount);
    EXPECT_EQ(1, SubClass::s_aliveCount);
    EXPECT_EQ(1, SubData::s_aliveCount);
    EXPECT_EQ(0, pointsBack1->backPointer());

    pointsBack1.release();
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(1, PointsBack::s_aliveCount);
    EXPECT_EQ(1, SuperClass::s_aliveCount);
    EXPECT_EQ(1, SubClass::s_aliveCount);
    EXPECT_EQ(1, SubData::s_aliveCount);

    subClass->doStuff(subClass.release(), pointsBack2.get(), 1);
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(1, PointsBack::s_aliveCount);
    EXPECT_EQ(0, SuperClass::s_aliveCount);
    EXPECT_EQ(0, SubClass::s_aliveCount);
    EXPECT_EQ(0, SubData::s_aliveCount);
    EXPECT_EQ(0, pointsBack2->backPointer());

    pointsBack2.release();
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0, PointsBack::s_aliveCount);
    EXPECT_EQ(0, SuperClass::s_aliveCount);
    EXPECT_EQ(0, SubClass::s_aliveCount);
    EXPECT_EQ(0, SubData::s_aliveCount);

    EXPECT_TRUE(superClass == subClass);
}

TEST(HeapTest, Threading)
{
    ThreadedHeapTester::test();
}

TEST(HeapTest, ThreadedWeakness)
{
    ThreadedWeaknessTester::test();
}

TEST(HeapTest, BasicFunctionality)
{
    HeapStats heapStats;
    clearOutOldGarbage(&heapStats);
    {
        size_t slack = 0;

        // When the test starts there may already have been leaked some memory
        // on the heap, so we establish a base line.
        size_t baseLevel = heapStats.totalObjectSpace();
        bool testPagesAllocated = !baseLevel;
        if (testPagesAllocated)
            EXPECT_EQ(heapStats.totalAllocatedSpace(), 0ul);

        // This allocates objects on the general heap which should add a page of memory.
        DynamicallySizedObject* alloc32 = DynamicallySizedObject::create(32);
        slack += 4;
        memset(alloc32, 40, 32);
        DynamicallySizedObject* alloc64 = DynamicallySizedObject::create(64);
        slack += 4;
        memset(alloc64, 27, 64);

        size_t total = 96;

        getHeapStats(&heapStats);
        CheckWithSlack(baseLevel + total, heapStats.totalObjectSpace(), slack);
        if (testPagesAllocated)
            EXPECT_EQ(heapStats.totalAllocatedSpace(), blinkPageSize);

        CheckWithSlack(alloc32 + 32 + sizeof(HeapObjectHeader), alloc64, slack);

        EXPECT_EQ(alloc32->get(0), 40);
        EXPECT_EQ(alloc32->get(31), 40);
        EXPECT_EQ(alloc64->get(0), 27);
        EXPECT_EQ(alloc64->get(63), 27);

        Heap::collectGarbage(ThreadState::HeapPointersOnStack);

        EXPECT_EQ(alloc32->get(0), 40);
        EXPECT_EQ(alloc32->get(31), 40);
        EXPECT_EQ(alloc64->get(0), 27);
        EXPECT_EQ(alloc64->get(63), 27);
    }

    clearOutOldGarbage(&heapStats);
    size_t total = 0;
    size_t slack = 0;
    size_t baseLevel = heapStats.totalObjectSpace();
    bool testPagesAllocated = !baseLevel;
    if (testPagesAllocated)
        EXPECT_EQ(heapStats.totalAllocatedSpace(), 0ul);

    size_t big = 1008;
    Persistent<DynamicallySizedObject> bigArea = DynamicallySizedObject::create(big);
    total += big;
    slack += 4;

    size_t persistentCount = 0;
    const size_t numPersistents = 100000;
    Persistent<DynamicallySizedObject>* persistents[numPersistents];

    for (int i = 0; i < 1000; i++) {
        size_t size = 128 + i * 8;
        total += size;
        persistents[persistentCount++] = new Persistent<DynamicallySizedObject>(DynamicallySizedObject::create(size));
        slack += 4;
        getHeapStats(&heapStats);
        CheckWithSlack(baseLevel + total, heapStats.totalObjectSpace(), slack);
        if (testPagesAllocated)
            EXPECT_EQ(0ul, heapStats.totalAllocatedSpace() & (blinkPageSize - 1));
    }

    {
        DynamicallySizedObject* alloc32b(DynamicallySizedObject::create(32));
        slack += 4;
        memset(alloc32b, 40, 32);
        DynamicallySizedObject* alloc64b(DynamicallySizedObject::create(64));
        slack += 4;
        memset(alloc64b, 27, 64);
        EXPECT_TRUE(alloc32b != alloc64b);

        total += 96;
        getHeapStats(&heapStats);
        CheckWithSlack(baseLevel + total, heapStats.totalObjectSpace(), slack);
        if (testPagesAllocated)
            EXPECT_EQ(0ul, heapStats.totalAllocatedSpace() & (blinkPageSize - 1));
    }

    clearOutOldGarbage(&heapStats);
    total -= 96;
    slack -= 8;
    if (testPagesAllocated)
        EXPECT_EQ(0ul, heapStats.totalAllocatedSpace() & (blinkPageSize - 1));

    DynamicallySizedObject* bigAreaRaw = bigArea;
    // Clear the persistent, so that the big area will be garbage collected.
    bigArea.release();
    clearOutOldGarbage(&heapStats);

    total -= big;
    slack -= 4;
    getHeapStats(&heapStats);
    CheckWithSlack(baseLevel + total, heapStats.totalObjectSpace(), slack);
    if (testPagesAllocated)
        EXPECT_EQ(0ul, heapStats.totalAllocatedSpace() & (blinkPageSize - 1));

    // Endless loop unless we eventually get the memory back that we just freed.
    while (true) {
        Persistent<DynamicallySizedObject>* alloc = new Persistent<DynamicallySizedObject>(DynamicallySizedObject::create(big / 2));
        slack += 4;
        persistents[persistentCount++] = alloc;
        EXPECT_LT(persistentCount, numPersistents);
        total += big / 2;
        if (bigAreaRaw == alloc->get())
            break;
    }

    getHeapStats(&heapStats);
    CheckWithSlack(baseLevel + total, heapStats.totalObjectSpace(), slack);
    if (testPagesAllocated)
        EXPECT_EQ(0ul, heapStats.totalAllocatedSpace() & (blinkPageSize - 1));

    for (size_t i = 0; i < persistentCount; i++) {
        delete persistents[i];
        persistents[i] = 0;
    }

    uint8_t* address = reinterpret_cast<uint8_t*>(Heap::reallocate<DynamicallySizedObject>(0, 100));
    for (int i = 0; i < 100; i++)
        address[i] = i;
    address = reinterpret_cast<uint8_t*>(Heap::reallocate<DynamicallySizedObject>(address, 100000));
    for (int i = 0; i < 100; i++)
        EXPECT_EQ(address[i], i);
    address = reinterpret_cast<uint8_t*>(Heap::reallocate<DynamicallySizedObject>(address, 50));
    for (int i = 0; i < 50; i++)
        EXPECT_EQ(address[i], i);
    // This should be equivalent to free(address).
    EXPECT_EQ(reinterpret_cast<uintptr_t>(Heap::reallocate<DynamicallySizedObject>(address, 0)), 0ul);
    // This should be equivalent to malloc(0).
    EXPECT_EQ(reinterpret_cast<uintptr_t>(Heap::reallocate<DynamicallySizedObject>(0, 0)), 0ul);
}

TEST(HeapTest, SimpleAllocation)
{
    HeapStats initialHeapStats;
    clearOutOldGarbage(&initialHeapStats);
    EXPECT_EQ(0ul, initialHeapStats.totalObjectSpace());

    // Allocate an object in the heap.
    HeapAllocatedArray* array = new HeapAllocatedArray();
    HeapStats statsAfterAllocation;
    getHeapStats(&statsAfterAllocation);
    EXPECT_TRUE(statsAfterAllocation.totalObjectSpace() >= sizeof(HeapAllocatedArray));

    // Sanity check of the contents in the heap.
    EXPECT_EQ(0, array->at(0));
    EXPECT_EQ(42, array->at(42));
    EXPECT_EQ(0, array->at(128));
    EXPECT_EQ(999 % 128, array->at(999));
}

TEST(HeapTest, SimplePersistent)
{
    Persistent<TraceCounter> traceCounter = TraceCounter::create();
    EXPECT_EQ(0, traceCounter->traceCount());

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(1, traceCounter->traceCount());

    Persistent<ClassWithMember> classWithMember = ClassWithMember::create();
    EXPECT_EQ(0, classWithMember->traceCount());

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(1, classWithMember->traceCount());
    EXPECT_EQ(2, traceCounter->traceCount());
}

TEST(HeapTest, SimpleFinalization)
{
    {
        Persistent<SimpleFinalizedObject> finalized = SimpleFinalizedObject::create();
        EXPECT_EQ(0, SimpleFinalizedObject::s_destructorCalls);
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(0, SimpleFinalizedObject::s_destructorCalls);
    }

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(1, SimpleFinalizedObject::s_destructorCalls);
}

TEST(HeapTest, Finalization)
{
    {
        HeapTestSubClass* t1 = HeapTestSubClass::create();
        HeapTestSubClass* t2 = HeapTestSubClass::create();
        HeapTestSuperClass* t3 = HeapTestSuperClass::create();
        // FIXME(oilpan): Ignore unused variables.
        (void)t1;
        (void)t2;
        (void)t3;
    }
    // Nothing is marked so the GC should free everything and call
    // the finalizer on all three objects.
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(2, HeapTestSubClass::s_destructorCalls);
    EXPECT_EQ(3, HeapTestSuperClass::s_destructorCalls);
    // Destructors not called again when GCing again.
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(2, HeapTestSubClass::s_destructorCalls);
    EXPECT_EQ(3, HeapTestSuperClass::s_destructorCalls);
}

TEST(HeapTest, TypedHeapSanity)
{
    // We use TraceCounter for allocating an object on the general heap.
    Persistent<TraceCounter> generalHeapObject = TraceCounter::create();
    Persistent<TestTypedHeapClass> typedHeapObject = TestTypedHeapClass::create();
    EXPECT_NE(pageHeaderAddress(reinterpret_cast<Address>(generalHeapObject.get())),
        pageHeaderAddress(reinterpret_cast<Address>(typedHeapObject.get())));
}

TEST(HeapTest, NoAllocation)
{
    EXPECT_TRUE(ThreadState::current()->isAllocationAllowed());
    {
        // Disallow allocation
        NoAllocationScope<AnyThread> noAllocationScope;
        EXPECT_FALSE(ThreadState::current()->isAllocationAllowed());
    }
    EXPECT_TRUE(ThreadState::current()->isAllocationAllowed());
}

TEST(HeapTest, Members)
{
    Bar::s_live = 0;
    {
        Persistent<Baz> h1;
        Persistent<Baz> h2;
        {
            h1 = Baz::create(Bar::create());
            Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
            EXPECT_EQ(1u, Bar::s_live);
            h2 = Baz::create(Bar::create());
            Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
            EXPECT_EQ(2u, Bar::s_live);
        }
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(2u, Bar::s_live);
        h1->clear();
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(1u, Bar::s_live);
    }
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0u, Bar::s_live);
}

TEST(HeapTest, MarkTest)
{
    {
        Bar::s_live = 0;
        Persistent<Bar> bar = Bar::create();
        EXPECT_TRUE(ThreadState::current()->contains(bar));
        EXPECT_EQ(1u, Bar::s_live);
        {
            Foo* foo = Foo::create(bar);
            EXPECT_TRUE(ThreadState::current()->contains(foo));
            EXPECT_EQ(2u, Bar::s_live);
            EXPECT_TRUE(reinterpret_cast<Address>(foo) != reinterpret_cast<Address>(bar.get()));
            Heap::collectGarbage(ThreadState::HeapPointersOnStack);
            EXPECT_TRUE(foo != bar); // To make sure foo is kept alive.
            EXPECT_EQ(2u, Bar::s_live);
        }
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(1u, Bar::s_live);
    }
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0u, Bar::s_live);
}

TEST(HeapTest, DeepTest)
{
    const unsigned depth = 100000;
    Bar::s_live = 0;
    {
        Bar* bar = Bar::create();
        EXPECT_TRUE(ThreadState::current()->contains(bar));
        Foo* foo = Foo::create(bar);
        EXPECT_TRUE(ThreadState::current()->contains(foo));
        EXPECT_EQ(2u, Bar::s_live);
        for (unsigned i = 0; i < depth; i++) {
            Foo* foo2 = Foo::create(foo);
            foo = foo2;
            EXPECT_TRUE(ThreadState::current()->contains(foo));
        }
        EXPECT_EQ(depth + 2, Bar::s_live);
        Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        EXPECT_TRUE(foo != bar); // To make sure foo and bar are kept alive.
        EXPECT_EQ(depth + 2, Bar::s_live);
    }
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0u, Bar::s_live);
}

TEST(HeapTest, WideTest)
{
    Bar::s_live = 0;
    {
        Bars* bars = Bars::create();
        unsigned width = Bars::width;
        EXPECT_EQ(width + 1, Bar::s_live);
        Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        EXPECT_EQ(width + 1, Bar::s_live);
        // Use bars here to make sure that it will be on the stack
        // for the conservative stack scan to find.
        EXPECT_EQ(width, bars->getWidth());
    }
    EXPECT_EQ(Bars::width + 1, Bar::s_live);
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0u, Bar::s_live);
}

TEST(HeapTest, HashMapOfMembers)
{
    HeapStats initialHeapSize;
    IntWrapper::s_destructorCalls = 0;

    clearOutOldGarbage(&initialHeapSize);
    {
        typedef HeapHashMap<
            Member<IntWrapper>,
            Member<IntWrapper>,
            DefaultHash<Member<IntWrapper> >::Hash,
            HashTraits<Member<IntWrapper> >,
            HashTraits<Member<IntWrapper> > > HeapObjectIdentityMap;

        Persistent<HeapObjectIdentityMap> map = new HeapObjectIdentityMap();

        map->clear();
        HeapStats afterSetWasCreated;
        getHeapStats(&afterSetWasCreated);
        EXPECT_TRUE(afterSetWasCreated.totalObjectSpace() > initialHeapSize.totalObjectSpace());

        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        HeapStats afterGC;
        getHeapStats(&afterGC);
        EXPECT_EQ(afterGC.totalObjectSpace(), afterSetWasCreated.totalObjectSpace());

        // If the additions below cause garbage collections, these
        // pointers should be found by conservative stack scanning.
        IntWrapper* one(IntWrapper::create(1));
        IntWrapper* anotherOne(IntWrapper::create(1));

        map->add(one, one);

        HeapStats afterOneAdd;
        getHeapStats(&afterOneAdd);
        EXPECT_TRUE(afterOneAdd.totalObjectSpace() > afterGC.totalObjectSpace());

        HeapObjectIdentityMap::iterator it(map->begin());
        HeapObjectIdentityMap::iterator it2(map->begin());
        ++it;
        ++it2;

        map->add(anotherOne, one);

        // The addition above can cause an allocation of a new
        // backing store. We therefore garbage collect before
        // taking the heap stats in order to get rid of the old
        // backing store. We make sure to not use conservative
        // stack scanning as that could find a pointer to the
        // old backing.
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        HeapStats afterAddAndGC;
        getHeapStats(&afterAddAndGC);
        EXPECT_TRUE(afterAddAndGC.totalObjectSpace() >= afterOneAdd.totalObjectSpace());

        EXPECT_EQ(map->size(), 2u); // Two different wrappings of '1' are distinct.

        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_TRUE(map->contains(one));
        EXPECT_TRUE(map->contains(anotherOne));

        IntWrapper* gotten(map->get(one));
        EXPECT_EQ(gotten->value(), one->value());
        EXPECT_EQ(gotten, one);

        HeapStats afterGC2;
        getHeapStats(&afterGC2);
        EXPECT_EQ(afterGC2.totalObjectSpace(), afterAddAndGC.totalObjectSpace());

        IntWrapper* dozen = 0;

        for (int i = 1; i < 1000; i++) { // 999 iterations.
            IntWrapper* iWrapper(IntWrapper::create(i));
            IntWrapper* iSquared(IntWrapper::create(i * i));
            map->add(iWrapper, iSquared);
            if (i == 12)
                dozen = iWrapper;
        }
        HeapStats afterAdding1000;
        getHeapStats(&afterAdding1000);
        EXPECT_TRUE(afterAdding1000.totalObjectSpace() > afterGC2.totalObjectSpace());

        IntWrapper* gross(map->get(dozen));
        EXPECT_EQ(gross->value(), 144);

        // This should clear out junk created by all the adds.
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        HeapStats afterGC3;
        getHeapStats(&afterGC3);
        EXPECT_TRUE(afterGC3.totalObjectSpace() < afterAdding1000.totalObjectSpace());
    }

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    // The objects 'one', anotherOne, and the 999 other pairs.
    EXPECT_EQ(IntWrapper::s_destructorCalls, 2000);
    HeapStats afterGC4;
    getHeapStats(&afterGC4);
    EXPECT_EQ(afterGC4.totalObjectSpace(), initialHeapSize.totalObjectSpace());
}

TEST(HeapTest, NestedAllocation)
{
    HeapStats initialHeapSize;
    clearOutOldGarbage(&initialHeapSize);
    {
        Persistent<ConstructorAllocation> constructorAllocation = ConstructorAllocation::create();
    }
    HeapStats afterFree;
    clearOutOldGarbage(&afterFree);
    EXPECT_TRUE(initialHeapSize == afterFree);
}

TEST(HeapTest, LargeObjects)
{
    HeapStats initialHeapSize;
    clearOutOldGarbage(&initialHeapSize);
    IntWrapper::s_destructorCalls = 0;
    LargeObject::s_destructorCalls = 0;
    {
        int slack = 8; // LargeObject points to an IntWrapper that is also allocated.
        Persistent<LargeObject> object = LargeObject::create();
        EXPECT_TRUE(ThreadState::current()->contains(object));
        EXPECT_TRUE(ThreadState::current()->contains(reinterpret_cast<char*>(object.get()) + sizeof(LargeObject) - 1));
#if ENABLE(GC_TRACING)
        const GCInfo* info = ThreadState::current()->findGCInfo(reinterpret_cast<Address>(object.get()));
        EXPECT_NE(reinterpret_cast<const GCInfo*>(0), info);
        EXPECT_EQ(info, ThreadState::current()->findGCInfo(reinterpret_cast<Address>(object.get()) + sizeof(LargeObject) - 1));
        EXPECT_NE(info, ThreadState::current()->findGCInfo(reinterpret_cast<Address>(object.get()) + sizeof(LargeObject)));
        EXPECT_NE(info, ThreadState::current()->findGCInfo(reinterpret_cast<Address>(object.get()) - 1));
#endif
        HeapStats afterAllocation;
        clearOutOldGarbage(&afterAllocation);
        {
            object->set(0, 'a');
            EXPECT_EQ('a', object->get(0));
            object->set(object->length() - 1, 'b');
            EXPECT_EQ('b', object->get(object->length() - 1));
            size_t expectedObjectSpace = sizeof(LargeObject) + sizeof(IntWrapper);
            size_t actualObjectSpace =
                afterAllocation.totalObjectSpace() - initialHeapSize.totalObjectSpace();
            CheckWithSlack(expectedObjectSpace, actualObjectSpace, slack);
            // There is probably space for the IntWrapper in a heap page without
            // allocating extra pages. However, the IntWrapper allocation might cause
            // the addition of a heap page.
            size_t largeObjectAllocationSize =
                sizeof(LargeObject) + sizeof(LargeHeapObject<FinalizedHeapObjectHeader>) + sizeof(FinalizedHeapObjectHeader);
            size_t allocatedSpaceLowerBound =
                initialHeapSize.totalAllocatedSpace() + largeObjectAllocationSize;
            size_t allocatedSpaceUpperBound = allocatedSpaceLowerBound + slack + blinkPageSize;
            EXPECT_LE(allocatedSpaceLowerBound, afterAllocation.totalAllocatedSpace());
            EXPECT_LE(afterAllocation.totalAllocatedSpace(), allocatedSpaceUpperBound);
            EXPECT_EQ(0, IntWrapper::s_destructorCalls);
            EXPECT_EQ(0, LargeObject::s_destructorCalls);
            for (int i = 0; i < 10; i++)
                object = LargeObject::create();
        }
        HeapStats oneLargeObject;
        clearOutOldGarbage(&oneLargeObject);
        EXPECT_TRUE(oneLargeObject == afterAllocation);
        EXPECT_EQ(10, IntWrapper::s_destructorCalls);
        EXPECT_EQ(10, LargeObject::s_destructorCalls);
    }
    HeapStats backToInitial;
    clearOutOldGarbage(&backToInitial);
    EXPECT_TRUE(initialHeapSize == backToInitial);
    EXPECT_EQ(11, IntWrapper::s_destructorCalls);
    EXPECT_EQ(11, LargeObject::s_destructorCalls);
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
}

typedef std::pair<Member<IntWrapper>, int> PairWrappedUnwrapped;
typedef std::pair<int, Member<IntWrapper> > PairUnwrappedWrapped;
typedef std::pair<WeakMember<IntWrapper>, Member<IntWrapper> > PairWeakStrong;
typedef std::pair<Member<IntWrapper>, WeakMember<IntWrapper> > PairStrongWeak;
typedef std::pair<WeakMember<IntWrapper>, int> PairWeakUnwrapped;
typedef std::pair<int, WeakMember<IntWrapper> > PairUnwrappedWeak;

class Container : public GarbageCollected<Container> {
public:
    static Container* create() { return new Container(); }
    HeapHashMap<Member<IntWrapper>, Member<IntWrapper> > map;
    HeapHashSet<Member<IntWrapper> > set;
    HeapHashSet<Member<IntWrapper> > set2;
    HeapHashCountedSet<Member<IntWrapper> > set3;
    HeapVector<Member<IntWrapper>, 2> vector;
    HeapVector<PairWrappedUnwrapped, 2> vectorWU;
    HeapVector<PairUnwrappedWrapped, 2> vectorUW;
    HeapDeque<Member<IntWrapper>, 0> deque;
    HeapDeque<PairWrappedUnwrapped, 0> dequeWU;
    HeapDeque<PairUnwrappedWrapped, 0> dequeUW;
    void trace(Visitor* visitor)
    {
        visitor->trace(map);
        visitor->trace(set);
        visitor->trace(set2);
        visitor->trace(set3);
        visitor->trace(vector);
        visitor->trace(vectorWU);
        visitor->trace(vectorUW);
        visitor->trace(deque);
        visitor->trace(dequeWU);
        visitor->trace(dequeUW);
    }
};

struct ShouldBeTraced {
    explicit ShouldBeTraced(IntWrapper* wrapper) : m_wrapper(wrapper) { }
    void trace(Visitor* visitor) { visitor->trace(m_wrapper); }
    Member<IntWrapper> m_wrapper;
};

class OffHeapContainer : public GarbageCollectedFinalized<OffHeapContainer> {
public:
    static OffHeapContainer* create() { return new OffHeapContainer(); }

    static const int iterations = 300;
    static const int deadWrappers = 2700;

    OffHeapContainer()
    {
        for (int i = 0; i < iterations; i++) {
            m_deque1.append(ShouldBeTraced(IntWrapper::create(i)));
            m_vector1.append(ShouldBeTraced(IntWrapper::create(i)));
            m_deque2.append(IntWrapper::create(i));
            m_vector2.append(IntWrapper::create(i));
            m_hashSet.add(IntWrapper::create(i));
            m_hashMap.add(i + 103, IntWrapper::create(i));
            m_listHashSet.add(IntWrapper::create(i));
            m_linkedHashSet.add(IntWrapper::create(i));
            m_ownedVector.append(adoptPtr(new ShouldBeTraced(IntWrapper::create(i))));
        }

        Deque<ShouldBeTraced>::iterator d1Iterator(m_deque1.begin());
        Vector<ShouldBeTraced>::iterator v1Iterator(m_vector1.begin());
        Deque<Member<IntWrapper> >::iterator d2Iterator(m_deque2.begin());
        Vector<Member<IntWrapper> >::iterator v2Iterator(m_vector2.begin());
        HashSet<Member<IntWrapper> >::iterator setIterator(m_hashSet.begin());
        HashMap<int, Member<IntWrapper> >::iterator mapIterator(m_hashMap.begin());
        ListHashSet<Member<IntWrapper> >::iterator listSetIterator(m_listHashSet.begin());
        LinkedHashSet<Member<IntWrapper> >::iterator linkedSetIterator(m_linkedHashSet.begin());
        Vector<OwnPtr<ShouldBeTraced> >::iterator ownedVectorIterator(m_ownedVector.begin());

        for (int i = 0; i < iterations; i++) {
            EXPECT_EQ(i, m_vector1[i].m_wrapper->value());
            EXPECT_EQ(i, m_vector2[i]->value());
            EXPECT_EQ(i, d1Iterator->m_wrapper->value());
            EXPECT_EQ(i, v1Iterator->m_wrapper->value());
            EXPECT_EQ(i, d2Iterator->get()->value());
            EXPECT_EQ(i, v2Iterator->get()->value());
            EXPECT_EQ(i, linkedSetIterator->get()->value());
            EXPECT_EQ(i, listSetIterator->get()->value());
            EXPECT_EQ(i, ownedVectorIterator->get()->m_wrapper->value());
            int value = setIterator->get()->value();
            EXPECT_LE(0, value);
            EXPECT_GT(iterations, value);
            value = mapIterator->value.get()->value();
            EXPECT_LE(0, value);
            EXPECT_GT(iterations, value);
            ++d1Iterator;
            ++v1Iterator;
            ++d2Iterator;
            ++v2Iterator;
            ++setIterator;
            ++mapIterator;
            ++listSetIterator;
            ++linkedSetIterator;
            ++ownedVectorIterator;
        }
        EXPECT_EQ(d1Iterator, m_deque1.end());
        EXPECT_EQ(v1Iterator, m_vector1.end());
        EXPECT_EQ(d2Iterator, m_deque2.end());
        EXPECT_EQ(v2Iterator, m_vector2.end());
        EXPECT_EQ(setIterator, m_hashSet.end());
        EXPECT_EQ(mapIterator, m_hashMap.end());
        EXPECT_EQ(listSetIterator, m_listHashSet.end());
        EXPECT_EQ(linkedSetIterator, m_linkedHashSet.end());
        EXPECT_EQ(ownedVectorIterator, m_ownedVector.end());
    }

    void trace(Visitor* visitor)
    {
        visitor->trace(m_deque1);
        visitor->trace(m_vector1);
        visitor->trace(m_deque2);
        visitor->trace(m_vector2);
        visitor->trace(m_hashSet);
        visitor->trace(m_hashMap);
        visitor->trace(m_listHashSet);
        visitor->trace(m_linkedHashSet);
        visitor->trace(m_listHashSet);
        visitor->trace(m_ownedVector);
    }

    Deque<ShouldBeTraced> m_deque1;
    Vector<ShouldBeTraced> m_vector1;
    Deque<Member<IntWrapper> > m_deque2;
    Vector<Member<IntWrapper> > m_vector2;
    HashSet<Member<IntWrapper> > m_hashSet;
    HashMap<int, Member<IntWrapper> > m_hashMap;
    ListHashSet<Member<IntWrapper> > m_listHashSet;
    LinkedHashSet<Member<IntWrapper> > m_linkedHashSet;
    Vector<OwnPtr<ShouldBeTraced> > m_ownedVector;
};

const int OffHeapContainer::iterations;
const int OffHeapContainer::deadWrappers;

// These class definitions test compile-time asserts with transition
// types. They are therefore unused in test code and just need to
// compile. This is intentional; do not delete the A and B classes below.
class A : public WillBeGarbageCollectedMixin {
};

class B : public NoBaseWillBeGarbageCollected<B>, public A {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(B);
public:
    void trace(Visitor*) { }
};

TEST(HeapTest, HeapVectorFilledWithValue)
{
    IntWrapper* val = IntWrapper::create(1);
    HeapVector<Member<IntWrapper> > vector(10, val);
    EXPECT_EQ(10u, vector.size());
    for (size_t i = 0; i < vector.size(); i++)
        EXPECT_EQ(val, vector[i]);
}

TEST(HeapTest, HeapVectorWithInlineCapacity)
{
    IntWrapper* one = IntWrapper::create(1);
    IntWrapper* two = IntWrapper::create(2);
    IntWrapper* three = IntWrapper::create(3);
    IntWrapper* four = IntWrapper::create(4);
    IntWrapper* five = IntWrapper::create(5);
    IntWrapper* six = IntWrapper::create(6);
    {
        HeapVector<Member<IntWrapper>, 2> vector;
        vector.append(one);
        vector.append(two);
        Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        EXPECT_TRUE(vector.contains(one));
        EXPECT_TRUE(vector.contains(two));

        vector.append(three);
        vector.append(four);
        Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        EXPECT_TRUE(vector.contains(one));
        EXPECT_TRUE(vector.contains(two));
        EXPECT_TRUE(vector.contains(three));
        EXPECT_TRUE(vector.contains(four));

        vector.shrink(1);
        Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        EXPECT_TRUE(vector.contains(one));
        EXPECT_FALSE(vector.contains(two));
        EXPECT_FALSE(vector.contains(three));
        EXPECT_FALSE(vector.contains(four));
    }
    {
        HeapVector<Member<IntWrapper>, 2> vector1;
        HeapVector<Member<IntWrapper>, 2> vector2;

        vector1.append(one);
        vector2.append(two);
        vector1.swap(vector2);
        Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        EXPECT_TRUE(vector1.contains(two));
        EXPECT_TRUE(vector2.contains(one));
    }
    {
        HeapVector<Member<IntWrapper>, 2> vector1;
        HeapVector<Member<IntWrapper>, 2> vector2;

        vector1.append(one);
        vector1.append(two);
        vector2.append(three);
        vector2.append(four);
        vector2.append(five);
        vector2.append(six);
        vector1.swap(vector2);
        Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        EXPECT_TRUE(vector1.contains(three));
        EXPECT_TRUE(vector1.contains(four));
        EXPECT_TRUE(vector1.contains(five));
        EXPECT_TRUE(vector1.contains(six));
        EXPECT_TRUE(vector2.contains(one));
        EXPECT_TRUE(vector2.contains(two));
    }
}

template<typename T, size_t inlineCapacity, typename U>
bool dequeContains(HeapDeque<T, inlineCapacity>& deque, U u)
{
    typedef typename HeapDeque<T, inlineCapacity>::iterator iterator;
    for (iterator it = deque.begin(); it != deque.end(); ++it) {
        if (*it == u)
            return true;
    }
    return false;
}

TEST(HeapTest, HeapCollectionTypes)
{
    HeapStats initialHeapSize;
    IntWrapper::s_destructorCalls = 0;

    typedef HeapHashMap<Member<IntWrapper>, Member<IntWrapper> > MemberMember;
    typedef HeapHashMap<Member<IntWrapper>, int> MemberPrimitive;
    typedef HeapHashMap<int, Member<IntWrapper> > PrimitiveMember;

    typedef HeapHashSet<Member<IntWrapper> > MemberSet;
    typedef HeapHashCountedSet<Member<IntWrapper> > MemberCountedSet;

    typedef HeapVector<Member<IntWrapper>, 2> MemberVector;
    typedef HeapDeque<Member<IntWrapper>, 0> MemberDeque;

    typedef HeapVector<PairWrappedUnwrapped, 2> VectorWU;
    typedef HeapVector<PairUnwrappedWrapped, 2> VectorUW;
    typedef HeapDeque<PairWrappedUnwrapped, 0> DequeWU;
    typedef HeapDeque<PairUnwrappedWrapped, 0> DequeUW;

    Persistent<MemberMember> memberMember = new MemberMember();
    Persistent<MemberMember> memberMember2 = new MemberMember();
    Persistent<MemberMember> memberMember3 = new MemberMember();
    Persistent<MemberPrimitive> memberPrimitive = new MemberPrimitive();
    Persistent<PrimitiveMember> primitiveMember = new PrimitiveMember();
    Persistent<MemberSet> set = new MemberSet();
    Persistent<MemberSet> set2 = new MemberSet();
    Persistent<MemberCountedSet> set3 = new MemberCountedSet();
    Persistent<MemberVector> vector = new MemberVector();
    Persistent<MemberVector> vector2 = new MemberVector();
    Persistent<VectorWU> vectorWU = new VectorWU();
    Persistent<VectorWU> vectorWU2 = new VectorWU();
    Persistent<VectorUW> vectorUW = new VectorUW();
    Persistent<VectorUW> vectorUW2 = new VectorUW();
    Persistent<MemberDeque> deque = new MemberDeque();
    Persistent<MemberDeque> deque2 = new MemberDeque();
    Persistent<DequeWU> dequeWU = new DequeWU();
    Persistent<DequeWU> dequeWU2 = new DequeWU();
    Persistent<DequeUW> dequeUW = new DequeUW();
    Persistent<DequeUW> dequeUW2 = new DequeUW();
    Persistent<Container> container = Container::create();

    clearOutOldGarbage(&initialHeapSize);
    {
        Persistent<IntWrapper> one(IntWrapper::create(1));
        Persistent<IntWrapper> two(IntWrapper::create(2));
        Persistent<IntWrapper> oneB(IntWrapper::create(1));
        Persistent<IntWrapper> twoB(IntWrapper::create(2));
        Persistent<IntWrapper> oneC(IntWrapper::create(1));
        Persistent<IntWrapper> oneD(IntWrapper::create(1));
        Persistent<IntWrapper> oneE(IntWrapper::create(1));
        Persistent<IntWrapper> oneF(IntWrapper::create(1));
        {
            IntWrapper* threeB(IntWrapper::create(3));
            IntWrapper* threeC(IntWrapper::create(3));
            IntWrapper* threeD(IntWrapper::create(3));
            IntWrapper* threeE(IntWrapper::create(3));
            IntWrapper* threeF(IntWrapper::create(3));
            IntWrapper* three(IntWrapper::create(3));
            IntWrapper* fourB(IntWrapper::create(4));
            IntWrapper* fourC(IntWrapper::create(4));
            IntWrapper* fourD(IntWrapper::create(4));
            IntWrapper* fourE(IntWrapper::create(4));
            IntWrapper* fourF(IntWrapper::create(4));
            IntWrapper* four(IntWrapper::create(4));
            IntWrapper* fiveC(IntWrapper::create(5));
            IntWrapper* fiveD(IntWrapper::create(5));
            IntWrapper* fiveE(IntWrapper::create(5));
            IntWrapper* fiveF(IntWrapper::create(5));

            // Member Collections.
            memberMember2->add(one, two);
            memberMember2->add(two, three);
            memberMember2->add(three, four);
            memberMember2->add(four, one);
            primitiveMember->add(1, two);
            primitiveMember->add(2, three);
            primitiveMember->add(3, four);
            primitiveMember->add(4, one);
            memberPrimitive->add(one, 2);
            memberPrimitive->add(two, 3);
            memberPrimitive->add(three, 4);
            memberPrimitive->add(four, 1);
            set2->add(one);
            set2->add(two);
            set2->add(three);
            set2->add(four);
            set->add(oneB);
            set3->add(oneB);
            set3->add(oneB);
            vector->append(oneB);
            deque->append(oneB);
            vector2->append(threeB);
            vector2->append(fourB);
            deque2->append(threeE);
            deque2->append(fourE);
            vectorWU->append(PairWrappedUnwrapped(&*oneC, 42));
            dequeWU->append(PairWrappedUnwrapped(&*oneE, 42));
            vectorWU2->append(PairWrappedUnwrapped(&*threeC, 43));
            vectorWU2->append(PairWrappedUnwrapped(&*fourC, 44));
            vectorWU2->append(PairWrappedUnwrapped(&*fiveC, 45));
            dequeWU2->append(PairWrappedUnwrapped(&*threeE, 43));
            dequeWU2->append(PairWrappedUnwrapped(&*fourE, 44));
            dequeWU2->append(PairWrappedUnwrapped(&*fiveE, 45));
            vectorUW->append(PairUnwrappedWrapped(1, &*oneD));
            vectorUW2->append(PairUnwrappedWrapped(103, &*threeD));
            vectorUW2->append(PairUnwrappedWrapped(104, &*fourD));
            vectorUW2->append(PairUnwrappedWrapped(105, &*fiveD));
            dequeUW->append(PairUnwrappedWrapped(1, &*oneF));
            dequeUW2->append(PairUnwrappedWrapped(103, &*threeF));
            dequeUW2->append(PairUnwrappedWrapped(104, &*fourF));
            dequeUW2->append(PairUnwrappedWrapped(105, &*fiveF));

            EXPECT_TRUE(dequeContains(*deque, oneB));

            // Collect garbage. This should change nothing since we are keeping
            // alive the IntWrapper objects with on-stack pointers.
            Heap::collectGarbage(ThreadState::HeapPointersOnStack);

            EXPECT_TRUE(dequeContains(*deque, oneB));

            EXPECT_EQ(0u, memberMember->size());
            EXPECT_EQ(4u, memberMember2->size());
            EXPECT_EQ(4u, primitiveMember->size());
            EXPECT_EQ(4u, memberPrimitive->size());
            EXPECT_EQ(1u, set->size());
            EXPECT_EQ(4u, set2->size());
            EXPECT_EQ(1u, set3->size());
            EXPECT_EQ(1u, vector->size());
            EXPECT_EQ(2u, vector2->size());
            EXPECT_EQ(1u, vectorWU->size());
            EXPECT_EQ(3u, vectorWU2->size());
            EXPECT_EQ(1u, vectorUW->size());
            EXPECT_EQ(3u, vectorUW2->size());
            EXPECT_EQ(1u, deque->size());
            EXPECT_EQ(2u, deque2->size());
            EXPECT_EQ(1u, dequeWU->size());
            EXPECT_EQ(3u, dequeWU2->size());
            EXPECT_EQ(1u, dequeUW->size());
            EXPECT_EQ(3u, dequeUW2->size());

            MemberVector& cvec = container->vector;
            cvec.swap(*vector.get());
            vector2->swap(cvec);
            vector->swap(cvec);

            VectorWU& cvecWU = container->vectorWU;
            cvecWU.swap(*vectorWU.get());
            vectorWU2->swap(cvecWU);
            vectorWU->swap(cvecWU);

            VectorUW& cvecUW = container->vectorUW;
            cvecUW.swap(*vectorUW.get());
            vectorUW2->swap(cvecUW);
            vectorUW->swap(cvecUW);

            MemberDeque& cDeque = container->deque;
            cDeque.swap(*deque.get());
            deque2->swap(cDeque);
            deque->swap(cDeque);

            DequeWU& cDequeWU = container->dequeWU;
            cDequeWU.swap(*dequeWU.get());
            dequeWU2->swap(cDequeWU);
            dequeWU->swap(cDequeWU);

            DequeUW& cDequeUW = container->dequeUW;
            cDequeUW.swap(*dequeUW.get());
            dequeUW2->swap(cDequeUW);
            dequeUW->swap(cDequeUW);

            // Swap set and set2 in a roundabout way.
            MemberSet& cset1 = container->set;
            MemberSet& cset2 = container->set2;
            set->swap(cset1);
            set2->swap(cset2);
            set->swap(cset2);
            cset1.swap(cset2);
            cset2.swap(set2);

            MemberCountedSet& cCountedSet = container->set3;
            set3->swap(cCountedSet);
            EXPECT_EQ(0u, set3->size());
            set3->swap(cCountedSet);

            // Triple swap.
            container->map.swap(memberMember2);
            MemberMember& containedMap = container->map;
            memberMember3->swap(containedMap);
            memberMember3->swap(memberMember);

            EXPECT_TRUE(memberMember->get(one) == two);
            EXPECT_TRUE(memberMember->get(two) == three);
            EXPECT_TRUE(memberMember->get(three) == four);
            EXPECT_TRUE(memberMember->get(four) == one);
            EXPECT_TRUE(primitiveMember->get(1) == two);
            EXPECT_TRUE(primitiveMember->get(2) == three);
            EXPECT_TRUE(primitiveMember->get(3) == four);
            EXPECT_TRUE(primitiveMember->get(4) == one);
            EXPECT_EQ(1, memberPrimitive->get(four));
            EXPECT_EQ(2, memberPrimitive->get(one));
            EXPECT_EQ(3, memberPrimitive->get(two));
            EXPECT_EQ(4, memberPrimitive->get(three));
            EXPECT_TRUE(set->contains(one));
            EXPECT_TRUE(set->contains(two));
            EXPECT_TRUE(set->contains(three));
            EXPECT_TRUE(set->contains(four));
            EXPECT_TRUE(set2->contains(oneB));
            EXPECT_TRUE(set3->contains(oneB));
            EXPECT_TRUE(vector->contains(threeB));
            EXPECT_TRUE(vector->contains(fourB));
            EXPECT_TRUE(dequeContains(*deque, threeE));
            EXPECT_TRUE(dequeContains(*deque, fourE));
            EXPECT_TRUE(vector2->contains(oneB));
            EXPECT_FALSE(vector2->contains(threeB));
            EXPECT_TRUE(dequeContains(*deque2, oneB));
            EXPECT_FALSE(dequeContains(*deque2, threeE));
            EXPECT_TRUE(vectorWU->contains(PairWrappedUnwrapped(&*threeC, 43)));
            EXPECT_TRUE(vectorWU->contains(PairWrappedUnwrapped(&*fourC, 44)));
            EXPECT_TRUE(vectorWU->contains(PairWrappedUnwrapped(&*fiveC, 45)));
            EXPECT_TRUE(vectorWU2->contains(PairWrappedUnwrapped(&*oneC, 42)));
            EXPECT_FALSE(vectorWU2->contains(PairWrappedUnwrapped(&*threeC, 43)));
            EXPECT_TRUE(vectorUW->contains(PairUnwrappedWrapped(103, &*threeD)));
            EXPECT_TRUE(vectorUW->contains(PairUnwrappedWrapped(104, &*fourD)));
            EXPECT_TRUE(vectorUW->contains(PairUnwrappedWrapped(105, &*fiveD)));
            EXPECT_TRUE(vectorUW2->contains(PairUnwrappedWrapped(1, &*oneD)));
            EXPECT_FALSE(vectorUW2->contains(PairUnwrappedWrapped(103, &*threeD)));
            EXPECT_TRUE(dequeContains(*dequeWU, PairWrappedUnwrapped(&*threeE, 43)));
            EXPECT_TRUE(dequeContains(*dequeWU, PairWrappedUnwrapped(&*fourE, 44)));
            EXPECT_TRUE(dequeContains(*dequeWU, PairWrappedUnwrapped(&*fiveE, 45)));
            EXPECT_TRUE(dequeContains(*dequeWU2, PairWrappedUnwrapped(&*oneE, 42)));
            EXPECT_FALSE(dequeContains(*dequeWU2, PairWrappedUnwrapped(&*threeE, 43)));
            EXPECT_TRUE(dequeContains(*dequeUW, PairUnwrappedWrapped(103, &*threeF)));
            EXPECT_TRUE(dequeContains(*dequeUW, PairUnwrappedWrapped(104, &*fourF)));
            EXPECT_TRUE(dequeContains(*dequeUW, PairUnwrappedWrapped(105, &*fiveF)));
            EXPECT_TRUE(dequeContains(*dequeUW2, PairUnwrappedWrapped(1, &*oneF)));
            EXPECT_FALSE(dequeContains(*dequeUW2, PairUnwrappedWrapped(103, &*threeF)));
        }

        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);

        EXPECT_EQ(4u, memberMember->size());
        EXPECT_EQ(0u, memberMember2->size());
        EXPECT_EQ(4u, primitiveMember->size());
        EXPECT_EQ(4u, memberPrimitive->size());
        EXPECT_EQ(4u, set->size());
        EXPECT_EQ(1u, set2->size());
        EXPECT_EQ(1u, set3->size());
        EXPECT_EQ(2u, vector->size());
        EXPECT_EQ(1u, vector2->size());
        EXPECT_EQ(3u, vectorUW->size());
        EXPECT_EQ(1u, vector2->size());
        EXPECT_EQ(2u, deque->size());
        EXPECT_EQ(1u, deque2->size());
        EXPECT_EQ(3u, dequeUW->size());
        EXPECT_EQ(1u, deque2->size());

        EXPECT_TRUE(memberMember->get(one) == two);
        EXPECT_TRUE(primitiveMember->get(1) == two);
        EXPECT_TRUE(primitiveMember->get(4) == one);
        EXPECT_EQ(2, memberPrimitive->get(one));
        EXPECT_EQ(3, memberPrimitive->get(two));
        EXPECT_TRUE(set->contains(one));
        EXPECT_TRUE(set->contains(two));
        EXPECT_FALSE(set->contains(oneB));
        EXPECT_TRUE(set2->contains(oneB));
        EXPECT_TRUE(set3->contains(oneB));
        EXPECT_EQ(2u, set3->find(oneB)->value);
        EXPECT_EQ(3, vector->at(0)->value());
        EXPECT_EQ(4, vector->at(1)->value());
        EXPECT_EQ(3, deque->begin()->get()->value());
    }

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);

    EXPECT_EQ(4u, memberMember->size());
    EXPECT_EQ(4u, primitiveMember->size());
    EXPECT_EQ(4u, memberPrimitive->size());
    EXPECT_EQ(4u, set->size());
    EXPECT_EQ(1u, set2->size());
    EXPECT_EQ(2u, vector->size());
    EXPECT_EQ(1u, vector2->size());
    EXPECT_EQ(3u, vectorWU->size());
    EXPECT_EQ(1u, vectorWU2->size());
    EXPECT_EQ(3u, vectorUW->size());
    EXPECT_EQ(1u, vectorUW2->size());
    EXPECT_EQ(2u, deque->size());
    EXPECT_EQ(1u, deque2->size());
    EXPECT_EQ(3u, dequeWU->size());
    EXPECT_EQ(1u, dequeWU2->size());
    EXPECT_EQ(3u, dequeUW->size());
    EXPECT_EQ(1u, dequeUW2->size());
}

template<typename T>
void MapIteratorCheck(T& it, const T& end, int expected)
{
    int found = 0;
    while (it != end) {
        found++;
        int key = it->key->value();
        int value = it->value->value();
        EXPECT_TRUE(key >= 0 && key < 1100);
        EXPECT_TRUE(value >= 0 && value < 1100);
        ++it;
    }
    EXPECT_EQ(expected, found);
}

template<typename T>
void SetIteratorCheck(T& it, const T& end, int expected)
{
    int found = 0;
    while (it != end) {
        found++;
        int value = (*it)->value();
        EXPECT_TRUE(value >= 0 && value < 1100);
        ++it;
    }
    EXPECT_EQ(expected, found);
}

TEST(HeapTest, HeapWeakCollectionSimple)
{
    HeapStats initialHeapStats;
    clearOutOldGarbage(&initialHeapStats);
    IntWrapper::s_destructorCalls = 0;

    PersistentHeapVector<Member<IntWrapper> > keepNumbersAlive;

    typedef HeapHashMap<WeakMember<IntWrapper>, Member<IntWrapper> > WeakStrong;
    typedef HeapHashMap<Member<IntWrapper>, WeakMember<IntWrapper> > StrongWeak;
    typedef HeapHashMap<WeakMember<IntWrapper>, WeakMember<IntWrapper> > WeakWeak;
    typedef HeapHashSet<WeakMember<IntWrapper> > WeakSet;
    typedef HeapHashCountedSet<WeakMember<IntWrapper> > WeakCountedSet;

    Persistent<WeakStrong> weakStrong = new WeakStrong();
    Persistent<StrongWeak> strongWeak = new StrongWeak();
    Persistent<WeakWeak> weakWeak = new WeakWeak();
    Persistent<WeakSet> weakSet = new WeakSet();
    Persistent<WeakCountedSet> weakCountedSet = new WeakCountedSet();

    Persistent<IntWrapper> two = IntWrapper::create(2);

    keepNumbersAlive.append(IntWrapper::create(103));
    keepNumbersAlive.append(IntWrapper::create(10));

    {
        weakStrong->add(IntWrapper::create(1), two);
        strongWeak->add(two, IntWrapper::create(1));
        weakWeak->add(two, IntWrapper::create(42));
        weakWeak->add(IntWrapper::create(42), two);
        weakSet->add(IntWrapper::create(0));
        weakSet->add(two);
        weakSet->add(keepNumbersAlive[0]);
        weakSet->add(keepNumbersAlive[1]);
        weakCountedSet->add(IntWrapper::create(0));
        weakCountedSet->add(two);
        weakCountedSet->add(two);
        weakCountedSet->add(two);
        weakCountedSet->add(keepNumbersAlive[0]);
        weakCountedSet->add(keepNumbersAlive[1]);
        EXPECT_EQ(1u, weakStrong->size());
        EXPECT_EQ(1u, strongWeak->size());
        EXPECT_EQ(2u, weakWeak->size());
        EXPECT_EQ(4u, weakSet->size());
        EXPECT_EQ(4u, weakCountedSet->size());
        EXPECT_EQ(3u, weakCountedSet->find(two)->value);
        weakCountedSet->remove(two);
        EXPECT_EQ(2u, weakCountedSet->find(two)->value);
    }

    keepNumbersAlive[0] = nullptr;

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);

    EXPECT_EQ(0u, weakStrong->size());
    EXPECT_EQ(0u, strongWeak->size());
    EXPECT_EQ(0u, weakWeak->size());
    EXPECT_EQ(2u, weakSet->size());
    EXPECT_EQ(2u, weakCountedSet->size());
}

template<typename Set>
void orderedSetHelper(bool strong)
{
    HeapStats initialHeapStats;
    clearOutOldGarbage(&initialHeapStats);
    IntWrapper::s_destructorCalls = 0;

    PersistentHeapVector<Member<IntWrapper> > keepNumbersAlive;

    Persistent<Set> set1 = new Set();
    Persistent<Set> set2 = new Set();

    const Set& constSet = *set1.get();

    keepNumbersAlive.append(IntWrapper::create(2));
    keepNumbersAlive.append(IntWrapper::create(103));
    keepNumbersAlive.append(IntWrapper::create(10));

    set1->add(IntWrapper::create(0));
    set1->add(keepNumbersAlive[0]);
    set1->add(keepNumbersAlive[1]);
    set1->add(keepNumbersAlive[2]);

    set2->clear();
    set2->add(IntWrapper::create(42));
    set2->clear();

    EXPECT_EQ(4u, set1->size());
    typename Set::iterator it(set1->begin());
    typename Set::reverse_iterator reverse(set1->rbegin());
    typename Set::const_iterator cit(constSet.begin());
    typename Set::const_reverse_iterator creverse(constSet.rbegin());

    EXPECT_EQ(0, (*it)->value());
    EXPECT_EQ(0, (*cit)->value());
    ++it;
    ++cit;
    EXPECT_EQ(2, (*it)->value());
    EXPECT_EQ(2, (*cit)->value());
    --it;
    --cit;
    EXPECT_EQ(0, (*it)->value());
    EXPECT_EQ(0, (*cit)->value());
    ++it;
    ++cit;
    ++it;
    ++cit;
    EXPECT_EQ(103, (*it)->value());
    EXPECT_EQ(103, (*cit)->value());
    ++it;
    ++cit;
    EXPECT_EQ(10, (*it)->value());
    EXPECT_EQ(10, (*cit)->value());
    ++it;
    ++cit;

    EXPECT_EQ(10, (*reverse)->value());
    EXPECT_EQ(10, (*creverse)->value());
    ++reverse;
    ++creverse;
    EXPECT_EQ(103, (*reverse)->value());
    EXPECT_EQ(103, (*creverse)->value());
    --reverse;
    --creverse;
    EXPECT_EQ(10, (*reverse)->value());
    EXPECT_EQ(10, (*creverse)->value());
    ++reverse;
    ++creverse;
    ++reverse;
    ++creverse;
    EXPECT_EQ(2, (*reverse)->value());
    EXPECT_EQ(2, (*creverse)->value());
    ++reverse;
    ++creverse;
    EXPECT_EQ(0, (*reverse)->value());
    EXPECT_EQ(0, (*creverse)->value());
    ++reverse;
    ++creverse;

    EXPECT_EQ(set1->end(), it);
    EXPECT_EQ(constSet.end(), cit);
    EXPECT_EQ(set1->rend(), reverse);
    EXPECT_EQ(constSet.rend(), creverse);

    typename Set::iterator iX(set2->begin());
    EXPECT_EQ(set2->end(), iX);

    if (strong)
        set1->remove(keepNumbersAlive[0]);

    keepNumbersAlive[0] = nullptr;

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);

    EXPECT_EQ(2u + (strong ? 1u : 0u), set1->size());

    EXPECT_EQ(2 + (strong ? 0 : 1), IntWrapper::s_destructorCalls);

    typename Set::iterator i2(set1->begin());
    if (strong) {
        EXPECT_EQ(0, (*i2)->value());
        ++i2;
        EXPECT_NE(set1->end(), i2);
    }
    EXPECT_EQ(103, (*i2)->value());
    ++i2;
    EXPECT_NE(set1->end(), i2);
    EXPECT_EQ(10, (*i2)->value());
    ++i2;
    EXPECT_EQ(set1->end(), i2);
}

TEST(HeapTest, HeapWeakLinkedHashSet)
{
    orderedSetHelper<HeapLinkedHashSet<Member<IntWrapper> > >(true);
    orderedSetHelper<HeapLinkedHashSet<WeakMember<IntWrapper> > >(false);
    orderedSetHelper<HeapListHashSet<Member<IntWrapper> > >(true);
}

class ThingWithDestructor {
public:
    ThingWithDestructor()
        : m_x(emptyValue)
    {
        s_liveThingsWithDestructor++;
    }

    ThingWithDestructor(int x)
        : m_x(x)
    {
        s_liveThingsWithDestructor++;
    }

    ThingWithDestructor(const ThingWithDestructor&other)
    {
        *this = other;
        s_liveThingsWithDestructor++;
    }

    ~ThingWithDestructor()
    {
        s_liveThingsWithDestructor--;
    }

    int value() { return m_x; }

    static int s_liveThingsWithDestructor;

    unsigned hash() { return IntHash<int>::hash(m_x); }

private:
    static const int emptyValue = 0;
    int m_x;
};

int ThingWithDestructor::s_liveThingsWithDestructor;

struct ThingWithDestructorTraits : public HashTraits<ThingWithDestructor> {
    static const bool needsDestruction = true;
};

static void heapMapDestructorHelper(bool clearMaps)
{
    HeapStats initialHeapStats;
    clearOutOldGarbage(&initialHeapStats);
    ThingWithDestructor::s_liveThingsWithDestructor = 0;

    typedef HeapHashMap<WeakMember<IntWrapper>, RefPtr<RefCountedAndGarbageCollected> > RefMap;

    typedef HeapHashMap<
        WeakMember<IntWrapper>,
        ThingWithDestructor,
        DefaultHash<WeakMember<IntWrapper> >::Hash,
        HashTraits<WeakMember<IntWrapper> >,
        ThingWithDestructorTraits> Map;

    Persistent<Map> map(new Map());
    Persistent<RefMap> refMap(new RefMap());

    Persistent<IntWrapper> luck(IntWrapper::create(103));

    int baseLine, refBaseLine;

    {
        Map stackMap;
        RefMap stackRefMap;

        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);

        stackMap.add(IntWrapper::create(42), ThingWithDestructor(1729));
        stackMap.add(luck, ThingWithDestructor(8128));
        stackRefMap.add(IntWrapper::create(42), RefCountedAndGarbageCollected::create());
        stackRefMap.add(luck, RefCountedAndGarbageCollected::create());

        baseLine = ThingWithDestructor::s_liveThingsWithDestructor;
        refBaseLine = RefCountedAndGarbageCollected::s_destructorCalls;

        // Although the heap maps are on-stack, we can't expect prompt
        // finalization of the elements, so when they go out of scope here we
        // will not necessarily have called the relevant destructors.
    }

    // The RefCountedAndGarbageCollected things need an extra GC to discover
    // that they are no longer ref counted.
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(baseLine - 2, ThingWithDestructor::s_liveThingsWithDestructor);
    EXPECT_EQ(refBaseLine + 2, RefCountedAndGarbageCollected::s_destructorCalls);

    // Now use maps kept alive with persistents. Here we don't expect any
    // destructors to be called before there have been GCs.

    map->add(IntWrapper::create(42), ThingWithDestructor(1729));
    map->add(luck, ThingWithDestructor(8128));
    refMap->add(IntWrapper::create(42), RefCountedAndGarbageCollected::create());
    refMap->add(luck, RefCountedAndGarbageCollected::create());

    baseLine  =  ThingWithDestructor::s_liveThingsWithDestructor;
    refBaseLine = RefCountedAndGarbageCollected::s_destructorCalls;

    luck.clear();
    if (clearMaps) {
        map->clear(); // Clear map.
        refMap->clear(); // Clear map.
    } else {
        map.clear(); // Clear Persistent handle, not map.
        refMap.clear(); // Clear Persistent handle, not map.
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    }

    EXPECT_EQ(baseLine - 2, ThingWithDestructor::s_liveThingsWithDestructor);

    // Need a GC to make sure that the RefCountedAndGarbageCollected thing
    // noticies it's been decremented to zero.
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(refBaseLine + 2, RefCountedAndGarbageCollected::s_destructorCalls);
}

TEST(HeapTest, HeapMapDestructor)
{
    heapMapDestructorHelper(true);
    heapMapDestructorHelper(false);
}

typedef HeapHashSet<PairWeakStrong> WeakStrongSet;
typedef HeapHashSet<PairWeakUnwrapped> WeakUnwrappedSet;
typedef HeapHashSet<PairStrongWeak> StrongWeakSet;
typedef HeapHashSet<PairUnwrappedWeak> UnwrappedWeakSet;
typedef HeapLinkedHashSet<PairWeakStrong> WeakStrongLinkedSet;
typedef HeapLinkedHashSet<PairWeakUnwrapped> WeakUnwrappedLinkedSet;
typedef HeapLinkedHashSet<PairStrongWeak> StrongWeakLinkedSet;
typedef HeapLinkedHashSet<PairUnwrappedWeak> UnwrappedWeakLinkedSet;
typedef HeapHashCountedSet<PairWeakStrong> WeakStrongCountedSet;
typedef HeapHashCountedSet<PairWeakUnwrapped> WeakUnwrappedCountedSet;
typedef HeapHashCountedSet<PairStrongWeak> StrongWeakCountedSet;
typedef HeapHashCountedSet<PairUnwrappedWeak> UnwrappedWeakCountedSet;

template<typename T>
T& iteratorExtractor(WTF::KeyValuePair<T, unsigned>& pair)
{
    return pair.key;
}

template<typename T>
T& iteratorExtractor(T& notAPair)
{
    return notAPair;
}

template<typename WSSet, typename SWSet, typename WUSet, typename UWSet>
void checkPairSets(
    Persistent<WSSet>& weakStrong,
    Persistent<SWSet>& strongWeak,
    Persistent<WUSet>& weakUnwrapped,
    Persistent<UWSet>& unwrappedWeak,
    bool ones,
    Persistent<IntWrapper>& two)
{
    typename WSSet::iterator itWS = weakStrong->begin();
    typename SWSet::iterator itSW = strongWeak->begin();
    typename WUSet::iterator itWU = weakUnwrapped->begin();
    typename UWSet::iterator itUW = unwrappedWeak->begin();

    EXPECT_EQ(2u, weakStrong->size());
    EXPECT_EQ(2u, strongWeak->size());
    EXPECT_EQ(2u, weakUnwrapped->size());
    EXPECT_EQ(2u, unwrappedWeak->size());

    PairWeakStrong p = iteratorExtractor(*itWS);
    PairStrongWeak p2 = iteratorExtractor(*itSW);
    PairWeakUnwrapped p3 = iteratorExtractor(*itWU);
    PairUnwrappedWeak p4 = iteratorExtractor(*itUW);
    if (p.first == two && p.second == two)
        ++itWS;
    if (p2.first == two && p2.second == two)
        ++itSW;
    if (p3.first == two && p3.second == 2)
        ++itWU;
    if (p4.first == 2 && p4.second == two)
        ++itUW;
    p = iteratorExtractor(*itWS);
    p2 = iteratorExtractor(*itSW);
    p3 = iteratorExtractor(*itWU);
    p4 = iteratorExtractor(*itUW);
    IntWrapper* nullWrapper = 0;
    if (ones) {
        EXPECT_EQ(p.first->value(), 1);
        EXPECT_EQ(p2.second->value(), 1);
        EXPECT_EQ(p3.first->value(), 1);
        EXPECT_EQ(p4.second->value(), 1);
    } else {
        EXPECT_EQ(p.first, nullWrapper);
        EXPECT_EQ(p2.second, nullWrapper);
        EXPECT_EQ(p3.first, nullWrapper);
        EXPECT_EQ(p4.second, nullWrapper);
    }

    EXPECT_EQ(p.second->value(), 2);
    EXPECT_EQ(p2.first->value(), 2);
    EXPECT_EQ(p3.second, 2);
    EXPECT_EQ(p4.first, 2);

    EXPECT_TRUE(weakStrong->contains(PairWeakStrong(&*two, &*two)));
    EXPECT_TRUE(strongWeak->contains(PairStrongWeak(&*two, &*two)));
    EXPECT_TRUE(weakUnwrapped->contains(PairWeakUnwrapped(&*two, 2)));
    EXPECT_TRUE(unwrappedWeak->contains(PairUnwrappedWeak(2, &*two)));
}

template<typename WSSet, typename SWSet, typename WUSet, typename UWSet>
void weakPairsHelper()
{
    IntWrapper::s_destructorCalls = 0;

    PersistentHeapVector<Member<IntWrapper> > keepNumbersAlive;

    Persistent<WSSet> weakStrong = new WSSet();
    Persistent<SWSet> strongWeak = new SWSet();
    Persistent<WUSet> weakUnwrapped = new WUSet();
    Persistent<UWSet> unwrappedWeak = new UWSet();

    Persistent<IntWrapper> two = IntWrapper::create(2);

    weakStrong->add(PairWeakStrong(IntWrapper::create(1), &*two));
    weakStrong->add(PairWeakStrong(&*two, &*two));
    strongWeak->add(PairStrongWeak(&*two, IntWrapper::create(1)));
    strongWeak->add(PairStrongWeak(&*two, &*two));
    weakUnwrapped->add(PairWeakUnwrapped(IntWrapper::create(1), 2));
    weakUnwrapped->add(PairWeakUnwrapped(&*two, 2));
    unwrappedWeak->add(PairUnwrappedWeak(2, IntWrapper::create(1)));
    unwrappedWeak->add(PairUnwrappedWeak(2, &*two));

    checkPairSets<WSSet, SWSet, WUSet, UWSet>(weakStrong, strongWeak, weakUnwrapped, unwrappedWeak, true, two);

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    checkPairSets<WSSet, SWSet, WUSet, UWSet>(weakStrong, strongWeak, weakUnwrapped, unwrappedWeak, false, two);
}

TEST(HeapTest, HeapWeakPairs)
{
    {
        typedef HeapHashSet<PairWeakStrong> WeakStrongSet;
        typedef HeapHashSet<PairWeakUnwrapped> WeakUnwrappedSet;
        typedef HeapHashSet<PairStrongWeak> StrongWeakSet;
        typedef HeapHashSet<PairUnwrappedWeak> UnwrappedWeakSet;
        weakPairsHelper<WeakStrongSet, StrongWeakSet, WeakUnwrappedSet, UnwrappedWeakSet>();
    }

    {
        typedef HeapListHashSet<PairWeakStrong> WeakStrongSet;
        typedef HeapListHashSet<PairWeakUnwrapped> WeakUnwrappedSet;
        typedef HeapListHashSet<PairStrongWeak> StrongWeakSet;
        typedef HeapListHashSet<PairUnwrappedWeak> UnwrappedWeakSet;
        weakPairsHelper<WeakStrongSet, StrongWeakSet, WeakUnwrappedSet, UnwrappedWeakSet>();
    }

    {
        typedef HeapLinkedHashSet<PairWeakStrong> WeakStrongSet;
        typedef HeapLinkedHashSet<PairWeakUnwrapped> WeakUnwrappedSet;
        typedef HeapLinkedHashSet<PairStrongWeak> StrongWeakSet;
        typedef HeapLinkedHashSet<PairUnwrappedWeak> UnwrappedWeakSet;
        weakPairsHelper<WeakStrongSet, StrongWeakSet, WeakUnwrappedSet, UnwrappedWeakSet>();
    }
}

TEST(HeapTest, HeapWeakCollectionTypes)
{
    HeapStats initialHeapSize;
    IntWrapper::s_destructorCalls = 0;

    typedef HeapHashMap<WeakMember<IntWrapper>, Member<IntWrapper> > WeakStrong;
    typedef HeapHashMap<Member<IntWrapper>, WeakMember<IntWrapper> > StrongWeak;
    typedef HeapHashMap<WeakMember<IntWrapper>, WeakMember<IntWrapper> > WeakWeak;
    typedef HeapHashSet<WeakMember<IntWrapper> > WeakSet;
    typedef HeapLinkedHashSet<WeakMember<IntWrapper> > WeakOrderedSet;

    clearOutOldGarbage(&initialHeapSize);

    const int weakStrongIndex = 0;
    const int strongWeakIndex = 1;
    const int weakWeakIndex = 2;
    const int numberOfMapIndices = 3;
    const int weakSetIndex = 3;
    const int weakOrderedSetIndex = 4;
    const int numberOfCollections = 5;

    for (int testRun = 0; testRun < 4; testRun++) {
        for (int collectionNumber = 0; collectionNumber < numberOfCollections; collectionNumber++) {
            bool deleteAfterwards = (testRun == 1);
            bool addAfterwards = (testRun == 2);
            bool testThatIteratorsMakeStrong = (testRun == 3);

            // The test doesn't work for strongWeak with deleting because we lost
            // the key from the keepNumbersAlive array, so we can't do the lookup.
            if (deleteAfterwards && collectionNumber == strongWeakIndex)
                continue;

            unsigned added = addAfterwards ? 100 : 0;

            Persistent<WeakStrong> weakStrong = new WeakStrong();
            Persistent<StrongWeak> strongWeak = new StrongWeak();
            Persistent<WeakWeak> weakWeak = new WeakWeak();

            Persistent<WeakSet> weakSet = new WeakSet();
            Persistent<WeakOrderedSet> weakOrderedSet = new WeakOrderedSet();

            PersistentHeapVector<Member<IntWrapper> > keepNumbersAlive;
            for (int i = 0; i < 128; i += 2) {
                IntWrapper* wrapped = IntWrapper::create(i);
                IntWrapper* wrapped2 = IntWrapper::create(i + 1);
                keepNumbersAlive.append(wrapped);
                keepNumbersAlive.append(wrapped2);
                weakStrong->add(wrapped, wrapped2);
                strongWeak->add(wrapped2, wrapped);
                weakWeak->add(wrapped, wrapped2);
                weakSet->add(wrapped);
                weakOrderedSet->add(wrapped);
            }

            EXPECT_EQ(64u, weakStrong->size());
            EXPECT_EQ(64u, strongWeak->size());
            EXPECT_EQ(64u, weakWeak->size());
            EXPECT_EQ(64u, weakSet->size());
            EXPECT_EQ(64u, weakOrderedSet->size());

            // Collect garbage. This should change nothing since we are keeping
            // alive the IntWrapper objects.
            Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);

            EXPECT_EQ(64u, weakStrong->size());
            EXPECT_EQ(64u, strongWeak->size());
            EXPECT_EQ(64u, weakWeak->size());
            EXPECT_EQ(64u, weakSet->size());
            EXPECT_EQ(64u, weakOrderedSet->size());

            for (int i = 0; i < 128; i += 2) {
                IntWrapper* wrapped = keepNumbersAlive[i];
                IntWrapper* wrapped2 = keepNumbersAlive[i + 1];
                EXPECT_EQ(wrapped2, weakStrong->get(wrapped));
                EXPECT_EQ(wrapped, strongWeak->get(wrapped2));
                EXPECT_EQ(wrapped2, weakWeak->get(wrapped));
                EXPECT_TRUE(weakSet->contains(wrapped));
                EXPECT_TRUE(weakOrderedSet->contains(wrapped));
            }

            for (int i = 0; i < 128; i += 3)
                keepNumbersAlive[i] = nullptr;

            if (collectionNumber != weakStrongIndex)
                weakStrong->clear();
            if (collectionNumber != strongWeakIndex)
                strongWeak->clear();
            if (collectionNumber != weakWeakIndex)
                weakWeak->clear();
            if (collectionNumber != weakSetIndex)
                weakSet->clear();
            if (collectionNumber != weakOrderedSetIndex)
                weakOrderedSet->clear();

            if (testThatIteratorsMakeStrong) {
                WeakStrong::iterator it1 = weakStrong->begin();
                StrongWeak::iterator it2 = strongWeak->begin();
                WeakWeak::iterator it3 = weakWeak->begin();
                WeakSet::iterator it4 = weakSet->begin();
                WeakOrderedSet::iterator it5 = weakOrderedSet->begin();
                // Collect garbage. This should change nothing since the
                // iterators make the collections strong.
                Heap::collectGarbage(ThreadState::HeapPointersOnStack);
                if (collectionNumber == weakStrongIndex) {
                    EXPECT_EQ(64u, weakStrong->size());
                    MapIteratorCheck(it1, weakStrong->end(), 64);
                } else if (collectionNumber == strongWeakIndex) {
                    EXPECT_EQ(64u, strongWeak->size());
                    MapIteratorCheck(it2, strongWeak->end(), 64);
                } else if (collectionNumber == weakWeakIndex) {
                    EXPECT_EQ(64u, weakWeak->size());
                    MapIteratorCheck(it3, weakWeak->end(), 64);
                } else if (collectionNumber == weakSetIndex) {
                    EXPECT_EQ(64u, weakSet->size());
                    SetIteratorCheck(it4, weakSet->end(), 64);
                } else if (collectionNumber == weakOrderedSetIndex) {
                    EXPECT_EQ(64u, weakOrderedSet->size());
                    SetIteratorCheck(it5, weakOrderedSet->end(), 64);
                }
            } else {
                // Collect garbage. This causes weak processing to remove
                // things from the collections.
                Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
                unsigned count = 0;
                for (int i = 0; i < 128; i += 2) {
                    bool firstAlive = keepNumbersAlive[i];
                    bool secondAlive = keepNumbersAlive[i + 1];
                    if (firstAlive && (collectionNumber == weakStrongIndex || collectionNumber == strongWeakIndex))
                        secondAlive = true;
                    if (firstAlive && secondAlive && collectionNumber < numberOfMapIndices) {
                        if (collectionNumber == weakStrongIndex) {
                            if (deleteAfterwards)
                                EXPECT_EQ(i + 1, weakStrong->take(keepNumbersAlive[i])->value());
                        } else if (collectionNumber == strongWeakIndex) {
                            if (deleteAfterwards)
                                EXPECT_EQ(i, strongWeak->take(keepNumbersAlive[i + 1])->value());
                        } else if (collectionNumber == weakWeakIndex) {
                            if (deleteAfterwards)
                                EXPECT_EQ(i + 1, weakWeak->take(keepNumbersAlive[i])->value());
                        }
                        if (!deleteAfterwards)
                            count++;
                    } else if (collectionNumber == weakSetIndex && firstAlive) {
                        ASSERT_TRUE(weakSet->contains(keepNumbersAlive[i]));
                        if (deleteAfterwards)
                            weakSet->remove(keepNumbersAlive[i]);
                        else
                            count++;
                    } else if (collectionNumber == weakOrderedSetIndex && firstAlive) {
                        ASSERT_TRUE(weakOrderedSet->contains(keepNumbersAlive[i]));
                        if (deleteAfterwards)
                            weakOrderedSet->remove(keepNumbersAlive[i]);
                        else
                            count++;
                    }
                }
                if (addAfterwards) {
                    for (int i = 1000; i < 1100; i++) {
                        IntWrapper* wrapped = IntWrapper::create(i);
                        keepNumbersAlive.append(wrapped);
                        weakStrong->add(wrapped, wrapped);
                        strongWeak->add(wrapped, wrapped);
                        weakWeak->add(wrapped, wrapped);
                        weakSet->add(wrapped);
                        weakOrderedSet->add(wrapped);
                    }
                }
                if (collectionNumber == weakStrongIndex)
                    EXPECT_EQ(count + added, weakStrong->size());
                else if (collectionNumber == strongWeakIndex)
                    EXPECT_EQ(count + added, strongWeak->size());
                else if (collectionNumber == weakWeakIndex)
                    EXPECT_EQ(count + added, weakWeak->size());
                else if (collectionNumber == weakSetIndex)
                    EXPECT_EQ(count + added, weakSet->size());
                else if (collectionNumber == weakOrderedSetIndex)
                    EXPECT_EQ(count + added, weakOrderedSet->size());
                WeakStrong::iterator it1 = weakStrong->begin();
                StrongWeak::iterator it2 = strongWeak->begin();
                WeakWeak::iterator it3 = weakWeak->begin();
                WeakSet::iterator it4 = weakSet->begin();
                WeakOrderedSet::iterator it5 = weakOrderedSet->begin();
                MapIteratorCheck(it1, weakStrong->end(), (collectionNumber == weakStrongIndex ? count : 0) + added);
                MapIteratorCheck(it2, strongWeak->end(), (collectionNumber == strongWeakIndex ? count : 0) + added);
                MapIteratorCheck(it3, weakWeak->end(), (collectionNumber == weakWeakIndex ? count : 0) + added);
                SetIteratorCheck(it4, weakSet->end(), (collectionNumber == weakSetIndex ? count : 0) + added);
                SetIteratorCheck(it5, weakOrderedSet->end(), (collectionNumber == weakOrderedSetIndex ? count : 0) + added);
            }
            for (unsigned i = 0; i < 128 + added; i++)
                keepNumbersAlive[i] = nullptr;
            Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
            EXPECT_EQ(added, weakStrong->size());
            EXPECT_EQ(added, strongWeak->size());
            EXPECT_EQ(added, weakWeak->size());
            EXPECT_EQ(added, weakSet->size());
            EXPECT_EQ(added, weakOrderedSet->size());
        }
    }
}

TEST(HeapTest, RefCountedGarbageCollected)
{
    RefCountedAndGarbageCollected::s_destructorCalls = 0;
    {
        RefPtr<RefCountedAndGarbageCollected> refPtr3;
        {
            Persistent<RefCountedAndGarbageCollected> persistent;
            {
                RefPtr<RefCountedAndGarbageCollected> refPtr1 = RefCountedAndGarbageCollected::create();
                RefPtr<RefCountedAndGarbageCollected> refPtr2 = RefCountedAndGarbageCollected::create();
                Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
                EXPECT_EQ(0, RefCountedAndGarbageCollected::s_destructorCalls);
                persistent = refPtr1.get();
            }
            // Reference count is zero for both objects but one of
            // them is kept alive by a persistent handle.
            Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
            EXPECT_EQ(1, RefCountedAndGarbageCollected::s_destructorCalls);
            refPtr3 = persistent.get();
        }
        // The persistent handle is gone but the ref count has been
        // increased to 1.
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(1, RefCountedAndGarbageCollected::s_destructorCalls);
    }
    // Both persistent handle is gone and ref count is zero so the
    // object can be collected.
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(2, RefCountedAndGarbageCollected::s_destructorCalls);
}

TEST(HeapTest, RefCountedGarbageCollectedWithStackPointers)
{
    RefCountedAndGarbageCollected::s_destructorCalls = 0;
    RefCountedAndGarbageCollected2::s_destructorCalls = 0;
    {
        RefCountedAndGarbageCollected* pointer1 = 0;
        RefCountedAndGarbageCollected2* pointer2 = 0;
        {
            RefPtr<RefCountedAndGarbageCollected> object1 = RefCountedAndGarbageCollected::create();
            RefPtr<RefCountedAndGarbageCollected2> object2 = RefCountedAndGarbageCollected2::create();
            pointer1 = object1.get();
            pointer2 = object2.get();
            void* objects[2] = { object1.get(), object2.get() };
            RefCountedGarbageCollectedVisitor visitor(2, objects);
            ThreadState::current()->visitPersistents(&visitor);
            EXPECT_TRUE(visitor.validate());

            Heap::collectGarbage(ThreadState::HeapPointersOnStack);
            EXPECT_EQ(0, RefCountedAndGarbageCollected::s_destructorCalls);
            EXPECT_EQ(0, RefCountedAndGarbageCollected2::s_destructorCalls);
        }
        Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        EXPECT_EQ(0, RefCountedAndGarbageCollected::s_destructorCalls);
        EXPECT_EQ(0, RefCountedAndGarbageCollected2::s_destructorCalls);

        // At this point, the reference counts of object1 and object2 are 0.
        // Only pointer1 and pointer2 keep references to object1 and object2.
        void* objects[] = { 0 };
        RefCountedGarbageCollectedVisitor visitor(0, objects);
        ThreadState::current()->visitPersistents(&visitor);
        EXPECT_TRUE(visitor.validate());

        {
            RefPtr<RefCountedAndGarbageCollected> object1(pointer1);
            RefPtr<RefCountedAndGarbageCollected2> object2(pointer2);
            void* objects[2] = { object1.get(), object2.get() };
            RefCountedGarbageCollectedVisitor visitor(2, objects);
            ThreadState::current()->visitPersistents(&visitor);
            EXPECT_TRUE(visitor.validate());

            Heap::collectGarbage(ThreadState::HeapPointersOnStack);
            EXPECT_EQ(0, RefCountedAndGarbageCollected::s_destructorCalls);
            EXPECT_EQ(0, RefCountedAndGarbageCollected2::s_destructorCalls);
        }

        Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        EXPECT_EQ(0, RefCountedAndGarbageCollected::s_destructorCalls);
        EXPECT_EQ(0, RefCountedAndGarbageCollected2::s_destructorCalls);
    }

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(1, RefCountedAndGarbageCollected::s_destructorCalls);
    EXPECT_EQ(1, RefCountedAndGarbageCollected2::s_destructorCalls);
}

TEST(HeapTest, WeakMembers)
{
    Bar::s_live = 0;
    {
        Persistent<Bar> h1 = Bar::create();
        Persistent<Weak> h4;
        Persistent<WithWeakMember> h5;
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        ASSERT_EQ(1u, Bar::s_live); // h1 is live.
        {
            Bar* h2 = Bar::create();
            Bar* h3 = Bar::create();
            h4 = Weak::create(h2, h3);
            h5 = WithWeakMember::create(h2, h3);
            Heap::collectGarbage(ThreadState::HeapPointersOnStack);
            EXPECT_EQ(5u, Bar::s_live); // The on-stack pointer keeps h3 alive.
            EXPECT_TRUE(h4->strongIsThere());
            EXPECT_TRUE(h4->weakIsThere());
            EXPECT_TRUE(h5->strongIsThere());
            EXPECT_TRUE(h5->weakIsThere());
        }
        // h3 is collected, weak pointers from h4 and h5 don't keep it alive.
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(4u, Bar::s_live);
        EXPECT_TRUE(h4->strongIsThere());
        EXPECT_FALSE(h4->weakIsThere()); // h3 is gone from weak pointer.
        EXPECT_TRUE(h5->strongIsThere());
        EXPECT_FALSE(h5->weakIsThere()); // h3 is gone from weak pointer.
        h1.release(); // Zero out h1.
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(3u, Bar::s_live); // Only h4, h5 and h2 are left.
        EXPECT_TRUE(h4->strongIsThere()); // h2 is still pointed to from h4.
        EXPECT_TRUE(h5->strongIsThere()); // h2 is still pointed to from h5.
    }
    // h4 and h5 have gone out of scope now and they were keeping h2 alive.
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0u, Bar::s_live); // All gone.
}

TEST(HeapTest, FinalizationObserver)
{
    Persistent<FinalizationObserver<Observable> > o;
    {
        Observable* foo = Observable::create(Bar::create());
        // |o| observes |foo|.
        o = FinalizationObserver<Observable>::create(foo);
    }
    // FinalizationObserver doesn't have a strong reference to |foo|. So |foo|
    // and its member will be collected.
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0u, Bar::s_live);
    EXPECT_TRUE(o->didCallWillFinalize());

    FinalizationObserverWithHashMap::s_didCallWillFinalize = false;
    Observable* foo = Observable::create(Bar::create());
    FinalizationObserverWithHashMap::ObserverMap& map = FinalizationObserverWithHashMap::observe(*foo);
    EXPECT_EQ(1u, map.size());
    foo = 0;
    // FinalizationObserverWithHashMap doesn't have a strong reference to
    // |foo|. So |foo| and its member will be collected.
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0u, Bar::s_live);
    EXPECT_EQ(0u, map.size());
    EXPECT_TRUE(FinalizationObserverWithHashMap::s_didCallWillFinalize);
}

TEST(HeapTest, Comparisons)
{
    Persistent<Bar> barPersistent = Bar::create();
    Persistent<Foo> fooPersistent = Foo::create(barPersistent);
    EXPECT_TRUE(barPersistent != fooPersistent);
    barPersistent = fooPersistent;
    EXPECT_TRUE(barPersistent == fooPersistent);
}

TEST(HeapTest, CheckAndMarkPointer)
{
    HeapStats initialHeapStats;
    clearOutOldGarbage(&initialHeapStats);

    Vector<Address> objectAddresses;
    Vector<Address> endAddresses;
    Address largeObjectAddress;
    Address largeObjectEndAddress;
    CountingVisitor visitor;
    for (int i = 0; i < 10; i++) {
        SimpleObject* object = SimpleObject::create();
        Address objectAddress = reinterpret_cast<Address>(object);
        objectAddresses.append(objectAddress);
        endAddresses.append(objectAddress + sizeof(SimpleObject) - 1);
    }
    LargeObject* largeObject = LargeObject::create();
    largeObjectAddress = reinterpret_cast<Address>(largeObject);
    largeObjectEndAddress = largeObjectAddress + sizeof(LargeObject) - 1;

    // This is a low-level test where we call checkAndMarkPointer. This method
    // causes the object start bitmap to be computed which requires the heap
    // to be in a consistent state (e.g. the free allocation area must be put
    // into a free list header). However when we call makeConsistentForGC it
    // also clears out the freelists so we have to rebuild those before trying
    // to allocate anything again. We do this by forcing a GC after doing the
    // checkAndMarkPointer tests.
    {
        TestGCScope scope(ThreadState::HeapPointersOnStack);
        EXPECT_TRUE(scope.allThreadsParked()); // Fail the test if we could not park all threads.
        Heap::makeConsistentForGC();
        for (size_t i = 0; i < objectAddresses.size(); i++) {
            EXPECT_TRUE(Heap::checkAndMarkPointer(&visitor, objectAddresses[i]));
            EXPECT_TRUE(Heap::checkAndMarkPointer(&visitor, endAddresses[i]));
        }
        EXPECT_EQ(objectAddresses.size() * 2, visitor.count());
        visitor.reset();
        EXPECT_TRUE(Heap::checkAndMarkPointer(&visitor, largeObjectAddress));
        EXPECT_TRUE(Heap::checkAndMarkPointer(&visitor, largeObjectEndAddress));
        EXPECT_EQ(2ul, visitor.count());
        visitor.reset();
    }
    // This forces a GC without stack scanning which results in the objects
    // being collected. This will also rebuild the above mentioned freelists,
    // however we don't rely on that below since we don't have any allocations.
    clearOutOldGarbage(&initialHeapStats);
    {
        TestGCScope scope(ThreadState::HeapPointersOnStack);
        EXPECT_TRUE(scope.allThreadsParked());
        Heap::makeConsistentForGC();
        for (size_t i = 0; i < objectAddresses.size(); i++) {
            // We would like to assert that checkAndMarkPointer returned false
            // here because the pointers no longer point into a valid object
            // (it's been freed by the GCs. But checkAndMarkPointer will return
            // true for any pointer that points into a heap page, regardless of
            // whether it points at a valid object (this ensures the
            // correctness of the page-based on-heap address caches), so we
            // can't make that assert.
            Heap::checkAndMarkPointer(&visitor, objectAddresses[i]);
            Heap::checkAndMarkPointer(&visitor, endAddresses[i]);
        }
        EXPECT_EQ(0ul, visitor.count());
        Heap::checkAndMarkPointer(&visitor, largeObjectAddress);
        Heap::checkAndMarkPointer(&visitor, largeObjectEndAddress);
        EXPECT_EQ(0ul, visitor.count());
    }
    // This round of GC is important to make sure that the object start
    // bitmap are cleared out and that the free lists are rebuild.
    clearOutOldGarbage(&initialHeapStats);
}

TEST(HeapTest, VisitOffHeapCollections)
{
    HeapStats initialHeapStats;
    clearOutOldGarbage(&initialHeapStats);
    IntWrapper::s_destructorCalls = 0;
    Persistent<OffHeapContainer> container = OffHeapContainer::create();
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0, IntWrapper::s_destructorCalls);
    container = nullptr;
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(OffHeapContainer::deadWrappers, IntWrapper::s_destructorCalls);
}

TEST(HeapTest, PersistentHeapCollectionTypes)
{
    HeapStats initialHeapSize;
    IntWrapper::s_destructorCalls = 0;

    typedef HeapVector<Member<IntWrapper> > Vec;
    typedef PersistentHeapVector<Member<IntWrapper> > PVec;
    typedef PersistentHeapHashSet<Member<IntWrapper> > PSet;
    typedef PersistentHeapListHashSet<Member<IntWrapper> > PListSet;
    typedef PersistentHeapLinkedHashSet<Member<IntWrapper> > PLinkedSet;
    typedef PersistentHeapHashMap<Member<IntWrapper>, Member<IntWrapper> > PMap;
    typedef PersistentHeapDeque<Member<IntWrapper> > PDeque;

    clearOutOldGarbage(&initialHeapSize);
    {
        PVec pVec;
        PDeque pDeque;
        PSet pSet;
        PListSet pListSet;
        PLinkedSet pLinkedSet;
        PMap pMap;

        IntWrapper* one(IntWrapper::create(1));
        IntWrapper* two(IntWrapper::create(2));
        IntWrapper* three(IntWrapper::create(3));
        IntWrapper* four(IntWrapper::create(4));
        IntWrapper* five(IntWrapper::create(5));
        IntWrapper* six(IntWrapper::create(6));
        IntWrapper* seven(IntWrapper::create(7));
        IntWrapper* eight(IntWrapper::create(8));
        IntWrapper* nine(IntWrapper::create(9));

        pVec.append(one);
        pVec.append(two);

        pDeque.append(seven);
        pDeque.append(two);

        Vec* vec = new Vec();
        vec->swap(pVec);

        pVec.append(two);
        pVec.append(three);

        pSet.add(four);
        pListSet.add(eight);
        pLinkedSet.add(nine);
        pMap.add(five, six);

        // Collect |vec| and |one|.
        vec = 0;
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(1, IntWrapper::s_destructorCalls);

        EXPECT_EQ(2u, pVec.size());
        EXPECT_EQ(two, pVec.at(0));
        EXPECT_EQ(three, pVec.at(1));

        EXPECT_EQ(2u, pDeque.size());
        EXPECT_EQ(seven, pDeque.first());
        EXPECT_EQ(seven, pDeque.takeFirst());
        EXPECT_EQ(two, pDeque.first());

        EXPECT_EQ(1u, pDeque.size());

        EXPECT_EQ(1u, pSet.size());
        EXPECT_TRUE(pSet.contains(four));

        EXPECT_EQ(1u, pListSet.size());
        EXPECT_TRUE(pListSet.contains(eight));

        EXPECT_EQ(1u, pLinkedSet.size());
        EXPECT_TRUE(pLinkedSet.contains(nine));

        EXPECT_EQ(1u, pMap.size());
        EXPECT_EQ(six, pMap.get(five));
    }

    // Collect previous roots.
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(9, IntWrapper::s_destructorCalls);
}

TEST(HeapTest, CollectionNesting)
{
    HeapStats initialStats;
    clearOutOldGarbage(&initialStats);
    int* key = &IntWrapper::s_destructorCalls;
    IntWrapper::s_destructorCalls = 0;
    typedef HeapVector<Member<IntWrapper> > IntVector;
    typedef HeapDeque<Member<IntWrapper> > IntDeque;
    HeapHashMap<void*, IntVector>* map = new HeapHashMap<void*, IntVector>();
    HeapHashMap<void*, IntDeque>* map2 = new HeapHashMap<void*, IntDeque>();

    map->add(key, IntVector());
    map2->add(key, IntDeque());

    HeapHashMap<void*, IntVector>::iterator it = map->find(key);
    EXPECT_EQ(0u, map->get(key).size());

    HeapHashMap<void*, IntDeque>::iterator it2 = map2->find(key);
    EXPECT_EQ(0u, map2->get(key).size());

    it->value.append(IntWrapper::create(42));
    EXPECT_EQ(1u, map->get(key).size());

    it2->value.append(IntWrapper::create(42));
    EXPECT_EQ(1u, map2->get(key).size());

    Persistent<HeapHashMap<void*, IntVector> > keepAlive(map);
    Persistent<HeapHashMap<void*, IntDeque> > keepAlive2(map2);

    for (int i = 0; i < 100; i++) {
        map->add(key + 1 + i, IntVector());
        map2->add(key + 1 + i, IntDeque());
    }

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);

    EXPECT_EQ(1u, map->get(key).size());
    EXPECT_EQ(1u, map2->get(key).size());
    EXPECT_EQ(0, IntWrapper::s_destructorCalls);

    keepAlive = nullptr;
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(1, IntWrapper::s_destructorCalls);
}

TEST(HeapTest, GarbageCollectedMixin)
{
    HeapStats initialHeapStats;
    clearOutOldGarbage(&initialHeapStats);

    Persistent<UseMixin> usemixin = UseMixin::create();
    EXPECT_EQ(0, UseMixin::s_traceCount);
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(1, UseMixin::s_traceCount);

    Persistent<Mixin> mixin = usemixin;
    usemixin = nullptr;
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(2, UseMixin::s_traceCount);

    PersistentHeapHashSet<WeakMember<Mixin> > weakMap;
    weakMap.add(UseMixin::create());
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0u, weakMap.size());
}

TEST(HeapTest, CollectionNesting2)
{
    HeapStats initialStats;
    clearOutOldGarbage(&initialStats);
    void* key = &IntWrapper::s_destructorCalls;
    IntWrapper::s_destructorCalls = 0;
    typedef HeapHashSet<Member<IntWrapper> > IntSet;
    HeapHashMap<void*, IntSet>* map = new HeapHashMap<void*, IntSet>();

    map->add(key, IntSet());

    HeapHashMap<void*, IntSet>::iterator it = map->find(key);
    EXPECT_EQ(0u, map->get(key).size());

    it->value.add(IntWrapper::create(42));
    EXPECT_EQ(1u, map->get(key).size());

    Persistent<HeapHashMap<void*, IntSet> > keepAlive(map);
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(1u, map->get(key).size());
    EXPECT_EQ(0, IntWrapper::s_destructorCalls);
}

TEST(HeapTest, CollectionNesting3)
{
    HeapStats initialStats;
    clearOutOldGarbage(&initialStats);
    IntWrapper::s_destructorCalls = 0;
    typedef HeapVector<Member<IntWrapper> > IntVector;
    typedef HeapDeque<Member<IntWrapper> > IntDeque;
    HeapVector<IntVector>* vector = new HeapVector<IntVector>();
    HeapDeque<IntDeque>* deque = new HeapDeque<IntDeque>();

    vector->append(IntVector());
    deque->append(IntDeque());

    HeapVector<IntVector>::iterator it = vector->begin();
    HeapDeque<IntDeque>::iterator it2 = deque->begin();
    EXPECT_EQ(0u, it->size());
    EXPECT_EQ(0u, it2->size());

    it->append(IntWrapper::create(42));
    it2->append(IntWrapper::create(42));
    EXPECT_EQ(1u, it->size());
    EXPECT_EQ(1u, it2->size());

    Persistent<HeapVector<IntVector> > keepAlive(vector);
    Persistent<HeapDeque<IntDeque> > keepAlive2(deque);
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(1u, it->size());
    EXPECT_EQ(1u, it2->size());
    EXPECT_EQ(0, IntWrapper::s_destructorCalls);
}

TEST(HeapTest, EmbeddedInVector)
{
    HeapStats initialStats;
    clearOutOldGarbage(&initialStats);
    SimpleFinalizedObject::s_destructorCalls = 0;
    {
        PersistentHeapVector<VectorObject, 2> inlineVector;
        PersistentHeapVector<VectorObject> outlineVector;
        VectorObject i1, i2;
        inlineVector.append(i1);
        inlineVector.append(i2);

        VectorObject o1, o2;
        outlineVector.append(o1);
        outlineVector.append(o2);

        PersistentHeapVector<VectorObjectInheritedTrace> vectorInheritedTrace;
        VectorObjectInheritedTrace it1, it2;
        vectorInheritedTrace.append(it1);
        vectorInheritedTrace.append(it2);

        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(0, SimpleFinalizedObject::s_destructorCalls);

        // Since VectorObjectNoTrace has no trace method it will
        // not be traced and hence be collected when doing GC.
        // We trace items in a collection braced on the item's
        // having a trace method. This is determined via the
        // NeedsTracing trait in wtf/TypeTraits.h.
        PersistentHeapVector<VectorObjectNoTrace> vectorNoTrace;
        VectorObjectNoTrace n1, n2;
        vectorNoTrace.append(n1);
        vectorNoTrace.append(n2);
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(2, SimpleFinalizedObject::s_destructorCalls);
    }
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(8, SimpleFinalizedObject::s_destructorCalls);
}

TEST(HeapTest, EmbeddedInDeque)
{
    HeapStats initialStats;
    clearOutOldGarbage(&initialStats);
    SimpleFinalizedObject::s_destructorCalls = 0;
    {
        PersistentHeapDeque<VectorObject, 2> inlineDeque;
        PersistentHeapDeque<VectorObject> outlineDeque;
        VectorObject i1, i2;
        inlineDeque.append(i1);
        inlineDeque.append(i2);

        VectorObject o1, o2;
        outlineDeque.append(o1);
        outlineDeque.append(o2);

        PersistentHeapDeque<VectorObjectInheritedTrace> dequeInheritedTrace;
        VectorObjectInheritedTrace it1, it2;
        dequeInheritedTrace.append(it1);
        dequeInheritedTrace.append(it2);

        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(0, SimpleFinalizedObject::s_destructorCalls);

        // Since VectorObjectNoTrace has no trace method it will
        // not be traced and hence be collected when doing GC.
        // We trace items in a collection braced on the item's
        // having a trace method. This is determined via the
        // NeedsTracing trait in wtf/TypeTraits.h.
        PersistentHeapDeque<VectorObjectNoTrace> dequeNoTrace;
        VectorObjectNoTrace n1, n2;
        dequeNoTrace.append(n1);
        dequeNoTrace.append(n2);
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(2, SimpleFinalizedObject::s_destructorCalls);
    }
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(8, SimpleFinalizedObject::s_destructorCalls);
}

template<typename Set>
void rawPtrInHashHelper()
{
    Set set;
    set.add(new int(42));
    set.add(new int(42));
    EXPECT_EQ(2u, set.size());
    for (typename Set::iterator it = set.begin(); it != set.end(); ++it)
        EXPECT_EQ(42, **it);
}

TEST(HeapTest, RawPtrInHash)
{
    rawPtrInHashHelper<HashSet<RawPtr<int> > >();
    rawPtrInHashHelper<ListHashSet<RawPtr<int> > >();
    rawPtrInHashHelper<LinkedHashSet<RawPtr<int> > >();
}

TEST(HeapTest, HeapTerminatedArray)
{
    HeapStats initialHeapSize;
    clearOutOldGarbage(&initialHeapSize);
    IntWrapper::s_destructorCalls = 0;

    HeapTerminatedArray<TerminatedArrayItem>* arr = 0;

    const size_t prefixSize = 4;
    const size_t suffixSize = 4;

    {
        HeapTerminatedArrayBuilder<TerminatedArrayItem> builder(arr);
        builder.grow(prefixSize);
        for (size_t i = 0; i < prefixSize; i++)
            builder.append(TerminatedArrayItem(IntWrapper::create(i)));
        arr = builder.release();
    }

    Heap::collectGarbage(ThreadState::HeapPointersOnStack);
    EXPECT_EQ(0, IntWrapper::s_destructorCalls);
    EXPECT_EQ(prefixSize, arr->size());
    for (size_t i = 0; i < prefixSize; i++)
        EXPECT_EQ(i, static_cast<size_t>(arr->at(i).payload()->value()));

    {
        HeapTerminatedArrayBuilder<TerminatedArrayItem> builder(arr);
        builder.grow(suffixSize);
        for (size_t i = 0; i < suffixSize; i++)
            builder.append(TerminatedArrayItem(IntWrapper::create(prefixSize + i)));
        arr = builder.release();
    }

    Heap::collectGarbage(ThreadState::HeapPointersOnStack);
    EXPECT_EQ(0, IntWrapper::s_destructorCalls);
    EXPECT_EQ(prefixSize + suffixSize, arr->size());
    for (size_t i = 0; i < prefixSize + suffixSize; i++)
        EXPECT_EQ(i, static_cast<size_t>(arr->at(i).payload()->value()));

    {
        Persistent<HeapTerminatedArray<TerminatedArrayItem> > persistentArr = arr;
        arr = 0;
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        arr = persistentArr.get();
        EXPECT_EQ(0, IntWrapper::s_destructorCalls);
        EXPECT_EQ(prefixSize + suffixSize, arr->size());
        for (size_t i = 0; i < prefixSize + suffixSize; i++)
            EXPECT_EQ(i, static_cast<size_t>(arr->at(i).payload()->value()));
    }

    arr = 0;
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(8, IntWrapper::s_destructorCalls);
}

TEST(HeapTest, HeapLinkedStack)
{
    HeapStats initialHeapSize;
    clearOutOldGarbage(&initialHeapSize);
    IntWrapper::s_destructorCalls = 0;

    HeapLinkedStack<TerminatedArrayItem>* stack = new HeapLinkedStack<TerminatedArrayItem>();

    const size_t stackSize = 10;

    for (size_t i = 0; i < stackSize; i++)
        stack->push(TerminatedArrayItem(IntWrapper::create(i)));

    Heap::collectGarbage(ThreadState::HeapPointersOnStack);
    EXPECT_EQ(0, IntWrapper::s_destructorCalls);
    EXPECT_EQ(stackSize, stack->size());
    while (!stack->isEmpty()) {
        EXPECT_EQ(stack->size() - 1, static_cast<size_t>(stack->peek().payload()->value()));
        stack->pop();
    }

    Persistent<HeapLinkedStack<TerminatedArrayItem> > pStack = stack;

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(stackSize, static_cast<size_t>(IntWrapper::s_destructorCalls));
    EXPECT_EQ(0u, pStack->size());
}

TEST(HeapTest, AllocationDuringFinalization)
{
    HeapStats initialHeapSize;
    clearOutOldGarbage(&initialHeapSize);
    IntWrapper::s_destructorCalls = 0;
    OneKiloByteObject::s_destructorCalls = 0;

    Persistent<IntWrapper> wrapper;
    new FinalizationAllocator(&wrapper);

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0, IntWrapper::s_destructorCalls);
    // Check that the wrapper allocated during finalization is not
    // swept away and zapped later in the same sweeping phase.
    EXPECT_EQ(42, wrapper->value());

    wrapper.clear();
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(10, IntWrapper::s_destructorCalls);
    EXPECT_EQ(512, OneKiloByteObject::s_destructorCalls);
}

class SimpleClassWithDestructor {
public:
    SimpleClassWithDestructor() { }
    ~SimpleClassWithDestructor()
    {
        s_wasDestructed = true;
    }
    static bool s_wasDestructed;
};

bool SimpleClassWithDestructor::s_wasDestructed;

class RefCountedWithDestructor : public RefCounted<RefCountedWithDestructor> {
public:
    RefCountedWithDestructor() { }
    ~RefCountedWithDestructor()
    {
        s_wasDestructed = true;
    }
    static bool s_wasDestructed;
};

bool RefCountedWithDestructor::s_wasDestructed;

template<typename Set>
void destructorsCalledOnGC(bool addLots)
{
    RefCountedWithDestructor::s_wasDestructed = false;
    {
        Set set;
        RefCountedWithDestructor* hasDestructor = new RefCountedWithDestructor();
        set.add(adoptRef(hasDestructor));
        EXPECT_FALSE(RefCountedWithDestructor::s_wasDestructed);

        if (addLots) {
            for (int i = 0; i < 1000; i++) {
                set.add(adoptRef(new RefCountedWithDestructor()));
            }
        }

        EXPECT_FALSE(RefCountedWithDestructor::s_wasDestructed);
        Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        EXPECT_FALSE(RefCountedWithDestructor::s_wasDestructed);
    }
    // The destructors of the sets don't call the destructors of the elements
    // in the heap sets. You have to actually remove the elments, call clear()
    // or have a GC to get the destructors called.
    EXPECT_FALSE(RefCountedWithDestructor::s_wasDestructed);
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_TRUE(RefCountedWithDestructor::s_wasDestructed);
}

template<typename Set>
void destructorsCalledOnClear(bool addLots)
{
    RefCountedWithDestructor::s_wasDestructed = false;
    Set set;
    RefCountedWithDestructor* hasDestructor = new RefCountedWithDestructor();
    set.add(adoptRef(hasDestructor));
    EXPECT_FALSE(RefCountedWithDestructor::s_wasDestructed);

    if (addLots) {
        for (int i = 0; i < 1000; i++) {
            set.add(adoptRef(new RefCountedWithDestructor()));
        }
    }

    EXPECT_FALSE(RefCountedWithDestructor::s_wasDestructed);
    set.clear();
    EXPECT_TRUE(RefCountedWithDestructor::s_wasDestructed);
}

TEST(HeapTest, DestructorsCalled)
{
    HeapHashMap<SimpleClassWithDestructor*, OwnPtr<SimpleClassWithDestructor> > map;
    SimpleClassWithDestructor* hasDestructor = new SimpleClassWithDestructor();
    map.add(hasDestructor, adoptPtr(hasDestructor));
    SimpleClassWithDestructor::s_wasDestructed = false;
    map.clear();
    EXPECT_TRUE(SimpleClassWithDestructor::s_wasDestructed);

    destructorsCalledOnClear<HeapHashSet<RefPtr<RefCountedWithDestructor> > >(false);
    destructorsCalledOnClear<HeapListHashSet<RefPtr<RefCountedWithDestructor> > >(false);
    destructorsCalledOnClear<HeapLinkedHashSet<RefPtr<RefCountedWithDestructor> > >(false);
    destructorsCalledOnClear<HeapHashSet<RefPtr<RefCountedWithDestructor> > >(true);
    destructorsCalledOnClear<HeapListHashSet<RefPtr<RefCountedWithDestructor> > >(true);
    destructorsCalledOnClear<HeapLinkedHashSet<RefPtr<RefCountedWithDestructor> > >(true);

    destructorsCalledOnGC<HeapHashSet<RefPtr<RefCountedWithDestructor> > >(false);
    destructorsCalledOnGC<HeapListHashSet<RefPtr<RefCountedWithDestructor> > >(false);
    destructorsCalledOnGC<HeapLinkedHashSet<RefPtr<RefCountedWithDestructor> > >(false);
    destructorsCalledOnGC<HeapHashSet<RefPtr<RefCountedWithDestructor> > >(true);
    destructorsCalledOnGC<HeapListHashSet<RefPtr<RefCountedWithDestructor> > >(true);
    destructorsCalledOnGC<HeapLinkedHashSet<RefPtr<RefCountedWithDestructor> > >(true);
}

class MixinA : public GarbageCollectedMixin {
public:
    MixinA() : m_obj(IntWrapper::create(100)) { }
    virtual void trace(Visitor* visitor)
    {
        visitor->trace(m_obj);
    }
    Member<IntWrapper> m_obj;
};

class MixinB : public GarbageCollectedMixin {
public:
    MixinB() : m_obj(IntWrapper::create(101)) { }
    virtual void trace(Visitor* visitor)
    {
        visitor->trace(m_obj);
    }
    Member<IntWrapper> m_obj;
};

class MultipleMixins : public GarbageCollected<MultipleMixins>, public MixinA, public MixinB {
    USING_GARBAGE_COLLECTED_MIXIN(MultipleMixins);
public:
    MultipleMixins() : m_obj(IntWrapper::create(102)) { }
    virtual void trace(Visitor* visitor)
    {
        visitor->trace(m_obj);
        MixinA::trace(visitor);
        MixinB::trace(visitor);
    }
    Member<IntWrapper> m_obj;
};

static const bool s_isMixinTrue = IsGarbageCollectedMixin<MultipleMixins>::value;
static const bool s_isMixinFalse = IsGarbageCollectedMixin<IntWrapper>::value;

TEST(HeapTest, MultipleMixins)
{
    EXPECT_TRUE(s_isMixinTrue);
    EXPECT_FALSE(s_isMixinFalse);

    HeapStats initialHeapSize;
    clearOutOldGarbage(&initialHeapSize);
    IntWrapper::s_destructorCalls = 0;
    MultipleMixins* obj = new MultipleMixins();
    {
        Persistent<MixinA> a = obj;
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(0, IntWrapper::s_destructorCalls);
    }
    {
        Persistent<MixinB> b = obj;
        Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
        EXPECT_EQ(0, IntWrapper::s_destructorCalls);
    }
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(3, IntWrapper::s_destructorCalls);
}

class GCParkingThreadTester {
public:
    static void test()
    {
        createThread(&sleeperMainFunc, 0, "SleepingThread");

        // Wait for the sleeper to run.
        while (!s_sleeperRunning) {
            yield();
        }

        {
            // Expect the first attempt to park the sleeping thread to fail
            TestGCScope scope(ThreadState::NoHeapPointersOnStack);
            EXPECT_FALSE(scope.allThreadsParked());
        }

        s_sleeperDone = true;

        // Wait for the sleeper to finish.
        while (s_sleeperRunning) {
            // We enter the safepoint here since the sleeper thread will detach
            // causing it to GC.
            ThreadState::current()->safePoint(ThreadState::NoHeapPointersOnStack);
            yield();
        }
        {
            // Since the sleeper thread has detached this is the only thread.
            TestGCScope scope(ThreadState::NoHeapPointersOnStack);
            EXPECT_TRUE(scope.allThreadsParked());
        }
    }

private:
    static void sleeperMainFunc(void* data)
    {
        ThreadState::attach();
        s_sleeperRunning = true;

        // Simulate a long running op that is not entering a safepoint.
        while (!s_sleeperDone) {
            yield();
        }

        ThreadState::detach();
        s_sleeperRunning = false;
    }

    static volatile bool s_sleeperRunning;
    static volatile bool s_sleeperDone;
};

volatile bool GCParkingThreadTester::s_sleeperRunning = false;
volatile bool GCParkingThreadTester::s_sleeperDone = false;

TEST(HeapTest, GCParkingTimeout)
{
    GCParkingThreadTester::test();
}

TEST(HeapTest, NeedsAdjustAndMark)
{
    // class Mixin : public GarbageCollectedMixin {};
    EXPECT_TRUE(NeedsAdjustAndMark<Mixin>::value);
    EXPECT_TRUE(NeedsAdjustAndMark<const Mixin>::value);

    // class SimpleObject : public GarbageCollected<SimpleObject> {};
    EXPECT_FALSE(NeedsAdjustAndMark<SimpleObject>::value);
    EXPECT_FALSE(NeedsAdjustAndMark<const SimpleObject>::value);

    // class UseMixin : public SimpleObject, public Mixin {};
    EXPECT_FALSE(NeedsAdjustAndMark<UseMixin>::value);
    EXPECT_FALSE(NeedsAdjustAndMark<const UseMixin>::value);
}

template<typename Set>
void setWithCustomWeaknessHandling()
{
    typedef typename Set::iterator Iterator;
    Persistent<IntWrapper> livingInt(IntWrapper::create(42));
    Persistent<Set> set1(new Set());
    {
        Set set2;
        Set* set3 = new Set();
        set2.add(PairWithWeakHandling(IntWrapper::create(0), IntWrapper::create(1)));
        set3->add(PairWithWeakHandling(IntWrapper::create(2), IntWrapper::create(3)));
        set1->add(PairWithWeakHandling(IntWrapper::create(4), IntWrapper::create(5)));
        Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        // The first set is pointed to from a persistent, so it's referenced, but
        // the weak processing may have taken place.
        if (set1->size()) {
            Iterator i1 = set1->begin();
            EXPECT_EQ(4, i1->first->value());
            EXPECT_EQ(5, i1->second->value());
        }
        // The second set is on-stack, so its backing store must be referenced from
        // the stack. That makes the weak references strong.
        Iterator i2 = set2.begin();
        EXPECT_EQ(0, i2->first->value());
        EXPECT_EQ(1, i2->second->value());
        // The third set is pointed to from the stack, so it's referenced, but the
        // weak processing may have taken place.
        if (set3->size()) {
            Iterator i3 = set3->begin();
            EXPECT_EQ(2, i3->first->value());
            EXPECT_EQ(3, i3->second->value());
        }
    }
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0u, set1->size());
    set1->add(PairWithWeakHandling(IntWrapper::create(103), livingInt));
    set1->add(PairWithWeakHandling(livingInt, IntWrapper::create(103))); // This one gets zapped at GC time because nothing holds the 103 alive.
    set1->add(PairWithWeakHandling(IntWrapper::create(103), IntWrapper::create(103))); // This one gets zapped too.
    set1->add(PairWithWeakHandling(livingInt, livingInt));
    set1->add(PairWithWeakHandling(livingInt, livingInt)); // This one is identical to the previous and doesn't add anything.
    EXPECT_EQ(4u, set1->size());
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(2u, set1->size());
    Iterator i1 = set1->begin();
    EXPECT_TRUE(i1->first->value() == 103 || i1->first == livingInt);
    EXPECT_EQ(livingInt, i1->second);
    ++i1;
    EXPECT_TRUE(i1->first->value() == 103 || i1->first == livingInt);
    EXPECT_EQ(livingInt, i1->second);
}

TEST(HeapTest, SetWithCustomWeaknessHandling)
{
    setWithCustomWeaknessHandling<HeapHashSet<PairWithWeakHandling> >();
    setWithCustomWeaknessHandling<HeapLinkedHashSet<PairWithWeakHandling> >();
}

TEST(HeapTest, MapWithCustomWeaknessHandling)
{
    typedef HeapHashMap<PairWithWeakHandling, RefPtr<OffHeapInt> > Map;
    typedef Map::iterator Iterator;
    HeapStats initialHeapSize;
    clearOutOldGarbage(&initialHeapSize);
    OffHeapInt::s_destructorCalls = 0;

    Persistent<Map> map1(new Map());
    Persistent<IntWrapper> livingInt(IntWrapper::create(42));
    {
        Map map2;
        Map* map3 = new Map();
        map2.add(PairWithWeakHandling(IntWrapper::create(0), IntWrapper::create(1)), OffHeapInt::create(1001));
        map3->add(PairWithWeakHandling(IntWrapper::create(2), IntWrapper::create(3)), OffHeapInt::create(1002));
        map1->add(PairWithWeakHandling(IntWrapper::create(4), IntWrapper::create(5)), OffHeapInt::create(1003));
        EXPECT_EQ(0, OffHeapInt::s_destructorCalls);

        Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        // The first map2 is pointed to from a persistent, so it's referenced, but
        // the weak processing may have taken place.
        if (map1->size()) {
            Iterator i1 = map1->begin();
            EXPECT_EQ(4, i1->key.first->value());
            EXPECT_EQ(5, i1->key.second->value());
            EXPECT_EQ(1003, i1->value->value());
        }
        // The second map2 is on-stack, so its backing store must be referenced from
        // the stack. That makes the weak references strong.
        Iterator i2 = map2.begin();
        EXPECT_EQ(0, i2->key.first->value());
        EXPECT_EQ(1, i2->key.second->value());
        EXPECT_EQ(1001, i2->value->value());
        // The third map2 is pointed to from the stack, so it's referenced, but the
        // weak processing may have taken place.
        if (map3->size()) {
            Iterator i3 = map3->begin();
            EXPECT_EQ(2, i3->key.first->value());
            EXPECT_EQ(3, i3->key.second->value());
            EXPECT_EQ(1002, i3->value->value());
        }
    }
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);

    EXPECT_EQ(0u, map1->size());
    EXPECT_EQ(3, OffHeapInt::s_destructorCalls);

    OffHeapInt::s_destructorCalls = 0;

    map1->add(PairWithWeakHandling(IntWrapper::create(103), livingInt), OffHeapInt::create(2000));
    map1->add(PairWithWeakHandling(livingInt, IntWrapper::create(103)), OffHeapInt::create(2001)); // This one gets zapped at GC time because nothing holds the 103 alive.
    map1->add(PairWithWeakHandling(IntWrapper::create(103), IntWrapper::create(103)), OffHeapInt::create(2002)); // This one gets zapped too.
    RefPtr<OffHeapInt> dupeInt(OffHeapInt::create(2003));
    map1->add(PairWithWeakHandling(livingInt, livingInt), dupeInt);
    map1->add(PairWithWeakHandling(livingInt, livingInt), dupeInt); // This one is identical to the previous and doesn't add anything.
    dupeInt.clear();

    EXPECT_EQ(0, OffHeapInt::s_destructorCalls);
    EXPECT_EQ(4u, map1->size());
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(2, OffHeapInt::s_destructorCalls);
    EXPECT_EQ(2u, map1->size());
    Iterator i1 = map1->begin();
    EXPECT_TRUE(i1->key.first->value() == 103 || i1->key.first == livingInt);
    EXPECT_EQ(livingInt, i1->key.second);
    ++i1;
    EXPECT_TRUE(i1->key.first->value() == 103 || i1->key.first == livingInt);
    EXPECT_EQ(livingInt, i1->key.second);
}

TEST(HeapTest, MapWithCustomWeaknessHandling2)
{
    typedef HeapHashMap<RefPtr<OffHeapInt>, PairWithWeakHandling> Map;
    typedef Map::iterator Iterator;
    HeapStats initialHeapSize;
    clearOutOldGarbage(&initialHeapSize);
    OffHeapInt::s_destructorCalls = 0;

    Persistent<Map> map1(new Map());
    Persistent<IntWrapper> livingInt(IntWrapper::create(42));

    {
        Map map2;
        Map* map3 = new Map();
        map2.add(OffHeapInt::create(1001), PairWithWeakHandling(IntWrapper::create(0), IntWrapper::create(1)));
        map3->add(OffHeapInt::create(1002), PairWithWeakHandling(IntWrapper::create(2), IntWrapper::create(3)));
        map1->add(OffHeapInt::create(1003), PairWithWeakHandling(IntWrapper::create(4), IntWrapper::create(5)));
        EXPECT_EQ(0, OffHeapInt::s_destructorCalls);

        Heap::collectGarbage(ThreadState::HeapPointersOnStack);
        // The first map2 is pointed to from a persistent, so it's referenced, but
        // the weak processing may have taken place.
        if (map1->size()) {
            Iterator i1 = map1->begin();
            EXPECT_EQ(4, i1->value.first->value());
            EXPECT_EQ(5, i1->value.second->value());
            EXPECT_EQ(1003, i1->key->value());
        }
        // The second map2 is on-stack, so its backing store must be referenced from
        // the stack. That makes the weak references strong.
        Iterator i2 = map2.begin();
        EXPECT_EQ(0, i2->value.first->value());
        EXPECT_EQ(1, i2->value.second->value());
        EXPECT_EQ(1001, i2->key->value());
        // The third map2 is pointed to from the stack, so it's referenced, but the
        // weak processing may have taken place.
        if (map3->size()) {
            Iterator i3 = map3->begin();
            EXPECT_EQ(2, i3->value.first->value());
            EXPECT_EQ(3, i3->value.second->value());
            EXPECT_EQ(1002, i3->key->value());
        }
    }
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);

    EXPECT_EQ(0u, map1->size());
    EXPECT_EQ(3, OffHeapInt::s_destructorCalls);

    OffHeapInt::s_destructorCalls = 0;

    map1->add(OffHeapInt::create(2000), PairWithWeakHandling(IntWrapper::create(103), livingInt));
    map1->add(OffHeapInt::create(2001), PairWithWeakHandling(livingInt, IntWrapper::create(103))); // This one gets zapped at GC time because nothing holds the 103 alive.
    map1->add(OffHeapInt::create(2002), PairWithWeakHandling(IntWrapper::create(103), IntWrapper::create(103))); // This one gets zapped too.
    RefPtr<OffHeapInt> dupeInt(OffHeapInt::create(2003));
    map1->add(dupeInt, PairWithWeakHandling(livingInt, livingInt));
    map1->add(dupeInt, PairWithWeakHandling(livingInt, livingInt)); // This one is identical to the previous and doesn't add anything.
    dupeInt.clear();

    EXPECT_EQ(0, OffHeapInt::s_destructorCalls);
    EXPECT_EQ(4u, map1->size());
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(2, OffHeapInt::s_destructorCalls);
    EXPECT_EQ(2u, map1->size());
    Iterator i1 = map1->begin();
    EXPECT_TRUE(i1->value.first->value() == 103 || i1->value.first == livingInt);
    EXPECT_EQ(livingInt, i1->value.second);
    ++i1;
    EXPECT_TRUE(i1->value.first->value() == 103 || i1->value.first == livingInt);
    EXPECT_EQ(livingInt, i1->value.second);
}

TEST(HeapTest, Bind)
{
    Closure closure = bind(&Bar::trace, Bar::create(), static_cast<Visitor*>(0));
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    // The closure should have a persistent handle to the Bar.
    EXPECT_EQ(1u, Bar::s_live);

    Closure closure2 = bind(&Bar::trace, RawPtr<Bar>(Bar::create()), static_cast<Visitor*>(0));
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    // The closure should have a persistent handle to the Bar.
    EXPECT_EQ(2u, Bar::s_live);
    // RawPtr<OffHeapInt> should not make Persistent.
    Closure closure3 = bind(&OffHeapInt::voidFunction, RawPtr<OffHeapInt>(OffHeapInt::create(1).get()));

    UseMixin::s_traceCount = 0;
    Mixin* mixin = UseMixin::create();
    Closure mixinClosure = bind(&Mixin::trace, mixin, static_cast<Visitor*>(0));
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    // The closure should have a persistent handle to the mixin.
    EXPECT_EQ(1, UseMixin::s_traceCount);
}

typedef HeapHashSet<WeakMember<IntWrapper> > WeakSet;

// These special traits will remove a set from a map when the set is empty.
struct EmptyClearingHastSetTraits : HashTraits<WeakSet> {
    static const WTF::WeakHandlingFlag weakHandlingFlag = WTF::WeakHandlingInCollections;
    static bool shouldRemoveFromCollection(Visitor* visitor, WeakSet& set)
    {
        return set.isEmpty(); // Remove this set from any maps it is in.
    }
    static void traceInCollection(Visitor* visitor, WeakSet& set, WebCore::ShouldWeakPointersBeMarkedStrongly strongify)
    {
        // We just trace normally, which will invoke the normal weak handling
        // of the set, removing individual items.
        set.trace(visitor);
    }
};

// This is an example to show how you can remove entries from a T->WeakSet map
// when the weak sets become empty. For this example we are using a type that
// is given to use (HeapHashSet) rather than a type of our own. This means:
// 1) We can't just override the HashTrait for the type since this would affect
//    all collections that use this kind of weak set. Instead we have our own
//    traits and use a map with custom traits for the value type. These traits
//    are the 5th template parameter, so we have to supply default values for
//    the 3rd and 4th template parameters
// 2) We can't just inherit from WeakHandlingHashTraits, since that trait
//    assumes we can add methods to the type, but we can't add methods to
//    HeapHashSet.
TEST(HeapTest, RemoveEmptySets)
{
    HeapStats initialHeapSize;
    clearOutOldGarbage(&initialHeapSize);
    OffHeapInt::s_destructorCalls = 0;

    Persistent<IntWrapper> livingInt(IntWrapper::create(42));

    typedef RefPtr<OffHeapInt> Key;
    typedef HeapHashMap<Key, WeakSet, WTF::DefaultHash<Key>::Hash, HashTraits<Key>, EmptyClearingHastSetTraits> Map;
    Persistent<Map> map(new Map());
    map->add(OffHeapInt::create(1), WeakSet());
    {
        WeakSet& set = map->begin()->value;
        set.add(IntWrapper::create(103)); // Weak set can't hold this long.
        set.add(livingInt); // This prevents the set from being emptied.
        EXPECT_EQ(2u, set.size());
    }

    // The set we add here is empty, so the entry will be removed from the map
    // at the next GC.
    map->add(OffHeapInt::create(2), WeakSet());
    EXPECT_EQ(2u, map->size());

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(1u, map->size()); // The one with key 2 was removed.
    EXPECT_EQ(1, OffHeapInt::s_destructorCalls);
    {
        WeakSet& set = map->begin()->value;
        EXPECT_EQ(1u, set.size());
    }

    livingInt.clear(); // The weak set can no longer keep the '42' alive now.
    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    if (map->size() == 1u) {
        // If the weak processing for the set ran after the weak processing for
        // the map, then the set was not empty, and so the entry in the map was
        // not removed yet.
        WeakSet& set = map->begin()->value;
        EXPECT_EQ(0u, set.size());
    }

    Heap::collectGarbage(ThreadState::NoHeapPointersOnStack);
    EXPECT_EQ(0u, map->size());
}

} // WebCore namespace

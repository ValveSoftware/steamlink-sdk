/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include <qv4engine_p.h>
#include <qv4context_p.h>
#include <qv4value_inl_p.h>
#include <qv4object_p.h>
#include <qv4objectproto_p.h>
#include <qv4objectiterator_p.h>
#include <qv4arrayobject_p.h>
#include <qv4booleanobject_p.h>
#include <qv4globalobject_p.h>
#include <qv4errorobject_p.h>
#include <qv4functionobject_p.h>
#include "qv4function_p.h"
#include <qv4mathobject_p.h>
#include <qv4numberobject_p.h>
#include <qv4regexpobject_p.h>
#include <qv4regexp_p.h>
#include <qv4variantobject_p.h>
#include <qv4runtime_p.h>
#include "qv4mm_p.h"
#include <qv4argumentsobject_p.h>
#include <qv4dateobject_p.h>
#include <qv4jsonobject_p.h>
#include <qv4stringobject_p.h>
#include <qv4identifiertable_p.h>
#include "qv4debugging_p.h"
#include "qv4profiling_p.h"
#include "qv4executableallocator_p.h"
#include "qv4sequenceobject_p.h"
#include "qv4qobjectwrapper_p.h"
#include "qv4qmlextensions_p.h"
#include "qv4memberdata_p.h"

#include <QtCore/QTextStream>
#include <QDateTime>

#ifdef V4_ENABLE_JIT
#include "qv4isel_masm_p.h"
#endif // V4_ENABLE_JIT

#include "qv4isel_moth_p.h"

#if USE(PTHREADS)
#  include <pthread.h>
#  include <sys/resource.h>
#if HAVE(PTHREAD_NP_H)
#  include <pthread_np.h>
#endif
#endif

QT_BEGIN_NAMESPACE

using namespace QV4;

static QBasicAtomicInt engineSerial = Q_BASIC_ATOMIC_INITIALIZER(1);

static ReturnedValue throwTypeError(CallContext *ctx)
{
    return ctx->throwTypeError();
}

const int MinimumStackSize = 256; // in kbytes

quintptr getStackLimit()
{
    quintptr stackLimit;
#if USE(PTHREADS) && !OS(QNX)
#  if OS(DARWIN)
    pthread_t thread_self = pthread_self();
    void *stackTop = pthread_get_stackaddr_np(thread_self);
    stackLimit = reinterpret_cast<quintptr>(stackTop);
    quintptr size = 0;
    if (pthread_main_np()) {
        rlimit limit;
        getrlimit(RLIMIT_STACK, &limit);
        size = limit.rlim_cur;
    } else
        size = pthread_get_stacksize_np(thread_self);
    stackLimit -= size;
#  else
    void* stackBottom = 0;
    pthread_attr_t attr;
#if HAVE(PTHREAD_NP_H) && OS(FREEBSD)
    if (pthread_attr_get_np(pthread_self(), &attr) == 0) {
#else
    if (pthread_getattr_np(pthread_self(), &attr) == 0) {
#endif
        size_t stackSize = 0;
        pthread_attr_getstack(&attr, &stackBottom, &stackSize);
        pthread_attr_destroy(&attr);

#        if defined(Q_OS_ANDROID)
        // Bionic pretends that the main thread has a tiny stack; work around it
        if (gettid() == getpid()) {
            rlimit limit;
            getrlimit(RLIMIT_STACK, &limit);
            stackBottom = reinterpret_cast<void*>(reinterpret_cast<quintptr>(stackBottom) + stackSize - limit.rlim_cur);
        }
#       endif

        stackLimit = reinterpret_cast<quintptr>(stackBottom);
    } else {
        int dummy;
        // this is inexact, as part of the stack is used when being called here,
        // but let's simply default to 1MB from where the stack is right now
        stackLimit = reinterpret_cast<qintptr>(&dummy) - 1024*1024;
    }

#  endif
// This is wrong. StackLimit is the currently committed stack size, not the real end.
// only way to get that limit is apparently by using VirtualQuery (Yuck)
//#elif OS(WINDOWS)
//    PNT_TIB tib = (PNT_TIB)NtCurrentTeb();
//    stackLimit = static_cast<quintptr>(tib->StackLimit);
#else
    int dummy;
    // this is inexact, as part of the stack is used when being called here,
    // but let's simply default to 1MB from where the stack is right now
    stackLimit = reinterpret_cast<qintptr>(&dummy) - 1024*1024;
#endif

    // 256k slack
    return stackLimit + MinimumStackSize*1024;
}


ExecutionEngine::ExecutionEngine(EvalISelFactory *factory)
    : current(0)
    , memoryManager(new QV4::MemoryManager)
    , executableAllocator(new QV4::ExecutableAllocator)
    , regExpAllocator(new QV4::ExecutableAllocator)
    , bumperPointerAllocator(new WTF::BumpPointerAllocator)
    , jsStack(new WTF::PageAllocation)
    , debugger(0)
    , profiler(0)
    , globalObject(0)
    , globalCode(0)
    , v8Engine(0)
    , argumentsAccessors(0)
    , nArgumentsAccessors(0)
    , m_engineId(engineSerial.fetchAndAddOrdered(1))
    , regExpCache(0)
    , m_multiplyWrappedQObjects(0)
    , m_qmlExtensions(0)
{
    MemoryManager::GCBlocker gcBlocker(memoryManager);

    exceptionValue = Encode::undefined();
    hasException = false;

    if (!factory) {

#ifdef V4_ENABLE_JIT
        static const bool forceMoth = !qgetenv("QV4_FORCE_INTERPRETER").isEmpty();
        if (forceMoth)
            factory = new Moth::ISelFactory;
        else
            factory = new JIT::ISelFactory;
#else // !V4_ENABLE_JIT
        factory = new Moth::ISelFactory;
#endif // V4_ENABLE_JIT
    }
    iselFactory.reset(factory);

    memoryManager->setExecutionEngine(this);

    // reserve space for the JS stack
    // we allow it to grow to 2 times JSStackLimit, as we can overshoot due to garbage collection
    // and ScopedValues allocated outside of JIT'ed methods.
    *jsStack = WTF::PageAllocation::allocate(2*JSStackLimit, WTF::OSAllocator::JSVMStackPages, true);
    jsStackBase = (Value *)jsStack->base();
    jsStackTop = jsStackBase;

    // set up stack limits
    jsStackLimit = jsStackBase + JSStackLimit/sizeof(Value);
    cStackLimit = getStackLimit();
    if (!recheckCStackLimits())
        qFatal("Fatal: Not enough stack space available for QML. Please increase the process stack size to more than %d KBytes.", MinimumStackSize);

    Scope scope(this);

    identifierTable = new IdentifierTable(this);

    classPool = new InternalClassPool;

    emptyClass =  new (classPool) InternalClass(this);
    executionContextClass = InternalClass::create(this, ExecutionContext::staticVTable(), 0);
    constructClass = InternalClass::create(this, Object::staticVTable(), 0);
    stringClass = InternalClass::create(this, String::staticVTable(), 0);
    regExpValueClass = InternalClass::create(this, RegExp::staticVTable(), 0);

    id_empty = newIdentifier(QString());
    id_undefined = newIdentifier(QStringLiteral("undefined"));
    id_null = newIdentifier(QStringLiteral("null"));
    id_true = newIdentifier(QStringLiteral("true"));
    id_false = newIdentifier(QStringLiteral("false"));
    id_boolean = newIdentifier(QStringLiteral("boolean"));
    id_number = newIdentifier(QStringLiteral("number"));
    id_string = newIdentifier(QStringLiteral("string"));
    id_object = newIdentifier(QStringLiteral("object"));
    id_function = newIdentifier(QStringLiteral("function"));
    id_length = newIdentifier(QStringLiteral("length"));
    id_prototype = newIdentifier(QStringLiteral("prototype"));
    id_constructor = newIdentifier(QStringLiteral("constructor"));
    id_arguments = newIdentifier(QStringLiteral("arguments"));
    id_caller = newIdentifier(QStringLiteral("caller"));
    id_callee = newIdentifier(QStringLiteral("callee"));
    id_this = newIdentifier(QStringLiteral("this"));
    id___proto__ = newIdentifier(QStringLiteral("__proto__"));
    id_enumerable = newIdentifier(QStringLiteral("enumerable"));
    id_configurable = newIdentifier(QStringLiteral("configurable"));
    id_writable = newIdentifier(QStringLiteral("writable"));
    id_value = newIdentifier(QStringLiteral("value"));
    id_get = newIdentifier(QStringLiteral("get"));
    id_set = newIdentifier(QStringLiteral("set"));
    id_eval = newIdentifier(QStringLiteral("eval"));
    id_uintMax = newIdentifier(QStringLiteral("4294967295"));
    id_name = newIdentifier(QStringLiteral("name"));
    id_index = newIdentifier(QStringLiteral("index"));
    id_input = newIdentifier(QStringLiteral("input"));
    id_toString = newIdentifier(QStringLiteral("toString"));
    id_destroy = newIdentifier(QStringLiteral("destroy"));
    id_valueOf = newIdentifier(QStringLiteral("valueOf"));

    memberDataClass = InternalClass::create(this, MemberData::staticVTable(), 0);

    ScopedObject objectPrototype(scope, memoryManager->alloc<ObjectPrototype>(InternalClass::create(this, ObjectPrototype::staticVTable(), 0)));
    objectClass = InternalClass::create(this, Object::staticVTable(), objectPrototype);
    Q_ASSERT(objectClass->vtable == Object::staticVTable());

    arrayClass = InternalClass::create(this, ArrayObject::staticVTable(), objectPrototype);
    arrayClass = arrayClass->addMember(id_length, Attr_NotConfigurable|Attr_NotEnumerable);
    ScopedObject arrayPrototype(scope, memoryManager->alloc<ArrayPrototype>(arrayClass));
    arrayClass = arrayClass->changePrototype(arrayPrototype);

    simpleArrayDataClass = InternalClass::create(this, SimpleArrayData::staticVTable(), 0);

    InternalClass *argsClass = InternalClass::create(this, ArgumentsObject::staticVTable(), objectPrototype);
    argsClass = argsClass->addMember(id_length, Attr_NotEnumerable);
    argumentsObjectClass = argsClass->addMember(id_callee, Attr_Data|Attr_NotEnumerable);
    strictArgumentsObjectClass = argsClass->addMember(id_callee, Attr_Accessor|Attr_NotConfigurable|Attr_NotEnumerable);
    strictArgumentsObjectClass = strictArgumentsObjectClass->addMember(id_caller, Attr_Accessor|Attr_NotConfigurable|Attr_NotEnumerable);
    Q_ASSERT(argumentsObjectClass->vtable == ArgumentsObject::staticVTable());
    Q_ASSERT(strictArgumentsObjectClass->vtable == ArgumentsObject::staticVTable());

    initRootContext();

    ScopedObject stringPrototype(scope, memoryManager->alloc<StringPrototype>(InternalClass::create(this, StringPrototype::staticVTable(), objectPrototype)));
    stringObjectClass = InternalClass::create(this, String::staticVTable(), stringPrototype);

    ScopedObject numberPrototype(scope, memoryManager->alloc<NumberPrototype>(InternalClass::create(this, NumberPrototype::staticVTable(), objectPrototype)));
    numberClass = InternalClass::create(this, NumberObject::staticVTable(), numberPrototype);

    ScopedObject booleanPrototype(scope, memoryManager->alloc<BooleanPrototype>(InternalClass::create(this, BooleanPrototype::staticVTable(), objectPrototype)));
    booleanClass = InternalClass::create(this, BooleanObject::staticVTable(), booleanPrototype);

    ScopedObject datePrototype(scope, memoryManager->alloc<DatePrototype>(InternalClass::create(this, DatePrototype::staticVTable(), objectPrototype)));
    dateClass = InternalClass::create(this, DateObject::staticVTable(), datePrototype);

    InternalClass *functionProtoClass = InternalClass::create(this, FunctionObject::staticVTable(), objectPrototype);
    uint index;
    functionProtoClass = functionProtoClass->addMember(id_prototype, Attr_NotEnumerable, &index);
    Q_ASSERT(index == FunctionObject::Index_Prototype);
    ScopedObject functionPrototype(scope, memoryManager->alloc<FunctionPrototype>(functionProtoClass));
    functionClass = InternalClass::create(this, FunctionObject::staticVTable(), functionPrototype);
    functionClass = functionClass->addMember(id_prototype, Attr_NotEnumerable|Attr_NotConfigurable, &index);
    Q_ASSERT(index == FunctionObject::Index_Prototype);
    protoClass = objectClass->addMember(id_constructor, Attr_NotEnumerable, &index);
    Q_ASSERT(index == FunctionObject::Index_ProtoConstructor);

    Scoped<RegExpPrototype> regExpPrototype(scope, memoryManager->alloc<RegExpPrototype>(InternalClass::create(this, RegExpPrototype::staticVTable(), objectPrototype)));
    regExpClass = InternalClass::create(this, RegExpObject::staticVTable(), regExpPrototype.getPointer());
    regExpExecArrayClass = arrayClass->addMember(id_index, Attr_Data, &index);
    Q_ASSERT(index == RegExpObject::Index_ArrayIndex);
    regExpExecArrayClass = regExpExecArrayClass->addMember(id_input, Attr_Data, &index);
    Q_ASSERT(index == RegExpObject::Index_ArrayInput);

    ScopedObject errorPrototype(scope, memoryManager->alloc<ErrorPrototype>(InternalClass::create(this, ErrorObject::staticVTable(), objectPrototype)));
    errorClass = InternalClass::create(this, ErrorObject::staticVTable(), errorPrototype);
    ScopedObject evalErrorPrototype(scope, memoryManager->alloc<EvalErrorPrototype>(errorClass));
    evalErrorClass = InternalClass::create(this, EvalErrorObject::staticVTable(), evalErrorPrototype);
    ScopedObject rangeErrorPrototype(scope, memoryManager->alloc<RangeErrorPrototype>(errorClass));
    rangeErrorClass = InternalClass::create(this, RangeErrorObject::staticVTable(), rangeErrorPrototype);
    ScopedObject referenceErrorPrototype(scope, memoryManager->alloc<ReferenceErrorPrototype>(errorClass));
    referenceErrorClass = InternalClass::create(this, ReferenceErrorObject::staticVTable(), referenceErrorPrototype);
    ScopedObject syntaxErrorPrototype(scope, memoryManager->alloc<SyntaxErrorPrototype>(errorClass));
    syntaxErrorClass = InternalClass::create(this, SyntaxErrorObject::staticVTable(), syntaxErrorPrototype);
    ScopedObject typeErrorPrototype(scope, memoryManager->alloc<TypeErrorPrototype>(errorClass));
    typeErrorClass = InternalClass::create(this, TypeErrorObject::staticVTable(), typeErrorPrototype);
    ScopedObject uRIErrorPrototype(scope, memoryManager->alloc<URIErrorPrototype>(errorClass));
    uriErrorClass = InternalClass::create(this, URIErrorObject::staticVTable(), uRIErrorPrototype);

    ScopedObject variantPrototype(scope, memoryManager->alloc<VariantPrototype>(InternalClass::create(this, VariantPrototype::staticVTable(), objectPrototype)));
    variantClass = InternalClass::create(this, VariantObject::staticVTable(), variantPrototype);
    Q_ASSERT(variantClass->prototype == variantPrototype);
    Q_ASSERT(variantPrototype->internalClass()->prototype == objectPrototype);

    sequencePrototype = ScopedValue(scope, memoryManager->alloc<SequencePrototype>(arrayClass));

    objectCtor = memoryManager->alloc<ObjectCtor>(rootContext);
    stringCtor = memoryManager->alloc<StringCtor>(rootContext);
    numberCtor = memoryManager->alloc<NumberCtor>(rootContext);
    booleanCtor = memoryManager->alloc<BooleanCtor>(rootContext);
    arrayCtor = memoryManager->alloc<ArrayCtor>(rootContext);
    functionCtor = memoryManager->alloc<FunctionCtor>(rootContext);
    dateCtor = memoryManager->alloc<DateCtor>(rootContext);
    regExpCtor = memoryManager->alloc<RegExpCtor>(rootContext);
    errorCtor = memoryManager->alloc<ErrorCtor>(rootContext);
    evalErrorCtor = memoryManager->alloc<EvalErrorCtor>(rootContext);
    rangeErrorCtor = memoryManager->alloc<RangeErrorCtor>(rootContext);
    referenceErrorCtor = memoryManager->alloc<ReferenceErrorCtor>(rootContext);
    syntaxErrorCtor = memoryManager->alloc<SyntaxErrorCtor>(rootContext);
    typeErrorCtor = memoryManager->alloc<TypeErrorCtor>(rootContext);
    uRIErrorCtor = memoryManager->alloc<URIErrorCtor>(rootContext);

    static_cast<ObjectPrototype *>(objectPrototype.getPointer())->init(this, objectCtor.asObject());
    static_cast<StringPrototype *>(stringPrototype.getPointer())->init(this, stringCtor.asObject());
    static_cast<NumberPrototype *>(numberPrototype.getPointer())->init(this, numberCtor.asObject());
    static_cast<BooleanPrototype *>(booleanPrototype.getPointer())->init(this, booleanCtor.asObject());
    static_cast<ArrayPrototype *>(arrayPrototype.getPointer())->init(this, arrayCtor.asObject());
    static_cast<DatePrototype *>(datePrototype.getPointer())->init(this, dateCtor.asObject());
    static_cast<FunctionPrototype *>(functionPrototype.getPointer())->init(this, functionCtor.asObject());
    static_cast<RegExpPrototype *>(regExpPrototype.getPointer())->init(this, regExpCtor.asObject());
    static_cast<ErrorPrototype *>(errorPrototype.getPointer())->init(this, errorCtor.asObject());
    static_cast<EvalErrorPrototype *>(evalErrorPrototype.getPointer())->init(this, evalErrorCtor.asObject());
    static_cast<RangeErrorPrototype *>(rangeErrorPrototype.getPointer())->init(this, rangeErrorCtor.asObject());
    static_cast<ReferenceErrorPrototype *>(referenceErrorPrototype.getPointer())->init(this, referenceErrorCtor.asObject());
    static_cast<SyntaxErrorPrototype *>(syntaxErrorPrototype.getPointer())->init(this, syntaxErrorCtor.asObject());
    static_cast<TypeErrorPrototype *>(typeErrorPrototype.getPointer())->init(this, typeErrorCtor.asObject());
    static_cast<URIErrorPrototype *>(uRIErrorPrototype.getPointer())->init(this, uRIErrorCtor.asObject());

    static_cast<VariantPrototype *>(variantPrototype.getPointer())->init();
    static_cast<SequencePrototype *>(sequencePrototype.managed())->init();

    //
    // set up the global object
    //
    globalObject = newObject()->getPointer();
    rootContext->d()->global = globalObject;
    rootContext->d()->callData->thisObject = globalObject;
    Q_ASSERT(globalObject->internalClass()->vtable);

    globalObject->defineDefaultProperty(QStringLiteral("Object"), objectCtor);
    globalObject->defineDefaultProperty(QStringLiteral("String"), stringCtor);
    globalObject->defineDefaultProperty(QStringLiteral("Number"), numberCtor);
    globalObject->defineDefaultProperty(QStringLiteral("Boolean"), booleanCtor);
    globalObject->defineDefaultProperty(QStringLiteral("Array"), arrayCtor);
    globalObject->defineDefaultProperty(QStringLiteral("Function"), functionCtor);
    globalObject->defineDefaultProperty(QStringLiteral("Date"), dateCtor);
    globalObject->defineDefaultProperty(QStringLiteral("RegExp"), regExpCtor);
    globalObject->defineDefaultProperty(QStringLiteral("Error"), errorCtor);
    globalObject->defineDefaultProperty(QStringLiteral("EvalError"), evalErrorCtor);
    globalObject->defineDefaultProperty(QStringLiteral("RangeError"), rangeErrorCtor);
    globalObject->defineDefaultProperty(QStringLiteral("ReferenceError"), referenceErrorCtor);
    globalObject->defineDefaultProperty(QStringLiteral("SyntaxError"), syntaxErrorCtor);
    globalObject->defineDefaultProperty(QStringLiteral("TypeError"), typeErrorCtor);
    globalObject->defineDefaultProperty(QStringLiteral("URIError"), uRIErrorCtor);
    ScopedObject o(scope);
    globalObject->defineDefaultProperty(QStringLiteral("Math"), (o = memoryManager->alloc<MathObject>(QV4::InternalClass::create(this, MathObject::staticVTable(), objectPrototype))));
    globalObject->defineDefaultProperty(QStringLiteral("JSON"), (o = memoryManager->alloc<JsonObject>(QV4::InternalClass::create(this, JsonObject::staticVTable(), objectPrototype))));

    globalObject->defineReadonlyProperty(QStringLiteral("undefined"), Primitive::undefinedValue());
    globalObject->defineReadonlyProperty(QStringLiteral("NaN"), Primitive::fromDouble(std::numeric_limits<double>::quiet_NaN()));
    globalObject->defineReadonlyProperty(QStringLiteral("Infinity"), Primitive::fromDouble(Q_INFINITY));


    evalFunction = Scoped<EvalFunction>(scope, memoryManager->alloc<EvalFunction>(rootContext));
    globalObject->defineDefaultProperty(QStringLiteral("eval"), (o = evalFunction));

    globalObject->defineDefaultProperty(QStringLiteral("parseInt"), GlobalFunctions::method_parseInt, 2);
    globalObject->defineDefaultProperty(QStringLiteral("parseFloat"), GlobalFunctions::method_parseFloat, 1);
    globalObject->defineDefaultProperty(QStringLiteral("isNaN"), GlobalFunctions::method_isNaN, 1);
    globalObject->defineDefaultProperty(QStringLiteral("isFinite"), GlobalFunctions::method_isFinite, 1);
    globalObject->defineDefaultProperty(QStringLiteral("decodeURI"), GlobalFunctions::method_decodeURI, 1);
    globalObject->defineDefaultProperty(QStringLiteral("decodeURIComponent"), GlobalFunctions::method_decodeURIComponent, 1);
    globalObject->defineDefaultProperty(QStringLiteral("encodeURI"), GlobalFunctions::method_encodeURI, 1);
    globalObject->defineDefaultProperty(QStringLiteral("encodeURIComponent"), GlobalFunctions::method_encodeURIComponent, 1);
    globalObject->defineDefaultProperty(QStringLiteral("escape"), GlobalFunctions::method_escape, 1);
    globalObject->defineDefaultProperty(QStringLiteral("unescape"), GlobalFunctions::method_unescape, 1);

    Scoped<String> name(scope, newString(QStringLiteral("thrower")));
    thrower = ScopedFunctionObject(scope, BuiltinFunction::create(rootContext, name.getPointer(), throwTypeError)).getPointer();
}

ExecutionEngine::~ExecutionEngine()
{
    delete debugger;
    debugger = 0;
    delete profiler;
    profiler = 0;
    delete m_multiplyWrappedQObjects;
    m_multiplyWrappedQObjects = 0;
    delete identifierTable;
    delete memoryManager;

    QSet<QV4::CompiledData::CompilationUnit*> remainingUnits;
    qSwap(compilationUnits, remainingUnits);
    foreach (QV4::CompiledData::CompilationUnit *unit, remainingUnits)
        unit->unlink();

    delete m_qmlExtensions;
    emptyClass->destroy();
    delete classPool;
    delete bumperPointerAllocator;
    delete regExpCache;
    delete regExpAllocator;
    delete executableAllocator;
    jsStack->deallocate();
    delete jsStack;
    delete [] argumentsAccessors;
}

void ExecutionEngine::enableDebugger()
{
    Q_ASSERT(!debugger);
    debugger = new Debugging::Debugger(this);
    iselFactory.reset(new Moth::ISelFactory);
}

void ExecutionEngine::enableProfiler()
{
    Q_ASSERT(!profiler);
    profiler = new QV4::Profiling::Profiler(this);
}

void ExecutionEngine::initRootContext()
{
    GlobalContext *r = static_cast<GlobalContext*>(memoryManager->allocManaged(sizeof(GlobalContext::Data) + sizeof(CallData)));
    new (r->d()) GlobalContext::Data(this);
    r->d()->callData = reinterpret_cast<CallData *>(r->d() + 1);
    r->d()->callData->tag = QV4::Value::_Integer_Type;
    r->d()->callData->argc = 0;
    r->d()->callData->thisObject = globalObject;
    r->d()->callData->args[0] = Encode::undefined();

    rootContext = r;
}

InternalClass *ExecutionEngine::newClass(const InternalClass &other)
{
    return new (classPool) InternalClass(other);
}

ExecutionContext *ExecutionEngine::pushGlobalContext()
{
    GlobalContext *g = memoryManager->alloc<GlobalContext>(this);
    g->d()->callData = rootContext->d()->callData;

    Q_ASSERT(currentContext() == g);
    return g;
}


Returned<Object> *ExecutionEngine::newObject()
{
    Scope scope(this);
    ScopedObject object(scope, memoryManager->alloc<Object>(this));
    return object->asReturned<Object>();
}

Returned<Object> *ExecutionEngine::newObject(InternalClass *internalClass)
{
    Scope scope(this);
    ScopedObject object(scope, memoryManager->alloc<Object>(internalClass));
    return object->asReturned<Object>();
}

Returned<String> *ExecutionEngine::newString(const QString &s)
{
    Scope scope(this);
    return ScopedString(scope, memoryManager->alloc<String>(this, s))->asReturned<String>();
}

String *ExecutionEngine::newIdentifier(const QString &text)
{
    return identifierTable->insertString(text);
}

Returned<Object> *ExecutionEngine::newStringObject(const ValueRef value)
{
    Scope scope(this);
    Scoped<StringObject> object(scope, memoryManager->alloc<StringObject>(this, value));
    return object->asReturned<Object>();
}

Returned<Object> *ExecutionEngine::newNumberObject(const ValueRef value)
{
    Scope scope(this);
    Scoped<NumberObject> object(scope, memoryManager->alloc<NumberObject>(this, value));
    return object->asReturned<Object>();
}

Returned<Object> *ExecutionEngine::newBooleanObject(const ValueRef value)
{
    Scope scope(this);
    ScopedObject object(scope, memoryManager->alloc<BooleanObject>(this, value));
    return object->asReturned<Object>();
}

Returned<ArrayObject> *ExecutionEngine::newArrayObject(int count)
{
    Scope scope(this);
    ScopedArrayObject object(scope, memoryManager->alloc<ArrayObject>(this));

    if (count) {
        if (count < 0x1000)
            object->arrayReserve(count);
        object->setArrayLengthUnchecked(count);
    }
    return object->asReturned<ArrayObject>();
}

Returned<ArrayObject> *ExecutionEngine::newArrayObject(const QStringList &list)
{
    Scope scope(this);
    ScopedArrayObject object(scope, memoryManager->alloc<ArrayObject>(this, list));
    return object->asReturned<ArrayObject>();
}

Returned<ArrayObject> *ExecutionEngine::newArrayObject(InternalClass *ic)
{
    Scope scope(this);
    ScopedArrayObject object(scope, memoryManager->alloc<ArrayObject>(ic));
    return object->asReturned<ArrayObject>();
}


Returned<DateObject> *ExecutionEngine::newDateObject(const ValueRef value)
{
    Scope scope(this);
    Scoped<DateObject> object(scope, memoryManager->alloc<DateObject>(this, value));
    return object->asReturned<DateObject>();
}

Returned<DateObject> *ExecutionEngine::newDateObject(const QDateTime &dt)
{
    Scope scope(this);
    Scoped<DateObject> object(scope, memoryManager->alloc<DateObject>(this, dt));
    return object->asReturned<DateObject>();
}

Returned<RegExpObject> *ExecutionEngine::newRegExpObject(const QString &pattern, int flags)
{
    bool global = (flags & IR::RegExp::RegExp_Global);
    bool ignoreCase = false;
    bool multiline = false;
    if (flags & IR::RegExp::RegExp_IgnoreCase)
        ignoreCase = true;
    if (flags & IR::RegExp::RegExp_Multiline)
        multiline = true;

    Scope scope(this);
    Scoped<RegExp> re(scope, RegExp::create(this, pattern, ignoreCase, multiline));
    return newRegExpObject(re, global);
}

Returned<RegExpObject> *ExecutionEngine::newRegExpObject(RegExp *re, bool global)
{
    Scope scope(this);
    Scoped<RegExpObject> object(scope, memoryManager->alloc<RegExpObject>(this, re, global));
    return object->asReturned<RegExpObject>();
}

Returned<RegExpObject> *ExecutionEngine::newRegExpObject(const QRegExp &re)
{
    Scope scope(this);
    Scoped<RegExpObject> object(scope, memoryManager->alloc<RegExpObject>(this, re));
    return object->asReturned<RegExpObject>();
}

Returned<Object> *ExecutionEngine::newErrorObject(const ValueRef value)
{
    Scope scope(this);
    ScopedObject object(scope, memoryManager->alloc<ErrorObject>(errorClass, value));
    return object->asReturned<Object>();
}

Returned<Object> *ExecutionEngine::newSyntaxErrorObject(const QString &message)
{
    Scope scope(this);
    ScopedString s(scope, newString(message));
    ScopedObject error(scope, memoryManager->alloc<SyntaxErrorObject>(this, s));
    return error->asReturned<Object>();
}

Returned<Object> *ExecutionEngine::newSyntaxErrorObject(const QString &message, const QString &fileName, int line, int column)
{
    Scope scope(this);
    ScopedObject error(scope, memoryManager->alloc<SyntaxErrorObject>(this, message, fileName, line, column));
    return error->asReturned<Object>();
}


Returned<Object> *ExecutionEngine::newReferenceErrorObject(const QString &message)
{
    Scope scope(this);
    ScopedObject o(scope, memoryManager->alloc<ReferenceErrorObject>(this, message));
    return o->asReturned<Object>();
}

Returned<Object> *ExecutionEngine::newReferenceErrorObject(const QString &message, const QString &fileName, int lineNumber, int columnNumber)
{
    Scope scope(this);
    ScopedObject o(scope, memoryManager->alloc<ReferenceErrorObject>(this, message, fileName, lineNumber, columnNumber));
    return o->asReturned<Object>();
}


Returned<Object> *ExecutionEngine::newTypeErrorObject(const QString &message)
{
    Scope scope(this);
    ScopedObject o(scope, memoryManager->alloc<TypeErrorObject>(this, message));
    return o->asReturned<Object>();
}

Returned<Object> *ExecutionEngine::newRangeErrorObject(const QString &message)
{
    Scope scope(this);
    ScopedObject o(scope, memoryManager->alloc<RangeErrorObject>(this, message));
    return o->asReturned<Object>();
}

Returned<Object> *ExecutionEngine::newURIErrorObject(const ValueRef message)
{
    Scope scope(this);
    ScopedObject o(scope, memoryManager->alloc<URIErrorObject>(this, message));
    return o->asReturned<Object>();
}

Returned<Object> *ExecutionEngine::newVariantObject(const QVariant &v)
{
    Scope scope(this);
    ScopedObject o(scope, memoryManager->alloc<VariantObject>(this, v));
    return o->asReturned<Object>();
}

Returned<Object> *ExecutionEngine::newForEachIteratorObject(ExecutionContext *ctx, Object *o)
{
    Scope scope(this);
    ScopedObject obj(scope, memoryManager->alloc<ForEachIteratorObject>(ctx, o));
    return obj->asReturned<Object>();
}

Returned<Object> *ExecutionEngine::qmlContextObject() const
{
    ExecutionContext *ctx = currentContext();

    if (ctx->d()->type == QV4::ExecutionContext::Type_SimpleCallContext && !ctx->d()->outer)
        ctx = ctx->d()->parent;

    if (!ctx->d()->outer)
        return 0;

    while (ctx->d()->outer && ctx->d()->outer->d()->type != ExecutionContext::Type_GlobalContext)
        ctx = ctx->d()->outer;

    Q_ASSERT(ctx);
    if (ctx->d()->type != ExecutionContext::Type_QmlContext)
        return 0;

    return static_cast<CallContext *>(ctx)->d()->activation->asReturned<Object>();
}

QVector<StackFrame> ExecutionEngine::stackTrace(int frameLimit) const
{
    Scope scope(this->currentContext());
    ScopedString name(scope);
    QVector<StackFrame> stack;

    QV4::ExecutionContext *c = currentContext();
    while (c && frameLimit) {
        if (FunctionObject *function = c->getFunctionObject()) {
            StackFrame frame;
            if (const Function *f = function->function())
                frame.source = f->sourceFile();
            name = function->name();
            frame.function = name->toQString();
            frame.line = -1;
            frame.column = -1;

            if (function->function())
                // line numbers can be negative for places where you can't set a real breakpoint
                frame.line = qAbs(c->d()->lineNumber);

            stack.append(frame);
            --frameLimit;
        }
        c = c->d()->parent;
    }

    if (frameLimit && globalCode) {
        StackFrame frame;
        frame.source = globalCode->sourceFile();
        frame.function = globalCode->name()->toQString();
        frame.line = rootContext->d()->lineNumber;
        frame.column = -1;

        stack.append(frame);
    }
    return stack;
}

StackFrame ExecutionEngine::currentStackFrame() const
{
    StackFrame frame;
    frame.line = -1;
    frame.column = -1;

    QVector<StackFrame> trace = stackTrace(/*limit*/ 1);
    if (!trace.isEmpty())
        frame = trace.first();

    return frame;
}

/* Helper and "C" linkage exported function to format a GDBMI stacktrace for
 * invocation by a debugger.
 * Sample GDB invocation: print qt_v4StackTrace((void*)0x7fffffffb290)
 * Sample CDB invocation: .call Qt5Qmld!qt_v4StackTrace(0x7fffffffb290) ; gh
 * Note: The helper is there to suppress MSVC warning 4190 about anything
 * with UDT return types in a "C" linkage function. */

static inline char *v4StackTrace(const ExecutionContext *context)
{
    QString result;
    QTextStream str(&result);
    str << "stack=[";
    if (context && context->d()->engine) {
        const QVector<StackFrame> stackTrace = context->d()->engine->stackTrace(20);
        for (int i = 0; i < stackTrace.size(); ++i) {
            if (i)
                str << ',';
            const QUrl url(stackTrace.at(i).source);
            const QString fileName = url.isLocalFile() ? url.toLocalFile() : url.toString();
            str << "frame={level=\"" << i << "\",func=\"" << stackTrace.at(i).function
                << "\",file=\"" << fileName << "\",fullname=\"" << fileName
                << "\",line=\"" << stackTrace.at(i).line << "\",language=\"js\"}";
        }
    }
    str << ']';
    return qstrdup(result.toLocal8Bit().constData());
}

extern "C" Q_QML_EXPORT char *qt_v4StackTrace(void *executionContext)
{
    return v4StackTrace(reinterpret_cast<const ExecutionContext *>(executionContext));
}

QUrl ExecutionEngine::resolvedUrl(const QString &file)
{
    QUrl src(file);
    if (!src.isRelative())
        return src;

    QUrl base;
    QV4::ExecutionContext *c = currentContext();
    while (c) {
        CallContext *callCtx = c->asCallContext();
        if (callCtx && callCtx->d()->function) {
            if (callCtx->d()->function->function())
                base.setUrl(callCtx->d()->function->function()->sourceFile());
            break;
        }
        c = c->d()->parent;
    }

    if (base.isEmpty() && globalCode)
        base.setUrl(globalCode->sourceFile());

    if (base.isEmpty())
        return src;

    return base.resolved(src);
}

void ExecutionEngine::requireArgumentsAccessors(int n)
{
    if (n <= nArgumentsAccessors)
        return;

    Scope scope(this);
    ScopedFunctionObject get(scope);
    ScopedFunctionObject set(scope);

    if (n >= nArgumentsAccessors) {
        Property *oldAccessors = argumentsAccessors;
        int oldSize = nArgumentsAccessors;
        nArgumentsAccessors = qMax(8, n);
        argumentsAccessors = new Property[nArgumentsAccessors];
        if (oldAccessors) {
            memcpy(argumentsAccessors, oldAccessors, oldSize*sizeof(Property));
            delete [] oldAccessors;
        }
        for (int i = oldSize; i < nArgumentsAccessors; ++i) {
            argumentsAccessors[i].value = ScopedValue(scope, memoryManager->alloc<ArgumentsGetterFunction>(rootContext, i));
            argumentsAccessors[i].set = ScopedValue(scope, memoryManager->alloc<ArgumentsSetterFunction>(rootContext, i));
        }
    }
}

void ExecutionEngine::markObjects()
{
    identifierTable->mark(this);

    globalObject->mark(this);

    for (int i = 0; i < nArgumentsAccessors; ++i) {
        const Property &pd = argumentsAccessors[i];
        if (FunctionObject *getter = pd.getter())
            getter->mark(this);
        if (FunctionObject *setter = pd.setter())
            setter->mark(this);
    }

    ExecutionContext *c = currentContext();
    while (c) {
        Q_ASSERT(c->inUse());
        if (!c->markBit()) {
            c->d()->markBit = 1;
            c->markObjects(c, this);
        }
        c = c->d()->parent;
    }

    id_empty->mark(this);
    id_undefined->mark(this);
    id_null->mark(this);
    id_true->mark(this);
    id_false->mark(this);
    id_boolean->mark(this);
    id_number->mark(this);
    id_string->mark(this);
    id_object->mark(this);
    id_function->mark(this);
    id_length->mark(this);
    id_prototype->mark(this);
    id_constructor->mark(this);
    id_arguments->mark(this);
    id_caller->mark(this);
    id_callee->mark(this);
    id_this->mark(this);
    id___proto__->mark(this);
    id_enumerable->mark(this);
    id_configurable->mark(this);
    id_writable->mark(this);
    id_value->mark(this);
    id_get->mark(this);
    id_set->mark(this);
    id_eval->mark(this);
    id_uintMax->mark(this);
    id_name->mark(this);
    id_index->mark(this);
    id_input->mark(this);
    id_toString->mark(this);
    id_destroy->mark(this);
    id_valueOf->mark(this);

    objectCtor.mark(this);
    stringCtor.mark(this);
    numberCtor.mark(this);
    booleanCtor.mark(this);
    arrayCtor.mark(this);
    functionCtor.mark(this);
    dateCtor.mark(this);
    regExpCtor.mark(this);
    errorCtor.mark(this);
    evalErrorCtor.mark(this);
    rangeErrorCtor.mark(this);
    referenceErrorCtor.mark(this);
    syntaxErrorCtor.mark(this);
    typeErrorCtor.mark(this);
    uRIErrorCtor.mark(this);
    sequencePrototype.mark(this);

    exceptionValue.mark(this);

    thrower->mark(this);

    if (m_qmlExtensions)
        m_qmlExtensions->markObjects(this);

    classPool->markObjects(this);

    for (QSet<CompiledData::CompilationUnit*>::ConstIterator it = compilationUnits.constBegin(), end = compilationUnits.constEnd();
         it != end; ++it)
        (*it)->markObjects(this);
}

QmlExtensions *ExecutionEngine::qmlExtensions()
{
    if (!m_qmlExtensions)
        m_qmlExtensions = new QmlExtensions;
    return m_qmlExtensions;
}

ReturnedValue ExecutionEngine::throwException(const ValueRef value)
{
    // we can get in here with an exception already set, as the runtime
    // doesn't check after every operation that can throw.
    // in this case preserve the first exception to give correct error
    // information
    if (hasException)
        return Encode::undefined();

    hasException = true;
    exceptionValue = value;
    QV4::Scope scope(this);
    QV4::Scoped<ErrorObject> error(scope, value);
    if (!!error)
        exceptionStackTrace = error->d()->stackTrace;
    else
        exceptionStackTrace = stackTrace();

    if (debugger)
        debugger->aboutToThrow();

    return Encode::undefined();
}

ReturnedValue ExecutionEngine::catchException(ExecutionContext *catchingContext, StackTrace *trace)
{
    Q_ASSERT(hasException);
    Q_UNUSED(catchingContext);
    Q_ASSERT(currentContext() == catchingContext);
    if (trace)
        *trace = exceptionStackTrace;
    exceptionStackTrace.clear();
    hasException = false;
    ReturnedValue res = exceptionValue.asReturnedValue();
    exceptionValue = Primitive::emptyValue();
    return res;
}

QQmlError ExecutionEngine::catchExceptionAsQmlError(ExecutionContext *context)
{
    QV4::StackTrace trace;
    QV4::Scope scope(context);
    QV4::ScopedValue exception(scope, context->catchException(&trace));
    QQmlError error;
    if (!trace.isEmpty()) {
        QV4::StackFrame frame = trace.first();
        error.setUrl(QUrl(frame.source));
        error.setLine(frame.line);
        error.setColumn(frame.column);
    }
    QV4::Scoped<QV4::ErrorObject> errorObj(scope, exception);
    if (!!errorObj && errorObj->asSyntaxError()) {
        QV4::ScopedString m(scope, errorObj->engine()->newString(QStringLiteral("message")));
        QV4::ScopedValue v(scope, errorObj->get(m.getPointer()));
        error.setDescription(v->toQStringNoThrow());
    } else
        error.setDescription(exception->toQStringNoThrow());
    return error;
}

bool ExecutionEngine::recheckCStackLimits()
{
    int dummy;
#ifdef Q_OS_WIN
    // ### this is only required on windows, where we currently use heuristics to get the stack limit
    if (cStackLimit - reinterpret_cast<quintptr>(&dummy) > 128*1024)
        // we're more then 128k away from our stack limit, assume the thread has changed, and
        // call getStackLimit
#endif
    // this can happen after a thread change
    cStackLimit = getStackLimit();

    return (reinterpret_cast<quintptr>(&dummy) >= cStackLimit);
}

QT_END_NAMESPACE

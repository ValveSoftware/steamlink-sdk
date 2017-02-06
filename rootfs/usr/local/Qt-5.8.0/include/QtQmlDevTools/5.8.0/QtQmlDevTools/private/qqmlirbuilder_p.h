/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QQMLIRBUILDER_P_H
#define QQMLIRBUILDER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qqmljsast_p.h>
#include <private/qqmljsengine_p.h>
#include <private/qv4compiler_p.h>
#include <private/qv4compileddata_p.h>
#include <private/qqmljsmemorypool_p.h>
#include <private/qv4codegen_p.h>
#include <private/qv4compiler_p.h>
#include <private/qqmljslexer_p.h>
#include <QTextStream>
#include <QCoreApplication>

#ifndef V4_BOOTSTRAP
#include <private/qqmlpropertycache_p.h>
#endif

QT_BEGIN_NAMESPACE

class QQmlPropertyCache;
class QQmlContextData;
class QQmlTypeNameCache;

namespace QmlIR {

struct Document;
struct IRLoader;

template <typename T>
struct PoolList
{
    PoolList()
        : first(0)
        , last(0)
        , count(0)
    {}

    T *first;
    T *last;
    int count;

    int append(T *item) {
        item->next = 0;
        if (last)
            last->next = item;
        else
            first = item;
        last = item;
        return count++;
    }

    void prepend(T *item) {
        item->next = first;
        first = item;
        if (!last)
            last = first;
        ++count;
    }

    template <typename Sortable, typename Base, Sortable Base::*sortMember>
    T *findSortedInsertionPoint(T *item) const
    {
        T *insertPos = 0;

        for (T *it = first; it; it = it->next) {
            if (!(it->*sortMember <= item->*sortMember))
                break;
            insertPos = it;
        }

        return insertPos;
    }

    void insertAfter(T *insertionPoint, T *item) {
        if (!insertionPoint) {
            prepend(item);
        } else if (insertionPoint == last) {
            append(item);
        } else {
            item->next = insertionPoint->next;
            insertionPoint->next = item;
            ++count;
        }
    }

    T *unlink(T *before, T *item) {
        T * const newNext = item->next;

        if (before)
            before->next = newNext;
        else
            first = newNext;

        if (item == last) {
            if (newNext)
                last = newNext;
            else
                last = first;
        }

        --count;
        return newNext;
    }

    T *slowAt(int index) const
    {
        T *result = first;
        while (index > 0 && result) {
            result = result->next;
            --index;
        }
        return result;
    }

    struct Iterator {
        T *ptr;

        explicit Iterator(T *p) : ptr(p) {}

        T *operator->() {
            return ptr;
        }

        const T *operator->() const {
            return ptr;
        }

        T &operator*() {
            return *ptr;
        }

        const T &operator*() const {
            return *ptr;
        }

        void operator++() {
            ptr = ptr->next;
        }

        bool operator==(const Iterator &rhs) const {
            return ptr == rhs.ptr;
        }

        bool operator!=(const Iterator &rhs) const {
            return ptr != rhs.ptr;
        }
    };

    Iterator begin() { return Iterator(first); }
    Iterator end() { return Iterator(nullptr); }
};

template <typename T>
class FixedPoolArray
{
    T *data;
public:
    int count;

    FixedPoolArray()
        : data(0)
        , count(0)
    {}

    void allocate(QQmlJS::MemoryPool *pool, int size)
    {
        count = size;
        data = reinterpret_cast<T*>(pool->allocate(count * sizeof(T)));
    }

    void allocate(QQmlJS::MemoryPool *pool, const QVector<T> &vector)
    {
        count = vector.count();
        data = reinterpret_cast<T*>(pool->allocate(count * sizeof(T)));

        if (QTypeInfo<T>::isComplex) {
            for (int i = 0; i < count; ++i)
                new (data + i) T(vector.at(i));
        } else {
            memcpy(data, static_cast<const void*>(vector.constData()), count * sizeof(T));
        }
    }

    template <typename Container>
    void allocate(QQmlJS::MemoryPool *pool, const Container &container)
    {
        count = container.count();
        data = reinterpret_cast<T*>(pool->allocate(count * sizeof(T)));
        typename Container::ConstIterator it = container.constBegin();
        for (int i = 0; i < count; ++i)
            new (data + i) T(*it++);
    }

    const T &at(int index) const {
        Q_ASSERT(index >= 0 && index < count);
        return data[index];
    }

    T &operator[](int index) {
        Q_ASSERT(index >= 0 && index < count);
        return data[index];
    }


    int indexOf(const T &value) const {
        for (int i = 0; i < count; ++i)
            if (data[i] == value)
                return i;
        return -1;
    }

    const T *begin() const { return data; }
    const T *end() const { return data + count; }
};

struct Object;

struct SignalParameter : public QV4::CompiledData::Parameter
{
    SignalParameter *next;
};

struct Signal
{
    int nameIndex;
    QV4::CompiledData::Location location;
    PoolList<SignalParameter> *parameters;

    QStringList parameterStringList(const QV4::Compiler::StringTableGenerator *stringPool) const;

    int parameterCount() const { return parameters->count; }
    PoolList<SignalParameter>::Iterator parametersBegin() const { return parameters->begin(); }
    PoolList<SignalParameter>::Iterator parametersEnd() const { return parameters->end(); }

    Signal *next;
};

struct Property : public QV4::CompiledData::Property
{
    Property *next;
};

struct Binding : public QV4::CompiledData::Binding
{
    // The offset in the source file where the binding appeared. This is used for sorting to ensure
    // that assignments to list properties are done in the correct order. We use the offset here instead
    // of Binding::location as the latter has limited precision.
    quint32 offset;
    // Binding's compiledScriptIndex is index in object's functionsAndExpressions
    Binding *next;
};

struct Alias : public QV4::CompiledData::Alias
{
    Alias *next;
};

struct Function
{
    QQmlJS::AST::FunctionDeclaration *functionDeclaration;
    QV4::CompiledData::Location location;
    int nameIndex;
    quint32 index; // index in parsedQML::functions
    FixedPoolArray<int> formals;

    // --- QQmlPropertyCacheCreator interface
    const int *formalsBegin() const { return formals.begin(); }
    const int *formalsEnd() const { return formals.end(); }
    // ---

    Function *next;
};

struct Q_QML_PRIVATE_EXPORT CompiledFunctionOrExpression
{
    CompiledFunctionOrExpression()
        : node(0)
        , nameIndex(0)
        , disableAcceleratedLookups(false)
        , next(0)
    {}
    CompiledFunctionOrExpression(QQmlJS::AST::Node *n)
        : node(n)
        , nameIndex(0)
        , disableAcceleratedLookups(false)
        , next(0)
    {}
    QQmlJS::AST::Node *node; // FunctionDeclaration, Statement or Expression
    quint32 nameIndex;
    bool disableAcceleratedLookups;
    CompiledFunctionOrExpression *next;
};

struct Q_QML_PRIVATE_EXPORT Object
{
    Q_DECLARE_TR_FUNCTIONS(Object)
public:
    quint32 inheritedTypeNameIndex;
    quint32 idNameIndex;
    int id;
    int indexOfDefaultPropertyOrAlias;
    bool defaultPropertyIsAlias;
    quint32 flags;

    QV4::CompiledData::Location location;
    QV4::CompiledData::Location locationOfIdProperty;

    const Property *firstProperty() const { return properties->first; }
    int propertyCount() const { return properties->count; }
    Alias *firstAlias() const { return aliases->first; }
    int aliasCount() const { return aliases->count; }
    const Signal *firstSignal() const { return qmlSignals->first; }
    int signalCount() const { return qmlSignals->count; }
    Binding *firstBinding() const { return bindings->first; }
    int bindingCount() const { return bindings->count; }
    const Function *firstFunction() const { return functions->first; }
    int functionCount() const { return functions->count; }

    PoolList<Binding>::Iterator bindingsBegin() const { return bindings->begin(); }
    PoolList<Binding>::Iterator bindingsEnd() const { return bindings->end(); }
    PoolList<Property>::Iterator propertiesBegin() const { return properties->begin(); }
    PoolList<Property>::Iterator propertiesEnd() const { return properties->end(); }
    PoolList<Alias>::Iterator aliasesBegin() const { return aliases->begin(); }
    PoolList<Alias>::Iterator aliasesEnd() const { return aliases->end(); }
    PoolList<Signal>::Iterator signalsBegin() const { return qmlSignals->begin(); }
    PoolList<Signal>::Iterator signalsEnd() const { return qmlSignals->end(); }
    PoolList<Function>::Iterator functionsBegin() const { return functions->begin(); }
    PoolList<Function>::Iterator functionsEnd() const { return functions->end(); }

    // If set, then declarations for this object (and init bindings for these) should go into the
    // specified object. Used for declarations inside group properties.
    Object *declarationsOverride;

    void init(QQmlJS::MemoryPool *pool, int typeNameIndex, int idIndex, const QQmlJS::AST::SourceLocation &location = QQmlJS::AST::SourceLocation());

    QString sanityCheckFunctionNames(const QSet<QString> &illegalNames, QQmlJS::AST::SourceLocation *errorLocation);

    QString appendSignal(Signal *signal);
    QString appendProperty(Property *prop, const QString &propertyName, bool isDefaultProperty, const QQmlJS::AST::SourceLocation &defaultToken, QQmlJS::AST::SourceLocation *errorLocation);
    QString appendAlias(Alias *prop, const QString &aliasName, bool isDefaultProperty, const QQmlJS::AST::SourceLocation &defaultToken, QQmlJS::AST::SourceLocation *errorLocation);
    void appendFunction(QmlIR::Function *f);

    QString appendBinding(Binding *b, bool isListBinding);
    Binding *findBinding(quint32 nameIndex) const;
    Binding *unlinkBinding(Binding *before, Binding *binding) { return bindings->unlink(before, binding); }
    void insertSorted(Binding *b);
    QString bindingAsString(Document *doc, int scriptIndex) const;

    PoolList<CompiledFunctionOrExpression> *functionsAndExpressions;
    FixedPoolArray<int> runtimeFunctionIndices;

    FixedPoolArray<quint32> namedObjectsInComponent;
    int namedObjectsInComponentCount() const { return namedObjectsInComponent.count; }
    const quint32 *namedObjectsInComponentTable() const { return namedObjectsInComponent.begin(); }

private:
    friend struct IRLoader;

    PoolList<Property> *properties;
    PoolList<Alias> *aliases;
    PoolList<Signal> *qmlSignals;
    PoolList<Binding> *bindings;
    PoolList<Function> *functions;
};

struct Q_QML_PRIVATE_EXPORT Pragma
{
    enum PragmaType {
        PragmaSingleton = 0x1
    };
    quint32 type;

    QV4::CompiledData::Location location;
};

struct Q_QML_PRIVATE_EXPORT Document
{
    Document(bool debugMode);
    QString code;
    QQmlJS::Engine jsParserEngine;
    QV4::IR::Module jsModule;
    QList<const QV4::CompiledData::Import *> imports;
    QList<Pragma*> pragmas;
    QQmlJS::AST::UiProgram *program;
    int indexOfRootObject;
    QVector<Object*> objects;
    QV4::Compiler::JSUnitGenerator jsGenerator;

    QQmlRefPointer<QV4::CompiledData::CompilationUnit> javaScriptCompilationUnit;

    int registerString(const QString &str) { return jsGenerator.registerString(str); }
    QString stringAt(int index) const { return jsGenerator.stringForIndex(index); }

    static void removeScriptPragmas(QString &script);
};

struct Q_QML_PRIVATE_EXPORT ScriptDirectivesCollector : public QQmlJS::Directives
{
    ScriptDirectivesCollector(QQmlJS::Engine *engine, QV4::Compiler::JSUnitGenerator *unitGenerator);

    QQmlJS::Engine *engine;
    QV4::Compiler::JSUnitGenerator *jsGenerator;
    QList<const QV4::CompiledData::Import *> imports;
    bool hasPragmaLibrary;

    virtual void pragmaLibrary();
    virtual void importFile(const QString &jsfile, const QString &module, int lineNumber, int column);
    virtual void importModule(const QString &uri, const QString &version, const QString &module, int lineNumber, int column);
};

struct Q_QML_PRIVATE_EXPORT IRBuilder : public QQmlJS::AST::Visitor
{
    Q_DECLARE_TR_FUNCTIONS(QQmlCodeGenerator)
public:
    IRBuilder(const QSet<QString> &illegalNames);
    bool generateFromQml(const QString &code, const QString &url, Document *output);

    static bool isSignalPropertyName(const QString &name);

    using QQmlJS::AST::Visitor::visit;
    using QQmlJS::AST::Visitor::endVisit;

    virtual bool visit(QQmlJS::AST::UiArrayMemberList *ast);
    virtual bool visit(QQmlJS::AST::UiImport *ast);
    virtual bool visit(QQmlJS::AST::UiPragma *ast);
    virtual bool visit(QQmlJS::AST::UiHeaderItemList *ast);
    virtual bool visit(QQmlJS::AST::UiObjectInitializer *ast);
    virtual bool visit(QQmlJS::AST::UiObjectMemberList *ast);
    virtual bool visit(QQmlJS::AST::UiParameterList *ast);
    virtual bool visit(QQmlJS::AST::UiProgram *);
    virtual bool visit(QQmlJS::AST::UiQualifiedId *ast);
    virtual bool visit(QQmlJS::AST::UiArrayBinding *ast);
    virtual bool visit(QQmlJS::AST::UiObjectBinding *ast);
    virtual bool visit(QQmlJS::AST::UiObjectDefinition *ast);
    virtual bool visit(QQmlJS::AST::UiPublicMember *ast);
    virtual bool visit(QQmlJS::AST::UiScriptBinding *ast);
    virtual bool visit(QQmlJS::AST::UiSourceElement *ast);

    void accept(QQmlJS::AST::Node *node);

    // returns index in _objects
    bool defineQMLObject(int *objectIndex, QQmlJS::AST::UiQualifiedId *qualifiedTypeNameId, const QQmlJS::AST::SourceLocation &location, QQmlJS::AST::UiObjectInitializer *initializer, Object *declarationsOverride = 0);
    bool defineQMLObject(int *objectIndex, QQmlJS::AST::UiObjectDefinition *node, Object *declarationsOverride = 0)
    { return defineQMLObject(objectIndex, node->qualifiedTypeNameId, node->qualifiedTypeNameId->firstSourceLocation(), node->initializer, declarationsOverride); }

    static QString asString(QQmlJS::AST::UiQualifiedId *node);
    QStringRef asStringRef(QQmlJS::AST::Node *node);
    static void extractVersion(QStringRef string, int *maj, int *min);
    QStringRef textRefAt(const QQmlJS::AST::SourceLocation &loc) const
    { return QStringRef(&sourceCode, loc.offset, loc.length); }
    QStringRef textRefAt(const QQmlJS::AST::SourceLocation &first,
                         const QQmlJS::AST::SourceLocation &last) const;

    void setBindingValue(QV4::CompiledData::Binding *binding, QQmlJS::AST::Statement *statement);

    void appendBinding(QQmlJS::AST::UiQualifiedId *name, QQmlJS::AST::Statement *value);
    void appendBinding(QQmlJS::AST::UiQualifiedId *name, int objectIndex, bool isOnAssignment = false);
    void appendBinding(const QQmlJS::AST::SourceLocation &qualifiedNameLocation, const QQmlJS::AST::SourceLocation &nameLocation, quint32 propertyNameIndex, QQmlJS::AST::Statement *value);
    void appendBinding(const QQmlJS::AST::SourceLocation &qualifiedNameLocation, const QQmlJS::AST::SourceLocation &nameLocation, quint32 propertyNameIndex, int objectIndex, bool isListItem = false, bool isOnAssignment = false);

    bool appendAlias(QQmlJS::AST::UiPublicMember *node);

    Object *bindingsTarget() const;

    bool setId(const QQmlJS::AST::SourceLocation &idLocation, QQmlJS::AST::Statement *value);

    // resolves qualified name (font.pixelSize for example) and returns the last name along
    // with the object any right-hand-side of a binding should apply to.
    bool resolveQualifiedId(QQmlJS::AST::UiQualifiedId **nameToResolve, Object **object, bool onAssignment = false);

    void recordError(const QQmlJS::AST::SourceLocation &location, const QString &description);

    quint32 registerString(const QString &str) const { return jsGenerator->registerString(str); }
    template <typename _Tp> _Tp *New() { return pool->New<_Tp>(); }

    QString stringAt(int index) const { return jsGenerator->stringForIndex(index); }

    static bool isStatementNodeScript(QQmlJS::AST::Statement *statement);
    static bool isRedundantNullInitializerForPropertyDeclaration(Property *property, QQmlJS::AST::Statement *statement);

    QList<QQmlJS::DiagnosticMessage> errors;

    QSet<QString> illegalNames;

    QList<const QV4::CompiledData::Import *> _imports;
    QList<Pragma*> _pragmas;
    QVector<Object*> _objects;

    QV4::CompiledData::TypeReferenceMap _typeReferences;

    Object *_object;
    Property *_propertyDeclaration;

    QQmlJS::MemoryPool *pool;
    QString sourceCode;
    QV4::Compiler::JSUnitGenerator *jsGenerator;
};

struct Q_QML_PRIVATE_EXPORT QmlUnitGenerator
{
    QV4::CompiledData::Unit *generate(Document &output, QQmlEngine *engine, const QV4::CompiledData::ResolvedTypeReferenceMap &dependentTypes);

private:
    typedef bool (Binding::*BindingFilter)() const;
    char *writeBindings(char *bindingPtr, const Object *o, BindingFilter filter) const;
};

#ifndef V4_BOOTSTRAP
struct Q_QML_EXPORT PropertyResolver
{
    PropertyResolver(const QQmlPropertyCache *cache)
        : cache(cache)
    {}

    QQmlPropertyData *property(int index) const
    {
        return cache->property(index);
    }

    enum RevisionCheck {
        CheckRevision,
        IgnoreRevision
    };

    QQmlPropertyData *property(const QString &name, bool *notInRevision = 0, RevisionCheck check = CheckRevision) const;

    // This code must match the semantics of QQmlPropertyPrivate::findSignalByName
    QQmlPropertyData *signal(const QString &name, bool *notInRevision) const;

    const QQmlPropertyCache *cache;
};
#endif

struct Q_QML_PRIVATE_EXPORT JSCodeGen : public QQmlJS::Codegen
{
    JSCodeGen(const QString &fileName, const QString &sourceCode, QV4::IR::Module *jsModule,
              QQmlJS::Engine *jsEngine, QQmlJS::AST::UiProgram *qmlRoot, QQmlTypeNameCache *imports,
              const QV4::Compiler::StringTableGenerator *stringPool);

    struct IdMapping
    {
        QString name;
        int idIndex;
        QQmlPropertyCache *type;
    };
    typedef QVector<IdMapping> ObjectIdMapping;

    void beginContextScope(const ObjectIdMapping &objectIds, QQmlPropertyCache *contextObject);
    void beginObjectScope(QQmlPropertyCache *scopeObject);

    // Returns mapping from input functions to index in IR::Module::functions / compiledData->runtimeFunctions
    QVector<int> generateJSCodeForFunctionsAndBindings(const QList<CompiledFunctionOrExpression> &functions);

protected:
    virtual void beginFunctionBodyHook();
    virtual QV4::IR::Expr *fallbackNameLookup(const QString &name, int line, int col);

private:
    QQmlPropertyData *lookupQmlCompliantProperty(QQmlPropertyCache *cache, const QString &name, bool *propertyExistsButForceNameLookup = 0);

    QString sourceCode;
    QQmlJS::Engine *jsEngine; // needed for memory pool
    QQmlJS::AST::UiProgram *qmlRoot;
    QQmlTypeNameCache *imports;
    const QV4::Compiler::StringTableGenerator *stringPool;

    bool _disableAcceleratedLookups;
    ObjectIdMapping _idObjects;
    QQmlPropertyCache *_contextObject;
    QQmlPropertyCache *_scopeObject;
    int _qmlContextTemp;
    int _importedScriptsTemp;
};

} // namespace QmlIR

QT_END_NAMESPACE

#endif // QQMLIRBUILDER_P_H

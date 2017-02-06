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
#ifndef QQMLTYPECOMPILER_P_H
#define QQMLTYPECOMPILER_P_H

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

#include <qglobal.h>
#include <qqmlerror.h>
#include <qhash.h>
#include <private/qqmltypeloader_p.h>
#include <private/qqmlirbuilder_p.h>

QT_BEGIN_NAMESPACE

class QQmlEnginePrivate;
class QQmlError;
class QQmlTypeData;
class QQmlImports;

namespace QmlIR {
struct Document;
}

namespace QV4 {
namespace CompiledData {
struct QmlUnit;
struct Location;
}
}

struct QQmlCompileError
{
    QQmlCompileError() {}
    QQmlCompileError(const QV4::CompiledData::Location &location, const QString &description)
        : location(location), description(description) {}
    QV4::CompiledData::Location location;
    QString description;

    bool isSet() const { return !description.isEmpty(); }
};

struct QQmlTypeCompiler
{
    Q_DECLARE_TR_FUNCTIONS(QQmlTypeCompiler)
public:
    QQmlTypeCompiler(QQmlEnginePrivate *engine, QQmlTypeData *typeData, QmlIR::Document *document, const QQmlRefPointer<QQmlTypeNameCache> &importCache, const QV4::CompiledData::ResolvedTypeReferenceMap &resolvedTypeCache);

    // --- interface used by QQmlPropertyCacheCreator
    typedef QmlIR::Object CompiledObject;
    const QmlIR::Object *objectAt(int index) const { return document->objects.at(index); }
    int objectCount() const { return document->objects.count(); }
    QString stringAt(int idx) const;
    QmlIR::PoolList<QmlIR::Function>::Iterator objectFunctionsBegin(const QmlIR::Object *object) const { return object->functionsBegin(); }
    QmlIR::PoolList<QmlIR::Function>::Iterator objectFunctionsEnd(const QmlIR::Object *object) const { return object->functionsEnd(); }
    QV4::CompiledData::ResolvedTypeReferenceMap resolvedTypes;
    // ---

    QV4::CompiledData::CompilationUnit *compile();

    QList<QQmlError> compilationErrors() const { return errors; }
    void recordError(QQmlError error);
    void recordError(const QV4::CompiledData::Location &location, const QString &description);
    void recordError(const QQmlCompileError &error);

    int registerString(const QString &str);

    QV4::IR::Module *jsIRModule() const;

    const QV4::CompiledData::Unit *qmlUnit() const;

    QUrl url() const { return typeData->finalUrl(); }
    QQmlEnginePrivate *enginePrivate() const { return engine; }
    const QQmlImports *imports() const;
    QVector<QmlIR::Object *> *qmlObjects() const;
    int rootObjectIndex() const;
    void setPropertyCaches(QQmlPropertyCacheVector &&caches);
    const QQmlPropertyCacheVector *propertyCaches() const;
    QQmlPropertyCacheVector &&takePropertyCaches();
    void setComponentRoots(const QVector<quint32> &roots) { m_componentRoots = roots; }
    const QVector<quint32> &componentRoots() const { return m_componentRoots; }
    QQmlJS::MemoryPool *memoryPool();
    QStringRef newStringRef(const QString &string);
    const QV4::Compiler::StringTableGenerator *stringPool() const;
    void setBindingPropertyDataPerObject(const QVector<QV4::CompiledData::BindingPropertyData> &propertyData);

    const QHash<int, QQmlCustomParser*> &customParserCache() const { return customParsers; }

    QString bindingAsString(const QmlIR::Object *object, int scriptIndex) const;

    void addImport(const QString &module, const QString &qualifier, int majorVersion, int minorVersion);

private:
    QList<QQmlError> errors;
    QQmlEnginePrivate *engine;
    QQmlTypeData *typeData;
    QQmlRefPointer<QQmlTypeNameCache> importCache;
    QmlIR::Document *document;
    // index is string index of type name (use obj->inheritedTypeNameIndex)
    QHash<int, QQmlCustomParser*> customParsers;

    // index in first hash is component index, vector inside contains object indices of objects with id property
    QVector<quint32> m_componentRoots;
    QQmlPropertyCacheVector m_propertyCaches;
};

struct QQmlCompilePass
{
    QQmlCompilePass(QQmlTypeCompiler *typeCompiler);

    QString stringAt(int idx) const { return compiler->stringAt(idx); }
protected:
    void recordError(const QV4::CompiledData::Location &location, const QString &description) const
    { compiler->recordError(location, description); }
    void recordError(const QQmlCompileError &error)
    { compiler->recordError(error); }

    QQmlTypeCompiler *compiler;
};

// "Converts" signal expressions to full-fleged function declarations with
// parameters taken from the signal declarations
// It also updates the QV4::CompiledData::Binding objects to set the property name
// to the final signal name (onTextChanged -> textChanged) and sets the IsSignalExpression flag.
struct SignalHandlerConverter : public QQmlCompilePass
{
    Q_DECLARE_TR_FUNCTIONS(SignalHandlerConverter)
public:
    SignalHandlerConverter(QQmlTypeCompiler *typeCompiler);

    bool convertSignalHandlerExpressionsToFunctionDeclarations();

private:
    bool convertSignalHandlerExpressionsToFunctionDeclarations(const QmlIR::Object *obj, const QString &typeName, QQmlPropertyCache *propertyCache);

    QQmlEnginePrivate *enginePrivate;
    const QVector<QmlIR::Object*> &qmlObjects;
    const QQmlImports *imports;
    const QHash<int, QQmlCustomParser*> &customParsers;
    const QV4::CompiledData::ResolvedTypeReferenceMap &resolvedTypes;
    const QSet<QString> &illegalNames;
    const QQmlPropertyCacheVector * const propertyCaches;
};

// ### This will go away when the codegen resolves all enums to constant expressions
// and we replace the constant expression with a literal binding instead of using
// a script.
class QQmlEnumTypeResolver : public QQmlCompilePass
{
    Q_DECLARE_TR_FUNCTIONS(QQmlEnumTypeResolver)
public:
    QQmlEnumTypeResolver(QQmlTypeCompiler *typeCompiler);

    bool resolveEnumBindings();

private:
    bool assignEnumToBinding(QmlIR::Binding *binding, const QStringRef &enumName, int enumValue, bool isQtObject);
    bool assignEnumToBinding(QmlIR::Binding *binding, const QString &enumName, int enumValue, bool isQtObject)
    {
        return assignEnumToBinding(binding, QStringRef(&enumName), enumValue, isQtObject);
    }
    bool tryQualifiedEnumAssignment(const QmlIR::Object *obj, const QQmlPropertyCache *propertyCache,
                                    const QQmlPropertyData *prop,
                                    QmlIR::Binding *binding);
    int evaluateEnum(const QString &scope, const QByteArray &enumValue, bool *ok) const;


    const QVector<QmlIR::Object*> &qmlObjects;
    const QQmlPropertyCacheVector * const propertyCaches;
    const QQmlImports *imports;
    QV4::CompiledData::ResolvedTypeReferenceMap *resolvedTypes;
};

class QQmlCustomParserScriptIndexer: public QQmlCompilePass
{
public:
    QQmlCustomParserScriptIndexer(QQmlTypeCompiler *typeCompiler);

    void annotateBindingsWithScriptStrings();

private:
    void scanObjectRecursively(int objectIndex, bool annotateScriptBindings = false);

    const QVector<QmlIR::Object*> &qmlObjects;
    const QHash<int, QQmlCustomParser*> &customParsers;
};

// Annotate properties bound to aliases with a flag
class QQmlAliasAnnotator : public QQmlCompilePass
{
public:
    QQmlAliasAnnotator(QQmlTypeCompiler *typeCompiler);

    void annotateBindingsToAliases();
private:
    const QVector<QmlIR::Object*> &qmlObjects;
    const QQmlPropertyCacheVector * const propertyCaches;
};

class QQmlScriptStringScanner : public QQmlCompilePass
{
public:
    QQmlScriptStringScanner(QQmlTypeCompiler *typeCompiler);

    void scan();

private:
    const QVector<QmlIR::Object*> &qmlObjects;
    const QQmlPropertyCacheVector * const propertyCaches;
};

class QQmlComponentAndAliasResolver : public QQmlCompilePass
{
    Q_DECLARE_TR_FUNCTIONS(QQmlAnonymousComponentResolver)
public:
    QQmlComponentAndAliasResolver(QQmlTypeCompiler *typeCompiler);

    bool resolve();

protected:
    void findAndRegisterImplicitComponents(const QmlIR::Object *obj, QQmlPropertyCache *propertyCache);
    bool collectIdsAndAliases(int objectIndex);
    bool resolveAliases(int componentIndex);
    void propertyDataForAlias(QmlIR::Alias *alias, int *type, quint32 *propertyFlags);

    enum AliasResolutionResult {
        NoAliasResolved,
        SomeAliasesResolved,
        AllAliasesResolved
    };

    AliasResolutionResult resolveAliasesInObject(int objectIndex, QQmlCompileError *error);

    QQmlEnginePrivate *enginePrivate;
    QQmlJS::MemoryPool *pool;

    QVector<QmlIR::Object*> *qmlObjects;
    const int indexOfRootObject;

    // indices of the objects that are actually Component {}
    QVector<quint32> componentRoots;

    // Deliberate choice of map over hash here to ensure stable generated output.
    QMap<int, int> _idToObjectIndex;
    QVector<int> _objectsWithAliases;

    QV4::CompiledData::ResolvedTypeReferenceMap *resolvedTypes;
    QQmlPropertyCacheVector propertyCaches;
};

class QQmlDeferredAndCustomParserBindingScanner : public QQmlCompilePass
{
public:
    QQmlDeferredAndCustomParserBindingScanner(QQmlTypeCompiler *typeCompiler);

    bool scanObject();

private:
    bool scanObject(int objectIndex);

    QVector<QmlIR::Object*> *qmlObjects;
    const QQmlPropertyCacheVector * const propertyCaches;
    const QHash<int, QQmlCustomParser*> &customParsers;

    bool _seenObjectWithId;
};

// ### merge with QtQml::JSCodeGen and operate directly on object->functionsAndExpressions once old compiler is gone.
class QQmlJSCodeGenerator : public QQmlCompilePass
{
public:
    QQmlJSCodeGenerator(QQmlTypeCompiler *typeCompiler, QmlIR::JSCodeGen *v4CodeGen);

    bool generateCodeForComponents();

private:
    bool compileComponent(int componentRoot);
    bool compileJavaScriptCodeInObjectsRecursively(int objectIndex, int scopeObjectIndex);

    const QV4::CompiledData::ResolvedTypeReferenceMap &resolvedTypes;
    const QHash<int, QQmlCustomParser*> &customParsers;
    const QVector<QmlIR::Object*> &qmlObjects;
    const QQmlPropertyCacheVector * const propertyCaches;
    QmlIR::JSCodeGen * const v4CodeGen;
};

class QQmlDefaultPropertyMerger : public QQmlCompilePass
{
public:
    QQmlDefaultPropertyMerger(QQmlTypeCompiler *typeCompiler);

    void mergeDefaultProperties();

private:
    void mergeDefaultProperties(int objectIndex);

    const QVector<QmlIR::Object*> &qmlObjects;
    const QQmlPropertyCacheVector * const propertyCaches;
};

class QQmlJavaScriptBindingExpressionSimplificationPass : public QQmlCompilePass
{
public:
    QQmlJavaScriptBindingExpressionSimplificationPass(QQmlTypeCompiler *typeCompiler);

    void reduceTranslationBindings();

private:
    void reduceTranslationBindings(int objectIndex);

    void visit(QV4::IR::Stmt *s)
    {
        switch (s->stmtKind) {
        case QV4::IR::Stmt::MoveStmt:
            visitMove(s->asMove());
            break;
        case QV4::IR::Stmt::RetStmt:
            visitRet(s->asRet());
            break;
        case QV4::IR::Stmt::CJumpStmt:
            discard();
            break;
        case QV4::IR::Stmt::ExpStmt:
            discard();
            break;
        case QV4::IR::Stmt::JumpStmt:
            break;
        case QV4::IR::Stmt::PhiStmt:
            break;
        }
    }

    void visitMove(QV4::IR::Move *move);
    void visitRet(QV4::IR::Ret *ret);

    void visitFunctionCall(const QString *name, QV4::IR::ExprList *args, QV4::IR::Temp *target);

    void discard() { _canSimplify = false; }

    bool simplifyBinding(QV4::IR::Function *function, QmlIR::Binding *binding);
    bool detectTranslationCallAndConvertBinding(QmlIR::Binding *binding);

    const QVector<QmlIR::Object*> &qmlObjects;
    QV4::IR::Module *jsModule;

    bool _canSimplify;
    const QString *_nameOfFunctionCalled;
    QVector<int> _functionParameters;
    int _functionCallReturnValue;

    QHash<int, QV4::IR::Expr*> _temps;
    int _returnValueOfBindingExpression;
    int _synthesizedConsts;

    QVector<int> irFunctionsToRemove;
};

class QQmlIRFunctionCleanser : public QQmlCompilePass
{
public:
    QQmlIRFunctionCleanser(QQmlTypeCompiler *typeCompiler, const QVector<int> &functionsToRemove);

    void clean();

private:
    virtual void visitMove(QV4::IR::Move *s) {
        visit(s->source);
        visit(s->target);
    }

    void visit(QV4::IR::Stmt *s);
    void visit(QV4::IR::Expr *e);

private:
    QV4::IR::Module *module;
    const QVector<int> &functionsToRemove;

    QVector<int> newFunctionIndices;
};

QT_END_NAMESPACE

#endif // QQMLTYPECOMPILER_P_H

/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include <qtest.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlincubator.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qfile.h>
#include <QtCore/qdebug.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qdir.h>
#include <QSignalSpy>
#include <QFont>
#include <QQmlFileSelector>
#include <QFileSelector>

#include <private/qqmlproperty_p.h>
#include <private/qqmlmetatype_p.h>
#include <private/qqmlglobal_p.h>
#include <private/qqmlscriptstring_p.h>
#include <private/qqmlvmemetaobject_p.h>

#include "testtypes.h"
#include "testhttpserver.h"

#include "../../shared/util.h"

#if defined(Q_OS_MAC)
#include <unistd.h>
#endif

DEFINE_BOOL_CONFIG_OPTION(qmlCheckTypes, QML_CHECK_TYPES)

static inline bool isCaseSensitiveFileSystem(const QString &path) {
    Q_UNUSED(path)
#if defined(Q_OS_MAC)
    return pathconf(path.toLatin1().constData(), _PC_CASE_SENSITIVE);
#elif defined(Q_OS_WIN)
    return false;
#else
    return true;
#endif
}

/*
This test case covers QML language issues.  This covers everything that does not
involve evaluating ECMAScript expressions and bindings.

Evaluation of expressions and bindings is covered in qmlecmascript
*/
class tst_qqmllanguage : public QQmlDataTest
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void errors_data();
    void errors();

    void insertedSemicolon_data();
    void insertedSemicolon();

    void simpleObject();
    void simpleContainer();
    void interfaceProperty();
    void interfaceQList();
    void assignObjectToSignal();
    void assignObjectToVariant();
    void assignLiteralSignalProperty();
    void assignQmlComponent();
    void assignBasicTypes();
    void assignTypeExtremes();
    void assignCompositeToType();
    void assignLiteralToVariant();
    void assignLiteralToVar();
    void assignLiteralToJSValue();
    void assignNullStrings();
    void bindJSValueToVar();
    void bindJSValueToVariant();
    void bindJSValueToType();
    void bindTypeToJSValue();
    void customParserTypes();
    void rootAsQmlComponent();
    void rootItemIsComponent();
    void inlineQmlComponents();
    void idProperty();
    void autoNotifyConnection();
    void assignSignal();
    void assignSignalFunctionExpression();
    void overrideSignal_data();
    void overrideSignal();
    void dynamicProperties();
    void dynamicPropertiesNested();
    void listProperties();
    void badListItemType();
    void dynamicObjectProperties();
    void dynamicSignalsAndSlots();
    void simpleBindings();
    void autoComponentCreation();
    void autoComponentCreationInGroupProperty();
    void propertyValueSource();
    void attachedProperties();
    void dynamicObjects();
    void customVariantTypes();
    void valueTypes();
    void cppnamespace();
    void aliasProperties();
    void aliasPropertiesAndSignals();
    void aliasPropertyChangeSignals();
    void componentCompositeType();
    void i18n();
    void i18n_data();
    void onCompleted();
    void onDestruction();
    void scriptString();
    void scriptStringJs();
    void scriptStringWithoutSourceCode();
    void scriptStringComparison();
    void defaultPropertyListOrder();
    void declaredPropertyValues();
    void dontDoubleCallClassBegin();
    void reservedWords_data();
    void reservedWords();
    void inlineAssignmentsOverrideBindings();
    void nestedComponentRoots();
    void registrationOrder();
    void readonly();
    void readonlyObjectProperties();
    void receivers();
    void registeredCompositeType();
    void registeredCompositeTypeWithEnum();
    void registeredCompositeTypeWithAttachedProperty();
    void implicitImportsLast();

    void basicRemote_data();
    void basicRemote();
    void importsBuiltin_data();
    void importsBuiltin();
    void importsLocal_data();
    void importsLocal();
    void importsRemote_data();
    void importsRemote();
    void importsInstalled_data();
    void importsInstalled();
    void importsInstalledRemote_data();
    void importsInstalledRemote();
    void importsPath_data();
    void importsPath();
    void importsOrder_data();
    void importsOrder();
    void importIncorrectCase();
    void importJs_data();
    void importJs();

    void qmlAttachedPropertiesObjectMethod();
    void customOnProperty();
    void variantNotify();

    void revisions();
    void revisionOverloads();

    void subclassedUncreateableRevision_data();
    void subclassedUncreateableRevision();

    void uncreatableTypesAsProperties();

    void propertyInit();
    void remoteLoadCrash();
    void signalWithDefaultArg();
    void signalParameterTypes();

    // regression tests for crashes
    void crash1();
    void crash2();

    void globalEnums();
    void lowercaseEnumRuntime_data();
    void lowercaseEnumRuntime();
    void lowercaseEnumCompileTime_data();
    void lowercaseEnumCompileTime();
    void literals_data();
    void literals();

    void objectDeletionNotify_data();
    void objectDeletionNotify();

    void scopedProperties();

    void deepProperty();

    void compositeSingletonProperties();
    void compositeSingletonSameEngine();
    void compositeSingletonDifferentEngine();
    void compositeSingletonNonTypeError();
    void compositeSingletonQualifiedNamespace();
    void compositeSingletonModule();
    void compositeSingletonModuleVersioned();
    void compositeSingletonModuleQualified();
    void compositeSingletonInstantiateError();
    void compositeSingletonDynamicPropertyError();
    void compositeSingletonDynamicSignal();
    void compositeSingletonQmlRegisterTypeError();
    void compositeSingletonQmldirNoPragmaError();
    void compositeSingletonQmlDirError();
    void compositeSingletonRemote();
    void compositeSingletonJavaScriptPragma();
    void compositeSingletonSelectors();
    void compositeSingletonRegistered();

    void customParserBindingScopes();
    void customParserEvaluateEnum();
    void customParserProperties();
    void customParserWithExtendedObject();
    void nestedCustomParsers();

    void preservePropertyCacheOnGroupObjects();
    void propertyCacheInSync();

    void rootObjectInCreationNotForSubObjects();

    void noChildEvents();

    void earlyIdObjectAccess();

    void deleteSingletons();

    void arrayBuffer_data();
    void arrayBuffer();

    void defaultListProperty();
    void namespacedPropertyTypes();

private:
    QQmlEngine engine;
    QStringList defaultImportPathList;

    void testType(const QString& qml, const QString& type, const QString& error, bool partialMatch = false);

    // When calling into JavaScript, the specific type of the return value can differ if that return
    // value is a number. This is not only the case for non-integral numbers, or numbers that do not
    // fit into the (signed) integer range, but it also depends on which optimizations are run. So,
    // to check if the return value is of a number type, use this method instead of checking against
    // a specific userType.
    static bool isJSNumberType(int userType)
    {
        return userType == (int) QVariant::Int
                || userType == (int) QVariant::UInt
                || userType == (int) QVariant::Double;
    }

    void getSingletonInstance(QQmlEngine& engine, const char* fileName, const char* propertyName, QObject** result /* out */);
    void getSingletonInstance(QObject* o, const char* propertyName, QObject** result /* out */);
};

#define DETERMINE_ERRORS(errorfile,expected,actual)\
    QList<QByteArray> expected; \
    QList<QByteArray> actual; \
    do { \
        QFile file(testFile(errorfile)); \
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text)); \
        QByteArray data = file.readAll(); \
        file.close(); \
        expected = data.split('\n'); \
        expected.removeAll(QByteArray("")); \
        QList<QQmlError> errors = component.errors(); \
        for (int ii = 0; ii < errors.count(); ++ii) { \
            const QQmlError &error = errors.at(ii); \
            QByteArray errorStr = QByteArray::number(error.line()) + ':' +  \
                                  QByteArray::number(error.column()) + ':' + \
                                  error.description().toUtf8(); \
            actual << errorStr; \
        } \
    } while (false);

#define VERIFY_ERRORS(errorfile) \
    if (!errorfile) { \
        if (qgetenv("DEBUG") != "" && !component.errors().isEmpty()) \
            qWarning() << "Unexpected Errors:" << component.errors(); \
        QVERIFY(!component.isError()); \
        QVERIFY(component.errors().isEmpty()); \
    } else { \
        DETERMINE_ERRORS(errorfile,expected,actual);\
        if (qgetenv("DEBUG") != "" && expected != actual) \
            qWarning() << "Expected:" << expected << "Actual:" << actual;  \
        if (qgetenv("QDECLARATIVELANGUAGE_UPDATEERRORS") != "" && expected != actual) {\
            QFile file(testFile(errorfile)); \
            QVERIFY(file.open(QIODevice::WriteOnly)); \
            for (int ii = 0; ii < actual.count(); ++ii) { \
                file.write(actual.at(ii)); file.write("\n"); \
            } \
            file.close(); \
        } else { \
            QCOMPARE(expected, actual); \
        } \
    }

void tst_qqmllanguage::cleanupTestCase()
{
    QVERIFY(QFile::remove(testFile(QString::fromUtf8("I18nType\303\201\303\242\303\243\303\244\303\245.qml"))));
}

void tst_qqmllanguage::insertedSemicolon_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("errorFile");
    QTest::addColumn<bool>("create");

    QTest::newRow("insertedSemicolon.1") << "insertedSemicolon.1.qml" << "insertedSemicolon.1.errors.txt" << false;
}

void tst_qqmllanguage::insertedSemicolon()
{
    QFETCH(QString, file);
    QFETCH(QString, errorFile);
    QFETCH(bool, create);

    QQmlComponent component(&engine, testFileUrl(file));

    if(create) {
        QObject *object = component.create();
        QVERIFY(!object);
    }

    VERIFY_ERRORS(errorFile.toLatin1().constData());
}

void tst_qqmllanguage::errors_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("errorFile");
    QTest::addColumn<bool>("create");

    QTest::newRow("nonexistantProperty.1") << "nonexistantProperty.1.qml" << "nonexistantProperty.1.errors.txt" << false;
    QTest::newRow("nonexistantProperty.2") << "nonexistantProperty.2.qml" << "nonexistantProperty.2.errors.txt" << false;
    QTest::newRow("nonexistantProperty.3") << "nonexistantProperty.3.qml" << "nonexistantProperty.3.errors.txt" << false;
    QTest::newRow("nonexistantProperty.4") << "nonexistantProperty.4.qml" << "nonexistantProperty.4.errors.txt" << false;
    QTest::newRow("nonexistantProperty.5") << "nonexistantProperty.5.qml" << "nonexistantProperty.5.errors.txt" << false;
    QTest::newRow("nonexistantProperty.6") << "nonexistantProperty.6.qml" << "nonexistantProperty.6.errors.txt" << false;
    QTest::newRow("nonexistantProperty.7") << "nonexistantProperty.7.qml" << "nonexistantProperty.7.errors.txt" << false;
    QTest::newRow("nonexistantProperty.8") << "nonexistantProperty.8.qml" << "nonexistantProperty.8.errors.txt" << false;

    QTest::newRow("wrongType (string for int)") << "wrongType.1.qml" << "wrongType.1.errors.txt" << false;
    QTest::newRow("wrongType (int for bool)") << "wrongType.2.qml" << "wrongType.2.errors.txt" << false;
    QTest::newRow("wrongType (bad rect)") << "wrongType.3.qml" << "wrongType.3.errors.txt" << false;

    QTest::newRow("wrongType (invalid enum)") << "wrongType.4.qml" << "wrongType.4.errors.txt" << false;
    QTest::newRow("wrongType (int for uint)") << "wrongType.5.qml" << "wrongType.5.errors.txt" << false;
    QTest::newRow("wrongType (string for real)") << "wrongType.6.qml" << "wrongType.6.errors.txt" << false;
    QTest::newRow("wrongType (int for color)") << "wrongType.7.qml" << "wrongType.7.errors.txt" << false;
    QTest::newRow("wrongType (int for date)") << "wrongType.8.qml" << "wrongType.8.errors.txt" << false;
    QTest::newRow("wrongType (int for time)") << "wrongType.9.qml" << "wrongType.9.errors.txt" << false;
    QTest::newRow("wrongType (int for datetime)") << "wrongType.10.qml" << "wrongType.10.errors.txt" << false;
    QTest::newRow("wrongType (string for point)") << "wrongType.11.qml" << "wrongType.11.errors.txt" << false;
    QTest::newRow("wrongType (color for size)") << "wrongType.12.qml" << "wrongType.12.errors.txt" << false;
    QTest::newRow("wrongType (number string for int)") << "wrongType.13.qml" << "wrongType.13.errors.txt" << false;
    QTest::newRow("wrongType (int for string)") << "wrongType.14.qml" << "wrongType.14.errors.txt" << false;
    QTest::newRow("wrongType (int for url)") << "wrongType.15.qml" << "wrongType.15.errors.txt" << false;
    QTest::newRow("wrongType (invalid object)") << "wrongType.16.qml" << "wrongType.16.errors.txt" << false;
    QTest::newRow("wrongType (int for enum)") << "wrongType.17.qml" << "wrongType.17.errors.txt" << false;

    QTest::newRow("readOnly.1") << "readOnly.1.qml" << "readOnly.1.errors.txt" << false;
    QTest::newRow("readOnly.2") << "readOnly.2.qml" << "readOnly.2.errors.txt" << false;
    QTest::newRow("readOnly.3") << "readOnly.3.qml" << "readOnly.3.errors.txt" << false;
    QTest::newRow("readOnly.4") << "readOnly.4.qml" << "readOnly.4.errors.txt" << false;
    QTest::newRow("readOnly.5") << "readOnly.5.qml" << "readOnly.5.errors.txt" << false;

    QTest::newRow("listAssignment.1") << "listAssignment.1.qml" << "listAssignment.1.errors.txt" << false;
    QTest::newRow("listAssignment.2") << "listAssignment.2.qml" << "listAssignment.2.errors.txt" << false;
    QTest::newRow("listAssignment.3") << "listAssignment.3.qml" << "listAssignment.3.errors.txt" << false;

    QTest::newRow("invalidID.1") << "invalidID.qml" << "invalidID.errors.txt" << false;
    QTest::newRow("invalidID.2") << "invalidID.2.qml" << "invalidID.2.errors.txt" << false;
    QTest::newRow("invalidID.3") << "invalidID.3.qml" << "invalidID.3.errors.txt" << false;
    QTest::newRow("invalidID.4") << "invalidID.4.qml" << "invalidID.4.errors.txt" << false;
    QTest::newRow("invalidID.5") << "invalidID.5.qml" << "invalidID.5.errors.txt" << false;
    QTest::newRow("invalidID.6") << "invalidID.6.qml" << "invalidID.6.errors.txt" << false;
    QTest::newRow("invalidID.7") << "invalidID.7.qml" << "invalidID.7.errors.txt" << false;
    QTest::newRow("invalidID.8") << "invalidID.8.qml" << "invalidID.8.errors.txt" << false;
    QTest::newRow("invalidID.9") << "invalidID.9.qml" << "invalidID.9.errors.txt" << false;

    QTest::newRow("scriptString.1") << "scriptString.1.qml" << "scriptString.1.errors.txt" << false;
    QTest::newRow("scriptString.2") << "scriptString.2.qml" << "scriptString.2.errors.txt" << false;

    QTest::newRow("unsupportedProperty") << "unsupportedProperty.qml" << "unsupportedProperty.errors.txt" << false;
    QTest::newRow("nullDotProperty") << "nullDotProperty.qml" << "nullDotProperty.errors.txt" << true;
    QTest::newRow("fakeDotProperty") << "fakeDotProperty.qml" << "fakeDotProperty.errors.txt" << false;
    QTest::newRow("duplicateIDs") << "duplicateIDs.qml" << "duplicateIDs.errors.txt" << false;
    QTest::newRow("unregisteredObject") << "unregisteredObject.qml" << "unregisteredObject.errors.txt" << false;
    QTest::newRow("empty") << "empty.qml" << "empty.errors.txt" << false;
    QTest::newRow("missingObject") << "missingObject.qml" << "missingObject.errors.txt" << false;
    QTest::newRow("failingComponent") << "failingComponentTest.qml" << "failingComponent.errors.txt" << false;
    QTest::newRow("missingSignal") << "missingSignal.qml" << "missingSignal.errors.txt" << false;
    QTest::newRow("missingSignal2") << "missingSignal.2.qml" << "missingSignal.2.errors.txt" << false;
    QTest::newRow("finalOverride") << "finalOverride.qml" << "finalOverride.errors.txt" << false;
    QTest::newRow("customParserIdNotAllowed") << "customParserIdNotAllowed.qml" << "customParserIdNotAllowed.errors.txt" << false;

    QTest::newRow("invalidGroupedProperty.1") << "invalidGroupedProperty.1.qml" << "invalidGroupedProperty.1.errors.txt" << false;
    QTest::newRow("invalidGroupedProperty.2") << "invalidGroupedProperty.2.qml" << "invalidGroupedProperty.2.errors.txt" << false;
    QTest::newRow("invalidGroupedProperty.3") << "invalidGroupedProperty.3.qml" << "invalidGroupedProperty.3.errors.txt" << false;
    QTest::newRow("invalidGroupedProperty.4") << "invalidGroupedProperty.4.qml" << "invalidGroupedProperty.4.errors.txt" << false;
    QTest::newRow("invalidGroupedProperty.5") << "invalidGroupedProperty.5.qml" << "invalidGroupedProperty.5.errors.txt" << false;
    QTest::newRow("invalidGroupedProperty.6") << "invalidGroupedProperty.6.qml" << "invalidGroupedProperty.6.errors.txt" << false;
    QTest::newRow("invalidGroupedProperty.7") << "invalidGroupedProperty.7.qml" << "invalidGroupedProperty.7.errors.txt" << true;
    QTest::newRow("invalidGroupedProperty.8") << "invalidGroupedProperty.8.qml" << "invalidGroupedProperty.8.errors.txt" << false;
    QTest::newRow("invalidGroupedProperty.9") << "invalidGroupedProperty.9.qml" << "invalidGroupedProperty.9.errors.txt" << false;
    QTest::newRow("invalidGroupedProperty.10") << "invalidGroupedProperty.10.qml" << "invalidGroupedProperty.10.errors.txt" << false;

    QTest::newRow("importNamespaceConflict") << "importNamespaceConflict.qml" << "importNamespaceConflict.errors.txt" << false;
    QTest::newRow("importVersionMissing (builtin)") << "importVersionMissingBuiltIn.qml" << "importVersionMissingBuiltIn.errors.txt" << false;
    QTest::newRow("importVersionMissing (installed)") << "importVersionMissingInstalled.qml" << "importVersionMissingInstalled.errors.txt" << false;
    QTest::newRow("importNonExist (installed)") << "importNonExist.qml" << "importNonExist.errors.txt" << false;
    QTest::newRow("importNonExistOlder (installed)") << "importNonExistOlder.qml" << "importNonExistOlder.errors.txt" << false;
    QTest::newRow("importNewerVersion (installed)") << "importNewerVersion.qml" << "importNewerVersion.errors.txt" << false;
    QTest::newRow("invalidImportID") << "invalidImportID.qml" << "invalidImportID.errors.txt" << false;
    QTest::newRow("importFile") << "importFile.qml" << "importFile.errors.txt" << false;

    QTest::newRow("signal.1") << "signal.1.qml" << "signal.1.errors.txt" << false;
    QTest::newRow("signal.2") << "signal.2.qml" << "signal.2.errors.txt" << false;
    QTest::newRow("signal.3") << "signal.3.qml" << "signal.3.errors.txt" << false;
    QTest::newRow("signal.4") << "signal.4.qml" << "signal.4.errors.txt" << false;
    QTest::newRow("signal.5") << "signal.5.qml" << "signal.5.errors.txt" << false;
    QTest::newRow("signal.6") << "signal.6.qml" << "signal.6.errors.txt" << false;

    QTest::newRow("method.1") << "method.1.qml" << "method.1.errors.txt" << false;

    QTest::newRow("property.1") << "property.1.qml" << "property.1.errors.txt" << false;
    QTest::newRow("property.2") << "property.2.qml" << "property.2.errors.txt" << false;
    QTest::newRow("property.3") << "property.3.qml" << "property.3.errors.txt" << false;
    QTest::newRow("property.4") << "property.4.qml" << "property.4.errors.txt" << false;
    QTest::newRow("property.6") << "property.6.qml" << "property.6.errors.txt" << false;
    QTest::newRow("property.7") << "property.7.qml" << "property.7.errors.txt" << false;

    QTest::newRow("importScript.1") << "importscript.1.qml" << "importscript.1.errors.txt" << false;

    QTest::newRow("Component.1") << "component.1.qml" << "component.1.errors.txt" << false;
    QTest::newRow("Component.2") << "component.2.qml" << "component.2.errors.txt" << false;
    QTest::newRow("Component.3") << "component.3.qml" << "component.3.errors.txt" << false;
    QTest::newRow("Component.4") << "component.4.qml" << "component.4.errors.txt" << false;
    QTest::newRow("Component.5") << "component.5.qml" << "component.5.errors.txt" << false;
    QTest::newRow("Component.6") << "component.6.qml" << "component.6.errors.txt" << false;
    QTest::newRow("Component.7") << "component.7.qml" << "component.7.errors.txt" << false;
    QTest::newRow("Component.8") << "component.8.qml" << "component.8.errors.txt" << false;
    QTest::newRow("Component.9") << "component.9.qml" << "component.9.errors.txt" << false;

    QTest::newRow("MultiSet.1") << "multiSet.1.qml" << "multiSet.1.errors.txt" << false;
    QTest::newRow("MultiSet.2") << "multiSet.2.qml" << "multiSet.2.errors.txt" << false;
    QTest::newRow("MultiSet.3") << "multiSet.3.qml" << "multiSet.3.errors.txt" << false;
    QTest::newRow("MultiSet.4") << "multiSet.4.qml" << "multiSet.4.errors.txt" << false;
    QTest::newRow("MultiSet.5") << "multiSet.5.qml" << "multiSet.5.errors.txt" << false;
    QTest::newRow("MultiSet.6") << "multiSet.6.qml" << "multiSet.6.errors.txt" << false;
    QTest::newRow("MultiSet.7") << "multiSet.7.qml" << "multiSet.7.errors.txt" << false;
    QTest::newRow("MultiSet.8") << "multiSet.8.qml" << "multiSet.8.errors.txt" << false;
    QTest::newRow("MultiSet.9") << "multiSet.9.qml" << "multiSet.9.errors.txt" << false;
    QTest::newRow("MultiSet.10") << "multiSet.10.qml" << "multiSet.10.errors.txt" << false;
    QTest::newRow("MultiSet.11") << "multiSet.11.qml" << "multiSet.11.errors.txt" << false;

    QTest::newRow("dynamicMeta.1") << "dynamicMeta.1.qml" << "dynamicMeta.1.errors.txt" << false;
    QTest::newRow("dynamicMeta.2") << "dynamicMeta.2.qml" << "dynamicMeta.2.errors.txt" << false;
    QTest::newRow("dynamicMeta.3") << "dynamicMeta.3.qml" << "dynamicMeta.3.errors.txt" << false;
    QTest::newRow("dynamicMeta.4") << "dynamicMeta.4.qml" << "dynamicMeta.4.errors.txt" << false;
    QTest::newRow("dynamicMeta.5") << "dynamicMeta.5.qml" << "dynamicMeta.5.errors.txt" << false;

    QTest::newRow("invalidAlias.1") << "invalidAlias.1.qml" << "invalidAlias.1.errors.txt" << false;
    QTest::newRow("invalidAlias.2") << "invalidAlias.2.qml" << "invalidAlias.2.errors.txt" << false;
    QTest::newRow("invalidAlias.3") << "invalidAlias.3.qml" << "invalidAlias.3.errors.txt" << false;
    QTest::newRow("invalidAlias.4") << "invalidAlias.4.qml" << "invalidAlias.4.errors.txt" << false;
    QTest::newRow("invalidAlias.5") << "invalidAlias.5.qml" << "invalidAlias.5.errors.txt" << false;
    QTest::newRow("invalidAlias.6") << "invalidAlias.6.qml" << "invalidAlias.6.errors.txt" << false;
    QTest::newRow("invalidAlias.7") << "invalidAlias.7.qml" << "invalidAlias.7.errors.txt" << false;
    QTest::newRow("invalidAlias.8") << "invalidAlias.8.qml" << "invalidAlias.8.errors.txt" << false;
    QTest::newRow("invalidAlias.9") << "invalidAlias.9.qml" << "invalidAlias.9.errors.txt" << false;
    QTest::newRow("invalidAlias.10") << "invalidAlias.10.qml" << "invalidAlias.10.errors.txt" << false;
    QTest::newRow("invalidAlias.11") << "invalidAlias.11.qml" << "invalidAlias.11.errors.txt" << false;

    QTest::newRow("invalidAttachedProperty.1") << "invalidAttachedProperty.1.qml" << "invalidAttachedProperty.1.errors.txt" << false;
    QTest::newRow("invalidAttachedProperty.2") << "invalidAttachedProperty.2.qml" << "invalidAttachedProperty.2.errors.txt" << false;
    QTest::newRow("invalidAttachedProperty.3") << "invalidAttachedProperty.3.qml" << "invalidAttachedProperty.3.errors.txt" << false;
    QTest::newRow("invalidAttachedProperty.4") << "invalidAttachedProperty.4.qml" << "invalidAttachedProperty.4.errors.txt" << false;
    QTest::newRow("invalidAttachedProperty.5") << "invalidAttachedProperty.5.qml" << "invalidAttachedProperty.5.errors.txt" << false;
    QTest::newRow("invalidAttachedProperty.6") << "invalidAttachedProperty.6.qml" << "invalidAttachedProperty.6.errors.txt" << false;
    QTest::newRow("invalidAttachedProperty.7") << "invalidAttachedProperty.7.qml" << "invalidAttachedProperty.7.errors.txt" << false;
    QTest::newRow("invalidAttachedProperty.8") << "invalidAttachedProperty.8.qml" << "invalidAttachedProperty.8.errors.txt" << false;
    QTest::newRow("invalidAttachedProperty.9") << "invalidAttachedProperty.9.qml" << "invalidAttachedProperty.9.errors.txt" << false;
    QTest::newRow("invalidAttachedProperty.10") << "invalidAttachedProperty.10.qml" << "invalidAttachedProperty.10.errors.txt" << false;
    QTest::newRow("invalidAttachedProperty.11") << "invalidAttachedProperty.11.qml" << "invalidAttachedProperty.11.errors.txt" << false;
    QTest::newRow("invalidAttachedProperty.12") << "invalidAttachedProperty.12.qml" << "invalidAttachedProperty.12.errors.txt" << false;
    QTest::newRow("invalidAttachedProperty.13") << "invalidAttachedProperty.13.qml" << "invalidAttachedProperty.13.errors.txt" << false;

    QTest::newRow("assignValueToSignal") << "assignValueToSignal.qml" << "assignValueToSignal.errors.txt" << false;
    QTest::newRow("emptySignal") << "emptySignal.qml" << "emptySignal.errors.txt" << false;

    QTest::newRow("nestedErrors") << "nestedErrors.qml" << "nestedErrors.errors.txt" << false;
    QTest::newRow("defaultGrouped") << "defaultGrouped.qml" << "defaultGrouped.errors.txt" << false;
    QTest::newRow("doubleSignal") << "doubleSignal.qml" << "doubleSignal.errors.txt" << false;
    QTest::newRow("missingValueTypeProperty") << "missingValueTypeProperty.qml" << "missingValueTypeProperty.errors.txt" << false;
    QTest::newRow("objectValueTypeProperty") << "objectValueTypeProperty.qml" << "objectValueTypeProperty.errors.txt" << false;
    QTest::newRow("enumTypes") << "enumTypes.qml" << "enumTypes.errors.txt" << false;
    QTest::newRow("noCreation") << "noCreation.qml" << "noCreation.errors.txt" << false;
    QTest::newRow("destroyedSignal") << "destroyedSignal.qml" << "destroyedSignal.errors.txt" << false;
    QTest::newRow("assignToNamespace") << "assignToNamespace.qml" << "assignToNamespace.errors.txt" << false;
    QTest::newRow("invalidOn") << "invalidOn.qml" << "invalidOn.errors.txt" << false;
    QTest::newRow("invalidProperty") << "invalidProperty.qml" << "invalidProperty.errors.txt" << false;
    QTest::newRow("nonScriptableProperty") << "nonScriptableProperty.qml" << "nonScriptableProperty.errors.txt" << false;
    QTest::newRow("notAvailable") << "notAvailable.qml" << "notAvailable.errors.txt" << false;
    QTest::newRow("singularProperty") << "singularProperty.qml" << "singularProperty.errors.txt" << false;
    QTest::newRow("singularProperty.2") << "singularProperty.2.qml" << "singularProperty.2.errors.txt" << false;

    const QString expectedError = isCaseSensitiveFileSystem(dataDirectory()) ?
        QStringLiteral("incorrectCase.errors.sensitive.txt") :
        QStringLiteral("incorrectCase.errors.insensitive.txt");
    QTest::newRow("incorrectCase") << "incorrectCase.qml" << expectedError << false;

    QTest::newRow("metaobjectRevision.1") << "metaobjectRevision.1.qml" << "metaobjectRevision.1.errors.txt" << false;
    QTest::newRow("metaobjectRevision.2") << "metaobjectRevision.2.qml" << "metaobjectRevision.2.errors.txt" << false;
    QTest::newRow("metaobjectRevision.3") << "metaobjectRevision.3.qml" << "metaobjectRevision.3.errors.txt" << false;

    QTest::newRow("invalidRoot.1") << "invalidRoot.1.qml" << "invalidRoot.1.errors.txt" << false;
    QTest::newRow("invalidRoot.2") << "invalidRoot.2.qml" << "invalidRoot.2.errors.txt" << false;
    QTest::newRow("invalidRoot.3") << "invalidRoot.3.qml" << "invalidRoot.3.errors.txt" << false;
    QTest::newRow("invalidRoot.4") << "invalidRoot.4.qml" << "invalidRoot.4.errors.txt" << false;

    QTest::newRow("invalidTypeName.1") << "invalidTypeName.1.qml" << "invalidTypeName.1.errors.txt" << false;
    QTest::newRow("invalidTypeName.2") << "invalidTypeName.2.qml" << "invalidTypeName.2.errors.txt" << false;
    QTest::newRow("invalidTypeName.3") << "invalidTypeName.3.qml" << "invalidTypeName.3.errors.txt" << false;
    QTest::newRow("invalidTypeName.4") << "invalidTypeName.4.qml" << "invalidTypeName.4.errors.txt" << false;

    QTest::newRow("Major version isolation") << "majorVersionIsolation.qml" << "majorVersionIsolation.errors.txt" << false;

    QTest::newRow("badCompositeRegistration.1") << "badCompositeRegistration.1.qml" << "badCompositeRegistration.1.errors.txt" << false;
    QTest::newRow("badCompositeRegistration.2") << "badCompositeRegistration.2.qml" << "badCompositeRegistration.2.errors.txt" << false;
}


void tst_qqmllanguage::errors()
{
    QFETCH(QString, file);
    QFETCH(QString, errorFile);
    QFETCH(bool, create);

    QQmlComponent component(&engine, testFileUrl(file));

    if (create) {
        QObject *object = component.create();
        QVERIFY(!object);
    }

    VERIFY_ERRORS(errorFile.toLatin1().constData());
}

void tst_qqmllanguage::simpleObject()
{
    QQmlComponent component(&engine, testFileUrl("simpleObject.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);
}

void tst_qqmllanguage::simpleContainer()
{
    QQmlComponent component(&engine, testFileUrl("simpleContainer.qml"));
    VERIFY_ERRORS(0);
    MyContainer *container= qobject_cast<MyContainer*>(component.create());
    QVERIFY(container != 0);
    QCOMPARE(container->getChildren()->count(),2);
}

void tst_qqmllanguage::interfaceProperty()
{
    QQmlComponent component(&engine, testFileUrl("interfaceProperty.qml"));
    VERIFY_ERRORS(0);
    MyQmlObject *object = qobject_cast<MyQmlObject*>(component.create());
    QVERIFY(object != 0);
    QVERIFY(object->interface());
    QCOMPARE(object->interface()->id, 913);
}

void tst_qqmllanguage::interfaceQList()
{
    QQmlComponent component(&engine, testFileUrl("interfaceQList.qml"));
    VERIFY_ERRORS(0);
    MyContainer *container= qobject_cast<MyContainer*>(component.create());
    QVERIFY(container != 0);
    QCOMPARE(container->getQListInterfaces()->count(), 2);
    for(int ii = 0; ii < 2; ++ii)
        QCOMPARE(container->getQListInterfaces()->at(ii)->id, 913);
}

void tst_qqmllanguage::assignObjectToSignal()
{
    QQmlComponent component(&engine, testFileUrl("assignObjectToSignal.qml"));
    VERIFY_ERRORS(0);
    MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
    QVERIFY(object != 0);
    QTest::ignoreMessage(QtWarningMsg, "MyQmlObject::basicSlot");
    emit object->basicSignal();
}

void tst_qqmllanguage::assignObjectToVariant()
{
    QQmlComponent component(&engine, testFileUrl("assignObjectToVariant.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);
    QVariant v = object->property("a");
    QVERIFY(v.userType() == qMetaTypeId<QObject *>());
}

void tst_qqmllanguage::assignLiteralSignalProperty()
{
    QQmlComponent component(&engine, testFileUrl("assignLiteralSignalProperty.qml"));
    VERIFY_ERRORS(0);
    MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
    QVERIFY(object != 0);
    QCOMPARE(object->onLiteralSignal(), 10);
}

// Test is an external component can be loaded and assigned (to a qlist)
void tst_qqmllanguage::assignQmlComponent()
{
    QQmlComponent component(&engine, testFileUrl("assignQmlComponent.qml"));
    VERIFY_ERRORS(0);
    MyContainer *object = qobject_cast<MyContainer *>(component.create());
    QVERIFY(object != 0);
    QCOMPARE(object->getChildren()->count(), 1);
    QObject *child = object->getChildren()->at(0);
    QCOMPARE(child->property("x"), QVariant(10));
    QCOMPARE(child->property("y"), QVariant(11));
}

// Test literal assignment to all the basic types
void tst_qqmllanguage::assignBasicTypes()
{
    QQmlComponent component(&engine, testFileUrl("assignBasicTypes.qml"));
    VERIFY_ERRORS(0);
    MyTypeObject *object = qobject_cast<MyTypeObject *>(component.create());
    QVERIFY(object != 0);
    QCOMPARE(object->flagProperty(), MyTypeObject::FlagVal1 | MyTypeObject::FlagVal3);
    QCOMPARE(object->enumProperty(), MyTypeObject::EnumVal2);
    QCOMPARE(object->qtEnumProperty(), Qt::RichText);
    QCOMPARE(object->mirroredEnumProperty(), MyTypeObject::MirroredEnumVal3);
    QCOMPARE(object->relatedEnumProperty(), MyEnumContainer::RelatedValue);
    QCOMPARE(object->stringProperty(), QString("Hello World!"));
    QCOMPARE(object->uintProperty(), uint(10));
    QCOMPARE(object->intProperty(), -19);
    QCOMPARE((float)object->realProperty(), float(23.2));
    QCOMPARE((float)object->doubleProperty(), float(-19.7));
    QCOMPARE((float)object->floatProperty(), float(8.5));
    QCOMPARE(object->colorProperty(), QColor("red"));
    QCOMPARE(object->dateProperty(), QDate(1982, 11, 25));
    QCOMPARE(object->timeProperty(), QTime(11, 11, 32));
    QCOMPARE(object->dateTimeProperty(), QDateTime(QDate(2009, 5, 12), QTime(13, 22, 1)));
    QCOMPARE(object->pointProperty(), QPoint(99,13));
    QCOMPARE(object->pointFProperty(), QPointF(-10.1, 12.3));
    QCOMPARE(object->sizeProperty(), QSize(99, 13));
    QCOMPARE(object->sizeFProperty(), QSizeF(0.1, 0.2));
    QCOMPARE(object->rectProperty(), QRect(9, 7, 100, 200));
    QCOMPARE(object->rectFProperty(), QRectF(1000.1, -10.9, 400, 90.99));
    QCOMPARE(object->boolProperty(), true);
    QCOMPARE(object->variantProperty(), QVariant("Hello World!"));
    QCOMPARE(object->vectorProperty(), QVector3D(10, 1, 2.2f));
    QCOMPARE(object->vector2Property(), QVector2D(2, 3));
    QCOMPARE(object->vector4Property(), QVector4D(10, 1, 2.2f, 2.3f));
    const QUrl encoded = QUrl::fromEncoded("main.qml?with%3cencoded%3edata", QUrl::TolerantMode);
    QCOMPARE(object->urlProperty(), component.url().resolved(encoded));
    QVERIFY(object->objectProperty() != 0);
    MyTypeObject *child = qobject_cast<MyTypeObject *>(object->objectProperty());
    QVERIFY(child != 0);
    QCOMPARE(child->intProperty(), 8);

    //these used to go via script. Ensure they no longer do
    QCOMPARE(object->property("qtEnumTriggeredChange").toBool(), false);
    QCOMPARE(object->property("mirroredEnumTriggeredChange").toBool(), false);
}

// Test edge case type assignments
void tst_qqmllanguage::assignTypeExtremes()
{
    QQmlComponent component(&engine, testFileUrl("assignTypeExtremes.qml"));
    VERIFY_ERRORS(0);
    MyTypeObject *object = qobject_cast<MyTypeObject *>(component.create());
    QVERIFY(object != 0);
    QCOMPARE(object->uintProperty(), 0xEE6B2800);
    QCOMPARE(object->intProperty(), -0x77359400);
}

// Test that a composite type can assign to a property of its base type
void tst_qqmllanguage::assignCompositeToType()
{
    QQmlComponent component(&engine, testFileUrl("assignCompositeToType.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);
}

// Test that literals are stored correctly in variant properties
void tst_qqmllanguage::assignLiteralToVariant()
{
    QQmlComponent component(&engine, testFileUrl("assignLiteralToVariant.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);

    QVERIFY(isJSNumberType(object->property("test1").userType()));
    QVERIFY(isJSNumberType(object->property("test2").userType()));
    QCOMPARE(object->property("test3").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test4").userType(), (int)QVariant::Color);
    QCOMPARE(object->property("test5").userType(), (int)QVariant::RectF);
    QCOMPARE(object->property("test6").userType(), (int)QVariant::PointF);
    QCOMPARE(object->property("test7").userType(), (int)QVariant::SizeF);
    QCOMPARE(object->property("test8").userType(), (int)QVariant::Vector3D);
    QCOMPARE(object->property("test9").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test10").userType(), (int)QVariant::Bool);
    QCOMPARE(object->property("test11").userType(), (int)QVariant::Bool);
    QCOMPARE(object->property("test12").userType(), (int)QVariant::Vector4D);

    QCOMPARE(object->property("test1"), QVariant(1));
    QCOMPARE(object->property("test2"), QVariant((double)1.7));
    QVERIFY(object->property("test3") == QVariant(QString(QLatin1String("Hello world!"))));
    QCOMPARE(object->property("test4"), QVariant(QColor::fromRgb(0xFF008800)));
    QVERIFY(object->property("test5") == QVariant(QRectF(10, 10, 10, 10)));
    QVERIFY(object->property("test6") == QVariant(QPointF(10, 10)));
    QVERIFY(object->property("test7") == QVariant(QSizeF(10, 10)));
    QVERIFY(object->property("test8") == QVariant(QVector3D(100, 100, 100)));
    QCOMPARE(object->property("test9"), QVariant(QString(QLatin1String("#FF008800"))));
    QCOMPARE(object->property("test10"), QVariant(bool(true)));
    QCOMPARE(object->property("test11"), QVariant(bool(false)));
    QVERIFY(object->property("test12") == QVariant(QVector4D(100, 100, 100, 100)));

    delete object;
}

// Test that literals are stored correctly in "var" properties
// Note that behaviour differs from "variant" properties in that
// no conversion from "special strings" to QVariants is performed.
void tst_qqmllanguage::assignLiteralToVar()
{
    QQmlComponent component(&engine, testFileUrl("assignLiteralToVar.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);

    QVERIFY(isJSNumberType(object->property("test1").userType()));
    QCOMPARE(object->property("test2").userType(), (int)QMetaType::Double);
    QCOMPARE(object->property("test3").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test4").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test5").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test6").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test7").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test8").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test9").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test10").userType(), (int)QVariant::Bool);
    QCOMPARE(object->property("test11").userType(), (int)QVariant::Bool);
    QCOMPARE(object->property("test12").userType(), (int)QVariant::Color);
    QCOMPARE(object->property("test13").userType(), (int)QVariant::RectF);
    QCOMPARE(object->property("test14").userType(), (int)QVariant::PointF);
    QCOMPARE(object->property("test15").userType(), (int)QVariant::SizeF);
    QCOMPARE(object->property("test16").userType(), (int)QVariant::Vector3D);
    QVERIFY(isJSNumberType(object->property("variantTest1Bound").userType()));
    QVERIFY(isJSNumberType(object->property("test1Bound").userType()));

    QCOMPARE(object->property("test1"), QVariant(5));
    QCOMPARE(object->property("test2"), QVariant((double)1.7));
    QCOMPARE(object->property("test3"), QVariant(QString(QLatin1String("Hello world!"))));
    QCOMPARE(object->property("test4"), QVariant(QString(QLatin1String("#FF008800"))));
    QCOMPARE(object->property("test5"), QVariant(QString(QLatin1String("10,10,10x10"))));
    QCOMPARE(object->property("test6"), QVariant(QString(QLatin1String("10,10"))));
    QCOMPARE(object->property("test7"), QVariant(QString(QLatin1String("10x10"))));
    QCOMPARE(object->property("test8"), QVariant(QString(QLatin1String("100,100,100"))));
    QCOMPARE(object->property("test9"), QVariant(QString(QLatin1String("#FF008800"))));
    QCOMPARE(object->property("test10"), QVariant(bool(true)));
    QCOMPARE(object->property("test11"), QVariant(bool(false)));
    QCOMPARE(object->property("test12"), QVariant(QColor::fromRgbF(0.2, 0.3, 0.4, 0.5)));
    QCOMPARE(object->property("test13"), QVariant(QRectF(10, 10, 10, 10)));
    QCOMPARE(object->property("test14"), QVariant(QPointF(10, 10)));
    QCOMPARE(object->property("test15"), QVariant(QSizeF(10, 10)));
    QCOMPARE(object->property("test16"), QVariant(QVector3D(100, 100, 100)));
    QCOMPARE(object->property("variantTest1Bound"), QVariant(9));
    QCOMPARE(object->property("test1Bound"), QVariant(11));

    delete object;
}

void tst_qqmllanguage::assignLiteralToJSValue()
{
    QQmlComponent component(&engine, testFileUrl("assignLiteralToJSValue.qml"));
    VERIFY_ERRORS(0);
    QObject *root = component.create();
    QVERIFY(root != 0);

    {
        MyQmlObject *object = root->findChild<MyQmlObject *>("test1");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isNumber());
        QCOMPARE(value.toNumber(), qreal(5));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("test2");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isNumber());
        QCOMPARE(value.toNumber(), qreal(1.7));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("test3");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isString());
        QCOMPARE(value.toString(), QString(QLatin1String("Hello world!")));
    }{
        MyQmlObject *object = root->findChild<MyQmlObject *>("test4");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isString());
        QCOMPARE(value.toString(), QString(QLatin1String("#FF008800")));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("test5");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isString());
        QCOMPARE(value.toString(), QString(QLatin1String("10,10,10x10")));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("test6");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isString());
        QCOMPARE(value.toString(), QString(QLatin1String("10,10")));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("test7");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isString());
        QCOMPARE(value.toString(), QString(QLatin1String("10x10")));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("test8");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isString());
        QCOMPARE(value.toString(), QString(QLatin1String("100,100,100")));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("test9");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isString());
        QCOMPARE(value.toString(), QString(QLatin1String("#FF008800")));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("test10");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isBool());
        QCOMPARE(value.toBool(), true);
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("test11");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isBool());
        QCOMPARE(value.toBool(), false);
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("test20");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isCallable());
        QCOMPARE(value.call(QList<QJSValue> () << QJSValue(4)).toInt(), 12);
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("test21");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isUndefined());
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("test22");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isNull());
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("test1Bound");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isNumber());
        QCOMPARE(value.toNumber(), qreal(9));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("test20Bound");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isNumber());
        QCOMPARE(value.toNumber(), qreal(27));
    }
}

void tst_qqmllanguage::assignNullStrings()
{
    QQmlComponent component(&engine, testFileUrl("assignNullStrings.qml"));
    VERIFY_ERRORS(0);
    MyTypeObject *object = qobject_cast<MyTypeObject *>(component.create());
    QVERIFY(object != 0);
    QVERIFY(object->stringProperty().isNull());
    QVERIFY(object->byteArrayProperty().isNull());
    QMetaObject::invokeMethod(object, "assignNullStringsFromJs", Qt::DirectConnection);
    QVERIFY(object->stringProperty().isNull());
    QVERIFY(object->byteArrayProperty().isNull());
}

void tst_qqmllanguage::bindJSValueToVar()
{
    QQmlComponent component(&engine, testFileUrl("assignLiteralToJSValue.qml"));

    VERIFY_ERRORS(0);
    QObject *root = component.create();
    QVERIFY(root != 0);

    QObject *object = root->findChild<QObject *>("varProperties");

    QVERIFY(isJSNumberType(object->property("test1").userType()));
    QVERIFY(isJSNumberType(object->property("test2").userType()));
    QCOMPARE(object->property("test3").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test4").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test5").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test6").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test7").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test8").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test9").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test10").userType(), (int)QVariant::Bool);
    QCOMPARE(object->property("test11").userType(), (int)QVariant::Bool);
    QCOMPARE(object->property("test12").userType(), (int)QVariant::Color);
    QCOMPARE(object->property("test13").userType(), (int)QVariant::RectF);
    QCOMPARE(object->property("test14").userType(), (int)QVariant::PointF);
    QCOMPARE(object->property("test15").userType(), (int)QVariant::SizeF);
    QCOMPARE(object->property("test16").userType(), (int)QVariant::Vector3D);
    QVERIFY(isJSNumberType(object->property("test1Bound").userType()));
    QVERIFY(isJSNumberType(object->property("test20Bound").userType()));

    QCOMPARE(object->property("test1"), QVariant(5));
    QCOMPARE(object->property("test2"), QVariant((double)1.7));
    QCOMPARE(object->property("test3"), QVariant(QString(QLatin1String("Hello world!"))));
    QCOMPARE(object->property("test4"), QVariant(QString(QLatin1String("#FF008800"))));
    QCOMPARE(object->property("test5"), QVariant(QString(QLatin1String("10,10,10x10"))));
    QCOMPARE(object->property("test6"), QVariant(QString(QLatin1String("10,10"))));
    QCOMPARE(object->property("test7"), QVariant(QString(QLatin1String("10x10"))));
    QCOMPARE(object->property("test8"), QVariant(QString(QLatin1String("100,100,100"))));
    QCOMPARE(object->property("test9"), QVariant(QString(QLatin1String("#FF008800"))));
    QCOMPARE(object->property("test10"), QVariant(bool(true)));
    QCOMPARE(object->property("test11"), QVariant(bool(false)));
    QCOMPARE(object->property("test12"), QVariant(QColor::fromRgbF(0.2, 0.3, 0.4, 0.5)));
    QCOMPARE(object->property("test13"), QVariant(QRectF(10, 10, 10, 10)));
    QCOMPARE(object->property("test14"), QVariant(QPointF(10, 10)));
    QCOMPARE(object->property("test15"), QVariant(QSizeF(10, 10)));
    QCOMPARE(object->property("test16"), QVariant(QVector3D(100, 100, 100)));
    QCOMPARE(object->property("test1Bound"), QVariant(9));
    QCOMPARE(object->property("test20Bound"), QVariant(27));
}

void tst_qqmllanguage::bindJSValueToVariant()
{
    QQmlComponent component(&engine, testFileUrl("assignLiteralToJSValue.qml"));

    VERIFY_ERRORS(0);
    QObject *root = component.create();
    QVERIFY(root != 0);

    QObject *object = root->findChild<QObject *>("variantProperties");

    QVERIFY(isJSNumberType(object->property("test1").userType()));
    QVERIFY(isJSNumberType(object->property("test2").userType()));
    QCOMPARE(object->property("test3").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test4").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test5").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test6").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test7").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test8").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test9").userType(), (int)QVariant::String);
    QCOMPARE(object->property("test10").userType(), (int)QVariant::Bool);
    QCOMPARE(object->property("test11").userType(), (int)QVariant::Bool);
    QCOMPARE(object->property("test12").userType(), (int)QVariant::Color);
    QCOMPARE(object->property("test13").userType(), (int)QVariant::RectF);
    QCOMPARE(object->property("test14").userType(), (int)QVariant::PointF);
    QCOMPARE(object->property("test15").userType(), (int)QVariant::SizeF);
    QCOMPARE(object->property("test16").userType(), (int)QVariant::Vector3D);
    QVERIFY(isJSNumberType(object->property("test1Bound").userType()));
    QVERIFY(isJSNumberType(object->property("test20Bound").userType()));

    QCOMPARE(object->property("test1"), QVariant(5));
    QCOMPARE(object->property("test2"), QVariant((double)1.7));
    QCOMPARE(object->property("test3"), QVariant(QString(QLatin1String("Hello world!"))));
    QCOMPARE(object->property("test4"), QVariant(QString(QLatin1String("#FF008800"))));
    QCOMPARE(object->property("test5"), QVariant(QString(QLatin1String("10,10,10x10"))));
    QCOMPARE(object->property("test6"), QVariant(QString(QLatin1String("10,10"))));
    QCOMPARE(object->property("test7"), QVariant(QString(QLatin1String("10x10"))));
    QCOMPARE(object->property("test8"), QVariant(QString(QLatin1String("100,100,100"))));
    QCOMPARE(object->property("test9"), QVariant(QString(QLatin1String("#FF008800"))));
    QCOMPARE(object->property("test10"), QVariant(bool(true)));
    QCOMPARE(object->property("test11"), QVariant(bool(false)));
    QCOMPARE(object->property("test12"), QVariant(QColor::fromRgbF(0.2, 0.3, 0.4, 0.5)));
    QCOMPARE(object->property("test13"), QVariant(QRectF(10, 10, 10, 10)));
    QCOMPARE(object->property("test14"), QVariant(QPointF(10, 10)));
    QCOMPARE(object->property("test15"), QVariant(QSizeF(10, 10)));
    QCOMPARE(object->property("test16"), QVariant(QVector3D(100, 100, 100)));
    QCOMPARE(object->property("test1Bound"), QVariant(9));
    QCOMPARE(object->property("test20Bound"), QVariant(27));
}

void tst_qqmllanguage::bindJSValueToType()
{
    QQmlComponent component(&engine, testFileUrl("assignLiteralToJSValue.qml"));

    VERIFY_ERRORS(0);
    QObject *root = component.create();
    QVERIFY(root != 0);

    {
        MyTypeObject *object = root->findChild<MyTypeObject *>("typedProperties");

        QCOMPARE(object->intProperty(), 5);
        QCOMPARE(object->doubleProperty(), double(1.7));
        QCOMPARE(object->stringProperty(), QString(QLatin1String("Hello world!")));
        QCOMPARE(object->boolProperty(), true);
        QCOMPARE(object->colorProperty(), QColor::fromRgbF(0.2, 0.3, 0.4, 0.5));
        QCOMPARE(object->rectFProperty(), QRectF(10, 10, 10, 10));
        QCOMPARE(object->pointFProperty(), QPointF(10, 10));
        QCOMPARE(object->sizeFProperty(), QSizeF(10, 10));
        QCOMPARE(object->vectorProperty(), QVector3D(100, 100, 100));
    } {
        MyTypeObject *object = root->findChild<MyTypeObject *>("stringProperties");

        QCOMPARE(object->intProperty(), 1);
        QCOMPARE(object->doubleProperty(), double(1.7));
        QCOMPARE(object->stringProperty(), QString(QLatin1String("Hello world!")));
        QCOMPARE(object->boolProperty(), true);
        QCOMPARE(object->colorProperty(), QColor::fromRgb(0x00, 0x88, 0x00, 0xFF));
        QCOMPARE(object->rectFProperty(), QRectF(10, 10, 10, 10));
        QCOMPARE(object->pointFProperty(), QPointF(10, 10));
        QCOMPARE(object->sizeFProperty(), QSizeF(10, 10));
        QCOMPARE(object->vectorProperty(), QVector3D(100, 100, 100));
    }
}

void tst_qqmllanguage::bindTypeToJSValue()
{
    QQmlComponent component(&engine, testFileUrl("bindTypeToJSValue.qml"));

    VERIFY_ERRORS(0);
    QObject *root = component.create();
    QVERIFY(root != 0);

    {
        MyQmlObject *object = root->findChild<MyQmlObject *>("flagProperty");
        QVERIFY(object);
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isNumber());
        QCOMPARE(value.toNumber(), qreal(MyTypeObject::FlagVal1 | MyTypeObject::FlagVal3));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("enumProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isNumber());
        QCOMPARE(value.toNumber(), qreal(MyTypeObject::EnumVal2));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("stringProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isString());
        QCOMPARE(value.toString(), QString(QLatin1String("Hello World!")));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("uintProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isNumber());
        QCOMPARE(value.toNumber(), qreal(10));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("intProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isNumber());
        QCOMPARE(value.toNumber(), qreal(-19));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("realProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isNumber());
        QCOMPARE(value.toNumber(), qreal(23.2));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("doubleProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isNumber());
        QCOMPARE(value.toNumber(), qreal(-19.7));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("floatProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isNumber());
        QCOMPARE(value.toNumber(), qreal(8.5));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("colorProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isObject());
        QCOMPARE(value.property(QLatin1String("r")).toNumber(), qreal(1.0));
        QCOMPARE(value.property(QLatin1String("g")).toNumber(), qreal(0.0));
        QCOMPARE(value.property(QLatin1String("b")).toNumber(), qreal(0.0));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("dateProperty");
        QJSValue value = object->qjsvalue();
        QCOMPARE(value.toDateTime().isValid(), true);
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("timeProperty");
        QJSValue value = object->qjsvalue();
        QCOMPARE(value.toDateTime().isValid(), true);
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("dateTimeProperty");
        QJSValue value = object->qjsvalue();
        QCOMPARE(value.toDateTime().isValid(), true);
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("pointProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isObject());
        QCOMPARE(value.property(QLatin1String("x")).toNumber(), qreal(99));
        QCOMPARE(value.property(QLatin1String("y")).toNumber(), qreal(13));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("pointFProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isObject());
        QCOMPARE(value.property(QLatin1String("x")).toNumber(), qreal(-10.1));
        QCOMPARE(value.property(QLatin1String("y")).toNumber(), qreal(12.3));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("rectProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isObject());
        QCOMPARE(value.property(QLatin1String("x")).toNumber(), qreal(9));
        QCOMPARE(value.property(QLatin1String("y")).toNumber(), qreal(7));
        QCOMPARE(value.property(QLatin1String("width")).toNumber(), qreal(100));
        QCOMPARE(value.property(QLatin1String("height")).toNumber(), qreal(200));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("rectFProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isObject());
        QCOMPARE(value.property(QLatin1String("x")).toNumber(), qreal(1000.1));
        QCOMPARE(value.property(QLatin1String("y")).toNumber(), qreal(-10.9));
        QCOMPARE(value.property(QLatin1String("width")).toNumber(), qreal(400));
        QCOMPARE(value.property(QLatin1String("height")).toNumber(), qreal(90.99));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("boolProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isBool());
        QCOMPARE(value.toBool(), true);
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("variantProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isString());
        QCOMPARE(value.toString(), QString(QLatin1String("Hello World!")));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("vectorProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isObject());
        QCOMPARE(value.property(QLatin1String("x")).toNumber(), qreal(10.0f));
        QCOMPARE(value.property(QLatin1String("y")).toNumber(), qreal(1.0f));
        QCOMPARE(value.property(QLatin1String("z")).toNumber(), qreal(2.2f));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("vector4Property");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isObject());
        QCOMPARE(value.property(QLatin1String("x")).toNumber(), qreal(10.0f));
        QCOMPARE(value.property(QLatin1String("y")).toNumber(), qreal(1.0f));
        QCOMPARE(value.property(QLatin1String("z")).toNumber(), qreal(2.2f));
        QCOMPARE(value.property(QLatin1String("w")).toNumber(), qreal(2.3f));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("urlProperty");
        QJSValue value = object->qjsvalue();
        const QUrl encoded = QUrl::fromEncoded("main.qml?with%3cencoded%3edata", QUrl::TolerantMode);
        QCOMPARE(value.toString(), component.url().resolved(encoded).toString());
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("objectProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isQObject());
        QVERIFY(qobject_cast<MyTypeObject *>(value.toQObject()));
    } {
        MyQmlObject *object = root->findChild<MyQmlObject *>("varProperty");
        QJSValue value = object->qjsvalue();
        QVERIFY(value.isString());
        QCOMPARE(value.toString(), QString(QLatin1String("Hello World!")));
    }
}

// Tests that custom parser types can be instantiated
void tst_qqmllanguage::customParserTypes()
{
    QQmlComponent component(&engine, testFileUrl("customParserTypes.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property("count"), QVariant(2));
}

// Tests that the root item can be a custom component
void tst_qqmllanguage::rootAsQmlComponent()
{
    QQmlComponent component(&engine, testFileUrl("rootAsQmlComponent.qml"));
    VERIFY_ERRORS(0);
    MyContainer *object = qobject_cast<MyContainer *>(component.create());
    QVERIFY(object != 0);
    QCOMPARE(object->property("x"), QVariant(11));
    QCOMPARE(object->getChildren()->count(), 2);
}

void tst_qqmllanguage::rootItemIsComponent()
{
    QQmlComponent component(&engine, testFileUrl("rootItemIsComponent.qml"));
    VERIFY_ERRORS(0);
    QScopedPointer<QObject> root(component.create());
    QVERIFY(qobject_cast<QQmlComponent*>(root.data()));
    QScopedPointer<QObject> other(qobject_cast<QQmlComponent*>(root.data())->create());
    QVERIFY(!other.isNull());
    QQmlContext *context = qmlContext(other.data());
    QVERIFY(context);
    QCOMPARE(context->nameForObject(other.data()), QStringLiteral("blah"));
}

// Tests that components can be specified inline
void tst_qqmllanguage::inlineQmlComponents()
{
    QQmlComponent component(&engine, testFileUrl("inlineQmlComponents.qml"));
    VERIFY_ERRORS(0);
    MyContainer *object = qobject_cast<MyContainer *>(component.create());
    QVERIFY(object != 0);
    QCOMPARE(object->getChildren()->count(), 1);
    QQmlComponent *comp = qobject_cast<QQmlComponent *>(object->getChildren()->at(0));
    QVERIFY(comp != 0);
    MyQmlObject *compObject = qobject_cast<MyQmlObject *>(comp->create());
    QVERIFY(compObject != 0);
    QCOMPARE(compObject->value(), 11);
}

// Tests that types that have an id property have it set
void tst_qqmllanguage::idProperty()
{
    {
        QQmlComponent component(&engine, testFileUrl("idProperty.qml"));
        VERIFY_ERRORS(0);
        MyContainer *object = qobject_cast<MyContainer *>(component.create());
        QVERIFY(object != 0);
        QCOMPARE(object->getChildren()->count(), 2);
        MyTypeObject *child =
                qobject_cast<MyTypeObject *>(object->getChildren()->at(0));
        QVERIFY(child != 0);
        QCOMPARE(child->id(), QString("myObjectId"));
        QCOMPARE(object->property("object"), QVariant::fromValue((QObject *)child));

        child =
                qobject_cast<MyTypeObject *>(object->getChildren()->at(1));
        QVERIFY(child != 0);
        QCOMPARE(child->id(), QString("name.with.dots"));
    }
    {
        QQmlComponent component(&engine, testFileUrl("idPropertyMismatch.qml"));
        VERIFY_ERRORS(0);
        QScopedPointer<QObject> root(component.create());
        QVERIFY(!root.isNull());
        QQmlContext *ctx = qmlContext(root.data());
        QVERIFY(ctx);
        QCOMPARE(ctx->nameForObject(root.data()), QString("root"));
    }
}

// Tests automatic connection to notify signals if "onBlahChanged" syntax is used
// even if the notify signal for "blah" is not called "blahChanged"
void tst_qqmllanguage::autoNotifyConnection()
{
    QQmlComponent component(&engine, testFileUrl("autoNotifyConnection.qml"));
    VERIFY_ERRORS(0);
    MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
    QVERIFY(object != 0);
    QMetaProperty prop = object->metaObject()->property(object->metaObject()->indexOfProperty("receivedNotify"));
    QVERIFY(prop.isValid());

    QCOMPARE(prop.read(object), QVariant::fromValue(false));
    object->setPropertyWithNotify(1);
    QCOMPARE(prop.read(object), QVariant::fromValue(true));
}

// Tests that signals can be assigned to
void tst_qqmllanguage::assignSignal()
{
    QQmlComponent component(&engine, testFileUrl("assignSignal.qml"));
    VERIFY_ERRORS(0);
    MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
    QVERIFY(object != 0);
    QTest::ignoreMessage(QtWarningMsg, "MyQmlObject::basicSlot");
    emit object->basicSignal();
    QTest::ignoreMessage(QtWarningMsg, "MyQmlObject::basicSlotWithArgs(9)");
    emit object->basicParameterizedSignal(9);
}

void tst_qqmllanguage::assignSignalFunctionExpression()
{
    QQmlComponent component(&engine, testFileUrl("assignSignalFunctionExpression.qml"));
    VERIFY_ERRORS(0);
    MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
    QVERIFY(object != 0);
    QTest::ignoreMessage(QtWarningMsg, "MyQmlObject::basicSlot");
    emit object->basicSignal();
    QTest::ignoreMessage(QtWarningMsg, "MyQmlObject::basicSlotWithArgs(9)");
    emit object->basicParameterizedSignal(9);
}

void tst_qqmllanguage::overrideSignal_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("errorFile");

    QTest::newRow("override signal with signal") << "overrideSignal.1.qml" << "overrideSignal.1.errors.txt";
    QTest::newRow("override signal with method") << "overrideSignal.2.qml" << "overrideSignal.2.errors.txt";
    QTest::newRow("override signal with property") << "overrideSignal.3.qml" << "";
    QTest::newRow("override signal of alias property with signal") << "overrideSignal.4.qml" << "overrideSignal.4.errors.txt";
    QTest::newRow("override signal of superclass with signal") << "overrideSignal.5.qml" << "overrideSignal.5.errors.txt";
    QTest::newRow("override builtin signal with signal") << "overrideSignal.6.qml" << "overrideSignal.6.errors.txt";
}

void tst_qqmllanguage::overrideSignal()
{
    QFETCH(QString, file);
    QFETCH(QString, errorFile);

    QQmlComponent component(&engine, testFileUrl(file));
    if (errorFile.isEmpty()) {
        VERIFY_ERRORS(0);
        QObject *object = component.create();
        QVERIFY(object != 0);
        QVERIFY(object->property("success").toBool());
        delete object;
    } else {
        VERIFY_ERRORS(errorFile.toLatin1().constData());
    }
}

// Tests the creation and assignment of dynamic properties
void tst_qqmllanguage::dynamicProperties()
{
    QQmlComponent component(&engine, testFileUrl("dynamicProperties.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property("intProperty"), QVariant(10));
    QCOMPARE(object->property("boolProperty"), QVariant(false));
    QCOMPARE(object->property("doubleProperty"), QVariant(-10.1));
    QCOMPARE(object->property("realProperty"), QVariant((qreal)-19.9));
    QCOMPARE(object->property("stringProperty"), QVariant("Hello World!"));
    QCOMPARE(object->property("urlProperty"), QVariant(testFileUrl("main.qml")));
    QCOMPARE(object->property("colorProperty"), QVariant(QColor("red")));
    QCOMPARE(object->property("dateProperty"), QVariant(QDate(1945, 9, 2)));
    QCOMPARE(object->property("varProperty"), QVariant("Hello World!"));
}

// Test that nested types can use dynamic properties
void tst_qqmllanguage::dynamicPropertiesNested()
{
    QQmlComponent component(&engine, testFileUrl("dynamicPropertiesNested.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("super_a").toInt(), 11); // Overridden
    QCOMPARE(object->property("super_c").toInt(), 14); // Inherited
    QCOMPARE(object->property("a").toInt(), 13); // New
    QCOMPARE(object->property("b").toInt(), 12); // New

    delete object;
}

// Tests the creation and assignment to dynamic list properties
void tst_qqmllanguage::listProperties()
{
    QQmlComponent component(&engine, testFileUrl("listProperties.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test").toInt(), 2);
}

void tst_qqmllanguage::badListItemType()
{
    QQmlComponent component(&engine, testFileUrl("badListItemType.qml"));
    QVERIFY(component.isError());
    VERIFY_ERRORS("badListItemType.errors.txt");
}

// Tests the creation and assignment of dynamic object properties
// ### Not complete
void tst_qqmllanguage::dynamicObjectProperties()
{
    {
    QQmlComponent component(&engine, testFileUrl("dynamicObjectProperties.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("objectProperty"), qVariantFromValue((QObject*)0));
    QVERIFY(object->property("objectProperty2") != qVariantFromValue((QObject*)0));
    }
    {
    QQmlComponent component(&engine, testFileUrl("dynamicObjectProperties.2.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);

    QVERIFY(object->property("objectProperty") != qVariantFromValue((QObject*)0));
    }
}

// Tests the declaration of dynamic signals and slots
void tst_qqmllanguage::dynamicSignalsAndSlots()
{
    QTest::ignoreMessage(QtDebugMsg, "1921");

    QQmlComponent component(&engine, testFileUrl("dynamicSignalsAndSlots.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);
    QVERIFY(object->metaObject()->indexOfMethod("signal1()") != -1);
    QVERIFY(object->metaObject()->indexOfMethod("signal2()") != -1);
    QVERIFY(object->metaObject()->indexOfMethod("slot1()") != -1);
    QVERIFY(object->metaObject()->indexOfMethod("slot2()") != -1);

    QCOMPARE(object->property("test").toInt(), 0);
    QMetaObject::invokeMethod(object, "slot3", Qt::DirectConnection, Q_ARG(QVariant, QVariant(10)));
    QCOMPARE(object->property("test").toInt(), 10);
}

void tst_qqmllanguage::simpleBindings()
{
    QQmlComponent component(&engine, testFileUrl("simpleBindings.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property("value1"), QVariant(10));
    QCOMPARE(object->property("value2"), QVariant(10));
    QCOMPARE(object->property("value3"), QVariant(21));
    QCOMPARE(object->property("value4"), QVariant(10));
    QCOMPARE(object->property("objectProperty"), QVariant::fromValue(object));
}

void tst_qqmllanguage::autoComponentCreation()
{
    {
        QQmlComponent component(&engine, testFileUrl("autoComponentCreation.qml"));
        VERIFY_ERRORS(0);
        MyTypeObject *object = qobject_cast<MyTypeObject *>(component.create());
        QVERIFY(object != 0);
        QVERIFY(object->componentProperty() != 0);
        MyTypeObject *child = qobject_cast<MyTypeObject *>(object->componentProperty()->create());
        QVERIFY(child != 0);
        QCOMPARE(child->realProperty(), qreal(9));
    }
    {
        QQmlComponent component(&engine, testFileUrl("autoComponentCreation.2.qml"));
        VERIFY_ERRORS(0);
        MyTypeObject *object = qobject_cast<MyTypeObject *>(component.create());
        QVERIFY(object != 0);
        QVERIFY(object->componentProperty() != 0);
        MyTypeObject *child = qobject_cast<MyTypeObject *>(object->componentProperty()->create());
        QVERIFY(child != 0);
        QCOMPARE(child->realProperty(), qreal(9));
    }
}

void tst_qqmllanguage::autoComponentCreationInGroupProperty()
{
    QQmlComponent component(&engine, testFileUrl("autoComponentCreationInGroupProperties.qml"));
    VERIFY_ERRORS(0);
    MyTypeObject *object = qobject_cast<MyTypeObject *>(component.create());
    QVERIFY(object != 0);
    QVERIFY(object->componentProperty() != 0);
    MyTypeObject *child = qobject_cast<MyTypeObject *>(object->componentProperty()->create());
    QVERIFY(child != 0);
    QCOMPARE(child->realProperty(), qreal(9));
}

void tst_qqmllanguage::propertyValueSource()
{
    {
    QQmlComponent component(&engine, testFileUrl("propertyValueSource.qml"));
    VERIFY_ERRORS(0);
    MyTypeObject *object = qobject_cast<MyTypeObject *>(component.create());
    QVERIFY(object != 0);

    QList<QObject *> valueSources;
    QObjectList allChildren = object->findChildren<QObject*>();
    foreach (QObject *child, allChildren) {
        if (qobject_cast<QQmlPropertyValueSource *>(child))
            valueSources.append(child);
    }

    QCOMPARE(valueSources.count(), 1);
    MyPropertyValueSource *valueSource =
        qobject_cast<MyPropertyValueSource *>(valueSources.at(0));
    QVERIFY(valueSource != 0);
    QCOMPARE(valueSource->prop.object(), qobject_cast<QObject*>(object));
    QCOMPARE(valueSource->prop.name(), QString(QLatin1String("intProperty")));
    }

    {
    QQmlComponent component(&engine, testFileUrl("propertyValueSource.2.qml"));
    VERIFY_ERRORS(0);
    MyTypeObject *object = qobject_cast<MyTypeObject *>(component.create());
    QVERIFY(object != 0);

    QList<QObject *> valueSources;
    QObjectList allChildren = object->findChildren<QObject*>();
    foreach (QObject *child, allChildren) {
        if (qobject_cast<QQmlPropertyValueSource *>(child))
            valueSources.append(child);
    }

    QCOMPARE(valueSources.count(), 1);
    MyPropertyValueSource *valueSource =
        qobject_cast<MyPropertyValueSource *>(valueSources.at(0));
    QVERIFY(valueSource != 0);
    QCOMPARE(valueSource->prop.object(), qobject_cast<QObject*>(object));
    QCOMPARE(valueSource->prop.name(), QString(QLatin1String("intProperty")));
    }
}

void tst_qqmllanguage::attachedProperties()
{
    QQmlComponent component(&engine, testFileUrl("attachedProperties.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);
    QObject *attached = qmlAttachedPropertiesObject<MyQmlObject>(object);
    QVERIFY(attached != 0);
    QCOMPARE(attached->property("value"), QVariant(10));
    QCOMPARE(attached->property("value2"), QVariant(13));
}

// Tests non-static object properties
void tst_qqmllanguage::dynamicObjects()
{
    QQmlComponent component(&engine, testFileUrl("dynamicObject.1.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);
}

// Tests the registration of custom variant string converters
void tst_qqmllanguage::customVariantTypes()
{
    QQmlComponent component(&engine, testFileUrl("customVariantTypes.qml"));
    VERIFY_ERRORS(0);
    MyQmlObject *object = qobject_cast<MyQmlObject*>(component.create());
    QVERIFY(object != 0);
    QCOMPARE(object->customType().a, 10);
}

void tst_qqmllanguage::valueTypes()
{
    QQmlComponent component(&engine, testFileUrl("valueTypes.qml"));
    VERIFY_ERRORS(0);

    QString message = component.url().toString() + ":2:1: QML MyTypeObject: Binding loop detected for property \"rectProperty.width\"";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(message));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(message));

    MyTypeObject *object = qobject_cast<MyTypeObject*>(component.create());
    QVERIFY(object != 0);


    QCOMPARE(object->rectProperty(), QRect(10, 11, 12, 13));
    QCOMPARE(object->rectProperty2(), QRect(10, 11, 12, 13));
    QCOMPARE(object->intProperty(), 10);
    object->doAction();
    QCOMPARE(object->rectProperty(), QRect(12, 11, 14, 13));
    QCOMPARE(object->rectProperty2(), QRect(12, 11, 14, 13));
    QCOMPARE(object->intProperty(), 12);

    // ###
#if 0
    QQmlProperty p(object, "rectProperty.x");
    QCOMPARE(p.read(), QVariant(12));
    p.write(13);
    QCOMPARE(p.read(), QVariant(13));

    quint32 r = QQmlPropertyPrivate::saveValueType(p.coreIndex(), p.valueTypeCoreIndex());
    QQmlProperty p2;
    QQmlPropertyPrivate::restore(p2, r, object);
    QCOMPARE(p2.read(), QVariant(13));
#endif
}

void tst_qqmllanguage::cppnamespace()
{
    {
        QQmlComponent component(&engine, testFileUrl("cppnamespace.qml"));
        VERIFY_ERRORS(0);
        QObject *object = component.create();
        QVERIFY(object != 0);
        delete object;
    }

    {
        QQmlComponent component(&engine, testFileUrl("cppnamespace.2.qml"));
        VERIFY_ERRORS(0);
        QObject *object = component.create();
        QVERIFY(object != 0);
        delete object;
    }
}

void tst_qqmllanguage::aliasProperties()
{
    // Simple "int" alias
    {
        QQmlComponent component(&engine, testFileUrl("alias.1.qml"));
        VERIFY_ERRORS(0);
        QObject *object = component.create();
        QVERIFY(object != 0);

        // Read through alias
        QCOMPARE(object->property("valueAlias").toInt(), 10);
        object->setProperty("value", QVariant(13));
        QCOMPARE(object->property("valueAlias").toInt(), 13);

        // Write through alias
        object->setProperty("valueAlias", QVariant(19));
        QCOMPARE(object->property("valueAlias").toInt(), 19);
        QCOMPARE(object->property("value").toInt(), 19);

        delete object;
    }

    // Complex object alias
    {
        QQmlComponent component(&engine, testFileUrl("alias.2.qml"));
        VERIFY_ERRORS(0);
        QObject *object = component.create();
        QVERIFY(object != 0);

        // Read through alias
        MyQmlObject *v =
            qvariant_cast<MyQmlObject *>(object->property("aliasObject"));
        QVERIFY(v != 0);
        QCOMPARE(v->value(), 10);

        // Write through alias
        MyQmlObject *v2 = new MyQmlObject();
        v2->setParent(object);
        object->setProperty("aliasObject", qVariantFromValue(v2));
        MyQmlObject *v3 =
            qvariant_cast<MyQmlObject *>(object->property("aliasObject"));
        QVERIFY(v3 != 0);
        QCOMPARE(v3, v2);

        delete object;
    }

    // Nested aliases
    {
        QQmlComponent component(&engine, testFileUrl("alias.3.qml"));
        VERIFY_ERRORS(0);
        QObject *object = component.create();
        QVERIFY(object != 0);

        QCOMPARE(object->property("value").toInt(), 1892);
        QCOMPARE(object->property("value2").toInt(), 1892);

        object->setProperty("value", QVariant(1313));
        QCOMPARE(object->property("value").toInt(), 1313);
        QCOMPARE(object->property("value2").toInt(), 1313);

        object->setProperty("value2", QVariant(8080));
        QCOMPARE(object->property("value").toInt(), 8080);
        QCOMPARE(object->property("value2").toInt(), 8080);

        delete object;
    }

    // Enum aliases
    {
        QQmlComponent component(&engine, testFileUrl("alias.4.qml"));
        VERIFY_ERRORS(0);
        QObject *object = component.create();
        QVERIFY(object != 0);

        QCOMPARE(object->property("enumAlias").toInt(), 1);

        delete object;
    }

    // Id aliases
    {
        QQmlComponent component(&engine, testFileUrl("alias.5.qml"));
        VERIFY_ERRORS(0);
        QObject *object = component.create();
        QVERIFY(object != 0);

        QVariant v = object->property("otherAlias");
        QCOMPARE(v.userType(), qMetaTypeId<MyQmlObject*>());
        MyQmlObject *o = qvariant_cast<MyQmlObject*>(v);
        QCOMPARE(o->value(), 10);

        delete o;

        v = object->property("otherAlias");
        QCOMPARE(v.userType(), qMetaTypeId<MyQmlObject*>());
        o = qvariant_cast<MyQmlObject*>(v);
        QVERIFY(!o);

        delete object;
    }

    // Nested aliases - this used to cause a crash
    {
        QQmlComponent component(&engine, testFileUrl("alias.6.qml"));
        VERIFY_ERRORS(0);
        QObject *object = component.create();
        QVERIFY(object != 0);

        QCOMPARE(object->property("a").toInt(), 1923);
    }

    // Ptr Alias Cleanup - check that aliases to ptr types return 0
    // if the object aliased to is removed
    {
        QQmlComponent component(&engine, testFileUrl("alias.7.qml"));
        VERIFY_ERRORS(0);

        QObject *object = component.create();
        QVERIFY(object != 0);

        QObject *object1 = qvariant_cast<QObject *>(object->property("object"));
        QVERIFY(object1 != 0);
        QObject *object2 = qvariant_cast<QObject *>(object1->property("object"));
        QVERIFY(object2 != 0);

        QObject *alias = qvariant_cast<QObject *>(object->property("aliasedObject"));
        QCOMPARE(alias, object2);

        delete object1;

        QObject *alias2 = object; // "Random" start value
        int status = -1;
        void *a[] = { &alias2, 0, &status };
        QMetaObject::metacall(object, QMetaObject::ReadProperty,
                              object->metaObject()->indexOfProperty("aliasedObject"), a);
        QVERIFY(!alias2);
    }

    // Simple composite type
    {
        QQmlComponent component(&engine, testFileUrl("alias.8.qml"));
        VERIFY_ERRORS(0);
        QObject *object = component.create();
        QVERIFY(object != 0);

        QCOMPARE(object->property("value").toInt(), 10);

        delete object;
    }

    // Complex composite type
    {
        QQmlComponent component(&engine, testFileUrl("alias.9.qml"));
        VERIFY_ERRORS(0);
        QObject *object = component.create();
        QVERIFY(object != 0);

        QCOMPARE(object->property("value").toInt(), 10);

        delete object;
    }

    // Valuetype alias
    // Simple "int" alias
    {
        QQmlComponent component(&engine, testFileUrl("alias.10.qml"));
        VERIFY_ERRORS(0);
        QObject *object = component.create();
        QVERIFY(object != 0);

        // Read through alias
        QCOMPARE(object->property("valueAlias").toRect(), QRect(10, 11, 9, 8));
        object->setProperty("rectProperty", QVariant(QRect(33, 12, 99, 100)));
        QCOMPARE(object->property("valueAlias").toRect(), QRect(33, 12, 99, 100));

        // Write through alias
        object->setProperty("valueAlias", QVariant(QRect(3, 3, 4, 9)));
        QCOMPARE(object->property("valueAlias").toRect(), QRect(3, 3, 4, 9));
        QCOMPARE(object->property("rectProperty").toRect(), QRect(3, 3, 4, 9));

        delete object;
    }

    // Valuetype sub-alias
    {
        QQmlComponent component(&engine, testFileUrl("alias.11.qml"));
        VERIFY_ERRORS(0);
        QObject *object = component.create();
        QVERIFY(object != 0);

        // Read through alias
        QCOMPARE(object->property("aliasProperty").toInt(), 19);
        object->setProperty("rectProperty", QVariant(QRect(33, 8, 102, 111)));
        QCOMPARE(object->property("aliasProperty").toInt(), 33);

        // Write through alias
        object->setProperty("aliasProperty", QVariant(4));
        QCOMPARE(object->property("aliasProperty").toInt(), 4);
        QCOMPARE(object->property("rectProperty").toRect(), QRect(4, 8, 102, 111));

        delete object;
    }

    // Nested aliases with a qml file
    {
        QQmlComponent component(&engine, testFileUrl("alias.12.qml"));
        VERIFY_ERRORS(0);
        QScopedPointer<QObject> object(component.create());
        QVERIFY(!object.isNull());

        QPointer<QObject> subObject = qvariant_cast<QObject*>(object->property("referencingSubObject"));
        QVERIFY(!subObject.isNull());

        QVERIFY(subObject->property("success").toBool());
    }

    // Nested aliases with a qml file with reverse ordering
    {
        // This is known to fail at the moment.
        QQmlComponent component(&engine, testFileUrl("alias.13.qml"));
        VERIFY_ERRORS(0);
        QScopedPointer<QObject> object(component.create());
        QVERIFY(!object.isNull());

        QPointer<QObject> subObject = qvariant_cast<QObject*>(object->property("referencingSubObject"));
        QVERIFY(!subObject.isNull());

        QVERIFY(subObject->property("success").toBool());
    }

    // "Nested" aliases within an object that require iterative resolution
    {
        // This is known to fail at the moment.
        QQmlComponent component(&engine, testFileUrl("alias.14.qml"));
        VERIFY_ERRORS(0);

        QScopedPointer<QObject> object(component.create());
        QVERIFY(!object.isNull());

        QPointer<QObject> subObject = qvariant_cast<QObject*>(object->property("referencingSubObject"));
        QVERIFY(!subObject.isNull());

        QVERIFY(subObject->property("success").toBool());
    }
}

// QTBUG-13374 Test that alias properties and signals can coexist
void tst_qqmllanguage::aliasPropertiesAndSignals()
{
    QQmlComponent component(&engine, testFileUrl("aliasPropertiesAndSignals.qml"));
    VERIFY_ERRORS(0);
    QObject *o = component.create();
    QVERIFY(o);
    QCOMPARE(o->property("test").toBool(), true);
    delete o;
}

// Test that the root element in a composite type can be a Component
void tst_qqmllanguage::componentCompositeType()
{
    QQmlComponent component(&engine, testFileUrl("componentCompositeType.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);
}

class TestType : public QObject {
    Q_OBJECT
public:
    TestType(QObject *p=0) : QObject(p) {}
};

class TestType2 : public QObject {
    Q_OBJECT
public:
    TestType2(QObject *p=0) : QObject(p) {}
};

void tst_qqmllanguage::i18n_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("stringProperty");
    QTest::newRow("i18nStrings") << "i18nStrings.qml" << QString::fromUtf8("Test \303\241\303\242\303\243\303\244\303\245 (5 accented 'a' letters)");
    QTest::newRow("i18nDeclaredPropertyNames") << "i18nDeclaredPropertyNames.qml" << QString::fromUtf8("Test \303\241\303\242\303\243\303\244\303\245: 10");
    QTest::newRow("i18nDeclaredPropertyUse") << "i18nDeclaredPropertyUse.qml" << QString::fromUtf8("Test \303\241\303\242\303\243\303\244\303\245: 15");
    QTest::newRow("i18nScript") << "i18nScript.qml" << QString::fromUtf8("Test \303\241\303\242\303\243\303\244\303\245: 20");
    QTest::newRow("i18nType") << "i18nType.qml" << QString::fromUtf8("Test \303\241\303\242\303\243\303\244\303\245: 30");
    QTest::newRow("i18nNameSpace") << "i18nNameSpace.qml" << QString::fromUtf8("Test \303\241\303\242\303\243\303\244\303\245: 40");
}

void tst_qqmllanguage::i18n()
{
    QFETCH(QString, file);
    QFETCH(QString, stringProperty);
    QQmlComponent component(&engine, testFileUrl(file));
    VERIFY_ERRORS(0);
    MyTypeObject *object = qobject_cast<MyTypeObject *>(component.create());
    QVERIFY(object != 0);
    QCOMPARE(object->stringProperty(), stringProperty);

    delete object;
}

// Check that the Component::onCompleted attached property works
void tst_qqmllanguage::onCompleted()
{
    QQmlComponent component(&engine, testFileUrl("onCompleted.qml"));
    VERIFY_ERRORS(0);
    QTest::ignoreMessage(QtDebugMsg, "Completed 6 10");
    QTest::ignoreMessage(QtDebugMsg, "Completed 6 10");
    QTest::ignoreMessage(QtDebugMsg, "Completed 10 11");
    QObject *object = component.create();
    QVERIFY(object != 0);
}

// Check that the Component::onDestruction attached property works
void tst_qqmllanguage::onDestruction()
{
    QQmlComponent component(&engine, testFileUrl("onDestruction.qml"));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);
    QTest::ignoreMessage(QtDebugMsg, "Destruction 6 10");
    QTest::ignoreMessage(QtDebugMsg, "Destruction 6 10");
    QTest::ignoreMessage(QtDebugMsg, "Destruction 10 11");
    delete object;
}

// Check that assignments to QQmlScriptString properties work
void tst_qqmllanguage::scriptString()
{
    {
        QQmlComponent component(&engine, testFileUrl("scriptString.qml"));
        VERIFY_ERRORS(0);

        MyTypeObject *object = qobject_cast<MyTypeObject*>(component.create());
        QVERIFY(object != 0);
        QVERIFY(!object->scriptProperty().isEmpty());
        QCOMPARE(object->scriptProperty().stringLiteral(), QString());
        bool ok;
        QCOMPARE(object->scriptProperty().numberLiteral(&ok), qreal(0.));
        QCOMPARE(ok, false);

        const QQmlScriptStringPrivate *scriptPrivate = QQmlScriptStringPrivate::get(object->scriptProperty());
        QVERIFY(scriptPrivate != 0);
        QCOMPARE(scriptPrivate->script, QString("foo + bar"));
        QCOMPARE(scriptPrivate->scope, qobject_cast<QObject*>(object));
        QCOMPARE(scriptPrivate->context, qmlContext(object));

        QVERIFY(object->grouped() != 0);
        const QQmlScriptStringPrivate *groupedPrivate = QQmlScriptStringPrivate::get(object->grouped()->script());
        QCOMPARE(groupedPrivate->script, QString("console.log(1921)"));
        QCOMPARE(groupedPrivate->scope, qobject_cast<QObject*>(object));
        QCOMPARE(groupedPrivate->context, qmlContext(object));
    }

    {
        QQmlComponent component(&engine, testFileUrl("scriptString2.qml"));
        VERIFY_ERRORS(0);

        MyTypeObject *object = qobject_cast<MyTypeObject*>(component.create());
        QVERIFY(object != 0);
        QCOMPARE(object->scriptProperty().stringLiteral(), QString("hello\\n\\\"world\\\""));
    }

    {
        QQmlComponent component(&engine, testFileUrl("scriptString3.qml"));
        VERIFY_ERRORS(0);

        MyTypeObject *object = qobject_cast<MyTypeObject*>(component.create());
        QVERIFY(object != 0);
        bool ok;
        QCOMPARE(object->scriptProperty().numberLiteral(&ok), qreal(12.345));
        QCOMPARE(ok, true);

    }

    {
        QQmlComponent component(&engine, testFileUrl("scriptString4.qml"));
        VERIFY_ERRORS(0);

        MyTypeObject *object = qobject_cast<MyTypeObject*>(component.create());
        QVERIFY(object != 0);
        bool ok;
        QCOMPARE(object->scriptProperty().booleanLiteral(&ok), true);
        QCOMPARE(ok, true);
    }

    {
        QQmlComponent component(&engine, testFileUrl("scriptString5.qml"));
        VERIFY_ERRORS(0);

        MyTypeObject *object = qobject_cast<MyTypeObject*>(component.create());
        QVERIFY(object != 0);
        QCOMPARE(object->scriptProperty().isNullLiteral(), true);
    }

    {
        QQmlComponent component(&engine, testFileUrl("scriptString6.qml"));
        VERIFY_ERRORS(0);

        MyTypeObject *object = qobject_cast<MyTypeObject*>(component.create());
        QVERIFY(object != 0);
        QCOMPARE(object->scriptProperty().isUndefinedLiteral(), true);
    }
    {
        QQmlComponent component(&engine, testFileUrl("scriptString7.qml"));
        VERIFY_ERRORS(0);

        MyTypeObject *object = qobject_cast<MyTypeObject*>(component.create());
        QVERIFY(object != 0);
        QQmlScriptString ss = object->scriptProperty();

        {
            QQmlExpression expr(ss, /*context*/0, object);
            QCOMPARE(expr.evaluate().toInt(), int(100));
        }

        {
            SimpleObjectWithCustomParser testScope;
            QVERIFY(testScope.metaObject()->indexOfProperty("intProperty") != object->metaObject()->indexOfProperty("intProperty"));

            testScope.setIntProperty(42);
            QQmlExpression expr(ss, /*context*/0, &testScope);
            QCOMPARE(expr.evaluate().toInt(), int(42));
        }
    }
}

// Check that assignments to QQmlScriptString properties works also from within Javascript
void tst_qqmllanguage::scriptStringJs()
{
    QQmlComponent component(&engine, testFileUrl("scriptStringJs.qml"));
    VERIFY_ERRORS(0);

    MyTypeObject *object = qobject_cast<MyTypeObject*>(component.create());
    QVERIFY(object != 0);
    QQmlContext *context = QQmlEngine::contextForObject(object);
    QVERIFY(context != 0);
    bool ok;

    QCOMPARE(QQmlScriptStringPrivate::get(object->scriptProperty())->script, QString("\" hello \\\" world \""));
    QVERIFY(!object->scriptProperty().isEmpty());
    QVERIFY(!object->scriptProperty().isUndefinedLiteral());
    QVERIFY(!object->scriptProperty().isNullLiteral());
    QCOMPARE(object->scriptProperty().stringLiteral(), QString(" hello \\\" world "));
    QVERIFY(object->scriptProperty().numberLiteral(&ok) == 0.0 && !ok);
    QVERIFY(!object->scriptProperty().booleanLiteral(&ok) && !ok);

    QJSValue inst = engine.newQObject(object);
    QJSValue func = engine.evaluate("function(value) { this.scriptProperty = value }");

    func.callWithInstance(inst, QJSValueList() << "test a \"string ");
    QCOMPARE(QQmlScriptStringPrivate::get(object->scriptProperty())->script, QString("\"test a \\\"string \""));
    QVERIFY(!object->scriptProperty().isEmpty());
    QVERIFY(!object->scriptProperty().isUndefinedLiteral());
    QVERIFY(!object->scriptProperty().isNullLiteral());
    QCOMPARE(object->scriptProperty().stringLiteral(), QString("test a \\\"string "));
    QVERIFY(object->scriptProperty().numberLiteral(&ok) == 0.0 && !ok);
    QVERIFY(!object->scriptProperty().booleanLiteral(&ok) && !ok);

    func.callWithInstance(inst, QJSValueList() << QJSValue::UndefinedValue);
    QCOMPARE(QQmlScriptStringPrivate::get(object->scriptProperty())->script, QString("undefined"));
    QVERIFY(!object->scriptProperty().isEmpty());
    QVERIFY(object->scriptProperty().isUndefinedLiteral());
    QVERIFY(!object->scriptProperty().isNullLiteral());
    QVERIFY(object->scriptProperty().stringLiteral().isEmpty());
    QVERIFY(object->scriptProperty().numberLiteral(&ok) == 0.0 && !ok);
    QVERIFY(!object->scriptProperty().booleanLiteral(&ok) && !ok);

    func.callWithInstance(inst, QJSValueList() << true);
    QCOMPARE(QQmlScriptStringPrivate::get(object->scriptProperty())->script, QString("true"));
    QVERIFY(!object->scriptProperty().isEmpty());
    QVERIFY(!object->scriptProperty().isUndefinedLiteral());
    QVERIFY(!object->scriptProperty().isNullLiteral());
    QVERIFY(object->scriptProperty().stringLiteral().isEmpty());
    QVERIFY(object->scriptProperty().numberLiteral(&ok) == 0.0 && !ok);
    QVERIFY(object->scriptProperty().booleanLiteral(&ok) && ok);

    func.callWithInstance(inst, QJSValueList() << false);
    QCOMPARE(QQmlScriptStringPrivate::get(object->scriptProperty())->script, QString("false"));
    QVERIFY(!object->scriptProperty().isEmpty());
    QVERIFY(!object->scriptProperty().isUndefinedLiteral());
    QVERIFY(!object->scriptProperty().isNullLiteral());
    QVERIFY(object->scriptProperty().stringLiteral().isEmpty());
    QVERIFY(object->scriptProperty().numberLiteral(&ok) == 0.0 && !ok);
    QVERIFY(!object->scriptProperty().booleanLiteral(&ok) && ok);

    func.callWithInstance(inst, QJSValueList() << QJSValue::NullValue);
    QCOMPARE(QQmlScriptStringPrivate::get(object->scriptProperty())->script, QString("null"));
    QVERIFY(!object->scriptProperty().isEmpty());
    QVERIFY(!object->scriptProperty().isUndefinedLiteral());
    QVERIFY(object->scriptProperty().isNullLiteral());
    QVERIFY(object->scriptProperty().stringLiteral().isEmpty());
    QVERIFY(object->scriptProperty().numberLiteral(&ok) == 0.0 && !ok);
    QVERIFY(!object->scriptProperty().booleanLiteral(&ok) && !ok);

    func.callWithInstance(inst, QJSValueList() << 12.34);
    QCOMPARE(QQmlScriptStringPrivate::get(object->scriptProperty())->script, QString("12.34"));
    QVERIFY(!object->scriptProperty().isEmpty());
    QVERIFY(!object->scriptProperty().isUndefinedLiteral());
    QVERIFY(!object->scriptProperty().isNullLiteral());
    QVERIFY(object->scriptProperty().stringLiteral().isEmpty());
    QVERIFY(object->scriptProperty().numberLiteral(&ok) == 12.34 && ok);
    QVERIFY(!object->scriptProperty().booleanLiteral(&ok) && !ok);
}

void tst_qqmllanguage::scriptStringWithoutSourceCode()
{
    QUrl url = testFileUrl("scriptString7.qml");
    {
        QQmlEnginePrivate *eng = QQmlEnginePrivate::get(&engine);
        QQmlTypeData *td = eng->typeLoader.getType(url);
        Q_ASSERT(td);

        const QV4::CompiledData::Unit *readOnlyQmlUnit = td->compilationUnit()->data;
        Q_ASSERT(readOnlyQmlUnit);
        QV4::CompiledData::Unit *qmlUnit = reinterpret_cast<QV4::CompiledData::Unit *>(malloc(readOnlyQmlUnit->unitSize));
        memcpy(qmlUnit, readOnlyQmlUnit, readOnlyQmlUnit->unitSize);
        qmlUnit->flags &= ~QV4::CompiledData::Unit::StaticData;
        td->compilationUnit()->data = qmlUnit;

        const QV4::CompiledData::Object *rootObject = qmlUnit->objectAt(qmlUnit->indexOfRootObject);
        QCOMPARE(qmlUnit->stringAt(rootObject->inheritedTypeNameIndex), QString("MyTypeObject"));
        quint32 i;
        for (i = 0; i < rootObject->nBindings; ++i) {
            const QV4::CompiledData::Binding *binding = rootObject->bindingTable() + i;
            if (qmlUnit->stringAt(binding->propertyNameIndex) != QString("scriptProperty"))
                continue;
            QCOMPARE(binding->valueAsScriptString(qmlUnit), QString("intProperty"));
            const_cast<QV4::CompiledData::Binding*>(binding)->stringIndex = 0; // empty string index
            QVERIFY(binding->valueAsScriptString(qmlUnit).isEmpty());
            break;
        }
        QVERIFY(i < rootObject->nBindings);
    }
    QQmlComponent component(&engine, url);
    VERIFY_ERRORS(0);

    MyTypeObject *object = qobject_cast<MyTypeObject*>(component.create());
    QVERIFY(object != 0);
    QQmlScriptString ss = object->scriptProperty();
    QVERIFY(!ss.isEmpty());
    QCOMPARE(ss.stringLiteral(), QString());
    bool ok;
    QCOMPARE(ss.numberLiteral(&ok), qreal(0.));
    QCOMPARE(ok, false);

    const QQmlScriptStringPrivate *scriptPrivate = QQmlScriptStringPrivate::get(ss);
    QVERIFY(scriptPrivate != 0);
    QVERIFY(scriptPrivate->script.isEmpty());
    QCOMPARE(scriptPrivate->scope, qobject_cast<QObject*>(object));
    QCOMPARE(scriptPrivate->context, qmlContext(object));

    {
        QQmlExpression expr(ss, /*context*/0, object);
        QCOMPARE(expr.evaluate().toInt(), int(100));
    }
}

// Test the QQmlScriptString comparison operators. The script strings are considered
// equal if there evaluation would produce the same result.
void tst_qqmllanguage::scriptStringComparison()
{
    QQmlComponent component1(&engine, testFileUrl("scriptString.qml"));
    QVERIFY(!component1.isError() && component1.errors().isEmpty());
    MyTypeObject *object1 = qobject_cast<MyTypeObject*>(component1.create());
    QVERIFY(object1 != 0);

    QQmlComponent component2(&engine, testFileUrl("scriptString2.qml"));
    QVERIFY(!component2.isError() && component2.errors().isEmpty());
    MyTypeObject *object2 = qobject_cast<MyTypeObject*>(component2.create());
    QVERIFY(object2 != 0);

    QQmlComponent component3(&engine, testFileUrl("scriptString3.qml"));
    QVERIFY(!component3.isError() && component3.errors().isEmpty());
    MyTypeObject *object3 = qobject_cast<MyTypeObject*>(component3.create());
    QVERIFY(object3 != 0);

    //QJSValue inst1 = engine.newQObject(object1);
    QJSValue inst2 = engine.newQObject(object2);
    QJSValue inst3 = engine.newQObject(object3);
    QJSValue func = engine.evaluate("function(value) { this.scriptProperty = value }");

    const QString s = "hello\\n\\\"world\\\"";
    const qreal n = 12.345;
    bool ok;

    QCOMPARE(object2->scriptProperty().stringLiteral(), s);
    QVERIFY(object3->scriptProperty().numberLiteral(&ok) == n && ok);
    QCOMPARE(object1->scriptProperty(), object1->scriptProperty());
    QCOMPARE(object2->scriptProperty(), object2->scriptProperty());
    QCOMPARE(object3->scriptProperty(), object3->scriptProperty());
    QVERIFY(object2->scriptProperty() != object3->scriptProperty());
    QVERIFY(object1->scriptProperty() != object2->scriptProperty());
    QVERIFY(object1->scriptProperty() != object3->scriptProperty());

    func.callWithInstance(inst2, QJSValueList() << n);
    QCOMPARE(object2->scriptProperty(), object3->scriptProperty());

    func.callWithInstance(inst2, QJSValueList() << s);
    QVERIFY(object2->scriptProperty() != object3->scriptProperty());
    func.callWithInstance(inst3, QJSValueList() << s);
    QCOMPARE(object2->scriptProperty(), object3->scriptProperty());

    func.callWithInstance(inst2, QJSValueList() << QJSValue::UndefinedValue);
    QVERIFY(object2->scriptProperty() != object3->scriptProperty());
    func.callWithInstance(inst3, QJSValueList() << QJSValue::UndefinedValue);
    QCOMPARE(object2->scriptProperty(), object3->scriptProperty());

    func.callWithInstance(inst2, QJSValueList() << QJSValue::NullValue);
    QVERIFY(object2->scriptProperty() != object3->scriptProperty());
    func.callWithInstance(inst3, QJSValueList() << QJSValue::NullValue);
    QCOMPARE(object2->scriptProperty(), object3->scriptProperty());

    func.callWithInstance(inst2, QJSValueList() << false);
    QVERIFY(object2->scriptProperty() != object3->scriptProperty());
    func.callWithInstance(inst3, QJSValueList() << false);
    QCOMPARE(object2->scriptProperty(), object3->scriptProperty());

    func.callWithInstance(inst2, QJSValueList() << true);
    QVERIFY(object2->scriptProperty() != object3->scriptProperty());
    func.callWithInstance(inst3, QJSValueList() << true);
    QCOMPARE(object2->scriptProperty(), object3->scriptProperty());

    QVERIFY(object1->scriptProperty() != object2->scriptProperty());
    object2->setScriptProperty(object1->scriptProperty());
    QCOMPARE(object1->scriptProperty(), object2->scriptProperty());

    QVERIFY(object1->scriptProperty() != object3->scriptProperty());
    func.callWithInstance(inst3, QJSValueList() << engine.toScriptValue(object1->scriptProperty()));
    QCOMPARE(object1->scriptProperty(), object3->scriptProperty());

    // While this are two instances of the same object they are still considered different
    // because the (none literal) script string may access variables which have different
    // values in both instances and hence evaluated to different results.
    MyTypeObject *object1_2 = qobject_cast<MyTypeObject*>(component1.create());
    QVERIFY(object1_2 != 0);
    QVERIFY(object1->scriptProperty() != object1_2->scriptProperty());
}

// Check that default property assignments are correctly spliced into explicit
// property assignments
void tst_qqmllanguage::defaultPropertyListOrder()
{
    QQmlComponent component(&engine, testFileUrl("defaultPropertyListOrder.qml"));
    VERIFY_ERRORS(0);

    MyContainer *container = qobject_cast<MyContainer *>(component.create());
    QVERIFY(container  != 0);

    QCOMPARE(container->getChildren()->count(), 6);
    QCOMPARE(container->getChildren()->at(0)->property("index"), QVariant(0));
    QCOMPARE(container->getChildren()->at(1)->property("index"), QVariant(1));
    QCOMPARE(container->getChildren()->at(2)->property("index"), QVariant(2));
    QCOMPARE(container->getChildren()->at(3)->property("index"), QVariant(3));
    QCOMPARE(container->getChildren()->at(4)->property("index"), QVariant(4));
    QCOMPARE(container->getChildren()->at(5)->property("index"), QVariant(5));
}

void tst_qqmllanguage::declaredPropertyValues()
{
    QQmlComponent component(&engine, testFileUrl("declaredPropertyValues.qml"));
    VERIFY_ERRORS(0);
}

void tst_qqmllanguage::dontDoubleCallClassBegin()
{
    QQmlComponent component(&engine, testFileUrl("dontDoubleCallClassBegin.qml"));
    QObject *o = component.create();
    QVERIFY(o);

    MyParserStatus *o2 = qobject_cast<MyParserStatus *>(qvariant_cast<QObject *>(o->property("object")));
    QVERIFY(o2);
    QCOMPARE(o2->classBeginCount(), 1);
    QCOMPARE(o2->componentCompleteCount(), 1);

    delete o;
}

void tst_qqmllanguage::reservedWords_data()
{
    QTest::addColumn<QByteArray>("word");

    QTest::newRow("abstract") << QByteArray("abstract");
    QTest::newRow("as") << QByteArray("as");
    QTest::newRow("boolean") << QByteArray("boolean");
    QTest::newRow("break") << QByteArray("break");
    QTest::newRow("byte") << QByteArray("byte");
    QTest::newRow("case") << QByteArray("case");
    QTest::newRow("catch") << QByteArray("catch");
    QTest::newRow("char") << QByteArray("char");
    QTest::newRow("class") << QByteArray("class");
    QTest::newRow("continue") << QByteArray("continue");
    QTest::newRow("const") << QByteArray("const");
    QTest::newRow("debugger") << QByteArray("debugger");
    QTest::newRow("default") << QByteArray("default");
    QTest::newRow("delete") << QByteArray("delete");
    QTest::newRow("do") << QByteArray("do");
    QTest::newRow("double") << QByteArray("double");
    QTest::newRow("else") << QByteArray("else");
    QTest::newRow("enum") << QByteArray("enum");
    QTest::newRow("export") << QByteArray("export");
    QTest::newRow("extends") << QByteArray("extends");
    QTest::newRow("false") << QByteArray("false");
    QTest::newRow("final") << QByteArray("final");
    QTest::newRow("finally") << QByteArray("finally");
    QTest::newRow("float") << QByteArray("float");
    QTest::newRow("for") << QByteArray("for");
    QTest::newRow("function") << QByteArray("function");
    QTest::newRow("goto") << QByteArray("goto");
    QTest::newRow("if") << QByteArray("if");
    QTest::newRow("implements") << QByteArray("implements");
    QTest::newRow("import") << QByteArray("import");
    QTest::newRow("pragma") << QByteArray("pragma");
    QTest::newRow("in") << QByteArray("in");
    QTest::newRow("instanceof") << QByteArray("instanceof");
    QTest::newRow("int") << QByteArray("int");
    QTest::newRow("interface") << QByteArray("interface");
    QTest::newRow("long") << QByteArray("long");
    QTest::newRow("native") << QByteArray("native");
    QTest::newRow("new") << QByteArray("new");
    QTest::newRow("null") << QByteArray("null");
    QTest::newRow("package") << QByteArray("package");
    QTest::newRow("private") << QByteArray("private");
    QTest::newRow("protected") << QByteArray("protected");
    QTest::newRow("public") << QByteArray("public");
    QTest::newRow("return") << QByteArray("return");
    QTest::newRow("short") << QByteArray("short");
    QTest::newRow("static") << QByteArray("static");
    QTest::newRow("super") << QByteArray("super");
    QTest::newRow("switch") << QByteArray("switch");
    QTest::newRow("synchronized") << QByteArray("synchronized");
    QTest::newRow("this") << QByteArray("this");
    QTest::newRow("throw") << QByteArray("throw");
    QTest::newRow("throws") << QByteArray("throws");
    QTest::newRow("transient") << QByteArray("transient");
    QTest::newRow("true") << QByteArray("true");
    QTest::newRow("try") << QByteArray("try");
    QTest::newRow("typeof") << QByteArray("typeof");
    QTest::newRow("var") << QByteArray("var");
    QTest::newRow("void") << QByteArray("void");
    QTest::newRow("volatile") << QByteArray("volatile");
    QTest::newRow("while") << QByteArray("while");
    QTest::newRow("with") << QByteArray("with");
}

void tst_qqmllanguage::reservedWords()
{
    QFETCH(QByteArray, word);
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nQtObject { property string " + word + " }", QUrl());
    QCOMPARE(component.errorString(), QLatin1String(":2 Expected token `identifier'\n"));
}

// Check that first child of qml is of given type. Empty type insists on error.
void tst_qqmllanguage::testType(const QString& qml, const QString& type, const QString& expectederror, bool partialMatch)
{
    if (engine.importPathList() == defaultImportPathList)
        engine.addImportPath(testFile("lib"));

    QQmlComponent component(&engine);
    component.setData(qml.toUtf8(), testFileUrl("empty.qml")); // just a file for relative local imports

    QTRY_VERIFY(!component.isLoading());

    if (type.isEmpty()) {
        QVERIFY(component.isError());
        QString actualerror;
        foreach (const QQmlError e, component.errors()) {
            if (!actualerror.isEmpty())
                actualerror.append("; ");
            actualerror.append(e.description());
        }
        QCOMPARE(actualerror.left(partialMatch ? expectederror.length(): -1),expectederror);
    } else {
        VERIFY_ERRORS(0);
        QObject *object = component.create();
        QVERIFY(object != 0);
        QCOMPARE(QString(object->metaObject()->className()), type);
        delete object;
    }

    engine.setImportPathList(defaultImportPathList);
}

// QTBUG-17276
void tst_qqmllanguage::inlineAssignmentsOverrideBindings()
{
    QQmlComponent component(&engine, testFileUrl("inlineAssignmentsOverrideBindings.qml"));

    QObject *o = component.create();
    QVERIFY(o != 0);
    QCOMPARE(o->property("test").toInt(), 11);
    delete o;
}

// QTBUG-19354
void tst_qqmllanguage::nestedComponentRoots()
{
    QQmlComponent component(&engine, testFileUrl("nestedComponentRoots.qml"));
}

// Import tests (QT-558)
void tst_qqmllanguage::importsBuiltin_data()
{
    // QT-610

    QTest::addColumn<QString>("qml");
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("error");

    // import built-ins
    QTest::newRow("missing import")
        << "Test {}"
        << ""
        << "Test is not a type";
    QTest::newRow("not in version 0.0")
        << "import org.qtproject.Test 0.0\n"
           "Test {}"
        << ""
        << "Test is not a type";
    QTest::newRow("version not installed")
        << "import org.qtproject.Test 99.0\n"
           "Test {}"
        << ""
        << "module \"org.qtproject.Test\" version 99.0 is not installed";
    QTest::newRow("in version 0.0")
        << "import org.qtproject.Test 0.0\n"
           "TestTP {}"
        << "TestType"
        << "";
    QTest::newRow("qualified in version 0.0")
        << "import org.qtproject.Test 0.0 as T\n"
           "T.TestTP {}"
        << "TestType"
        << "";
    QTest::newRow("in version 1.0")
        << "import org.qtproject.Test 1.0\n"
           "Test {}"
        << "TestType"
        << "";
    QTest::newRow("qualified wrong")
        << "import org.qtproject.Test 1.0 as T\n" // QT-610
           "Test {}"
        << ""
        << "Test is not a type";
    QTest::newRow("qualified right")
        << "import org.qtproject.Test 1.0 as T\n"
           "T.Test {}"
        << "TestType"
        << "";
    QTest::newRow("qualified right but not in version 0.0")
        << "import org.qtproject.Test 0.0 as T\n"
           "T.Test {}"
        << ""
        << "T.Test is not a type";
    QTest::newRow("in version 1.1")
        << "import org.qtproject.Test 1.1\n"
           "Test {}"
        << "TestType"
        << "";
    QTest::newRow("in version 1.3")
        << "import org.qtproject.Test 1.3\n"
           "Test {}"
        << "TestType"
        << "";
    QTest::newRow("in version 1.5")
        << "import org.qtproject.Test 1.5\n"
           "Test {}"
        << "TestType"
        << "";
    QTest::newRow("changed in version 1.8")
        << "import org.qtproject.Test 1.8\n"
           "Test {}"
        << "TestType2"
        << "";
    QTest::newRow("in version 1.12")
        << "import org.qtproject.Test 1.12\n"
           "Test {}"
        << "TestType2"
        << "";
    QTest::newRow("old in version 1.9")
        << "import org.qtproject.Test 1.9\n"
           "OldTest {}"
        << "TestType"
        << "";
    QTest::newRow("old in version 1.11")
        << "import org.qtproject.Test 1.11\n"
           "OldTest {}"
        << "TestType"
        << "";
    QTest::newRow("multiversion 1")
        << "import org.qtproject.Test 1.11\n"
           "import org.qtproject.Test 1.12\n"
           "Test {}"
        << (!qmlCheckTypes()?"TestType2":"")
        << (!qmlCheckTypes()?"":"Test is ambiguous. Found in org/qtproject/Test/ in version 1.12 and 1.11");
    QTest::newRow("multiversion 2")
        << "import org.qtproject.Test 1.11\n"
           "import org.qtproject.Test 1.12\n"
           "OldTest {}"
        << (!qmlCheckTypes()?"TestType":"")
        << (!qmlCheckTypes()?"":"OldTest is ambiguous. Found in org/qtproject/Test/ in version 1.12 and 1.11");
    QTest::newRow("qualified multiversion 3")
        << "import org.qtproject.Test 1.0 as T0\n"
           "import org.qtproject.Test 1.8 as T8\n"
           "T0.Test {}"
        << "TestType"
        << "";
    QTest::newRow("qualified multiversion 4")
        << "import org.qtproject.Test 1.0 as T0\n"
           "import org.qtproject.Test 1.8 as T8\n"
           "T8.Test {}"
        << "TestType2"
        << "";
}

void tst_qqmllanguage::importsBuiltin()
{
    QFETCH(QString, qml);
    QFETCH(QString, type);
    QFETCH(QString, error);
    testType(qml,type,error);
}

void tst_qqmllanguage::importsLocal_data()
{
    QTest::addColumn<QString>("qml");
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("error");

    // import locals
    QTest::newRow("local import")
        << "import \"subdir\"\n" // QT-613
           "Test {}"
        << "QQuickRectangle"
        << "";
    QTest::newRow("local import second")
        << "import QtQuick 2.0\nimport \"subdir\"\n"
           "Test {}"
        << "QQuickRectangle"
        << "";
    QTest::newRow("local import subsubdir")
        << "import QtQuick 2.0\nimport \"subdir/subsubdir\"\n"
           "SubTest {}"
        << "QQuickRectangle"
        << "";
    QTest::newRow("local import QTBUG-7721 A")
        << "subdir.Test {}" // no longer allowed (QTBUG-7721)
        << ""
        << "subdir.Test - subdir is not a namespace";
    QTest::newRow("local import QTBUG-7721 B")
        << "import \"subdir\" as X\n"
           "X.subsubdir.SubTest {}" // no longer allowed (QTBUG-7721)
        << ""
        << "X.subsubdir.SubTest - nested namespaces not allowed";
    QTest::newRow("local import as")
        << "import \"subdir\" as T\n"
           "T.Test {}"
        << "QQuickRectangle"
        << "";
    QTest::newRow("wrong local import as")
        << "import \"subdir\" as T\n"
           "Test {}"
        << ""
        << "Test is not a type";
    QTest::newRow("library precedence over local import")
        << "import \"subdir\"\n"
           "import org.qtproject.Test 1.0\n"
           "Test {}"
        << (!qmlCheckTypes()?"TestType":"")
        << (!qmlCheckTypes()?"":"Test is ambiguous. Found in org/qtproject/Test/ and in subdir/");
    QTest::newRow("file URL survives percent-encoding")
        << "import \"" + QUrl::fromLocalFile(QDir::currentPath() + "/{subdir}").toString() + "\"\n"
           "Test {}"
        << "QQuickRectangle"
        << "";
}

void tst_qqmllanguage::importsLocal()
{
    QFETCH(QString, qml);
    QFETCH(QString, type);
    QFETCH(QString, error);
    testType(qml,type,error);
}

void tst_qqmllanguage::basicRemote_data()
{
    QTest::addColumn<QUrl>("url");
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("error");

    QString serverdir = "/qtest/qml/qqmllanguage/";

    QTest::newRow("no need for qmldir") << QUrl(serverdir+"Test.qml") << "" << "";
    QTest::newRow("absent qmldir") << QUrl(serverdir+"/noqmldir/Test.qml") << "" << "";
    QTest::newRow("need qmldir") << QUrl(serverdir+"TestNamed.qml") << "" << "";
}

void tst_qqmllanguage::basicRemote()
{
    QFETCH(QUrl, url);
    QFETCH(QString, type);
    QFETCH(QString, error);

    ThreadedTestHTTPServer server(dataDirectory());

    url = server.baseUrl().resolved(url);

    QQmlComponent component(&engine, url);

    QTRY_VERIFY(!component.isLoading());

    if (error.isEmpty()) {
        if (component.isError())
            qDebug() << component.errors();
        QVERIFY(!component.isError());
    } else {
        QVERIFY(component.isError());
    }
}

void tst_qqmllanguage::importsRemote_data()
{
    QTest::addColumn<QString>("qml");
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("error");

    QString serverdir = "{{ServerBaseUrl}}/qtest/qml/qqmllanguage";

    QTest::newRow("remote import") << "import \""+serverdir+"\"\nTest {}" << "QQuickRectangle"
        << "";
    QTest::newRow("remote import with subdir") << "import \""+serverdir+"\"\nTestSubDir {}" << "QQuickText"
        << "";
    QTest::newRow("remote import with local") << "import \""+serverdir+"\"\nTestLocal {}" << "QQuickImage"
        << "";
    QTest::newRow("remote import with qualifier") << "import \""+serverdir+"\" as NS\nNS.NamedLocal {}" << "QQuickImage"
        << "";
    QTest::newRow("wrong remote import with undeclared local") << "import \""+serverdir+"\"\nWrongTestLocal {}" << ""
        << "WrongTestLocal is not a type";
    QTest::newRow("wrong remote import of internal local") << "import \""+serverdir+"\"\nLocalInternal {}" << ""
        << "LocalInternal is not a type";
    QTest::newRow("wrong remote import of undeclared local") << "import \""+serverdir+"\"\nUndeclaredLocal {}" << ""
        << "UndeclaredLocal is not a type";
}

void tst_qqmllanguage::importsRemote()
{
    QFETCH(QString, qml);
    QFETCH(QString, type);
    QFETCH(QString, error);

    ThreadedTestHTTPServer server(dataDirectory());

    qml.replace(QStringLiteral("{{ServerBaseUrl}}"), server.baseUrl().toString());

    testType(qml,type,error);
}

void tst_qqmllanguage::importsInstalled_data()
{
    // QT-610

    QTest::addColumn<QString>("qml");
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("error");

    // import installed
    QTest::newRow("installed import 0")
        << "import org.qtproject.installedtest0 0.0\n"
           "InstalledTestTP {}"
        << "QQuickRectangle"
        << "";
    QTest::newRow("installed import 0 as TP")
        << "import org.qtproject.installedtest0 0.0 as TP\n"
           "TP.InstalledTestTP {}"
        << "QQuickRectangle"
        << "";
    QTest::newRow("installed import 1")
        << "import org.qtproject.installedtest 1.0\n"
           "InstalledTest {}"
        << "QQuickRectangle"
        << "";
    QTest::newRow("installed import 2")
        << "import org.qtproject.installedtest 1.3\n"
           "InstalledTest {}"
        << "QQuickRectangle"
        << "";
    QTest::newRow("installed import 3")
        << "import org.qtproject.installedtest 1.4\n"
           "InstalledTest {}"
        << "QQuickText"
        << "";
    QTest::newRow("installed import minor version not available") // QTBUG-11936
        << "import org.qtproject.installedtest 0.1\n"
           "InstalledTest {}"
        << ""
        << "module \"org.qtproject.installedtest\" version 0.1 is not installed";
    QTest::newRow("installed import minor version not available") // QTBUG-9627
        << "import org.qtproject.installedtest 1.10\n"
           "InstalledTest {}"
        << ""
        << "module \"org.qtproject.installedtest\" version 1.10 is not installed";
    QTest::newRow("installed import major version not available") // QTBUG-9627
        << "import org.qtproject.installedtest 9.0\n"
           "InstalledTest {}"
        << ""
        << "module \"org.qtproject.installedtest\" version 9.0 is not installed";
    QTest::newRow("installed import visibility") // QT-614
        << "import org.qtproject.installedtest 1.4\n"
           "PrivateType {}"
        << ""
        << "PrivateType is not a type";
    QTest::newRow("installed import version QML clash")
        << "import org.qtproject.installedtest1 1.0\n"
           "Test {}"
        << ""
        << "\"Test\" version 1.0 is defined more than once in module \"org.qtproject.installedtest1\"";
    QTest::newRow("installed import version JS clash")
        << "import org.qtproject.installedtest2 1.0\n"
           "Test {}"
        << ""
        << "\"Test\" version 1.0 is defined more than once in module \"org.qtproject.installedtest2\"";
}

void tst_qqmllanguage::importsInstalled()
{
    QFETCH(QString, qml);
    QFETCH(QString, type);
    QFETCH(QString, error);
    testType(qml,type,error);
}

void tst_qqmllanguage::importsInstalledRemote_data()
{
    // Repeat the tests for local installed data
    importsInstalled_data();
}

void tst_qqmllanguage::importsInstalledRemote()
{
    QFETCH(QString, qml);
    QFETCH(QString, type);
    QFETCH(QString, error);

    ThreadedTestHTTPServer server(dataDirectory());

    QString serverdir = server.urlString("/lib/");
    engine.setImportPathList(QStringList(defaultImportPathList) << serverdir);

    testType(qml,type,error);

    engine.setImportPathList(defaultImportPathList);
}

void tst_qqmllanguage::importsPath_data()
{
    QTest::addColumn<QStringList>("importPath");
    QTest::addColumn<QString>("qml");
    QTest::addColumn<QString>("value");

    QTest::newRow("local takes priority normal")
        << (QStringList() << testFile("lib") << "{{ServerBaseUrl}}/lib2/")
        << "import testModule 1.0\n"
           "Test {}"
        << "foo";

    QTest::newRow("local takes priority reversed")
        << (QStringList() << "{{ServerBaseUrl}}/lib/" << testFile("lib2"))
        << "import testModule 1.0\n"
           "Test {}"
        << "bar";

    QTest::newRow("earlier takes priority 1")
        << (QStringList() << "{{ServerBaseUrl}}/lib/" << "{{ServerBaseUrl}}/lib2/")
        << "import testModule 1.0\n"
           "Test {}"
        << "foo";

    QTest::newRow("earlier takes priority 2")
        << (QStringList() << "{{ServerBaseUrl}}/lib2/" << "{{ServerBaseUrl}}/lib/")
        << "import testModule 1.0\n"
           "Test {}"
        << "bar";

    QTest::newRow("major version takes priority over unversioned")
        << (QStringList() << "{{ServerBaseUrl}}/lib/" << "{{ServerBaseUrl}}/lib3/")
        << "import testModule 1.0\n"
           "Test {}"
        << "baz";

    QTest::newRow("major version takes priority over minor")
        << (QStringList() << "{{ServerBaseUrl}}/lib4/" << "{{ServerBaseUrl}}/lib3/")
        << "import testModule 1.0\n"
           "Test {}"
        << "baz";

    QTest::newRow("minor version takes priority over unversioned")
        << (QStringList() << "{{ServerBaseUrl}}/lib/" << "{{ServerBaseUrl}}/lib4/")
        << "import testModule 1.0\n"
           "Test {}"
        << "qux";
}

void tst_qqmllanguage::importsPath()
{
    QFETCH(QStringList, importPath);
    QFETCH(QString, qml);
    QFETCH(QString, value);

    ThreadedTestHTTPServer server(dataDirectory());

    for (int i = 0; i < importPath.count(); ++i)
        importPath[i].replace(QStringLiteral("{{ServerBaseUrl}}"), server.baseUrl().toString());

    engine.setImportPathList(QStringList(defaultImportPathList) << importPath);

    QQmlComponent component(&engine);
    component.setData(qml.toUtf8(), testFileUrl("empty.qml"));

    QTRY_VERIFY(component.isReady());
    VERIFY_ERRORS(0);

    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property("test").toString(), value);
    delete object;

    engine.setImportPathList(defaultImportPathList);
}

void tst_qqmllanguage::importsOrder_data()
{
    QTest::addColumn<QString>("qml");
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("error");
    QTest::addColumn<bool>("partialMatch");

    QTest::newRow("double import") <<
           "import org.qtproject.installedtest 1.4\n"
           "import org.qtproject.installedtest 1.4\n"
           "InstalledTest {}"
           << (!qmlCheckTypes()?"QQuickText":"")
           << (!qmlCheckTypes()?"":"InstalledTest is ambiguous. Found in lib/org/qtproject/installedtest/ in version 1.4 and 1.4")
           << false;
    QTest::newRow("installed import overrides 1") <<
           "import org.qtproject.installedtest 1.0\n"
           "import org.qtproject.installedtest 1.4\n"
           "InstalledTest {}"
           << (!qmlCheckTypes()?"QQuickText":"")
           << (!qmlCheckTypes()?"":"InstalledTest is ambiguous. Found in lib/org/qtproject/installedtest/ in version 1.4 and 1.0")
           << false;
    QTest::newRow("installed import overrides 2") <<
           "import org.qtproject.installedtest 1.4\n"
           "import org.qtproject.installedtest 1.0\n"
           "InstalledTest {}"
           << (!qmlCheckTypes()?"QQuickRectangle":"")
           << (!qmlCheckTypes()?"":"InstalledTest is ambiguous. Found in lib/org/qtproject/installedtest/ in version 1.0 and 1.4")
           << false;
    QTest::newRow("installed import re-overrides 1") <<
           "import org.qtproject.installedtest 1.4\n"
           "import org.qtproject.installedtest 1.0\n"
           "import org.qtproject.installedtest 1.4\n"
           "InstalledTest {}"
           << (!qmlCheckTypes()?"QQuickText":"")
           << (!qmlCheckTypes()?"":"InstalledTest is ambiguous. Found in lib/org/qtproject/installedtest/ in version 1.4 and 1.0")
           << false;
    QTest::newRow("installed import re-overrides 2") <<
           "import org.qtproject.installedtest 1.4\n"
           "import org.qtproject.installedtest 1.0\n"
           "import org.qtproject.installedtest 1.4\n"
           "import org.qtproject.installedtest 1.0\n"
           "InstalledTest {}"
           << (!qmlCheckTypes()?"QQuickRectangle":"")
           << (!qmlCheckTypes()?"":"InstalledTest is ambiguous. Found in lib/org/qtproject/installedtest/ in version 1.0 and 1.4")
           << false;
    QTest::newRow("installed import versus builtin 1") <<
           "import org.qtproject.installedtest 1.5\n"
           "import QtQuick 2.0\n"
           "Rectangle {}"
           << (!qmlCheckTypes()?"QQuickRectangle":"")
           << (!qmlCheckTypes()?"":"Rectangle is ambiguous. Found in file://")
           << true;
    QTest::newRow("installed import versus builtin 2") <<
           "import QtQuick 2.0\n"
           "import org.qtproject.installedtest 1.5\n"
           "Rectangle {}"
           << (!qmlCheckTypes()?"QQuickText":"")
           << (!qmlCheckTypes()?"":"Rectangle is ambiguous. Found in lib/org/qtproject/installedtest/ and in file://")
           << true;
    QTest::newRow("namespaces cannot be overridden by types 1") <<
           "import QtQuick 2.0 as Rectangle\n"
           "import org.qtproject.installedtest 1.5\n"
           "Rectangle {}"
           << ""
           << "Namespace Rectangle cannot be used as a type"
           << false;
    QTest::newRow("namespaces cannot be overridden by types 2") <<
           "import QtQuick 2.0 as Rectangle\n"
           "import org.qtproject.installedtest 1.5\n"
           "Rectangle.Image {}"
           << "QQuickImage"
           << ""
           << false;
    QTest::newRow("local last 1") <<
           "LocalLast {}"
           << "QQuickText"
           << ""
           << false;
    QTest::newRow("local last 2") <<
           "import org.qtproject.installedtest 1.0\n"
           "LocalLast {}"
           << (!qmlCheckTypes()?"QQuickRectangle":"")// i.e. from org.qtproject.installedtest, not data/LocalLast.qml
           << (!qmlCheckTypes()?"":"LocalLast is ambiguous. Found in lib/org/qtproject/installedtest/ and in ")
           << false;
    QTest::newRow("local last 3") << //Forces it to load the local qmldir to resolve types, but they shouldn't override anything
           "import org.qtproject.installedtest 1.0\n"
           "LocalLast {LocalLast2{}}"
           << (!qmlCheckTypes()?"QQuickRectangle":"")// i.e. from org.qtproject.installedtest, not data/LocalLast.qml
           << (!qmlCheckTypes()?"":"LocalLast is ambiguous. Found in lib/org/qtproject/installedtest/ and in ")
           << false;
}

void tst_qqmllanguage::importsOrder()
{
    QFETCH(QString, qml);
    QFETCH(QString, type);
    QFETCH(QString, error);
    QFETCH(bool, partialMatch);
    testType(qml,type,error,partialMatch);
}

void tst_qqmllanguage::importIncorrectCase()
{
    if (engine.importPathList() == defaultImportPathList)
        engine.addImportPath(testFile("lib"));

    // Load "importIncorrectCase.qml" using wrong case
    QQmlComponent component(&engine, testFileUrl("ImportIncorrectCase.qml"));

    QList<QQmlError> errors = component.errors();
    QCOMPARE(errors.count(), 1);

    const QString expectedError = isCaseSensitiveFileSystem(dataDirectory()) ?
        QStringLiteral("No such file or directory") :
        QStringLiteral("File name case mismatch");
    QCOMPARE(errors.at(0).description(), expectedError);

    engine.setImportPathList(defaultImportPathList);
}

void tst_qqmllanguage::importJs_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("errorFile");
    QTest::addColumn<bool>("performTest");

    QTest::newRow("defaultVersion")
        << "importJs.1.qml"
        << "importJs.1.errors.txt"
        << true;

    QTest::newRow("specifiedVersion")
        << "importJs.2.qml"
        << "importJs.2.errors.txt"
        << true;

    QTest::newRow("excludeExcessiveVersion")
        << "importJs.3.qml"
        << "importJs.3.errors.txt"
        << false;

    QTest::newRow("includeAppropriateVersion")
        << "importJs.4.qml"
        << "importJs.4.errors.txt"
        << true;

    QTest::newRow("noDefaultVersion")
        << "importJs.5.qml"
        << "importJs.5.errors.txt"
        << false;

    QTest::newRow("repeatImportFails")
        << "importJs.6.qml"
        << "importJs.6.errors.txt"
        << false;

    QTest::newRow("multipleVersionImportFails")
        << "importJs.7.qml"
        << "importJs.7.errors.txt"
        << false;

    QTest::newRow("namespacedImport")
        << "importJs.8.qml"
        << "importJs.8.errors.txt"
        << true;

    QTest::newRow("namespacedVersionedImport")
        << "importJs.9.qml"
        << "importJs.9.errors.txt"
        << true;

    QTest::newRow("namespacedRepeatImport")
        << "importJs.10.qml"
        << "importJs.10.errors.txt"
        << true;

    QTest::newRow("emptyScript")
        << "importJs.11.qml"
        << "importJs.11.errors.txt"
        << true;
}

void tst_qqmllanguage::importJs()
{
    QFETCH(QString, file);
    QFETCH(QString, errorFile);
    QFETCH(bool, performTest);

    engine.setImportPathList(QStringList(defaultImportPathList) << testFile("lib"));

    QQmlComponent component(&engine, testFileUrl(file));

    {
        DETERMINE_ERRORS(errorFile,expected,actual);
        QCOMPARE(expected.size(), actual.size());
        for (int i = 0; i < expected.size(); ++i)
        {
            const int compareLen = qMin(expected.at(i).length(), actual.at(i).length());
            QCOMPARE(expected.at(i).left(compareLen), actual.at(i).left(compareLen));
        }
    }

    if (performTest) {
        QObject *object = component.create();
        QVERIFY(object != 0);
        QCOMPARE(object->property("test").toBool(),true);
        delete object;
    }

    engine.setImportPathList(defaultImportPathList);
}

void tst_qqmllanguage::qmlAttachedPropertiesObjectMethod()
{
    QObject object;

    QCOMPARE(qmlAttachedPropertiesObject<MyQmlObject>(&object, false), (QObject *)0);
    QCOMPARE(qmlAttachedPropertiesObject<MyQmlObject>(&object, true), (QObject *)0);

    {
        QQmlComponent component(&engine, testFileUrl("qmlAttachedPropertiesObjectMethod.1.qml"));
        VERIFY_ERRORS(0);
        QObject *object = component.create();
        QVERIFY(object != 0);

        QCOMPARE(qmlAttachedPropertiesObject<MyQmlObject>(object, false), (QObject *)0);
        QVERIFY(qmlAttachedPropertiesObject<MyQmlObject>(object, true) != 0);
    }

    {
        QQmlComponent component(&engine, testFileUrl("qmlAttachedPropertiesObjectMethod.2.qml"));
        VERIFY_ERRORS(0);
        QObject *object = component.create();
        QVERIFY(object != 0);

        QVERIFY(qmlAttachedPropertiesObject<MyQmlObject>(object, false) != 0);
        QVERIFY(qmlAttachedPropertiesObject<MyQmlObject>(object, true) != 0);
    }
}

void tst_qqmllanguage::crash1()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nComponent {}", QUrl());
}

void tst_qqmllanguage::crash2()
{
    QQmlComponent component(&engine, testFileUrl("crash2.qml"));
}

// QTBUG-8676
void tst_qqmllanguage::customOnProperty()
{
    QQmlComponent component(&engine, testFileUrl("customOnProperty.qml"));

    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("on").toInt(), 10);

    delete object;
}

// QTBUG-12601
void tst_qqmllanguage::variantNotify()
{
    QQmlComponent component(&engine, testFileUrl("variantNotify.qml"));

    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("notifyCount").toInt(), 1);

    delete object;
}

void tst_qqmllanguage::revisions()
{
    {
        QQmlComponent component(&engine, testFileUrl("revisions11.qml"));

        VERIFY_ERRORS(0);
        MyRevisionedClass *object = qobject_cast<MyRevisionedClass*>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->prop2(), 10.0);

        delete object;
    }
    {
        QQmlEngine myEngine;
        QQmlComponent component(&myEngine, testFileUrl("revisionssub11.qml"));

        VERIFY_ERRORS(0);
        MyRevisionedSubclass *object = qobject_cast<MyRevisionedSubclass*>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->prop1(), 10.0);
        QCOMPARE(object->prop2(), 10.0);
        QCOMPARE(object->prop3(), 10.0);
        QCOMPARE(object->prop4(), 10.0);

        delete object;
    }
    {
        QQmlComponent component(&engine, testFileUrl("versionedbase.qml"));
        VERIFY_ERRORS(0);
        MySubclass *object = qobject_cast<MySubclass*>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->prop1(), 10.0);
        QCOMPARE(object->prop2(), 10.0);

        delete object;
    }
}

void tst_qqmllanguage::revisionOverloads()
{
    {
    QQmlComponent component(&engine, testFileUrl("allowedRevisionOverloads.qml"));
    VERIFY_ERRORS(0);
    }
    {
    QQmlComponent component(&engine, testFileUrl("disallowedRevisionOverloads.qml"));
    QEXPECT_FAIL("", "QTBUG-13849", Abort);
    QVERIFY(0);
    VERIFY_ERRORS("disallowedRevisionOverloads.errors.txt");
    }
}

void tst_qqmllanguage::subclassedUncreateableRevision_data()
{
    QTest::addColumn<QString>("version");
    QTest::addColumn<QString>("prop");
    QTest::addColumn<bool>("shouldWork");

    QTest::newRow("prop1 exists in 1.0") << "1.0" << "prop1" << true;
    QTest::newRow("prop2 does not exist in 1.0") << "1.0" << "prop2" << false;
    QTest::newRow("prop3 does not exist in 1.0") << "1.0" << "prop3" << false;

    QTest::newRow("prop1 exists in 1.1") << "1.1" << "prop1" << true;
    QTest::newRow("prop2 works because it's re-declared in Derived") << "1.1" << "prop2" << true;
    QTest::newRow("prop3 only works if the Base REVISION 1 is picked up") << "1.1" << "prop3" << true;

}

void tst_qqmllanguage::subclassedUncreateableRevision()
{
    QFETCH(QString, version);
    QFETCH(QString, prop);
    QFETCH(bool, shouldWork);

    {
        QQmlEngine engine;
        QString qml = QString("import QtQuick 2.0\nimport Test %1\nMyUncreateableBaseClass {}").arg(version);
        QQmlComponent c(&engine);
        QTest::ignoreMessage(QtWarningMsg, "QQmlComponent: Component is not ready");
        c.setData(qml.toUtf8(), QUrl::fromLocalFile(QDir::currentPath()));
        QObject *obj = c.create();
        QCOMPARE(obj, static_cast<QObject*>(0));
        QCOMPARE(c.errors().count(), 1);
        QCOMPARE(c.errors().first().description(), QString("Cannot create MyUncreateableBaseClass"));
    }

    QQmlEngine engine;
    QString qml = QString("import QtQuick 2.0\nimport Test %1\nMyCreateableDerivedClass {\n%3: true\n}").arg(version).arg(prop);
    QQmlComponent c(&engine);
    if (!shouldWork)
        QTest::ignoreMessage(QtWarningMsg, "QQmlComponent: Component is not ready");
    c.setData(qml.toUtf8(), QUrl::fromLocalFile(QDir::currentPath()));
    QObject *obj = c.create();
    if (!shouldWork) {
        QCOMPARE(obj, static_cast<QObject*>(0));
        return;
    }

    QVERIFY(obj);
    MyUncreateableBaseClass *base = qobject_cast<MyUncreateableBaseClass*>(obj);
    QVERIFY(base);
    QCOMPARE(base->property(prop.toLatin1()).toBool(), true);
    delete obj;
}

void tst_qqmllanguage::uncreatableTypesAsProperties()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("uncreatableTypeAsProperty.qml"));
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());
}

void tst_qqmllanguage::initTestCase()
{
    QQmlDataTest::initTestCase();
    QVERIFY2(QDir::setCurrent(dataDirectory()), qPrintable("Could not chdir to " + dataDirectory()));

    defaultImportPathList = engine.importPathList();

    QQmlMetaType::registerCustomStringConverter(qMetaTypeId<MyCustomVariantType>(), myCustomVariantTypeConverter);

    registerTypes();
    // Registered here because it uses testFileUrl
    qmlRegisterType(testFileUrl("CompositeType.qml"), "Test", 1, 0, "RegisteredCompositeType");
    qmlRegisterType(testFileUrl("CompositeType.DoesNotExist.qml"), "Test", 1, 0, "RegisteredCompositeType2");
    qmlRegisterType(testFileUrl("invalidRoot.1.qml"), "Test", 1, 0, "RegisteredCompositeType3");
    qmlRegisterType(testFileUrl("CompositeTypeWithEnum.qml"), "Test", 1, 0, "RegisteredCompositeTypeWithEnum");
    qmlRegisterType(testFileUrl("CompositeTypeWithAttachedProperty.qml"), "Test", 1, 0, "RegisteredCompositeTypeWithAttachedProperty");

    // Registering the TestType class in other modules should have no adverse effects
    qmlRegisterType<TestType>("org.qtproject.TestPre", 1, 0, "Test");

    qmlRegisterType<TestType>("org.qtproject.Test", 0, 0, "TestTP");
    qmlRegisterType<TestType>("org.qtproject.Test", 1, 0, "Test");
    qmlRegisterType<TestType>("org.qtproject.Test", 1, 5, "Test");
    qmlRegisterType<TestType2>("org.qtproject.Test", 1, 8, "Test");
    qmlRegisterType<TestType>("org.qtproject.Test", 1, 9, "OldTest");
    qmlRegisterType<TestType2>("org.qtproject.Test", 1, 12, "Test");

    // Registering the TestType class in other modules should have no adverse effects
    qmlRegisterType<TestType>("org.qtproject.TestPost", 1, 0, "Test");

    // Create locale-specific file
    // For POSIX, this will just be data/I18nType.qml, since POSIX is 7-bit
    // For iso8859-1 locale, this will just be data/I18nType?????.qml where ????? is 5 8-bit characters
    // For utf-8 locale, this will be data/I18nType??????????.qml where ?????????? is 5 8-bit characters, UTF-8 encoded
    QFile in(testFileUrl(QLatin1String("I18nType30.qml")).toLocalFile());
    QVERIFY2(in.open(QIODevice::ReadOnly), qPrintable(QString::fromLatin1("Cannot open '%1': %2").arg(in.fileName(), in.errorString())));
    QFile out(testFileUrl(QString::fromUtf8("I18nType\303\201\303\242\303\243\303\244\303\245.qml")).toLocalFile());
    QVERIFY2(out.open(QIODevice::WriteOnly), qPrintable(QString::fromLatin1("Cannot open '%1': %2").arg(out.fileName(), out.errorString())));
    out.write(in.readAll());

    // Register a Composite Singleton.
    qmlRegisterSingletonType(testFileUrl("singleton/RegisteredCompositeSingletonType.qml"), "org.qtproject.Test", 1, 0, "RegisteredSingleton");
}

void tst_qqmllanguage::aliasPropertyChangeSignals()
{
    {
        QQmlComponent component(&engine, testFileUrl("aliasPropertyChangeSignals.qml"));

        VERIFY_ERRORS(0);
        QObject *o = component.create();
        QVERIFY(o != 0);

        QCOMPARE(o->property("test").toBool(), true);

        delete o;
    }

    // QTCREATORBUG-2769
    {
        QQmlComponent component(&engine, testFileUrl("aliasPropertyChangeSignals.2.qml"));

        VERIFY_ERRORS(0);
        QObject *o = component.create();
        QVERIFY(o != 0);

        QCOMPARE(o->property("test").toBool(), true);

        delete o;
    }
}

// Tests property initializers
void tst_qqmllanguage::propertyInit()
{
    {
        QQmlComponent component(&engine, testFileUrl("propertyInit.1.qml"));

        VERIFY_ERRORS(0);
        QObject *o = component.create();
        QVERIFY(o != 0);

        QCOMPARE(o->property("test").toInt(), 1);

        delete o;
    }

    {
        QQmlComponent component(&engine, testFileUrl("propertyInit.2.qml"));

        VERIFY_ERRORS(0);
        QObject *o = component.create();
        QVERIFY(o != 0);

        QCOMPARE(o->property("test").toInt(), 123);

        delete o;
    }
}

// Test that registration order doesn't break type availability
// QTBUG-16878
void tst_qqmllanguage::registrationOrder()
{
    QQmlComponent component(&engine, testFileUrl("registrationOrder.qml"));

    QObject *o = component.create();
    QVERIFY(o != 0);
    QCOMPARE(o->metaObject(), &MyVersion2Class::staticMetaObject);
    delete o;
}

void tst_qqmllanguage::readonly()
{
    QQmlComponent component(&engine, testFileUrl("readonly.qml"));

    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test1").toInt(), 10);
    QCOMPARE(o->property("test2").toInt(), 18);
    QCOMPARE(o->property("test3").toInt(), 13);

    o->setProperty("testData", 13);

    QCOMPARE(o->property("test1").toInt(), 10);
    QCOMPARE(o->property("test2").toInt(), 22);
    QCOMPARE(o->property("test3").toInt(), 13);

    o->setProperty("testData2", 2);

    QCOMPARE(o->property("test1").toInt(), 10);
    QCOMPARE(o->property("test2").toInt(), 22);
    QCOMPARE(o->property("test3").toInt(), 2);

    o->setProperty("test1", 11);
    o->setProperty("test2", 11);
    o->setProperty("test3", 11);

    QCOMPARE(o->property("test1").toInt(), 10);
    QCOMPARE(o->property("test2").toInt(), 22);
    QCOMPARE(o->property("test3").toInt(), 2);

    delete o;
}

void tst_qqmllanguage::readonlyObjectProperties()
{
    QQmlComponent component(&engine, testFileUrl("readonlyObjectProperty.qml"));

    QScopedPointer<QObject> o(component.create());
    QVERIFY(!o.isNull());

    QQmlProperty prop(o.data(), QStringLiteral("subObject"), &engine);
    QVERIFY(!prop.isWritable());
    QVERIFY(!prop.write(QVariant::fromValue(o.data())));

    QObject *subObject = qvariant_cast<QObject*>(prop.read());
    QVERIFY(subObject);
    QCOMPARE(subObject->property("readWrite").toInt(), int(42));
    subObject->setProperty("readWrite", QVariant::fromValue(int(100)));
    QCOMPARE(subObject->property("readWrite").toInt(), int(100));
}

void tst_qqmllanguage::receivers()
{
    QQmlComponent component(&engine, testFileUrl("receivers.qml"));

    MyReceiversTestObject *o = qobject_cast<MyReceiversTestObject*>(component.create());
    QVERIFY(o != 0);
    QCOMPARE(o->mySignalCount(), 1);
    QCOMPARE(o->propChangedCount(), 2);
    QCOMPARE(o->myUnconnectedSignalCount(), 0);

    QVERIFY(o->isSignalConnected(QMetaMethod::fromSignal(&MyReceiversTestObject::mySignal)));
    QVERIFY(o->isSignalConnected(QMetaMethod::fromSignal(&MyReceiversTestObject::propChanged)));
    QVERIFY(!o->isSignalConnected(QMetaMethod::fromSignal(&MyReceiversTestObject::myUnconnectedSignal)));

    delete o;
}

void tst_qqmllanguage::registeredCompositeType()
{
    QQmlComponent component(&engine, testFileUrl("registeredCompositeType.qml"));

    VERIFY_ERRORS(0);
    QObject *o = component.create();
    QVERIFY(o != 0);

    delete o;
}

// QTBUG-43582
void tst_qqmllanguage::registeredCompositeTypeWithEnum()
{
    QQmlComponent component(&engine, testFileUrl("registeredCompositeTypeWithEnum.qml"));

    VERIFY_ERRORS(0);
    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("enumValue0").toInt(), static_cast<int>(MyCompositeBaseType::EnumValue0));
    QCOMPARE(o->property("enumValue42").toInt(), static_cast<int>(MyCompositeBaseType::EnumValue42));

    delete o;
}

// QTBUG-43581
void tst_qqmllanguage::registeredCompositeTypeWithAttachedProperty()
{
    QQmlComponent component(&engine, testFileUrl("registeredCompositeTypeWithAttachedProperty.qml"));

    VERIFY_ERRORS(0);
    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("attachedProperty").toString(), QStringLiteral("test"));

    delete o;
}

// QTBUG-18268
void tst_qqmllanguage::remoteLoadCrash()
{
    ThreadedTestHTTPServer server(dataDirectory());

    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; Text {}", server.url("/remoteLoadCrash.qml"));
    while (component.isLoading())
        QCoreApplication::processEvents( QEventLoop::ExcludeUserInputEvents | QEventLoop::WaitForMoreEvents, 50);

    QObject *o = component.create();
    delete o;
}

void tst_qqmllanguage::signalWithDefaultArg()
{
    QQmlComponent component(&engine, testFileUrl("signalWithDefaultArg.qml"));

    MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
    QVERIFY(object != 0);

    QCOMPARE(object->property("signalCount").toInt(), 0);
    QCOMPARE(object->property("signalArg").toInt(), 0);

    emit object->signalWithDefaultArg();
    QCOMPARE(object-> property("signalCount").toInt(), 1);
    QCOMPARE(object->property("signalArg").toInt(), 5);

    emit object->signalWithDefaultArg(15);
    QCOMPARE(object->property("signalCount").toInt(), 2);
    QCOMPARE(object->property("signalArg").toInt(), 15);


    QMetaObject::invokeMethod(object, "emitNoArgSignal");
    QCOMPARE(object->property("signalCount").toInt(), 3);
    QCOMPARE(object->property("signalArg").toInt(), 5);

    QMetaObject::invokeMethod(object, "emitArgSignal");
    QCOMPARE(object->property("signalCount").toInt(), 4);
    QCOMPARE(object->property("signalArg").toInt(), 22);

    delete object;
}

void tst_qqmllanguage::signalParameterTypes()
{
    // bound signal handlers
    {
    QQmlComponent component(&engine, testFileUrl("signalParameterTypes.1.qml"));
    QObject *obj = component.create();
    QVERIFY(obj != 0);
    QVERIFY(obj->property("success").toBool());
    delete obj;
    }

    // dynamic signal connections
    {
    QQmlComponent component(&engine, testFileUrl("signalParameterTypes.2.qml"));
    QObject *obj = component.create();
    QVERIFY(obj != 0);
    QVERIFY(obj->property("success").toBool());
    delete obj;
    }
}

// QTBUG-20639
void tst_qqmllanguage::globalEnums()
{
    qRegisterMetaType<MyEnum1Class::EnumA>();
    qRegisterMetaType<MyEnum2Class::EnumB>();
    qRegisterMetaType<Qt::TextFormat>();

    QQmlComponent component(&engine, testFileUrl("globalEnums.qml"));

    QObject *o = component.create();
    QVERIFY(o != 0);

    MyEnum1Class *enum1Class = o->findChild<MyEnum1Class *>(QString::fromLatin1("enum1Class"));
    QVERIFY(enum1Class != 0);
    QVERIFY(enum1Class->getValue() == -1);

    MyEnumDerivedClass *enum2Class = o->findChild<MyEnumDerivedClass *>(QString::fromLatin1("enumDerivedClass"));
    QVERIFY(enum2Class != 0);
    QVERIFY(enum2Class->getValueA() == -1);
    QVERIFY(enum2Class->getValueB() == -1);
    QVERIFY(enum2Class->getValueC() == 0);
    QVERIFY(enum2Class->getValueD() == 0);
    QVERIFY(enum2Class->getValueE() == -1);
    QVERIFY(enum2Class->getValueE2() == -1);

    QVERIFY(enum2Class->property("aValue") == 0);
    QVERIFY(enum2Class->property("bValue") == 0);
    QVERIFY(enum2Class->property("cValue") == 0);
    QVERIFY(enum2Class->property("dValue") == 0);
    QVERIFY(enum2Class->property("eValue") == 0);
    QVERIFY(enum2Class->property("e2Value") == 0);

    QSignalSpy signalA(enum2Class, SIGNAL(valueAChanged(MyEnum1Class::EnumA)));
    QSignalSpy signalB(enum2Class, SIGNAL(valueBChanged(MyEnum2Class::EnumB)));

    QMetaObject::invokeMethod(o, "setEnumValues");

    QVERIFY(enum1Class->getValue() == MyEnum1Class::A_13);
    QVERIFY(enum2Class->getValueA() == MyEnum1Class::A_11);
    QVERIFY(enum2Class->getValueB() == MyEnum2Class::B_37);
    QVERIFY(enum2Class->getValueC() == Qt::RichText);
    QVERIFY(enum2Class->getValueD() == Qt::ElideMiddle);
    QVERIFY(enum2Class->getValueE() == MyEnum2Class::E_14);
    QVERIFY(enum2Class->getValueE2() == MyEnum2Class::E_76);

    QVERIFY(signalA.count() == 1);
    QVERIFY(signalB.count() == 1);

    QVERIFY(enum2Class->property("aValue") == MyEnum1Class::A_11);
    QVERIFY(enum2Class->property("bValue") == 37);
    QVERIFY(enum2Class->property("cValue") == 1);
    QVERIFY(enum2Class->property("dValue") == 2);
    QVERIFY(enum2Class->property("eValue") == 14);
    QVERIFY(enum2Class->property("e2Value") == 76);

    delete o;
}

void tst_qqmllanguage::lowercaseEnumRuntime_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("errorMessage");

    QTest::newRow("enum from normal type") << "lowercaseEnumRuntime.1.qml" << ":8: TypeError: Cannot access enum value 'lowercaseEnumVal' of 'MyTypeObject', enum values need to start with an uppercase letter.";
    QTest::newRow("enum from singleton type") << "lowercaseEnumRuntime.2.qml" << ":8: TypeError: Cannot access enum value 'lowercaseEnumVal' of 'MyTypeObjectSingleton', enum values need to start with an uppercase letter.";
}

void tst_qqmllanguage::lowercaseEnumRuntime()
{
    QFETCH(QString, file);
    QFETCH(QString, errorMessage);

    QQmlComponent component(&engine, testFileUrl(file));
    VERIFY_ERRORS(0);
    QString warning = component.url().toString() + errorMessage;
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning));
    delete component.create();
}

void tst_qqmllanguage::lowercaseEnumCompileTime_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("errorFile");

    QTest::newRow("assignment to int property") << "lowercaseEnumCompileTime.1.qml" << "lowercaseEnumCompileTime.1.errors.txt";
    QTest::newRow("assignment to enum property") << "lowercaseEnumCompileTime.2.qml" << "lowercaseEnumCompileTime.2.errors.txt";
}

void tst_qqmllanguage::lowercaseEnumCompileTime()
{
    QFETCH(QString, file);
    QFETCH(QString, errorFile);

    QQmlComponent component(&engine, testFileUrl(file));
    VERIFY_ERRORS(qPrintable(errorFile));
}

void tst_qqmllanguage::literals_data()
{
    QTest::addColumn<QString>("property");
    QTest::addColumn<QVariant>("value");

    QTest::newRow("hex") << "n1" << QVariant(0xfe32);
    // Octal integer literals are deprecated
//    QTest::newRow("octal") << "n2" << QVariant(015);
    QTest::newRow("fp1") << "n3" << QVariant(-4.2E11);
    QTest::newRow("fp2") << "n4" << QVariant(.1e9);
    QTest::newRow("fp3") << "n5" << QVariant(3e-12);
    QTest::newRow("fp4") << "n6" << QVariant(3e+12);
    QTest::newRow("fp5") << "n7" << QVariant(0.1e9);
    QTest::newRow("large-int1") << "n8" << QVariant((double) 1152921504606846976);
    QTest::newRow("large-int2") << "n9" << QVariant(100000000000000000000.);

    QTest::newRow("special1") << "c1" << QVariant(QString("\b"));
    QTest::newRow("special2") << "c2" << QVariant(QString("\f"));
    QTest::newRow("special3") << "c3" << QVariant(QString("\n"));
    QTest::newRow("special4") << "c4" << QVariant(QString("\r"));
    QTest::newRow("special5") << "c5" << QVariant(QString("\t"));
    QTest::newRow("special6") << "c6" << QVariant(QString("\v"));
    QTest::newRow("special7") << "c7" << QVariant(QString("\'"));
    QTest::newRow("special8") << "c8" << QVariant(QString("\""));
    QTest::newRow("special9") << "c9" << QVariant(QString("\\"));
    // We don't handle octal escape sequences
    QTest::newRow("special10") << "c10" << QVariant(QString(1, QChar(0xa9)));
    QTest::newRow("special11") << "c11" << QVariant(QString(1, QChar(0x00A9)));
}

void tst_qqmllanguage::literals()
{
    QFETCH(QString, property);
    QFETCH(QVariant, value);

    QQmlComponent component(&engine, testFile("literals.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property(property.toLatin1()), value);
    delete object;
}

void tst_qqmllanguage::objectDeletionNotify_data()
{
    QTest::addColumn<QString>("file");

    QTest::newRow("property QtObject") << "objectDeletionNotify.1.qml";
    QTest::newRow("property variant") << "objectDeletionNotify.2.qml";
    QTest::newRow("property var") << "objectDeletionNotify.3.qml";
    QTest::newRow("property var guard removed") << "objectDeletionNotify.4.qml";
}

void tst_qqmllanguage::objectDeletionNotify()
{
    QFETCH(QString, file);

    QQmlComponent component(&engine, testFile(file));

    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property("success").toBool(), true);

    QMetaObject::invokeMethod(object, "destroyObject");

    // Process the deletion event
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    QCOMPARE(object->property("success").toBool(), true);

    delete object;
}

void tst_qqmllanguage::scopedProperties()
{
    QQmlComponent component(&engine, testFile("scopedProperties.qml"));

    QScopedPointer<QObject> o(component.create());
    QVERIFY(o != 0);
    QVERIFY(o->property("success").toBool());
}

void tst_qqmllanguage::deepProperty()
{
    QQmlComponent component(&engine, testFile("deepProperty.qml"));
    QScopedPointer<QObject> o(component.create());
    QVERIFY(o != 0);
    QFont font = qvariant_cast<QFont>(qvariant_cast<QObject*>(o->property("someObject"))->property("font"));
    QCOMPARE(font.family(), QStringLiteral("test"));
}

// Tests that the implicit import has lowest precedence, in the case where
// there are conflicting types and types only found in the local import.
// Tests that just check one (or the root) type are in ::importsOrder
void tst_qqmllanguage::implicitImportsLast()
{
    if (qmlCheckTypes())
        QSKIP("This test is about maintaining the same choice when type is ambiguous.");

    if (engine.importPathList() == defaultImportPathList)
        engine.addImportPath(testFile("lib"));

    QQmlComponent component(&engine, testFile("localOrderTest.qml"));
    VERIFY_ERRORS(0);
    QObject *object = qobject_cast<QObject *>(component.create());
    QVERIFY(object != 0);
    QVERIFY(QString(object->metaObject()->className()).startsWith(QLatin1String("QQuickMouseArea")));
    QObject* object2 = object->property("item").value<QObject*>();
    QVERIFY(object2 != 0);
    QCOMPARE(QString(object2->metaObject()->className()), QLatin1String("QQuickRectangle"));

    engine.setImportPathList(defaultImportPathList);
}

void tst_qqmllanguage::getSingletonInstance(QQmlEngine& engine, const char* fileName, const char* propertyName, QObject** result /* out */)
{
    QVERIFY(fileName != 0);
    QVERIFY(propertyName != 0);

    if (!fileName || !propertyName)
        return;

    QQmlComponent component(&engine, testFile(fileName));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);

    getSingletonInstance(object, propertyName, result);
}

void tst_qqmllanguage::getSingletonInstance(QObject* o, const char* propertyName, QObject** result /* out */)
{
    QVERIFY(o != 0);
    QVERIFY(propertyName != 0);

    if (!o || !propertyName)
        return;

    QVariant variant = o->property(propertyName);
    QVERIFY(variant.userType() == qMetaTypeId<QObject *>());

    QObject *singleton = NULL;
    if (variant.canConvert<QObject*>())
        singleton = variant.value<QObject*>();

    QVERIFY(singleton != 0);
    *result = singleton;
}

void verifyCompositeSingletonPropertyValues(QObject* o, const char* n1, int v1, const char* n2, int v2)
{
    QCOMPARE(o->property(n1).userType(), (int)QMetaType::Int);
    QCOMPARE(o->property(n1), QVariant(v1));

    QCOMPARE(o->property(n2).userType(), (int)QVariant::String);
    QString numStr;
    QCOMPARE(o->property(n2), QVariant(QString(QLatin1String("Test value: ")).append(numStr.setNum(v2))));
}

// Reads values from a composite singleton type
void tst_qqmllanguage::compositeSingletonProperties()
{
    QQmlComponent component(&engine, testFile("singletonTest1.qml"));
    VERIFY_ERRORS(0);
    QObject *o = component.create();
    QVERIFY(o != 0);

    verifyCompositeSingletonPropertyValues(o, "value1", 125, "value2", -55);
}

// Checks that the addresses of the composite singletons used in the same
// engine are the same.
void tst_qqmllanguage::compositeSingletonSameEngine()
{
    QObject* s1 = NULL;
    getSingletonInstance(engine, "singletonTest2.qml", "singleton1", &s1);
    QVERIFY(s1 != 0);
    s1->setProperty("testProp2", QVariant(13));

    QObject* s2 = NULL;
    getSingletonInstance(engine, "singletonTest3.qml", "singleton2", &s2);
    QVERIFY(s2 != 0);
    QCOMPARE(s2->property("testProp2"), QVariant(13));

    QCOMPARE(s1, s2);
}

// Checks that the addresses of the composite singletons used in different
// engines are different.
void tst_qqmllanguage::compositeSingletonDifferentEngine()
{
    QQmlEngine e2;

    QObject* s1 = NULL;
    getSingletonInstance(engine, "singletonTest2.qml", "singleton1", &s1);
    QVERIFY(s1 != 0);
    s1->setProperty("testProp2", QVariant(13));

    QObject* s2 = NULL;
    getSingletonInstance(e2, "singletonTest3.qml", "singleton2", &s2);
    QVERIFY(s2 != 0);
    QCOMPARE(s2->property("testProp2"), QVariant(25));

    QVERIFY(s1 != s2);
}

// pragma Singleton in a non-type qml file fails
void tst_qqmllanguage::compositeSingletonNonTypeError()
{
    QQmlComponent component(&engine, testFile("singletonTest4.qml"));
    VERIFY_ERRORS("singletonTest4.error.txt");
}

// Loads the singleton using a namespace qualifier
void tst_qqmllanguage::compositeSingletonQualifiedNamespace()
{
    QQmlComponent component(&engine, testFile("singletonTest5.qml"));
    VERIFY_ERRORS(0);
    QObject *o = component.create();
    QVERIFY(o != 0);

    verifyCompositeSingletonPropertyValues(o, "value1", 125, "value2", -55);

    // lets verify that the singleton instance we are using is the same
    // when loaded through another file (without namespace!)
    QObject *s1 = NULL;
    getSingletonInstance(o, "singletonInstance", &s1);
    QVERIFY(s1 != 0);

    QObject* s2 = NULL;
    getSingletonInstance(engine, "singletonTest5a.qml", "singletonInstance", &s2);
    QVERIFY(s2 != 0);

    QCOMPARE(s1, s2);
}

// Loads a singleton from a module
void tst_qqmllanguage::compositeSingletonModule()
{
    engine.addImportPath(testFile("singleton/module"));

    QQmlComponent component(&engine, testFile("singletonTest6.qml"));
    VERIFY_ERRORS(0);
    QObject *o = component.create();
    QVERIFY(o != 0);

    verifyCompositeSingletonPropertyValues(o, "value1", 125, "value2", -55);
    verifyCompositeSingletonPropertyValues(o, "value3", 125, "value4", -55);

    // lets verify that the singleton instance we are using is the same
    // when loaded through another file
    QObject *s1 = NULL;
    getSingletonInstance(o, "singletonInstance", &s1);
    QVERIFY(s1 != 0);

    QObject* s2 = NULL;
    getSingletonInstance(engine, "singletonTest6a.qml", "singletonInstance", &s2);
    QVERIFY(s2 != 0);

    QCOMPARE(s1, s2);
}

// Loads a singleton from a module with a higher version
void tst_qqmllanguage::compositeSingletonModuleVersioned()
{
    engine.addImportPath(testFile("singleton/module"));

    QQmlComponent component(&engine, testFile("singletonTest7.qml"));
    VERIFY_ERRORS(0);
    QObject *o = component.create();
    QVERIFY(o != 0);

    verifyCompositeSingletonPropertyValues(o, "value1", 225, "value2", 55);
    verifyCompositeSingletonPropertyValues(o, "value3", 225, "value4", 55);

    // lets verify that the singleton instance we are using is the same
    // when loaded through another file
    QObject *s1 = NULL;
    getSingletonInstance(o, "singletonInstance", &s1);
    QVERIFY(s1 != 0);

    QObject* s2 = NULL;
    getSingletonInstance(engine, "singletonTest7a.qml", "singletonInstance", &s2);
    QVERIFY(s2 != 0);

    QCOMPARE(s1, s2);
}

// Loads a singleton from a module with a qualified namespace
void tst_qqmllanguage::compositeSingletonModuleQualified()
{
    engine.addImportPath(testFile("singleton/module"));

    QQmlComponent component(&engine, testFile("singletonTest8.qml"));
    VERIFY_ERRORS(0);
    QObject *o = component.create();
    QVERIFY(o != 0);

    verifyCompositeSingletonPropertyValues(o, "value1", 225, "value2", 55);
    verifyCompositeSingletonPropertyValues(o, "value3", 225, "value4", 55);

    // lets verify that the singleton instance we are using is the same
    // when loaded through another file
    QObject *s1 = NULL;
    getSingletonInstance(o, "singletonInstance", &s1);
    QVERIFY(s1 != 0);

    QObject* s2 = NULL;
    getSingletonInstance(engine, "singletonTest8a.qml", "singletonInstance", &s2);
    QVERIFY(s2 != 0);

    QCOMPARE(s1, s2);
}

// Tries to instantiate a type with a pragma Singleton and fails
void tst_qqmllanguage::compositeSingletonInstantiateError()
{
    QQmlComponent component(&engine, testFile("singletonTest9.qml"));
    VERIFY_ERRORS("singletonTest9.error.txt");
}

// Having a composite singleton type as dynamic property type is allowed
void tst_qqmllanguage::compositeSingletonDynamicPropertyError()
{
    QQmlComponent component(&engine, testFile("singletonTest10.qml"));
    VERIFY_ERRORS(0);
}

// Having a composite singleton type as dynamic signal parameter succeeds
// (like C++ singleton)
void tst_qqmllanguage::compositeSingletonDynamicSignal()
{
    QQmlComponent component(&engine, testFile("singletonTest11.qml"));
    VERIFY_ERRORS(0);
    QObject *o = component.create();
    QVERIFY(o != 0);

    verifyCompositeSingletonPropertyValues(o, "value1", 99, "value2", -55);
}

// Use qmlRegisterType to register a qml composite type with pragma Singleton defined in it.
// This will fail as qmlRegisterType will only instantiate CompositeTypes.
void tst_qqmllanguage::compositeSingletonQmlRegisterTypeError()
{
    qmlRegisterType(testFileUrl("singleton/registeredComposite/CompositeType.qml"),
        "CompositeSingletonTest", 1, 0, "RegisteredCompositeType");
    QQmlComponent component(&engine, testFile("singletonTest12.qml"));
    VERIFY_ERRORS("singletonTest12.error.txt");
}

// Qmldir defines a type as a singleton, but the qml file does not have a pragma Singleton.
void tst_qqmllanguage::compositeSingletonQmldirNoPragmaError()
{
    QQmlComponent component(&engine, testFile("singletonTest13.qml"));
    VERIFY_ERRORS("singletonTest13.error.txt");
}

// Invalid singleton definition in the qmldir file results in an error
void tst_qqmllanguage::compositeSingletonQmlDirError()
{
    QQmlComponent component(&engine, testFile("singletonTest14.qml"));
    VERIFY_ERRORS("singletonTest14.error.txt");
}

// Load a remote composite singleton type via qmldir that defines the type as a singleton
void tst_qqmllanguage::compositeSingletonRemote()
{
    ThreadedTestHTTPServer server(dataDirectory());

    QFile f(testFile("singletonTest15.qml"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray contents = f.readAll();
    f.close();

    contents.replace(QByteArrayLiteral("{{ServerBaseUrl}}"), server.baseUrl().toString().toUtf8());

    QQmlComponent component(&engine);
    component.setData(contents, testFileUrl("singletonTest15.qml"));

    while (component.isLoading())
        QCoreApplication::processEvents( QEventLoop::ExcludeUserInputEvents | QEventLoop::WaitForMoreEvents, 50);

    VERIFY_ERRORS(0);
    QObject *o = component.create();
    QVERIFY(o != 0);

    verifyCompositeSingletonPropertyValues(o, "value1", 525, "value2", 355);
}

// Load a composite singleton type and a javascript file that has .pragma library
// in it. This will make sure that the javascript .pragma does not get mixed with
// the pragma Singleton changes.
void tst_qqmllanguage::compositeSingletonJavaScriptPragma()
{
    QQmlComponent component(&engine, testFile("singletonTest16.qml"));
    VERIFY_ERRORS(0);
    QObject *o = component.create();
    QVERIFY(o != 0);

    // The value1 that is read from the SingletonType was changed from 125 to 99
    // in compositeSingletonDynamicSignal() above. As the type is a singleton and
    // the engine has not been destroyed, we just retrieve the old instance and
    // the value is still 99.
    verifyCompositeSingletonPropertyValues(o, "value1", 99, "value2", 333);
}

// Reads values from a Singleton accessed through selectors.
void tst_qqmllanguage::compositeSingletonSelectors()
{
    QQmlEngine e2;
    QQmlFileSelector qmlSelector(&e2);
    qmlSelector.setExtraSelectors(QStringList() << "basicSelector");
    QQmlComponent component(&e2, testFile("singletonTest1.qml"));
    VERIFY_ERRORS(0);
    QObject *o = component.create();
    QVERIFY(o != 0);

    verifyCompositeSingletonPropertyValues(o, "value1", 625, "value2", 455);
}

// Reads values from a Singleton that was registered through the C++ API:
// qmlRegisterSingletonType.
void tst_qqmllanguage::compositeSingletonRegistered()
{
    QQmlComponent component(&engine, testFile("singletonTest17.qml"));
    VERIFY_ERRORS(0);
    QObject *o = component.create();
    QVERIFY(o != 0);

    verifyCompositeSingletonPropertyValues(o, "value1", 925, "value2", 755);
}

void tst_qqmllanguage::customParserBindingScopes()
{
    QQmlComponent component(&engine, testFile("customParserBindingScopes.qml"));
    VERIFY_ERRORS(0);
    QScopedPointer<QObject> o(component.create());
    QVERIFY(!o.isNull());
    QPointer<QObject> child = qvariant_cast<QObject*>(o->property("child"));
    QVERIFY(!child.isNull());
    QCOMPARE(child->property("testProperty").toInt(), 42);
}

void tst_qqmllanguage::customParserEvaluateEnum()
{
    QQmlComponent component(&engine, testFile("customParserEvaluateEnum.qml"));
    VERIFY_ERRORS(0);
    QScopedPointer<QObject> o(component.create());
    QVERIFY(!o.isNull());
}

void tst_qqmllanguage::customParserProperties()
{
    QQmlComponent component(&engine, testFile("customParserProperties.qml"));
    VERIFY_ERRORS(0);
    QScopedPointer<QObject> o(component.create());
    QVERIFY(!o.isNull());
    SimpleObjectWithCustomParser *testObject = qobject_cast<SimpleObjectWithCustomParser*>(o.data());
    QVERIFY(testObject);
    QCOMPARE(testObject->customBindingsCount(), 0);
    QCOMPARE(testObject->intProperty(), 42);
    QCOMPARE(testObject->property("qmlString").toString(), QStringLiteral("Hello"));
    QVERIFY(!testObject->property("someObject").isNull());
}

void tst_qqmllanguage::customParserWithExtendedObject()
{
    QQmlComponent component(&engine, testFile("customExtendedParserProperties.qml"));
    VERIFY_ERRORS(0);
    QScopedPointer<QObject> o(component.create());
    QVERIFY(!o.isNull());
    SimpleObjectWithCustomParser *testObject = qobject_cast<SimpleObjectWithCustomParser*>(o.data());
    QVERIFY(testObject);
    QCOMPARE(testObject->customBindingsCount(), 0);
    QCOMPARE(testObject->intProperty(), 42);
    QCOMPARE(testObject->property("qmlString").toString(), QStringLiteral("Hello"));
    QVERIFY(!testObject->property("someObject").isNull());

    QVariant returnValue;
    QVERIFY(QMetaObject::invokeMethod(o.data(), "getExtendedProperty", Q_RETURN_ARG(QVariant, returnValue)));
    QCOMPARE(returnValue.toInt(), 1584);
}

void tst_qqmllanguage::nestedCustomParsers()
{
    QQmlComponent component(&engine, testFile("nestedCustomParsers.qml"));
    VERIFY_ERRORS(0);
    QScopedPointer<QObject> o(component.create());
    QVERIFY(!o.isNull());
    SimpleObjectWithCustomParser *testObject = qobject_cast<SimpleObjectWithCustomParser*>(o.data());
    QVERIFY(testObject);
    QCOMPARE(testObject->customBindingsCount(), 1);
    SimpleObjectWithCustomParser *nestedObject = qobject_cast<SimpleObjectWithCustomParser*>(testObject->property("nested").value<QObject*>());
    QVERIFY(nestedObject);
    QCOMPARE(nestedObject->customBindingsCount(), 1);
}

void tst_qqmllanguage::preservePropertyCacheOnGroupObjects()
{
    QQmlComponent component(&engine, testFile("preservePropertyCacheOnGroupObjects.qml"));
    VERIFY_ERRORS(0);
    QScopedPointer<QObject> o(component.create());
    QVERIFY(!o.isNull());
    QObject *subObject = qvariant_cast<QObject*>(o->property("subObject"));
    QVERIFY(subObject);
    QCOMPARE(subObject->property("value").toInt(), 42);

    QQmlData *ddata = QQmlData::get(subObject);
    QVERIFY(ddata);
    QQmlPropertyCache *subCache = ddata->propertyCache;
    QVERIFY(subCache);
    QQmlPropertyData *pd = subCache->property(QStringLiteral("newProperty"), /*object*/0, /*context*/0);
    QVERIFY(pd);
    QCOMPARE(pd->propType(), qMetaTypeId<int>());
}

void tst_qqmllanguage::propertyCacheInSync()
{
    QQmlComponent component(&engine, testFile("propertyCacheInSync.qml"));
    VERIFY_ERRORS(0);
    QScopedPointer<QObject> o(component.create());
    QVERIFY(!o.isNull());
    QObject *anchors = qvariant_cast<QObject*>(o->property("anchors"));
    QVERIFY(anchors);
    QQmlVMEMetaObject *vmemo = QQmlVMEMetaObject::get(anchors);
    QVERIFY(vmemo);
    QQmlPropertyCache *vmemoCache = vmemo->propertyCache();
    QVERIFY(vmemoCache);
    QQmlData *ddata = QQmlData::get(anchors);
    QVERIFY(ddata);
    QVERIFY(ddata->propertyCache);
    // Those always have to be in sync and correct.
    QCOMPARE(ddata->propertyCache, vmemoCache);
    QCOMPARE(anchors->property("margins").toInt(), 50);
}

void tst_qqmllanguage::rootObjectInCreationNotForSubObjects()
{
    QQmlComponent component(&engine, testFile("rootObjectInCreationNotForSubObjects.qml"));
    VERIFY_ERRORS(0);
    QScopedPointer<QObject> o(component.create());
    QVERIFY(!o.isNull());

    // QQmlComponent should have set this back to false anyway
    QQmlData *ddata = QQmlData::get(o.data());
    QVERIFY(!ddata->rootObjectInCreation);

    QObject *subObject = qvariant_cast<QObject*>(o->property("subObject"));
    QVERIFY(!subObject);

    qmlExecuteDeferred(o.data());

    subObject = qvariant_cast<QObject*>(o->property("subObject"));
    QVERIFY(subObject);

    ddata = QQmlData::get(subObject);
    // This should never have been set in the first place as there is no
    // QQmlComponent to set it back to false.
    QVERIFY(!ddata->rootObjectInCreation);
}

void tst_qqmllanguage::noChildEvents()
{
    QQmlComponent component(&engine);
    component.setData("import QtQml 2.0; import Test 1.0; MyQmlObject { property QtObject child: QtObject {} }", QUrl());
    VERIFY_ERRORS(0);
    MyQmlObject *object = qobject_cast<MyQmlObject*>(component.create());
    QVERIFY(object != 0);
    QCOMPARE(object->childAddedEventCount(), 0);
}

void tst_qqmllanguage::earlyIdObjectAccess()
{
    QQmlComponent component(&engine, testFileUrl("earlyIdObjectAccess.qml"));
    QScopedPointer<QObject> o(component.create());
    QVERIFY(!o.isNull());
    QVERIFY(o->property("success").toBool());
}

void tst_qqmllanguage::deleteSingletons()
{
    QPointer<QObject> singleton;
    {
        QQmlEngine tmpEngine;
        QQmlComponent component(&tmpEngine, testFile("singletonTest5.qml"));
        VERIFY_ERRORS(0);
        QObject *o = component.create();
        QVERIFY(o != 0);
        QObject *s1 = NULL;
        getSingletonInstance(o, "singletonInstance", &s1);
        QVERIFY(s1 != 0);
        singleton = s1;
        QVERIFY(singleton.data() != 0);
    }
    QVERIFY(singleton.data() == 0);
}

void tst_qqmllanguage::arrayBuffer_data()
{
    QTest::addColumn<QString>("file");
    QTest::newRow("arraybuffer_property_get") << "arraybuffer_property_get.qml";
    QTest::newRow("arraybuffer_property_set") << "arraybuffer_property_set.qml";
    QTest::newRow("arraybuffer_signal_arg") << "arraybuffer_signal_arg.qml";
    QTest::newRow("arraybuffer_method_arg") << "arraybuffer_method_arg.qml";
    QTest::newRow("arraybuffer_method_return") << "arraybuffer_method_return.qml";
    QTest::newRow("arraybuffer_method_overload") << "arraybuffer_method_overload.qml";
}

void tst_qqmllanguage::arrayBuffer()
{
    QFETCH(QString, file);
    QQmlComponent component(&engine, testFile(file));
    VERIFY_ERRORS(0);
    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property("ok").toBool(), true);
}

void tst_qqmllanguage::defaultListProperty()
{
    QQmlComponent component(&engine, testFileUrl("defaultListProperty.qml"));
    VERIFY_ERRORS(0);
    QScopedPointer<QObject> o(component.create());
}

void tst_qqmllanguage::namespacedPropertyTypes()
{
    QQmlComponent component(&engine, testFileUrl("namespacedPropertyTypes.qml"));
    VERIFY_ERRORS(0);
    QScopedPointer<QObject> o(component.create());
    QVERIFY(!o.isNull());
}

QTEST_MAIN(tst_qqmllanguage)

#include "tst_qqmllanguage.moc"

/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "abstracttestsuite.h"
#include <QtTest/QtTest>
#include <QtCore/qset.h>
#include <QtCore/QSysInfo>
#include <QtCore/qtextstream.h>
#include <private/qmetaobjectbuilder_p.h>

/*!
   AbstractTestSuite provides a way of building QtTest test objects
   dynamically. The use case is integration of JavaScript test suites
   into QtTest autotests.

   Subclasses add their tests functions with addTestFunction() in the
   constructor, and must reimplement runTestFunction(). Additionally,
   subclasses can reimplement initTestCase() and cleanupTestCase()
   (but make sure to call the base implementation).

   AbstractTestSuite uses configuration files for getting information
   about skipped tests (skip.txt) and expected test failures
   (expect_fail.txt). Subclasses must reimplement
   createSkipConfigFile() and createExpectFailConfigFile() for
   creating these files, and configData() for processing an entry of
   such a file.

   The config file format is as follows:
   - Lines starting with '#' are skipped.
   - Lines of the form [SYMBOL] means that the upcoming data
     should only be processed if the given SYMBOL is defined on
     this platform.
   - Any other line is split on ' | ' and handed off to the client.

   Subclasses must provide a default tests directory (where the
   subclass expects to find the script files to run as tests), and a
   default config file directory. Some environment variables can be
   used to affect where AbstractTestSuite will look for files:

   - QTSCRIPT_TEST_CONFIG_DIR: Overrides the default test config path.

   - QTSCRIPT_TEST_CONFIG_SUFFIX: Is appended to "skip" and
   "expect_fail" to create the test config name. This makes it easy to
   maintain skip- and expect_fail-files corresponding to different
   revisions of a test suite, and switch between them.

   - QTSCRIPT_TEST_DIR: Overrides the default test dir.

   AbstractTestSuite does _not_ define how the test dir itself is
   processed or how tests are run; this is left up to the subclass.

   If no config files are found, AbstractTestSuite will ask the
   subclass to create a default skip file. Also, the
   shouldGenerateExpectedFailures variable will be set to true. The
   subclass should check for this when a test fails, and add an entry
   to its set of expected failures. When all tests have been run,
   AbstractTestSuite will ask the subclass to create the expect_fail
   file based on the tests that failed. The next time the autotest is
   run, the created config files will be used.

   The reason for skipping a test is usually that it takes a very long
   time to complete (or even hangs completely), or it crashes. It's
   not possible for the test runner to know in advance which tests are
   problematic, which is why the entries to the skip file are
   typically added manually. When running tests for the first time, it
   can be useful to run the autotest with the -v1 command line option,
   so you can see the name of each test before it's run, and can add a
   skip entry if appropriate.
*/

class TestConfigClientInterface;
// For parsing information about skipped tests and expected failures.
class TestConfigParser
{
public:
    static void parse(const QString &path,
                      TestConfig::Mode mode,
                      TestConfigClientInterface *client);

private:
    static QString unescape(const QString &);
    static bool isKnownSymbol(const QString &);
    static bool isDefined(const QString &);

    static QSet<QString> knownSymbols;
    static QSet<QString> definedSymbols;
};

QSet<QString> TestConfigParser::knownSymbols;
QSet<QString> TestConfigParser::definedSymbols;

/**
   Parses the config file at the given \a path in the given \a mode.
   Handling of errors and data is delegated to the given \a client.
*/
void TestConfigParser::parse(const QString &path,
                             TestConfig::Mode mode,
                             TestConfigClientInterface *client)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    QTextStream stream(&file);
    int lineNumber = 0;
    QString predicate;
    const QString separator = QString::fromLatin1(" | ");
    while (!stream.atEnd()) {
        ++lineNumber;
        QString line = stream.readLine();
        if (line.isEmpty())
            continue;
        if (line.startsWith('#')) // Comment
            continue;
        if (line.startsWith('[')) { // Predicate
            if (!line.endsWith(']')) {
                client->configError(path, "malformed predicate", lineNumber);
                return;
            }
            QString symbol = line.mid(1, line.size()-2);
            if (isKnownSymbol(symbol)) {
                predicate = symbol;
            } else {
                qWarning("symbol %s is not known -- add it to TestConfigParser!", qPrintable(symbol));
                predicate = QString();
            }
        } else {
            if (predicate.isEmpty() || isDefined(predicate)) {
                QStringList parts = line.split(separator, QString::KeepEmptyParts);
                for (int i = 0; i < parts.size(); ++i)
                    parts[i] = unescape(parts[i]);
                client->configData(mode, parts);
            }
        }
    }
}

QString TestConfigParser::unescape(const QString &str)
{
    return QString(str).replace("\\n", "\n");
}

bool TestConfigParser::isKnownSymbol(const QString &symbol)
{
    if (knownSymbols.isEmpty()) {
        knownSymbols
            // If you add a symbol here, add a case for it in
            // isDefined() as well.
            << "Q_OS_LINUX"
            << "Q_OS_SOLARIS"
            << "Q_OS_WINCE"
            << "Q_OS_SYMBIAN"
            << "Q_OS_MAC"
            << "Q_OS_WIN"
            << "Q_CC_MSVC"
            << "Q_CC_MSVC32"
            << "Q_CC_MSVC64"
            << "Q_CC_MINGW"
            << "Q_CC_MINGW32"
            << "Q_CC_MINGW64"
            << "Q_CC_INTEL"
            << "Q_CC_INTEL32"
            << "Q_CC_INTEL64"
            ;
    }
    return knownSymbols.contains(symbol);
}

bool TestConfigParser::isDefined(const QString &symbol)
{
    if (definedSymbols.isEmpty()) {
        definedSymbols
#ifdef Q_OS_LINUX
            << "Q_OS_LINUX"
#endif
#ifdef Q_OS_SOLARIS
            << "Q_OS_SOLARIS"
#endif
#ifdef Q_OS_WINCE
            << "Q_OS_WINCE"
#endif
#ifdef Q_OS_SYMBIAN
            << "Q_OS_SYMBIAN"
#endif
#ifdef Q_OS_MAC
            << "Q_OS_MAC"
#endif
#ifdef Q_OS_WIN
            << "Q_OS_WIN"
#endif
#ifdef Q_CC_MSVC
            << "Q_CC_MSVC"
            << (QStringLiteral("Q_CC_MSVC") + QString::number(QSysInfo::WordSize))
#endif
#ifdef Q_CC_MINGW
            << "Q_CC_MINGW"
            << (QStringLiteral("Q_CC_MINGW") + QString::number(QSysInfo::WordSize))
#endif
#ifdef Q_CC_INTEL
            << "Q_CC_INTEL"
            << (QStringLiteral("Q_CC_INTEL") + QString::number(QSysInfo::WordSize))
#endif
            ;
    }
    return definedSymbols.contains(symbol);
}


const QMetaObject *AbstractTestSuite::metaObject() const
{
    return dynamicMetaObject;
}

void *AbstractTestSuite::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, dynamicMetaObject->className()))
        return static_cast<void*>(const_cast<AbstractTestSuite*>(this));
    return QObject::qt_metacast(_clname);
}

void AbstractTestSuite::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_UNUSED(_a);
    if (_c == QMetaObject::InvokeMetaMethod) {
        AbstractTestSuite *_t = static_cast<AbstractTestSuite *>(_o);
        switch (_id) {
        case 0:
            _t->initTestCase();
            break;
        case 1:
            _t->cleanupTestCase();
            break;
        default:
            // If another method is added above, this offset must be adjusted.
            _t->runTestFunction(_id - 2);
        }
    }
}

int AbstractTestSuite::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(dynamicMetaObject->cast(this));
        int ownMethodCount = dynamicMetaObject->methodCount() - dynamicMetaObject->methodOffset();
        if (_id < ownMethodCount)
            qt_static_metacall(this, _c, _id, _a);
        _id -= ownMethodCount;
    }
    return _id;
}

void AbstractTestSuite::addPrivateSlot(const QByteArray &signature)
{
    QMetaMethodBuilder slot = metaBuilder->addSlot(signature);
    slot.setAccess(QMetaMethod::Private);
}

AbstractTestSuite::AbstractTestSuite(const QByteArray &className,
                                     const QString &defaultTestsPath,
                                     const QString &defaultConfigPath)
    : shouldGenerateExpectedFailures(false),
      dynamicMetaObject(0),
      metaBuilder(new QMetaObjectBuilder)
{
    metaBuilder->setSuperClass(&QObject::staticMetaObject);
    metaBuilder->setClassName(className);
    metaBuilder->setStaticMetacallFunction(qt_static_metacall);

    QString testConfigPath = qgetenv("QTSCRIPT_TEST_CONFIG_DIR");
    if (testConfigPath.isEmpty())
        testConfigPath = defaultConfigPath;
    QString configSuffix = qgetenv("QTSCRIPT_TEST_CONFIG_SUFFIX");
    skipConfigPath = QString::fromLatin1("%0/skip%1.txt")
                     .arg(testConfigPath).arg(configSuffix);
    expectFailConfigPath = QString::fromLatin1("%0/expect_fail%1.txt")
                           .arg(testConfigPath).arg(configSuffix);

    QString testsPath = qgetenv("QTSCRIPT_TEST_DIR");
    if (testsPath.isEmpty())
        testsPath = defaultTestsPath;
    testsDir = QDir(testsPath);

    addTestFunction("initTestCase");
    addTestFunction("cleanupTestCase");

    // Subclass constructors should add their custom test functions to
    // the meta-object and call finalizeMetaObject().
}

AbstractTestSuite::~AbstractTestSuite()
{
    free(dynamicMetaObject);
}

void AbstractTestSuite::addTestFunction(const QString &name,
                                        DataFunctionCreation dfc)
{
    if (dfc == CreateDataFunction) {
        QString dataSignature = QString::fromLatin1("%0_data()").arg(name);
        addPrivateSlot(dataSignature.toLatin1());
    }
    QString signature = QString::fromLatin1("%0()").arg(name);
    addPrivateSlot(signature.toLatin1());
}

void AbstractTestSuite::finalizeMetaObject()
{
    dynamicMetaObject = metaBuilder->toMetaObject();
}

void AbstractTestSuite::initTestCase()
{
    if (!testsDir.exists()) {
        QString message = QString::fromLatin1("tests directory (%0) doesn't exist.")
                          .arg(testsDir.path());
        QFAIL(qPrintable(message));
        return;
    }

    if (QFileInfo(skipConfigPath).exists())
        TestConfigParser::parse(skipConfigPath, TestConfig::Skip, this);
    else
        createSkipConfigFile();

    if (QFileInfo(expectFailConfigPath).exists())
        TestConfigParser::parse(expectFailConfigPath, TestConfig::ExpectFail, this);
    else
        shouldGenerateExpectedFailures = true;
}

void AbstractTestSuite::cleanupTestCase()
{
    if (shouldGenerateExpectedFailures)
        createExpectFailConfigFile();
}

void AbstractTestSuite::configError(const QString &path, const QString &message, int lineNumber)
{
    QString output;
    output.append(path);
    if (lineNumber != -1)
        output.append(":").append(QString::number(lineNumber));
    output.append(": ").append(message);
    QFAIL(qPrintable(output));
}

void AbstractTestSuite::createSkipConfigFile()
{
    QFile file(skipConfigPath);
    if (!file.open(QIODevice::WriteOnly))
        return;
    QWARN(qPrintable(QString::fromLatin1("creating %0").arg(skipConfigPath)));
    QTextStream stream(&file);

    writeSkipConfigFile(stream);

    file.close();
}

void AbstractTestSuite::createExpectFailConfigFile()
{
    QFile file(expectFailConfigPath);
    if (!file.open(QFile::WriteOnly))
        return;
    QWARN(qPrintable(QString::fromLatin1("creating %0").arg(expectFailConfigPath)));
    QTextStream stream(&file);

    writeExpectFailConfigFile(stream);

    file.close();
}

/*!
  Convenience function for reading all contents of a file.
 */
QString AbstractTestSuite::readFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QFile::ReadOnly))
        return QString();
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    return stream.readAll();
}

/*!
  Escapes characters in the string \a str so it's suitable for writing
  to a config file.
 */
QString AbstractTestSuite::escape(const QString &str)
{
    return QString(str).replace("\n", "\\n");
}

/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#include "qdeclarativedatatest.h"

#include <QtDeclarative/QDeclarativeComponent>
#include <QtDeclarative/QDeclarativeError>
#include <QtDeclarative/QDeclarativeContext>
#include <QtDeclarative/QDeclarativeEngine>
#include <QtCore/QTextStream>

QDeclarativeDataTest *QDeclarativeDataTest::m_instance = 0;

QDeclarativeDataTest::QDeclarativeDataTest() :
#ifdef QT_TESTCASE_BUILDDIR
    m_dataDirectory(QTest::qFindTestData("data", QT_DECLARATIVETEST_DATADIR, 0, QT_TESTCASE_BUILDDIR)),
    m_importsDirectory(QTest::qFindTestData("imports", QT_DECLARATIVETEST_IMPORTSDIR, 0, QT_TESTCASE_BUILDDIR)),
#else
    m_dataDirectory(QTest::qFindTestData("data", QT_DECLARATIVETEST_DATADIR, 0)),
    m_importsDirectory(QTest::qFindTestData("imports", QT_DECLARATIVETEST_IMPORTSDIR, 0)),
#endif
    m_dataDirectoryUrl(QUrl::fromLocalFile(m_dataDirectory + QLatin1Char('/'))),
    m_importsDirectoryUrl(QUrl::fromLocalFile(m_importsDirectory + QLatin1Char('/')))
{
    m_instance = this;
}

QDeclarativeDataTest::~QDeclarativeDataTest()
{
    m_instance = 0;
}

void QDeclarativeDataTest::initTestCase()
{
    QVERIFY2(!m_dataDirectory.isEmpty(), "'data' directory not found");
    m_directory = QFileInfo(m_dataDirectory).absolutePath();
    QVERIFY2(QDir::setCurrent(m_directory), qPrintable(QLatin1String("Could not chdir to ") + m_directory));
}

QString QDeclarativeDataTest::testFile(const QString &fileName) const
{
    if (m_directory.isEmpty())
        qFatal("QDeclarativeDataTest::initTestCase() not called.");
    QString result = m_dataDirectory;
    result += QLatin1Char('/');
    result += fileName;
    return result;
}

QByteArray QDeclarativeDataTest::msgComponentError(const QDeclarativeComponent &c,
                                                   const QDeclarativeEngine *engine /* = 0 */)
{
    QString result;
    const QList<QDeclarativeError> errors = c.errors();
    QTextStream str(&result);
    str << "Component '" << c.url().toString() << "' has " << errors.size()
        << " errors: '";
    for (int i = 0; i < errors.size(); ++i) {
        if (i)
            str << ", '";
        str << errors.at(i).toString() << '\'';

    }
    if (!engine)
        if (QDeclarativeContext *context = c.creationContext())
            engine = context->engine();
    if (engine) {
        str << " Import paths: (" << engine->importPathList().join(QStringLiteral(", "))
            << ") Plugin paths: (" << engine->pluginPathList().join(QStringLiteral(", "))
            << ')';
    }
    return result.toLocal8Bit();
}

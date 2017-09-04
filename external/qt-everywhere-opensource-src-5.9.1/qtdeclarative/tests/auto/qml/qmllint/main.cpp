/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Sergio Martins <sergio.martins@kdab.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include <QtTest/QtTest>
#include <QProcess>
#include <QString>

class TestQmllint: public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void test();
    void test_data();
private:
    QString m_qmllintPath;
};

void TestQmllint::initTestCase()
{
    m_qmllintPath = QLibraryInfo::location(QLibraryInfo::BinariesPath) + QLatin1String("/qmllint");
#ifdef Q_OS_WIN
    m_qmllintPath += QLatin1String(".exe");
#endif
    if (!QFileInfo(m_qmllintPath).exists()) {
        QString message = QStringLiteral("qmllint executable not found (looked for %0)").arg(m_qmllintPath);
        QFAIL(qPrintable(message));
    }
}

void TestQmllint::test_data()
{
    QTest::addColumn<QString>("filename");
    QTest::addColumn<bool>("isValid");

    // Valid files:
    QTest::newRow("Simple_QML") << QStringLiteral("Simple.qml") << true;
    QTest::newRow("QML_importing_JS") << QStringLiteral("importing_js.qml") << true;
    QTest::newRow("QTBUG-45916_JS_with_pragma_and_import") << QStringLiteral("QTBUG-45916.js") << true;

    // Invalid files:
    QTest::newRow("Invalid_syntax_QML") << QStringLiteral("failure1.qml") << false;
    QTest::newRow("Invalid_syntax_JS") << QStringLiteral("failure1.js") << false;
}

void TestQmllint::test()
{
    QFETCH(QString, filename);
    QFETCH(bool, isValid);
    filename = QStringLiteral("data/") + filename;
    QStringList args;
    args << QStringLiteral("--silent") << filename;

    bool success = QProcess::execute(m_qmllintPath, args) == 0;
    QCOMPARE(success, isValid);
}

QTEST_MAIN(TestQmllint)
#include "main.moc"

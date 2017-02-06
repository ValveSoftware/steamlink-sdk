/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
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

#include <QtTest>
#include <QObject>
#include <QXmlStreamReader>
#include <QtScxml/qscxmlcompiler.h>
#include <QtScxml/qscxmlstatemachine.h>

Q_DECLARE_METATYPE(QScxmlError);

class tst_Parser: public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void error_data();
    void error();
};

void tst_Parser::error_data()
{
    QTest::addColumn<QString>("scxmlFileName");
    QTest::addColumn<QString>("errorFileName");

    QDir dir(QLatin1String(":/tst_parser/data/"));
    const auto dirEntries = dir.entryList();
    for (const QString &entry : dirEntries) {
        if (!entry.endsWith(QLatin1String(".errors"))) {
            QString scxmlFileName = dir.filePath(entry);
            QTest::newRow(entry.toLatin1().constData())
                    << scxmlFileName << (scxmlFileName + QLatin1String(".errors"));
        }
    }
}

void tst_Parser::error()
{
    QFETCH(QString, scxmlFileName);
    QFETCH(QString, errorFileName);

    QFile errorFile(errorFileName);
    errorFile.open(QIODevice::ReadOnly | QIODevice::Text);
    const QStringList expectedErrors =
            QString::fromUtf8(errorFile.readAll()).split('\n', QString::SkipEmptyParts);

    QScopedPointer<QScxmlStateMachine> stateMachine(QScxmlStateMachine::fromFile(scxmlFileName));
    QVERIFY(!stateMachine.isNull());

    const QVector<QScxmlError> errors = stateMachine->parseErrors();
    if (errors.count() != expectedErrors.count()) {
        for (const QScxmlError &error : errors) {
            qDebug() << error.toString();
        }
    }
    QCOMPARE(errors.count(), expectedErrors.count());

    for (int i = 0; i < errors.count(); ++i)
        QCOMPARE(errors.at(i).toString(), expectedErrors.at(i));
}

QTEST_MAIN(tst_Parser)

#include "tst_parser.moc"



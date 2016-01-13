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

#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QtTest>
#include <QCoreApplication>

#include <QtQuick/private/qquickfontmetrics_p.h>

#include <QFontMetricsF>

class tst_QuickFontMetrics : public QObject
{
    Q_OBJECT

public:
    tst_QuickFontMetrics();

private Q_SLOTS:
    void properties();
    void functions_data();
    void functions();
};

tst_QuickFontMetrics::tst_QuickFontMetrics()
{
}

void tst_QuickFontMetrics::properties()
{
    QStringList families = QFontDatabase().families().mid(0, 10);
    QQuickFontMetrics metrics;

    foreach (const QString &family, families) {
        QFont font(family);
        QFontMetricsF expected(font);

        QSignalSpy spy(&metrics, SIGNAL(fontChanged(QFont)));
        metrics.setFont(font);
        QCOMPARE(spy.count(), 1);

        QCOMPARE(metrics.ascent(), expected.ascent());
        QCOMPARE(metrics.descent(), expected.descent());
        QCOMPARE(metrics.height(), expected.height());
        QCOMPARE(metrics.leading(), expected.leading());
        QCOMPARE(metrics.lineSpacing(), expected.lineSpacing());
        QCOMPARE(metrics.minimumLeftBearing(), expected.minLeftBearing());
        QCOMPARE(metrics.minimumRightBearing(), expected.minRightBearing());
        QCOMPARE(metrics.maximumCharacterWidth(), expected.maxWidth());
        QCOMPARE(metrics.xHeight(), expected.xHeight());
        QCOMPARE(metrics.averageCharacterWidth(), expected.averageCharWidth());
        QCOMPARE(metrics.underlinePosition(), expected.underlinePos());
        QCOMPARE(metrics.overlinePosition(), expected.overlinePos());
        QCOMPARE(metrics.strikeOutPosition(), expected.strikeOutPos());
        QCOMPARE(metrics.lineWidth(), expected.lineWidth());
    }
}

Q_DECLARE_METATYPE(Qt::TextElideMode)

void tst_QuickFontMetrics::functions_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<Qt::TextElideMode>("mode");
    QTest::addColumn<qreal>("width");
    QTest::addColumn<int>("flags");

    QStringList strings;
    strings << QString()
            << QString::fromLatin1("")
            << QString::fromLatin1("0")
            << QString::fromLatin1("@@@@@@@")
            << QString::fromLatin1("Hello");

    QVector<Qt::TextElideMode> elideModes;
    elideModes << Qt::ElideLeft << Qt::ElideMiddle << Qt::ElideRight << Qt::ElideNone;

    for (int stringIndex = 0; stringIndex < strings.size(); ++stringIndex) {
        const QString string = strings.at(stringIndex);

        for (int elideModeIndex = 0; elideModeIndex < elideModes.size(); ++elideModeIndex) {
            Qt::TextElideMode elideMode = static_cast<Qt::TextElideMode>(elideModes.at(elideModeIndex));

            for (qreal width = 0; width < 100; width += 20) {
                const QString tag = QString::fromLatin1("string=%1, mode=%2, width=%3").arg(string).arg(elideMode).arg(width);
                QTest::newRow(qPrintable(tag)) << QString() << elideMode << width << 0;
            }
        }
    }
}

void tst_QuickFontMetrics::functions()
{
    QFETCH(QString, text);
    QFETCH(Qt::TextElideMode, mode);
    QFETCH(qreal, width);
    QFETCH(int, flags);

    QQuickFontMetrics metrics;
    QFontMetricsF expected = QFontMetricsF(QFont());

    QCOMPARE(metrics.elidedText(text, mode, width, flags), expected.elidedText(text, mode, width, flags));
    QCOMPARE(metrics.advanceWidth(text), expected.width(text));
    QCOMPARE(metrics.boundingRect(text), expected.boundingRect(text));
    QCOMPARE(metrics.tightBoundingRect(text), expected.tightBoundingRect(text));
}

QTEST_MAIN(tst_QuickFontMetrics)

#include "tst_quickfontmetrics.moc"

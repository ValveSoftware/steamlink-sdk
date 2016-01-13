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
#include <QString>
#include <QtTest>
#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>

#include <QtQuick/private/qquicktextmetrics_p.h>

#include <QFontMetricsF>

class tst_QQuickTextMetrics : public QObject
{
    Q_OBJECT

public:
    tst_QQuickTextMetrics();

private Q_SLOTS:
    void font();
    void functionsWithArguments_data();
    void functionsWithArguments();
};

tst_QQuickTextMetrics::tst_QQuickTextMetrics()
{
}

void tst_QQuickTextMetrics::font()
{
    QQuickTextMetrics metrics;

    QSignalSpy fontSpy(&metrics, SIGNAL(fontChanged()));
    QSignalSpy metricsSpy(&metrics, SIGNAL(metricsChanged()));
    QFont font;
    font.setPointSize(font.pointSize() + 1);
    metrics.setFont(font);
    QCOMPARE(fontSpy.count(), 1);
    QCOMPARE(metricsSpy.count(), 1);
}

Q_DECLARE_METATYPE(Qt::TextElideMode)

void tst_QQuickTextMetrics::functionsWithArguments_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<Qt::TextElideMode>("mode");
    QTest::addColumn<qreal>("width");

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
                QTest::newRow(qPrintable(tag)) << QString() << elideMode << width;
            }
        }
    }
}

void tst_QQuickTextMetrics::functionsWithArguments()
{
    QFETCH(QString, text);
    QFETCH(Qt::TextElideMode, mode);
    QFETCH(qreal, width);

    QQuickTextMetrics metrics;
    // Ensures that the values actually change.
    metrics.setText(text + "extra");
    metrics.setElideWidth(width + 1);
    switch (mode) {
        case Qt::ElideNone: metrics.setElide(Qt::ElideMiddle); break;
        case Qt::ElideLeft: metrics.setElide(Qt::ElideRight); break;
        case Qt::ElideMiddle: metrics.setElide(Qt::ElideNone); break;
        case Qt::ElideRight: metrics.setElide(Qt::ElideLeft); break;
    }

    QSignalSpy textSpy(&metrics, SIGNAL(textChanged()));
    QSignalSpy metricsSpy(&metrics, SIGNAL(metricsChanged()));
    metrics.setText(text);
    QCOMPARE(textSpy.count(), 1);
    QCOMPARE(metricsSpy.count(), 1);

    QSignalSpy elideSpy(&metrics, SIGNAL(elideChanged()));
    metrics.setElide(mode);
    QCOMPARE(elideSpy.count(), 1);
    QCOMPARE(metricsSpy.count(), 2);

    QSignalSpy elideWidthSpy(&metrics, SIGNAL(elideWidthChanged()));
    metrics.setElideWidth(width);
    QCOMPARE(elideWidthSpy.count(), 1);
    QCOMPARE(metricsSpy.count(), 3);

    QFontMetricsF expected = QFontMetricsF(QFont());

    QCOMPARE(metrics.elidedText(), expected.elidedText(text, mode, width, 0));
    QCOMPARE(metrics.advanceWidth(), expected.width(text));
    QCOMPARE(metrics.boundingRect(), expected.boundingRect(text));
    QCOMPARE(metrics.width(), expected.boundingRect(text).width());
    QCOMPARE(metrics.height(), expected.boundingRect(text).height());
    QCOMPARE(metrics.tightBoundingRect(), expected.tightBoundingRect(text));
}

QTEST_MAIN(tst_QQuickTextMetrics)

#include "tst_qquicktextmetrics.moc"

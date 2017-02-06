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

#include <QtCore/QString>
#include <QtTest/QtTest>

#include <QtLocation/QPlaceSearchSuggestionReply>

QT_USE_NAMESPACE

class SuggestionReply : public QPlaceSearchSuggestionReply
{
    Q_OBJECT
public:
    SuggestionReply(QObject *parent) : QPlaceSearchSuggestionReply(parent){}

    void setSuggestions(const QStringList &suggestions) {
        QPlaceSearchSuggestionReply::setSuggestions(suggestions);
    }
};

class tst_QPlaceSearchSuggestionReply : public QObject
{
    Q_OBJECT

public:
    tst_QPlaceSearchSuggestionReply();

private Q_SLOTS:
    void constructorTest();
    void typeTest();
    void suggestionsTest();
};

tst_QPlaceSearchSuggestionReply::tst_QPlaceSearchSuggestionReply()
{
}

void tst_QPlaceSearchSuggestionReply::constructorTest()
{
    SuggestionReply *reply = new SuggestionReply(this);
    QCOMPARE(reply->parent(), this);

    delete reply;
}

void tst_QPlaceSearchSuggestionReply::typeTest()
{
    SuggestionReply *reply = new SuggestionReply(this);
    QCOMPARE(reply->type(), QPlaceReply::SearchSuggestionReply);

    delete reply;
}

void tst_QPlaceSearchSuggestionReply::suggestionsTest()
{
    QStringList suggestions;
    suggestions << QStringLiteral("one") << QStringLiteral("two")
                << QStringLiteral("three");

    SuggestionReply *reply = new SuggestionReply(this);
    reply->setSuggestions(suggestions);
    QCOMPARE(reply->suggestions(), suggestions);

    delete reply;
}

QTEST_APPLESS_MAIN(tst_QPlaceSearchSuggestionReply)

#include "tst_qplacesearchsuggestionreply.moc"

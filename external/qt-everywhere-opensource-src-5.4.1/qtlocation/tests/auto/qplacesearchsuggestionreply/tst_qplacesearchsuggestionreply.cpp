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

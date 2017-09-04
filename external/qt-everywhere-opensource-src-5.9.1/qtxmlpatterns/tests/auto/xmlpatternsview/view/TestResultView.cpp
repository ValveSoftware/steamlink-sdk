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

#include <QHeaderView>

#include "ASTItem.h"
#include "ErrorItem.h"
#include "TestCase.h"
#include "TestResult.h"
#include "TreeModel.h"
#include "XDTItemItem.h"

#include "TestResultView.h"

using namespace QPatternistSDK;

TestResultView::TestResultView(QWidget *const p) : QDockWidget(QLatin1String("Test Result View"), p)
{
    Q_ASSERT(p);
    setObjectName(QLatin1String("TestResultView"));
    setWidget(new QWidget());
    setupUi(widget());

    QStringList astColumns;
    astColumns << QLatin1String("Node Name")
               << QLatin1String("Details")
               << QLatin1String("Static Type")
               << QLatin1String("Required Type");
    astView->setModel(new TreeModel(astColumns, this));

    QStringList itemColumns;
    itemColumns << QLatin1String("#")
                << QLatin1String("Value")
                << QLatin1String("Type");
    itemListResult->setModel(new TreeModel(itemColumns, this));

    QStringList errColumns;
    errColumns << QLatin1String("Severity")
               << QLatin1String("Error Code")
               << QLatin1String("Message");
    messageOutput->setModel(new TreeModel(errColumns, this));
    messageOutput->horizontalHeader()->setStretchLastSection(true);
}

void TestResultView::displayTestResult(const TestResult *const result)
{
    if(!result)
    {
        displayNone();
        return;
    }

    /* ------- the Test Status Label --------- */
    resultStatus->setText(result->status() ? TestResult::displayName(result->status())
                                           : QLatin1String("Not Applicable"));
    /* --------------------------------------- */

    /* ------------ the AST View ------------- */
    ASTItem *astTree = result->astTree();
    static_cast<TreeModel *>(astView->model())->setRoot(astTree);
    /* --------------------------------------- */

    /* ------- the Error code/msg View ------- */
    ErrorItem *msgRoot = new ErrorItem(ErrorHandler::Message(), 0);

    const ErrorHandler::Message::List msgs(result->messages());
    ErrorHandler::Message::List::const_iterator it(msgs.constBegin());
    const ErrorHandler::Message::List::const_iterator end(msgs.constEnd());

    for(; it != end; ++it)
        msgRoot->appendChild(new ErrorItem(*it, msgRoot));

    TreeModel *etm = static_cast<TreeModel *>(messageOutput->model());
    etm->setRoot(msgRoot);
    /* --------------------------------------- */

    const QPatternist::Item::List items(result->items());
    /* ----- the Serialized Output View ------ */
    serializedResult->setPlainText(result->asSerialized());
    /* --------------------------------------- */

    /* ------ the Item List Output View ------ */
    XDTItemItem *itemRoot = new XDTItemItem(QPatternist::Item(), 0);
    QPatternist::Item item;

    QPatternist::Item::List::const_iterator itemIt(items.constBegin());
    const QPatternist::Item::List::const_iterator itemsEnd(items.constEnd());

    for(; itemIt != itemsEnd; ++itemIt)
        itemRoot->appendChild(new XDTItemItem(*itemIt, itemRoot));

    TreeModel *itm = static_cast<TreeModel *>(itemListResult->model());
    itm->setRoot(itemRoot);
    /* --------------------------------------- */
}

void TestResultView::displayTestResult(TestCase *const tc)
{
    if(tc)
        displayTestResult(tc->testResult());
    else
        displayNone();
}

void TestResultView::displayNone()
{
    TreeModel *tm;

    tm = static_cast<TreeModel *>(astView->model());
    Q_ASSERT(tm);
    tm->setRoot(0);

    tm = static_cast<TreeModel *>(messageOutput->model());
    Q_ASSERT(tm);
    tm->setRoot(0);

    tm = static_cast<TreeModel *>(itemListResult->model());
    Q_ASSERT(tm);
    tm->setRoot(0);

    serializedResult->clear();
    resultStatus->clear();
}

// vim: et:ts=4:sw=4:sts=4

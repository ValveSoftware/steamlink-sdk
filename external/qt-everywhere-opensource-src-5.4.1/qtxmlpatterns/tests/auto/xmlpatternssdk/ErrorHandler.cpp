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

#include <cstdio>

#include <QBuffer>
#include <QStringList>
#include <QtDebug>
#include <QtTest>
#include <QtGlobal>
#include <QXmlQuery>
#include <QXmlResultItems>

#include "ErrorHandler.h"

using namespace QPatternistSDK;

ErrorHandler *ErrorHandler::handler = 0;

void qMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &description)
{
    if(type == QtDebugMsg)
    {
        std::fprintf(stderr, "%s\n", qPrintable(description));
        return;
    }

    QtMsgType t;

    switch(type)
    {
        case QtWarningMsg:
        {
            t = QtWarningMsg;
            break;
        }
        case QtCriticalMsg:
        {
            t = QtFatalMsg;
            break;
        }
        case QtFatalMsg:
        {
            /* We can't swallow Q_ASSERTs, we need to fail the hard way here.
             * But maybe not: when run from "patternistrunsingle" it could be an idea
             * to actually try to record it(but nevertheless fail somehow) such
             * that it gets reported. */
            std::fprintf(stderr, "Fatal error: %s\n", qPrintable(description));
            t = QtFatalMsg; /* Dummy, to silence a bogus compiler warning. */
            return;
        }
        case QtDebugMsg: /* This enum is handled above in the if-clause. */
        /* Fallthrough. */
        default:
        {
            Q_ASSERT(false);
            return;
        }
    }

    Q_ASSERT(ErrorHandler::handler);
    /* This message is hacky. Ideally, we should do it the same way
     * ReportContext::error() constructs messages, but this is just testing
     * code. */
    ErrorHandler::handler->message(t, QLatin1String("<p>") + QPatternist::escape(description) + QLatin1String("</p>"));
}

void ErrorHandler::installQtMessageHandler(ErrorHandler *const h)
{
    handler = h;

    if(h)
        qInstallMessageHandler(qMessageHandler);
    else
        qInstallMessageHandler(0);
}

void ErrorHandler::handleMessage(QtMsgType type,
                                 const QString &description,
                                 const QUrl &identifier,
                                 const QSourceLocation &)
{
    /* Don't use pDebug() in this function, it results in infinite
     * recursion. Talking from experience.. */

    Message msg;
    msg.setType(type);
    msg.setIdentifier(identifier);

    /* Let's remove all the XHTML markup. */
    QBuffer buffer;
    buffer.setData(description.toLatin1());
    buffer.open(QIODevice::ReadOnly);

    QXmlQuery query;
    query.bindVariable(QLatin1String("desc"), &buffer);
    query.setQuery(QLatin1String("string(doc($desc))"));

    QStringList result;
    const bool success = query.evaluateTo(&result);

    if(!description.startsWith(QLatin1String("\"Test-suite harness error:")))
    {
        const QString msg(QString::fromLatin1("Invalid description: %1").arg(description));
        QVERIFY2(success, qPrintable(msg));

        if(!success)
            QTextStream(stderr) << msg;
    }


    if(!result.isEmpty())
        msg.setDescription(result.first());

    m_messages.append(msg);
}

ErrorHandler::Message::List ErrorHandler::messages() const
{
    return m_messages;
}

void ErrorHandler::reset()
{
    m_messages.clear();
}

// vim: et:ts=4:sw=4:sts=4

/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSCriptTools module of the Qt Toolkit.
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

#include "qscriptstdmessagehandler_p.h"
#include <stdio.h>

QT_BEGIN_NAMESPACE

/*!
  \since 4.5
  \class QScriptStdMessageHandler
  \internal

  \brief The QScriptStdMessageHandler class implements a message handler that writes to standard output.
*/

class QScriptStdMessageHandlerPrivate
{
public:
    QScriptStdMessageHandlerPrivate() {}
    ~QScriptStdMessageHandlerPrivate() {}
};

QScriptStdMessageHandler::QScriptStdMessageHandler()
    : d_ptr(new QScriptStdMessageHandlerPrivate)
{
}

QScriptStdMessageHandler::~QScriptStdMessageHandler()
{
}

void QScriptStdMessageHandler::message(QtMsgType type, const QString &text,
                                       const QString &fileName,
                                       int lineNumber, int columnNumber,
                                       const QVariant &/*data*/)
{
    QString msg;
    if (!fileName.isEmpty() || (lineNumber != -1)) {
        if (!fileName.isEmpty())
            msg.append(fileName);
        else
            msg.append(QLatin1String("<noname>"));
        if (lineNumber != -1) {
            msg.append(QLatin1Char(':'));
            msg.append(QString::number(lineNumber));
            if (columnNumber != -1) {
                msg.append(QLatin1Char(':'));
                msg.append(QString::number(columnNumber));
            }
        }
        msg.append(QLatin1String(": "));
    }
    msg.append(text);

    FILE *fp = (type == QtDebugMsg) ? stdout : stderr;
    fprintf(fp, "%s\n", msg.toLatin1().constData());
    fflush(fp);
}

QT_END_NAMESPACE

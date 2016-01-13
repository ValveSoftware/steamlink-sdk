/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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

#include "stdinlistener.h"

#include "tracer.h"

QT_BEGIN_NAMESPACE

StdInListener::StdInListener(QObject *parent)
    : QSocketNotifier(fileno(stdin), QSocketNotifier::Read, parent)
{
    TRACE_OBJ
    connect(this, SIGNAL(activated(int)), this, SLOT(receivedData()));
}

StdInListener::~StdInListener()
{
    TRACE_OBJ
}

void StdInListener::start()
{
    setEnabled(true);
}

void StdInListener::receivedData()
{
    TRACE_OBJ
    QByteArray ba;
    while (true) {
        const int c = getc(stdin);
        if (c == EOF) {
            setEnabled(false);
            break;
        }
        if (c == '\0')
            break;
        if (c)
            ba.append(char(c));
         if (c == '\n')
             break;
    }
    emit receivedCommand(QString::fromLocal8Bit(ba));
}

QT_END_NAMESPACE

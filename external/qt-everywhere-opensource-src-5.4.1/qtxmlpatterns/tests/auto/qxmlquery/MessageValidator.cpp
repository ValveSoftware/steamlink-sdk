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

#include <QXmlStreamReader>

#include "MessageValidator.h"

MessageValidator::MessageValidator() : m_success(false)
                                     , m_hasChecked(false)
{
}

MessageValidator::~MessageValidator()
{
    if (!m_hasChecked)
        qFatal("%s: You must call success().", Q_FUNC_INFO);
}

void MessageValidator::handleMessage(QtMsgType type,
                                     const QString &description,
                                     const QUrl &identifier,
                                     const QSourceLocation &sourceLocation)
{
    Q_UNUSED(type);
    Q_UNUSED(description);
    Q_UNUSED(sourceLocation);
    Q_UNUSED(identifier);

    QXmlStreamReader reader(description);

    m_received =   QLatin1String("Type:")
                 + QString::number(type)
                 + QLatin1String("\nDescription: ")
                 + description
                 + QLatin1String("\nIdentifier: ")
                 + identifier.toString()
                 + QLatin1String("\nLocation: ")
                 + sourceLocation.uri().toString()
                 + QLatin1String("#")
                 + QString::number(sourceLocation.line())
                 + QLatin1String(",")
                 + QString::number(sourceLocation.column());

    /* We just walk through it, to check that it's valid. */
    while(!reader.atEnd())
        reader.readNext();

    m_success = !reader.hasError();
}

bool MessageValidator::success()
{
    m_hasChecked = true;
    return m_success;
}

QString MessageValidator::received() const
{
    return m_received;
}

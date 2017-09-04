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

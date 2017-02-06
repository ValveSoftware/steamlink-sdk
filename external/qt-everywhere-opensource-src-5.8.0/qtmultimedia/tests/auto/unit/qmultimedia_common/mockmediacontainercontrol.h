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

#ifndef MOCKMEDIACONTAINERCONTROL_H
#define MOCKMEDIACONTAINERCONTROL_H

#include <QObject>
#include "qmediacontainercontrol.h"
#include <QMap>
#include <QString>
#include <QStringList>

QT_USE_NAMESPACE
class MockMediaContainerControl : public QMediaContainerControl
{
    Q_OBJECT
public:
    MockMediaContainerControl(QObject *parent):
        QMediaContainerControl(parent)
    {
        m_supportedContainers.append("wav");
        m_supportedContainers.append("mp3");
        m_supportedContainers.append("mov");

        m_descriptions.insert("wav", "WAV format");
        m_descriptions.insert("mp3", "MP3 format");
        m_descriptions.insert("mov", "MOV format");
    }

    virtual ~MockMediaContainerControl() {};

    QStringList supportedContainers() const
    {
        return m_supportedContainers;
    }

    QString containerFormat() const
    {
        return m_format;
    }

    void setContainerFormat(const QString &formatMimeType)
    {
        if (m_supportedContainers.contains(formatMimeType))
            m_format = formatMimeType;
    }

    QString containerDescription(const QString &formatMimeType) const
    {
        return m_descriptions.value(formatMimeType);
    }

private:
    QStringList m_supportedContainers;
    QMap<QString, QString> m_descriptions;
    QString m_format;
};

#endif // MOCKMEDIACONTAINERCONTROL_H

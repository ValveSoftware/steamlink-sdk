/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwaylanddataoffer_p.h"
#include "qwaylanddatadevicemanager_p.h"
#include "qwaylanddisplay_p.h"

#include <QtCore/private/qcore_unix_p.h>
#include <QtGui/private/qguiapplication_p.h>
#include <qpa/qplatformclipboard.h>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

QWaylandDataOffer::QWaylandDataOffer(QWaylandDisplay *display, struct ::wl_data_offer *offer)
    : QtWayland::wl_data_offer(offer)
    , m_mimeData(new QWaylandMimeData(this, display))
{
}

QWaylandDataOffer::~QWaylandDataOffer()
{
    destroy();
}


QString QWaylandDataOffer::firstFormat() const
{
    if (m_mimeData->formats().isEmpty())
        return QString();

    return m_mimeData->formats().first();
}

QMimeData *QWaylandDataOffer::mimeData()
{
    return m_mimeData.data();
}

void QWaylandDataOffer::data_offer_offer(const QString &mime_type)
{
    m_mimeData->appendFormat(mime_type);
}

QWaylandMimeData::QWaylandMimeData(QWaylandDataOffer *dataOffer, QWaylandDisplay *display)
    : QInternalMimeData()
    , m_dataOffer(dataOffer)
    , m_display(display)
{
}

QWaylandMimeData::~QWaylandMimeData()
{
}

void QWaylandMimeData::appendFormat(const QString &mimeType)
{
    m_types << mimeType;
    m_data.remove(mimeType); // Clear previous contents
}

bool QWaylandMimeData::hasFormat_sys(const QString &mimeType) const
{
    return m_types.contains(mimeType);
}

QStringList QWaylandMimeData::formats_sys() const
{
    return m_types;
}

QVariant QWaylandMimeData::retrieveData_sys(const QString &mimeType, QVariant::Type type) const
{
    Q_UNUSED(type);

    if (m_data.contains(mimeType))
        return m_data.value(mimeType);

    if (!m_types.contains(mimeType))
        return QVariant();

    int pipefd[2];
    if (::pipe2(pipefd, O_CLOEXEC|O_NONBLOCK) == -1) {
        qWarning("QWaylandMimeData: pipe2() failed");
        return QVariant();
    }

    m_dataOffer->receive(mimeType, pipefd[1]);
    m_display->flushRequests();

    close(pipefd[1]);

    QByteArray content;
    if (readData(pipefd[0], content) != 0) {
        qWarning("QWaylandDataOffer: error reading data for mimeType %s", qPrintable(mimeType));
        content = QByteArray();
    }

    close(pipefd[0]);
    m_data.insert(mimeType, content);
    return content;
}

int QWaylandMimeData::readData(int fd, QByteArray &data) const
{
    char buf[4096];
    int retryCount = 0;
    int n;
    while (true) {
        n = QT_READ(fd, buf, sizeof buf);
        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK) && ++retryCount < 1000)
            usleep(1000);
        else
            break;
    }
    if (retryCount >= 1000)
        qWarning("QWaylandDataOffer: timeout reading from pipe");
    if (n > 0) {
        data.append(buf, n);
        n = readData(fd, data);
    }
    return n;
}

QT_END_NAMESPACE

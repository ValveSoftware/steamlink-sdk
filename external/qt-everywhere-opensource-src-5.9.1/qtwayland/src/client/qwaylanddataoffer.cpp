/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
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

namespace QtWaylandClient {

static QString utf8Text()
{
    return QStringLiteral("text/plain;charset=utf-8");
}

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
    if (m_types.contains(mimeType))
        return true;

    if (mimeType == QStringLiteral("text/plain") && m_types.contains(utf8Text()))
        return true;

    return false;
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

    QString mime = mimeType;

    if (!m_types.contains(mimeType)) {
        if (mimeType == QStringLiteral("text/plain") && m_types.contains(utf8Text()))
            mime = utf8Text();
        else
            return QVariant();
    }

    int pipefd[2];
    if (::pipe2(pipefd, O_CLOEXEC|O_NONBLOCK) == -1) {
        qWarning("QWaylandMimeData: pipe2() failed");
        return QVariant();
    }

    m_dataOffer->receive(mime, pipefd[1]);
    wl_display_flush(m_display->wl_display());

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

}

QT_END_NAMESPACE

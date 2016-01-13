/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#include "qnearfieldtarget_emulator_p.h"
#include "qnearfieldtarget_p.h"

#include <QtCore/QDirIterator>
#include <QtCore/QSettings>
#include <QtCore/QMutex>
#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>

QT_BEGIN_NAMESPACE

static QMutex tagMutex;
static QMap<TagBase *, bool> tagMap;
static TagActivator tagActivator;

TagType1::TagType1(TagBase *tag, QObject *parent)
:   QNearFieldTagType1(parent), m_tag(tag)
{
}

TagType1::~TagType1()
{
}

QByteArray TagType1::uid() const
{
    QMutexLocker locker(&tagMutex);

    return m_tag->uid();
}

QNearFieldTarget::AccessMethods TagType1::accessMethods() const
{
    return NdefAccess | TagTypeSpecificAccess;
}

QNearFieldTarget::RequestId TagType1::sendCommand(const QByteArray &command)
{
    QMutexLocker locker(&tagMutex);

    RequestId id(new RequestIdPrivate);

    // tag not in proximity
    if (!tagMap.value(m_tag)) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, TargetOutOfRangeError),
                                  Q_ARG(QNearFieldTarget::RequestId, id));
        return id;
    }

    quint16 crc = qNfcChecksum(command.constData(), command.length());

    QByteArray response = m_tag->processCommand(command + char(crc & 0xff) + char(crc >> 8));

    if (response.isEmpty()) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, NoResponseError),
                                  Q_ARG(QNearFieldTarget::RequestId, id));
        return id;
    }

    // check crc
    if (qNfcChecksum(response.constData(), response.length()) != 0) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, ChecksumMismatchError),
                                  Q_ARG(QNearFieldTarget::RequestId, id));
        return id;
    }

    response.chop(2);

    QMetaObject::invokeMethod(this, "handleResponse", Qt::QueuedConnection,
                              Q_ARG(QNearFieldTarget::RequestId, id), Q_ARG(QByteArray, response));

    return id;
}

bool TagType1::waitForRequestCompleted(const RequestId &id, int msecs)
{
    QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);

    return QNearFieldTagType1::waitForRequestCompleted(id, msecs);
}


TagType2::TagType2(TagBase *tag, QObject *parent)
:   QNearFieldTagType2(parent), m_tag(tag)
{
}

TagType2::~TagType2()
{
}

QByteArray TagType2::uid() const
{
    QMutexLocker locker(&tagMutex);

    return m_tag->uid();
}

QNearFieldTarget::AccessMethods TagType2::accessMethods() const
{
    return NdefAccess | TagTypeSpecificAccess;
}

QNearFieldTarget::RequestId TagType2::sendCommand(const QByteArray &command)
{
    QMutexLocker locker(&tagMutex);

    RequestId id(new RequestIdPrivate);

    // tag not in proximity
    if (!tagMap.value(m_tag)) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QNearFieldTarget::Error, TargetOutOfRangeError),
                                  Q_ARG(QNearFieldTarget::RequestId, id));
        return id;
    }

    quint16 crc = qNfcChecksum(command.constData(), command.length());

    QByteArray response = m_tag->processCommand(command + char(crc & 0xff) + char(crc >> 8));

    if (response.isEmpty())
        return id;

    if (response.length() > 1) {
        // check crc
        if (qNfcChecksum(response.constData(), response.length()) != 0) {
            QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                      Q_ARG(QNearFieldTarget::Error, ChecksumMismatchError),
                                      Q_ARG(QNearFieldTarget::RequestId, id));
            return id;
        }

        response.chop(2);
    }

    QMetaObject::invokeMethod(this, "handleResponse", Qt::QueuedConnection,
                              Q_ARG(QNearFieldTarget::RequestId, id), Q_ARG(QByteArray, response));

    return id;
}

bool TagType2::waitForRequestCompleted(const RequestId &id, int msecs)
{
    QCoreApplication::sendPostedEvents(this, QEvent::MetaCall);

    return QNearFieldTagType2::waitForRequestCompleted(id, msecs);
}


TagActivator::TagActivator()
:   timerId(-1)
{
    qRegisterMetaType<QNearFieldTarget::Error>("QNearFieldTarget::Error");
}

TagActivator::~TagActivator()
{
    QMutexLocker locker(&tagMutex);
    qDeleteAll(tagMap.keys());
    tagMap.clear();
}

void TagActivator::initialize()
{
    QMutexLocker locker(&tagMutex);

    if (!tagMap.isEmpty())
        return;

    QDirIterator nfcTargets(QDir::currentPath(), QStringList(QStringLiteral("*.nfc")), QDir::Files);
    while (nfcTargets.hasNext()) {
        const QString targetFilename = nfcTargets.next();

        QSettings target(targetFilename, QSettings::IniFormat);

        target.beginGroup(QStringLiteral("Target"));

        const QString tagType = target.value(QStringLiteral("Type")).toString();

        target.endGroup();

        if (tagType == QLatin1String("TagType1")) {
            NfcTagType1 *tag = new NfcTagType1;
            tag->load(&target);

            tagMap.insert(tag, false);
        } else if (tagType == QLatin1String("TagType2")) {
            NfcTagType2 *tag = new NfcTagType2;
            tag->load(&target);

            tagMap.insert(tag, false);
        } else {
            qWarning("Unknown tag type %s\n", qPrintable(tagType));
        }
    }

    m_current = tagMap.end();

    timerId = startTimer(1000);
}

void TagActivator::reset()
{
    QMutexLocker locker(&tagMutex);

    killTimer(timerId);
    timerId = -1;

    qDeleteAll(tagMap.keys());
    tagMap.clear();
}

TagActivator *TagActivator::instance()
{
    return &tagActivator;
}

void TagActivator::timerEvent(QTimerEvent *e)
{
    Q_UNUSED(e);

    tagMutex.lock();

    if (m_current != tagMap.end()) {
        if (m_current.key()->lastAccessTime() + 1500 > QDateTime::currentMSecsSinceEpoch()) {
            tagMutex.unlock();
            return;
        }

        *m_current = false;

        TagBase *tag = m_current.key();

        tagMutex.unlock();
        emit tagDeactivated(tag);
        tagMutex.lock();
    }

    if (m_current != tagMap.end())
        ++m_current;

    if (m_current == tagMap.end())
        m_current = tagMap.begin();

    if (m_current != tagMap.end()) {
        *m_current = true;

        TagBase *tag = m_current.key();

        tagMutex.unlock();
        emit tagActivated(tag);
        tagMutex.lock();
    }

    tagMutex.unlock();
}

QT_END_NAMESPACE

/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "stringtoint_p.h"
#include <QMutex>
#include <QReadWriteLock>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

namespace {

QMutex mutex;
QReadWriteLock readLock;

} // anonymous

QVector<QString> StringToInt::m_stringsArray = QVector<QString>();
QVector<QString> StringToInt::m_pendingStringsArray = QVector<QString>();
int StringToInt::m_calls = 0;

int StringToInt::lookupId(QLatin1String str)
{
    // ### optimize me
    return lookupId(QString(str));
}

int StringToInt::lookupId(const QString &str)
{
    // Note: how do we protect against the case where
    // we are synching the two arrays ?
    QReadLocker readLocker(&readLock);
    int idx = StringToInt::m_stringsArray.indexOf(str);

    if (Q_UNLIKELY(idx < 0)) {
        QMutexLocker lock(&mutex);

        // If not found in m_stringsArray, maybe it's in the pending array
        if ((idx = StringToInt::m_pendingStringsArray.indexOf(str)) >= 0) {
            idx += StringToInt::m_stringsArray.size();
        } else {
            // If not, we add it to the m_pendingStringArray
            StringToInt::m_pendingStringsArray.push_back(str);
            idx = StringToInt::m_stringsArray.size() + m_pendingStringsArray.size() - 1;
        }

        // We sync the two arrays every 20 calls
        if (StringToInt::m_calls % 20 == 0 && StringToInt::m_pendingStringsArray.size() > 0) {
            // Unlock reader to writeLock
            // since a read lock cannot be locked for writing
            lock.unlock();
            readLocker.unlock();

            QWriteLocker writeLock(&readLock);
            lock.relock();

            StringToInt::m_stringsArray += StringToInt::m_pendingStringsArray;
            StringToInt::m_pendingStringsArray.clear();
            StringToInt::m_calls = 0;
        }
    }
    return idx;
}

QString StringToInt::lookupString(int idx)
{
    QReadLocker readLocker(&readLock);
    if (Q_LIKELY(StringToInt::m_stringsArray.size() > idx))
        return StringToInt::m_stringsArray.at(idx);

    // Maybe it's in the pending array then
    QMutexLocker lock(&mutex);
    if (StringToInt::m_stringsArray.size() + StringToInt::m_pendingStringsArray.size() > idx)
        return StringToInt::m_pendingStringsArray.at(idx - StringToInt::m_stringsArray.size());

    return QString();
}

} // Render

} // Qt3DRender

QT_END_NAMESPACE

/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#include "qtremoteobjectglobal.h"

#include <QDataStream>
#include <QMetaObject>
#include <QMetaProperty>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(QT_REMOTEOBJECT, "qt.remoteobjects", QtWarningMsg)
Q_LOGGING_CATEGORY(QT_REMOTEOBJECT_MODELS, "qt.remoteobjects.models", QtWarningMsg)
Q_LOGGING_CATEGORY(QT_REMOTEOBJECT_IO, "qt.remoteobjects.io", QtWarningMsg)

namespace QtRemoteObjects {

void copyStoredProperties(const QMetaObject *mo, const void *src, void *dst)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
    if (!src) {
        qCWarning(QT_REMOTEOBJECT) << Q_FUNC_INFO << ": trying to copy from a null source";
        return;
    }
    if (!dst) {
        qCWarning(QT_REMOTEOBJECT) << Q_FUNC_INFO << ": trying to copy to a null destination";
        return;
    }

    for (int i = 0, end = mo->propertyCount(); i != end; ++i) {
        const QMetaProperty mp = mo->property(i);
        mp.writeOnGadget(dst, mp.readOnGadget(src));
    }
#else
    Q_UNUSED(mo);
    Q_UNUSED(src);
    Q_UNUSED(dst);
#endif
}

void copyStoredProperties(const QMetaObject *mo, const void *src, QDataStream &dst)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
    if (!src) {
        qCWarning(QT_REMOTEOBJECT) << Q_FUNC_INFO << ": trying to copy from a null source";
        return;
    }

    for (int i = 0, end = mo->propertyCount(); i != end; ++i) {
        const QMetaProperty mp = mo->property(i);
        dst << mp.readOnGadget(src);
    }
#else
    Q_UNUSED(mo);
    Q_UNUSED(src);
    Q_UNUSED(dst);
#endif
}

void copyStoredProperties(const QMetaObject *mo, QDataStream &src, void *dst)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
    if (!dst) {
        qCWarning(QT_REMOTEOBJECT) << Q_FUNC_INFO << ": trying to copy to a null destination";
        return;
    }

    for (int i = 0, end = mo->propertyCount(); i != end; ++i) {
        const QMetaProperty mp = mo->property(i);
        QVariant v;
        src >> v;
        mp.writeOnGadget(dst, v);
    }
#else
    Q_UNUSED(mo);
    Q_UNUSED(src);
    Q_UNUSED(dst);
#endif
}

} // namespace QtRemoteObjects

QT_END_NAMESPACE

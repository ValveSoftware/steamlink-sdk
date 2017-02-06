/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#include "qnearfieldmanager_emulator_p.h"
#include "qnearfieldtarget_emulator_p.h"

#include "qndefmessage.h"
#include "qtlv_p.h"

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

QNearFieldManagerPrivateImpl::QNearFieldManagerPrivateImpl()
{
    TagActivator *tagActivator = TagActivator::instance();

    tagActivator->initialize();

    connect(tagActivator, SIGNAL(tagActivated(TagBase*)), this, SLOT(tagActivated(TagBase*)));
    connect(tagActivator, SIGNAL(tagDeactivated(TagBase*)), this, SLOT(tagDeactivated(TagBase*)));
}

QNearFieldManagerPrivateImpl::~QNearFieldManagerPrivateImpl()
{
}

bool QNearFieldManagerPrivateImpl::isAvailable() const
{
    return true;
}

void QNearFieldManagerPrivateImpl::reset()
{
    TagActivator::instance()->reset();
}

void QNearFieldManagerPrivateImpl::tagActivated(TagBase *tag)
{
    QNearFieldTarget *target = m_targets.value(tag).data();
    if (!target) {
        if (dynamic_cast<NfcTagType1 *>(tag))
            target = new TagType1(tag, this);
        else if (dynamic_cast<NfcTagType2 *>(tag))
            target = new TagType2(tag, this);
        else
            qFatal("Unknown emulator tag type");

        m_targets.insert(tag, target);
    }

    targetActivated(target);
}

void QNearFieldManagerPrivateImpl::tagDeactivated(TagBase *tag)
{
    QNearFieldTarget *target = m_targets.value(tag).data();
    if (!target) {
        m_targets.remove(tag);
        return;
    }

    targetDeactivated(target);
}

QT_END_NAMESPACE

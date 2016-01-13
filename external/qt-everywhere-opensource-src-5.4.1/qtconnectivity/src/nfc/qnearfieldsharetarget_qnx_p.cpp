/***************************************************************************
 **
 ** Copyright (C) 2013 BlackBerry Limited. All rights reserved.
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

#include "qnearfieldsharetarget_qnx_p.h"
#include "qnearfieldsharemanager_p.h"
#include "qnx/qnxnfcsharemanager_p.h"

QT_BEGIN_NAMESPACE

using namespace bb::system;

QNearFieldShareTargetPrivateImpl::QNearFieldShareTargetPrivateImpl(QNearFieldShareManager::ShareModes modes, QNearFieldShareTarget *q)
:   QNearFieldShareTargetPrivate(modes, q), q_ptr(q), _error(QNearFieldShareManager::NoError)
{
    _manager = QNXNFCShareManager::instance();
    _manager->connect(this);
}

QNearFieldShareTargetPrivateImpl::~QNearFieldShareTargetPrivateImpl()
{
    _manager->disconnect(this);
}

QNearFieldShareManager::ShareModes QNearFieldShareTargetPrivateImpl::shareModes() const
{
    return QNXNFCShareManager::toShareModes(_manager->shareMode());
}

bool QNearFieldShareTargetPrivateImpl::share(const QNdefMessage &message)
{
    return _manager->shareNdef(message);
}

bool QNearFieldShareTargetPrivateImpl::share(const QList<QFileInfo> &files)
{
    return _manager->shareFiles(files);
}

void QNearFieldShareTargetPrivateImpl::cancel()
{
    _manager->cancel();
}

bool QNearFieldShareTargetPrivateImpl::isShareInProgress() const
{
    return QNXNFCShareManager::toShareModes(_manager->shareMode()) != QNearFieldShareManager::NoShare;
}

QNearFieldShareManager::ShareError QNearFieldShareTargetPrivateImpl::shareError() const
{
    return _error;
}

void QNearFieldShareTargetPrivateImpl::onShareModeChanged(NfcShareMode::Type mode)
{
    Q_UNUSED(mode)
}

void QNearFieldShareTargetPrivateImpl::onError(NfcShareError::Type error)
{
    _error = QNXNFCShareManager::toShareError(error);

    if (_error != QNearFieldShareManager::NoError) {
        emit q_ptr->error(_error);
    }
}

void QNearFieldShareTargetPrivateImpl::onFinished(NfcShareSuccess::Type result)
{
    Q_UNUSED(result)
    emit q_ptr->shareFinished();
}

void QNearFieldShareTargetPrivateImpl::onTargetAcquired()
{
}

void QNearFieldShareTargetPrivateImpl::onTargetCancelled()
{
}

QT_END_NAMESPACE

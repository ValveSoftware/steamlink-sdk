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

#include "qnearfieldsharemanager_qnx_p.h"
#include "qnearfieldsharetarget.h"
#include "qnx/qnxnfcsharemanager_p.h"

QT_BEGIN_NAMESPACE

using namespace bb::system;

QNearFieldShareManagerPrivateImpl::QNearFieldShareManagerPrivateImpl(QNearFieldShareManager* q)
    : QNearFieldShareManagerPrivate(q), q_ptr(q), _manager(0), _target(0), _shareError(QNearFieldShareManager::NoError)
{
    _manager = QNXNFCShareManager::instance();
    _manager->connect(this);
}

QNearFieldShareManagerPrivateImpl::~QNearFieldShareManagerPrivateImpl()
{
    if (_target) {
        delete _target;
    }

    _manager->disconnect(this);
}

void QNearFieldShareManagerPrivateImpl::onShareModeChanged(NfcShareMode::Type mode)
{
    switch (mode) {
        case NfcShareMode::DataSnep: {
            emit q_ptr->shareModesChanged(QNearFieldShareManager::NdefShare);
            break;
        }

        case NfcShareMode::File: {
            emit q_ptr->shareModesChanged(QNearFieldShareManager::FileShare);
            break;
        }

        case NfcShareMode::Disabled: {
            emit q_ptr->shareModesChanged(QNearFieldShareManager::NoShare);
            break;
        }

        default: {
        }
    }
}

void QNearFieldShareManagerPrivateImpl::onError(NfcShareError::Type error)
{
    _shareError = QNXNFCShareManager::toShareError(error);

    if (_shareError != QNearFieldShareManager::NoError)
        emit q_ptr->error(_shareError);
}

void QNearFieldShareManagerPrivateImpl::onFinished(NfcShareSuccess::Type result)
{
    Q_UNUSED(result)
}

void QNearFieldShareManagerPrivateImpl::onTargetAcquired()
{
    if (_target) {
        delete _target;
        _target = 0;
    }

    _target = new QNearFieldShareTarget(shareModes(), this);
    emit q_ptr->targetDetected(_target);
}

void QNearFieldShareManagerPrivateImpl::onTargetCancelled()
{
}

QNearFieldShareManager::ShareModes QNearFieldShareManagerPrivateImpl::supportedShareModes()
{
    return QNearFieldShareManager::NdefShare | QNearFieldShareManager::FileShare;
}

void QNearFieldShareManagerPrivateImpl::setShareModes(QNearFieldShareManager::ShareModes modes)
{
    _shareError = QNearFieldShareManager::NoError;

    if (modes == QNearFieldShareManager::NoShare) {
        _manager->setShareMode(NfcShareMode::Disabled);
        _manager->reset();
    }

    else {
        // TODO: Fix NfcShareManager to handle multiple share modes simultaneously
        if (modes.testFlag(QNearFieldShareManager::NdefShare))
            _manager->setShareMode(NfcShareMode::DataSnep);

        if (modes.testFlag(QNearFieldShareManager::FileShare))
            _manager->setShareMode(NfcShareMode::File);
    }
}

QNearFieldShareManager::ShareModes QNearFieldShareManagerPrivateImpl::shareModes() const
{
    return QNXNFCShareManager::toShareModes(_manager->shareMode());
}

QNearFieldShareManager::ShareError QNearFieldShareManagerPrivateImpl::shareError() const
{
    return _shareError;
}

QT_END_NAMESPACE

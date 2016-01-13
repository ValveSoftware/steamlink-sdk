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

#include "qnxnfcsharemanager_p.h"

QT_BEGIN_NAMESPACE

using namespace bb::system;

const char *QNXNFCShareManager::RECORD_NDEF = "application/vnd.rim.nfc.ndef";
Q_GLOBAL_STATIC(QNXNFCShareManager, shareStatic)

QNXNFCShareManager *QNXNFCShareManager::instance()
{
    return shareStatic();
}

bool QNXNFCShareManager::shareNdef(const QNdefMessage &message)
{
    bool ok = false;
    NfcShareDataContent content;
    content.setMimeType(RECORD_NDEF);
    content.setData(message.toByteArray());

    NfcShareSetContentError::Type err = _manager->setShareContent(content);

    switch (err) {
        case NfcShareSetContentError::None: {
            ok = true;
            break;
        }

        case NfcShareSetContentError::TransferInProgress: {
            emit error(NfcShareError::TransferInProgress);
            break;
        }

        case NfcShareSetContentError::InvalidShareMode:
        case NfcShareSetContentError::InvalidShareRequest: {
            emit error(NfcShareError::UnsupportedShareMode);
            break;
        }

        case NfcShareSetContentError::Unknown: {
            emit error(NfcShareError::Unknown);
            break;
        }
    }

    if (ok)
        _manager->startTransfer();

    return ok;
}

bool QNXNFCShareManager::shareFiles(const QList<QFileInfo> &files)
{
    bool ok = false;
    NfcShareFilesContent content;
    QList<QUrl> urls;

    for (int i=0; i<files.size(); i++) {
        urls.append(QUrl::fromLocalFile(files[i].filePath()));
    }

    content.setFileUrls(urls);

    NfcShareSetContentError::Type err = _manager->setShareContent(content);

    switch (err) {
        case NfcShareSetContentError::None: {
            ok = true;
            break;
        }

        case NfcShareSetContentError::TransferInProgress: {
            emit error(NfcShareError::TransferInProgress);
            break;
        }

        case NfcShareSetContentError::InvalidShareMode:
        case NfcShareSetContentError::InvalidShareRequest: {
            emit error(NfcShareError::UnsupportedShareMode);
            break;
        }

        case NfcShareSetContentError::Unknown: {
            emit error(NfcShareError::Unknown);
            break;
        }
    }

    if (ok)
        _manager->startTransfer();

    return ok;
}

void QNXNFCShareManager::cancel()
{
    _manager->cancelTarget();
}

void QNXNFCShareManager::setShareMode(NfcShareMode::Type type)
{
    _manager->setShareMode(type, NfcShareStartTransferMode::OnDemand);
}

NfcShareMode::Type QNXNFCShareManager::shareMode() const
{
    return _manager->shareMode();
}

void QNXNFCShareManager::connect(QObject *obj)
{
    QObject::connect(this, SIGNAL(shareModeChanged(bb::system::NfcShareMode::Type)),
                      obj, SLOT(onShareModeChanged(bb::system::NfcShareMode::Type)));
    QObject::connect(this, SIGNAL(error(bb::system::NfcShareError::Type)),
                      obj, SLOT(onError(bb::system::NfcShareError::Type)));
    QObject::connect(this, SIGNAL(finished(bb::system::NfcShareSuccess::Type)),
                      obj, SLOT(onFinished(bb::system::NfcShareSuccess::Type)));
    QObject::connect(this, SIGNAL(targetAcquired()),
                      obj, SLOT(onTargetAcquired()));
    QObject::connect(this, SIGNAL(targetCancelled()),
                      obj, SLOT(onTargetCancelled()));
}

void QNXNFCShareManager::disconnect(QObject *obj)
{
    QObject::disconnect(this, SIGNAL(shareModeChanged(bb::system::NfcShareMode::Type)),
                      obj, SLOT(onShareModeChanged(bb::system::NfcShareMode::Type)));
    QObject::disconnect(this, SIGNAL(error(bb::system::NfcShareError::Type)),
                      obj, SLOT(onError(bb::system::NfcShareError::Type)));
    QObject::disconnect(this, SIGNAL(finished(bb::system::NfcShareSuccess::Type)),
                      obj, SLOT(onFinished(bb::system::NfcShareSuccess::Type)));
    QObject::disconnect(this, SIGNAL(targetAcquired()),
                      obj, SLOT(onTargetAcquired()));
    QObject::disconnect(this, SIGNAL(targetCancelled()),
                      obj, SLOT(onTargetCancelled()));
}

void QNXNFCShareManager::reset()
{
    _manager->reset();
}

QNXNFCShareManager::QNXNFCShareManager()
    : QObject()
{
    _manager = new NfcShareManager(this);
    QObject::connect(_manager, SIGNAL(shareModeChanged(bb::system::NfcShareMode::Type)),
                         this, SIGNAL(shareModeChanged(bb::system::NfcShareMode::Type)));
    QObject::connect(_manager, SIGNAL(error(bb::system::NfcShareError::Type)),
                         this, SIGNAL(error(bb::system::NfcShareError::Type)));
    QObject::connect(_manager, SIGNAL(finished(bb::system::NfcShareSuccess::Type)),
                         this, SIGNAL(finished(bb::system::NfcShareSuccess::Type)));
    QObject::connect(_manager, SIGNAL(targetAcquired()),
                         this, SIGNAL(targetAcquired()));
    QObject::connect(_manager, SIGNAL(targetCancelled()),
                         this, SIGNAL(targetCancelled()));
}

QNXNFCShareManager::~QNXNFCShareManager()
{
    delete _manager;
}

QNearFieldShareManager::ShareError QNXNFCShareManager::toShareError(NfcShareError::Type nfcShareError)
{
    QNearFieldShareManager::ShareError shareError = QNearFieldShareManager::NoError;

    switch (nfcShareError) {
        case NfcShareError::Unknown: {
            shareError = QNearFieldShareManager::UnknownError;
            break;
        }

        case NfcShareError::NoContentToShare:
        case NfcShareError::MessageSize:
        case NfcShareError::TagLocked:
        case NfcShareError::UnsupportedTagType: {
            shareError = QNearFieldShareManager::InvalidShareContentError;
            break;
        }

        case NfcShareError::RegisterFileSharing:
        case NfcShareError::RegisterDataSharing: {
            shareError = QNearFieldShareManager::InvalidShareContentError;
            break;
        }

        case NfcShareError::DataTransferFailed:
        case NfcShareError::BluetoothFileTransferFailed:
        case NfcShareError::WiFiDirectFileTransferFailed:
        case NfcShareError::HandoverFailed: {
            shareError = QNearFieldShareManager::ShareInterruptedError;
            break;
        }

        case NfcShareError::BluetoothFileTransferCancelled:
        case NfcShareError::WiFiDirectFileTransferCancelled: {
            shareError = QNearFieldShareManager::ShareCanceledError;
            break;
        }

        case NfcShareError::TransferInProgress: {
            shareError = QNearFieldShareManager::ShareAlreadyInProgressError;
            break;
        }

        case NfcShareError::UnsupportedShareMode: {
            shareError = QNearFieldShareManager::UnsupportedShareModeError;
            break;
        }

        case NfcShareError::NoTransferTarget: {
            shareError = QNearFieldShareManager::ShareRejectedError;
            break;
        }

        default: {
            shareError = QNearFieldShareManager::UnknownError;
        }
    }

    return shareError;
}

QNearFieldShareManager::ShareModes QNXNFCShareManager::toShareModes(NfcShareMode::Type nfcShareMode)
{
    QNearFieldShareManager::ShareModes modes = QNearFieldShareManager::NoShare;

    if (nfcShareMode == NfcShareMode::DataSnep)
        modes |= QNearFieldShareManager::NdefShare;

    else if (nfcShareMode == NfcShareMode::File)
        modes |= QNearFieldShareManager::FileShare;

    return modes;
}

QT_END_NAMESPACE

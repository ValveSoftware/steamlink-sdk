/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtNfc module.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "annotatedurl.h"

#include <qnearfieldmanager.h>
#include <qnearfieldtarget.h>
#include <qndefmessage.h>
#include <qndefrecord.h>
#include <qndefnfctextrecord.h>
#include <qndefnfcurirecord.h>

#include <QtCore/QUrl>
#include <QtCore/QLocale>

#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QDesktopServices>
#include <QDebug>

AnnotatedUrl::AnnotatedUrl(QObject *parent)
:   QObject(parent)
{
    //! [QNearFieldManager register handler]
    manager = new QNearFieldManager(this);
    if (!manager->isAvailable()) {
        qWarning() << "NFC not available";
        return;
    }

    QNdefFilter filter;
    filter.setOrderMatch(false);
    filter.appendRecord<QNdefNfcTextRecord>(1, UINT_MAX);
    filter.appendRecord<QNdefNfcUriRecord>();
    // type parameter cannot specify substring so filter for "image/" below
    filter.appendRecord(QNdefRecord::Mime, QByteArray(), 0, 1);

    int result = manager->registerNdefMessageHandler(filter, this,
                                       SLOT(handleMessage(QNdefMessage,QNearFieldTarget*)));
    //! [QNearFieldManager register handler]

    if (result < 0)
        qWarning() << "Platform does not support NDEF message handler registration";

    manager->startTargetDetection();
    connect(manager, SIGNAL(targetDetected(QNearFieldTarget*)),
            this, SLOT(targetDetected(QNearFieldTarget*)));
    connect(manager, SIGNAL(targetLost(QNearFieldTarget*)),
            this, SLOT(targetLost(QNearFieldTarget*)));
}

AnnotatedUrl::~AnnotatedUrl()
{

}

void AnnotatedUrl::targetDetected(QNearFieldTarget *target)
{
    if (!target)
        return;

    connect(target, SIGNAL(ndefMessageRead(QNdefMessage)),
            this, SLOT(handlePolledNdefMessage(QNdefMessage)));
    target->readNdefMessages();
}

void AnnotatedUrl::targetLost(QNearFieldTarget *target)
{
    if (target)
        target->deleteLater();
}

void AnnotatedUrl::handlePolledNdefMessage(QNdefMessage message)
{
    QNearFieldTarget *target = qobject_cast<QNearFieldTarget *>(sender());
    handleMessage(message, target);
}

//! [handleMessage 1]
void AnnotatedUrl::handleMessage(const QNdefMessage &message, QNearFieldTarget *target)
{
//! [handleMessage 1]
    Q_UNUSED(target);

    enum {
        MatchedNone,
        MatchedFirst,
        MatchedEnglish,
        MatchedLanguage,
        MatchedLanguageAndCountry
    } bestMatch = MatchedNone;

    QLocale defaultLocale;

    QString title;
    QUrl url;
    QPixmap pixmap;

//! [handleMessage 2]
    foreach (const QNdefRecord &record, message) {
        if (record.isRecordType<QNdefNfcTextRecord>()) {
            QNdefNfcTextRecord textRecord(record);

            title = textRecord.text();
            QLocale locale(textRecord.locale());
//! [handleMessage 2]
            // already found best match
            if (bestMatch == MatchedLanguageAndCountry) {
                // do nothing
            } else if (bestMatch <= MatchedLanguage && locale == defaultLocale) {
                bestMatch = MatchedLanguageAndCountry;
            } else if (bestMatch <= MatchedEnglish &&
                       locale.language() == defaultLocale.language()) {
                bestMatch = MatchedLanguage;
            } else if (bestMatch <= MatchedFirst && locale.language() == QLocale::English) {
                bestMatch = MatchedEnglish;
            } else if (bestMatch == MatchedNone) {
                bestMatch = MatchedFirst;
            }
//! [handleMessage 3]
        } else if (record.isRecordType<QNdefNfcUriRecord>()) {
            QNdefNfcUriRecord uriRecord(record);

            url = uriRecord.uri();
//! [handleMessage 3]
        } else if (record.typeNameFormat() == QNdefRecord::Mime &&
                   record.type().startsWith("image/")) {
            pixmap = QPixmap::fromImage(QImage::fromData(record.payload()));
//! [handleMessage 4]
        }
    }

    emit annotatedUrl(url, title, pixmap);
}
//! [handleMessage 4]

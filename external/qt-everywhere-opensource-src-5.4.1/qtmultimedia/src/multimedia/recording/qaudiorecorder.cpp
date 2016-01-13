/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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

#include "qaudiorecorder.h"
#include "qaudioinputselectorcontrol.h"
#include "qmediaobject_p.h"
#include "qmediarecorder_p.h"
#include <qmediaservice.h>
#include <qmediaserviceprovider_p.h>

#include <QtCore/qdebug.h>
#include <QtCore/qurl.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qmetaobject.h>

#include <qaudioformat.h>

QT_BEGIN_NAMESPACE

/*!
    \class QAudioRecorder
    \inmodule QtMultimedia
    \ingroup multimedia
    \ingroup multimedia_recording

    \brief The QAudioRecorder class is used for the recording of audio.

    The QAudioRecorder class is a high level media recording class and contains
    the same functionality as \l QMediaRecorder.

    \snippet multimedia-snippets/media.cpp Audio recorder

    In addition QAudioRecorder provides functionality for selecting the audio input.

    \snippet multimedia-snippets/media.cpp Audio recorder inputs

    The \l {audiorecorder}{Audio Recorder} example shows how to use this class
    in more detail.

    \sa QMediaRecorder, QAudioInputSelectorControl
*/

class QAudioRecorderObject : public QMediaObject
{
public:
    QAudioRecorderObject(QObject *parent, QMediaService *service)
        :QMediaObject(parent, service)
    {
    }

    ~QAudioRecorderObject()
    {
    }
};

class QAudioRecorderPrivate : public QMediaRecorderPrivate
{
    Q_DECLARE_NON_CONST_PUBLIC(QAudioRecorder)

public:
    void initControls()
    {
        Q_Q(QAudioRecorder);
        audioInputSelector = 0;

        QMediaService *service = mediaObject ? mediaObject->service() : 0;

        if (service != 0)
            audioInputSelector = qobject_cast<QAudioInputSelectorControl*>(service->requestControl(QAudioInputSelectorControl_iid));

        if (audioInputSelector) {
            q->connect(audioInputSelector, SIGNAL(activeInputChanged(QString)),
                       SIGNAL(audioInputChanged(QString)));
            q->connect(audioInputSelector, SIGNAL(availableInputsChanged()),
                       SIGNAL(availableAudioInputsChanged()));
        }
    }

    QAudioRecorderPrivate():
        QMediaRecorderPrivate(),
        provider(0),
        audioInputSelector(0) {}

    QMediaServiceProvider *provider;
    QAudioInputSelectorControl   *audioInputSelector;
};



/*!
    Constructs an audio recorder.
    The \a parent is passed to QMediaObject.
*/

QAudioRecorder::QAudioRecorder(QObject *parent):
    QMediaRecorder(*new QAudioRecorderPrivate, 0, parent)
{
    Q_D(QAudioRecorder);
    d->provider = QMediaServiceProvider::defaultServiceProvider();

    QMediaService *service = d->provider->requestService(Q_MEDIASERVICE_AUDIOSOURCE);
    setMediaObject(new QAudioRecorderObject(this, service));
    d->initControls();
}

/*!
    Destroys an audio recorder object.
*/

QAudioRecorder::~QAudioRecorder()
{
    Q_D(QAudioRecorder);
    QMediaService *service = d->mediaObject ? d->mediaObject->service() : 0;
    QMediaObject *mediaObject = d->mediaObject;
    setMediaObject(0);

    if (service && d->audioInputSelector)
        service->releaseControl(d->audioInputSelector);

    if (d->provider && service)
        d->provider->releaseService(service);

    delete mediaObject;
}

/*!
    Returns a list of available audio inputs
*/

QStringList QAudioRecorder::audioInputs() const
{
    Q_D(const QAudioRecorder);
    if (d->audioInputSelector)
        return d->audioInputSelector->availableInputs();
    else
        return QStringList();
}

/*!
    Returns the readable translated description of the audio input device with \a name.
*/

QString QAudioRecorder::audioInputDescription(const QString& name) const
{
    Q_D(const QAudioRecorder);

    if (d->audioInputSelector)
        return d->audioInputSelector->inputDescription(name);
    else
        return QString();
}

/*!
    Returns the default audio input name.
*/

QString QAudioRecorder::defaultAudioInput() const
{
    Q_D(const QAudioRecorder);

    if (d->audioInputSelector)
        return d->audioInputSelector->defaultInput();
    else
        return QString();
}

/*!
    \property QAudioRecorder::audioInput
    \brief the active audio input name.

*/

/*!
    Returns the active audio input name.
*/

QString QAudioRecorder::audioInput() const
{
    Q_D(const QAudioRecorder);

    if (d->audioInputSelector)
        return d->audioInputSelector->activeInput();
    else
        return QString();
}

/*!
    Set the active audio input to \a name.
*/

void QAudioRecorder::setAudioInput(const QString& name)
{
    Q_D(const QAudioRecorder);

    if (d->audioInputSelector)
        return d->audioInputSelector->setActiveInput(name);
}

/*!
    \fn QAudioRecorder::audioInputChanged(const QString& name)

    Signal emitted when active audio input changes to \a name.
*/

/*!
    \fn QAudioRecorder::availableAudioInputsChanged()

    Signal is emitted when the available audio inputs change.
*/



#include "moc_qaudiorecorder.cpp"
QT_END_NAMESPACE


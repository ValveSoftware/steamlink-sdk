/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#ifndef QWASAPIUTILS_H
#define QWASAPIUTILS_H
#include "qwasapiaudiodeviceinfo.h"
#include "qwasapiaudioinput.h"
#include "qwasapiaudiooutput.h"
#include <QtCore/QLoggingCategory>
#include <QtCore/QThread>
#include <QtMultimedia/QAudio>
#include <QtMultimedia/QAudioFormat>

#include <wrl.h>
#include <Audioclient.h>
#include <mmdeviceapi.h>

struct IAudioClient;

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcMmAudioInterface)
Q_DECLARE_LOGGING_CATEGORY(lcMmUtils)

#define EMIT_RETURN_FALSE_IF_FAILED(msg, err) \
    if (FAILED(hr)) { \
      m_currentError = err; \
      emit errorChanged(m_currentError); \
    } \
    RETURN_FALSE_IF_FAILED(msg)

#define EMIT_RETURN_VOID_IF_FAILED(msg, err) \
    if (FAILED(hr)) { \
      m_currentError = err; \
      emit errorChanged(m_currentError); \
    } \
    RETURN_VOID_IF_FAILED(msg)

class AudioInterface : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags
        <Microsoft::WRL::Delegate>, Microsoft::WRL::FtmBase, IActivateAudioInterfaceCompletionHandler>
{
public:
    enum State {
        Initialized = 0,
        Activating,
        Activated,
        Error
    } ;

    explicit AudioInterface();

    ~AudioInterface();

    virtual HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation *op);

    inline State state() const { return m_currentState; }
    void setState(State s) { m_currentState = s; }

    Microsoft::WRL::ComPtr<IAudioClient> m_client;
    QWasapiAudioDeviceInfo *m_parent;
    State m_currentState;
    QAudioFormat m_mixFormat;
};

class QWasapiProcessThread : public QThread
{
public:
    explicit QWasapiProcessThread(QObject *item, bool output = true) : QThread(),
        m_endpoint(item),
        m_output(output)
    {
        qCDebug(lcMmUtils) << __FUNCTION__ << item;
    }

    ~QWasapiProcessThread()
    {
        qCDebug(lcMmUtils) << __FUNCTION__;
        CloseHandle(m_event);
    }

    void run()
    {
        qCDebug(lcMmUtils) << __FUNCTION__ << m_endpoint;
        if (m_output) {
            QWasapiAudioOutput *output = static_cast<QWasapiAudioOutput *>(m_endpoint);
            output->process();
        } else {
            QWasapiAudioInput *input = static_cast<QWasapiAudioInput *>(m_endpoint);
            input->process();
        }
    }

    HANDLE m_event;
private:
    QObject *m_endpoint;
    bool m_output;
};

namespace QWasapiUtils
{
    bool convertToNativeFormat(const QAudioFormat &qt, WAVEFORMATEX *native);
    bool convertFromNativeFormat(const WAVEFORMATEX *native, QAudioFormat *qt);

    QByteArray defaultDevice(QAudio::Mode mode);
    QList<QByteArray> availableDevices(QAudio::Mode mode);
    Microsoft::WRL::ComPtr<AudioInterface> createOrGetInterface(const QByteArray &dev, QAudio::Mode mode);
}

QT_END_NAMESPACE

#endif // QWASAPIUTILS_H

/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Speech module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qtexttospeech_winrt.h"

#include <QtCore/QCoreApplication>
#include <QtCore/qfunctions_winrt.h>
#include <QtCore/QMap>
#include <QtCore/QTimer>
#include <private/qeventdispatcher_winrt_p.h>

#include <windows.foundation.h>
#include <windows.foundation.collections.h>
#include <windows.media.speechsynthesis.h>
#include <windows.storage.streams.h>
#include <windows.ui.xaml.h>
#include <windows.ui.xaml.controls.h>
#include <windows.ui.xaml.markup.h>

#include <functional>
#include <wrl.h>

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Media::SpeechSynthesis;
using namespace ABI::Windows::Storage::Streams;
using namespace ABI::Windows::UI::Xaml;
using namespace ABI::Windows::UI::Xaml::Controls;
using namespace ABI::Windows::UI::Xaml::Markup;
using namespace ABI::Windows::UI::Xaml::Media;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

QT_BEGIN_NAMESPACE

#define LSTRING(str) L#str
static const wchar_t webviewXaml[] = LSTRING(
<MediaElement xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" />
);

class QTextToSpeechEngineWinRTPrivate
{
public:
    QTimer timer;
    ComPtr<IXamlReaderStatics> xamlReader;
    ComPtr<ISpeechSynthesizer> synth;
    QVector<QLocale> locales;
    QVector<QVoice> voices;
    QVector<ComPtr<IVoiceInformation>> infos;
    EventRegistrationToken tok;

    ComPtr<IMediaElement> media;

    double rate;
    double volume;

    QTextToSpeech::State state;
};

QTextToSpeechEngineWinRT::QTextToSpeechEngineWinRT(const QVariantMap &, QObject *parent)
    : QTextToSpeechEngine(parent)
    , d_ptr(new QTextToSpeechEngineWinRTPrivate)
{
    d_ptr->rate = 0;
    d_ptr->volume = 1.0;
    d_ptr->timer.setInterval(100);
    connect(&d_ptr->timer, &QTimer::timeout, this, &QTextToSpeechEngineWinRT::checkElementState);

    init();
}

QTextToSpeechEngineWinRT::~QTextToSpeechEngineWinRT()
{
}

QVector<QLocale> QTextToSpeechEngineWinRT::availableLocales() const
{
    Q_D(const QTextToSpeechEngineWinRT);
    return d->locales;
}

QVector<QVoice> QTextToSpeechEngineWinRT::availableVoices() const
{
    Q_D(const QTextToSpeechEngineWinRT);
    return d->voices;
}

void QTextToSpeechEngineWinRT::say(const QString &text)
{
    Q_D(QTextToSpeechEngineWinRT);

    HRESULT hr;

    hr = QEventDispatcherWinRT::runOnXamlThread([text, d]() {
        HRESULT hr;
        HStringReference nativeText(reinterpret_cast<LPCWSTR>(text.utf16()), text.length());
        ComPtr<IAsyncOperation<SpeechSynthesisStream*>> op;

        hr = d->synth->SynthesizeTextToStreamAsync(nativeText.Get(), &op);
        RETURN_HR_IF_FAILED("Could not synthesize text.");

        ComPtr<ISpeechSynthesisStream> stream;
        hr = QWinRTFunctions::await(op, stream.GetAddressOf());
        RETURN_HR_IF_FAILED("Synthesizing failed.");

        ComPtr<IRandomAccessStream> randomStream;
        hr = stream.As(&randomStream);
        RETURN_HR_IF_FAILED("Could not cast to RandomAccessStream.");

        // Directly instantiating a MediaElement works, but it throws an exception
        // when setting the source. Using a XamlReader appears to set it up properly.
        ComPtr<IInspectable> element;
        hr = d->xamlReader->Load(HString::MakeReference(webviewXaml).Get(), &element);
        Q_ASSERT_SUCCEEDED(hr);

        if (d->media)
            d->media.Reset();

        hr = element.As(&d->media);
        RETURN_HR_IF_FAILED("Could not create MediaElement for playback.");

        // Volume and Playback Rate cannot be changed for synthesized audio once
        // it has been created. Hence QTextToSpeechEngineWinRT::setVolume/Rate
        // only cache the value until playback is started.
        hr = d->media->put_DefaultPlaybackRate(d->rate + 1);
        if (FAILED(hr))
            qWarning("Could not set playback rate.");

        const DOUBLE vol = DOUBLE(d->volume);
        hr = d->media->put_Volume(vol);
        if (FAILED(hr))
            qWarning("Could not set volume.");

        static const HStringReference empty(L"");
        hr = d->media->SetSource(randomStream.Get(), empty.Get());
        RETURN_HR_IF_FAILED("Could not set media source.");

        hr = d->media->Play();
        RETURN_HR_IF_FAILED("Could not initiate playback.");

        return S_OK;
    });
    if (SUCCEEDED(hr)) {
        d->timer.start();
        d->state = QTextToSpeech::Speaking;
    } else {
        d->state = QTextToSpeech::BackendError;
    }
    emit stateChanged(d->state);
}

void QTextToSpeechEngineWinRT::stop()
{
    Q_D(QTextToSpeechEngineWinRT);

    if (!d->media)
        return;

    HRESULT hr;
    hr = QEventDispatcherWinRT::runOnXamlThread([d]() {
        HRESULT hr = d->media->Stop();
        RETURN_HR_IF_FAILED("Could not stop playback.");

        d->media.Reset();
        return hr;
    });
    if (SUCCEEDED(hr)) {
        d->timer.stop();
        d->state = QTextToSpeech::Ready;
        emit stateChanged(d->state);
    }
}

void QTextToSpeechEngineWinRT::pause()
{
    Q_D(QTextToSpeechEngineWinRT);

    if (!d->media)
        return;

    // Stop timer first to not have checkElementState being invoked
    // while context switch to/from Xaml thread happens.
    d->timer.stop();

    HRESULT hr;
    hr = QEventDispatcherWinRT::runOnXamlThread([d]() {
        HRESULT hr = d->media->Pause();
        RETURN_HR_IF_FAILED("Could not pause playback.");
        return hr;
    });
    if (SUCCEEDED(hr)) {
        d->state = QTextToSpeech::Paused;
        emit stateChanged(d->state);
    }
}

void QTextToSpeechEngineWinRT::resume()
{
    Q_D(QTextToSpeechEngineWinRT);

    if (!d->media)
        return;

    HRESULT hr;
    hr = QEventDispatcherWinRT::runOnXamlThread([d]() {
        HRESULT hr = d->media->Play();
        RETURN_HR_IF_FAILED("Could not resume playback.");
        return hr;
    });
    if (SUCCEEDED(hr)) {
        d->timer.start();
        d->state = QTextToSpeech::Speaking;
        emit stateChanged(d->state);
    }
}

double QTextToSpeechEngineWinRT::rate() const
{
    Q_D(const QTextToSpeechEngineWinRT);

    return d->rate;
}

bool QTextToSpeechEngineWinRT::setRate(double rate)
{
    Q_D(QTextToSpeechEngineWinRT);

    d->rate = rate;
    return true;
}

double QTextToSpeechEngineWinRT::pitch() const
{
    // Not supported for WinRT
    Q_UNIMPLEMENTED();
    return 1.;
}

bool QTextToSpeechEngineWinRT::setPitch(double pitch)
{
    // Not supported for WinRT
    Q_UNUSED(pitch);
    Q_UNIMPLEMENTED();
    return false;
}

QLocale QTextToSpeechEngineWinRT::locale() const
{
    Q_D(const QTextToSpeechEngineWinRT);

    HRESULT hr;
    ComPtr<IVoiceInformation> info;
    hr = d->synth->get_Voice(&info);

    HString language;
    hr = info->get_Language(language.GetAddressOf());

    return QLocale(QString::fromWCharArray(language.GetRawBuffer(0)));
}

bool QTextToSpeechEngineWinRT::setLocale(const QLocale &locale)
{
    Q_D(QTextToSpeechEngineWinRT);

    const int index = d->locales.indexOf(locale);
    if (index == -1)
        return false;

    return setVoice(d->voices.at(index));
}

double QTextToSpeechEngineWinRT::volume() const
{
    Q_D(const QTextToSpeechEngineWinRT);

    return d->volume;
}

bool QTextToSpeechEngineWinRT::setVolume(double volume)
{
    Q_D(QTextToSpeechEngineWinRT);

    d->volume = volume;
    return true;
}

QVoice QTextToSpeechEngineWinRT::voice() const
{
    Q_D(const QTextToSpeechEngineWinRT);

    HRESULT hr;
    ComPtr<IVoiceInformation> info;
    hr = d->synth->get_Voice(&info);

    return createVoiceForInformation(info);
}

bool QTextToSpeechEngineWinRT::setVoice(const QVoice &voice)
{
    Q_D(QTextToSpeechEngineWinRT);

    const int index = d->voices.indexOf(voice);
    if (index == -1)
        return false;

    HRESULT hr;
    hr = d->synth->put_Voice(d->infos.at(index).Get());
    return SUCCEEDED(hr);
}

QTextToSpeech::State QTextToSpeechEngineWinRT::state() const
{
    Q_D(const QTextToSpeechEngineWinRT);
    return d->state;
}

void QTextToSpeechEngineWinRT::checkElementState()
{
    Q_D(QTextToSpeechEngineWinRT);

    // MediaElement does not move into Stopped or Closed state when it finished
    // playback of synthesised text. Instead it goes into Pause mode.
    // Because of this MediaElement::add_MediaEnded() is not invoked and we
    // cannot add an event listener to the Media Element to properly emit
    // state changes.
    // To still be able to capture when it is ready, use a periodic timer and
    // check if the MediaElement went into Pause state.
    bool finished = false;
    HRESULT hr;
    hr = QEventDispatcherWinRT::runOnXamlThread([d, &finished]() {
        HRESULT hr;
        ABI::Windows::UI::Xaml::Media::MediaElementState s;
        hr = d->media.Get()->get_CurrentState(&s);
        if (SUCCEEDED(hr) && s == MediaElementState_Paused)
            finished = true;
        return hr;
    });

    if (finished)
        stop();
}

void QTextToSpeechEngineWinRT::init()
{
    Q_D(QTextToSpeechEngineWinRT);

    d->state = QTextToSpeech::BackendError;

    HRESULT hr;

    hr = QEventDispatcherWinRT::runOnXamlThread([d]() {
        HRESULT hr = RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_UI_Xaml_Markup_XamlReader).Get(),
                                            IID_PPV_ARGS(&d->xamlReader));
        Q_ASSERT_SUCCEEDED(hr);

        return hr;
    });

    ComPtr<IInstalledVoicesStatic> stat;
    hr = RoGetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Media_SpeechSynthesis_SpeechSynthesizer).Get(),
                                IID_PPV_ARGS(&stat));
    Q_ASSERT_SUCCEEDED(hr);

    hr = RoActivateInstance(HString::MakeReference(RuntimeClass_Windows_Media_SpeechSynthesis_SpeechSynthesizer).Get(),
                            &d->synth);
    Q_ASSERT_SUCCEEDED(hr);

    ComPtr<IVectorView<VoiceInformation*>> voices;
    hr = stat->get_AllVoices(&voices);
    RETURN_VOID_IF_FAILED("Could not get voice information.");

    quint32 voiceSize;
    hr = voices->get_Size(&voiceSize);
    RETURN_VOID_IF_FAILED("Could not access size of voice information.");

    for (quint32 i = 0; i < voiceSize; ++i) {
        ComPtr<IVoiceInformation> info;
        hr = voices->GetAt(i, &info);
        Q_ASSERT_SUCCEEDED(hr);

        HString nativeLanguage;
        hr = info->get_Language(nativeLanguage.GetAddressOf());
        Q_ASSERT_SUCCEEDED(hr);

        const QString languageString = QString::fromWCharArray(nativeLanguage.GetRawBuffer(0));
        QLocale locale(languageString);
        if (!d->locales.contains(locale))
            d->locales.append(locale);

        QVoice voice = createVoiceForInformation(info);
        d->voices.append(voice);
        d->infos.append(info);
    }

    d->state = QTextToSpeech::Ready;
}

QVoice QTextToSpeechEngineWinRT::createVoiceForInformation(ComPtr<IVoiceInformation> info) const
{
    HRESULT hr;
    HString nativeName;
    hr = info->get_DisplayName(nativeName.GetAddressOf());
    Q_ASSERT_SUCCEEDED(hr);

    const QString name = QString::fromWCharArray(nativeName.GetRawBuffer(0));

    VoiceGender gender;
    hr = info->get_Gender(&gender);
    Q_ASSERT_SUCCEEDED(hr);

    return QTextToSpeechEngine::createVoice(name, gender == VoiceGender_Male ? QVoice::Male : QVoice::Female,
                                            QVoice::Other, QVariant());
}

QT_END_NAMESPACE

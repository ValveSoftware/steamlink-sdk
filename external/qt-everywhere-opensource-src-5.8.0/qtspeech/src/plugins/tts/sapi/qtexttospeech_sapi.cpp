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

#include "qtexttospeech_sapi.h"

#include <windows.h>
#include <sapi.h>
#include <sphelper.h>
#include <qdebug.h>

QT_BEGIN_NAMESPACE

QTextToSpeechEngineSapi::QTextToSpeechEngineSapi(const QVariantMap &, QObject *)
    : m_pitch(0.0), m_pauseCount(0), m_state(QTextToSpeech::BackendError)
{
    init();
}

QTextToSpeechEngineSapi::~QTextToSpeechEngineSapi()
{
}

void QTextToSpeechEngineSapi::init()
{
    if (FAILED(::CoInitialize(NULL))) {
        qWarning() << "Init of COM failed";
        return;
    }

    HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void **)&m_voice);
    if (!SUCCEEDED(hr)) {
        qWarning() << "Could not init voice";
        return;
    }

    m_voice->SetInterest(SPFEI_ALL_TTS_EVENTS, SPFEI_ALL_TTS_EVENTS);
    m_voice->SetNotifyCallbackInterface(this, 0, 0);
    updateVoices();
    m_state = QTextToSpeech::Ready;
}

bool QTextToSpeechEngineSapi::isSpeaking() const
{
    SPVOICESTATUS eventStatus;
    m_voice->GetStatus(&eventStatus, NULL);
    return eventStatus.dwRunningState == SPRS_IS_SPEAKING;
}

void QTextToSpeechEngineSapi::say(const QString &text)
{
    if (text.isEmpty())
        return;

    QString textString = text;
    if (m_state != QTextToSpeech::Ready)
        stop();

    textString.prepend(QString::fromLatin1("<pitch absmiddle=\"%1\"/>").arg(m_pitch * 10));

    std::wstring wtext = textString.toStdWString();
    m_voice->Speak(wtext.data(), SPF_ASYNC, NULL);
}

void QTextToSpeechEngineSapi::stop()
{
    if (m_state == QTextToSpeech::Paused)
        resume();
    m_voice->Speak(NULL, SPF_PURGEBEFORESPEAK, 0);
}

void QTextToSpeechEngineSapi::pause()
{
    if (!isSpeaking())
        return;

    if (m_pauseCount == 0) {
        ++m_pauseCount;
        m_voice->Pause();
        m_state = QTextToSpeech::Paused;
        emit stateChanged(m_state);
    }
}

void QTextToSpeechEngineSapi::resume()
{
    if (m_pauseCount > 0) {
        --m_pauseCount;
        m_voice->Resume();
        if (isSpeaking()) {
            m_state = QTextToSpeech::Speaking;
        } else {
            m_state = QTextToSpeech::Ready;
        }
        emit stateChanged(m_state);
    }
}

bool QTextToSpeechEngineSapi::setPitch(double pitch)
{
    m_pitch = pitch;
    return true;
}

double QTextToSpeechEngineSapi::pitch() const
{
    return m_pitch;
}

bool QTextToSpeechEngineSapi::setRate(double rate)
{
    // -10 to 10
    m_voice->SetRate(long(rate*10));
    return true;
}

double QTextToSpeechEngineSapi::rate() const
{
    long rateValue;
    if (m_voice->GetRate(&rateValue) == S_OK)
        return rateValue / 10.0;
    return -1;
}

bool QTextToSpeechEngineSapi::setVolume(double volume)
{
    // 0 to 1
    m_voice->SetVolume(volume * 100);
    return true;
}

double QTextToSpeechEngineSapi::volume() const
{
    USHORT baseVolume;
    if (m_voice->GetVolume(&baseVolume) == S_OK)
    {
        return baseVolume / 100.0;
    }
    return 0.0;
}

QString QTextToSpeechEngineSapi::voiceId(ISpObjectToken *speechToken) const
{
    HRESULT hr = S_OK;
    LPWSTR vId = nullptr;
    hr = speechToken->GetId(&vId);
    if (FAILED(hr)) {
        qWarning() << "ISpObjectToken::GetId failed";
        return QString();
    }
    return QString::fromWCharArray(vId);
}

QMap<QString, QString> QTextToSpeechEngineSapi::voiceAttributes(ISpObjectToken *speechToken) const
{
    HRESULT hr = S_OK;
    QMap<QString, QString> result;

    ISpDataKey *pAttrKey = nullptr;
    hr = speechToken->OpenKey(L"Attributes", &pAttrKey);
    if (FAILED(hr)) {
        qWarning() << "ISpObjectToken::OpenKeys failed";
        return result;
    }

    // enumerate values
    for (ULONG v = 0; ; v++) {
        LPWSTR val = nullptr;
        hr = pAttrKey->EnumValues(v, &val);
        if (SPERR_NO_MORE_ITEMS == hr) {
            // done
            break;
        } else if (FAILED(hr)) {
            qWarning() << "ISpDataKey::EnumValues failed";
            continue;
        }

        // how do we know whether it's a string or a DWORD?
        LPWSTR data = nullptr;
        hr = pAttrKey->GetStringValue(val, &data);
        if (FAILED(hr)) {
            qWarning() << "ISpDataKey::GetStringValue failed";
            continue;
        }

        if (0 != wcscmp(val, L"")) {
            result[QString::fromWCharArray(val)] = QString::fromWCharArray(data);
        }

        // FIXME: Do we need to free the memory here?
        CoTaskMemFree(val);
        CoTaskMemFree(data);
    }

    return result;
}

QLocale QTextToSpeechEngineSapi::lcidToLocale(const QString &lcid) const
{
    bool ok;
    LCID locale = lcid.toInt(&ok, 16);
    if (!ok) {
        qWarning() << "Could not convert language attribute to LCID";
        return QLocale();
    }
    int nchars = GetLocaleInfoW(locale, LOCALE_SISO639LANGNAME, NULL, 0);
    wchar_t* languageCode = new wchar_t[nchars];
    GetLocaleInfoW(locale, LOCALE_SISO639LANGNAME, languageCode, nchars);
    QString iso = QString::fromWCharArray(languageCode);
    delete[] languageCode;
    return QLocale(iso);
}

void QTextToSpeechEngineSapi::updateVoices()
{
    HRESULT hr = S_OK;
    CComPtr<ISpObjectToken> cpVoiceToken;
    CComPtr<IEnumSpObjectTokens> cpEnum;
    ULONG ulCount = 0;

    hr = SpEnumTokens(SPCAT_VOICES, NULL, NULL, &cpEnum);
    if (SUCCEEDED(hr)) {
        hr = cpEnum->GetCount(&ulCount);
    }

    // Loop through all voices
    while (SUCCEEDED(hr) && ulCount--) {
        cpVoiceToken.Release();
        hr = cpEnum->Next(1, &cpVoiceToken, NULL);

        // Get attributes of the voice
        QMap<QString, QString> vAttr = voiceAttributes(cpVoiceToken);

        // Transform Windows LCID to QLocale
        QLocale vLocale = lcidToLocale(vAttr["Language"]);
        if (!m_locales.contains(vLocale))
            m_locales.append(vLocale);

        // Create voice

        QString name = vAttr["Name"];
        QVoice::Age age = vAttr["Age"] == "Adult" ?  QVoice::Adult : QVoice::Other;
        QVoice::Gender gender = vAttr["Gender"] == "Male" ? QVoice::Male :
                                vAttr["Gender"] == "Female" ? QVoice::Female :
                                QVoice::Unknown;
        // Getting the ID of the voice to set the voice later
        QString vId = voiceId(cpVoiceToken);
        QVoice voice = createVoice(name, gender, age, vId);
        m_voices.insert(vLocale.name(), voice);
    }
}

QVector<QLocale> QTextToSpeechEngineSapi::availableLocales() const
{
    return m_locales;
}

bool QTextToSpeechEngineSapi::setLocale(const QLocale &locale)
{
    QList<QVoice> voicesForLocale = m_voices.values(locale.name());
    if (voicesForLocale.length() > 0) {
        setVoice(voicesForLocale[0]);
        return true;
    } else {
        qWarning() << "No voice found for given locale";
    }
    return false;
}

QLocale QTextToSpeechEngineSapi::locale() const
{
    // Get current voice id
    CComPtr<ISpObjectToken> cpVoiceToken;
    m_voice->GetVoice(&cpVoiceToken);
    // read attributes
    QMap<QString, QString> vAttr = voiceAttributes(cpVoiceToken);
    return lcidToLocale(vAttr["Language"]);
}

QVector<QVoice> QTextToSpeechEngineSapi::availableVoices() const
{
    return m_voices.values(locale().name()).toVector();
}

bool QTextToSpeechEngineSapi::setVoice(const QVoice &voice)
{
    // Convert voice id to null-terminated wide char string
    QString vId = voiceData(voice).toString();
    wchar_t* tokenId = new wchar_t[vId.size()+1];
    vId.toWCharArray(tokenId);
    tokenId[vId.size()] = 0;

    // create the voice token via the id
    HRESULT hr = S_OK;
    CComPtr<ISpObjectToken> cpVoiceToken;
    hr = SpCreateNewToken(tokenId, &cpVoiceToken);
    if (FAILED(hr)) {
        qWarning() << "Creating the voice token from ID failed";
        m_state = QTextToSpeech::BackendError;
        emit stateChanged(m_state);
        return false;
    }

    if (m_state != QTextToSpeech::Ready) {
        m_state = QTextToSpeech::Ready;
        emit stateChanged(m_state);
    }

    delete[] tokenId;
    m_voice->SetVoice(cpVoiceToken);
    return true;
}

QVoice QTextToSpeechEngineSapi::voice() const
{
    CComPtr<ISpObjectToken> cpVoiceToken;
    m_voice->GetVoice(&cpVoiceToken);
    QString vId = voiceId(cpVoiceToken);
    foreach (const QVoice &voice, m_voices.values()) {
        if (voiceData(voice).toString() == vId) {
            return voice;
        }
    }
    return QVoice();
}

QTextToSpeech::State QTextToSpeechEngineSapi::state() const
{
    return m_state;
}

HRESULT QTextToSpeechEngineSapi::NotifyCallback(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    QTextToSpeech::State newState = QTextToSpeech::Ready;
    if (isPaused()) {
        newState = QTextToSpeech::Paused;
    } else if (isSpeaking()) {
        newState = QTextToSpeech::Speaking;
    } else {
        newState = QTextToSpeech::Ready;
    }

    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(newState);
    }

    return S_OK;
}

QT_END_NAMESPACE

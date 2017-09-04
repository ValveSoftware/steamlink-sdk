/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QTEXTTOSPEECHENGINE_IOS_H
#define QTEXTTOSPEECHENGINE_IOS_H

#include <QtCore/qvector.h>
#include <QtTextToSpeech/qtexttospeechengine.h>
#include <QtTextToSpeech/qvoice.h>

Q_FORWARD_DECLARE_OBJC_CLASS(AVSpeechSynthesizer);
Q_FORWARD_DECLARE_OBJC_CLASS(AVSpeechSynthesisVoice);

QT_BEGIN_NAMESPACE

class QTextToSpeechEngineIos : public QTextToSpeechEngine
{
    Q_OBJECT

public:
    QTextToSpeechEngineIos(const QVariantMap &parameters, QObject *parent);
    ~QTextToSpeechEngineIos();

    QVector<QLocale> availableLocales() const override;
    QVector<QVoice> availableVoices() const override;
    void say(const QString &text) override;
    void stop() override;
    void pause() override;
    void resume() override;
    double rate() const override;
    bool setRate(double rate) override;
    double pitch() const override;
    bool setPitch(double pitch) override;
    QLocale locale() const override;
    bool setLocale(const QLocale &locale) override;
    double volume() const override;
    bool setVolume(double volume) override;
    QVoice voice() const override;
    bool setVoice(const QVoice &voice) override;
    QTextToSpeech::State state() const override;

    void setState(QTextToSpeech::State state);

private:
    AVSpeechSynthesisVoice *fromQVoice(const QVoice &voice) const;
    QVoice toQVoice(AVSpeechSynthesisVoice *avVoice) const;

    AVSpeechSynthesizer *m_speechSynthesizer;
    QLocale m_locale;
    QVoice m_voice;
    QTextToSpeech::State m_state;

    double m_pitch;
    double m_rate;
    double m_volume;
};

QT_END_NAMESPACE

#endif

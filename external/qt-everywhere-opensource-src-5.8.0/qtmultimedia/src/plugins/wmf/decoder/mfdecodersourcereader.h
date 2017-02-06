/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Mobility Components.
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

#ifndef MFDECODERSOURCEREADER_H
#define MFDECODERSOURCEREADER_H
#include <mfapi.h>
#include <mfidl.h>
#include <Mfreadwrite.h>

#include <QtCore/qobject.h>
#include <QtCore/qmutex.h>
#include "qaudioformat.h"

QT_USE_NAMESPACE

class MFDecoderSourceReader : public QObject, public IMFSourceReaderCallback
{
    Q_OBJECT
public:
    MFDecoderSourceReader(QObject *parent = 0);
    void shutdown();

    IMFMediaSource* mediaSource();
    IMFMediaType* setSource(IMFMediaSource *source, const QAudioFormat &audioFormat);

    void reset();
    void readNextSample();
    QList<IMFSample*> takeSamples(); //internal samples will be cleared after this

    //from IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, LPVOID *ppvObject);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    //from IMFSourceReaderCallback
    STDMETHODIMP OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex,
        DWORD dwStreamFlags, LONGLONG llTimestamp, IMFSample *pSample);
    STDMETHODIMP OnFlush(DWORD dwStreamIndex);
    STDMETHODIMP OnEvent(DWORD dwStreamIndex, IMFMediaEvent *pEvent);

Q_SIGNALS:
    void sampleAdded();
    void finished();

private:
    long m_cRef;
    QList<IMFSample*>   m_cachedSamples;
    QMutex              m_samplesMutex;

    IMFSourceReader             *m_sourceReader;
    IMFMediaSource              *m_source;
};
#endif//MFDECODERSOURCEREADER_H

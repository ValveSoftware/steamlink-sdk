/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef DIRECTSHOWPIN_H
#define DIRECTSHOWPIN_H

#include "directshowobject.h"

#include "directshowmediatype.h"
#include <qstring.h>
#include <qmutex.h>

QT_BEGIN_NAMESPACE

class DirectShowBaseFilter;

class DirectShowPin : public DirectShowObject
                    , public IPin
{
    DIRECTSHOW_OBJECT

public:
    virtual ~DirectShowPin();

    QString name() const { return m_name; }
    bool isConnected() const { return m_peerPin != NULL; }

    virtual bool isMediaTypeSupported(const AM_MEDIA_TYPE *type) = 0;
    virtual QList<DirectShowMediaType> supportedMediaTypes();
    virtual bool setMediaType(const AM_MEDIA_TYPE *type);

    virtual HRESULT completeConnection(IPin *pin);
    virtual HRESULT connectionEnded();

    virtual HRESULT setActive(bool active);

    // DirectShowObject
    HRESULT getInterface(REFIID riid, void **ppvObject);

    // IPin
    STDMETHODIMP Connect(IPin *pReceivePin, const AM_MEDIA_TYPE *pmt);
    STDMETHODIMP ReceiveConnection(IPin *pConnector, const AM_MEDIA_TYPE *pmt);
    STDMETHODIMP Disconnect();
    STDMETHODIMP ConnectedTo(IPin **ppPin);

    STDMETHODIMP ConnectionMediaType(AM_MEDIA_TYPE *pmt);

    STDMETHODIMP QueryPinInfo(PIN_INFO *pInfo);
    STDMETHODIMP QueryId(LPWSTR *Id);

    STDMETHODIMP QueryAccept(const AM_MEDIA_TYPE *pmt);

    STDMETHODIMP EnumMediaTypes(IEnumMediaTypes **ppEnum);

    STDMETHODIMP QueryInternalConnections(IPin **apPin, ULONG *nPin);

    STDMETHODIMP EndOfStream();

    STDMETHODIMP BeginFlush();
    STDMETHODIMP EndFlush();

    STDMETHODIMP NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

    STDMETHODIMP QueryDirection(PIN_DIRECTION *pPinDir);

protected:
    DirectShowPin(DirectShowBaseFilter *filter, const QString &name, PIN_DIRECTION direction);

    QMutex m_mutex;

    DirectShowBaseFilter *m_filter;
    QString m_name;
    PIN_DIRECTION m_direction;

    IPin *m_peerPin;
    DirectShowMediaType m_mediaType;

private:
    Q_DISABLE_COPY(DirectShowPin)
    HRESULT tryMediaTypes(IPin *pin, const AM_MEDIA_TYPE *type, IEnumMediaTypes *enumMediaTypes);
    HRESULT tryConnect(IPin *pin, const AM_MEDIA_TYPE *type);
};


class DirectShowOutputPin : public DirectShowPin
{
    DIRECTSHOW_OBJECT

public:
    virtual ~DirectShowOutputPin();

    // DirectShowPin
    virtual HRESULT completeConnection(IPin *pin);
    virtual HRESULT connectionEnded();
    virtual HRESULT setActive(bool active);

    // IPin
    STDMETHODIMP EndOfStream();

protected:
    DirectShowOutputPin(DirectShowBaseFilter *filter, const QString &name);

    IMemAllocator *m_allocator;
    IMemInputPin *m_inputPin;

private:
    Q_DISABLE_COPY(DirectShowOutputPin)
};


class DirectShowInputPin : public DirectShowPin
                         , public IMemInputPin
{
    DIRECTSHOW_OBJECT

public:
    virtual ~DirectShowInputPin();

    const AM_SAMPLE2_PROPERTIES *currentSampleProperties() const { return &m_sampleProperties; }

    // DirectShowObject
    HRESULT getInterface(REFIID riid, void **ppvObject);

    // DirectShowPin
    HRESULT connectionEnded();
    HRESULT setActive(bool active);

    // IPin
    STDMETHODIMP EndOfStream();
    STDMETHODIMP BeginFlush();
    STDMETHODIMP EndFlush();

    // IMemInputPin
    STDMETHODIMP GetAllocator(IMemAllocator **ppAllocator);
    STDMETHODIMP NotifyAllocator(IMemAllocator *pAllocator, BOOL bReadOnly);
    STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps);

    STDMETHODIMP Receive(IMediaSample *pSample);
    STDMETHODIMP ReceiveMultiple(IMediaSample **pSamples, long nSamples, long *nSamplesProcessed);
    STDMETHODIMP ReceiveCanBlock();

protected:
    DirectShowInputPin(DirectShowBaseFilter *filter, const QString &name);

    IMemAllocator *m_allocator;
    bool m_flushing;
    bool m_inErrorState;
    AM_SAMPLE2_PROPERTIES m_sampleProperties;

private:
    Q_DISABLE_COPY(DirectShowInputPin)
};

QT_END_NAMESPACE

#endif // DIRECTSHOWPIN_H

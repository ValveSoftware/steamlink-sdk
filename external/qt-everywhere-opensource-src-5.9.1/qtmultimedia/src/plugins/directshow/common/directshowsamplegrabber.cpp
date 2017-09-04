/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "directshowsamplegrabber.h"

#include "directshowglobal.h"
#include "directshowutils.h"


QT_BEGIN_NAMESPACE

// Previously contained in <qedit.h>.
static const IID iID_ISampleGrabber = { 0x6B652FFF, 0x11FE, 0x4fce, { 0x92, 0xAD, 0x02, 0x66, 0xB5, 0xD7, 0xC7, 0x8F } };
static const CLSID cLSID_SampleGrabber = { 0xC1F400A0, 0x3F08, 0x11d3, { 0x9F, 0x0B, 0x00, 0x60, 0x08, 0x03, 0x9E, 0x37 } };

class SampleGrabberCallbackPrivate : public ISampleGrabberCB
{
public:
    explicit SampleGrabberCallbackPrivate(DirectShowSampleGrabber *grabber)
        : m_ref(1)
        , m_grabber(grabber)
    { }

    virtual ~SampleGrabberCallbackPrivate() { }

    STDMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&m_ref);
    }

    STDMETHODIMP_(ULONG) Release()
    {
        ULONG ref = InterlockedDecrement(&m_ref);
        if (ref == 0)
            delete this;
        return ref;
    }

    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject)
    {
        if (NULL == ppvObject)
            return E_POINTER;

        if (riid == IID_IUnknown /*__uuidof(IUnknown) */ ) {
            AddRef();
            *ppvObject = static_cast<IUnknown *>(this);
            return S_OK;
        } else if (riid == IID_ISampleGrabberCB /*__uuidof(ISampleGrabberCB)*/ ) {
            AddRef();
            *ppvObject = static_cast<ISampleGrabberCB *>(this);
            return S_OK;
        }
        return E_NOTIMPL;
    }

    STDMETHODIMP SampleCB(double time, IMediaSample *mediaSample)
    {
        if (m_grabber)
            Q_EMIT m_grabber->sampleAvailable(time, mediaSample);

        return S_OK;
    }

    STDMETHODIMP BufferCB(double time, BYTE *buffer, long bufferLen)
    {
        if (m_grabber)
            Q_EMIT m_grabber->bufferAvailable(time, buffer, bufferLen);

        return S_OK;
    }

private:
    ULONG m_ref;
    DirectShowSampleGrabber *m_grabber;
};

DirectShowSampleGrabber::DirectShowSampleGrabber(QObject *p)
    : QObject(p)
    , m_sampleGrabber(nullptr)
    , m_sampleGabberCb(nullptr)
    , m_callbackType(CallbackMethod::BufferCB)
{
    // Create sample grabber filter
    HRESULT hr = CoCreateInstance(cLSID_SampleGrabber, NULL, CLSCTX_INPROC, iID_ISampleGrabber, reinterpret_cast<void **>(&m_sampleGrabber));

    if (FAILED(hr)) {
        qCWarning(qtDirectShowPlugin, "Failed to create sample grabber");
        return;
    }

    hr = m_sampleGrabber->QueryInterface(IID_IBaseFilter, reinterpret_cast<void **>(&m_filter));
    if (FAILED(hr))
        qCWarning(qtDirectShowPlugin, "Failed to get base filter interface");
}

DirectShowSampleGrabber::~DirectShowSampleGrabber()
{
    stop();
    SAFE_RELEASE(m_sampleGabberCb);
    SAFE_RELEASE(m_filter);
    SAFE_RELEASE(m_sampleGrabber);
}

void DirectShowSampleGrabber::stop()
{
    if (!m_sampleGrabber)
        return;

    if (FAILED(m_sampleGrabber->SetCallback(nullptr, static_cast<long>(m_callbackType)))) {
        qCWarning(qtDirectShowPlugin, "Failed to stop sample grabber callback");
        return;
    }
}

bool DirectShowSampleGrabber::getConnectedMediaType(AM_MEDIA_TYPE *mediaType)
{
    Q_ASSERT(mediaType);

    if (!isValid())
        return false;

    if (FAILED(m_sampleGrabber->GetConnectedMediaType(mediaType))) {
        qCWarning(qtDirectShowPlugin, "Failed to retrieve the connected media type");
        return false;
    }

    return true;
}

bool DirectShowSampleGrabber::setMediaType(const AM_MEDIA_TYPE *mediaType)
{
    Q_ASSERT(mediaType);

    if (FAILED(m_sampleGrabber->SetMediaType(mediaType))) {
        qCWarning(qtDirectShowPlugin, "Failed to set media type");
        return false;
    }

    return true;
}

void DirectShowSampleGrabber::start(DirectShowSampleGrabber::CallbackMethod method,
                            bool oneShot,
                            bool bufferSamples)
{
    if (!m_sampleGrabber)
        return;

    stop();

    if (!m_sampleGabberCb)
        m_sampleGabberCb = new SampleGrabberCallbackPrivate(this);

    m_callbackType = method;
    m_sampleGrabber->SetCallback(m_sampleGabberCb, static_cast<long>(method));
    m_sampleGrabber->SetOneShot(oneShot);
    m_sampleGrabber->SetBufferSamples(bufferSamples);
}

QT_END_NAMESPACE

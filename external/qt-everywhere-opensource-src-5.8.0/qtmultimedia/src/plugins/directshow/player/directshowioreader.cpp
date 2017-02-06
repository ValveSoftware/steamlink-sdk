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

#include "directshowioreader.h"

#include "directshoweventloop.h"
#include "directshowglobal.h"
#include "directshowiosource.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qthread.h>

class DirectShowSampleRequest
{
public:
    DirectShowSampleRequest(
            IMediaSample *sample, DWORD_PTR userData, LONGLONG position, LONG length, BYTE *buffer)
        : next(0)
        , sample(sample)
        , userData(userData)
        , position(position)
        , length(length)
        , buffer(buffer)
        , result(S_FALSE)
    {
    }

    DirectShowSampleRequest *remove() { DirectShowSampleRequest *n = next; delete this; return n; }

    DirectShowSampleRequest *next;
    IMediaSample *sample;
    DWORD_PTR userData;
    LONGLONG position;
    LONG length;
    BYTE *buffer;
    HRESULT result;
};

DirectShowIOReader::DirectShowIOReader(
        QIODevice *device, DirectShowIOSource *source, DirectShowEventLoop *loop)
    : m_source(source)
    , m_device(device)
    , m_loop(loop)
    , m_pendingHead(0)
    , m_pendingTail(0)
    , m_readyHead(0)
    , m_readyTail(0)
    , m_synchronousPosition(0)
    , m_synchronousLength(0)
    , m_synchronousBytesRead(0)
    , m_synchronousBuffer(0)
    , m_synchronousResult(S_OK)
    , m_totalLength(0)
    , m_availableLength(0)
    , m_flushing(false)
{
    moveToThread(device->thread());

    connect(device, SIGNAL(readyRead()), this, SLOT(readyRead()));
}

DirectShowIOReader::~DirectShowIOReader()
{
    flushRequests();
}

HRESULT DirectShowIOReader::QueryInterface(REFIID riid, void **ppvObject)
{
    return m_source->QueryInterface(riid, ppvObject);
}

ULONG DirectShowIOReader::AddRef()
{
    return m_source->AddRef();
}

ULONG DirectShowIOReader::Release()
{
    return m_source->Release();
}

// IAsyncReader
HRESULT DirectShowIOReader::RequestAllocator(
        IMemAllocator *pPreferred, ALLOCATOR_PROPERTIES *pProps, IMemAllocator **ppActual)
{
    if (!ppActual || !pProps) {
        return E_POINTER;
    } else {
        ALLOCATOR_PROPERTIES actualProperties;

        if (pProps->cbAlign == 0)
            pProps->cbAlign = 1;

        if (pPreferred && pPreferred->SetProperties(pProps, &actualProperties) == S_OK) {
            pPreferred->AddRef();

            *ppActual = pPreferred;

            m_source->setAllocator(*ppActual);

            return S_OK;
        } else {
            *ppActual = com_new<IMemAllocator>(CLSID_MemoryAllocator);

            if (*ppActual) {
                if ((*ppActual)->SetProperties(pProps, &actualProperties) != S_OK) {
                    (*ppActual)->Release();
                } else {
                    m_source->setAllocator(*ppActual);

                    return S_OK;
                }
            }
        }
        ppActual = 0;

        return E_FAIL;
    }
}

HRESULT DirectShowIOReader::Request(IMediaSample *pSample, DWORD_PTR dwUser)
{
    QMutexLocker locker(&m_mutex);

    if (!pSample) {
        return E_POINTER;
    } else if (m_flushing) {
        return VFW_E_WRONG_STATE;
    } else {
        REFERENCE_TIME startTime = 0;
        REFERENCE_TIME endTime = 0;
        BYTE *buffer;

        if (pSample->GetTime(&startTime, &endTime) != S_OK
                || pSample->GetPointer(&buffer) != S_OK) {
            return VFW_E_SAMPLE_TIME_NOT_SET;
        } else {
            LONGLONG position = startTime / 10000000;
            LONG length = (endTime - startTime) / 10000000;

            DirectShowSampleRequest *request = new DirectShowSampleRequest(
                    pSample, dwUser, position, length, buffer);

            if (m_pendingTail) {
                m_pendingTail->next = request;
            } else {
                m_pendingHead = request;

                m_loop->postEvent(this, new QEvent(QEvent::User));
            }
            m_pendingTail = request;

            return S_OK;
        }
    }
}

HRESULT DirectShowIOReader::WaitForNext(
        DWORD dwTimeout, IMediaSample **ppSample, DWORD_PTR *pdwUser)
{
    if (!ppSample || !pdwUser)
        return E_POINTER;

    QMutexLocker locker(&m_mutex);

    do {
        if (m_readyHead) {
            DirectShowSampleRequest *request = m_readyHead;

            *ppSample = request->sample;
            *pdwUser = request->userData;

            HRESULT hr = request->result;

            m_readyHead = request->next;

            if (!m_readyHead)
                m_readyTail = 0;

            delete request;

            return hr;
        } else if (m_flushing) {
            *ppSample = 0;
            *pdwUser = 0;

            return VFW_E_WRONG_STATE;
        }
    } while (m_wait.wait(&m_mutex, dwTimeout));

    *ppSample = 0;
    *pdwUser = 0;

    return VFW_E_TIMEOUT;
}

HRESULT DirectShowIOReader::SyncReadAligned(IMediaSample *pSample)
{
    if (!pSample) {
        return E_POINTER;
    } else {
        REFERENCE_TIME startTime = 0;
        REFERENCE_TIME endTime = 0;
        BYTE *buffer;

        if (pSample->GetTime(&startTime, &endTime) != S_OK
                || pSample->GetPointer(&buffer) != S_OK) {
            return VFW_E_SAMPLE_TIME_NOT_SET;
        } else {
            LONGLONG position = startTime / 10000000;
            LONG length = (endTime - startTime) / 10000000;

            QMutexLocker locker(&m_mutex);

            if (thread() == QThread::currentThread()) {
                qint64 bytesRead = 0;

                HRESULT hr = blockingRead(position, length, buffer, &bytesRead);

                if (SUCCEEDED(hr))
                    pSample->SetActualDataLength(bytesRead);

                return hr;
            } else {
                m_synchronousPosition = position;
                m_synchronousLength = length;
                m_synchronousBuffer = buffer;

                m_loop->postEvent(this, new QEvent(QEvent::User));

                m_wait.wait(&m_mutex);

                m_synchronousBuffer = 0;

                if (SUCCEEDED(m_synchronousResult))
                    pSample->SetActualDataLength(m_synchronousBytesRead);

                return m_synchronousResult;
            }
        }
    }
}

HRESULT DirectShowIOReader::SyncRead(LONGLONG llPosition, LONG lLength, BYTE *pBuffer)
{
    if (!pBuffer) {
        return E_POINTER;
    } else {
        if (thread() == QThread::currentThread()) {
            qint64 bytesRead;

            return blockingRead(llPosition, lLength, pBuffer, &bytesRead);
        } else {
            QMutexLocker locker(&m_mutex);

            m_synchronousPosition = llPosition;
            m_synchronousLength = lLength;
            m_synchronousBuffer = pBuffer;

            m_loop->postEvent(this, new QEvent(QEvent::User));

            m_wait.wait(&m_mutex);

            m_synchronousBuffer = 0;

            return m_synchronousResult;
        }
    }
}

HRESULT DirectShowIOReader::Length(LONGLONG *pTotal, LONGLONG *pAvailable)
{
    if (!pTotal || !pAvailable) {
        return E_POINTER;
    } else {
        QMutexLocker locker(&m_mutex);

        *pTotal = m_totalLength;
        *pAvailable = m_availableLength;

        return S_OK;
    }
}


HRESULT DirectShowIOReader::BeginFlush()
{
    QMutexLocker locker(&m_mutex);

    if (m_flushing)
        return S_FALSE;

    m_flushing = true;

    flushRequests();

    m_wait.wakeAll();

    return S_OK;
}

HRESULT DirectShowIOReader::EndFlush()
{
    QMutexLocker locker(&m_mutex);

    if (!m_flushing)
        return S_FALSE;

    m_flushing = false;

    return S_OK;
}

void DirectShowIOReader::customEvent(QEvent *event)
{
    if (event->type() == QEvent::User) {
        readyRead();
    } else {
        QObject::customEvent(event);
    }
}

void DirectShowIOReader::readyRead()
{
    QMutexLocker locker(&m_mutex);

    m_availableLength = m_device->bytesAvailable() + m_device->pos();
    m_totalLength = m_device->size();

    if (m_synchronousBuffer) {
        if (nonBlockingRead(
                m_synchronousPosition,
                m_synchronousLength,
                m_synchronousBuffer,
                &m_synchronousBytesRead,
                &m_synchronousResult)) {
            m_wait.wakeAll();
        }
    } else {
        qint64 bytesRead = 0;

        while (m_pendingHead && nonBlockingRead(
                m_pendingHead->position,
                m_pendingHead->length,
                m_pendingHead->buffer,
                &bytesRead,
            &m_pendingHead->result)) {
            m_pendingHead->sample->SetActualDataLength(bytesRead);

            if (m_readyTail)
                m_readyTail->next = m_pendingHead;
            m_readyTail = m_pendingHead;

            m_pendingHead = m_pendingHead->next;

            m_readyTail->next = 0;

            if (!m_pendingHead)
                m_pendingTail = 0;

            if (!m_readyHead)
                m_readyHead = m_readyTail;

            m_wait.wakeAll();
        }
    }
}

HRESULT DirectShowIOReader::blockingRead(
        LONGLONG position, LONG length, BYTE *buffer, qint64 *bytesRead)
{
    *bytesRead = 0;

    if (qint64(position) > m_device->size())
        return S_FALSE;

    const qint64 maxSize = qMin<qint64>(m_device->size(), position + length);

    while (m_device->bytesAvailable() + m_device->pos() < maxSize) {
        if (!m_device->waitForReadyRead(-1))
            return S_FALSE;
    }

    if (m_device->pos() != position && !m_device->seek(position))
        return S_FALSE;

    const qint64 maxBytes = qMin<qint64>(length, m_device->bytesAvailable());

    *bytesRead = m_device->read(reinterpret_cast<char *>(buffer), maxBytes);

    if (*bytesRead != length) {
        ::memset(buffer + *bytesRead, 0, length - *bytesRead);

        return S_FALSE;
    } else {
        return S_OK;
    }
}

bool DirectShowIOReader::nonBlockingRead(
        LONGLONG position, LONG length, BYTE *buffer, qint64 *bytesRead, HRESULT *result)
{
    const qint64 maxSize = qMin<qint64>(m_device->size(), position + length);

    if (position > m_device->size()) {
        *bytesRead = 0;
        *result = S_FALSE;

        return true;
    } else if (m_device->bytesAvailable() + m_device->pos() >= maxSize) {
        if (m_device->pos() != position && !m_device->seek(position)) {
            *bytesRead = 0;
            *result = S_FALSE;

            return true;
        } else {
            const qint64 maxBytes = qMin<qint64>(length, m_device->bytesAvailable());

            *bytesRead = m_device->read(reinterpret_cast<char *>(buffer), maxBytes);

            if (*bytesRead != length) {
                ::memset(buffer + *bytesRead, 0, length - *bytesRead);

                *result = S_FALSE;
            } else {
                *result = S_OK;
            }

            return true;
        }
    } else {
        return false;
    }
}

void DirectShowIOReader::flushRequests()
{
    while (m_pendingHead) {
        m_pendingHead->result = VFW_E_WRONG_STATE;

        if (m_readyTail)
            m_readyTail->next = m_pendingHead;

        m_readyTail = m_pendingHead;

        m_pendingHead = m_pendingHead->next;

        m_readyTail->next = 0;

        if (!m_pendingHead)
            m_pendingTail = 0;

        if (!m_readyHead)
            m_readyHead = m_readyTail;
    }
}

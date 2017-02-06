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

#ifndef DIRECTSHOWIOREADER_H
#define DIRECTSHOWIOREADER_H

#include <dshow.h>

#include <QtCore/qmutex.h>
#include <QtCore/qobject.h>
#include <QtCore/qwaitcondition.h>

QT_BEGIN_NAMESPACE
class QIODevice;
QT_END_NAMESPACE

class DirectShowEventLoop;
class DirectShowIOSource;
class DirectShowSampleRequest;

class DirectShowIOReader : public QObject, public IAsyncReader
{
    Q_OBJECT
public:
    DirectShowIOReader(QIODevice *device, DirectShowIOSource *source, DirectShowEventLoop *loop);
    ~DirectShowIOReader();

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    // IAsyncReader
    HRESULT STDMETHODCALLTYPE RequestAllocator(
            IMemAllocator *pPreferred, ALLOCATOR_PROPERTIES *pProps, IMemAllocator **ppActual);

    HRESULT STDMETHODCALLTYPE Request(IMediaSample *pSample, DWORD_PTR dwUser);

    HRESULT STDMETHODCALLTYPE WaitForNext(
            DWORD dwTimeout, IMediaSample **ppSample, DWORD_PTR *pdwUser);

    HRESULT STDMETHODCALLTYPE SyncReadAligned(IMediaSample *pSample);

    HRESULT STDMETHODCALLTYPE SyncRead(LONGLONG llPosition, LONG lLength, BYTE *pBuffer);

    HRESULT STDMETHODCALLTYPE Length(LONGLONG *pTotal, LONGLONG *pAvailable);

    HRESULT STDMETHODCALLTYPE BeginFlush();
    HRESULT STDMETHODCALLTYPE EndFlush();

protected:
    void customEvent(QEvent *event);

private Q_SLOTS:
    void readyRead();

private:
    HRESULT blockingRead(LONGLONG position, LONG length, BYTE *buffer, qint64 *bytesRead);
    bool nonBlockingRead(
            LONGLONG position, LONG length, BYTE *buffer, qint64 *bytesRead, HRESULT *result);
    void flushRequests();

    DirectShowIOSource *m_source;
    QIODevice *m_device;
    DirectShowEventLoop *m_loop;
    DirectShowSampleRequest *m_pendingHead;
    DirectShowSampleRequest *m_pendingTail;
    DirectShowSampleRequest *m_readyHead;
    DirectShowSampleRequest *m_readyTail;
    LONGLONG m_synchronousPosition;
    LONG m_synchronousLength;
    qint64 m_synchronousBytesRead;
    BYTE *m_synchronousBuffer;
    HRESULT m_synchronousResult;
    LONGLONG m_totalLength;
    LONGLONG m_availableLength;
    bool m_flushing;
    QMutex m_mutex;
    QWaitCondition m_wait;
};

#endif

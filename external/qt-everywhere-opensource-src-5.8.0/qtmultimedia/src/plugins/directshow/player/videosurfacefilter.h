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

#ifndef VIDEOSURFACEFILTER_H
#define VIDEOSURFACEFILTER_H

#include "directshowbasefilter.h"

#include <QtCore/qcoreevent.h>
#include <QtCore/qmutex.h>
#include <qreadwritelock.h>
#include <qsemaphore.h>
#include <qwaitcondition.h>

QT_BEGIN_NAMESPACE
class QAbstractVideoSurface;
QT_END_NAMESPACE

class DirectShowEventLoop;
class VideoSurfaceInputPin;

class VideoSurfaceFilter : public QObject
                         , public DirectShowBaseFilter
                         , public IAMFilterMiscFlags
{
    Q_OBJECT
    DIRECTSHOW_OBJECT
public:
    VideoSurfaceFilter(QAbstractVideoSurface *surface, DirectShowEventLoop *loop, QObject *parent = 0);
    ~VideoSurfaceFilter();

    // DirectShowObject
    HRESULT getInterface(REFIID riid, void **ppvObject);

    // DirectShowBaseFilter
    QList<DirectShowPin *> pins();

    // IPersist
    STDMETHODIMP GetClassID(CLSID *pClassID);

    // IMediaFilter
    STDMETHODIMP Run(REFERENCE_TIME tStart);
    STDMETHODIMP Pause();
    STDMETHODIMP Stop();

    // IAMFilterMiscFlags
    STDMETHODIMP_(ULONG) GetMiscFlags();

    // DirectShowPin (delegate)
    bool isMediaTypeSupported(const DirectShowMediaType *type);
    bool setMediaType(const DirectShowMediaType *type);
    HRESULT completeConnection(IPin *pin);
    HRESULT connectionEnded();

    // IPin (delegate)
    HRESULT EndOfStream();
    HRESULT BeginFlush();
    HRESULT EndFlush();

    // IMemInputPin (delegate)
    HRESULT Receive(IMediaSample *pMediaSample);

private Q_SLOTS:
    void supportedFormatsChanged();
    void checkEOS();

private:
    enum Events {
        StartSurface = QEvent::User,
        StopSurface = QEvent::User + 1,
        RestartSurface = QEvent::User + 2,
        FlushSurface = QEvent::User + 3,
        RenderSample = QEvent::User + 4
    };

    bool event(QEvent *);

    bool startSurface();
    void stopSurface();
    bool restartSurface();
    void flushSurface();

    bool scheduleSample(IMediaSample *sample);
    void unscheduleSample();
    void renderPendingSample();
    void clearPendingSample();

    void notifyEOS();
    void resetEOS();
    void resetEOSTimer();
    void onEOSTimerTimeout();

    friend void QT_WIN_CALLBACK EOSTimerCallback(UINT, UINT, DWORD_PTR dwUser, DWORD_PTR, DWORD_PTR);

    QMutex m_mutex;

    DirectShowEventLoop *m_loop;
    VideoSurfaceInputPin *m_pin;

    QWaitCondition m_waitSurface;
    QAbstractVideoSurface *m_surface;
    QVideoSurfaceFormat m_surfaceFormat;
    int m_bytesPerLine;
    bool m_surfaceStarted;

    QList<GUID> m_supportedTypes;
    QReadWriteLock m_typesLock;

    QMutex m_renderMutex;
    bool m_running;
    IMediaSample *m_pendingSample;
    REFERENCE_TIME m_pendingSampleEndTime;
    HANDLE m_renderEvent;
    HANDLE m_flushEvent;
    DWORD_PTR m_adviseCookie;

    bool m_EOS;
    bool m_EOSDelivered;
    UINT m_EOSTimer;

    friend class VideoSurfaceInputPin;
};

#endif

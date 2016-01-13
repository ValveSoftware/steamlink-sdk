/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef EVRD3DPRESENTENGINE_H
#define EVRD3DPRESENTENGINE_H

#include <QObject>
#include <EGL/egl.h>
#include <QMutex>
#include <d3d9types.h>
#include <QVideoSurfaceFormat>

struct IDirect3D9Ex;
struct IDirect3DDevice9;
struct IDirect3DDevice9Ex;
struct IDirect3DDeviceManager9;
struct IDirect3DSurface9;
struct IDirect3DTexture9;
struct IMFSample;
struct IMFMediaType;
struct IDirect3DSwapChain9;

// Randomly generated GUIDs
static const GUID MFSamplePresenter_SampleCounter =
{ 0xb0bb83cc, 0xf10f, 0x4e2e, { 0xaa, 0x2b, 0x29, 0xea, 0x5e, 0x92, 0xef, 0x85 } };

static const GUID MFSamplePresenter_SampleSwapChain =
{ 0xad885bd1, 0x7def, 0x414a, { 0xb5, 0xb0, 0xd3, 0xd2, 0x63, 0xd6, 0xe9, 0x6d } };

QT_BEGIN_NAMESPACE

class QAbstractVideoSurface;
class QOpenGLContext;

class EGLWrapper
{
public:
    EGLWrapper();

    __eglMustCastToProperFunctionPointerType getProcAddress(const char *procname);
    EGLSurface createPbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list);
    EGLBoolean destroySurface(EGLDisplay dpy, EGLSurface surface);
    EGLBoolean bindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
    EGLBoolean releaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);

private:
    typedef __eglMustCastToProperFunctionPointerType (EGLAPIENTRYP EglGetProcAddress)(const char *procname);
    typedef EGLSurface (EGLAPIENTRYP EglCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list);
    typedef EGLBoolean (EGLAPIENTRYP EglDestroySurface)(EGLDisplay dpy, EGLSurface surface);
    typedef EGLBoolean (EGLAPIENTRYP EglBindTexImage)(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
    typedef EGLBoolean (EGLAPIENTRYP EglReleaseTexImage)(EGLDisplay dpy, EGLSurface surface, EGLint buffer);

    EglGetProcAddress m_eglGetProcAddress;
    EglCreatePbufferSurface m_eglCreatePbufferSurface;
    EglDestroySurface m_eglDestroySurface;
    EglBindTexImage m_eglBindTexImage;
    EglReleaseTexImage m_eglReleaseTexImage;
};

class D3DPresentEngine : public QObject
{
    Q_OBJECT
public:
    D3DPresentEngine();
    virtual ~D3DPresentEngine();

    void start();
    void stop();

    HRESULT getService(REFGUID guidService, REFIID riid, void** ppv);
    HRESULT checkFormat(D3DFORMAT format);

    HRESULT createVideoSamples(IMFMediaType *format, QList<IMFSample*>& videoSampleQueue);
    void releaseResources();

    UINT refreshRate() const { return m_displayMode.RefreshRate; }

    void setSurface(QAbstractVideoSurface *surface);
    void setSurfaceFormat(const QVideoSurfaceFormat &format);

    void createOffscreenTexture();
    bool updateTexture(IDirect3DSurface9 *src);

public Q_SLOTS:
    void presentSample(void* sample, qint64 llTarget);

private:
    HRESULT initializeD3D();
    HRESULT getSwapChainPresentParameters(IMFMediaType *type, D3DPRESENT_PARAMETERS *pp);
    HRESULT createD3DDevice();
    HRESULT createD3DSample(IDirect3DSwapChain9 *swapChain, IMFSample **videoSample);

    QMutex m_mutex;

    UINT m_deviceResetToken;
    D3DDISPLAYMODE m_displayMode;

    IDirect3D9Ex *m_D3D9;
    IDirect3DDevice9Ex *m_device;
    IDirect3DDeviceManager9 *m_deviceManager;

    QVideoSurfaceFormat m_surfaceFormat;
    QAbstractVideoSurface *m_surface;

    QOpenGLContext *m_glContext;
    QWindow *m_offscreenSurface;

    EGLDisplay *m_eglDisplay;
    EGLConfig *m_eglConfig;
    EGLSurface m_eglSurface;
    unsigned int m_glTexture;
    IDirect3DTexture9 *m_texture;
    EGLWrapper *m_egl;
};

QT_END_NAMESPACE

#endif // EVRD3DPRESENTENGINE_H

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

#ifndef EVRDEFS_H
#define EVRDEFS_H

#include <d3d9.h>
#include <evr9.h>
#include <evr.h>
#include <dxva2api.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>

extern const CLSID clsid_EnhancedVideoRenderer;
extern const GUID mr_VIDEO_RENDER_SERVICE;
extern const GUID mr_VIDEO_MIXER_SERVICE;
extern const GUID mr_BUFFER_SERVICE;
extern const GUID video_ZOOM_RECT;
extern const GUID iid_IDirect3DDevice9;
extern const GUID iid_IDirect3DSurface9;

// The following is required to compile with MinGW

extern "C" {
HRESULT WINAPI MFCreateVideoSampleFromSurface(IUnknown *pUnkSurface, IMFSample **ppSample);
HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex**);
}

#ifndef PRESENTATION_CURRENT_POSITION
#define PRESENTATION_CURRENT_POSITION 0x7fffffffffffffff
#endif

#ifndef MF_E_SHUTDOWN
#define MF_E_SHUTDOWN ((HRESULT)0xC00D3E85L)
#endif

#ifndef MF_E_SAMPLEALLOCATOR_EMPTY
#define MF_E_SAMPLEALLOCATOR_EMPTY ((HRESULT)0xC00D4A3EL)
#endif

#ifndef MF_E_TRANSFORM_STREAM_CHANGE
#define MF_E_TRANSFORM_STREAM_CHANGE ((HRESULT)0xC00D6D61L)
#endif

#ifndef MF_E_TRANSFORM_NEED_MORE_INPUT
#define MF_E_TRANSFORM_NEED_MORE_INPUT ((HRESULT)0xC00D6D72L)
#endif

#ifdef __GNUC__
typedef struct MFVideoNormalizedRect {
    float left;
    float top;
    float right;
    float bottom;
} MFVideoNormalizedRect;
#endif

#include <initguid.h>

#ifndef __IMFGetService_INTERFACE_DEFINED__
#define __IMFGetService_INTERFACE_DEFINED__
DEFINE_GUID(IID_IMFGetService, 0xfa993888, 0x4383, 0x415a, 0xa9,0x30, 0xdd,0x47,0x2a,0x8c,0xf6,0xf7);
MIDL_INTERFACE("fa993888-4383-415a-a930-dd472a8cf6f7")
IMFGetService : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetService(REFGUID, REFIID, LPVOID *) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IMFGetService, 0xfa993888, 0x4383, 0x415a, 0xa9,0x30, 0xdd,0x47,0x2a,0x8c,0xf6,0xf7)
#endif
#endif // __IMFGetService_INTERFACE_DEFINED__

#ifndef __IMFVideoDisplayControl_INTERFACE_DEFINED__
#define __IMFVideoDisplayControl_INTERFACE_DEFINED__
typedef enum MFVideoAspectRatioMode
{
    MFVideoARMode_None = 0,
    MFVideoARMode_PreservePicture = 0x1,
    MFVideoARMode_PreservePixel = 0x2,
    MFVideoARMode_NonLinearStretch = 0x4,
    MFVideoARMode_Mask = 0x7
} MFVideoAspectRatioMode;

DEFINE_GUID(IID_IMFVideoDisplayControl, 0xa490b1e4, 0xab84, 0x4d31, 0xa1,0xb2, 0x18,0x1e,0x03,0xb1,0x07,0x7a);
MIDL_INTERFACE("a490b1e4-ab84-4d31-a1b2-181e03b1077a")
IMFVideoDisplayControl : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetNativeVideoSize(SIZE *, SIZE *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIdealVideoSize(SIZE *, SIZE *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetVideoPosition(const MFVideoNormalizedRect *, const LPRECT) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVideoPosition(MFVideoNormalizedRect *, LPRECT) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetAspectRatioMode(DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAspectRatioMode(DWORD *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetVideoWindow(HWND) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVideoWindow(HWND *) = 0;
    virtual HRESULT STDMETHODCALLTYPE RepaintVideo(void) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentImage(BITMAPINFOHEADER *, BYTE **, DWORD *, LONGLONG *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBorderColor(COLORREF) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBorderColor(COLORREF *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetRenderingPrefs(DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRenderingPrefs(DWORD *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFullscreen(BOOL) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFullscreen(BOOL *) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IMFVideoDisplayControl, 0xa490b1e4, 0xab84, 0x4d31, 0xa1,0xb2, 0x18,0x1e,0x03,0xb1,0x07,0x7a)
#endif
#endif // __IMFVideoDisplayControl_INTERFACE_DEFINED__

#ifndef __IMFVideoProcessor_INTERFACE_DEFINED__
#define __IMFVideoProcessor_INTERFACE_DEFINED__
DEFINE_GUID(IID_IMFVideoProcessor, 0x6AB0000C, 0xFECE, 0x4d1f, 0xA2,0xAC, 0xA9,0x57,0x35,0x30,0x65,0x6E);
MIDL_INTERFACE("6AB0000C-FECE-4d1f-A2AC-A9573530656E")
IMFVideoProcessor : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetAvailableVideoProcessorModes(UINT *, GUID **) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVideoProcessorCaps(LPGUID, DXVA2_VideoProcessorCaps *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVideoProcessorMode(LPGUID) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetVideoProcessorMode(LPGUID) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcAmpRange(DWORD, DXVA2_ValueRange *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcAmpValues(DWORD, DXVA2_ProcAmpValues *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcAmpValues(DWORD, DXVA2_ProcAmpValues *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFilteringRange(DWORD, DXVA2_ValueRange *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFilteringValue(DWORD, DXVA2_Fixed32 *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFilteringValue(DWORD, DXVA2_Fixed32 *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBackgroundColor(COLORREF *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBackgroundColor(COLORREF) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IMFVideoProcessor, 0x6AB0000C, 0xFECE, 0x4d1f, 0xA2,0xAC, 0xA9,0x57,0x35,0x30,0x65,0x6E)
#endif
#endif // __IMFVideoProcessor_INTERFACE_DEFINED__

#ifndef __IMFVideoDeviceID_INTERFACE_DEFINED__
#define __IMFVideoDeviceID_INTERFACE_DEFINED__
DEFINE_GUID(IID_IMFVideoDeviceID, 0xA38D9567, 0x5A9C, 0x4f3c, 0xB2,0x93, 0x8E,0xB4,0x15,0xB2,0x79,0xBA);
MIDL_INTERFACE("A38D9567-5A9C-4f3c-B293-8EB415B279BA")
IMFVideoDeviceID : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetDeviceID(IID *pDeviceID) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IMFVideoDeviceID, 0xA38D9567, 0x5A9C, 0x4f3c, 0xB2,0x93, 0x8E,0xB4,0x15,0xB2,0x79,0xBA)
#endif
#endif // __IMFVideoDeviceID_INTERFACE_DEFINED__

#ifndef __IMFClockStateSink_INTERFACE_DEFINED__
#define __IMFClockStateSink_INTERFACE_DEFINED__
DEFINE_GUID(IID_IMFClockStateSink, 0xF6696E82, 0x74F7, 0x4f3d, 0xA1,0x78, 0x8A,0x5E,0x09,0xC3,0x65,0x9F);
MIDL_INTERFACE("F6696E82-74F7-4f3d-A178-8A5E09C3659F")
IMFClockStateSink : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnClockStop(MFTIME hnsSystemTime) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnClockPause(MFTIME hnsSystemTime) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnClockRestart(MFTIME hnsSystemTime) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnClockSetRate(MFTIME hnsSystemTime, float flRate) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IMFClockStateSink, 0xF6696E82, 0x74F7, 0x4f3d, 0xA1,0x78, 0x8A,0x5E,0x09,0xC3,0x65,0x9F)
#endif
#endif // __IMFClockStateSink_INTERFACE_DEFINED__

#ifndef __IMFVideoPresenter_INTERFACE_DEFINED__
#define __IMFVideoPresenter_INTERFACE_DEFINED__
typedef enum MFVP_MESSAGE_TYPE
{
    MFVP_MESSAGE_FLUSH = 0,
    MFVP_MESSAGE_INVALIDATEMEDIATYPE = 0x1,
    MFVP_MESSAGE_PROCESSINPUTNOTIFY = 0x2,
    MFVP_MESSAGE_BEGINSTREAMING = 0x3,
    MFVP_MESSAGE_ENDSTREAMING = 0x4,
    MFVP_MESSAGE_ENDOFSTREAM = 0x5,
    MFVP_MESSAGE_STEP = 0x6,
    MFVP_MESSAGE_CANCELSTEP = 0x7
} MFVP_MESSAGE_TYPE;

DEFINE_GUID(IID_IMFVideoPresenter, 0x29AFF080, 0x182A, 0x4a5d, 0xAF,0x3B, 0x44,0x8F,0x3A,0x63,0x46,0xCB);
MIDL_INTERFACE("29AFF080-182A-4a5d-AF3B-448F3A6346CB")
IMFVideoPresenter : public IMFClockStateSink
{
public:
    virtual HRESULT STDMETHODCALLTYPE ProcessMessage(MFVP_MESSAGE_TYPE eMessage, ULONG_PTR ulParam) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentMediaType(IMFVideoMediaType **ppMediaType) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IMFVideoPresenter, 0x29AFF080, 0x182A, 0x4a5d, 0xAF,0x3B, 0x44,0x8F,0x3A,0x63,0x46,0xCB)
#endif
#endif // __IMFVideoPresenter_INTERFACE_DEFINED__

#ifndef __IMFRateSupport_INTERFACE_DEFINED__
#define __IMFRateSupport_INTERFACE_DEFINED__
DEFINE_GUID(IID_IMFRateSupport, 0x0a9ccdbc, 0xd797, 0x4563, 0x96,0x67, 0x94,0xec,0x5d,0x79,0x29,0x2d);
MIDL_INTERFACE("0a9ccdbc-d797-4563-9667-94ec5d79292d")
IMFRateSupport : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetSlowestRate(MFRATE_DIRECTION eDirection, BOOL fThin, float *pflRate) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFastestRate(MFRATE_DIRECTION eDirection, BOOL fThin, float *pflRate) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsRateSupported(BOOL fThin, float flRate, float *pflNearestSupportedRate) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IMFRateSupport, 0x0a9ccdbc, 0xd797, 0x4563, 0x96,0x67, 0x94,0xec,0x5d,0x79,0x29,0x2d)
#endif
#endif // __IMFRateSupport_INTERFACE_DEFINED__

#ifndef __IMFTopologyServiceLookup_INTERFACE_DEFINED__
#define __IMFTopologyServiceLookup_INTERFACE_DEFINED__
typedef enum _MF_SERVICE_LOOKUP_TYPE
{
    MF_SERVICE_LOOKUP_UPSTREAM = 0,
    MF_SERVICE_LOOKUP_UPSTREAM_DIRECT = (MF_SERVICE_LOOKUP_UPSTREAM + 1),
    MF_SERVICE_LOOKUP_DOWNSTREAM = (MF_SERVICE_LOOKUP_UPSTREAM_DIRECT + 1),
    MF_SERVICE_LOOKUP_DOWNSTREAM_DIRECT = (MF_SERVICE_LOOKUP_DOWNSTREAM + 1),
    MF_SERVICE_LOOKUP_ALL = (MF_SERVICE_LOOKUP_DOWNSTREAM_DIRECT + 1),
    MF_SERVICE_LOOKUP_GLOBAL = (MF_SERVICE_LOOKUP_ALL + 1)
} MF_SERVICE_LOOKUP_TYPE;

DEFINE_GUID(IID_IMFTopologyServiceLookup, 0xfa993889, 0x4383, 0x415a, 0xa9,0x30, 0xdd,0x47,0x2a,0x8c,0xf6,0xf7);
MIDL_INTERFACE("fa993889-4383-415a-a930-dd472a8cf6f7")
IMFTopologyServiceLookup : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE LookupService(MF_SERVICE_LOOKUP_TYPE Type,
                                                    DWORD dwIndex,
                                                    REFGUID guidService,
                                                    REFIID riid,
                                                    LPVOID *ppvObjects,
                                                    DWORD *pnObjects) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IMFTopologyServiceLookup, 0xfa993889, 0x4383, 0x415a, 0xa9,0x30, 0xdd,0x47,0x2a,0x8c,0xf6,0xf7)
#endif
#endif // __IMFTopologyServiceLookup_INTERFACE_DEFINED__

#ifndef __IMFTopologyServiceLookupClient_INTERFACE_DEFINED__
#define __IMFTopologyServiceLookupClient_INTERFACE_DEFINED__
DEFINE_GUID(IID_IMFTopologyServiceLookupClient, 0xfa99388a, 0x4383, 0x415a, 0xa9,0x30, 0xdd,0x47,0x2a,0x8c,0xf6,0xf7);
MIDL_INTERFACE("fa99388a-4383-415a-a930-dd472a8cf6f7")
IMFTopologyServiceLookupClient : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE InitServicePointers(IMFTopologyServiceLookup *pLookup) = 0;
    virtual HRESULT STDMETHODCALLTYPE ReleaseServicePointers(void) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IMFTopologyServiceLookupClient, 0xfa99388a, 0x4383, 0x415a, 0xa9,0x30, 0xdd,0x47,0x2a,0x8c,0xf6,0xf7)
#endif
#endif // __IMFTopologyServiceLookupClient_INTERFACE_DEFINED__

#ifndef __IMediaEventSink_INTERFACE_DEFINED__
#define __IMediaEventSink_INTERFACE_DEFINED__
DEFINE_GUID(IID_IMediaEventSink, 0x56a868a2, 0x0ad4, 0x11ce, 0xb0,0x3a, 0x00,0x20,0xaf,0x0b,0xa7,0x70);
MIDL_INTERFACE("56a868a2-0ad4-11ce-b03a-0020af0ba770")
IMediaEventSink : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Notify(long EventCode, LONG_PTR EventParam1, LONG_PTR EventParam2) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IMediaEventSink, 0x56a868a2, 0x0ad4, 0x11ce, 0xb0,0x3a, 0x00,0x20,0xaf,0x0b,0xa7,0x70)
#endif
#endif // __IMediaEventSink_INTERFACE_DEFINED__

#ifndef __IMFVideoRenderer_INTERFACE_DEFINED__
#define __IMFVideoRenderer_INTERFACE_DEFINED__
DEFINE_GUID(IID_IMFVideoRenderer, 0xDFDFD197, 0xA9CA, 0x43d8, 0xB3,0x41, 0x6A,0xF3,0x50,0x37,0x92,0xCD);
MIDL_INTERFACE("DFDFD197-A9CA-43d8-B341-6AF3503792CD")
IMFVideoRenderer : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE InitializeRenderer(IMFTransform *pVideoMixer,
                                                         IMFVideoPresenter *pVideoPresenter) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IMFVideoRenderer, 0xDFDFD197, 0xA9CA, 0x43d8, 0xB3,0x41, 0x6A,0xF3,0x50,0x37,0x92,0xCD)
#endif
#endif // __IMFVideoRenderer_INTERFACE_DEFINED__

#ifndef __IMFTrackedSample_INTERFACE_DEFINED__
#define __IMFTrackedSample_INTERFACE_DEFINED__
DEFINE_GUID(IID_IMFTrackedSample, 0x245BF8E9, 0x0755, 0x40f7, 0x88,0xA5, 0xAE,0x0F,0x18,0xD5,0x5E,0x17);
MIDL_INTERFACE("245BF8E9-0755-40f7-88A5-AE0F18D55E17")
IMFTrackedSample : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE SetAllocator(IMFAsyncCallback *pSampleAllocator, IUnknown *pUnkState) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IMFTrackedSample, 0x245BF8E9, 0x0755, 0x40f7, 0x88,0xA5, 0xAE,0x0F,0x18,0xD5,0x5E,0x17)
#endif
#endif // __IMFTrackedSample_INTERFACE_DEFINED__

#ifndef __IMFDesiredSample_INTERFACE_DEFINED__
#define __IMFDesiredSample_INTERFACE_DEFINED__
DEFINE_GUID(IID_IMFDesiredSample, 0x56C294D0, 0x753E, 0x4260, 0x8D,0x61, 0xA3,0xD8,0x82,0x0B,0x1D,0x54);
MIDL_INTERFACE("56C294D0-753E-4260-8D61-A3D8820B1D54")
IMFDesiredSample : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetDesiredSampleTimeAndDuration(LONGLONG *phnsSampleTime,
                                                                      LONGLONG *phnsSampleDuration) = 0;
    virtual void STDMETHODCALLTYPE SetDesiredSampleTimeAndDuration(LONGLONG hnsSampleTime,
                                                                   LONGLONG hnsSampleDuration) = 0;
    virtual void STDMETHODCALLTYPE Clear( void) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IMFDesiredSample, 0x56C294D0, 0x753E, 0x4260, 0x8D,0x61, 0xA3,0xD8,0x82,0x0B,0x1D,0x54)
#endif
#endif

#endif // EVRDEFS_H


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

#ifndef DIRECTSHOWGLOBAL_H
#define DIRECTSHOWGLOBAL_H

#include <QtCore/qglobal.h>

#include <dshow.h>

DEFINE_GUID(MEDIASUBTYPE_I420,
        0x30323449,0x0000,0x0010,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71);

extern const GUID MEDIASUBTYPE_RGB24;
extern const GUID MEDIASUBTYPE_RGB32;
extern const GUID MEDIASUBTYPE_YUY2;
extern const GUID MEDIASUBTYPE_MJPG;
extern const GUID MEDIASUBTYPE_RGB555;
extern const GUID MEDIASUBTYPE_YVU9;
extern const GUID MEDIASUBTYPE_UYVY;
extern const GUID PIN_CATEGORY_CAPTURE;
extern const GUID PIN_CATEGORY_PREVIEW;

extern const IID IID_IPropertyBag;
extern const IID IID_ISampleGrabber;
extern const IID IID_ICaptureGraphBuilder2;
extern const IID IID_IAMStreamConfig;


extern const CLSID CLSID_CVidCapClassManager;
extern const CLSID CLSID_VideoInputDeviceCategory;
extern const CLSID CLSID_SampleGrabber;
extern const CLSID CLSID_CaptureGraphBuilder2;

#define SAFE_RELEASE(x) { if(x) x->Release(); x = NULL; }

typedef struct IFileSinkFilter *LPFILESINKFILTER;
typedef struct IAMCopyCaptureFileProgress *LPAMCOPYCAPTUREFILEPROGRESS;

#ifndef __ICaptureGraphBuilder2_INTERFACE_DEFINED__
#define __ICaptureGraphBuilder2_INTERFACE_DEFINED__
struct ICaptureGraphBuilder2 : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE SetFiltergraph(
        /* [in] */ IGraphBuilder *pfg) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetFiltergraph(
        /* [out] */ IGraphBuilder **ppfg) = 0;

    virtual HRESULT STDMETHODCALLTYPE SetOutputFileName(
        /* [in] */ const GUID *pType,
        /* [in] */ LPCOLESTR lpstrFile,
        /* [out] */ IBaseFilter **ppf,
        /* [out] */ IFileSinkFilter **ppSink) = 0;

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE FindInterface(
        /* [in] */ const GUID *pCategory,
        /* [in] */ const GUID *pType,
        /* [in] */ IBaseFilter *pf,
        /* [in] */ REFIID riid,
        /* [out] */ void **ppint) = 0;

    virtual HRESULT STDMETHODCALLTYPE RenderStream(
        /* [in] */ const GUID *pCategory,
        /* [in] */ const GUID *pType,
        /* [in] */ IUnknown *pSource,
        /* [in] */ IBaseFilter *pfCompressor,
        /* [in] */ IBaseFilter *pfRenderer) = 0;

    virtual HRESULT STDMETHODCALLTYPE ControlStream(
        /* [in] */ const GUID *pCategory,
        /* [in] */ const GUID *pType,
        /* [in] */ IBaseFilter *pFilter,
        /* [in] */ REFERENCE_TIME *pstart,
        /* [in] */ REFERENCE_TIME *pstop,
        /* [in] */ WORD wStartCookie,
        /* [in] */ WORD wStopCookie) = 0;

    virtual HRESULT STDMETHODCALLTYPE AllocCapFile(
        /* [in] */ LPCOLESTR lpstr,
        /* [in] */ DWORDLONG dwlSize) = 0;

    virtual HRESULT STDMETHODCALLTYPE CopyCaptureFile(
        /* [in] */ LPOLESTR lpwstrOld,
        /* [in] */ LPOLESTR lpwstrNew,
        /* [in] */ int fAllowEscAbort,
        /* [in] */ IAMCopyCaptureFileProgress *pCallback) = 0;

    virtual HRESULT STDMETHODCALLTYPE FindPin(
        /* [in] */ IUnknown *pSource,
        /* [in] */ PIN_DIRECTION pindir,
        /* [in] */ const GUID *pCategory,
        /* [in] */ const GUID *pType,
        /* [in] */ BOOL fUnconnected,
        /* [in] */ int num,
        /* [out] */ IPin **ppPin) = 0;

};
#endif

#ifndef __IAMStreamConfig_INTERFACE_DEFINED__
#define __IAMStreamConfig_INTERFACE_DEFINED__
struct IAMStreamConfig : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE SetFormat(
        /* [in] */ AM_MEDIA_TYPE *pmt) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetFormat(
        /* [out] */ AM_MEDIA_TYPE **ppmt) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetNumberOfCapabilities(
        /* [out] */ int *piCount,
        /* [out] */ int *piSize) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetStreamCaps(
        /* [in] */ int iIndex,
        /* [out] */ AM_MEDIA_TYPE **ppmt,
        /* [out] */ BYTE *pSCC) = 0;

};
#endif

#ifndef __IErrorLog_INTERFACE_DEFINED__
#define __IErrorLog_INTERFACE_DEFINED__
struct IErrorLog : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE AddError(
            /* [in] */ LPCOLESTR pszPropName,
            /* [in] */ EXCEPINFO *pExcepInfo) = 0;

    };
#endif

#ifndef __IPropertyBag_INTERFACE_DEFINED__
#define __IPropertyBag_INTERFACE_DEFINED__
struct IPropertyBag : public IUnknown
{
public:
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Read(
        /* [in] */ LPCOLESTR pszPropName,
        /* [out][in] */ VARIANT *pVar,
        /* [in] */ IErrorLog *pErrorLog) = 0;

    virtual HRESULT STDMETHODCALLTYPE Write(
        /* [in] */ LPCOLESTR pszPropName,
        /* [in] */ VARIANT *pVar) = 0;

};
#endif

typedef struct IMediaSample *LPMEDIASAMPLE;

EXTERN_C const IID IID_ISampleGrabberCB;

#ifndef __ISampleGrabberCB_INTERFACE_DEFINED__
#define __ISampleGrabberCB_INTERFACE_DEFINED__

#undef INTERFACE
#define INTERFACE ISampleGrabberCB
DECLARE_INTERFACE_(ISampleGrabberCB, IUnknown)
{
//    STDMETHOD(QueryInterface) (THIS_ const GUID *, void **) PURE;
    STDMETHOD(QueryInterface) (THIS_ REFIID riid, void **) PURE;
    STDMETHOD_(ULONG, AddRef) (THIS) PURE;
    STDMETHOD_(ULONG, Release) (THIS) PURE;
    STDMETHOD_(HRESULT, SampleCB) (THIS_ double, LPMEDIASAMPLE) PURE;
    STDMETHOD_(HRESULT, BufferCB) (THIS_ double, BYTE *, long) PURE;
};
#undef INTERFACE

#endif


#ifndef __ISampleGrabber_INTERFACE_DEFINED__
#define __ISampleGrabber_INTERFACE_DEFINED__

#define INTERFACE ISampleGrabber
DECLARE_INTERFACE_(ISampleGrabber,IUnknown)
{
  STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
  STDMETHOD_(ULONG,AddRef)(THIS) PURE;
  STDMETHOD_(ULONG,Release)(THIS) PURE;
  STDMETHOD(SetOneShot)(THIS_ BOOL) PURE;
  STDMETHOD(SetMediaType)(THIS_ const AM_MEDIA_TYPE*) PURE;
  STDMETHOD(GetConnectedMediaType)(THIS_ AM_MEDIA_TYPE*) PURE;
  STDMETHOD(SetBufferSamples)(THIS_ BOOL) PURE;
  STDMETHOD(GetCurrentBuffer)(THIS_ long*,long*) PURE;
  STDMETHOD(GetCurrentSample)(THIS_ IMediaSample**) PURE;
  STDMETHOD(SetCallback)(THIS_ ISampleGrabberCB *,long) PURE;
};
#undef INTERFACE
#endif


#endif

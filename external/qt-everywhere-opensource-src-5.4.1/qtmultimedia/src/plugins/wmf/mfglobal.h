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

#ifndef MFGLOBAL_H
#define MFGLOBAL_H

#include <mfapi.h>
#include <mfidl.h>
#include <Mferror.h>


template<class T>
class AsyncCallback : public IMFAsyncCallback
{
public:
    typedef HRESULT (T::*InvokeFn)(IMFAsyncResult *asyncResult);

    AsyncCallback(T *parent, InvokeFn fn) : m_parent(parent), m_invokeFn(fn)
    {
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv)
    {
        if (!ppv)
            return E_POINTER;

        if (iid == __uuidof(IUnknown)) {
            *ppv = static_cast<IUnknown*>(static_cast<IMFAsyncCallback*>(this));
        } else if (iid == __uuidof(IMFAsyncCallback)) {
            *ppv = static_cast<IMFAsyncCallback*>(this);
        } else {
            *ppv = NULL;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    STDMETHODIMP_(ULONG) AddRef() {
        // Delegate to parent class.
        return m_parent->AddRef();
    }
    STDMETHODIMP_(ULONG) Release() {
        // Delegate to parent class.
        return m_parent->Release();
    }


    // IMFAsyncCallback methods
    STDMETHODIMP GetParameters(DWORD*, DWORD*)
    {
        // Implementation of this method is optional.
        return E_NOTIMPL;
    }

    STDMETHODIMP Invoke(IMFAsyncResult* asyncResult)
    {
        return (m_parent->*m_invokeFn)(asyncResult);
    }

    T *m_parent;
    InvokeFn m_invokeFn;
};

template <class T> void qt_wmf_safeRelease(T **ppT)
{
    if (*ppT) {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

template <class T>
void qt_wmf_copyComPointer(T* &dest, T *src)
{
    if (dest)
        dest->Release();
    dest = src;
    if (dest)
        dest->AddRef();
}

HRESULT qt_wmf_getFourCC(IMFMediaType *type, DWORD *fourCC);
MFRatio qt_wmf_getPixelAspectRatio(IMFMediaType *type);
bool qt_wmf_areMediaTypesEqual(IMFMediaType *type1, IMFMediaType *type2);
HRESULT qt_wmf_validateVideoArea(const MFVideoArea& area, UINT32 width, UINT32 height);
bool qt_wmf_isSampleTimePassed(IMFClock *clock, IMFSample *sample);

inline float qt_wmf_MFOffsetToFloat(const MFOffset& offset)
{
    return offset.value + (float(offset.fract) / 65536);
}

inline MFOffset qt_wmf_makeMFOffset(float v)
{
    MFOffset offset;
    offset.value = short(v);
    offset.fract = WORD(65536 * (v-offset.value));
    return offset;
}

inline MFVideoArea qt_wmf_makeMFArea(float x, float y, DWORD width, DWORD height)
{
    MFVideoArea area;
    area.OffsetX = qt_wmf_makeMFOffset(x);
    area.OffsetY = qt_wmf_makeMFOffset(y);
    area.Area.cx = width;
    area.Area.cy = height;
    return area;
}

inline HRESULT qt_wmf_getFrameRate(IMFMediaType *pType, MFRatio *pRatio)
{
    return MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, (UINT32*)&pRatio->Numerator, (UINT32*)&pRatio->Denominator);
}


#endif // MFGLOBAL_H

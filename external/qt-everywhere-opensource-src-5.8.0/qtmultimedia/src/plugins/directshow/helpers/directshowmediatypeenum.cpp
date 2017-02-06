/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "directshowmediatypeenum.h"

#include "directshowpin.h"

DirectShowMediaTypeEnum::DirectShowMediaTypeEnum(DirectShowPin *pin)
    : m_pin(pin)
    , m_mediaTypes(pin->supportedMediaTypes())
    , m_index(0)
{
    m_pin->AddRef();
}

DirectShowMediaTypeEnum::DirectShowMediaTypeEnum(const QList<DirectShowMediaType> &types)
    : m_pin(NULL)
    , m_mediaTypes(types)
    , m_index(0)
{
}

DirectShowMediaTypeEnum::~DirectShowMediaTypeEnum()
{
    if (m_pin)
        m_pin->Release();
}

HRESULT DirectShowMediaTypeEnum::getInterface(REFIID riid, void **ppvObject)
{
    if (riid == IID_IEnumMediaTypes) {
        return GetInterface(static_cast<IEnumMediaTypes *>(this), ppvObject);
    } else {
        return DirectShowObject::getInterface(riid, ppvObject);
    }
}

HRESULT DirectShowMediaTypeEnum::Next(ULONG cMediaTypes, AM_MEDIA_TYPE **ppMediaTypes, ULONG *pcFetched)
{
    if (ppMediaTypes && (pcFetched || cMediaTypes == 1)) {
        ULONG count = qBound<ULONG>(0, cMediaTypes, m_mediaTypes.count() - m_index);

        for (ULONG i = 0; i < count; ++i, ++m_index) {
            ppMediaTypes[i] = reinterpret_cast<AM_MEDIA_TYPE *>(CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE)));
            DirectShowMediaType::copy(ppMediaTypes[i], m_mediaTypes.at(m_index));
        }

        if (pcFetched)
            *pcFetched = count;

        return count == cMediaTypes ? S_OK : S_FALSE;
    } else {
        return E_POINTER;
    }
}

HRESULT DirectShowMediaTypeEnum::Skip(ULONG cMediaTypes)
{
    m_index = qMin(int(m_index + cMediaTypes), m_mediaTypes.count());
    return m_index < m_mediaTypes.count() ? S_OK : S_FALSE;
}

HRESULT DirectShowMediaTypeEnum::Reset()
{
    m_index = 0;
    return S_OK;
}

HRESULT DirectShowMediaTypeEnum::Clone(IEnumMediaTypes **ppEnum)
{
    if (ppEnum) {
        if (m_pin)
            *ppEnum = new DirectShowMediaTypeEnum(m_pin);
        else
            *ppEnum = new DirectShowMediaTypeEnum(m_mediaTypes);
        return S_OK;
    } else {
        return E_POINTER;
    }
}


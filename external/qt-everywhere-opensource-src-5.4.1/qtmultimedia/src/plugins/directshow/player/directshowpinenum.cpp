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

#include "directshowpinenum.h"


DirectShowPinEnum::DirectShowPinEnum(const QList<IPin *> &pins)
    : m_ref(1)
    , m_pins(pins)
    , m_index(0)
{
    foreach (IPin *pin, m_pins)
        pin->AddRef();
}

DirectShowPinEnum::~DirectShowPinEnum()
{
    foreach (IPin *pin, m_pins)
        pin->Release();
}

HRESULT DirectShowPinEnum::QueryInterface(REFIID riid, void **ppvObject)
{
    if (riid == IID_IUnknown
            || riid == IID_IEnumPins) {
        AddRef();

        *ppvObject = static_cast<IEnumPins *>(this);

        return S_OK;
    } else {
        *ppvObject = 0;

        return E_NOINTERFACE;
    }
}

ULONG DirectShowPinEnum::AddRef()
{
    return InterlockedIncrement(&m_ref);
}

ULONG DirectShowPinEnum::Release()
{
    ULONG ref = InterlockedDecrement(&m_ref);

    if (ref == 0) {
        delete this;
    }

    return ref;
}

HRESULT DirectShowPinEnum::Next(ULONG cPins, IPin **ppPins, ULONG *pcFetched)
{
    if (ppPins && (pcFetched || cPins == 1)) {
        ULONG count = qBound<ULONG>(0, cPins, m_pins.count() - m_index);

        for (ULONG i = 0; i < count; ++i, ++m_index) {
            ppPins[i] = m_pins.at(m_index);
            ppPins[i]->AddRef();
        }

        if (pcFetched)
            *pcFetched = count;

        return count == cPins ? S_OK : S_FALSE;
    } else {
        return E_POINTER;
    }
}

HRESULT DirectShowPinEnum::Skip(ULONG cPins)
{
    m_index = qMin(int(m_index + cPins), m_pins.count());

    return m_index < m_pins.count() ? S_OK : S_FALSE;
}

HRESULT DirectShowPinEnum::Reset()
{
    m_index = 0;

    return S_OK;
}

HRESULT DirectShowPinEnum::Clone(IEnumPins **ppEnum)
{
    if (ppEnum) {
        *ppEnum = new DirectShowPinEnum(m_pins);

        return S_OK;
    } else {
        return E_POINTER;
    }
}

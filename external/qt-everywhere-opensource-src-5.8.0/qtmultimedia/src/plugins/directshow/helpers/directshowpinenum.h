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

#ifndef DIRECTSHOWPINENUM_H
#define DIRECTSHOWPINENUM_H

#include <dshow.h>

#include <QtCore/qlist.h>
#include "directshowpin.h"

QT_USE_NAMESPACE

class DirectShowBaseFilter;

class DirectShowPinEnum : public DirectShowObject
                        , public IEnumPins
{
    DIRECTSHOW_OBJECT

public:
    DirectShowPinEnum(DirectShowBaseFilter *filter);
    DirectShowPinEnum(const QList<IPin *> &pins);
    ~DirectShowPinEnum();

    // DirectShowObject
    HRESULT getInterface(REFIID riid, void **ppvObject);

    // IEnumPins
    STDMETHODIMP Next(ULONG cPins, IPin **ppPins, ULONG *pcFetched);
    STDMETHODIMP Skip(ULONG cPins);
    STDMETHODIMP Reset();
    STDMETHODIMP Clone(IEnumPins **ppEnum);

private:
    Q_DISABLE_COPY(DirectShowPinEnum)

    DirectShowBaseFilter *m_filter;
    QList<IPin *> m_pins;
    int m_index;
};

#endif

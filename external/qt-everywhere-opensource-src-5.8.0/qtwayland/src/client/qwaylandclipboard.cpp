/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qwaylandclipboard_p.h"
#include "qwaylanddisplay_p.h"
#include "qwaylandinputdevice_p.h"
#include "qwaylanddataoffer_p.h"
#include "qwaylanddatasource_p.h"
#include "qwaylanddatadevice_p.h"

#if QT_CONFIG(draganddrop)

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandClipboard::QWaylandClipboard(QWaylandDisplay *display)
    : mDisplay(display)
{
}

QWaylandClipboard::~QWaylandClipboard()
{
}

QMimeData *QWaylandClipboard::mimeData(QClipboard::Mode mode)
{
    if (mode != QClipboard::Clipboard)
        return &m_emptyData;

    QWaylandInputDevice *inputDevice = mDisplay->currentInputDevice();
    if (!inputDevice || !inputDevice->dataDevice())
        return &m_emptyData;

    QWaylandDataSource *source = inputDevice->dataDevice()->selectionSource();
    if (source) {
        return source->mimeData();
    }

    if (inputDevice->dataDevice()->selectionOffer())
        return inputDevice->dataDevice()->selectionOffer()->mimeData();

    return &m_emptyData;
}

void QWaylandClipboard::setMimeData(QMimeData *data, QClipboard::Mode mode)
{
    if (mode != QClipboard::Clipboard)
        return;

    QWaylandInputDevice *inputDevice = mDisplay->currentInputDevice();
    if (!inputDevice || !inputDevice->dataDevice())
        return;

    static const QString plain = QStringLiteral("text/plain");
    static const QString utf8 = QStringLiteral("text/plain;charset=utf-8");
    if (data && data->hasFormat(plain) && !data->hasFormat(utf8))
        data->setData(utf8, data->data(plain));
    inputDevice->dataDevice()->setSelectionSource(data ? new QWaylandDataSource(mDisplay->dndSelectionHandler(), data) : 0);

    emitChanged(mode);
}

bool QWaylandClipboard::supportsMode(QClipboard::Mode mode) const
{
    return mode == QClipboard::Clipboard;
}

bool QWaylandClipboard::ownsMode(QClipboard::Mode mode) const
{
    if (mode != QClipboard::Clipboard)
        return false;

    QWaylandInputDevice *inputDevice = mDisplay->currentInputDevice();
    if (!inputDevice || !inputDevice->dataDevice())
        return false;

    return inputDevice->dataDevice()->selectionSource() != 0;
}

}

QT_END_NAMESPACE

#endif  // draganddrop

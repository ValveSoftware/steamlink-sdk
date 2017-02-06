/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the MacJp2 plugin in the Qt ImageFormats module.
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

#include "qmacjp2handler.h"
#include "qiiofhelpers_p.h"
#include <QVariant>

QT_BEGIN_NAMESPACE

class QMacJp2HandlerPrivate
{
    Q_DECLARE_PUBLIC(QMacJp2Handler)
    Q_DISABLE_COPY(QMacJp2HandlerPrivate)
public:
    QMacJp2HandlerPrivate(QMacJp2Handler *q_ptr)
        : writeQuality(-1), q_ptr(q_ptr)
    {}

    int writeQuality;
    QMacJp2Handler *q_ptr;
};


QMacJp2Handler::QMacJp2Handler()
    : d_ptr(new QMacJp2HandlerPrivate(this))
{
}

QMacJp2Handler::~QMacJp2Handler()
{
}

bool QMacJp2Handler::canRead(QIODevice *iod)
{
    bool bCanRead = false;
    char buf[12];
    if (iod && iod->peek(buf, 12) == 12)
        bCanRead = !qstrncmp(buf, "\000\000\000\fjP  \r\n\207\n", 12);
    return bCanRead;
}

bool QMacJp2Handler::canRead() const
{
    if (canRead(device())) {
        setFormat("jp2");
        return true;
    }
    return false;
}

bool QMacJp2Handler::read(QImage *image)
{
    return QIIOFHelpers::readImage(this, image);
}

bool QMacJp2Handler::write(const QImage &image)
{
    return QIIOFHelpers::writeImage(this, image, QStringLiteral("public.jpeg-2000"));
}

QVariant QMacJp2Handler::option(ImageOption option) const
{
    Q_D(const QMacJp2Handler);
    if (option == Quality)
        return QVariant(d->writeQuality);
    return QVariant();
}

void QMacJp2Handler::setOption(ImageOption option, const QVariant &value)
{
    Q_D(QMacJp2Handler);
    if (option == Quality) {
        bool ok;
        const int quality = value.toInt(&ok);
        if (ok)
            d->writeQuality = quality;
    }
}

bool QMacJp2Handler::supportsOption(ImageOption option) const
{
    return (option == Quality);
}

QByteArray QMacJp2Handler::name() const
{
    return QByteArrayLiteral("jp2");
}


QT_END_NAMESPACE

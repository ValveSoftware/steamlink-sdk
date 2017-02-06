/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Petroules Corporation.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the JP2 plugins in the Qt ImageFormats module.
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

#ifndef QJP2HANDLER_H
#define QJP2HANDLER_H

#include <QtCore/qscopedpointer.h>
#include <QtGui/qimageiohandler.h>

QT_BEGIN_NAMESPACE

class QImage;
class QByteArray;
class QIODevice;
class QVariant;
class QJp2HandlerPrivate;

class QJp2Handler : public QImageIOHandler
{
public:
    QJp2Handler();
    virtual ~QJp2Handler();
    static bool canRead(QIODevice *iod, QByteArray *subType);
    virtual bool canRead() const;
    virtual bool read(QImage *image);
    virtual bool write(const QImage &image);
    virtual QVariant option(ImageOption option) const;
    virtual void setOption(ImageOption option, const QVariant &value);
    virtual bool supportsOption(ImageOption option) const;
    virtual QByteArray name() const;

private:
    Q_DECLARE_PRIVATE(QJp2Handler)
    QScopedPointer<QJp2HandlerPrivate> d_ptr;
};

QT_END_NAMESPACE

#endif // QJP2HANDLER_P_H

/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the WBMP plugin in the Qt ImageFormats module.
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

#include <qimageiohandler.h>

#ifndef QT_NO_IMAGEFORMATPLUGIN

#ifdef QT_NO_IMAGEFORMAT_WBMP
#undef QT_NO_IMAGEFORMAT_WBMP
#endif

#include "qwbmphandler_p.h"

QT_BEGIN_NAMESPACE

class QWbmpPlugin : public QImageIOPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QImageIOHandlerFactoryInterface" FILE "wbmp.json")

public:
    QStringList keys() const;
    QImageIOPlugin::Capabilities capabilities(QIODevice *device, const QByteArray &format) const;
    QImageIOHandler *create(QIODevice *device, const QByteArray &format = QByteArray()) const;
};

QImageIOPlugin::Capabilities QWbmpPlugin::capabilities(QIODevice *device, const QByteArray &format) const
{
    if (format == "wbmp")
        return Capabilities(CanRead | CanWrite);

    if (!format.isEmpty())
        return 0;

    if (!device->isOpen())
        return 0;

    Capabilities cap;
    if (device->isReadable() && QWbmpHandler::canRead(device))
        cap |= CanRead;

    if (device->isWritable())
        cap |= CanWrite;

    return cap;
}

QImageIOHandler * QWbmpPlugin::create(QIODevice *device, const QByteArray &format) const
{
    QImageIOHandler *handler = new QWbmpHandler(device);

    handler->setFormat(format);
    return handler;
}

QStringList QWbmpPlugin::keys() const
{
    return QStringList() << QLatin1String("wbmp");
}

QT_END_NAMESPACE

#include "main.moc"

#endif /* QT_NO_IMAGEFORMATPLUGIN */

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


#ifndef QGSTREAMERMEDIACONTAINERCONTROL_H
#define QGSTREAMERMEDIACONTAINERCONTROL_H

#include <qmediacontainercontrol.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qset.h>

#include <gst/gst.h>

QT_BEGIN_NAMESPACE

class QGstreamerMediaContainerControl : public QMediaContainerControl
{
Q_OBJECT
public:
    QGstreamerMediaContainerControl(QObject *parent);
    virtual ~QGstreamerMediaContainerControl() {};

    virtual QStringList supportedContainers() const { return m_supportedContainers; }
    virtual QString containerFormat() const { return m_format; }
    virtual void setContainerFormat(const QString &formatMimeType) { m_format = formatMimeType; }

    virtual QString containerDescription(const QString &formatMimeType) const { return m_containerDescriptions.value(formatMimeType); }

    QByteArray formatElementName() const { return m_elementNames.value(containerFormat()); }

    QSet<QString> supportedStreamTypes(const QString &container) const;

    static QSet<QString> supportedStreamTypes(GstElementFactory *factory, GstPadDirection direction);

    QString containerExtension() const;

private:
    QString m_format;
    QStringList m_supportedContainers;
    QMap<QString,QByteArray> m_elementNames;
    QMap<QString, QString> m_containerDescriptions;
    QMap<QString, QString> m_containerExtensions;
    QMap<QString, QSet<QString> > m_streamTypes;
};

QT_END_NAMESPACE

#endif // QGSTREAMERMEDIACONTAINERCONTROL_H

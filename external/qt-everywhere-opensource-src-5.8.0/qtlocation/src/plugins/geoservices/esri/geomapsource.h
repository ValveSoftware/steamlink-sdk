/****************************************************************************
**
** Copyright (C) 2013-2016 Esri <contracts@esri.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#ifndef GEOMAPSOURCE_H
#define GEOMAPSOURCE_H

#include <QtLocation/private/qgeomaptype_p.h>

QT_BEGIN_NAMESPACE

class GeoMapSource : public QGeoMapType
{
public:
    GeoMapSource(QGeoMapType::MapStyle style, const QString &name,
                 const QString &description, bool mobile, bool night, int mapId,
                 const QString &url, const QString &copyright);

    inline const QString &url() const;
    inline const QString &copyright() const;

    static QString toFormat(const QString &url);
    static QGeoMapType::MapStyle mapStyle(const QString &styleString);

private:
    QString m_url;
    QString m_copyright;
};

inline const QString &GeoMapSource::url() const
{
    return m_url;
}

inline const QString &GeoMapSource::copyright() const
{
    return m_copyright;
}

QT_END_NAMESPACE

#endif // GEOMAPSOURCE_H

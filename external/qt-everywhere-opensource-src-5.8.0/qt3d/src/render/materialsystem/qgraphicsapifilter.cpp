/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qgraphicsapifilter.h"
#include "qgraphicsapifilter_p.h"
#include "private/qobject_p.h"
#include <QOpenGLContext>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

GraphicsApiFilterData::GraphicsApiFilterData()
    : m_api(QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL ? QGraphicsApiFilter::OpenGL : QGraphicsApiFilter::OpenGLES)
    , m_profile(QGraphicsApiFilter::NoProfile) // matches all (no profile, core, compat)
    , m_minor(0)
    , m_major(0)
{}

bool GraphicsApiFilterData::operator ==(const GraphicsApiFilterData &other) const
{
    // Check API
    if (other.m_api != m_api)
        return false;

    // Check versions
    const bool versionsCompatible = other.m_major < m_major
            || (other.m_major == m_major && other.m_minor <= m_minor);
    if (!versionsCompatible)
        return false;

    // Check profiles if requiring OpenGL (profiles not relevant for OpenGL ES)
    if (other.m_api == QGraphicsApiFilter::OpenGL) {
        const bool profilesCompatible = m_profile != QGraphicsApiFilter::CoreProfile
                || other.m_profile == m_profile;
        if (!profilesCompatible)
            return false;
    }

    // Check extensions
    for (const QString &neededExt : other.m_extensions) {
        if (!m_extensions.contains(neededExt))
            return false;
    }

    // Check vendor
    if (!other.m_vendor.isEmpty())
        return (other.m_vendor == m_vendor);

    // If we get here everything looks good :)
    return true;
}

bool GraphicsApiFilterData::operator <(const GraphicsApiFilterData &other) const
{
    if (this->m_major > other.m_major)
        return false;
    if (this->m_major == other.m_major &&
        this->m_minor > other.m_minor)
        return false;
    return true;
}

bool GraphicsApiFilterData::operator !=(const GraphicsApiFilterData &other) const
{
    return !(*this == other);
}

QGraphicsApiFilterPrivate *QGraphicsApiFilterPrivate::get(QGraphicsApiFilter *q)
{
    return q->d_func();
}

/*!
    \class Qt3DRender::QGraphicsApiFilter
    \inmodule Qt3DRender
    \since 5.5
    \brief The QGraphicsApiFilter class identifies the API required for the attached QTechnique
*/

/*!
    \qmltype GraphicsApiFilter
    \instantiates Qt3DRender::QGraphicsApiFilter
    \inherits QtObject
    \inqmlmodule Qt3D.Render
    \since 5.5
    \brief For OpenGL identifies the API required for the attached technique
*/

/*!
    \enum QGraphicsApiFilter::OpenGLProfile

    This enum identifies the type of profile required
    \value NoProfile
    \value CoreProfile
    \value CompatibilityProfile
*/

/*! \fn Qt3DRender::QGraphicsApiFilter::QGraphicsApiFilter(QObject *parent)
  Constructs a new QGraphicsApiFilter with the specified \a parent.
 */
QGraphicsApiFilter::QGraphicsApiFilter(QObject *parent)
    : QObject(*new QGraphicsApiFilterPrivate, parent)
{
}

/*! \internal */
QGraphicsApiFilter::~QGraphicsApiFilter()
{
}

/*!
  \enum Qt3DRender::QGraphicsApiFilter::Api

  \value OpenGLES QSurfaceFormat::OpenGLES
  \value OpenGL QSurfaceFormat::OpenGL

*/

/*!
  \enum Qt3DRender::QGraphicsApiFilter::Profile

  \value NoProfile QSurfaceFormat::NoProfile
  \value CoreProfile QSurfaceFormat::CoreProfile
  \value CompatibilityProfile QSurfaceFormat::CompatibilityProfile

*/

/*!
  \property Qt3DRender::QGraphicsApiFilter::api

*/

/*!
  \qmlproperty enumeration Qt3D.Render::GraphicsApiFilter::api


  \value OpenGLES QSurfaceFormat::OpenGLES
  \value OpenGL QSurfaceFormat::OpenGL
*/

QGraphicsApiFilter::Api QGraphicsApiFilter::api() const
{
    Q_D(const QGraphicsApiFilter);
    return d->m_data.m_api;
}

/*!
  \property Qt3DRender::QGraphicsApiFilter::profile

*/

/*!
  \qmlproperty enumeration Qt3D.Render::GraphicsApiFilter::profile

  \value NoProfile QSurfaceFormat::NoProfile
  \value CoreProfile QSurfaceFormat::CoreProfile
  \value CompatibilityProfile QSurfaceFormat::CompatibilityProfile
*/

QGraphicsApiFilter::OpenGLProfile QGraphicsApiFilter::profile() const
{
    Q_D(const QGraphicsApiFilter);
    return d->m_data.m_profile;
}

/*!
  \property Qt3DRender::QGraphicsApiFilter::minorVersion

 */

/*!
  \qmlproperty int Qt3D.Render::GraphicsApiFilter::minorVersion

*/

int QGraphicsApiFilter::minorVersion() const
{
    Q_D(const QGraphicsApiFilter);
    return d->m_data.m_minor;
}

/*!
  \property Qt3DRender::QGraphicsApiFilter::majorVersion

 */

/*!
  \qmlproperty int Qt3D.Render::GraphicsApiFilter::majorVersion

*/

int QGraphicsApiFilter::majorVersion() const
{
    Q_D(const QGraphicsApiFilter);
    return d->m_data.m_major;
}

/*!
  \property Qt3DRender::QGraphicsApiFilter::extensions

 */

/*!
  \qmlproperty stringlist Qt3D.Render::GraphicsApiFilter::extensions

*/

QStringList QGraphicsApiFilter::extensions() const
{
    Q_D(const QGraphicsApiFilter);
    return d->m_data.m_extensions;
}

/*!
  \property Qt3DRender::QGraphicsApiFilter::vendor

 */

/*!
  \qmlproperty string Qt3D.Render::GraphicsApiFilter::vendor

*/

QString QGraphicsApiFilter::vendor() const
{
    Q_D(const QGraphicsApiFilter);
    return d->m_data.m_vendor;
}

void QGraphicsApiFilter::setApi(QGraphicsApiFilter::Api api)
{
    Q_D(QGraphicsApiFilter);
    if (d->m_data.m_api != api) {
        d->m_data.m_api = api;
        emit apiChanged(api);
        emit graphicsApiFilterChanged();
    }
}

void QGraphicsApiFilter::setProfile(QGraphicsApiFilter::OpenGLProfile profile)
{
    Q_D(QGraphicsApiFilter);
    if (d->m_data.m_profile != profile) {
        d->m_data.m_profile = profile;
        emit profileChanged(profile);
        emit graphicsApiFilterChanged();
    }
}

void QGraphicsApiFilter::setMinorVersion(int minorVersion)
{
    Q_D(QGraphicsApiFilter);
    if (minorVersion != d->m_data.m_minor) {
        d->m_data.m_minor = minorVersion;
        emit minorVersionChanged(minorVersion);
        emit graphicsApiFilterChanged();
    }
}

void QGraphicsApiFilter::setMajorVersion(int majorVersion)
{
    Q_D(QGraphicsApiFilter);
    if (d->m_data.m_major != majorVersion) {
        d->m_data.m_major = majorVersion;
        emit majorVersionChanged(majorVersion);
        emit graphicsApiFilterChanged();
    }
}

void QGraphicsApiFilter::setExtensions(const QStringList &extensions)
{
    Q_D(QGraphicsApiFilter);
    if (d->m_data.m_extensions != extensions) {
        d->m_data.m_extensions = extensions;
        emit extensionsChanged(extensions);
        emit graphicsApiFilterChanged();
    }
}

void QGraphicsApiFilter::setVendor(const QString &vendor)
{
    Q_D(QGraphicsApiFilter);
    if (d->m_data.m_vendor != vendor) {
        d->m_data.m_vendor = vendor;
        emit vendorChanged(vendor);
        emit graphicsApiFilterChanged();
    }
}

/*! \fn bool Qt3DRender::operator ==(const QGraphicsApiFilter &reference, const QGraphicsApiFilter &sample)
  \relates Qt3DRender::QGraphicsApiFilter

  Returns \c true if \a reference and \a sample are equivalent.
 */
bool operator ==(const QGraphicsApiFilter &reference, const QGraphicsApiFilter &sample)
{
    return QGraphicsApiFilterPrivate::get(const_cast<QGraphicsApiFilter *>(&reference))->m_data ==
            QGraphicsApiFilterPrivate::get(const_cast<QGraphicsApiFilter *>(&sample))->m_data;
}

/*! \fn bool Qt3DRender::operator !=(const QGraphicsApiFilter &reference, const QGraphicsApiFilter &sample)
  \relates Qt3DRender::QGraphicsApiFilter

  Returns \c true if \a reference and \a sample are different.
 */
bool operator !=(const QGraphicsApiFilter &reference, const QGraphicsApiFilter &sample)
{
    return !(reference == sample);
}

/*! \fn void Qt3DRender::QGraphicsApiFilter::graphicsApiFilterChanged()
  This signal is emitted when the value of any property is changed.
*/
} // namespace Qt3DRender

QT_END_NAMESPACE

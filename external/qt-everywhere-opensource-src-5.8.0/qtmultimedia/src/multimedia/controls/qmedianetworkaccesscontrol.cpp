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

#include "qmedianetworkaccesscontrol.h"

QT_BEGIN_NAMESPACE

/*!
    \class QMediaNetworkAccessControl
    \brief The QMediaNetworkAccessControl class allows the setting of the Network Access Point for media related activities.
    \inmodule QtMultimedia


    \ingroup multimedia_control

    The functionality provided by this control allows the
    setting of a Network Access Point.

    This control can be used to set a network access for various
    network related activities.  The exact nature is dependent on the underlying
    usage by the supported QMediaObject.
*/

/*!
  \internal
*/
QMediaNetworkAccessControl::QMediaNetworkAccessControl(QObject *parent) :
    QMediaControl(parent)
{
}

/*!
    Destroys a network access control.
*/
QMediaNetworkAccessControl::~QMediaNetworkAccessControl()
{
}

/*!
    \fn void QMediaNetworkAccessControl::setConfigurations(const QList<QNetworkConfiguration> &configurations)

    The \a configurations parameter contains a list of network configurations to be used for network access.

    It is assumed the list is given in highest to lowest preference order.
    By calling this function all previous configurations will be invalidated
    and replaced with the new list.
*/

/*!
    \fn QNetworkConfiguration QMediaNetworkAccessControl::currentConfiguration() const

    Returns the current active configuration in use.
    A default constructed QNetworkConfigration is returned if no user supplied configuration are in use.
*/


/*!
    \fn QMediaNetworkAccessControl::configurationChanged(const QNetworkConfiguration &configuration)
    This signal is emitted when the current active network configuration changes
    to \a configuration.
*/



#include "moc_qmedianetworkaccesscontrol.cpp"
QT_END_NAMESPACE

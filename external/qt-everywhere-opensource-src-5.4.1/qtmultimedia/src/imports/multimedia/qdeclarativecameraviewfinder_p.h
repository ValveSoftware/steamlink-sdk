/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QDECLARATIVECAMERAVIEWFINDER_P_H
#define QDECLARATIVECAMERAVIEWFINDER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <qcamera.h>
#include <qmediarecorder.h>
#include <qmediaencodersettings.h>

QT_BEGIN_NAMESPACE

class QDeclarativeCamera;
class QCameraViewfinderSettingsControl;

class QDeclarativeCameraViewfinder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QSize resolution READ resolution WRITE setResolution NOTIFY resolutionChanged)
    Q_PROPERTY(qreal minimumFrameRate READ minimumFrameRate WRITE setMinimumFrameRate NOTIFY minimumFrameRateChanged)
    Q_PROPERTY(qreal maximumFrameRate READ maximumFrameRate WRITE setMaximumFrameRate NOTIFY maximumFrameRateChanged)
public:
    QDeclarativeCameraViewfinder(QCamera *camera, QObject *parent = 0);
    ~QDeclarativeCameraViewfinder();

    QSize resolution() const;
    void setResolution(const QSize &resolution);

    qreal minimumFrameRate() const;
    void setMinimumFrameRate(qreal frameRate);

    qreal maximumFrameRate() const;
    void setMaximumFrameRate(qreal frameRate);

Q_SIGNALS:
    void resolutionChanged();
    void minimumFrameRateChanged();
    void maximumFrameRateChanged();

private:
    QCamera *m_camera;
    QCameraViewfinderSettingsControl *m_control;
};

QT_END_NAMESPACE

#endif

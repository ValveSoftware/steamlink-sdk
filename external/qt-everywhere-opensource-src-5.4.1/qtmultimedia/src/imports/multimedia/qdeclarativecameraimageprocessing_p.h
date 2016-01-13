/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QDECLARATIVECAMERAIMAGEPROCESSING_H
#define QDECLARATIVECAMERAIMAGEPROCESSING_H

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
#include <qcameraimageprocessing.h>

QT_BEGIN_NAMESPACE

class QDeclarativeCamera;

class QDeclarativeCameraImageProcessing : public QObject
{
    Q_OBJECT
    Q_ENUMS(WhiteBalanceMode)

    Q_PROPERTY(WhiteBalanceMode whiteBalanceMode READ whiteBalanceMode WRITE setWhiteBalanceMode NOTIFY whiteBalanceModeChanged)
    Q_PROPERTY(qreal manualWhiteBalance READ manualWhiteBalance WRITE setManualWhiteBalance NOTIFY manualWhiteBalanceChanged)
    Q_PROPERTY(qreal contrast READ contrast WRITE setContrast NOTIFY contrastChanged)
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY saturationChanged)
    Q_PROPERTY(qreal sharpeningLevel READ sharpeningLevel WRITE setSharpeningLevel NOTIFY sharpeningLevelChanged)
    Q_PROPERTY(qreal denoisingLevel READ denoisingLevel WRITE setDenoisingLevel NOTIFY denoisingLevelChanged)

public:
    enum WhiteBalanceMode {
        WhiteBalanceAuto = QCameraImageProcessing::WhiteBalanceAuto,
        WhiteBalanceManual = QCameraImageProcessing::WhiteBalanceManual,
        WhiteBalanceSunlight = QCameraImageProcessing::WhiteBalanceSunlight,
        WhiteBalanceCloudy = QCameraImageProcessing::WhiteBalanceCloudy,
        WhiteBalanceShade = QCameraImageProcessing::WhiteBalanceShade,
        WhiteBalanceTungsten = QCameraImageProcessing::WhiteBalanceTungsten,
        WhiteBalanceFluorescent = QCameraImageProcessing::WhiteBalanceFluorescent,
        WhiteBalanceFlash = QCameraImageProcessing::WhiteBalanceFlash,
        WhiteBalanceSunset = QCameraImageProcessing::WhiteBalanceSunset,
        WhiteBalanceVendor = QCameraImageProcessing::WhiteBalanceVendor
    };

    ~QDeclarativeCameraImageProcessing();

    WhiteBalanceMode whiteBalanceMode() const;
    qreal manualWhiteBalance() const;

    qreal contrast() const;
    qreal saturation() const;
    qreal sharpeningLevel() const;
    qreal denoisingLevel() const;

public Q_SLOTS:
    void setWhiteBalanceMode(QDeclarativeCameraImageProcessing::WhiteBalanceMode mode) const;
    void setManualWhiteBalance(qreal colorTemp) const;

    void setContrast(qreal value);
    void setSaturation(qreal value);
    void setSharpeningLevel(qreal value);
    void setDenoisingLevel(qreal value);

Q_SIGNALS:
    void whiteBalanceModeChanged(QDeclarativeCameraImageProcessing::WhiteBalanceMode) const;
    void manualWhiteBalanceChanged(qreal) const;

    void contrastChanged(qreal);
    void saturationChanged(qreal);
    void sharpeningLevelChanged(qreal);
    void denoisingLevelChanged(qreal);

private:
    friend class QDeclarativeCamera;
    QDeclarativeCameraImageProcessing(QCamera *camera, QObject *parent = 0);

    QCameraImageProcessing *m_imageProcessing;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QT_PREPEND_NAMESPACE(QDeclarativeCameraImageProcessing))

#endif

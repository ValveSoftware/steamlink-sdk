/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MOCKCAMERAZOOMCONTROL_H
#define MOCKCAMERAZOOMCONTROL_H

#include "qcamerazoomcontrol.h"

class MockCameraZoomControl : public QCameraZoomControl
{
    Q_OBJECT
public:
    MockCameraZoomControl(QObject *parent = 0):
        QCameraZoomControl(parent),
        m_opticalZoom(1.0),
        m_digitalZoom(1.0),
        m_maxOpticalZoom(3.0),
        m_maxDigitalZoom(4.0)

    {
    }

    ~MockCameraZoomControl() {}

    qreal maximumOpticalZoom() const
    {
        return m_maxOpticalZoom;
    }

    qreal maximumDigitalZoom() const
    {
        return m_maxDigitalZoom;
    }

    qreal currentOpticalZoom() const
    {
        return m_opticalZoom;
    }

    qreal currentDigitalZoom() const
    {
        return m_digitalZoom;
    }

    qreal requestedOpticalZoom() const
    {
        return m_opticalZoom;
    }

    qreal requestedDigitalZoom() const
    {
        return m_digitalZoom;
    }

    void zoomTo(qreal optical, qreal digital)
    {
        optical = qBound<qreal>(1.0, optical, maximumOpticalZoom());
        digital = qBound<qreal>(1.0, digital, maximumDigitalZoom());

        if (!qFuzzyCompare(digital, m_digitalZoom)) {
            m_digitalZoom = digital;
            emit requestedDigitalZoomChanged(m_digitalZoom);
            emit currentDigitalZoomChanged(m_digitalZoom);
        }

        if (!qFuzzyCompare(optical, m_opticalZoom)) {
            m_opticalZoom = optical;
            emit requestedOpticalZoomChanged(m_opticalZoom);
            emit currentOpticalZoomChanged(m_opticalZoom);
        }

        maxOpticalDigitalZoomChange(4.0, 5.0);
    }

    // helper function to emit maximum Optical and Digital Zoom Changed signals
    void maxOpticalDigitalZoomChange(qreal maxOptical, qreal maxDigital)
    {
        if (maxOptical != m_maxOpticalZoom) {
            m_maxOpticalZoom = maxOptical;
            emit maximumOpticalZoomChanged(m_maxOpticalZoom);
        }

        if (maxDigital != m_maxDigitalZoom) {
            m_maxDigitalZoom = maxDigital;
            emit maximumDigitalZoomChanged(m_maxDigitalZoom);
        }
    }

private:
    qreal m_opticalZoom;
    qreal m_digitalZoom;
    qreal m_maxOpticalZoom;
    qreal m_maxDigitalZoom;
};

#endif // MOCKCAMERAZOOMCONTROL_H

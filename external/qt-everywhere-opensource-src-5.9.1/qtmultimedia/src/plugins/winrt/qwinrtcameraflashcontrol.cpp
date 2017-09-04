/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwinrtcameraflashcontrol.h"
#include "qwinrtcameracontrol.h"
#include <QtCore/QTimer>
#include <QtCore/qfunctions_winrt.h>
#include <QtCore/private/qeventdispatcher_winrt_p.h>

#include <windows.media.devices.h>
#include <wrl.h>
#include <functional>

using namespace Microsoft::WRL;
using namespace ABI::Windows::Media::Devices;

QT_BEGIN_NAMESPACE

class QWinRTCameraFlashControlPrivate
{
public:
    ComPtr<IFlashControl> flashControl;

    QList<QCameraExposure::FlashModes> supportedModes;
    QCameraExposure::FlashModes currentModes;
    bool initialized;
};

QWinRTCameraFlashControl::QWinRTCameraFlashControl(QWinRTCameraControl *parent)
    : QCameraFlashControl(parent), d_ptr(new QWinRTCameraFlashControlPrivate)
{
    qCDebug(lcMMCamera) << __FUNCTION__ << parent;
    Q_D(QWinRTCameraFlashControl);

    d->initialized = false;
    d->currentModes = QCameraExposure::FlashOff;
}

void QWinRTCameraFlashControl::initialize(Microsoft::WRL::ComPtr<IAdvancedVideoCaptureDeviceController2> &controller)
{
    qCDebug(lcMMCamera) << __FUNCTION__;
    Q_D(QWinRTCameraFlashControl);

    d->initialized = false;

    d->supportedModes.clear();
    HRESULT hr;
    hr = QEventDispatcherWinRT::runOnXamlThread([d, controller]() {
        HRESULT hr;
        hr = controller->get_FlashControl(&d->flashControl);
        RETURN_HR_IF_FAILED("Could not access flash control.");

        boolean oldAuto;
        boolean oldEnabled;
        // IFlashControl::get_Supported() is only valid for additional
        // controls (RedEye, etc.) so we have to manually try to set
        // and reset flash
        if (SUCCEEDED(d->flashControl->get_Auto(&oldAuto))) {
            hr = d->flashControl->put_Auto(!oldAuto);
            if (SUCCEEDED(hr)) {
                d->flashControl->put_Auto(oldAuto);
                d->supportedModes.append(QCameraExposure::FlashAuto);
            }
        }

        if (SUCCEEDED(d->flashControl->get_Enabled(&oldEnabled))) {
            hr = d->flashControl->put_Enabled(!oldEnabled);
            if (SUCCEEDED(hr)) {
                d->flashControl->put_Enabled(oldEnabled);
                d->supportedModes.append(QCameraExposure::FlashOff);
                d->supportedModes.append(QCameraExposure::FlashOn);
            }
        }

        boolean val;
        hr = d->flashControl->get_Supported(&val);
        if (SUCCEEDED(hr) && val) {
            hr = d->flashControl->get_RedEyeReductionSupported(&val);
            if (SUCCEEDED(hr) && val)
                d->supportedModes.append(QCameraExposure::FlashRedEyeReduction);

            // ### There is no Qt API to actually set the power values.
            // However query if the camera could theoretically do it
            hr = d->flashControl->get_PowerSupported(&val);
            if (SUCCEEDED(hr) && val)
                d->supportedModes.append(QCameraExposure::FlashManual);
        }

        return S_OK;
    });
    Q_ASSERT_SUCCEEDED(hr);
    d->initialized = true;
    setFlashMode(d->currentModes);
}

QCameraExposure::FlashModes QWinRTCameraFlashControl::flashMode() const
{
    Q_D(const QWinRTCameraFlashControl);
    return d->currentModes;
}

void QWinRTCameraFlashControl::setFlashMode(QCameraExposure::FlashModes mode)
{
    qCDebug(lcMMCamera) << __FUNCTION__ << mode;
    Q_D(QWinRTCameraFlashControl);

    if (!d->initialized) {
        d->currentModes = mode;
        return;
    }

    if (!isFlashModeSupported(mode))
        return;

    QEventDispatcherWinRT::runOnXamlThread([d, mode]() {
        HRESULT hr;
        if (mode.testFlag(QCameraExposure::FlashAuto)) {
            hr = d->flashControl->put_Enabled(true);
            RETURN_OK_IF_FAILED("Could not set flash mode on.");
            hr = d->flashControl->put_Auto(true);
            RETURN_OK_IF_FAILED("Could not set flash mode auto.");
            d->currentModes = QCameraExposure::FlashAuto;
        } else if (mode.testFlag(QCameraExposure::FlashOn)) {
            hr = d->flashControl->put_Enabled(true);
            RETURN_OK_IF_FAILED("Could not set flash mode on.");
            hr = d->flashControl->put_Auto(false);
            RETURN_OK_IF_FAILED("Could not disable flash auto mode.");
            d->currentModes = QCameraExposure::FlashOn;
        } else if (mode.testFlag(QCameraExposure::FlashRedEyeReduction)) {
            hr = d->flashControl->put_Enabled(true);
            RETURN_OK_IF_FAILED("Could not set flash mode on.");
            hr = d->flashControl->put_RedEyeReduction(true);
            RETURN_OK_IF_FAILED("Could not set flash mode red eye reduction.");
            d->currentModes = QCameraExposure::FlashRedEyeReduction;
        } else {
            hr = d->flashControl->put_Enabled(false);
            RETURN_OK_IF_FAILED("Could not set flash mode off.");
            d->currentModes = QCameraExposure::FlashOff;
        }
        return S_OK;
    });
}

bool QWinRTCameraFlashControl::isFlashModeSupported(QCameraExposure::FlashModes mode) const
{
    Q_D(const QWinRTCameraFlashControl);
    qCDebug(lcMMCamera) << __FUNCTION__ << mode;
    return d->initialized ? d->supportedModes.contains(mode) : false;
}

bool QWinRTCameraFlashControl::isFlashReady() const
{
    qCDebug(lcMMCamera) << __FUNCTION__;
    // No native API to query state
    return true;
}

QT_END_NAMESPACE

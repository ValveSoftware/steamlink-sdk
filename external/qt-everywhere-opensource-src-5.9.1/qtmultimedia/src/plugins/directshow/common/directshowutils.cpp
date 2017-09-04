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

#include "directshowutils.h"

QT_BEGIN_NAMESPACE

/**
 * @brief DirectShowUtils::isPinConnected
 * @param pin
 * @param hrOut
 * @return
 */
bool DirectShowUtils::isPinConnected(IPin *pin, HRESULT *hrOut)
{
    IPin *connectedPin = nullptr;
    const ScopedSafeRelease<IPin> releasePin { &connectedPin };
    HRESULT hr = S_OK;
    if (!hrOut)
        hrOut = &hr;

    *hrOut = pin->ConnectedTo(&connectedPin);
    if (*hrOut == VFW_E_NOT_CONNECTED) // Not an error in this case
        *hrOut = S_OK;

    if (FAILED(*hrOut)) {
        qCDebug(qtDirectShowPlugin, "Querying pin connection failed!");
        return false;
    }

    return true;
}

/**
 * @brief DirectShowUtils::hasPinDirection
 * @param pin
 * @param direction
 * @param hrOut
 * @return
 */
bool DirectShowUtils::hasPinDirection(IPin *pin, PIN_DIRECTION direction, HRESULT *hrOut)
{
    PIN_DIRECTION pinDir;
    HRESULT hr = S_OK;
    if (!hrOut)
        hrOut = &hr;

    *hrOut = pin->QueryDirection(&pinDir);

    if (FAILED(*hrOut)) {
        qCDebug(qtDirectShowPlugin, "Querying pin direction failed!");
        return false;
    }

    return (pinDir == direction);
}

/**
 * @brief DirectShowUtils::getPin
 * @param filter
 * @param pinDirection
 * @param pin
 * @param hrOut
 * @return
 */
bool DirectShowUtils::getPin(IBaseFilter *filter, PIN_DIRECTION pinDirection, IPin **pin, HRESULT *hrOut)
{
    IEnumPins *enumPins = nullptr;
    const ScopedSafeRelease<IEnumPins> releaseEnumPins { &enumPins };
    HRESULT hr S_OK;
    if (!hrOut)
        hrOut = &hr;

    *hrOut = filter->EnumPins(&enumPins);
    if (FAILED(*hrOut)) {
        qCDebug(qtDirectShowPlugin, "Unable to retrieve pins from the filter!");
        return false;
    }

    enumPins->Reset();
    IPin *nextPin = nullptr;
    while (enumPins->Next(1, &nextPin, NULL) == S_OK) {
        const ScopedSafeRelease<IPin> releasePin { &nextPin };
        PIN_DIRECTION currentPinDir;
        *hrOut = nextPin->QueryDirection(&currentPinDir);
        if (currentPinDir == pinDirection) {
            *pin = nextPin;
            (*pin)->AddRef();
            return true;
        }
    }

    return false;
}

/**
 * @brief DirectShowUtils::matchPin
 * @param pin
 * @param pinDirection
 * @param shouldBeConnected
 * @param hrOut
 * @return
 */
bool DirectShowUtils::matchPin(IPin *pin, PIN_DIRECTION pinDirection, BOOL shouldBeConnected, HRESULT *hrOut)
{
    HRESULT hr = S_OK;
    if (!hrOut)
        hrOut = &hr;

    const BOOL isConnected = isPinConnected(pin, hrOut);
    if (FAILED(*hrOut)) // Error reason will already be logged, so just return.
        return false;

    if (isConnected == shouldBeConnected)
        return hasPinDirection(pin, pinDirection, hrOut);

    return SUCCEEDED(*hrOut);
}

/**
 * @brief DirectShowUtils::findUnconnectedPin
 * @param filter
 * @param pinDirection
 * @param pin
 * @param hrOut
 * @return
 */
bool DirectShowUtils::findUnconnectedPin(IBaseFilter *filter, PIN_DIRECTION pinDirection, IPin **pin, HRESULT *hrOut)
{
    HRESULT hr = S_OK;
    if (!hrOut)
        hrOut = &hr;

    IEnumPins *enumPins = nullptr;
    const ScopedSafeRelease<IEnumPins> releaseEnumPins { &enumPins };
    *hrOut = filter->EnumPins(&enumPins);
    if (FAILED(*hrOut)) {
        qCDebug(qtDirectShowPlugin, "Unable to retrieve pins from the DS filter");
        return false;
    }

    IPin *nextPin = nullptr;
    while (S_OK == enumPins->Next(1, &nextPin, nullptr)) {
        const ScopedSafeRelease<IPin> releaseNextPin { &nextPin };
        if (matchPin(nextPin, pinDirection, FALSE, hrOut)) {
            *pin = nextPin;
            (*pin)->AddRef();
            return true;
        }

        if (FAILED(*hrOut))
            return false;
    }

    qCDebug(qtDirectShowPlugin, "No unconnected pins found");
    *hrOut = VFW_E_NOT_FOUND;

    return false;
}

/**
 * @brief DirectShowUtils::connectFilters - Attempts to connect \a outputPin to \a filter
 * @param graph
 * @param outputPin
 * @param filter
 * @param hrOut
 * @return
 */
bool DirectShowUtils::connectFilters(IGraphBuilder *graph, IPin *outputPin, IBaseFilter *filter, HRESULT *hrOut)
{

    // Find an input pin on the downstream filter.
    HRESULT hr = S_OK;
    if (!hrOut)
        hrOut = &hr;

    IPin *inputPin = nullptr;
    const ScopedSafeRelease<IPin> releaseInputPin { &inputPin };
    if (!findUnconnectedPin(filter, PINDIR_INPUT, &inputPin, hrOut))
        return false;


    // Try to connect them.
    *hrOut = graph->Connect(outputPin, inputPin);
    if (FAILED(*hrOut)) {
        qCDebug(qtDirectShowPlugin, "Unable to connect output pin to filter!");
        return false;
    }

    return true;
}

/**
 * @brief DirectShowUtils::connectFilters - Attempts to connect \a filter to \a inputPin.
 * @param graph
 * @param filter
 * @param inputPin
 * @param hrOut
 * @return
 */
bool DirectShowUtils::connectFilters(IGraphBuilder *graph, IBaseFilter *filter, IPin *inputPin, HRESULT *hrOut)
{
    HRESULT hr = S_OK;
    if (!hrOut)
        hrOut = &hr;

    IPin *outputPin = nullptr;
    const ScopedSafeRelease<IPin> releaseOutputPin { &outputPin };
    // Find an output pin on the upstream filter.
    if (findUnconnectedPin(filter, PINDIR_OUTPUT, &outputPin, hrOut))
        return false;

    *hrOut = graph->Connect(outputPin, inputPin);
    if (FAILED(*hrOut)) {
        qCDebug(qtDirectShowPlugin, "Unable to connect filter to input pin!");
        return false;
    }

    return true;
}

/**
 * @brief DirectShowUtils::connectFilters - Attempts to connect the \a upstreamFilter to \a downstreamFilter.
 * @param graph
 * @param upstreamFilter
 * @param downstreamFilter
 * @param autoConnect - If set to true all filters in the graph will be considered.
 * @param hrOut
 * @return true if the the filters were connected, false otherwise.
 */
bool DirectShowUtils::connectFilters(IGraphBuilder *graph,
                                     IBaseFilter *upstreamFilter,
                                     IBaseFilter *downstreamFilter,
                                     bool autoConnect,
                                     HRESULT *hrOut)
{
    HRESULT hr = S_OK;
    if (!hrOut)
        hrOut = &hr;

    const auto findAndConnect = [graph, downstreamFilter, hrOut](IBaseFilter *filter) -> bool {
        IPin *outputPin = nullptr;
        const ScopedSafeRelease<IPin> releaseOutputPin { &outputPin };
        if (findUnconnectedPin(filter, PINDIR_OUTPUT, &outputPin, hrOut))
            return connectFilters(graph, outputPin, downstreamFilter, hrOut);

        return false;
    };

    // Try to connect to the upstream filter first.
    if (findAndConnect(upstreamFilter))
        return S_OK;

    const auto getFilters = [graph, hrOut]() -> IEnumFilters * {
        IEnumFilters *f = nullptr;
        *hrOut = graph->EnumFilters(&f);
        return f;
    };
    IEnumFilters *filters = autoConnect ? getFilters()
                                        : nullptr;
    const ScopedSafeRelease<IEnumFilters> releaseEnumFilters { &filters };
    if (!filters) {
        qCDebug(qtDirectShowPlugin, "No filters found!");
        return false;
    }

    IBaseFilter *nextFilter = nullptr;
    while (S_OK == filters->Next(1, &nextFilter, 0)) {
        const ScopedSafeRelease<IBaseFilter> releaseNextFilter { &nextFilter };
        if (nextFilter && findAndConnect(nextFilter))
            break;
    }

    return SUCCEEDED(*hrOut);
}

QT_END_NAMESPACE

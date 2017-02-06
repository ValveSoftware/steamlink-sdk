// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/Bluetooth.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/bluetooth/BluetoothDevice.h"
#include "modules/bluetooth/BluetoothError.h"
#include "modules/bluetooth/BluetoothSupplement.h"
#include "modules/bluetooth/BluetoothUUID.h"
#include "modules/bluetooth/RequestDeviceOptions.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/modules/bluetooth/WebBluetooth.h"
#include "public/platform/modules/bluetooth/WebRequestDeviceOptions.h"

namespace blink {

namespace {
// A device name can never be longer than 29 bytes. A adv packet is at most
// 31 bytes long. The length and identifier of the length field take 2 bytes.
// That least 29 bytes for the name.
const size_t kMaxFilterNameLength = 29;
const char kFilterNameTooLong[] =
    "A 'name' or 'namePrefix' longer than 29 bytes results in no devices being found, because a device can't advertise a name longer than 29 bytes.";
// Per the Bluetooth Spec: The name is a user-friendly name associated with the
// device and consists of a maximum of 248 bytes coded according to the UTF-8
// standard.
const size_t kMaxDeviceNameLength = 248;
const char kDeviceNameTooLong[] = "A device name can't be longer than 248 bytes.";
} // namespace

static void canonicalizeFilter(const BluetoothScanFilter& filter, WebBluetoothScanFilter& canonicalizedFilter, ExceptionState& exceptionState)
{
    if (!(filter.hasServices() || filter.hasName() || filter.hasNamePrefix())) {
        exceptionState.throwTypeError(
            "A filter must restrict the devices in some way.");
        return;
    }

    if (filter.hasServices()) {
        if (filter.services().size() == 0) {
            exceptionState.throwTypeError(
                "'services', if present, must contain at least one service.");
            return;
        }
        Vector<WebString> services;
        for (const StringOrUnsignedLong& service : filter.services()) {
            const String& validatedService = BluetoothUUID::getService(service, exceptionState);
            if (exceptionState.hadException())
                return;
            services.append(validatedService);
        }
        canonicalizedFilter.services.assign(services);
    }

    canonicalizedFilter.hasName = filter.hasName();
    if (filter.hasName()) {
        size_t nameLength = filter.name().utf8().length();
        if (nameLength > kMaxDeviceNameLength) {
            exceptionState.throwTypeError(kDeviceNameTooLong);
            return;
        }
        if (nameLength > kMaxFilterNameLength) {
            exceptionState.throwDOMException(NotFoundError, kFilterNameTooLong);
            return;
        }
        canonicalizedFilter.name = filter.name();
    }

    if (filter.hasNamePrefix()) {
        size_t namePrefixLength = filter.namePrefix().utf8().length();
        if (namePrefixLength > kMaxDeviceNameLength) {
            exceptionState.throwTypeError(kDeviceNameTooLong);
            return;
        }
        if (namePrefixLength > kMaxFilterNameLength) {
            exceptionState.throwDOMException(NotFoundError, kFilterNameTooLong);
            return;
        }
        if (filter.namePrefix().length() == 0) {
            exceptionState.throwTypeError(
                "'namePrefix', if present, must me non-empty.");
            return;
        }
        canonicalizedFilter.namePrefix = filter.namePrefix();
    }
}

static void convertRequestDeviceOptions(const RequestDeviceOptions& options, WebRequestDeviceOptions& result, ExceptionState& exceptionState)
{
    ASSERT(options.hasFilters());

    if (options.filters().isEmpty()) {
        exceptionState.throwTypeError(
            "'filters' member must be non-empty to find any devices.");
    }

    Vector<WebBluetoothScanFilter> filters;
    for (const BluetoothScanFilter& filter : options.filters()) {
        WebBluetoothScanFilter canonicalizedFilter = WebBluetoothScanFilter();

        canonicalizeFilter(filter, canonicalizedFilter, exceptionState);

        if (exceptionState.hadException())
            return;

        filters.append(canonicalizedFilter);
    }

    result.filters.assign(filters);

    if (options.hasOptionalServices()) {
        Vector<WebString> optionalServices;
        for (const StringOrUnsignedLong& optionalService : options.optionalServices()) {
            const String& validatedOptionalService = BluetoothUUID::getService(optionalService, exceptionState);
            if (exceptionState.hadException())
                return;
            optionalServices.append(validatedOptionalService);
        }
        result.optionalServices.assign(optionalServices);
    }
}

// https://webbluetoothchrome.github.io/web-bluetooth/#dom-bluetooth-requestdevice
ScriptPromise Bluetooth::requestDevice(ScriptState* scriptState, const RequestDeviceOptions& options, ExceptionState& exceptionState)
{

    // By adding the "OriginTrialEnabled" extended binding, we enable the
    // requestDevice function on all platforms for websites that contain an
    // origin trial token. Since we only support Chrome OS, Android and MacOS
    // for this experiment we reject any promises from other platforms unless
    // they have the enable-web-bluetooth flag on.
#if !OS(CHROMEOS) && !OS(ANDROID) && !OS(MACOSX)
    if (!RuntimeEnabledFeatures::webBluetoothEnabled()) {
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "Web Bluetooth is not enabled on this platform. To find out how to enable it and the current implementation status visit https://goo.gl/HKa2If"));
    }
#endif

    // 1. If the incumbent settings object is not a secure context, reject promise with a SecurityError and abort these steps.
    String errorMessage;
    if (!scriptState->getExecutionContext()->isSecureContext(errorMessage)) {
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(SecurityError, errorMessage));
    }

    // 2. If the algorithm is not allowed to show a popup, reject promise with a SecurityError and abort these steps.
    if (!UserGestureIndicator::consumeUserGesture()) {
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(SecurityError, "Must be handling a user gesture to show a permission request."));
    }

    WebBluetooth* webbluetooth = BluetoothSupplement::fromScriptState(scriptState);
    if (!webbluetooth)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError));

    // 3. In order to convert the arguments from service names and aliases to just UUIDs, do the following substeps:
    WebRequestDeviceOptions webOptions;
    convertRequestDeviceOptions(options, webOptions, exceptionState);
    if (exceptionState.hadException())
        return exceptionState.reject(scriptState);

    // Subsequent steps are handled in the browser process.
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    webbluetooth->requestDevice(webOptions, new CallbackPromiseAdapter<BluetoothDevice, BluetoothError>(resolver));
    return promise;

}

} // namespace blink

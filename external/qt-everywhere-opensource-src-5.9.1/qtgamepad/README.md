Qt Gamepad

A Qt 5 module that adds support for getting events from gamepad devices on multiple platforms.

Supported Platforms:
Native Backends
 - Linux (evdev)
 - Window (xinput)
 - Android
 - OS X/iOS/tvOS

For other platforms there is a backend that uses SDL2 for gamepad support.

This module provides classes that can:
 - Read input events from game controllers (Button and Axis events), both from C++ and Qt Quick (QML)
 - Provide a queryable input state (by processing events)
 - Provide key bindings

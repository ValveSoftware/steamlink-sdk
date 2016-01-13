Qt bindings for ConnMan
=======================

The Qt bindings are direct reflection of ConnMan's D-Bus interfaces:
the class `NetworkManager` represents `net.connman.Manager`,
`NetworkTechnology` represents `net.connman.Technology` and
`NetworkService` represents `net.connman.Service`.
`NetworkSession` represents `net.connman.Session`.
`NetworkTechnology` represents `net.connman.Technology`.

`TechnologyModel` is a QML component representing a list of network
services of a given technology.

`UserAgent` is a QML component providing `net.connman.Agent` D-Bus interface.

The class `NetworkingModel` is a QML component adapting a static instance of
`NetworkManager`. Also it provides the D-Bus interface `net.connman.Agent`.

.. warning:: `NetworkingModel` is going to be deprecated.

These classes are written to be re-usable in the qml environment, by setting the
path property to that of an appropriate dbus path, will re-initialize the object to be used for the path given.


QMake CONFIG flags
-----------
* notests: doesn't compile tests
* noplugin: doesn't compile qml plugin

Example:
``qmake CONFIG+=notests``

The requestConnect signal in UserAgent requires a patch to connman.
http://gitweb.merproject.org/gitweb?p=mer-core/connman.git;a=blob_plain;f=0003-connman-request-connect-notify.patch;hb=HEAD

This allows a signal being sent to the UserAgent when there is a connection started/tried but no technology is connected.
This only works if you use that patch and connman's dnsproxy.

The Agent class has two answers it can handle "Clear" and "Suppress"
"Suppress" will suppress the request signal for the timeout period or until "Clear" is sent to the agent.


Extra make targets
-----------

* check: run tests directly inside build tree
* coverage: generate code coverage report

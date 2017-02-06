Mojo Public C API
=================

This directory contains C language bindings for the Mojo Public API.

System
------

The system/ subdirectory provides definitions of the basic low-level API used by
all Mojo applications (whether directly or indirectly). These consist primarily
of the IPC primitives used to communicate with Mojo services.

Though the message protocol is stable, the implementation of the transport is
not, and access to the IPC mechanisms must be via the primitives defined in this
directory.

Test Support
------------

This directory contains a C API for running tests. This API is only available
under special, specific test conditions. It is not meant for general use by Mojo
applications.

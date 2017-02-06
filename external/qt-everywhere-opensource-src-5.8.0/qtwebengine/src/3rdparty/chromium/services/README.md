Chrome Foundation Services
====

### Overview

This directory contains Chrome Foundation Services. If you think of Chrome as a
"portable OS," Chrome Foundation Services can be thought of as that OS'
foundational "system services" layer.

Roughly each subdirectory here corresponds to a service that:

  * is a client of `//services/shell` with its own unique Identity.
  * could logically run a standalone process for security/performance isolation
    benefits depending on the constraints of the host OS.

### Service Directory Structure

Individual services are structured like so:

    //services/foo/                   <-- Implementation code, may have subdirs.
                  /public/    
                         /cpp/        <-- C++ client libraries (optional)
                         /interfaces/ <-- Mojom interfaces

### Dependencies

Code within `//services` may only depend on each other via each other's
`/public/` directories, i.e. implementation code cannot be shared directly.

Service code should also take care to tightly limit the dependencies on static
libraries from outside of `//services`. Dependencies to large platform
layers like `//content`, `//chrome` or `//third_party/WebKit` must be avoided.

### Physical Packaging

Note that while it may be possible to build a discrete physical package (DSO)
for each service, products consuming these services may package them
differently, e.g. by combining them into a single package.

### High-level Design Doc
https://docs.google.com/document/d/15I7sQyQo6zsqXVNAlVd520tdGaS8FCicZHrN0yRu-oU/edit#heading=h.p37l9e7o0io5

### Relationship to other top-level directories in //

Services can be thought of as integrators of library code from across the
Chromium repository, most commonly //base and //mojo (obviously) but for each
service also //components, //ui, etc in accordance with the functionality they
provide.

Not everything in //components is automatically a service in its own right.
Think of //components as sort of like a //lib. Individual //components can
define, implement and use mojom interfaces, but only //services have unique
identities with the Mojo Shell.
Navigation Service
====

### Overview

Navigation is a Chrome Foundation Service that provides the concept of a
"navigable view". A navigable view is an application host that can be
navigated to different states in sequence, like a traditional "webview" on other
platforms.

The Navigation service maintains a tree of frames, within which different
applications run. Each navigation state corresponds to a potentially different
tree structure with different applications run in each frame.

In theory, the Navigation service should not care about the specifics of the
application hosted in each frame, though in practice today because the
implementation is provided by //content, all frames are instances of Blink.

When an application hosted by a view requires a capability not intrinsic to the
container, it requests via the ViewClient that the embedder provide it. For some
capabilities the grant action is simply providing some metadata directly. For
others, they are modeled by the acquisition of a Mojo message pipe handle
(interface) to some service that exposes the capability. In this latter case it
is possible, again in theory, that the middleware layer (Navigation service) not
know about the specifics of the capability and just forward the request from the
frame renderer to the Navigation service client where it will either be bound or
the message pipe closed.

A client of the Navigation service may embed a visual representation of a view into
its own rendering using Mus. The View interface provides a means to acquire a
mus.mojom.WindowTreeClient that can be embedded in a mus::Window created by the
client. The Navigation service may create its own mus::Windows within this window,
but these will not be visible to the client according to Mus security policy.

### Details

Concretely, this service is a //content client, using the Content Public API
to construct a WebContents for every request for a navigation::mojom::View.
The class that binds these two together is //services/navigation/view_impl.h.
This class implements the WebContentsDelegate (& other) interfaces to learn
about state changes within the Content layer & transmit them to the Navigation
Service client via navigation::mojom::ViewClient.

Note that the //content client provided here
(in //services/navigation/content_client) is very simple, even simpler than
Content Shell's. This is not intended to be a long term solution, just a
mechanism to avoid changing Content. If we were to deploy this code in //chrome,
we might want to expose these interfaces from content directly.

Lastly, the desired end state of the service-refactoring is that //content
ceases to exist, so at that point it's conceivable that //content types that
are used to implement these interfaces end up in this directory. But that's a
long way off.

### Usage

For convenience, as synchronizing some navigable view state is tricky, a client
library is provided in //services/navigation/public/cpp. The types exposed in
this library mimic conventional platform WebView types.

    std::unique_ptr<navigation::View> view(new navigation::View);
    ViewDelegateImpl delegate;
    ViewObserverImpl observer;
    view->set_delegate(&delegate);
    view->AddObserver(&observer);
    view->EmbedInWindow(some_mus_window);
    view->NavigateToURL(GURL("http://www.chromium.org/"));

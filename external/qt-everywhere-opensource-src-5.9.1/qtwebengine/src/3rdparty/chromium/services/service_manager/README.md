# Service Manager User Guide

## What is the Service Manager?

The Service Manager is a tool that brokers connections and capabilities between
and manages instances of components, referred to henceforth as services.

The Service Manager performs the following functions:

* Brokering connections between services, including communicating policies such
  as capabilities (which include access to interfaces), user identity, etc.
* Launching and managing the lifecycle services and processes (though services
  may also create their own processes and tell the Service Manager about them).
* Tracks running services, and provides an API that allows services to
  understand whats running.

The Service Manager presents a series of Mojo interfaces to services, though in
practice interacting with the Service is made simpler with a client library.
Currently, there is only a client library written in C++, since that meets the
needs of most of the use cases in Chrome.

## Details

### Mojo Recap

The Mojo system provides two key components of interest here - a lightweight
message pipe concept allowing two endpoints to communicate, and a bindings layer
that allows interfaces to be described to bind to those endpoints, with
ergonomic bindings for languages used in Chrome.

Mojo message pipes are designed to be lightweight and may be read from/written
to and passed around from one process to the next. In most situations however
the developer wont interact with the pipes directly, rather with a generated
types encapsulating a bound interface.

To use the bindings, a developer defines their interface in the Mojo IDL format,
**mojom**. With some build magic, the generated headers can then be included and
used from C++, JS and Java.

It is important to note here that Mojo Interfaces have fully qualified
identifiers in string form, generated from the module path and interface name:
**`module.path.InterfaceName`**. This is how interfaces are referenced in
Service Manifests, and how they will be referenced throughout this document.

This would be a good place for me to refer to this in-depth Mojo User Guide,
which spells all of this out in great detail.

### Services

A Service is any bit of code the Service Manager knows about. This could be a
unique process, or just a bit of code run in some existing process.

The Service Manager disambiguates services by their **Identity**. Every service
has its own unique Identity. From the Service Managers perspective, a services
Identity is represented by the tuple of the its Name, UserId and Instance Name.
The Name is a formatted string that superficially represents a scheme:host pair,
but actually isnt a URL. More on the structure of these names later. The UserId
is a string GUID, representing the user the service is run as. The Instance Name
is a string, typically (but not necessarily) derived from the Name, which can be
used to allow multiple instances of a service to exist for the same Name,UserId
pair. In Chrome an example of this would be multiple instances of the renderer
or the same profile.

A Service implements the Mojo interface service_manager.mojom.Service, which is
the primary means the Service Manager has of communicating with its service.
Service has two methods: OnStart(), called once at when the Service Manager
first learns about the service, and OnConnect(), which the Service Manager calls
 every time some other service tries to connect to this one.

Services have a link back to the Service Manager too, primarily in the form of
the service_manager.mojom.Connector interface. The Connector allows services to 
open connections to other services.

A unique connection from the Service Manager to a service is called an
instance, each with its own unique identifier, called an instance id. Every
instance has a unique Identity. It is possible to locate an existing instance
purely using its Identity.

Services define their own lifetimes. Services in processes started by other
services (rather than the Service Manager) may even outlive the connection with
the Service Manager. For processes launched by the Service Manager, when a
service wishes to terminate it closes the Service pipe with the Service Manager
and the Service Manager destroys its corresponding instance and asks the process
to exit.

#### A simple Service example

Consider this simple application that implements the Service interface:

**my_service.cc:**

    #include "mojo/public/c/system/main.h"
    #include "services/service_manager/public/cpp/application_runner.h"
    #include "services/service_manager/public/cpp/connector.h"
    #include "services/service_manager/public/cpp/connection.h"
    #include "services/service_manager/public/cpp/identity.h"
    #include "services/service_manager/public/cpp/service.h"

    class MyService : public service_manager::Service {
     public:
      MyService() {}
      ~MyService() override {}

      // Overridden from service_manager::Service:
      void OnStart(const service_manager::ServiceInfo& info) override {
      }
      bool OnConnect(const service_manager::SerivceInfo& remote_info,
                     service_manager::InterfaceRegistry* registry) override {
        return true;
      }
    };

    MojoResult ServiceMain(MojoHandle service_request_handle) {
      return service_manager::ServiceRunner(new MyService).Run(
          service_request_handle);
    }

**manifest.json:**

    {
      "name": "service:my_service",
      "display_name": "My Service",
      "inteface_provider_spec": {
        "service_manager:connector": {}
      }
    }

**BUILD.gn:**

    import("//mojo/public/mojo_application.gni")

    service("my_service") {
      sources = [ "my_service.cc" ]
      deps = [ "//base", "//services/service_manager/public/cpp" ]
      data_deps = [ ":manifest" ]
    }

    service_manifest("manifest") {
      name = "my_service"
      source = "manifest.json"
    }

What does all this do? Building the app target produces two files in the output
directory: Packages/my_service/my_service.library and
Packages/my_service/manifest.json. app.library is a DSO loaded by the Service
Manager in its own process when another service connects to the 
service:my_service name. This is not the only way (nor even the most likely one)
 you can implement a Service, but it's the simplest and easiest to reason about.

This service doesn't do much. Its implementation of OnStart() is empty, and its
implementation of OnConnect just returns true to allow the inbound connection to
complete. Let's study the parameters to these methods though, since they'll be
important as we begin to do more in our service.

##### OnStart Parameters

###### const service_manager::ServiceInfo& info
ServiceInfo is a struct containing two fields, Identity and
InterfaceProviderSpec. The Identity field identifies this Service instance in
the Service Manager, and contains three components - the service name, the user
id the instance is run as, and an instance qualifier. The InterfaceProviderSpec
contains the definitions of the capabilities exposed and consumed by this
service that are statically declared in its manifest.

##### OnConnect Parameters

###### const service_manager::ServiceInfo& remote_info
Like the ServiceInfo parameter passed to OnStart, but defines the remote Service
that initiated the connection. The Service Manager client library uses this
information to limit what capabilities are exposed to the remote via the
InterfaceRegistry parameter.

###### service_manager::InterfaceRegistry* registry
An object the local service uses to expose interfaces to be consumed by the
remote. This object is constructed by the Service Manager client library and
uses the InterfaceProviderSpecs of both the local and the remote service to
limit which interfaces can be bound by the remote. This object implements
service_manager::mojom::InterfaceProvider, which encapsulates the physical link
between the two services. The InterfaceRegistry is owned by the ServiceContext,
and will outlive the underlying pipe.

The service can decide to block the connection outright by returning false from
this method. In that scenario the underlying pipe will be closed and the remote
end will see an error and have the chance to recover.

Before we add any functionality to our service, such as exposing an interface,
we should look at how we connect to another service and bind an interface from
it. This will lay the groundwork to understanding how to export an interface.

### Connecting

Once we have a Connector, we can connect to other services and bind interfaces
from them. In the trivial app above we can do this directly in OnStart:

    void OnStart(const service_manager::ServiceInfo& info) override {
      std::unique_ptr<service_manager::Connection> connection =
          connector()->Connect("service:other_service");
      mojom::SomeInterfacePtr some_interface;
      connection->GetInterface(&some_interface);
      some_interface->Foo();
    }

This assumes an interface called 'mojo.SomeInterface' with a method 'Foo()'
exported by another service identified by the name 'service:other_service'.

What is happening here? Let's look line-by-line


    std::unique_ptr<service_manager::Connection> connection =
        connector->Connect("service:other_service");

This asks the Service Manager to open a connection to the service named
'service:other_service'. The Connect() method returns a Connection object similar
 to the one received by OnConnect() - in fact this Connection object binds the
 other ends of the pipes of the Connection object received by OnConnect in the
remote service. This time, the caller of Connect() takes ownership of the
Connection, and when it is destroyed the connection (and the underlying pipes)
is closed. A note on this later.

    mojom::SomeInterfacePtr some_interface;

This is a shorthand from the mojom bindings generator, producing an
instantiation of a mojo::InterfacePtr<mojom::SomeInterface>. At this point the
InterfacePtr is unbound (has no pipe handle), and calling is_bound() on it will
return false. Before we can call any methods, we need to bind it to a Mojo
message pipe. This is accomplished on the following line:

    connection->GetInterface(&some_interface);

Calling this method allocates a Mojo message pipe, binds the client handle to
the provided InterfacePtr, and sends the server handle to the remote service,
where it will eventually (asynchronously) be bound to an object implementing the
requested interface. Now that our InterfacePtr has been bound, we can start
calling methods on it:

    some_interface->Foo();

Now an important note about lifetimes. At this point the Connection returned by
Connect() goes out of scope, and is destroyed. This closes the underlying
InterfaceProvider pipes with the remote service. But Mojo methods are
asynchronous. Does this mean that the call to Foo() above is lost? No. Before
closing, queued writes to the pipe are flushed.

### Implementing an Interface

Let's look at how to implement an interface now from a service and expose it to
inbound connections from other services. To do this we'll need to implement
OnConnect() in our Service implementation, and implement a couple of other
interfaces. For the sake of this example, we'll imagine now we're writing the
'service:other_service' service, implementing the interface defined in this
mojom:

**some_interface.mojom:**

    module mojom;

    interface SomeInterface {
      Foo();
    };

To build this mojom we need to invoke the mojom gn template from
`//mojo/public/tools/bindings/mojom.gni`. Once we do that and look at the
output, we can see that the C++ class mojom::SomeInterface is generated and can
be #included from the same path as the .mojom file at some_interface.mojom.h.
In our implementation of the service:other_service, we'll need to derive
from this class to implement the interface. But that's not enough. We'll also have
to find a way to bind inbound requests to bind this interface to the object that
implements it. Let's look at a snippet of a class that does all of this:

**other_service.cc:**

    class Service : public service_manager::Service,
                    public service_manager::InterfaceFactory<mojom::SomeInterface>,
                    public mojom::SomeInterface {
     public:
      ..

      // Overridden from service_manager::Service:
      bool OnConnect(const service_manager::ServiceInfo& remote_info,
                     service_manager::InterfaceRegistry* registry) override {
        registry->AddInterface<mojom::SomeInterface>(this);
        return true;
      }

      // Overridden from service_manager::InterfaceFactory<mojom::SomeInterface>:
      void Create(const service_manager::Identity& remote_identity,
                  mojom::SomeInterfaceRequest request) override {
        bindings_.AddBinding(this, std::move(request));
      }

      // Overridden from mojom::SomeInterface:
      void Foo() override { /* .. */ }

      mojo::BindingSet<mojom::SomeInterface> bindings_;
    };

Let's study what's going on, starting with the obvious - we derive from
`mojom::SomeInterface` and implement `Foo()`. How do we bind this implementation
to a pipe handle from a connected service? First we have to advertise the
interface to the service via the registry provided at the incoming connection.
This is accomplished in OnConnect():

    registry->AddInterface<mojom::SomeInterface>(this);

This adds the `mojom.SomeInterface` interface name to the inbound
InterfaceRegistry, and tells it to consult this object when it needs to
construct an implementation to bind. Why this object? Well in addition to
Service and SomeInterface, we also implement an instantiation of the generic
interface InterfaceFactory. InterfaceFactory is the missing piece - it binds a
request for SomeInterface (in the form of a message pipe server handle) to the
object that implements the interface (this). This is why we implement Create():

    bindings_.AddBinding(this, std::move(request));

In this case, this single instance binds requests for this interface from all
connected services, so we use a mojo::BindingSet to hold them all.
Alternatively, we could construct an object per request, and use mojo::Binding.

### Statically Declaring Capabilities

While the code above looks like it should work, if we were to type it all in,
build it and run it it still wouldn't. In fact, if we ran it, we'd see this
error in the console:

`InterfaceProviderSpec prevented connection from: service:my_service to service:other_service`

The answer lies in an omission in one of the files I didn't discuss earlier, the
manifest.json, specifically the empty 'interface_provider_specs' dictionary.

When held, a capability controls some behavior in a service, including the
ability to bind specific interfaces. At a primitive level, a simple way to think
about a capability is the ability to bind a pipe and communicate over it.

At the top level, the Service Manager implements the delegation of capabilities
in accordance with rules spelled out in each service's manifest.

Each service produces a manifest file with some typical metadata about itself,
and an 'interface_provider_spec'. An interface_provider_spec describes
capabilities offered by the service and those consumed from other services.
Let's study a fairly complete interface_provider_spec from another service's
manifest:

    "interface_provider_specs": {
      "service_manager:connector": {
        "provides": {
          "web": ["if1", "if2"],
          "uid": []
        },
        "requires": {
          "*": ["c1", "c2"],
          "service:foo": ["c3"]
        }
      }
    }

At the top level of the interface_provider_spec dictionary are one or more
sub-dictionaries, each corresponding to an individual InterfaceProviderSpec
definition. In all cases, there is a dictionary called
"service_manager:connector". The name here means that the spec is meaningful to
the Service Manager, and relates to the service_manager::mojom::InterfaceProvider
pair expressed via the Connector interface. This is the spec that controls what
capabilities are exposed between services via the Connect()/OnConnect() methods.

Within each spec definition there are two sub-dictionaries:

#### Provided Capabilities

The provides dictionary enumerates the capabilities provided by the service. A
capability is an alias, either to some special behavior exposed by the service
to remote services that request that capability, or to a set of interfaces
exposed by the service to remote services. In the former case, in the
dictionary we provide an empty array as the value of the capability key, in the
latter case we provide an array with a list of the fully qualified Mojo
interface names (module.path.InterfaceName). A special case of array is one that
contains the single entry "*", which means 'all interfaces'.

Let's consider our previous example the `service:other_service`, which we want
our `service:my_service` to connect to, and bind mojom::SomeInterface. Every
interface that a service provides that is intended to be reachable via
Connect()/OnConnect() must be statically declared in the manifest as exported
in the providing service's manifest as part of a named capability. A capability
name is just a string that provider and consumer agree upon. Here's what
`service:other_service`'s manifest must then look like:

    {
      "name": "service:other_service",
      "display_name": "Other Service",
      "interface_provider_specs": {
        "service_manager:connector": {
          "provides": {
            "other_capability": [ "mojom.SomeInterface" ]
          }
        }
      }
    }

#### Required Capabilities

The requires dictionary enumerates the capabilities required by the service. The
keys into this dictionary are the names of the services it intends to connect
to, and the values for each key are an array of capability names required of
that service. A "*" key in the 'requires' dictionary allows the service to provide
a capability spec that must be adhered to by all services it connects to.

A consequence of this is that a service must statically declare every interface
it provides in at least one capability in its manifest.

Armed with this knowledge, we can return to manifest.json from the first
example and fill out the interface_provider_specs:

    {
      "name": "service:my_service",
      "display_name": "My Service",
      "interface_provider_specs": {
        "service_manager:connector": {
          "requires": {
            "service:other_service": [],
          }
        }
      }
    }

If we just run now, it still won't work, and we'll see this error:

    InterfaceProviderSpec "service_manager:connector" prevented service:
    service:my_service from binding interface mojom.SomeInterface exposed by:
    service:other_service

The connection was allowed to complete, but the attempt to bind
`mojom.SomeInterface` was blocked. As it happens, this interface is provided as
part of the capability `other_capability` exported by `service:other_service`.
We need to add that capability to the array in our manifest:

    "requires": {
      "service:other_service": [ "other_capability" ],
    }

Now everything should work.

### Testing

Now that we've built a simple application and service, it's time to write a test
for them. The Service Manager client library provides a gtest base class
**service_manager::test::ServiceTest** that makes writing integration tests of
services straightforward. Let's look at a simple test of our service:

    #include "base/bind.h"
    #include "base/run_loop.h"
    #include "services/service_manager/public/cpp/service_test.h"
    #include "path/to/some_interface.mojom.h"

    void QuitLoop(base::RunLoop* loop) {
      loop->Quit();
    }

    class Test : public service_manager::test::ServiceTest {
     public:
      Test() : service_manager::test::ServiceTest("exe:service_unittest") {}
      ~Test() override {}
    }

    TEST_F(Test, Basic) {
      mojom::SomeInterface some_interface;
      connector()->ConnectToInterface("service:other_service", &some_interface);
      base::RunLoop loop;
      some_interface->Foo(base::Bind(&QuitLoop, &loop));
      loop.Run();
    }

The BUILD.gn for this test file looks like any other using the test() template.
It must also depend on
//services/service_manager/public/cpp:service_test_support.

ServiceTest does a few things, but most importantly it register the test itself
as a Service, with the name you pass it via its constructor. In the example
above, we supplied the name 'exe:service_unittest'. This name is has no special
meaning other than that henceforth it will be used to identify the test service.

Behind the scenes, ServiceTest spins up the Service Manager on a background
thread, and asks it to create an instance for the test service on the main
thread, with the name supplied. ServiceTest blocks the main thread while the
Service Manager thread does this initialization. Once the Service Manager has
created the instance, it calls OnStart() (as for any other service), and the
main thread continues, running the test. At this point accessors defined in
service_test.h like connector() can be used to connect to other services.

You'll note in the example above I made Foo() take a callback, this is to give
the test something interesting to do. In the mojom for SomeInterface we'd have
the Foo() method return an empty response. In service:other_service, we'd have
Foo() take the callback as a parameter, and run it. In the test, we spin a
RunLoop until we get that response. In real world cases we can pass back state &
validate expectations. You can see real examples of this test framework in use
in the Service Manager's own suite of tests, under
//services/service_manager/tests.

### Packaging

By default a .library statically links its dependencies, so having many of them
will yield an installed product many times larger than Chrome today. For this
reason it's desirable to package several Services together in a single binary.
The Service Manager provides an interface **service_manager.mojom.ServiceFactory**:

    interface ServiceFactory {
      CreateService(Service& service, string name);
    };

When implemented by a service, the service becomes a 'package' of other
services, which are instantiated by this interface. Imagine we have two services
service:service1 and service:service2, and we wish to package them together in a
single package service:services. We write the Service implementations for
service:service1 and service:service2, and then a Service implementation for
service:services - the latter implements ServiceFactory and instantiates the
other two:

    using service_manager::mojom::ServiceFactory;
    using service_manager::mojom::ServiceRequest;

    class Services : public service_manager::Service,
                     public service_manager::InterfaceFactory<ServiceFactory>,
                     public ServiceFactory {

      // Expose ServiceFactory to inbound connections and implement
      // InterfaceFactory to bind requests for it to this object.
      void CreateService(ServiceRequest request,
                         const std::string& name) {
        if (name == "service:service1")
          new Service1(std::move(request));
        else if (name == "service:service2")
          new Service2(std::move(request));
      }
    }

This is only half the story though. While this does mean that service:service1
and service:service2 are now packaged (statically linked) with service:services,
as it stands to connect to either packaged service you'd have to connect to
service:services first, and call CreateService yourself. This is undesirable for
a couple of reasons, firstly in that it complicates the connect flow, secondly
in that it forces details of the packaging, which are a distribution-level
implementation detail on services wishing to use a service.

To solve this, the Service Manager actually automates resolving packaged service
names to the package service. The Service Manager considers the name of a
service provided by some other package service to be an 'alias' to that package
service. The Service Manager resolves these aliases based on information found,
you guessed it, in the manifests for the package service.

Let's imagine service:service1 and service:service2 have typical manifests of the
form we covered earlier. Now imagine service:services, the package service that
combines the two. In the package install directory rather than the following
structure:

    service1/service1.library,manifest.json
    service2/service2.library,manifest.json

Instead we'll have:

    package/services.library,manifest.json

The manifest for the package service describes not only itself, but includes the
manifests of all the services it provides. Fortunately there is some GN build
magic that automates generating this meta-manifest, so you don't need to write
it by hand. In the service_manifest() template instantiation for services, we
add the following lines:

    deps = [ ":service1_manifest", ":service2_manifest" ]
    packaged_services = [ "service1", "service2" ]

The deps line lists the service_manifest targets for the packaged services to be
consumed, and the packaged_services line provides the service names, without the
'service:' prefix. The presence of these two lines will cause the Manifest Collator
script to run, merging the dependent manifests into the package manifest. You
can study the resulting manifest to see what gets generated.

At startup, the Service Manager will scan the package directory and consume the
manifests it finds, so it can learn about how to resolve aliases that it might
encounter subsequently.

### Executables

Thus far, the examples we've covered have packaged Services in .library files.
It's also possible to have a conventional executable provide a Service. There
are two different ways to use executables with the Service Manager, the first is
to have the Service Manager start the executable itself, the second is to have
some other executable start the process and then tell the Service Manager about
it. In both cases, the target executable has to perform a handshake with the
Service Manager early on so it can bind the Service request the Service Manager
sends it.

Assuming you have an executable that properly initializes the Mojo EDK, you add
the following lines at some point early in application startup to establish the
connection with the Service Manager:

    #include "services/service_manager/public/cpp/service.h"
    #include "services/service_manager/public/cpp/service_context.h"
    #include "services/service_manager/runner/child/runner_connection.h"

    class MyService : public service_manager::Service {
    ..
    };

    service_manager::mojom::ServiceRequest request;
    std::unique_ptr<service_manager::RunnerConnection> connection(
       service_manager::RunnerConnection::ConnectToRunner(
            &request, ScopedMessagePipeHandle()));
    MyService service;
    service_manager::ServiceContext context(&service, std::move(request));

What's happening here? The Service/ServiceContext usage should be familiar from
our earlier examples. The interesting part here happens in
`RunnerConnection::ConnectToRunner()`. Before we look at what ConnectToRunner
does, it's important to cover how this process is launched. In this example,
this process is launched by the Service Manager. This is achieved through the
use of the 'exe' Service Name type. The Service Names we've covered thus far
have looked like 'service:foo'. The 'mojo' prefix means that the Service Manager
should look for a .library file at 'foo/foo.library' alongside the Service Manager
executable. If the code above was linked into an executable 'app.exe' alongside
the Service Manager executable in the output directory, it can be launched by
connecting to the name 'exe:app'. When the Service Manager launches an
executable, it passes a pipe to it on the command line, which the executable is
expected to bind to receive a ServiceRequest on. Now back to ConnectToRunner.
It spins up a background 'control' thread with the Service Manager, binds the
pipe from the command line parameter, and blocks the main thread until the
ServiceRequest arrives and can be bound.

Like services provided from .library files, we have to provide a manifest for
services provided from executables. The format is identical, but in the
service_manifest template we need to set the type property to 'exe' to cause the
generation step to put the manifest in the right place (it gets placed alongside
the executable, with the name <exe_name>_manifest.json.)

### Service-Launched Processes

There are some scenarios where a service will need to launch its own process,
rather than relying on the Service Manager to do it. The Connector API provides
the ability to tell the Service Manager about a process that the service has or
will create. The executable that the service launches (henceforth referred to as
the 'target') should be written using RunnerConnection as discussed in the
previous section. The connect flow in the service that launches the target
(henceforth referred to as the driver) works like this:

    base::FilePath target_path;
    base::PathService::Get(base::DIR_EXE, &target_path);
    target_path = target_path.Append(FILE_PATH_LITERAL("target.exe"));
    base::CommandLine target_command_line(target_path);

    mojo::edk::PlatformChannelPair pair;
    mojo::edk::HandlePassingInformation info;
    pair.PrepareToPassClientHandleToChildProcess(&target_command_line, &info);

    std::string token = mojo::edk::GenerateRandomToken();
    target_command_line.AppendSwitchASCII(switches::kPrimordialPipeToken,
                                          token);

    mojo::ScopedMessagePipeHandle pipe =
        mojo::edk::CreateParentMessagePipe(token);

    service_manager::mojom::ServiceFactoryPtr factory;
    factory.Bind(
        mojo::InterfacePtrInfo<service_manager::mojom::ServiceFactory>(
            std::move(pipe), 0u));
    service_manager::mojom::PIDReceiverPtr receiver;

    service_manager::Identity target("exe:target",service_manager::mojom::kInheritUserID);
    service_manager::Connector::ConnectParams params(target);
    params.set_client_process_connection(std::move(factory),
                                         GetProxy(&receiver));
    std::unique_ptr<service_manager::Connection> connection = connector->Connect(&params);

    base::LaunchOptions options;
    options.handles_to_inherit = &info;
    base::Process process = base::LaunchProcess(target_command_line, options);
    mojo::edk::ChildProcessLaunched(process.Handle(), pair.PassServerHandle());

That's a lot. But it boils down to these steps:
1. Creating the message pipe to connect the target process and the Service
Manager.
2. Putting the server end of the pipe onto the command line to the target
process.
3. Binding the client end to a ServiceFactoryPtr, constructing an Identity for
the target process and passing both through Connector::Connect().
4. Starting the process with the configured command line.

In this example the target executable could be the same as the previous example.

A word about process lifetimes. Processes created by the Service Manager are
also managed by the Service Manager. While a service-launched process may quit
itself at any point, when the Service Manager shuts down it will also shut down
any process it started. Processes created by services themselves are left to
those services to manage.

### Other InterfaceProviderSpecs

We discussed InterfaceProviderSpecs in detail in the section above about
exchange of capabilities between services. That section focused on how
interfaces are exposed via Connect()/OnConnect(). Looking at the structure of
service manifests:

    "interface_provider_specs": {
      "service_manager:connector": {
        "provides": {
          ...
        },
        "requires": {
          ...
        }
      }
    }

It was discussed that the "service_manager:connector" dictionary described
capabilities of interest to the Service Manager. While our use cases thus far
have focused on this single InterfaceProviderSpec, it's possible (and desirable)
to use others, any time an InterfaceProvider is used. Why? Well
InterfaceProvider is a generic interface - it can theoretically be used to bind,
anything and as such it's useful to be able to statically assert what interfaces
are exposed to what contexts. In Chromium, manifest files get security review,
which provides an extra layer of care when we think about what capabilities are
being exposed between contexts at different trust levels. A concrete example
from Chrome - a pair of InterfaceProviders is used to expose frame-specific
interfaces between browser and renderer processes. To define another spec, we do
this:

    "interface_provider_specs": {
      "service_manager:connector": {
        "provides": {
          ...
        },
        "requires": {
          ...
        }
      },
      "my_spec_name": {
        "provides": {
          ...
        },
        "requires": {
          ...
        }
      }
    }

And here again we can define capabilities & consume them. To actually hook up
this new spec in code, we must do what `service_manager::ServiceContext` does
for us with the `service_manager:connector` spec, and configure a
`service_manager::InterfaceRegistry` appropriately:

    void OnStart(const service_manager::ServiceInfo& info) override {
      registry_ =
          base::MakeUnique<service_manager::InterfaceRegistry>("my_spec_name");
      registry_->AddInterface<mojom::Foo>(this);
      registry_->AddInterface<mojom::Bar>(this);

      // Store this so we can use it when we Bind() registry_.
      local_info_ = info;
    }

    bool OnConnect(const service_manager::ServiceInfo& remote_info,
                   service_manager::InterfaceRegistry* remote) override {
      remote_info_ = remote_info;
      registry->AddInterface<mojom::MyInterface>(this);
      return true;
    }

    ...

    // mojom::MyInterface:
    void GetInterfaceProvider(
        service_manager::mojom::InterfaceProviderRequest request) override {
      service_manager::InterfaceProviderSpec my_spec, remote_spec;
      service_manager::GetInterfaceProviderSpec(
          "my_spec_name", local_info_.interface_provider_specs, &my_spec);
      service_manager::GetInterfaceProviderSpec(
          "my_spec_name", remote_info_.interface_provider_specs, &remote_spec);
      registry_->Bind(std::move(request), local_info_.identity, my_spec,
                      remote_info_.identity, remote_spec);
      // |registry_| is now bound to the remote, and its GetInterface()
      // implementation is now controlled via the rules set out in
      // `my_spec_name` declared in this service's and the remote service's
      // manifests.
    }

    ...

    std::unique_ptr<service_manager::InterfaceRegistry> registry_;
    service_manager::ServiceInfo local_info_;
    service_manager::ServiceInfo remote_info_;

When we construct an `InterfaceRegistry` we pass the name of the spec that
controls it. When our service is started we're given (by the Service Manager)
our own spec. This allows us to know everything we provide. When we receive a
connection request from another service, the Service Manager provides us with
the remote service's spec. This is enough information that when we're asked to
bind the InterfaceRegistry to a pipe from the remote, the appropriate filtering
is performed.


***

TBD:

Instances & Processes

Service lifetime strategies

Process lifetimes.

Under the Hood
Four major components: Service Manager API (Mojom), Service Manager, Catalog,
Service Manager Client Lib.
The connect flow, catalog, etc.
Capability brokering in the Service Manager
Userids

Finer points:

Service Names: mojo, exe

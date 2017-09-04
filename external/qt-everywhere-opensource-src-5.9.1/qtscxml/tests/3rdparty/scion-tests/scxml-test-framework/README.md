Overview
========

The SCXML Test Framework project has two main purposes:

1. To collect test cases to run against SCXML implementations. 
2. To implement a generic test runner client for a client-server, HTTP- and JSON-based SCXML testing protocol. By separating out the client test runner from the SCXML interpreter on the server, it should be possible to test various SCXML implementations in a language-agnostic way. 


SCXML Test Cases
===============

Each SCXML test case comprises a pair of documents: 

- an SCXML document
- a JSON "test script", which defines events to send into the state machine, and the expected basic state machine configuration after processing each event


JSON Test Script
----------------

See test [basic/basic1.json](scxml-test-framework/blob/master/test/basic/basic1.json) for an example of the JSON test script format:

```json
{
	"initialConfiguration" : ["a"],
	"events" : [
		{
			"event" : { "name" : "t" },
			"nextConfiguration" : ["b"]
		}
	]
}
```

This test script indicates that after SCXML document [basic/basic1.scxml](scxml-test-framework/blob/master/test/basic/basic1.scxml) is loaded into the SCXML interpreter, the expected initial configuration of the state machine will be a single state with id "a". Next, an event with name "t" and no data will be dispatched on the state machine, and the resulting state machine configuration will be a single state with id "b".

Note that the "initialConfiguration" and "nextConfiguration" properties should only contain the ids of expected *basic* states, which is to say "initialConfiguration" and "nextConfiguration" specify a "basic configuration", or a configuration of basic states. As a "full configuration", or a configuration composed of both basic and non-basic states, can be derived from a basic configuration, specifying only basic configurations in the test script can be done without leading to a loss of safety or generality.

Also note that the format of the test script assumes that each event dispatched on the state machine will trigger a single macrostep which may update the state configuration. The state machine configuration at the end of the macrostep can then be compared to an expected configuration. The testing framework explicitly does not test the intermediate state changes resulting from individual microsteps. This implies that the SCXML implementation being tested must be able to report the state configuration at the end of a macrostep in order for this testing scheme to be applied.


Delay
-----

The JSON test script may also specify that an event should be sent after a delay. For example, from [delayedSend/send1.json](scxml-test-framework/blob/master/test/delayedSend/send1.json):

```json
{
	"initialConfiguration" : ["a"],
	"events" : [
		{
			"event" : { "name" : "t1" },
			"nextConfiguration" : ["b"]
		},
		{
			"after" : 100,
			"event" : { "name" : "t2" },
			"nextConfiguration" : ["d"]
		}
	]
}
```

This test says that after SCXML document [delayedSend/send1.scxml](scxml-test-framework/blob/master/test/delayedSend/send1.scxml) is loaded, the initial configuration will contain only the state with id "a". After dispatching event "t1" on the loaded SCXML session, the next configuration will contain only state with id "b". The test runner will then wait 100 milliseconds, and subsequently dispatch event "t2" on the loaded SCXML session. The expected next configuration will contain only the state with id "d".


Dependency on SCION Semantics
-----------------------------

The JSON test scripts included in this project assume a particular Statecharts semantics will be implemented by the SCXML interpreter. These semantics are currently those of the SCION project, which are documented [here](https://github.com/jbeard4/SCION/wiki/Scion-Semantics), and are not the same as the semantics specified by the Step Algorithm in the SCXML specification. The reason for this is documented [here](https://github.com/jbeard4/SCION/wiki/SCION-vs.-SCXML-Comparison). A desirable feature for this project would be to allow the test scripts to be parameterizable for different Statecharts semantics. For now the best approach to allow for alternative semantics is to fork this project and rewrite the JSON test scripts as needed.


Test Runner Client
==================

The SCXML Test Framework project includes a test runner client, written in JavaScript for node.js, which implements the client side of an HTTP- and JSON-based SCXML testing protocol. The SCXML interpreter implementation to be tested runs on an HTTP server, which the SCXML interpreter project should provide. This should allow various SCXML implementations to be tested in a language-agnostic way.

The test runner client can run tests sequentially or in parallel. It will exit when all tests have completed, and its exit status will be the number of tests failed or errored; thus, if all tests pass, the return status will be 0.


Installation
------------

The SCXML Test Framework can be installed through npm, which is bundled with node.js:

    npm install scxml-test-framework

Or:

    npm install git://github.com/jbeard4/scxml-test-framework.git


Usage
-----

To run it:

    node scxml-test-framework [--test-server-url url] [--parallel] [path/to/test1.scxml [path/to/test2.scxml ...]]

For example, to run the client on all tests included in this project in parallel against the SCXML test server running on localhost:9000 (in the bash shell):

    node scxml-test-framework --test-server-url localhost:9000 --parallel test/*/*.scxml

Note that each SCXML document specified should have a JSON test script in the same directory, with the same basename and a ".json" extension. This is already done for the tests included with this project.

Testing Protocol
----------------

A test case involves the following sequence of events:

1. The client selects a test case, and sends the server a "load" event and a URL pointing to the associated SCXML document (the test client is also running a simple HTTP file server, and so is able to serve this document). 
2. The server receives request to load the SCXML document, downloads the document via an HTTP GET request, and creates a new SCXML session from the document. The server also generates a token that can be used to map subsequent client requests back to the newly-created SCXML session. The server then returns initial configuration of the SCXML session, and the generated token, to the client on the HTTP response.
3. The client receives the server response, and compares the returned initial configuration to the expected initial configuration specified in the test script. 
4. For each event and expected configuration in JSON test script:
    1. The client sends event to server. Each event is sent along with SCXML session token. 
    2. The server receives the event and token, and uses the token to retrieve SCXML session. The server then dispatches the received event on the SCXML session, and returns the new SCXML session configuration to the client on the HTTP response. 
    3. The client receives the new configuration on the HTTP response and compares it to the expected configuration. If the configuration from the server matches the expected configuration, then the client will continue sending events; otherwise, the test fails.


The use of tokens is needed because the client may run tests in parallel, rather than sequentially, which would require multiple SCXML sessions to be loaded on the server simultaneously, and the token is thus needed to distinguish them.


Here is an example of the JSON sent over the wire when running test [basic/basic1.scxml](scxml-test-framework/blob/master/test/basic/basic1.scxml):

Client request to load statechart.

```json
    {
        "load":"http://localhost:9999/test/basic/basic1.scxml"
    }
```

Server response with token and initial configuration

```json
    {
        "sessionToken" : 1,
        "nextConfiguration" : ["a"]
    }
```

Client request to send event to statechart associated with token 1

```json
    {
        "event" : { "name" : "t" },
        "sessionToken" : 1 
    }
```

Server response with next state configuration

```json
    {
        "nextConfiguration" : ["b"]
    }
```

Server Implementation
--------------------

The following are examples of SCXML test server implementations:
* [JavaScript (node.js)](https://github.com/jbeard4/SCION/blob/master/test/node-test-server.js)
* [JavaScript (Rhino)](https://github.com/jbeard4/SCION/blob/master/test/rhino-test-server.js)
* [Python](https://github.com/jbeard4/pySCION/blob/master/test/test-server.py)
* [C#](https://github.com/jbeard4/SCION.NET/blob/master/test/TestServer.cs)

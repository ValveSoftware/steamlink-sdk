This directory contains files from several different implementations and
implementation strategies:

## Traditional ReadableStream Implementation

- ReadableByteStream.{cpp,h,idl}
- ReadableByteStreamReader.{h,idl}
- ReadableStream.{cpp,h,idl}
- ReadableStreamImpl.h
- ReadableStreamReader.{cpp,h,idl}
- ReadableStreamReaderTest.cpp
- ReadableStreamTest.cpp
- UnderlyingSource.h

These files implement the current streams spec, plus the more speculative
ReadableByteStream, to the extent necessary to support Fetch response bodies.
They do not support author-constructed readable streams. They use the normal
approach for implementing web-exposed classes, i.e. IDL bindings with C++
implementation. They are now deprecated and will be removed shortly. See
https://crbug.com/613435.

## V8 Extras ReadableStream Implementation

- ByteLengthQueuingStrategy.js
- CountQueuingStrategy.js
- ReadableStream.js
- ReadableStreamController.h
- UnderlyingSourceBase.{cpp,h,idl}
- bindings/core/v8/ReadableStreamOperations.{cpp,h}

These files implement [V8 extras][1] based ReadableStream which is currently
used.

[1]: https://docs.google.com/document/d/1AT5-T0aHGp7Lt29vPWFr2-qG8r3l9CByyvKwEuA8Ec0

## Old streams

- Stream.{cpp,h,idl}

These files support an old streams spec. They should eventually be removed, but
right now XMLHttpRequest and Media Streams Extension code in Blink still
depends on them.

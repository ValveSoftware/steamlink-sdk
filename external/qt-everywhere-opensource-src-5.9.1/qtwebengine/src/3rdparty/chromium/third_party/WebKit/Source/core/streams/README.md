This directory contains the Blink implementation of the WHATWG Streams standard:
https://streams.spec.whatwg.org/. There is also a legacy streams implementation.

## V8 Extras ReadableStream Implementation

- ByteLengthQueuingStrategy.js
- CountQueuingStrategy.js
- ReadableStream.js
- ReadableStreamController.h
- ReadableStreamOperations.{cpp,h}
- UnderlyingSourceBase.{cpp,h,idl}

These files implement ReadableStream using [V8 extras][1]. All new code should
use this implementation.

[1]: https://docs.google.com/document/d/1AT5-T0aHGp7Lt29vPWFr2-qG8r3l9CByyvKwEuA8Ec0

## Old streams

- Stream.{cpp,h,idl}

These files support an old streams spec. They should eventually be removed, but
right now XMLHttpRequest and Media Streams Extension code in Blink still
depends on them.

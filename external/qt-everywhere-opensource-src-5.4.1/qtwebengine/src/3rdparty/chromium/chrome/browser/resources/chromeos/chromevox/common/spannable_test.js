// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_unittest_base.js']);

UnserializableSpan = function() {};

StatelessSerializableSpan = function() {};

NonStatelessSerializableSpan = function(value) {
  this.value = value;
};

/**
 * @param {!Object} obj object containing the
 *     serializable representation.
 * @return {!Object} The Spannable.
 */
NonStatelessSerializableSpan.fromJson = function(obj) {
  return new NonStatelessSerializableSpan(obj.value / 2);
};

/**
 * @return {Object} the json serializable form.
 */
NonStatelessSerializableSpan.prototype.toJson = function() {
  return {value: this.value * 2};
};

/**
 * Test fixture.
 * @constructor
 * @extends {ChromeVoxUnitTestBase}
 */
function CvoxSpannableUnitTest() {}

CvoxSpannableUnitTest.prototype = {
  __proto__: ChromeVoxUnitTestBase.prototype,

  /** @override */
  closureModuleDeps: [
    'cvox.Spannable',
  ],

  /** @override */
  setUp: function() {
    cvox.Spannable.registerStatelessSerializableSpan(
        StatelessSerializableSpan, 'StatelessSerializableSpan');

    cvox.Spannable.registerSerializableSpan(
        NonStatelessSerializableSpan, 'NonStatelessSerializableSpan',
        NonStatelessSerializableSpan.fromJson,
        NonStatelessSerializableSpan.prototype.toJson);
  }
};

TEST_F('CvoxSpannableUnitTest', 'ToStringUnannotated', function() {
  assertEquals('', new cvox.Spannable().toString());
  assertEquals('hello world', new cvox.Spannable('hello world').toString());
});

/** Tests that toString works correctly on annotated strings. */
TEST_F('CvoxSpannableUnitTest', 'ToStringAnnotated', function() {
  var spannable = new cvox.Spannable('Hello Google');
  spannable.setSpan('http://www.google.com/', 6, 12);
  assertEquals('Hello Google', spannable.toString());
});

/** Tests the length calculation. */
TEST_F('CvoxSpannableUnitTest', 'GetLength', function() {
  var spannable = new cvox.Spannable('Hello');
  spannable.setSpan({}, 0, 3);
  assertEquals(5, spannable.getLength());
  spannable.append(' world');
  assertEquals(11, spannable.getLength());
  spannable.append(new cvox.Spannable(' from cvox.Spannable'));
  assertEquals(31, spannable.getLength());
});

/** Tests that a span can be added and retrieved at the beginning. */
TEST_F('CvoxSpannableUnitTest', 'SpanBeginning', function() {
  var annotation = {};
  var spannable = new cvox.Spannable('Hello world');
  spannable.setSpan(annotation, 0, 5);
  assertSame(annotation, spannable.getSpan(0));
  assertSame(annotation, spannable.getSpan(3));
  assertUndefined(spannable.getSpan(5));
  assertUndefined(spannable.getSpan(8));
});

/** Tests that a span can be added and retrieved at the beginning. */
TEST_F('CvoxSpannableUnitTest', 'SpanEnd', function() {
  var annotation = {};
  var spannable = new cvox.Spannable('Hello world');
  spannable.setSpan(annotation, 6, 11);
  assertUndefined(spannable.getSpan(3));
  assertUndefined(spannable.getSpan(5));
  assertSame(annotation, spannable.getSpan(6));
  assertSame(annotation, spannable.getSpan(10));
});

/** Tests that a zero-length span is not retrieved. */
TEST_F('CvoxSpannableUnitTest', 'SpanZeroLength', function() {
  var annotation = {};
  var spannable = new cvox.Spannable('Hello world');
  spannable.setSpan(annotation, 3, 3);
  assertUndefined(spannable.getSpan(2));
  assertUndefined(spannable.getSpan(3));
  assertUndefined(spannable.getSpan(4));
});

/** Tests that a removed span is not returned. */
TEST_F('CvoxSpannableUnitTest', 'RemoveSpan', function() {
  var annotation = {};
  var spannable = new cvox.Spannable('Hello world');
  spannable.setSpan(annotation, 0, 3);
  assertSame(annotation, spannable.getSpan(1));
  spannable.removeSpan(annotation);
  assertUndefined(spannable.getSpan(1));
});

/** Tests that adding a span in one place removes it from another. */
TEST_F('CvoxSpannableUnitTest', 'SetSpanMoves', function() {
  var annotation = {};
  var spannable = new cvox.Spannable('Hello world');
  spannable.setSpan(annotation, 0, 3);
  assertSame(annotation, spannable.getSpan(1));
  assertUndefined(spannable.getSpan(4));
  spannable.setSpan(annotation, 3, 6);
  assertUndefined(spannable.getSpan(1));
  assertSame(annotation, spannable.getSpan(4));
});

/** Tests that setSpan objects to out-of-range arguments. */
TEST_F('CvoxSpannableUnitTest', 'SetSpanRangeError', function() {
  var spannable = new cvox.Spannable('Hello world');

  // Start index out of range.
  assertException('expected range error', function() {
    spannable.setSpan({}, -1, 0);
  }, 'RangeError');

  // End index out of range.
  assertException('expected range error', function() {
    spannable.setSpan({}, 0, 12);
  }, 'RangeError');

  // End before start.
  assertException('expected range error', function() {
    spannable.setSpan({}, 1, 0);
  }, 'RangeError');
});

/**
 * Tests that multiple spans can be retrieved at one point.
 * The first one added which applies should be returned by getSpan.
 */
TEST_F('CvoxSpannableUnitTest', 'MultipleSpans', function() {
  var annotation1 = { number: 1 };
  var annotation2 = { number: 2 };
  assertNotSame(annotation1, annotation2);
  var spannable = new cvox.Spannable('Hello world');
  spannable.setSpan(annotation1, 1, 4);
  spannable.setSpan(annotation2, 2, 7);
  assertSame(annotation1, spannable.getSpan(1));
  assertThat([annotation1], eqJSON(spannable.getSpans(1)));
  assertSame(annotation1, spannable.getSpan(3));
  assertThat([annotation1, annotation2], eqJSON(spannable.getSpans(3)));
  assertSame(annotation2, spannable.getSpan(6));
  assertThat([annotation2], eqJSON(spannable.getSpans(6)));
});

/** Tests that appending appends the strings. */
TEST_F('CvoxSpannableUnitTest', 'AppendToString', function() {
  var spannable = new cvox.Spannable('Google');
  assertEquals('Google', spannable.toString());
  spannable.append(' Chrome');
  assertEquals('Google Chrome', spannable.toString());
  spannable.append(new cvox.Spannable('Vox'));
  assertEquals('Google ChromeVox', spannable.toString());
});

/**
 * Tests that appending Spannables combines annotations.
 */
TEST_F('CvoxSpannableUnitTest', 'AppendAnnotations', function() {
  var annotation1 = { number: 1 };
  var annotation2 = { number: 2 };
  assertNotSame(annotation1, annotation2);
  var left = new cvox.Spannable('hello');
  left.setSpan(annotation1, 0, 3);
  var right = new cvox.Spannable(' world');
  right.setSpan(annotation2, 0, 3);
  left.append(right);
  assertSame(annotation1, left.getSpan(1));
  assertSame(annotation2, left.getSpan(6));
});

/**
 * Tests that a span's bounds can be retrieved.
 */
TEST_F('CvoxSpannableUnitTest', 'GetSpanStartAndEnd', function() {
  var annotation = {};
  var spannable = new cvox.Spannable('potato wedges');
  spannable.setSpan(annotation, 8, 12);
  assertEquals(8, spannable.getSpanStart(annotation));
  assertEquals(12, spannable.getSpanEnd(annotation));
});

/**
 * Tests that an absent span's bounds are reported correctly.
 */
TEST_F('CvoxSpannableUnitTest', 'GetSpanStartAndEndAbsent', function() {
  var annotation = {};
  var spannable = new cvox.Spannable('potato wedges');
  assertUndefined(spannable.getSpanStart(annotation));
  assertUndefined(spannable.getSpanEnd(annotation));
});

/**
 * Test that a zero length span can still be found.
 */
TEST_F('CvoxSpannableUnitTest', 'GetSpanStartAndEndZeroLength', function() {
  var annotation = {};
  var spannable = new cvox.Spannable('potato wedges');
  spannable.setSpan(annotation, 8, 8);
  assertEquals(8, spannable.getSpanStart(annotation));
  assertEquals(8, spannable.getSpanEnd(annotation));
});

/**
 * Tests that == (but not ===) objects are treated distinctly when getting
 * span bounds.
 */
TEST_F('CvoxSpannableUnitTest', 'GetSpanStartAndEndEquality', function() {
  // Note that 0 == '' and '' == 0 in JavaScript.
  var spannable = new cvox.Spannable('wat');
  spannable.setSpan(0, 0, 0);
  spannable.setSpan('', 1, 3);
  assertEquals(0, spannable.getSpanStart(0));
  assertEquals(0, spannable.getSpanEnd(0));
  assertEquals(1, spannable.getSpanStart(''));
  assertEquals(3, spannable.getSpanEnd(''));
});

/**
 * Tests that substrings have the correct character sequence.
 */
TEST_F('CvoxSpannableUnitTest', 'Substring', function() {
  var assertSubstringResult = function(expected, initial, start, opt_end) {
    var spannable = new cvox.Spannable(initial);
    var substring = spannable.substring(start, opt_end);
    assertEquals(expected, substring.toString());
  };
  assertSubstringResult('Page', 'Google PageRank', 7, 11);
  assertSubstringResult('Goog', 'Google PageRank', 0, 4);
  assertSubstringResult('Rank', 'Google PageRank', 11, 15);
  assertSubstringResult('Rank', 'Google PageRank', 11);
});

/**
 * Tests that substring arguments are validated properly.
 */
TEST_F('CvoxSpannableUnitTest', 'SubstringRangeError', function() {
  var assertRangeError = function(initial, start, opt_end) {
    var spannable = new cvox.Spannable(initial);
    assertException('expected range error', function() {
      spannable.substring(start, opt_end);
    }, 'RangeError');
  };
  assertRangeError('Google PageRank', -1, 5);
  assertRangeError('Google PageRank', 0, 99);
  assertRangeError('Google PageRank', 5, 2);
});

/**
 * Tests that spans in the substring range are preserved.
 */
TEST_F('CvoxSpannableUnitTest', 'SubstringSpansIncluded', function() {
  var assertSpanIncluded = function(expectedSpanStart, expectedSpanEnd,
      initial, initialSpanStart, initialSpanEnd, start, opt_end) {
    var annotation = {};
    var spannable = new cvox.Spannable(initial);
    spannable.setSpan(annotation, initialSpanStart, initialSpanEnd);
    var substring = spannable.substring(start, opt_end);
    assertEquals(expectedSpanStart, substring.getSpanStart(annotation));
    assertEquals(expectedSpanEnd, substring.getSpanEnd(annotation));
  };
  assertSpanIncluded(1, 5, 'potato wedges', 8, 12, 7);
  assertSpanIncluded(1, 5, 'potato wedges', 8, 12, 7, 13);
  assertSpanIncluded(1, 5, 'potato wedges', 8, 12, 7, 12);
  assertSpanIncluded(0, 4, 'potato wedges', 8, 12, 8, 12);
  assertSpanIncluded(0, 3, 'potato wedges', 0, 3, 0);
  assertSpanIncluded(0, 3, 'potato wedges', 0, 3, 0, 3);
  assertSpanIncluded(0, 3, 'potato wedges', 0, 3, 0, 6);
  assertSpanIncluded(0, 5, 'potato wedges', 8, 13, 8);
  assertSpanIncluded(0, 5, 'potato wedges', 8, 13, 8, 13);
  assertSpanIncluded(1, 6, 'potato wedges', 8, 13, 7, 13);

  // Note: we should keep zero-length spans, even at the edges of the range.
  assertSpanIncluded(0, 0, 'potato wedges', 0, 0, 0, 0);
  assertSpanIncluded(0, 0, 'potato wedges', 0, 0, 0, 6);
  assertSpanIncluded(1, 1, 'potato wedges', 8, 8, 7, 13);
  assertSpanIncluded(6, 6, 'potato wedges', 6, 6, 0, 6);
});

/**
 * Tests that spans outside the range are omitted.
 * It's fine to keep zero-length spans at the ends, though.
 */
TEST_F('CvoxSpannableUnitTest', 'SubstringSpansExcluded', function() {
  var assertSpanExcluded = function(initial, spanStart, spanEnd,
      start, opt_end) {
    var annotation = {};
    var spannable = new cvox.Spannable(initial);
    spannable.setSpan(annotation, spanStart, spanEnd);
    var substring = spannable.substring(start, opt_end);
    assertUndefined(substring.getSpanStart(annotation));
    assertUndefined(substring.getSpanEnd(annotation));
  };
  assertSpanExcluded('potato wedges', 8, 12, 0, 6);
  assertSpanExcluded('potato wedges', 7, 12, 0, 6);
  assertSpanExcluded('potato wedges', 0, 6, 8);
  assertSpanExcluded('potato wedges', 6, 6, 8);
});

/**
 * Tests that spans which cross the boundary are clipped.
 */
TEST_F('CvoxSpannableUnitTest', 'SubstringSpansClipped', function() {
  var assertSpanIncluded = function(expectedSpanStart, expectedSpanEnd,
      initial, initialSpanStart, initialSpanEnd, start, opt_end) {
    var annotation = {};
    var spannable = new cvox.Spannable(initial);
    spannable.setSpan(annotation, initialSpanStart, initialSpanEnd);
    var substring = spannable.substring(start, opt_end);
    assertEquals(expectedSpanStart, substring.getSpanStart(annotation));
    assertEquals(expectedSpanEnd, substring.getSpanEnd(annotation));
  };
  assertSpanIncluded(0, 4, 'potato wedges', 7, 13, 8, 12);
  assertSpanIncluded(0, 0, 'potato wedges', 0, 6, 0, 0);
  assertSpanIncluded(0, 0, 'potato wedges', 0, 6, 6, 6);

  // The first of the above should produce "edge".
  assertEquals('edge',
      new cvox.Spannable('potato wedges').substring(8, 12).toString());
});

/**
 * Tests that whitespace is trimmed.
 */
TEST_F('CvoxSpannableUnitTest', 'Trim', function() {
  var assertTrimResult = function(expected, initial) {
    assertEquals(expected, new cvox.Spannable(initial).trim().toString());
  };
  assertTrimResult('John F. Kennedy', 'John F. Kennedy');
  assertTrimResult('John F. Kennedy', '  John F. Kennedy');
  assertTrimResult('John F. Kennedy', 'John F. Kennedy     ');
  assertTrimResult('John F. Kennedy', '   \r\t   \nJohn F. Kennedy\n\n \n');
  assertTrimResult('', '');
  assertTrimResult('', '     \t\t    \n\r');
});

/**
 * Tests that trim keeps, drops and clips spans.
 */
TEST_F('CvoxSpannableUnitTest', 'TrimSpans', function() {
  var spannable = new cvox.Spannable(' \t Kennedy\n');
  spannable.setSpan('tab', 1, 2);
  spannable.setSpan('jfk', 3, 10);
  spannable.setSpan('jfk-newline', 3, 11);
  var trimmed = spannable.trim();
  assertUndefined(trimmed.getSpanStart('tab'));
  assertUndefined(trimmed.getSpanEnd('tab'));
  assertEquals(0, trimmed.getSpanStart('jfk'));
  assertEquals(7, trimmed.getSpanEnd('jfk'));
  assertEquals(0, trimmed.getSpanStart('jfk-newline'));
  assertEquals(7, trimmed.getSpanEnd('jfk-newline'));
});

/**
 * Tests that when a string is all whitespace, we trim off the *end*.
 */
TEST_F('CvoxSpannableUnitTest', 'TrimAllWhitespace', function() {
  var spannable = new cvox.Spannable('    ');
  spannable.setSpan('cursor 1', 0, 0);
  spannable.setSpan('cursor 2', 2, 2);
  var trimmed = spannable.trim();
  assertEquals(0, trimmed.getSpanStart('cursor 1'));
  assertEquals(0, trimmed.getSpanEnd('cursor 1'));
  assertUndefined(trimmed.getSpanStart('cursor 2'));
  assertUndefined(trimmed.getSpanEnd('cursor 2'));
});

/**
 * Tests finding a span which is an instance of a given class.
 */
TEST_F('CvoxSpannableUnitTest', 'GetSpanInstanceOf', function() {
  function ExampleConstructorBase() {}
  function ExampleConstructor1() {}
  function ExampleConstructor2() {}
  function ExampleConstructor3() {}
  ExampleConstructor1.prototype = new ExampleConstructorBase();
  ExampleConstructor2.prototype = new ExampleConstructorBase();
  ExampleConstructor3.prototype = new ExampleConstructorBase();
  var ex1 = new ExampleConstructor1();
  var ex2 = new ExampleConstructor2();
  var spannable = new cvox.Spannable('Hello world');
  spannable.setSpan(ex1, 0, 0);
  spannable.setSpan(ex2, 1, 1);
  assertEquals(ex1, spannable.getSpanInstanceOf(ExampleConstructor1));
  assertEquals(ex2, spannable.getSpanInstanceOf(ExampleConstructor2));
  assertUndefined(spannable.getSpanInstanceOf(ExampleConstructor3));
  assertEquals(ex1, spannable.getSpanInstanceOf(ExampleConstructorBase));
});

/** Tests trimming only left or right. */
TEST_F('CvoxSpannableUnitTest', 'TrimLeftOrRight', function() {
  var spannable = new cvox.Spannable('    ');
  spannable.setSpan('cursor 1', 0, 0);
  spannable.setSpan('cursor 2', 2, 2);
  var trimmed = spannable.trimLeft();
  assertEquals(0, trimmed.getSpanStart('cursor 1'));
  assertEquals(0, trimmed.getSpanEnd('cursor 1'));
  assertUndefined(trimmed.getSpanStart('cursor 2'));
  assertUndefined(trimmed.getSpanEnd('cursor 2'));

  var spannable2 = new cvox.Spannable('0  ');
  spannable2.setSpan('cursor 1', 0, 0);
  spannable2.setSpan('cursor 2', 2, 2);
  var trimmed2 = spannable2.trimLeft();
  assertEquals(0, trimmed2.getSpanStart('cursor 1'));
  assertEquals(0, trimmed2.getSpanEnd('cursor 1'));
  assertEquals(2, trimmed2.getSpanStart('cursor 2'));
  assertEquals(2, trimmed2.getSpanEnd('cursor 2'));
  trimmed2 = trimmed2.trimRight();
  assertEquals(0, trimmed2.getSpanStart('cursor 1'));
  assertEquals(0, trimmed2.getSpanEnd('cursor 1'));
  assertUndefined(trimmed2.getSpanStart('cursor 2'));
  assertUndefined(trimmed2.getSpanEnd('cursor 2'));

  var spannable3 = new cvox.Spannable('  0');
  spannable3.setSpan('cursor 1', 0, 0);
  spannable3.setSpan('cursor 2', 2, 2);
  var trimmed3 = spannable3.trimRight();
  assertEquals(0, trimmed3.getSpanStart('cursor 1'));
  assertEquals(0, trimmed3.getSpanEnd('cursor 1'));
  assertEquals(2, trimmed3.getSpanStart('cursor 2'));
  assertEquals(2, trimmed3.getSpanEnd('cursor 2'));
  trimmed3 = trimmed3.trimLeft();
  assertUndefined(trimmed3.getSpanStart('cursor 1'));
  assertUndefined(trimmed3.getSpanEnd('cursor 1'));
  assertEquals(0, trimmed3.getSpanStart('cursor 2'));
  assertEquals(0, trimmed3.getSpanEnd('cursor 2'));
});

TEST_F('CvoxSpannableUnitTest', 'Serialize', function() {
  var fresh = new cvox.Spannable('text');
  var freshStatelessSerializable = new StatelessSerializableSpan();
  var freshNonStatelessSerializable = new NonStatelessSerializableSpan(14);
  fresh.setSpan(new UnserializableSpan(), 0, 1);
  fresh.setSpan(freshStatelessSerializable, 0, 2);
  fresh.setSpan(freshNonStatelessSerializable, 3, 4);
  var thawn = cvox.Spannable.fromJson(fresh.toJson());
  var thawnStatelessSerializable = thawn.getSpanInstanceOf(
      StatelessSerializableSpan);
  var thawnNonStatelessSerializable = thawn.getSpanInstanceOf(
      NonStatelessSerializableSpan);
  assertThat('text', eqJSON(thawn.toString()));
  assertUndefined(thawn.getSpanInstanceOf(UnserializableSpan));
  assertThat(
      fresh.getSpanStart(freshStatelessSerializable),
      eqJSON(thawn.getSpanStart(thawnStatelessSerializable)));
  assertThat(
      fresh.getSpanEnd(freshStatelessSerializable),
      eqJSON(thawn.getSpanEnd(thawnStatelessSerializable)));
  assertThat(freshNonStatelessSerializable,
             eqJSON(thawnNonStatelessSerializable));
});

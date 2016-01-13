#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Produces localized strings.xml files for Android.

In cases where an "android" type output file is requested in a grd, the classes
in android_xml will process the messages and translations to produce a valid
strings.xml that is properly localized with the specified language.

For example if the following output tag were to be included in a grd file
  <outputs>
    ...
    <output filename="values-es/strings.xml" type="android" lang="es" />
    ...
  </outputs>

for a grd file with the following messages:

  <message name="IDS_HELLO" desc="Simple greeting">Hello</message>
  <message name="IDS_WORLD" desc="The world">world</message>

and there existed an appropriate xtb file containing the Spanish translations,
then the output would be:

  <?xml version="1.0" encoding="utf-8"?>
  <resources xmlns:android="http://schemas.android.com/apk/res/android">
    <string name="hello">"Hola"</string>
    <string name="world">"mundo"</string>
  </resources>

which would be written to values-es/strings.xml and usable by the Android
resource framework.

Advanced usage
--------------

To process only certain messages in a grd file, tag each desired message by
adding "android_java" to formatter_data. Then set the environmental variable
ANDROID_JAVA_TAGGED_ONLY to "true" when building the grd file. For example:

  <message name="IDS_HELLO" formatter_data="android_java">Hello</message>

To specify the product attribute to be added to a <string> element, add
"android_java_product" to formatter_data. "android_java_name" can be used to
override the name in the <string> element. For example,

  <message name="IDS_FOO_NOSDCARD" formatter_data="android_java_product=nosdcard
      android_java_name=foo">no card</message>
  <message name="IDS_FOO_DEFAULT" formatter_data="android_java_product=default
      android_java_name=foo">has card</message>

would generate

  <string name="foo" product="nosdcard">"no card"</string>
  <string name="foo" product="default">"has card"</string>
"""

import os
import types
import xml.sax.saxutils

from grit import lazy_re
from grit.node import message


# When this environmental variable has value "true", only tagged messages will
# be outputted.
_TAGGED_ONLY_ENV_VAR = 'ANDROID_JAVA_TAGGED_ONLY'
_TAGGED_ONLY_DEFAULT = False

# In tagged-only mode, only messages with this tag will be ouputted.
_EMIT_TAG = 'android_java'

# This tag controls the product attribute of the generated <string> element.
_PRODUCT_TAG = 'android_java_product'

# This tag controls the name attribute of the generated <string> element.
_NAME_TAG = 'android_java_name'

# The Android resource name and optional product information are placed
# in the grd string name because grd doesn't know about Android product
# information.
# TODO(newt): Don't allow product information in mangled names, since it can now
#             be specified using "android_java_product" in formatter_data.
_NAME_PATTERN = lazy_re.compile(
    'IDS_(?P<name>[A-Z0-9_]+)(_product_(?P<product>[a-z]+))?\Z')


# In most cases we only need a name attribute and string value.
_SIMPLE_TEMPLATE = u'<string name="%s">%s</string>\n'


# In a few cases a product attribute is needed.
_PRODUCT_TEMPLATE = u'<string name="%s" product="%s">%s</string>\n'


def Format(root, lang='en', output_dir='.'):
  yield ('<?xml version="1.0" encoding="utf-8"?>\n'
          '<resources '
          'xmlns:android="http://schemas.android.com/apk/res/android">\n')

  tagged_only = _TAGGED_ONLY_DEFAULT
  if _TAGGED_ONLY_ENV_VAR in os.environ:
    tagged_only = os.environ[_TAGGED_ONLY_ENV_VAR].lower()
    if tagged_only == 'true':
      tagged_only = True
    elif tagged_only == 'false':
      tagged_only = False
    else:
      raise Exception('env variable ANDROID_JAVA_TAGGED_ONLY must have value '
                      'true or false. Invalid value: %s' % tagged_only)

  for item in root.ActiveDescendants():
    with item:
      if ShouldOutputNode(item, tagged_only):
        yield _FormatMessage(item, lang)

  yield '</resources>\n'


def ShouldOutputNode(node, tagged_only):
  """Returns true if node should be outputted.

  Args:
      node: a Node from the grd dom
      tagged_only: true, if only tagged messages should be outputted
  """
  return (isinstance(node, message.MessageNode) and
          (not tagged_only or _EMIT_TAG in node.formatter_data))


def _FormatMessage(item, lang):
  """Writes out a single string as a <resource/> element."""

  value = item.ws_at_start + item.Translate(lang) + item.ws_at_end
  # Replace < > & with &lt; &gt; &amp; to ensure we generate valid XML and
  # replace ' " with \' \" to conform to Android's string formatting rules.
  value = xml.sax.saxutils.escape(value, {"'": "\\'", '"': '\\"'})
  # Wrap the string in double quotes to preserve whitespace.
  value = '"' + value + '"'

  mangled_name = item.GetTextualIds()[0]
  match = _NAME_PATTERN.match(mangled_name)
  if not match:
    raise Exception('Unexpected resource name: %s' % mangled_name)
  name = match.group('name').lower()
  product = match.group('product')

  # Override product or name with values in formatter_data, if any.
  product = item.formatter_data.get(_PRODUCT_TAG, product)
  name = item.formatter_data.get(_NAME_TAG, name)

  if product:
    return _PRODUCT_TEMPLATE % (name, product, value)
  else:
    return _SIMPLE_TEMPLATE % (name, value)

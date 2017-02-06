#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates a JSON typemap from its command-line arguments and dependencies.

Each typemap should be specified in an command-line argument of the form
key=value, with an argument of "--start-typemap" preceding each typemap.

For example,
generate_type_mappings.py --output=foo.typemap --start-typemap \\
    public_headers=foo.h traits_headers=foo_traits.h \\
    type_mappings=mojom.Foo=FooImpl

generates a foo.typemap containing
{
  "c++": {
    "mojom.Foo": {
      "typename": "FooImpl",
      "traits_headers": [
        "foo_traits.h"
      ],
      "public_headers": [
        "foo.h"
      ]
    }
  }
}

Then,
generate_type_mappings.py --dependency foo.typemap --output=bar.typemap \\
    --start-typemap public_headers=bar.h traits_headers=bar_traits.h \\
    type_mappings=mojom.Bar=BarImpl

generates a bar.typemap containing
{
  "c++": {
    "mojom.Bar": {
      "typename": "BarImpl",
      "traits_headers": [
        "bar_traits.h"
      ],
      "public_headers": [
        "bar.h"
      ]
    },
    "mojom.Foo": {
      "typename": "FooImpl",
      "traits_headers": [
        "foo_traits.h"
      ],
      "public_headers": [
        "foo.h"
      ]
    }
  }
}
"""

import argparse
import json
import os
import re


def ReadTypemap(path):
  with open(path) as f:
    return json.load(f)['c++']


def ParseTypemapArgs(args):
  typemaps = [s for s in '\n'.join(args).split('--start-typemap\n') if s]
  result = {}
  for typemap in typemaps:
    result.update(ParseTypemap(typemap))
  return result


def ParseTypemap(typemap):
  values = {'type_mappings': [], 'public_headers': [], 'traits_headers': []}
  for line in typemap.split('\n'):
    if not line:
      continue
    key, _, value = line.partition('=')
    values[key].append(value.lstrip('/'))
  result = {}
  mapping_pattern = \
      re.compile(r"""^([^=]+)           # mojom type
                     =
                     ([^[]+)            # native type
                     (?:\[([^]]+)\])?$  # optional attribute in square brackets
                 """, re.X)
  for typename in values['type_mappings']:
    match_result = mapping_pattern.match(typename)
    assert match_result, (
        "Cannot parse entry in the \"type_mappings\" section: %s" % typename)

    mojom_type = match_result.group(1)
    native_type = match_result.group(2)
    # The only attribute supported currently is "pass_by_value".
    pass_by_value = (match_result.group(3) and
                     match_result.group(3) == "pass_by_value")

    assert mojom_type not in result, (
        "Cannot map multiple native types (%s, %s) to the same mojom type: %s" %
        (result[mojom_type]['typename'], native_type, mojom_type))

    result[mojom_type] = {
        'typename': native_type,
        'pass_by_value': pass_by_value,
        'public_headers': values['public_headers'],
        'traits_headers': values['traits_headers'],
    }
  return result


def main():
  parser = argparse.ArgumentParser(
      description=__doc__,
      formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument(
      '--dependency',
      type=str,
      action='append',
      default=[],
      help=('A path to another JSON typemap to merge into the output. '
            'This may be repeated to merge multiple typemaps.'))
  parser.add_argument('--output',
                      type=str,
                      required=True,
                      help='The path to which to write the generated JSON.')
  params, typemap_params = parser.parse_known_args()
  typemaps = ParseTypemapArgs(typemap_params)
  missing = [path for path in params.dependency if not os.path.exists(path)]
  if missing:
    raise IOError('Missing dependencies: %s' % ', '.join(missing))
  for path in params.dependency:
    typemaps.update(ReadTypemap(path))
  with open(params.output, 'w') as f:
    json.dump({'c++': typemaps}, f, indent=2)


if __name__ == '__main__':
  main()

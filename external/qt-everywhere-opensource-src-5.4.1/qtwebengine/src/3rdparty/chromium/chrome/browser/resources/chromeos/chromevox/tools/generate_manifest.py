#!/usr/bin/env python

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import io
import optparse
import os
import sys

jinja2_path = os.path.normpath(os.path.join(os.path.abspath(__file__),
                           *[os.path.pardir] * 7 + ['third_party']))
nom_path = os.path.normpath(os.path.join(os.path.abspath(__file__),
    *[os.path.pardir] * 7 + ['tools/json_comment_eater']))
sys.path.insert(0, jinja2_path)
sys.path.insert(0, nom_path)
import jinja2
from json_comment_eater import Nom


'''Generate an extension manifest based on a template.'''


def processJinjaTemplate(input_file, output_file, context):
  (template_path, template_name) = os.path.split(input_file)
  env = jinja2.Environment(loader=jinja2.FileSystemLoader(template_path),
                           trim_blocks=True)
  template = env.get_template(template_name)
  rendered = template.render(context)
  rendered_without_comments = Nom(rendered)
  # Simply for validation.
  json.loads(rendered_without_comments)
  with io.open(output_file, 'w', encoding='utf-8') as manifest_file:
    manifest_file.write(rendered_without_comments)


def main():
  parser = optparse.OptionParser(description=__doc__)
  parser.usage = '%prog [options] <template_manifest_path>'
  parser.add_option(
      '-o', '--output_manifest', action='store', metavar='OUTPUT_MANIFEST',
      help='File to place generated manifest')
  parser.add_option(
      '-g', '--is_guest_manifest', action='store', metavar='GUEST_MANIFEST',
      help='Generate a guest mode capable manifest')
  parser.add_option(
      '--use_chromevox_next', action='store', metavar='CHROMEVOX2',
      help='Generate a ChromeVox next manifest')

  options, args = parser.parse_args()
  if len(args) != 1:
    print >>sys.stderr, 'Expected exactly one argument'
    sys.exit(1)
  processJinjaTemplate(args[0], options.output_manifest, parser.values.__dict__)

if __name__ == '__main__':
  main()

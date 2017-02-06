#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.format.data_pack'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest

from grit import exception
from grit.format import data_pack


class FormatDataPackUnittest(unittest.TestCase):
  def testWriteDataPack(self):
    expected = (
        '\x04\x00\x00\x00'                  # header(version
        '\x04\x00\x00\x00'                  #        no. entries,
        '\x01'                              #        encoding)
        '\x01\x00\x27\x00\x00\x00'          # index entry 1
        '\x04\x00\x27\x00\x00\x00'          # index entry 4
        '\x06\x00\x33\x00\x00\x00'          # index entry 6
        '\x0a\x00\x3f\x00\x00\x00'          # index entry 10
        '\x00\x00\x3f\x00\x00\x00'          # extra entry for the size of last
        'this is id 4this is id 6')         # data
    input = {1: '', 4: 'this is id 4', 6: 'this is id 6', 10: ''}
    output = data_pack.WriteDataPackToString(input, data_pack.UTF8)
    self.failUnless(output == expected)

  def testIncludeWithReservedHeader(self):
    from grit import util
    from grit.node import misc, include, empty
    root = misc.GritNode()
    root.StartParsing(u'grit', None)
    root.HandleAttribute(u'latest_public_release', u'0')
    root.HandleAttribute(u'current_release', u'1')
    root.HandleAttribute(u'base_dir', ur'..\resource')
    release = misc.ReleaseNode()
    release.StartParsing(u'release', root)
    release.HandleAttribute(u'seq', u'1')
    root.AddChild(release)
    includes = empty.IncludesNode()
    includes.StartParsing(u'includes', release)
    release.AddChild(includes)
    include_node = include.IncludeNode()
    include_node.StartParsing(u'include', includes)
    include_node.HandleAttribute(u'name', u'test')
    include_node.HandleAttribute(u'type', u'BINDATA')
    include_node.HandleAttribute(u'file', u'doesntmatter')
    includes.AddChild(include_node)
    include_node.EndParsing()
    root.EndParsing()

    ReadFile_copy = util.ReadFile
    try:
      util.ReadFile = lambda a, b: include.IncludeNode.RESERVED_HEADER
      with self.assertRaises(exception.ReservedHeaderCollision):
        data_pack.Format(root)

    finally:
      util.ReadFile = ReadFile_copy

  def testRePackUnittest(self):
    expected_with_whitelist = {
        1: 'Never gonna', 10: 'give you up', 20: 'Never gonna let',
        30: 'you down', 40: 'Never', 50: 'gonna run around and',
        60: 'desert you'}
    expected_without_whitelist = {
        1: 'Never gonna', 10: 'give you up', 20: 'Never gonna let', 65: 'Close',
        30: 'you down', 40: 'Never', 50: 'gonna run around and', 4: 'click',
        60: 'desert you', 6: 'chirr', 32: 'oops, try again', 70: 'Awww, snap!'}
    inputs = [{1: 'Never gonna', 4: 'click', 6: 'chirr', 10: 'give you up'},
              {20: 'Never gonna let', 30: 'you down', 32: 'oops, try again'},
              {40: 'Never', 50: 'gonna run around and', 60: 'desert you'},
              {65: 'Close', 70: 'Awww, snap!'}]
    whitelist = [1, 10, 20, 30, 40, 50, 60]
    inputs = [data_pack.DataPackContents(input, data_pack.UTF8) for input
              in inputs]

    # RePack using whitelist
    output, _ = data_pack.RePackFromDataPackStrings(inputs, whitelist)
    self.assertDictEqual(expected_with_whitelist, output,
                         'Incorrect resource output')

    # RePack a None whitelist
    output, _ = data_pack.RePackFromDataPackStrings(inputs, None)
    self.assertDictEqual(expected_without_whitelist, output,
                         'Incorrect resource output')


if __name__ == '__main__':
  unittest.main()

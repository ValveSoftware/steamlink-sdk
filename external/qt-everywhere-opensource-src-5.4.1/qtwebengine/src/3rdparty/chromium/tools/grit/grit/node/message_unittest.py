#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for grit.node.message'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest
import StringIO

from grit import tclib
from grit import util
from grit.node import message

class MessageUnittest(unittest.TestCase):
  def testMessage(self):
    root = util.ParseGrdForUnittest('''
        <messages>
        <message name="IDS_GREETING"
                 desc="Printed to greet the currently logged in user">
        Hello <ph name="USERNAME">%s<ex>Joi</ex></ph>, how are you doing today?
        </message>
        </messages>''')
    msg, = root.GetChildrenOfType(message.MessageNode)
    cliques = msg.GetCliques()
    content = cliques[0].GetMessage().GetPresentableContent()
    self.failUnless(content == 'Hello USERNAME, how are you doing today?')

  def testMessageWithWhitespace(self):
    root = util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_BLA" desc="">
        '''  Hello there <ph name="USERNAME">%s</ph>   '''
        </message>
        </messages>""")
    msg, = root.GetChildrenOfType(message.MessageNode)
    content = msg.GetCliques()[0].GetMessage().GetPresentableContent()
    self.failUnless(content == 'Hello there USERNAME')
    self.failUnless(msg.ws_at_start == '  ')
    self.failUnless(msg.ws_at_end == '   ')

  def testConstruct(self):
    msg = tclib.Message(text="   Hello USERNAME, how are you?   BINGO\t\t",
                        placeholders=[tclib.Placeholder('USERNAME', '%s', 'Joi'),
                                      tclib.Placeholder('BINGO', '%d', '11')])
    msg_node = message.MessageNode.Construct(None, msg, 'BINGOBONGO')
    self.failUnless(msg_node.children[0].name == 'ph')
    self.failUnless(msg_node.children[0].children[0].name == 'ex')
    self.failUnless(msg_node.children[0].children[0].GetCdata() == 'Joi')
    self.failUnless(msg_node.children[1].children[0].GetCdata() == '11')
    self.failUnless(msg_node.ws_at_start == '   ')
    self.failUnless(msg_node.ws_at_end == '\t\t')

  def testUnicodeConstruct(self):
    text = u'Howdie \u00fe'
    msg = tclib.Message(text=text)
    msg_node = message.MessageNode.Construct(None, msg, 'BINGOBONGO')
    msg_from_node = msg_node.GetCdata()
    self.failUnless(msg_from_node == text)

  def testFormatterData(self):
    root = util.ParseGrdForUnittest("""\
        <messages>
        <message name="IDS_BLA" desc="" formatter_data="  foo=123 bar  qux=low">
          Text
        </message>
        </messages>""")
    msg, = root.GetChildrenOfType(message.MessageNode)
    expected_formatter_data = {
        'foo': '123',
        'bar': '',
        'qux': 'low'}

    # Can't use assertDictEqual, not available in Python 2.6, so do it
    # by hand.
    self.failUnlessEqual(len(expected_formatter_data),
                         len(msg.formatter_data))
    for key in expected_formatter_data:
      self.failUnlessEqual(expected_formatter_data[key],
                           msg.formatter_data[key])


if __name__ == '__main__':
  unittest.main()

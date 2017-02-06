#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import unittest

from trend_graph import TrendGraph


class TestTrendGraph(unittest.TestCase):

  def testUpdate(self):
    test_graph_file_backup_path = os.path.join('test_data', 'graph.html.bak')
    test_graph_file_path = os.path.join('test_data', 'graph.html')
    shutil.copyfile(test_graph_file_backup_path, test_graph_file_path)
    trend_graph = TrendGraph(test_graph_file_path)
    data_map = {}
    data_map['whole'] = (['test1'], 'undefined', 'undefined')
    data_map['skip'] = (['test1', 'test2'], 'undefined', 'undefined')
    data_map['nonskip'] = (['test1', 'test2', 'test3'], 'undefined',
                           'undefined')
    data_map['passingrate'] = (str(4), 'undefined', 'undefined')

    trend_graph.Update('2008,1,1,13,45,00', data_map)
    # Assert the result graph from the file.
    f = open(test_graph_file_path)
    lines2 = f.readlines()
    f.close()
    line_count = 0
    for line in lines2:
      if '2008,0,1,13,45,00' in line:
        line_count += 1
    self.assertEqual(line_count, 2)


if __name__ == '__main__':
  unittest.main()

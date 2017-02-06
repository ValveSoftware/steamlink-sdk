# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for manipulating trend graph with analyzer result history."""

import os

import layouttest_analyzer_helpers

DEFAULT_TREND_GRAPH_PATH = os.path.join('graph', 'graph.html')

# The following is necesasry to decide the point to insert.
LINE_INSERT_POINT_FOR_NUMBERS = r'// insert 1'
LINE_INSERT_POINT_FOR_PASSING_RATE = r'// insert 2'


class TrendGraph(object):
  """A class to manage trend graph which is using Google Visualization APIs.

  Google Visualization API (http://code.google.com/apis/chart/interactive/docs/
  gallery/annotatedtimeline.html) is used to present the historical analyzer
  result. Currently, data is directly written to JavaScript file using file
  in-place replacement for simplicity.

  TODO(imasaki): use GoogleSpreadsheet to store the analyzer result.
  """

  def __init__(self, location=DEFAULT_TREND_GRAPH_PATH):
    """Initialize this object with the location of trend graph."""
    self._location = location

  def Update(self, datetime_string, data_map):
    """Update trend graphs using |datetime_string| and |data_map|.

    There are two kinds of graphs to be updated (one is for numbers and the
    other is for passing rates).

    Args:
        datetime_string: a datetime string delimited by ','
          (e.g., '2008,1,1,13,45,00)'. For example, in the case of the year
          2008, this ranges from '2008,1,1,0,0,00' to '2008,12,31,23,59,99'.
        data_map: a dictionary containing 'whole', 'skip' , 'nonskip',
          'passingrate' as its keys and (number, tile, text) string tuples
          as values for graph annotation.
    """
    joined_str = ''
    # For a date format in GViz, month is shifted (e.g., '2008,2,1' means
    # March 1, 2008). So, the input parameter |datetime_string| (before this
    # conversion) must be shifted in order to show the date properly on GViz.
    # After the below conversion, for example, in the case of the year 2008,
    # |datetime_string| ranges from '2008,0,1,0,0,00' to '2008,11,31,23,59,99'.
    str_list = datetime_string.split(',')
    str_list[1] = str(int(str_list[1])-1)  # Month
    datetime_string = ','.join(str_list)
    for key in ['whole', 'skip', 'nonskip']:
      joined_str += str(len(data_map[key][0])) + ','
      joined_str += ','.join(data_map[key][1:]) + ','
    new_line_for_numbers = '         [new Date(%s),%s],\n' % (datetime_string,
                                                              joined_str)
    new_line_for_numbers += '         %s\n' % (
        LINE_INSERT_POINT_FOR_NUMBERS)
    layouttest_analyzer_helpers.ReplaceLineInFile(
        self._location, LINE_INSERT_POINT_FOR_NUMBERS,
        new_line_for_numbers)

    joined_str = '%s,%s,%s' % (
        str(data_map['passingrate'][0]), data_map['nonskip'][1],
        data_map['nonskip'][2])
    new_line_for_passingrate = '         [new Date(%s),%s],\n' % (
        datetime_string, joined_str)
    new_line_for_passingrate += '         %s\n' % (
        LINE_INSERT_POINT_FOR_PASSING_RATE)
    layouttest_analyzer_helpers.ReplaceLineInFile(
        self._location, LINE_INSERT_POINT_FOR_PASSING_RATE,
        new_line_for_passingrate)

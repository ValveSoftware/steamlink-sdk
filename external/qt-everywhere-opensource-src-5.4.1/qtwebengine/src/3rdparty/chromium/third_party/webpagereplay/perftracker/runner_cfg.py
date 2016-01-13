#!/usr/bin/env python
# Copyright 2010 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#
# Location of Chrome to test.
#
chrome_path = '<path to chrome>'

#
# Location of the SPDY proxy server to use (if testing SPDY)
#
spdy_proxy_server_path = "<path to spdy proxy program>"

#
# Location of the recorded replay data.
#
replay_data_archive = '<path to archive>'

#
# The URL of the PerfTracker web application to post results to
#
appengine_host = 'localhost'
appengine_port = 8080
appengine_user = None
appengine_password = None
appengine_url = 'http://%s:%d/' % (appengine_host, appengine_port)

#
# Script to run between each run.
#
# Use this to grab a fresh copy of the browser, update your sources, or turn
# on/off monitoring systems, etc.
#
inter_run_cleanup_script = None

#
# SSL network options
#
ssl = {
  'certfile': '../cert.pem',
  'keyfile': '../key.pem',
}

#
# Number of times to load each URL for each network.
#
iterations = 15

#
# The configuration to use in the runner
#
networks = []

# Unlimited network speed for both http and spdy.
networks +=  [
  {
    'bandwidth_kbps': {
      'down': 0,
      'up': 0,
    },
    'round_trip_time_ms': 0,
    'packet_loss_percent': 0,
    'protocol': protocols,
  }
  for protocols in ['http', 'spdy']
]

# Vary RTT against "Cable" speed bandwidth.
networks += [
  {
    'bandwidth_kbps': {
      'down': 5000,
      'up': 1000,
    },
    'round_trip_time_ms': round_trip_times,
    'packet_loss_percent': 0,
    'protocol': protocols,
  }
  for protocols in ['http', 'spdy']
  for round_trip_times in [20, 40, 80, 100, 120]
]

# Vary bandwidth against a pretty reasonable RTT.
networks += [
  {
    'bandwidth_kbps': bandwidths,
    'round_trip_time_ms': 40,
    'packet_loss_percent': 0,
    'protocol': protocols,
  }
  for protocols in ['http', 'spdy']
  for bandwidths in  [
    # DSL.
    {
      'down': 2000,
      'up': 400,
    },
    # Cable.
    {
      'down': 5000,
      'up': 1000,
    },
    # 10Mbps.
    {
      'down': 10000,
      'up': 10000,
    },
  ]
]

# Packet loss and a slow network.
networks += [
  {
    'bandwidth_kbps': {
      'down': 2000,
      'up': 400,
    },
    'round_trip_time_ms': 40,
    'packet_loss_percent': 1,
    'protocol': protocols,
  }
  for protocols in ['http', 'spdy']
]

#
# URLs to test.
#
urls = [
  "http://www.google.com/",
  "http://www.google.com/search?q=dogs",
  "http://www.facebook.com/",
  "http://www.youtube.com/",
  "http://www.yahoo.com/",
  "http://www.baidu.com/",
  "http://www.baidu.com/s?wd=obama",
  "http://www.wikipedia.org/",
  "http://en.wikipedia.org/wiki/Lady_gaga",
  "http://googleblog.blogspot.com/",
  "http://www.qq.com/",
  "http://twitter.com/",
  "http://twitter.com/search?q=pizza",
  "http://www.msn.com/",
  "http://www.yahoo.co.jp/",
  "http://www.amazon.com/",
  "http://wordpress.com/",
  "http://www.linkedin.com/",
  "http://www.microsoft.com/en/us/default.aspx",
  "http://www.ebay.com/",
  "http://fashion.ebay.com/womens-clothing",
  "http://www.bing.com/",
  "http://www.bing.com/search?q=cars",
  "http://www.yandex.ru/",
  "http://yandex.ru/yandsearch?text=obama&lr=84",
  "http://www.163.com/",
  "http://www.fc2.com/",
  "http://www.conduit.com/",
  "http://www.mail.ru/",
  "http://www.flickr.com/",
  "http://www.flickr.com/photos/tags/flowers",
  "http://www.nytimes.com/",
  "http://www.cnn.com/",
  "http://www.apple.com/",
  "http://www.bbc.co.uk/",
  "http://www.imdb.com/",
  "http://sfbay.craigslist.org/",
  "http://www.sohu.com/",
  "http://go.com/",
  "http://search.yahoo.com/search?p=disney",
  "http://espn.go.com/",
  "http://ameblo.jp/",
  "http://en.rakuten.co.jp/",
  "http://www.adobe.com/",
  "http://www.cnet.com/"
]

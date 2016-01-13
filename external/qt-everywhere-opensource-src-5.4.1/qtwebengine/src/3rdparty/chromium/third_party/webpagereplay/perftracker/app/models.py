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
# PerfTracker Model
#

from google.appengine.ext import db

# The unique list of versions tested
class Version(db.Model):
    version = db.StringProperty(required=True, indexed=True)

# The unique list of network types
class Network(db.Model):
    # The network_type is just a pretty name for the overall network
    # configuration.  We use it as the index to avoid the combinatorial
    # blowout of indexes if we used the individual network settings.
    network_type = db.StringProperty(required=True, indexed=True)
    download_bandwidth_kbps = db.IntegerProperty(required=True)
    upload_bandwidth_kbps = db.IntegerProperty(required=True)
    round_trip_time_ms = db.IntegerProperty(required=True)
    packet_loss_rate  = db.FloatProperty(required=True)
    protocol = db.StringProperty(required=True,
                                 choices=set([
                                   "http",      # HTTP current (adj cwnd)
                                   "http-base", # HTTP baseline (cwnd default)
                                   "spdy",      # SPDY over SSL
                                   "spdy-nossl" # SPDY over TCP
                                 ]))
    load_type = db.StringProperty(required=True,
                                  choices=set(["cold", "hot", "warm"]),
                                  default="cold")

# The unique list of CPUs
class Cpu(db.Model):
    cpu = db.StringProperty(required=True, indexed=True)

# A TestSet is a set of tests conducted over one or many URLs.
# The test setup and configuration is constant across all URLs in the TestSet.
class TestSet(db.Model):
    user = db.UserProperty()
    date = db.DateTimeProperty(auto_now_add=True)
    notes = db.StringProperty(multiline=True)
    version = db.ReferenceProperty(Version, indexed=True)
    platform = db.StringProperty()
    client_hostname = db.StringProperty()
    cpu = db.ReferenceProperty(Cpu, indexed=True)
    harness_version = db.StringProperty()
    cmdline = db.StringProperty()

    network = db.ReferenceProperty(Network, indexed=True)

    # These fields are summary data for the TestSet.
    # When the TestSet is created, these fields are blank.
    # When the TestSet finishes running, these fields will be updated.
    iterations = db.IntegerProperty()
    url_count = db.IntegerProperty()
    start_load_time = db.IntegerProperty()
    dns_time = db.IntegerProperty()
    connect_time = db.IntegerProperty()
    first_byte_time = db.IntegerProperty()
    last_byte_time = db.IntegerProperty()
    paint_time = db.IntegerProperty()
    doc_load_time = db.IntegerProperty()
    dcl_time = db.IntegerProperty()
    total_time = db.IntegerProperty()
    num_requests = db.IntegerProperty()
    num_connects = db.IntegerProperty()
    num_sessions = db.IntegerProperty()
    read_bytes_kb = db.IntegerProperty()
    write_bytes_kb = db.IntegerProperty()

# A TestResult is an individual test result for an individual URL.
class TestResult(db.Model):
    set = db.ReferenceProperty(TestSet, required=True, indexed=True, collection_name="results")
    date = db.DateTimeProperty(auto_now_add=True)
    url = db.StringProperty(required=True, indexed=True)
    using_spdy = db.BooleanProperty()
    start_load_time = db.IntegerProperty()
    dns_time = db.IntegerProperty()
    connect_time = db.IntegerProperty()
    first_byte_time = db.IntegerProperty()
    last_byte_time = db.IntegerProperty()
    paint_time = db.IntegerProperty()
    doc_load_time = db.IntegerProperty()
    dcl_time = db.IntegerProperty()
    total_time = db.IntegerProperty()
    num_requests = db.IntegerProperty()
    num_connects = db.IntegerProperty()
    num_sessions = db.IntegerProperty()
    read_bytes_kb = db.IntegerProperty()
    write_bytes_kb = db.IntegerProperty()

# A TestSummary is the aggregate result for a set of TestResults for
# a single URL.
class TestSummary(db.Model):
    set = db.ReferenceProperty(TestSet, required=True, indexed=True, collection_name="summaries")
    date = db.DateTimeProperty(auto_now_add=True)
    url = db.StringProperty(required=True, indexed=True)
    iterations = db.IntegerProperty()
    start_load_time = db.IntegerProperty()
    dns_time = db.IntegerProperty()
    connect_time = db.IntegerProperty()
    first_byte_time = db.IntegerProperty()
    last_byte_time = db.IntegerProperty()
    paint_time = db.IntegerProperty()
    doc_load_time = db.IntegerProperty()
    dcl_time = db.IntegerProperty()
    total_time = db.IntegerProperty()
    total_time_stddev = db.FloatProperty()
    num_requests = db.IntegerProperty()
    num_connects = db.IntegerProperty()
    num_sessions = db.IntegerProperty()
    read_bytes_kb = db.IntegerProperty()
    write_bytes_kb = db.IntegerProperty()

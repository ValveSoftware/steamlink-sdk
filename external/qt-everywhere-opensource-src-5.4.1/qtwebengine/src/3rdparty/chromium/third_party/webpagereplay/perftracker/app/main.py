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

import cgi
import json
import logging
import models
import os

from google.appengine.api import memcache
from google.appengine.api import users
from google.appengine.ext import webapp
from google.appengine.ext.webapp import template
from google.appengine.ext.webapp.util import run_wsgi_app
from google.appengine.ext import db

def ApplyStatisticsData(request, obj):
    """Applies statistics uploaded via the request to the object."""
    obj.start_load_time = max(int(float(request.get('start_load_time', 0))), 0)
    obj.dns_time = max(int(float(request.get('dns_time', 0))), 0)
    obj.connect_time = max(int(float(request.get('connect_time', 0))), 0)
    obj.first_byte_time = max(int(float(request.get('first_byte_time', 0))), 0)
    obj.last_byte_time = max(int(float(request.get('last_byte_time', 0))), 0)
    obj.paint_time = max(int(float(request.get('paint_time', 0))), 0)
    obj.doc_load_time = max(int(float(request.get('doc_load_time', 0))), 0)
    obj.dcl_time = max(int(float(request.get('dcl_time', 0))), 0)
    obj.total_time = max(int(float(request.get('total_time', 0))), 0)
    obj.num_requests = max(int(float(request.get('num_requests', 0))), 0)
    obj.num_connects = max(int(float(request.get('num_connects', 0))), 0)
    obj.num_sessions = max(int(float(request.get('num_sessions', 0))), 0)
    obj.read_bytes_kb = max(int(float(request.get('read_bytes_kb', 0))), 0)
    obj.write_bytes_kb = max(int(float(request.get('write_bytes_kb', 0))), 0)

def BandwidthPrettyString(bandwidth_kbps):
    if bandwidth_kbps >= 1000:
        bandwidth_mbps = bandwidth_kbps / 1000.0
        return str(bandwidth_mbps) + "Mbps"
    return str(bandwidth_kbps) + "Kbps"

def NetworkPrettyString(download_bandwidth_kbps,
                        upload_bandwidth_kbps,
                        round_trip_time_ms,
                        packet_loss_rate,
                        protocol_str,
                        load_type):
    network = protocol_str + "/"
    network += BandwidthPrettyString(download_bandwidth_kbps)
    network += "/"
    network += BandwidthPrettyString(upload_bandwidth_kbps)
    network += "/"
    network += str(round_trip_time_ms)
    network += "ms/"
    network += str(packet_loss_rate)
    network += "%/"
    network += load_type
    return network

class BaseRequestHandler(webapp.RequestHandler):
    def send_error(self, format, *args):
        """Send a fatal request error to the error log and response output."""
        logging.error(format, *args)
        self.response.out.write(format % args)

    def send_json_error(self, format, *args):
        """Send a fatal request error to the error log and json output."""
        logging.error(format, *args)
        json_error = {}
        json_error['error'] = format % args
        self.response.out.write(json.encode(json_error))


class JSONDataPage(BaseRequestHandler):
    """Do a search for TestSets."""
    def do_set_search(self):
        memcache_key = "set_search." + self.request.query
        cached_response = memcache.get(memcache_key)
        if cached_response is not None:
            self.response.out.write(cached_response)
            return

        query = models.TestSet.all()
        query.order("-date")

        # Apply filters.
        networks = self.request.get("networks_filter")
        if networks:
            query.filter("network IN ",
                [db.Key.from_path('Network', int(k)) for k in set(networks.split(","))])
        versions = self.request.get("version_filter")
        if versions:
            query.filter("version IN ",
                [db.Key.from_path('Version', int(k)) for k in set(versions.split(","))])
        cpus = self.request.get("cpus_filter")
        if cpus:
            query.filter("cpu IN ",
                [db.Key.from_path('Cpu', int(k)) for k in set(cpus.split(","))])
        if self.request.get("set_id"):
            test_set = models.TestSet.get_by_id(int(self.request.get("set_id")))
            results = test_set.summaries

        results = query.fetch(500)
        response = json.encode(results)
        memcache.add(memcache_key, response, 30)   # Cache for 30secs
        self.response.out.write(response)

    def do_set(self):
        """Lookup a specific TestSet."""
        set_id = self.request.get("id")
        if not set_id:
            self.send_json_error("Bad request, no id param")
            return
        test_set = models.TestSet.get_by_id(int(set_id))
        if not test_set:
            self.send_json_error("Could not find id: ", id)
            return

        # We do manual coalescing of multiple data structures
        # into a single json blob.
        json_output = {}
        json_output['obj'] = test_set
        json_output['version'] = test_set.version
        json_output['cpu'] = test_set.cpu
        json_output['network'] = test_set.network
        summaries_query = test_set.summaries
        summaries_query.order("url")
        json_output['summaries'] = [s for s in summaries_query]
        # There is no data; go ahead and pull the individual runs.
        if len(json_output['summaries']) == 0:
            query = models.TestResult.all()
            query.filter("set = ", db.Key.from_path('TestSet', int(set_id)))
            query.order('url')
            json_output['summaries'] = [r for r in query]
        self.response.out.write(json.encode(json_output))

    def do_summary(self):
        """ Lookup a specific TestSummary"""
        set_id = self.request.get("id")
        if not set_id:
            self.send_json_error("Bad request, no id param")
            return

        memcache_key = "summary." + set_id
        cached_response = memcache.get(memcache_key)
        if cached_response is not None:
            self.response.out.write(cached_response)
            return

        test_summary = models.TestSummary.get_by_id(int(set_id))
        if not test_summary:
            self.send_json_error("Could not find id: ", id)
            return

        json_output = {}
        json_output['obj'] = test_summary
        test_set = models.TestSet.get_by_id(test_summary.set.key().id())
        test_results = test_set.results
        test_results.filter("url =", test_summary.url)
        json_output['results'] = [r for r in test_results]

        response = json.encode(json_output)
        memcache.add(memcache_key, response, 60)   # Cache for 1min
        self.response.out.write(response)

    def do_filters(self):
        """Lookup the distinct values in the TestSet data, for use in filtering.
        """
        cached_response = memcache.get("filters")
        if cached_response is not None:
            self.response.out.write(cached_response)
            return

        versions = set()
        cpus = set()
        networks = set()

        query = models.Version.all()
        for item in query:
            versions.add(( item.version, str(item.key().id()) ))
        query = models.Cpu.all()
        for item in query:
            cpus.add(( item.cpu, str(item.key().id()) ))
        query = models.Network.all()
        for item in query:
            networks.add(( item.network_type, str(item.key().id()) ))

        filters = {}
        filters["versions"] = sorted(versions)
        filters["cpus"] = sorted(cpus)
        filters["networks"] = sorted(networks)
        response = json.encode(filters)
        memcache.add("filters", response, 60 * 10)  # Cache for 10 mins
        self.response.out.write(response)

    def do_latestresults(self):
        """Get the last 25 results posted to the server."""
        query = models.TestResult.all()
        query.order("-date")
        results = query.fetch(25)
        self.response.out.write(json.encode(results))

    def get(self):
        # TODO(mbelshe): the dev server doesn't properly handle logins?
        #if not user:
        #    self.redirect(users.create_login_url(self.request.uri))
        #    return

        resource_type = self.request.get("type")
        if not resource_type:
            self.send_json_error("Could not find type: ", type)
            return

        # Do a query for the appropriate resource type.
        if resource_type == "summary":
            self.do_summary()
            return
        elif resource_type == "result":
            # TODO(mbelshe): implement me!
            return
        elif resource_type == "set":
            self.do_set()
            return
        elif resource_type == "set_search":
            self.do_set_search()
            return
        elif resource_type == "filters":
            self.do_filters()
            return
        elif resource_type == "latestresults":
            self.do_latestresults()
            return

        self.response.out.write(json.encode({}))


class UploadTestSet(BaseRequestHandler):
    """ Get a version from the datastore.  If it doesn't exist, create it """
    def GetOrCreateVersion(self, version_str):
        query = models.Version.all()
        query.filter("version = ", version_str)
        versions = query.fetch(1)
        if versions:
            return versions[0]
        version = models.Version(version = version_str)
        version.put()
        return version

    """ Get a cpu from the datastore.  If it doesn't exist, create it """
    def GetOrCreateCpu(self, cpu_str):
        query = models.Cpu.all()
        query.filter("cpu = ", cpu_str)
        cpus = query.fetch(1)
        if cpus:
            return cpus[0]
        cpu = models.Cpu(cpu = cpu_str)
        cpu.put()
        return cpu

    """ Get a network from the datastore.  If it doesn't exist, create it """
    def GetOrCreateNetwork(self,
                           download_bandwidth_kbps,
                           upload_bandwidth_kbps,
                           round_trip_time_ms,
                           packet_loss_rate,
                           protocol_str,
                           load_type):
        network_type = NetworkPrettyString(download_bandwidth_kbps,
                                           upload_bandwidth_kbps,
                                           round_trip_time_ms,
                                           packet_loss_rate,
                                           protocol_str,
                                           load_type)
        query = models.Network.all()
        query.filter("network_type = ", network_type)
        networks = query.fetch(1)
        if networks:
            return networks[0]
        network = models.Network(
            network_type = network_type,
            download_bandwidth_kbps = download_bandwidth_kbps,
            upload_bandwidth_kbps = upload_bandwidth_kbps,
            round_trip_time_ms = round_trip_time_ms,
            packet_loss_rate = packet_loss_rate,
            protocol = protocol_str,
            load_type = load_type)
        network.put()
        return network

    """Create an entry in the store for a new test."""
    def post(self):
        user = users.get_current_user()
        # TODO(mbelshe): the dev server doesn't properly handle logins?
        #if not user:
        #    self.redirect(users.create_login_url(self.request.uri))
        #    return

        cmd = self.request.get("cmd")
        if not cmd:
            self.send_error("Bad request, no cmd param")
            return

        if cmd == "create":
            version_str  = self.request.get('version')
            if not version_str:
                raise Exception("missing version")
            download_bandwidth_kbps = \
                int(float(self.request.get('download_bandwidth_kbps')))
            upload_bandwidth_kbps = \
                int(float(self.request.get('upload_bandwidth_kbps')))
            round_trip_time_ms = int(float(self.request.get('round_trip_time_ms')))
            packet_loss_rate  = float(self.request.get('packet_loss_rate'))
            protocol_str = self.request.get('protocol')
            load_type = self.request.get('load_type')
            version = self.GetOrCreateVersion(version_str)
            if not version:
                raise Exception("could not create version")

            cpu = self.GetOrCreateCpu(self.request.get('cpu'))
            if not cpu:
                raise Exception("could not create cpu")

            network = self.GetOrCreateNetwork(download_bandwidth_kbps,
                                              upload_bandwidth_kbps,
                                              round_trip_time_ms,
                                              packet_loss_rate,
                                              protocol_str,
                                              load_type)
            if not network:
                raise Exception("could not create network")

            test_set = models.TestSet(user=user)
            test_set.version = version
            test_set.cpu  = cpu
            test_set.network = network
            test_set.notes = self.request.get('notes')
            test_set.cmdline  = self.request.get('cmdline')
            test_set.platform  = self.request.get('platform')
            test_set.client_hostname  = self.request.get('client_hostname')
            test_set.harness_version  = self.request.get('harness_version', '')
            key = test_set.put()
            self.response.out.write(key.id())

        elif cmd == "update":
            set_id = self.request.get("set_id")
            if not set_id:
                self.send_error("Bad request, no set_id param")
                return
            test_set = models.TestSet.get_by_id(int(set_id))
            if not test_set:
                self.send_error("Could not find set_id: %s", set_id)
                return
            ApplyStatisticsData(self.request, test_set)
            test_set.iterations = int(self.request.get('iterations'))
            test_set.url_count = int(self.request.get('url_count'))
            key = test_set.put()
            self.response.out.write(key.id())
        else:
            self.send_error("Bad request, unknown cmd: %s", cmd)

class UploadTestResult(BaseRequestHandler):
    """Create an entry in the store for a new test run."""
    def post(self):
        user = users.get_current_user()
        # TODO(mbelshe): the dev server doesn't properly handle logins?
        #if not user:
        #    self.redirect(users.create_login_url(self.request.uri))
        #    return

        set_id = self.request.get('set_id')
        if not set_id:
            self.send_error("Bad request, no set_id param")
            return
        try:
            set_id = int(set_id)
        except:
            self.send_error("Bad request, bad set_id param: %s", set_id)
            return
        test_set = models.TestSet.get_by_id(set_id)
        if not test_set:
            self.send_error("Could not find set_id: %s", set_id)
            return
        my_url = self.request.get('url')

        test_result = models.TestResult(set=test_set, url=my_url)
        test_result.using_spdy = bool(self.request.get('using_spdy') == 'true')
        ApplyStatisticsData(self.request, test_result)
        key = test_result.put()
        self.response.out.write(key.id())

class UploadTestSummary(BaseRequestHandler):
    def post(self):
        user = users.get_current_user()
        # TODO(mbelshe): the dev server doesn't properly handle logins?
        #if not user:
        #    self.redirect(users.create_login_url(self.request.uri))
        #    return

        set_id = self.request.get('set_id')
        if not set_id:
            self.send_error('Bad request, no set_id param')
            return
        try:
            set_id = int(set_id)
        except:
            self.send_error('Bad request, bad set_id param: %s', set_id)
            return
        test_set = models.TestSet.get_by_id(set_id)
        if not test_set:
            self.send_error('Could not find set_id: %s', set_id)
            return
        my_url = self.request.get('url')

        test_summary = models.TestSummary(set=test_set, url=my_url)
        ApplyStatisticsData(self.request, test_summary)
        test_summary.iterations = int(self.request.get('iterations'))
        test_summary.total_time_stddev = float(self.request.get('total_time_stddev'))
        key = test_summary.put()
        self.response.out.write(key.id())

class BulkDelete(BaseRequestHandler):
    def get(self):
        query = models.TestResult.all(keys_only=True)
        results = query.fetch(500)
        db.delete(results)
        self.response.out.write("500 entries deleted")


application = webapp.WSGIApplication(
                                     [
                                      ('/set', UploadTestSet),
                                      ('/result', UploadTestResult),
                                      ('/summary', UploadTestSummary),
                                      ('/json', JSONDataPage),
                                      ('/bulkdel', BulkDelete),
                                     ],
                                     debug=True)

def main():
    run_wsgi_app(application)

if __name__ == "__main__":
    main()

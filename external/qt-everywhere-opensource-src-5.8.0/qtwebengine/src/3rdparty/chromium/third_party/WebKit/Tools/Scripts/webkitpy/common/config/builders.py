# Copyright (c) 2016, Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

BUILDERS = {
    "WebKit Win7": {
        "port_name": "win-win7",
        "specifiers": ['Win7', 'Release']
    },
    "WebKit Win7 (dbg)": {
        "port_name": "win-win7",
        "specifiers": ['Win7', 'Debug']
    },
    "WebKit Win10": {
        "port_name": "win-win10",
        "specifiers": ['Win10', 'Release']
    },
    "WebKit Linux": {  # FIXME: Rename this. http://crbug.com/613305.
        "port_name": "linux-precise",
        "specifiers": ['Precise', 'Release']
    },
    "WebKit Linux Trusty": {
        "port_name": "linux-trusty",
        "specifiers": ['Trusty', 'Release']
    },
    "WebKit Linux (dbg)": {
        "port_name": "linux-precise",
        "specifiers": ['Precise', 'Debug']
    },
    "WebKit Mac10.9": {
        "port_name": "mac-mac10.9",
        "specifiers": ['Mac10.9', 'Release']
    },
    "WebKit Mac10.10": {
        "port_name": "mac-mac10.10",
        "specifiers": ['Mac10.10', 'Release']
    },
    "WebKit Mac10.11": {
        "port_name": "mac-mac10.11",
        "specifiers": ['10.11', 'Release']
    },
    "WebKit Mac10.11 (dbg)": {
        "port_name": "mac-mac10.11",
        "specifiers": ['10.11', 'Debug']
    },
    "WebKit Mac10.11 (retina)": {
        "port_name": "mac-retina",
        "specifiers": ['Retina', 'Release']
    },
    "WebKit Android (Nexus4)": {
        "port_name": "android",
        "specifiers": ['Android', 'Release']
    },
    # TODO(qyearsley): Update this set of builders when new builders
    # are set up for layout test try jobs. http://crbug.com/474273.
    "linux_chromium_rel_ng": {
        "port_name": "linux-precise",
        "specifiers": ['Precise', 'Release'],
        "is_try_bot": True,
    },
    "mac_chromium_rel_ng": {
        "port_name": "mac-mac10.9",
        "specifiers": ['Mac10.9', 'Release'],
        "is_try_bot": True,
    },
    "win_chromium_rel_ng": {
        "port_name": "win-win7",
        "specifiers": ['Win7', 'Release'],
        "is_try_bot": True,
    },
}

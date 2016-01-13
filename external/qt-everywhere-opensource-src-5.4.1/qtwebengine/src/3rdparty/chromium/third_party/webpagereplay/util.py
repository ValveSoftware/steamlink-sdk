#!/usr/bin/env python
# Copyright 2012 Google Inc. All Rights Reserved.
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


"""Miscellaneous utility functions."""


try:
  # pkg_resources (part of setuptools) is needed when WPR is
  # distributed as a package. (Resources may need to be extracted from
  # the package.)

  import pkg_resources

  def resource_exists(resource_name):
    return pkg_resources.resource_exists(__name__, resource_name)

  def resource_string(resource_name):
    return pkg_resources.resource_string(__name__, resource_name)

except ImportError:
  # Import of pkg_resources failed, so fall back to getting resources
  # from the file system.

  import os

  def _resource_path(resource_name):
    _replay_dir = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(_replay_dir, resource_name)

  def resource_exists(resource_name):
    return os.path.exists(_resource_path(resource_name))

  def resource_string(resource_name):
    return open(_resource_path(resource_name)).read()

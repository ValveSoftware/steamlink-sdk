#!/usr/bin/python
#
# python-v4l2capture
#
# 2009, 2010, 2011 Fredrik Portstrom
#
# I, the copyright holder of this file, hereby release it into the
# public domain. This applies worldwide. In case this is not legally
# possible: I grant anyone the right to use this work for any
# purpose, without any conditions, unless such conditions are
# required by law.

from distutils.core import Extension, setup
setup(
    name = "v4l2capture",
    version = "1.4",
    author = "Fredrik Portstrom",
    author_email = "fredrik@jemla.se",
    url = "http://fredrik.jemla.eu/v4l2capture",
    description = "Capture video with video4linux2",
    long_description = "python-v4l2capture is a slim and easy to use Python "
    "extension for capturing video with video4linux2.",
    license = "Public Domain",
    classifiers = [
        "License :: Public Domain",
        "Programming Language :: C"],
    ext_modules = [
        Extension("v4l2capture", ["v4l2capture.c"])])

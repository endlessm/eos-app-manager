#!/usr/bin/env python

import sys
import argparse
import os.path
from subprocess import Popen, PIPE
from collections import namedtuple, OrderedDict

VERSION="0.1"

PACKAGE_METADATA = OrderedDict([
    ('appid', 'Package'),
    ('version', 'Version'),
    ('homepage', 'Homepage'),
    ('architecture', 'Architecture'),
    ('description', 'Description'),
    ])


def system_exec(command, directory=None, show_output=True, ignore_error=False):
    if not directory:
        directory = os.getcwd()

    try:
        process = Popen(command, stdout=PIPE, stderr=PIPE, shell=True, cwd=directory)
        (output, error) = process.communicate()
        output = output.strip()
        error = error.strip()

        if show_output and len(output) > 0:
            print output
            sys.stdout.flush()

        if process.returncode != 0 and not ignore_error:
            raise Exception(error)

        ReturnInfo = namedtuple('ReturnInfo', 'return_code output error')
        return ReturnInfo(process.returncode, output, error)

    except Exception as err:
        print >>sys.stderr, Color.RED + "Could not execute", command
        print >>sys.stderr, err, Color.END
        print >>sys.stderr, "Terminating early"
        exit(1)

# Allows for invoking attributes as methods/functions
class AttributeDict(dict):
    def __getattr__(self, attr):
        return self[attr]
    def __setattr__(self, attr, value):
        self[attr] = value

class Color:
    GREEN = "\033[1;32m"
    BLUE = "\033[1;34m"
    YELLOW = "\033[1;33m"
    RED = "\033[1;31m"
    END = "\033[0m"

class BundleConverter(object):
    def __init__(self, args):
        self.args = args

    def _extract_bundle_data(self, deb_package):
        field_values = {}

        for info_key in PACKAGE_METADATA.keys():
            field_name = PACKAGE_METADATA[info_key]

            field_values[field_name] = system_exec("dpkg-deb -f %s %s" % (deb_package, field_name), show_output=False).output

            print field_name, "\t", field_values[field_name]

        return field_values

    def convert(self):
        if not os.path.isfile(self.args.deb_package):
            print >>sys.stderr, 'File not found:',  self.args.deb_package
            exit(1)

        bundle_info = self._extract_bundle_data(self.args.deb_package)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Converts Debian packages into application bundles')

    parser.add_argument('deb_package', \
            help='Debian package to be converted')

    parser.add_argument('--debug', \
            help='Enable debugging output', \
            action='store_true')

    parser.add_argument('--version', \
            action='version', \
            version='%(prog)s v' + VERSION)

    args = AttributeDict(vars(parser.parse_args()))
    BundleConverter(args).convert()

#!/usr/bin/env python

import sys
import argparse
import os.path
from shutil import rmtree
from subprocess import Popen, PIPE
from collections import namedtuple, OrderedDict
from tempfile import mkdtemp

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
        field_values = AttributeDict()

        print "-" * 40
        for info_key in PACKAGE_METADATA.keys():
            field_name = PACKAGE_METADATA[info_key]

            field_values[info_key] = system_exec("dpkg-deb -f %s %s" % (deb_package, field_name), show_output=False).output

            print info_key, "\t", field_values[info_key]
        print "-" * 40

        return field_values

    def convert(self):
        if not os.path.isfile(self.args.deb_package):
            print >>sys.stderr, 'File not found:',  self.args.deb_package
            exit(1)

        # Get bundle metadata
        bundle_info = self._extract_bundle_data(self.args.deb_package)

        # Fix eos-* named packages
        if bundle_info['appid'].startswith('eos-'):
            bundle_info['appid'] = "com.endlessm.%s" % bundle_info['appid'].replace('eos-','', 1)
            print "WARN: New package name is", bundle_info['appid']

        # Cretate a temp dir
        working_dir = mkdtemp(prefix='deb2bundle_')

        # Create package-named dir
        extraction_dir = os.path.join(working_dir, bundle_info.appid)
        os.mkdir(extraction_dir)
        print "Using", extraction_dir, "as staging area"

        print "Extracting..."
        system_exec("dpkg --extract %s %s" % (self.args.deb_package, extraction_dir))


        # Delete our temp dir
        print "Cleaning up"
        rmtree(working_dir)

        print "Done"

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

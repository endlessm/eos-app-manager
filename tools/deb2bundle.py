#!/usr/bin/env python

import sys
import argparse
import tarfile
from os import path, mkdir, listdir, getcwd, rmdir
from shutil import rmtree, move
from subprocess import Popen, PIPE
from collections import namedtuple, OrderedDict
from tempfile import mkdtemp

VERSION="0.1"

BUNDLE_METADATA_HEADER = "[Bundle]"
BUNDLE_METADATA = OrderedDict([
    ('appid', 'Package'),
    ('version', 'Version'),
    ('homepage', 'Homepage'),
    ('architecture', 'Architecture'),
    ('description', 'Description'),
    ])


def system_exec(command, directory=None, show_output=True, ignore_error=False):
    if not directory:
        directory = getcwd()

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

def get_color_str(text, color):
    return color + str(text) + Color.END

# Converter for Debian packages to bundles
class BundleConverter(object):
    def __init__(self, args):
        self.args = args

    def _extract_bundle_data(self, deb_package):
        field_values = AttributeDict()

        print get_color_str("-" * 40, Color.GREEN)
        for info_key in BUNDLE_METADATA.keys():
            field_name = BUNDLE_METADATA[info_key]

            field_values[info_key] = system_exec("dpkg-deb -f %s %s" % (deb_package, field_name), show_output=False).output

            print info_key, "\t", field_values[info_key]
        print get_color_str("-" * 40, Color.GREEN)

        return field_values

    def convert(self):
        if not path.isfile(self.args.deb_package):
            print >>sys.stderr, 'File not found:',  self.args.deb_package
            exit(1)

        # Get bundle metadata
        bundle_info = self._extract_bundle_data(self.args.deb_package)

        # Fix eos-* named packages
        if bundle_info['appid'].startswith('eos-'):
            bundle_info['appid'] = "com.endlessm.%s" % bundle_info['appid'].replace('eos-','', 1)
            print get_color_str("WARN: New package name is %s" % bundle_info.appid, Color.YELLOW)

        # Cretate a temp dir
        working_dir = mkdtemp(prefix='deb2bundle_')

        # Create package-named dir
        extraction_dir = path.join(working_dir, bundle_info.appid)
        mkdir(extraction_dir)
        print "Using", get_color_str(extraction_dir, Color.GREEN), "as staging area"

        print "Extracting..."
        # TODO: Convert this to native python
        system_exec("dpkg --extract %s %s" % (self.args.deb_package, extraction_dir))

        print "Moving /usr/* one level up"
        user_dir = path.join(extraction_dir, 'usr')
        for item in listdir(user_dir):
            move(path.join(user_dir, item), extraction_dir)
        rmdir(user_dir)

        # Use full package name if regular run and simple .tgz if debugging
        if not self.args.debug:
            target_path = "%s_%s_%s.bundle" % (bundle_info.appid, bundle_info.version, bundle_info.architecture)
        else:
            target_path = "%s.tgz" % (bundle_info.appid)

        print "Compressing data to %s" % get_color_str(target_path, Color.GREEN)

        print "  - Writing metadata..."
        metadata_path = path.join(extraction_dir, '.info')
        with open(metadata_path, 'w') as metadata:
            metadata.write(BUNDLE_METADATA_HEADER)

            for item in bundle_info.keys():
                metadata.write("%s=%s\n" % (item, bundle_info[item]))

        print "  - Writing files..."
        with tarfile.open(target_path, 'w:gz', format=tarfile.GNU_FORMAT) as bundle:
            bundle.add(extraction_dir, arcname=bundle_info.appid)

        print "Cleaning up..."
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

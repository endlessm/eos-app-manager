#!/usr/bin/env python3

import sys
import argparse
import tarfile
import re
import json
from os import path
from urllib import request
from subprocess import Popen, PIPE
from collections import namedtuple, OrderedDict

VERSION="0.1"
UPLOAD_PATH="/upload"

BUNDLE_FIELD_CONVERSION = {
    "app_id": "appId",
    "app_name": "appName",
    "version": "codeVersion",
    "min_os_Version": "minOsVersion",
    "from_version": "fromVersion",
    "personality": "personality",
    }

def system_exec(command, directory=None, show_output=True, ignore_error=False):
    if not directory:
        directory = getcwd()

    try:
        process = Popen(command, stdout=PIPE, stderr=PIPE, shell=True, cwd=directory)
        (output, error) = process.communicate()
        output = output.strip()
        error = error.strip()

        if show_output and len(output) > 0:
            print(output)
            sys.stdout.flush()

        if process.returncode != 0 and not ignore_error:
            raise Exception(error)

        ReturnInfo = namedtuple('ReturnInfo', 'return_code output error')
        return ReturnInfo(process.returncode, output, error)

    except Exception as err:
        sys.stderr.write(Color.RED + "Could not execute " + command + "\n")
        sys.stderr.write(str(err) + Color.END + "\n")
        sys.stderr.write("Terminating early" + "\n")
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

# Publisher for AppBundles
class BundlePublisher(object):
    def __init__(self, args):
        self.args = args

    def _extract_bundle_data(self, bundle):
        field_values = AttributeDict()

        def info_file_filter(members):
            for tarinfo in members:
                if path.basename(tarinfo.name) == '.info':
                    return tarinfo

            sys.stderr.write(get_color_str("WARN: Info not found in " + bundle + ". Skipping bundle.\n", Color.YELLOW))
            return

        info = None
        with tarfile.open(bundle, 'r:gz', format=tarfile.GNU_FORMAT) as bundle_tar:
            info_file = info_file_filter(bundle_tar)
            if not info_file:
                return

            info = bundle_tar.extractfile(member=info_file).read().decode('UTF-8')

        info = info.strip().split('\n')

        print(get_color_str("-" * 40, Color.GREEN))
        for info_item in info:
            # Ignore long-description field
            if info_item.startswith(' '):
                continue

            # Strip header(s)
            if re.match('\[.*\]', info_item):
                continue

            key, value = info_item.split('=')
            field_values[key] = value
            print("  %s\t%s" % (key, field_values[key]))

        print(get_color_str("-" * 40, Color.GREEN))

        return field_values

    def _publish_bundle(self, bundle, metadata, username, password):
        authhandler = request.HTTPDigestAuthHandler()
        authhandler.add_password("AppUpdates", self.args.endpoint, username, password)
        opener = request.build_opener(authhandler)
        request.install_opener(opener)

        csrf_request = request.Request(self.args.endpoint)
        page_content = request.urlopen(csrf_request).read().decode('UTF-8')
        csrf_token = re.search("\"authenticity_token\".* value=\"(.*)\"", page_content).group(1)

        print("Got authenticity_token: %s" % csrf_token)
        metadata.authenticity_token = csrf_token

        post_request = request.Request(self.args.endpoint, data=json.dumps(metadata).encode('UTF-8'))
        post_request.add_header('Accept', 'application/json')
        post_request.add_header('Content-Type', 'application/json')
        post_request.add_header('X-CSRF-Token', csrf_token)

        page_content = request.urlopen(post_request)
        print(page_content.read().decode('utf-8'))

    def publish(self, bundle):
        print("Using: %s" % get_color_str(bundle, Color.GREEN))

        bundle_info = self._extract_bundle_data(bundle)

        print("Converting bundle fields to server fields...")
        metadata = AttributeDict()
        for bundle_field in BUNDLE_FIELD_CONVERSION:
            if bundle_field in bundle_info:
                metadata[BUNDLE_FIELD_CONVERSION[bundle_field]] = bundle_info[bundle_field]
                print("  %s\t%s" % (BUNDLE_FIELD_CONVERSION[bundle_field], metadata[BUNDLE_FIELD_CONVERSION[bundle_field]]))

        self._publish_bundle(bundle, metadata, self.args.user, self.args.password)

    def publish_all(self):
        for bundle in self.args.app_bundle:
            if not path.isfile(bundle):
                sys.stderr.write("File not found: " + bundle + "\n")
                exit(1)

            self.publish(bundle)

        print("Done")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Publishes bundles to the server')

    parser.add_argument('server', \
            help='Debian package to be converted')

    parser.add_argument('password', \
            help='Password for uploading files')

    parser.add_argument('-u', '--user', \
            nargs='?',
            default='uploader',
            help='Username for uploading files')

    parser.add_argument('app_bundle', \
            nargs='+', \
            help='Debian package to be converted')

    parser.add_argument('--debug', \
            help='Enable debugging output', \
            action='store_true')

    parser.add_argument('--version', \
            action='version', \
            version='%(prog)s v' + VERSION)

    args = AttributeDict(vars(parser.parse_args()))

    args.endpoint = "http://%s%s" % (args.server, UPLOAD_PATH)
    print("Using endpoint: %s" % get_color_str(args.endpoint, Color.GREEN))

    BundlePublisher(args).publish_all()

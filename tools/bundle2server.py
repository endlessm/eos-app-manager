#!/usr/bin/env python3

import sys
import argparse
import tarfile
import re
import requests     #This requires python3-requests package
from html.parser import HTMLParser

from os import path
from requests.auth import HTTPDigestAuth
from urllib import request
from subprocess import Popen, PIPE
from collections import namedtuple, OrderedDict


VERSION="0.1"
UPLOAD_PATH="/upload"
UPLOAD_FORM_PATH="%s/new" % UPLOAD_PATH

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

# Used to parse the html response and retrieve the CSRF token
# There are other ways to parse the HTML but this one is built-in
class CsrfParser(HTMLParser):
    KEY = 0
    VALUE = 1

    def __init__(self, content):
        HTMLParser.__init__(self)
        self._csrf_token = None

        self.feed(content)

    def handle_starttag(self, tag, attrs):
        if tag == 'meta':
            for attr in attrs:
                attr_name, attr_value = attr
                if attr[self.KEY] == 'name' and attr[self.VALUE] == 'csrf-token':
                    self._csrf_token = [attr[self.VALUE] for attr in attrs if attr[self.KEY] == 'content'][0]
                    return

    @property
    def csrf_token(self):
        return self._csrf_token

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
        print("Publishing bundle...")

        if self.args.debug:
            # Log requests
            import logging
            import http.client
            http.client.HTTPConnection.debuglevel = 1

            logging.basicConfig()
            logging.getLogger().setLevel(logging.DEBUG)
            requests_log = logging.getLogger("requests.packages.urllib3")
            requests_log.setLevel(logging.DEBUG)
            requests_log.propagate = True


        session = requests.session()
        auth = HTTPDigestAuth(self.args.user, self.args.password)
        response = session.get(self.args.endpoint_form, auth=auth)
        page_content = response.text

        parser = CsrfParser(page_content)
        csrf_token = parser.csrf_token

        if self.args.debug:
            print("Using session cookie: %s" % get_color_str(session.cookies['_eos-app-updates_session'], Color.BLUE))
            print("Got authenticity_token: %s" % get_color_str(csrf_token, Color.BLUE))

        metadata.authenticity_token = csrf_token

        headers = {}
        headers['X-CSRF-Token'] = csrf_token
        headers['Referer'] = self.args.endpoint_form

        data = {}
        data['utf8'] = 'true'

        metadata_hash = {}
        for key in metadata.keys():
            data["app_update_bundle[%s]" % key] = metadata[key]

        files={'app_update_bundle[full_bundle]': open(bundle, 'rb')}

        response = session.post(self.args.endpoint, headers=headers, data=data, files=files, auth=auth)
        page_content = response.text

        #print(page_content)

        if response.status_code != 200:
            raise "Failed to upload bundle %s(%s)" % (metadata.app_name, response.status_code)

        errors = re.findall("\"form-group has-error\".*\<span .*\>(.*)\<\/span\>", page_content)
        if errors:
            for error in errors:
                print(get_color_str("  Validation error! %s" % error, Color.RED))
                #raise "Failed to upload bundle %s" % metadata.app_name
        else:
            print(get_color_str("Bundle %s uploaded!" % bundle, Color.GREEN))

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
            print()
            print(get_color_str("-" * 80, Color.GREEN))

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
    args.endpoint_form = "http://%s%s" % (args.server, UPLOAD_FORM_PATH)
    print("Using endpoint: %s" % get_color_str(args.endpoint, Color.GREEN))

    BundlePublisher(args).publish_all()

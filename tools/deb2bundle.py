#!/usr/bin/env python

import argparse

VERSION="0.1"

# Allows for invoking attributes as methods/functions
class AttributeDict(dict):
    def __getattr__(self, attr):
        return self[attr]
    def __setattr__(self, attr, value):
        self[attr] = value

class BundleConverter(object):
    def __init__(self, args):
        self.args = args

    def convert(self):
        pass

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Converts Debian packages into application bundles')

    parser.add_argument('--debug', \
            help='Enable debugging output', \
            action='store_true')

    parser.add_argument('--version', \
            action='version', \
            version='%(prog)s v' + VERSION)

    args = AttributeDict(vars(parser.parse_args()))

    BundleConverter(args).convert()

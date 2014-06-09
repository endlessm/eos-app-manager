#!/usr/bin/env python3

# Note: for the intended use case, the debian package will not be
# published to the apt repo yet, so we need to process the debian file
# rather than checking the apt repo using the debian package name

import apt
import apt.debfile
import argparse
import os
import sys

VERSION="0.1"

# Allows for invoking attributes as methods/functions
class AttributeDict(dict):
    def __getattr__(self, attr):
        return self[attr]
    def __setattr__(self, attr, value):
        self[attr] = value

# Dependency checker
class DependencyChecker(object):
    def __init__(self, core_name):
        self._core_name = core_name
        self._cache = apt.Cache()
        self._core_deps = []
        self._find_deps(self._core_deps, self._core_name)

    def _find_deps(self, deps, pkg_name):
        if pkg_name not in deps:
            deps.append(pkg_name)
            pkg = self._cache[pkg_name]
            for dep_list in pkg.candidate.dependencies:
                # The dep_list is a list of possibilities,
                # only one of which is required
                for dep in dep_list:
                    dep_name = dep.name
                    if self._cache.has_key(dep_name):
                        self._find_deps(deps, dep_name)
                        break;
                else:
                    # No candidate found in cache
                    # TODO: should raise an exception,
                    # but need to troubleshoot some edge cases first
                    print('No candidate found in cache for any of ' +
                          str(dep_list) + ' required by ' + pkg_name)

    def find_missing_deps(self, deb_package):
        if not os.path.isfile(deb_package):
            sys.stderr.write("File not found: " + deb_package + "\n")
            exit(1)

        # Recursively find all the dependencies of the package
        deb = apt.debfile.DebPackage(deb_package)
        deb.check()
        top_deps = deb.depends
        deb_deps = []
        for dep_list in top_deps:
            # The dep_list is a list of possibilities,
            # only one of which is required
            for dep in dep_list:
                dep_name = dep[0]
                if self._cache.has_key(dep_name):
                    self._find_deps(deb_deps, dep_name)
                    break;
            else:
                # No candidate found in cache
                # TODO: should raise an exception,
                # but need to troubleshoot some edge cases first
                print('No candidate found in cache for any of ' +
                      str(dep_list) + ' required by ' + deb_package)

        # Filter out the debian dependencies that are in the
        # core dependency tree
        missing_deps = []
        for deb_dep in deb_deps:
            if deb_dep not in self._core_deps:
                missing_deps.append(deb_dep)

        return missing_deps

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description = 'Recursively finds missing package dependencies')

    parser.add_argument(
        'deb_package',
        help = 'Debian package file to analyze')

    parser.add_argument(
        '-c', '--core_name',
        help = 'Core package name for the installation (default = eos-core)',
        default = 'eos-core')

    parser.add_argument(
        '--version',
        action = 'version',
        version = '%(prog)s v' + VERSION)

    args = AttributeDict(vars(parser.parse_args()))

    dep_checker = DependencyChecker(args.core_name)
    missing_deps = dep_checker.find_missing_deps(args.deb_package)
    print(missing_deps)

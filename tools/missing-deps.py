#!/usr/bin/env python3

import apt
import apt.debfile
import argparse
import os
import sys

VERSION = "0.1"

# Allows for invoking attributes as methods/functions
class AttributeDict(dict):
    def __getattr__(self, attr):
        return self[attr]
    def __setattr__(self, attr, value):
        self[attr] = value

# Dependency checker
class DependencyChecker(object):
    # Core names is a single string with comma-separated names
    def __init__(self, core_names):
        self._core_names = core_names
        self._cache = apt.Cache()
        self._core_deps = []
        if self._core_names:
            names = self._core_names.split(',')
            for name in names:
                self._core_deps.append(name)
                self._find_deps(self._core_deps, name)
            self._core_deps.sort()

    # Find the real package that provides the list of options,
    # which may include virtual packages
    def _find_providing_pkg(self, deps, pkg_list):
        for pkg in pkg_list:
            try:
                # For Cache providing packages and candidate dependencies,
                # each entry is a dictionary with a name
                pkg_name = pkg.name
            except AttributeError:
                # For DebPackage depends, each entry is a tuple
                # with name as the first element
                pkg_name = pkg[0]
            if pkg_name in deps:
                # We reached a circular dependency -- don't dive further
                return pkg_name
            if self._cache.is_virtual_package(pkg_name):
                providers = self._cache.get_providing_packages(pkg_name)
                providing_pkg = self._find_providing_pkg(deps, providers)
                if providing_pkg:
                    return providing_pkg
            elif self._cache.has_key(pkg_name):
                # Check if all the dependencies of the candidate are available
                # If so, use it -- if not, move on to the next candidate
                try:
                    # Make a copy of the dependency list
                    # that will modified during this trial
                    try_deps = deps.copy()
                    # Include the current package in the trial dependency list,
                    # so as to resolve circular dependencies
                    try_deps.append(pkg_name)
                    self._find_deps(try_deps, pkg_name)
                except:
                    # Not a valid candidate -- skip it
                    continue
                # All dependencies are available -- include it
                return pkg_name
        return None

    # Recursively find all the dependencies of the package
    # (either .deb file or package name -- not a virtual package)
    # and append to the provided deps list
    def _find_deps(self, deps, pkg_name):
        if pkg_name.endswith('.deb'):
            # Debian package file
            if not os.path.isfile(pkg_name):
                raise Exception(pkg_name + ': file not found')
            deb = apt.debfile.DebPackage(pkg_name)
            deb.check()
            direct_deps = deb.depends
        else:
            # Not a .deb file: assume it is a package name
            if not self._cache.has_key(pkg_name):
                raise Exception(pkg_name + ': package not found in cache')
            pkg = self._cache[pkg_name]
            direct_deps = pkg.candidate.dependencies
        for dep_list in direct_deps:
            # The dep_list is a list of possible packages,
            # real and/or virtual, only one of which is required
            real_pkg = self._find_providing_pkg(deps, dep_list)
            if not real_pkg:
                raise Exception(pkg_name + ': no candidate found for any of ' +
                                str(dep_list))
            # If not already processed, add the real package to the overall
            # dependencies and find all of its recursive dependencies
            if not real_pkg in deps:
                deps.append(real_pkg)
                self._find_deps(deps, real_pkg)

    # Find all the deps of the specified package (name or file) that are not
    # already present in the system as a dependency of the core package
    def find_missing_deps(self, pkg_name):
        # Recursively find all the dependencies of the package
        pkg_deps = []
        self._find_deps(pkg_deps, pkg_name)
        pkg_deps.sort()
        # Filter out the debian dependencies that are in the
        # core dependency tree
        missing_deps = []
        for pkg_dep in pkg_deps:
            if pkg_dep not in self._core_deps:
                missing_deps.append(pkg_dep)
        return missing_deps

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description = 'Recursively finds missing package dependencies')

    parser.add_argument(
        'deb_package',
        help = 'Debian package name or .deb file to analyze')

    parser.add_argument(
        '-c', '--core_names',
        help = 'Comma-separate core package name(s) (default = none)',
        default = None)

    parser.add_argument(
        '--version',
        action = 'version',
        version = '%(prog)s v' + VERSION)

    args = AttributeDict(vars(parser.parse_args()))

    dep_checker = DependencyChecker(args.core_names)
    missing_deps = dep_checker.find_missing_deps(args.deb_package)
    print(missing_deps)

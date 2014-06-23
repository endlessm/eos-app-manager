What is an Application?
#######################


An application consists of a directory, which contains binaries, auxiliary
private libraries, source code, content (assets, database files), and various
other metadata files.

An application is entirely self-contained within this directory. The code may
call code that lives in the SDK, but everything required to run the
application should be in its directory.


Where does an Application live?
###############################

Each application has an ‘app_id’ and lives in a pre-defined, designated place
for applications, which is in `/endless/{app_id}`

Crucially, this location is outside of OSTree’s control, since we want the
update process for the apps to be separate from that of the general OS. The
`/endless` directory will really be a link to `/var/endless` much like `/home`
links to `/var/home` in OSTree.

There are several metadata files related to applications, which need to live
in a common directory so that the OS can find them. So we will also have the
following directories:

  *  `/endless/share/applications/{all}.desktop`
     * Contains desktop files for the Endless applications. Each desktop file
       in here is simply a symlink to the actual desktop file which lives in
       an application’s directory.

  * `/endless/share/icons/EndlessOS/{desktop_icons}`
     * Contains desktop icons for Endless applications. Again, these are
       symlinks to the actual icons which live in an applications
       directory. Note that all other icons the application uses internally
       must not live in here, and the application needs to use another
       approach to locate them (like GResource or installing them in a
       location and adding it to GtkIconTheme search path)

  * `/endless/share/glib-2.0/schemas/{gsettings_files}`
     * Contains gsettings files for Endless applications. Again, symlinks

  * `/endless/share/dbus-1/services/{service_files}`
     * Contains dbus service files for Endless applications. Again, symlinks


We set the environment variable `$XDG_DATA_DIRS`. It is an array, specifying
an ordering of places in which to look for desktop files and icons. We include
in that array `/endless/share` so that the system will look there to find our
metadata files.


Installation Process
####################

We have a DBus system daemon called eos-app-installer:

  * Runs as root
  * Is itself installed with OSTree, and is therefore updated with the system
    updates
  * Entirely manages the /endless directory, and, therefore manages all
    Endless applications
  * Has an API for the app store to use


Methods available to eos-app-installer:
=======================================


Install (string app_id)
-----------------------

installs an app for a user:

  * If the application is NOT on the system at all, then we must do a system
    install:

    1. Download the application directory from our server (more on that
       later) - Fetch()
    2. Place the directory at location /endless/{app_id} - Apply()
    3. Create symlinks for desktop file, icons, service file, gsettings schema
    4. Run the commands glib-compile-schemas, gtk-update-icon-cache and
       update-desktop-database
    5. Return, so the app store can possibly add it to the user’s desktop.

  * If the application is on the system but not yet on the current user’s
    desktop, there’s nothing to do and the method can just return
    successfully. The logic in the app store will add it to the desktop grid,
    in the same way of the point above.

  * If we are installed on system but an update is available

    1. Download and apply patch to the application directory (more on this
       later too)
    2. Re-run the commands glib-compile-schemas, gtk-update-icon-cache and
       update-desktop-database


Uninstall (string app_id)
-------------------------

uninstalls an application. Removes all symlinks in `/endless/share` and
removes the application directory from `/endless/`


Refresh ()
----------

Updates the locally stored list of application with data from the Endless
server


ListAvailable () -> (a(sss)a(sss))
---------------------------------------------

Returns the installable applications and the updatable applications, as
two arrays of (id, name, version) tuples.


ListInstalled () -> (a(sss))
---------------------------------------------

Returns the installed applications as an array of (id, name, version)
tuples.


GetUserCapabilities () -> (a{sv})
---------------------------------

Returns a dictionary of available capabilities for the UID that connected
to the service. The dictionary keys and value types are:

 * 'CanInstall' -> b
 * 'CanUninstall' -> b
 * 'CanRefresh' -> b

This method should be used by a service to check the capabilities of the
current user when building its interface; the actual permissions are
regulated through PolicyKit.

GetState()
----------

Gets the state of the service IDLE - TRANSACTION - CANCELLING


Cancel()
--------

Cancels the current transaction


Quit()
------

Exit the DBus services process


Properties of eos-app-installer
===============================


AvailableUpdates as read-only
-----------------------------

Specifies the list of updates currently available by app_id


Signals of eos-app-installer
============================

AvailableApplicationsChanged(as available_applications)
-------------------------------------------------------

A signal to which other processes can subscribe, indicating that the list of
available applications has been updated. The passed-in array of strings is the
new value returned by ListAvailable()


PolicyKit
#########

Some of the actions eos-app-installer can perform are privileged, and so
should be protected by a PolicyKit authorization check. In particular there
should be a set of PolicyKit actions defining the possible privileged
interactions with the installer process. See
http://www.freedesktop.org/software/polkit/docs/latest/polkit.8.html for more
information.


To ensure administrative users can install and remove applications, these
actions permissions should all default in the action definition to

  * allow_any: no
  * allow_inactive: no
  * allow_active: auth_admin_keep

Additionally, a set of authorization rules should be installed on the system
together with eos-app-installer, that give users in the adm group the ability
to perform them without typing in a password.

The available actions should be:

  * com.endlessm.app-installer.install-application: Able to install an
    application that wasn’t previously on the system
  * com.endlessm.app-installer.update-application: Able to update an
    application that was previously installed on the system
  * com.endlessm.app-installer.uninstall-application: Able to remove an
    application that was previously installed on the system
  * com.endlessm.app-installer.refresh-applications: Able to refresh the list
    of available application from the server


How are applications launched?
##############################

We have an utility called `eos-app-launcher`. Each application has it’s own
dbus service file, which lives in `/endless/share/dbus-1/services/`.
Applications are DBus-activated, indicating that by specifying in their
desktop file

| DBusActivatable=True

The applications service file tells it which DBus service to run by specifying e.g.:

| Name=com.endlessm.eos-photos


Dependencies
############

An application may depend on certain packages, e.g. node, or clutter, or a
certain version of glib etc. Rather than specifying the dependencies for each
application in some control file, we instead have an application depend on a
version of the Endless OS. So version 2.1 of the Typing app might depend on
EOS 3.1.8. This allows us to simplify the dependency process and helps
ensure what OSTree is trying to ensure in the first place - namely that we
don’t have lots and lots of different configurations of EOS out there in the
wild because different applications have installed different packages.

Any libraries that an application might need to run must be packaged with the
application itself if they are not installed on the system as a
whole.


Update Process
##############

1. Copy the directory of the app to be updated to a staging area
2. Fetch the correct patch from the server (See Server’s API below) along with
   a checksum of the updated directory, and a digital signature
3. Apply the patch to the app directory using a recursive implementation of
   xdelta
4. Checksum the app directory and verify that it matches the downloaded
   checksum
5. Copy directory back to `/endless/{app_id}`
6. Define process for updating very old apps. Say 1.0 to 1.3 when there is
   1.1, 1.1.1, 1.2, 1.2.1.


When do application updates happen?
===================================

* When the user starts an update by clicking the Update button in the
  application store
* Upon every OS update, automatically
* Optionally in an automatic way, with a configurable policy


REST API for Server
###################

Considerations:

* A user with any EOS version installed should be able to fetch a functional
  application bundle for every eos-application

   * With this in mind, we’re requiring that for each EOS version v, the
     server exposes (at least) the latest application version that works on v

   * The server should only provide application updates from older versions of
     an application to the very latest version (as opposed to incremental
     updates). The oldest application version updatable in this way should be
     the first version since the last REQUIRES_EOS_VERSION requirement; all
     other application versions will require an OS update before they can be
     updated

+---------------------------------------+------------------------------------------------------+--------------------------------------------------------------------------------+
| Resource and method(s)*               | Params                                               | Description                                                                    |
| Both HTML and JSON request capable    |                                                      |                                                                                |
+---------------------------------------+------------------------------------------------------+--------------------------------------------------------------------------------+
| /api/v:version                        | :version - denotes API version (currently only v1)   | Returns all AppUpdate objects that the server knows about                      |
| /api/v:version/updates                |                                                      |                                                                                |
| Used for debugging                    |                                                      |                                                                                |
+---------------------------------------+------------------------------------------------------+--------------------------------------------------------------------------------+
| /api/v1/updates/:osVer                | :osVer - denotes OS version requesting updates       | Returns all AppUpdate objects that the server knows about filtered by min      |
|                                       |                                                      | eos version                                                                    |
+---------------------------------------+------------------------------------------------------+--------------------------------------------------------------------------------+
| /api/v1/updates/:osVer/:appId         | appId - Application of interest                      | Returns list of all AppUpdate objects filtered for a specific app, os, and     |
|                                       |                                                      | optionally a personality.                                                      |
| Used for debugging                    | personality - Personality type of the OS             |                                                                                |
| Optional query params:                |                                                      |                                                                                |
| personality=<personality>             |                                                      |                                                                                |
+---------------------------------------+------------------------------------------------------+--------------------------------------------------------------------------------+
| /api/v1/updates/:osVer/:appId/:appVer | appVer - Target app version the client is requesting | Returns AppUpdateLink filtered for a specific app and os version. Without      |
|                                       |                                                      | parameters, returns AppUpdateLink to full blob otherwise calculates if blob is |
| Optional query params:                | origVer - Source version from which to upgrade       | needed based on origVer.                                                       |
| from=<origVer>                        |                                                      |                                                                                |
| personality=<personality>             |                                                      |                                                                                |
+---------------------------------------+------------------------------------------------------+--------------------------------------------------------------------------------+
| /uploads/bundle/<SHA2 hash>           |                                                      | Returns (or redirects to) full update for a specific app. Includes             |
|                                       |                                                      | checksum in custom HTTP header (a la Amazon AWS API[1])                        |
+---------------------------------------+------------------------------------------------------+--------------------------------------------------------------------------------+

AppUpdateLink = AppUpdate + { personality, downloadLink, shaHash, isDiff,
fromVersion }AppUpdate = { appId, appName, codeVersion, minOsVersion }


Future improvements
===================

Save the list of installed applications per-user somewhere in THE CLOUD to
restore it later in case of catastrophic failures that bring the whole system
down. Needs the concept of user ID.


Appendix
########

Here we document background to the discussion and give reasons why other
options were not followed.


Git
###

We considered using git to do application updates. Git is optimized to create
diffs between files and is known by all of us so would be reasonably easy to
use. The problem was that git is designed to keep around a history of all
prior commits. We could not find a good way to repeatedly purge the git commit
history so that we were only keeping around the latest version, yet still be
able to fetch only the diff of the latest version from a server.

In any case, we realized that behind the scenes, git uses xdelta, so it makes
more sense for us to just take the xdelta functionality and use it ourselves
to create a diff between two directories, rather than try to hack git to do
something for which it was not designed.


OSTree
######

We considered maintaining a separate OSTree per application to do updates for
that app. After extensive discussion with Colin Walters (a contributor to
OSTree) and others, we realized that OSTree is really built for updating an
entire OS, not a single directory. The code itself works based on the
assumption that lots of OS specific files exists in its tree and so to try to
modify it to work with just a single app directory would likely be more
trouble than it is worth. It has knowledge of /etc, /var, and
bootloaders. Moreover, we decided there would be a non-trivial performance and
space overhead to using a separate OSTree for each application.


Courgette
#########

Courgette is a new differential compression algorithm written by Google and is
used for their Chromium updates. It claims to have significantly better
compression numbers than, say, bsdiff or xdelta. The problem is that it is
entirely optimized for compression source code. In fact the way it works is by
trying to reverse compile the binary into source code and then look for small
diffs there. However, given that our applications are going to contain not
only source code but large content files (e.g. assets and database files), we
don’t think this is the right tool for us.

/* Copyright 2014 Endless Mobile, Inc. */

#include <gio/gio.h>
#include <glib/gstdio.h>

#include "eam-fs-sanity.h"
#include "eam-config.h"

#define BIN_SUBDIR "bin"
#define DESKTOP_FILES_SUBDIR "share/applications"
#define DESKTOP_ICONS_SUBDIR "share/icons/EndlessOS"
#define DBUS_SERVICES_SUBDIR "share/dbus-1/services"
#define G_SCHEMAS_SUBDIR "share/glib-2.0/schemas"

#define ROOT_DIR "/var"

static gboolean
applications_directory_exists ()
{
    gboolean retval;

    const gchar *appdir = eam_config_appdir ();
    g_assert (appdir);

    gchar *dir =  g_build_filename (ROOT_DIR, appdir, NULL);
    retval = g_file_test (dir, G_FILE_TEST_EXISTS);
    g_free (dir);

    return retval;
}

static gboolean
applications_directory_clear ()
{
    gboolean retval;

    const gchar *appdir = eam_config_appdir ();
    g_assert (appdir);

    gchar *dir = g_build_filename (ROOT_DIR, appdir, NULL);
    retval = eam_fs_sanity_delete (dir);
    g_free (dir);

    return retval;
}

static gboolean
applications_directory_create (void)
{
    gboolean retval = TRUE;

    const gchar *appdir = eam_config_appdir ();
    g_assert (appdir);

    /* Create the applications' directory structure under ROOT_DIR */
    gchar *bin_dir = g_build_filename (ROOT_DIR, appdir, BIN_SUBDIR, NULL);
    gchar *desktop_files_dir = g_build_filename (ROOT_DIR, appdir, DESKTOP_FILES_SUBDIR, NULL);
    gchar *desktop_icons_dir = g_build_filename (ROOT_DIR, appdir, DESKTOP_ICONS_SUBDIR, NULL);
    gchar *dbus_services_dir = g_build_filename (ROOT_DIR, appdir, DBUS_SERVICES_SUBDIR, NULL);
    gchar *g_schemas_dir = g_build_filename (ROOT_DIR, appdir, G_SCHEMAS_SUBDIR, NULL);
    const gint mode = 0755;

    if (g_mkdir_with_parents (bin_dir, mode) != 0) {
        g_critical ("Unable to create '%s'", desktop_files_dir);
        retval = FALSE;
        goto bail;
    }
    if (g_mkdir_with_parents (desktop_files_dir, mode) != 0) {
        g_critical ("Unable to create '%s'", desktop_files_dir);
        retval = FALSE;
        goto bail;
    }
    if (g_mkdir_with_parents (desktop_icons_dir, mode) != 0) {
        g_critical ("Unable to create '%s'", desktop_icons_dir);
        retval = FALSE;
        goto bail;
    }
    if (g_mkdir_with_parents (dbus_services_dir, mode) != 0) {
        g_critical ("Unable to create '%s'", dbus_services_dir);
        retval = FALSE;
        goto bail;
    }
    if (g_mkdir_with_parents (g_schemas_dir, mode) != 0) {
        g_critical ("Unable to create '%s'", g_schemas_dir);
        retval = FALSE;
        goto bail;
    }

bail:
    g_free (bin_dir);
    g_free (desktop_files_dir);
    g_free (desktop_icons_dir);
    g_free (g_schemas_dir);
    g_free (dbus_services_dir);

    if (!retval && applications_directory_exists () && !applications_directory_clear ())
        g_critical ("Unable to clear files after the failed applications' directory creation");

    return retval;
}

static gboolean
applications_directory_symlink_exists ()
{
    const gchar *appdir = eam_config_appdir ();
    g_assert (appdir);

    return g_file_test (appdir, G_FILE_TEST_EXISTS);
}

static gboolean
applications_directory_symlink_clear ()
{
    const gchar *appdir = eam_config_appdir ();
    g_assert (appdir);

    return (g_remove (appdir) == 0);
}

static gboolean
applications_directory_symlink_create ()
{
    gboolean retval = TRUE;

    const gchar *appdir = eam_config_appdir ();
    g_assert (appdir);

    GFile *sym_link = g_file_new_for_path (appdir);
    gchar *dir = g_build_filename (ROOT_DIR, appdir, NULL);
    GError *error = NULL;

    g_file_make_symbolic_link (sym_link, dir, NULL, &error);
    if (error) {
        g_critical ("Unable to create symbolic link from '%s' to '%s'", appdir, dir);
        g_clear_error (&error);

        if (applications_directory_symlink_exists () && !applications_directory_symlink_clear ())
            g_critical ("Unable to clear a failed symlink creation");

        retval = FALSE;
    }

    g_free (dir);
    g_object_unref (sym_link);

    return retval;
}

/**
 * eam_fs_sanity_check:
 * Guarantees the existance of the applications' root installation directory and the
 * subdirectories required by the Application Manager to work.
 *
 * If the applications' directory does not exist, it and its required subdirectories
 * are created. If the applications' directory is corrupted, FALSE is returned.
 *
 * Returns: TRUE if the applications' directory and *all* its subdirectories exist or
 * are successfully created, FALSE otherwise.
 **/
gboolean
eam_fs_sanity_check (void)
{
    gboolean retval = TRUE;

    const gchar *appdir = eam_config_appdir ();
    g_assert (appdir);

    /* Create the applications installation directory if it does not exist */
    if (!applications_directory_exists () && !applications_directory_create ()) {
        g_critical ("Failed to create the applications directory structure '%s' under '%s'", appdir, ROOT_DIR);
      return FALSE;
    }
    if (!applications_directory_symlink_exists () && !applications_directory_symlink_create ()) {
        g_critical ("Failed to create the symbolic link '%s' to the applications directory", appdir);
        return FALSE;
    }

    /* Check if the existing applications directory structure is correct */
    gchar *bin_dir = g_build_filename (appdir, BIN_SUBDIR, NULL);
    gchar *desktop_files_dir = g_build_filename (appdir, DESKTOP_FILES_SUBDIR, NULL);
    gchar *desktop_icons_dir = g_build_filename (appdir, DESKTOP_ICONS_SUBDIR, NULL);
    gchar *dbus_services_dir = g_build_filename (appdir, DBUS_SERVICES_SUBDIR, NULL);
    gchar *g_schemas_dir = g_build_filename (appdir, G_SCHEMAS_SUBDIR, NULL);

    if (!g_file_test (appdir, G_FILE_TEST_IS_DIR)) {
        g_critical ("Missing directory: '%s' does not exist", appdir);
        retval = FALSE;
    }
    if (!g_file_test (bin_dir, G_FILE_TEST_IS_DIR)) {
        g_critical ("Missing directory: '%s' does not exist", appdir);
        retval = FALSE;
    }
    if (!g_file_test (desktop_files_dir, G_FILE_TEST_IS_DIR)) {
        g_critical ("Missing directory: '%s' does not exist", desktop_files_dir);
        retval = FALSE;
    }
    if (!g_file_test (desktop_icons_dir, G_FILE_TEST_IS_DIR)) {
        g_critical ("Missing directory: '%s' does not exist", desktop_icons_dir);
        retval = FALSE;
    }
    if (!g_file_test (g_schemas_dir, G_FILE_TEST_IS_DIR)) {
        g_critical ("Missing directory: '%s' does not exist", g_schemas_dir);
        retval = FALSE;
    }
    if  (!g_file_test (dbus_services_dir, G_FILE_TEST_IS_DIR)) {
        g_critical ("Missing directory: '%s' does not exist", dbus_services_dir);
        retval = FALSE;
    }

    g_free (bin_dir);
    g_free (desktop_files_dir);
    g_free (desktop_icons_dir);
    g_free (dbus_services_dir);
    g_free (g_schemas_dir);

    return retval;
}

/**
 * eam_fs_sanity_delete:
 * @path: absolute path to the file to be removed
 *
 * Deletes a file. If the file is a directory, deletes the directory and its
 * contents recursively.
 *
 * Returns: TRUE if the file was successfully removed, FALSE otherwise.
 **/
gboolean
eam_fs_sanity_delete (const gchar *path)
{
    GError *error = NULL;
    GFileInfo *child_info;
    gboolean retval = TRUE;

    g_assert (path);

    GFile *file = g_file_new_for_path (path);

    /* Check for symlinks first, to make an early decision if possible */
    GFileInfo *file_info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK,
                                             G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error);
    if (error) {
      g_warning ("Failure querying information for file '%s': %s\n", path, error->message);
      g_clear_error (&error);
      goto bail;
    }

    gboolean file_is_symlink = g_file_info_get_is_symlink (file_info);
    g_object_unref (file_info);
    if (file_is_symlink)
      goto delete;

    /* Now we can iterate over the children with NO_FOLLOW_LINKS safely */
    GFileEnumerator *children = g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                           G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                           NULL, &error);

    if (error) {
        int code = error->code;
        g_clear_error (&error);

        if (code == G_IO_ERROR_NOT_FOUND) {
            g_warning ("Trying to delete an unexistent file '%s'", path);
            retval = FALSE;
            goto bail;
        } else if (code == G_IO_ERROR_NOT_DIRECTORY) {
            /* The file is a regular file, not a directory */
            goto delete;
        } else {
            g_warning ("Failed to get the children of '%s': %s", path, error->message);
            g_clear_error (&error);
            retval = FALSE;
            goto bail;
        }
    }

    while ((child_info = g_file_enumerator_next_file (children, NULL, &error))) {
        GFile *child = g_file_get_child (file, g_file_info_get_name (child_info));
        gchar *child_path = g_file_get_path (child);
        g_object_unref (child_info);
        g_object_unref (child);

        eam_fs_sanity_delete (child_path);
        g_free (child_path);
    }
    g_object_unref (children);

    if (error) {
        g_warning ("Failure while processing the children of '%s': %s", path, error->message);
        g_clear_error (&error);
        retval = FALSE;
        goto bail;
    }

delete:
    g_file_delete (file, NULL, &error);
    if (error) {
        g_warning ("Failed to delete file '%s': %s", path, error->message);
        g_clear_error (&error);
        retval = FALSE;
    }

bail:
    g_object_unref (file);
    return retval;
}

/* Copyright 2014 Endless Mobile, Inc. */
#include "config.h"

#include <gio/gio.h>
#include <glib/gstdio.h>

#include "eam-fs-sanity.h"
#include "eam-config.h"
#include "eam-log.h"
#include "eam-utils.h"

#define BIN_SUBDIR "bin"
#define DESKTOP_FILES_SUBDIR "share/applications"
#define DESKTOP_ICONS_SUBDIR "share/icons"
#define DBUS_SERVICES_SUBDIR "share/dbus-1/services"
#define EKN_DATA_SUBDIR "share/ekn/data"
#define EKN_MANIFEST_SUBDIR "share/ekn/manifest"
#define G_SCHEMAS_SUBDIR "share/glib-2.0/schemas"

#define ROOT_DIR "/var"

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
  gchar *ekn_data_dir = g_build_filename (ROOT_DIR, appdir, EKN_DATA_SUBDIR, NULL);
  gchar *ekn_manifest_dir = g_build_filename (ROOT_DIR, appdir, EKN_MANIFEST_SUBDIR, NULL);
  gchar *g_schemas_dir = g_build_filename (ROOT_DIR, appdir, G_SCHEMAS_SUBDIR, NULL);
  const gint mode = 0755;

  if (g_mkdir_with_parents (bin_dir, mode) != 0) {
    eam_log_error_message ("Unable to create '%s'", bin_dir);
    retval = FALSE;
    goto bail;
  }
  if (g_mkdir_with_parents (desktop_files_dir, mode) != 0) {
    eam_log_error_message ("Unable to create '%s'", desktop_files_dir);
    retval = FALSE;
    goto bail;
  }
  if (g_mkdir_with_parents (desktop_icons_dir, mode) != 0) {
    eam_log_error_message ("Unable to create '%s'", desktop_icons_dir);
    retval = FALSE;
    goto bail;
  }
  if (g_mkdir_with_parents (dbus_services_dir, mode) != 0) {
    eam_log_error_message ("Unable to create '%s'", dbus_services_dir);
    retval = FALSE;
    goto bail;
  }
  if (g_mkdir_with_parents (ekn_data_dir, mode) != 0) {
    eam_log_error_message ("Unable to create '%s'", ekn_data_dir);
    retval = FALSE;
    goto bail;
  }
  if (g_mkdir_with_parents (ekn_manifest_dir, mode) != 0) {
    eam_log_error_message ("Unable to create '%s'", ekn_manifest_dir);
    retval = FALSE;
    goto bail;
  }
  if (g_mkdir_with_parents (g_schemas_dir, mode) != 0) {
    eam_log_error_message ("Unable to create '%s'", g_schemas_dir);
    retval = FALSE;
    goto bail;
  }

bail:
  g_free (bin_dir);
  g_free (desktop_files_dir);
  g_free (desktop_icons_dir);
  g_free (dbus_services_dir);
  g_free (ekn_data_dir);
  g_free (ekn_manifest_dir);
  g_free (g_schemas_dir);

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
    eam_log_error_message ("Unable to create symbolic link from '%s' to '%s'", appdir, dir);
    g_clear_error (&error);

    if (applications_directory_symlink_exists () && !applications_directory_symlink_clear ())
        eam_log_error_message ("Unable to clear a failed symlink creation");

    retval = FALSE;
  }

  g_free (dir);
  g_object_unref (sym_link);

  return retval;
}

static gboolean
is_application_dir (const char *path)
{
  g_assert (path);

  if (!g_file_test (path, G_FILE_TEST_IS_DIR))
    return FALSE;

  gchar *appid = g_path_get_basename (path);
  gboolean retval = eam_utils_appid_is_legal (appid);
  g_free (appid);

  return retval;
}

static gboolean
fix_permissions_for_application (const gchar *path)
{
  GStatBuf buf;
  GError *error = NULL;
  gboolean retval = TRUE;

  g_assert (path);

  GFile *file = g_file_new_for_path (path);

  /* We fix permissions for the children of a directory first, so that the
   * root path is the last thing we fix when everything else has worked as
   * expected, so that we can try again the next time if something went wrong.
   */
  if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
    GFileEnumerator *children = g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                           G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                           NULL, &error);
    if (error) {
      eam_log_error_message ("Failed to get the children of '%s': %s", path, error->message);
      g_clear_error (&error);
      retval = FALSE;
      goto bail;
    }

    GFileInfo *child_info = NULL;
    while ((child_info = g_file_enumerator_next_file (children, NULL, &error))) {
      GFile *child = g_file_get_child (file, g_file_info_get_name (child_info));
      gchar *child_path = g_file_get_path (child);
      g_object_unref (child_info);
      g_object_unref (child);

      retval = fix_permissions_for_application (child_path);
      g_free (child_path);
    }
    g_object_unref (children);

    if (error) {
      eam_log_error_message ("Failure while processing the children of '%s': %s", path, error->message);
      g_clear_error (&error);
      retval = FALSE;
      goto bail;
    }
  }

  /* Don't bother updating the current path if something went
   * wrong already now while looking at its children.
   */
  if (!retval)
    goto bail;

  if (g_stat (path, &buf) == -1) {
    eam_log_error_message ("Error retrieving information about path '%s'", path);
    retval = FALSE;
    goto bail;
  }

  /* It's impossible to know what the desired permissions are for each file,
   * so we check 'r-x' permissions for "owner" and apply them to "other".
   */
  mode_t mod_mask = buf.st_mode;
  mod_mask |= (((buf.st_mode & S_IRUSR) | (buf.st_mode & S_IXUSR)) >> 6);

  if (g_chmod (path, mod_mask) == -1) {
    eam_log_error_message ("Error fixing permissions for path '%s'", path);
    retval = FALSE;
    goto bail;
  }

 bail:
  g_object_unref (file);
  return retval;
}

static void
fix_application_permissions_if_needed (const gchar *path)
{
  GStatBuf buf;

  g_assert (path);

  if (g_stat (path, &buf) == -1) {
    eam_log_error_message ("Error retrieving information about file '%s'", path);
    return;
  }

  /* In order not to take too much time to decide whether fixing the permissions is needed,
   * we just check the permissions of the top directory for the app instead of doing ir
   * recursively, since that's probably enough most of the times.
   */
  if (!(buf.st_mode & S_IROTH && buf.st_mode & S_IXOTH))
    fix_permissions_for_application (path);
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

  /* Ensure the applications installation directory exists */
  if (!applications_directory_create ()) {
    eam_log_error_message ("Failed to create the applications directory structure '%s' under '%s'", appdir, ROOT_DIR);
    return FALSE;
  }
  if (!applications_directory_symlink_exists () && !applications_directory_symlink_create ()) {
    eam_log_error_message ("Failed to create the symbolic link '%s' to the applications directory", appdir);
    return FALSE;
  }

  /* Check if the existing applications directory structure is correct */
  gchar *bin_dir = g_build_filename (appdir, BIN_SUBDIR, NULL);
  gchar *desktop_files_dir = g_build_filename (appdir, DESKTOP_FILES_SUBDIR, NULL);
  gchar *desktop_icons_dir = g_build_filename (appdir, DESKTOP_ICONS_SUBDIR, NULL);
  gchar *dbus_services_dir = g_build_filename (appdir, DBUS_SERVICES_SUBDIR, NULL);
  gchar *ekn_data_dir = g_build_filename (appdir, EKN_DATA_SUBDIR, NULL);
  gchar *ekn_manifest_dir = g_build_filename (appdir, EKN_MANIFEST_SUBDIR, NULL);
  gchar *g_schemas_dir = g_build_filename (appdir, G_SCHEMAS_SUBDIR, NULL);

  if (!g_file_test (appdir, G_FILE_TEST_IS_DIR)) {
    eam_log_error_message ("Missing directory: '%s' does not exist", appdir);
    retval = FALSE;
  }
  if (!g_file_test (bin_dir, G_FILE_TEST_IS_DIR)) {
    eam_log_error_message ("Missing directory: '%s' does not exist", bin_dir);
    retval = FALSE;
  }
  if (!g_file_test (desktop_files_dir, G_FILE_TEST_IS_DIR)) {
    eam_log_error_message ("Missing directory: '%s' does not exist", desktop_files_dir);
    retval = FALSE;
  }
  if (!g_file_test (desktop_icons_dir, G_FILE_TEST_IS_DIR)) {
    eam_log_error_message ("Missing directory: '%s' does not exist", desktop_icons_dir);
    retval = FALSE;
  }
  if (!g_file_test (dbus_services_dir, G_FILE_TEST_IS_DIR)) {
    eam_log_error_message ("Missing directory: '%s' does not exist", dbus_services_dir);
    retval = FALSE;
  }
  if (!g_file_test (ekn_data_dir, G_FILE_TEST_IS_DIR)) {
    eam_log_error_message ("Missing directory: '%s' does not exist", ekn_data_dir);
    retval = FALSE;
  }
  if (!g_file_test (ekn_manifest_dir, G_FILE_TEST_IS_DIR)) {
    eam_log_error_message ("Missing directory: '%s' does not exist", ekn_manifest_dir);
    retval = FALSE;
  }
  if (!g_file_test (g_schemas_dir, G_FILE_TEST_IS_DIR)) {
    eam_log_error_message ("Missing directory: '%s' does not exist", g_schemas_dir);
    retval = FALSE;
  }

  /* Check if application directories have valid permissions, fixing them if needed */
  GError *error = NULL;
  GFile *file = g_file_new_for_path (appdir);
  GFileEnumerator *children = g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                         G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                         NULL, &error);
  if (error) {
    eam_log_error_message ("Failed to get the children of '%s': %s", appdir, error->message);
    g_clear_error (&error);
    retval = FALSE;
    goto bail;
  }

  GFileInfo *child_info = NULL;
  while ((child_info = g_file_enumerator_next_file (children, NULL, &error))) {
    GFile *dir = g_file_get_child (file, g_file_info_get_name (child_info));
    gchar *path = g_file_get_path (dir);
    g_object_unref (child_info);
    g_object_unref (dir);

    if (is_application_dir (path))
      fix_application_permissions_if_needed (path);

    g_free (path);
  }

  if (error) {
    eam_log_error_message ("Failure while processing the children of '%s': %s", appdir, error->message);
    g_clear_error (&error);
    retval = FALSE;
  }

 bail:
  g_free (bin_dir);
  g_free (desktop_files_dir);
  g_free (desktop_icons_dir);
  g_free (dbus_services_dir);
  g_free (ekn_data_dir);
  g_free (ekn_manifest_dir);
  g_free (g_schemas_dir);

  g_object_unref (children);
  g_object_unref (file);

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
  GFileInfo *file_info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK,
      				      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error);
  if (error) {
    eam_log_error_message ("Failure querying information for file '%s': %s\n", path, error->message);
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
      eam_log_error_message ("Trying to delete an unexistent file '%s'", path);
      retval = FALSE;
      goto bail;
    } else if (code == G_IO_ERROR_NOT_DIRECTORY) {
      /* The file is a regular file, not a directory */
      goto delete;
    } else {
      eam_log_error_message ("Failed to get the children of '%s': %s", path, error->message);
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
    eam_log_error_message ("Failure while processing the children of '%s': %s", path, error->message);
    g_clear_error (&error);
    retval = FALSE;
    goto bail;
  }

delete:
  g_file_delete (file, NULL, &error);
  if (error) {
    eam_log_error_message ("Failed to delete file '%s': %s", path, error->message);
    g_clear_error (&error);
    retval = FALSE;
  }

bail:
  g_object_unref (file);
  return retval;
}

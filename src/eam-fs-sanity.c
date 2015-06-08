/* Copyright 2014 Endless Mobile, Inc. */
#include "config.h"

#include "eam-fs-sanity.h"
#include "eam-config.h"
#include "eam-log.h"
#include "eam-utils.h"

#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <gio/gio.h>
#include <glib/gstdio.h>

#define BIN_SUBDIR "bin"
#define DESKTOP_FILES_SUBDIR "share/applications"
#define DESKTOP_ICONS_SUBDIR "share/icons"
#define DBUS_SERVICES_SUBDIR "share/dbus-1/services"
#define EKN_DATA_SUBDIR "share/ekn/data"
#define G_SCHEMAS_SUBDIR "share/glib-2.0/schemas"
#define XDG_AUTOSTART_SUBDIR "xdg/autostart"

#define ROOT_DIR "/var"

static const char *fs_layout[] = {
  [EAM_BUNDLE_DIRECTORY_BIN] = "bin",
  [EAM_BUNDLE_DIRECTORY_DESKTOP] = "share/applications",
  [EAM_BUNDLE_DIRECTORY_ICONS] = "share/icons",
  [EAM_BUNDLE_DIRECTORY_DBUS_SERVICES] = "share/dbus-1/services",
  [EAM_BUNDLE_DIRECTORY_GSETTINGS_SCHEMAS] = "share/glib-2.0/schemas",
  [EAM_BUNDLE_DIRECTORY_GNOME_HELP] = "share/help",
  [EAM_BUNDLE_DIRECTORY_KDE_HELP] = "share/doc/kde",
  [EAM_BUNDLE_DIRECTORY_EKN_DATA] = "share/ekn/data",
  [EAM_BUNDLE_DIRECTORY_SHELL_SEARCH] = "share/gnome-shell/search-providers",
  [EAM_BUNDLE_DIRECTORY_KDE4] = "share/kde4",
  [EAM_BUNDLE_DIRECTORY_XDG_AUTOSTART] = "xdg/autostart",
};

const char *
eam_fs_get_bundle_system_dir (EamBundleDirectory dir)
{
  return (dir >= EAM_BUNDLE_DIRECTORY_BIN && dir < EAM_BUNDLE_DIRECTORY_MAX)
    ? fs_layout[dir]
    : NULL;
}

static char *
get_bundle_path (EamBundleDirectory dir)
{
  return g_build_filename (ROOT_DIR, eam_config_appdir (), eam_fs_get_bundle_system_dir (dir), NULL);
}

static gboolean
applications_directory_create (void)
{
  for (guint i = 0; i < EAM_BUNDLE_DIRECTORY_MAX; i++) {
    g_autofree char *dir = get_bundle_path (i);

    if (g_mkdir_with_parents (dir, 0755) != 0) {
      eam_log_error_message ("Unable to create '%s': %s", dir, g_strerror (errno));
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
applications_directory_symlink_exists (void)
{
  const gchar *appdir = eam_config_appdir ();
  g_assert (appdir);

  return g_file_test (appdir, G_FILE_TEST_EXISTS);
}

static gboolean
applications_directory_symlink_clear (void)
{
  const gchar *appdir = eam_config_appdir ();
  g_assert (appdir);

  return (g_remove (appdir) == 0);
}

static gboolean
applications_directory_symlink_create (void)
{
  const char *appdir = eam_config_appdir ();

  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) sym_link = g_file_new_for_path (appdir);
  g_autofree char *dir = g_build_filename (ROOT_DIR, appdir, NULL);

  g_file_make_symbolic_link (sym_link, dir, NULL, &error);
  if (error) {
    eam_log_error_message ("Unable to create symbolic link from '%s' to '%s'", appdir, dir);

    if (applications_directory_symlink_exists () && !applications_directory_symlink_clear ())
      eam_log_error_message ("Unable to clear a failed symlink creation");

    return FALSE;
  }

  return TRUE;
}

static gboolean
is_application_dir (const char *path)
{
  g_assert (path);

  if (!g_file_test (path, G_FILE_TEST_IS_DIR))
    return FALSE;

  g_autofree char *info_path = g_build_filename (path, ".info", NULL);

  return g_file_test (info_path, G_FILE_TEST_EXISTS);
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
    eam_log_error_message ("Failed to create the applications directory '%s%s'", ROOT_DIR, appdir);
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
  gchar *g_schemas_dir = g_build_filename (appdir, G_SCHEMAS_SUBDIR, NULL);
  gchar *xdg_autostart_dir = g_build_filename (appdir, XDG_AUTOSTART_SUBDIR, NULL);

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
  if (!g_file_test (g_schemas_dir, G_FILE_TEST_IS_DIR)) {
    eam_log_error_message ("Missing directory: '%s' does not exist", g_schemas_dir);
    retval = FALSE;
  }
  if (!g_file_test (xdg_autostart_dir, G_FILE_TEST_IS_DIR)) {
    eam_log_error_message ("Missing directory: '%s' does not exist", xdg_autostart_dir);
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
  g_free (g_schemas_dir);
  g_free (xdg_autostart_dir);

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

static inline int
is_dot_or_dotdot (const char *name)
{
  return (strlen (name) < 3 &&
          name[0] == '.' &&
          (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')));
}

gboolean
eam_fs_rmdir_recursive (const char *path)
{
  g_autoptr(GError) err = NULL;
  g_autoptr(GDir) dir = g_dir_open (path, 0, &err);
  if (err != NULL) {
    /* If the directory is already gone, then we consider it a success */
    if (g_error_matches (err, G_FILE_ERROR, G_FILE_ERROR_NOENT))
      return TRUE;

    /* An empty dir could be removable even if it is unreadable. */
    if (g_error_matches (err, G_FILE_ERROR, G_FILE_ERROR_ACCES))
      return rmdir (path) == 0;

    return FALSE;
  }

  gboolean ret = TRUE;

  const char *fn;
  while ((fn = g_dir_read_name (dir)) != NULL) {
    if (is_dot_or_dotdot (fn))
      continue;

    g_autofree char *epath = g_build_filename (path, fn, NULL);

    struct stat st;
    if (lstat (epath, &st) == -1) {
      /* file disappeared, which is what we wanted anyway */
      if (errno == ENOENT)
        continue;
    }
    else if (S_ISDIR (st.st_mode)) {
      if (eam_fs_rmdir_recursive (epath) == 0)
        continue; /* happy */
    }
    else if (!unlink (epath) || errno == ENOENT) {
      continue; /* happy, too */
    }

    /* stat fails, or non-directory still exists */
    ret = FALSE;
    break;
  }

  /* The directory at path should now be empty */
  if (ret)
    ret = (rmdir (path) == 0 || errno == ENOENT) ? TRUE : FALSE;

  return ret;
}

gboolean
eam_fs_prune_dir (const char *prefix,
                  const char *appdir)
{
  char *path = g_build_filename (prefix, appdir, NULL);
  gboolean res = eam_fs_rmdir_recursive (path);

  g_free (path);

  return res;
}

#define CP_ENUMERATE_ATTRS \
  "standard::type," \
  "standard::name," \
  "unix::uid," \
  "unix::gid," \
  "unix::mode"

#define CP_QUERY_ATTRS \
  "standard::name," \
  "unix::mode," \
  "unix::uid," \
  "unix::gid," \
  "time::modified," \
  "time::modified-usec," \
  "time::access," \
  "time::access-usec"

static gboolean
cp_internal (GFile *source,
             GFile *target)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GFileEnumerator) enumerator =
    g_file_enumerate_children (source, CP_ENUMERATE_ATTRS,
                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                               NULL, &error);

  if (error != NULL) {
    eam_log_error_message ("Unable to enumerate source: %s", error->message);
    return FALSE;
  }

  g_autoptr(GFileInfo) source_info =
    g_file_query_info (source, CP_QUERY_ATTRS,
                       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                       NULL, &error);
  if (error != NULL) {
    eam_log_error_message ("Unable to query source info: %s", error->message);
    return FALSE;
  }

  g_autofree char *target_path = g_file_get_path (target);
  int r;

  do {
    r = mkdir (target_path, 0755);
  } while (r == -1 && errno == EINTR);

  if (r == -1) {
    eam_log_error_message ("Unable to create target directory: %s", g_strerror (errno));
    return FALSE;
  }

  /* XXX: This is Linux-specific */
  int target_fd = open (target_path, O_RDONLY | O_NONBLOCK | O_DIRECTORY | O_CLOEXEC);
  if (target_fd == -1) {
    eam_log_error_message ("Unable to open target directory: %s", g_strerror (errno));
    return FALSE;
  }

  do {
    guint32 src_uid = g_file_info_get_attribute_uint32 (source_info, "unix::uid");
    guint32 src_gid = g_file_info_get_attribute_uint32 (source_info, "uinx::gid");
    r = fchown (target_fd, src_uid, src_gid);
  } while (r == -1 && errno == EINTR);
  if (r == -1) {
    eam_log_error_message ("Unable to set ownership of target directory: %s",
                           g_strerror (errno));
    return FALSE;
  }

  do {
    guint32 src_mode = g_file_info_get_attribute_uint32 (source_info, "unix::mode");
    r = fchmod (target_fd, src_mode);
  } while (r == -1 && errno == EINTR);
  if (r == -1) {
    eam_log_error_message ("Unable to set mode of target directory: %s", g_strerror (errno));
    return FALSE;
  }

  (void) close (target_fd);

  GFile *target_child = NULL;
  while (TRUE) {
    g_autoptr(GFileInfo) file_info = NULL;
    g_autoptr(GFile) source_child;

    if (!g_file_enumerator_iterate (enumerator, &file_info, &source_child, NULL, &error)) {
      eam_log_error_message ("Unable to enumerate source: %s", error->message);
      return FALSE;
    }

    if (file_info == NULL)
      break;

    g_clear_object (&target_child);
    target_child = g_file_get_child (target, g_file_info_get_name (file_info));

    if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY) {
      if (!cp_internal (source_child, target_child)) {
        g_object_unref (target_child);
        return FALSE;
      }
    }
    else {
      GFileCopyFlags flags = G_FILE_COPY_OVERWRITE |
                             G_FILE_COPY_NOFOLLOW_SYMLINKS |
                             G_FILE_COPY_ALL_METADATA;

      if (!g_file_copy (source_child, target_child, flags, NULL, NULL, NULL, &error)) {
        eam_log_error_message ("Unable to copy source file: %s", error->message);
        return FALSE;
      }
    }
  }

  return TRUE;
}

gboolean
eam_fs_cpdir_recursive (const char *src,
                        const char *dst)
{
  g_autoptr(GFile) source = g_file_new_for_path (src);
  g_autoptr(GFile) target = g_file_new_for_path (dst);

  return cp_internal (source, target);
}


gboolean
eam_fs_deploy_app (const char *source,
                   const char *target,
                   const char *appid)
{
  g_autofree char *sdir = g_build_filename (source, appid, NULL);
  g_autofree char *tdir = g_build_filename (target, appid, NULL);

  gboolean ret = FALSE;

  if (rename (sdir, tdir) < 0) {
    /* If the rename() failed because we tried to move across
     * file system boundaries, then we do an explicit recursive
     * copy.
     */
    ret = (errno == EXDEV) && (eam_fs_cpdir_recursive (sdir, tdir));
  } else {
    ret = TRUE;
  }

  if (!ret) {
    eam_log_error_message ("Moving '%s' from '%s' to '%s' failed: %s",
                           appid, sdir, tdir, g_strerror (errno));
    eam_fs_rmdir_recursive (tdir); /* clean up the appdir */
  }

  eam_fs_rmdir_recursive (sdir);

  return ret;
}

static int
symlinkdirs_recursive (const char *source_dir,
                       const char *target_dir,
                       gboolean    shallow)
{
  struct dirent *e;
  int ret = 0;
  gchar *spath, *tpath;

  if (g_mkdir_with_parents (target_dir, 0755) < 0)
    return -1;

  DIR *dir = opendir (source_dir);
  if (!dir)
    return 0; /* it's OK if the bundle doesn't have that dir */

  spath = tpath = NULL;
  while ((e = readdir (dir)) != NULL) {
    if (is_dot_or_dotdot (e->d_name))
      continue;

    g_free (spath);
    g_free (tpath);
    spath = g_build_filename (source_dir, e->d_name, NULL);
    tpath = g_build_filename (target_dir, e->d_name, NULL);

    struct stat st;
    if (lstat (spath, &st)) {
      if (errno == ENOENT)
        continue;
    } else if (S_ISLNK (st.st_mode)) {
      gchar *file = g_file_read_link (spath, NULL);
      if (file) {
        if (!symlink (file, tpath) || errno == EEXIST)
          g_debug ("%s -> %s", file, tpath);
        g_free (file);
      }
      continue;
    } else if (S_ISREG (st.st_mode)) { /* symbolic link if regular file */
      if (!symlink (spath, tpath) || errno == EEXIST) {
        g_debug ("%s -> %s", spath, tpath);
        continue;
      }
    } else if (S_ISDIR (st.st_mode)) { /* recursive if directory and not shallow */
      if (!shallow && !symlinkdirs_recursive (spath, tpath, shallow))
        continue;
      else if (shallow && (!symlink (spath, tpath) || errno == EEXIST)) {
        g_debug ("%s -> %s", spath, tpath);
        continue;
      }
    } else { /* ignore the rest */
      continue;
    }

    ret = -1;
    break;
  }

  g_free (spath);
  g_free (tpath);

  closedir (dir);

  return ret;
}

static gchar *
find_first_desktop_file (const char *dir)
{
  g_autoptr(GDir) dp = g_dir_open (dir, 0, NULL);
  if (!dp)
    return NULL;

  const char *fn;
  while ((fn = g_dir_read_name (dp)) != NULL) {
    if (is_dot_or_dotdot (fn))
      continue;

    if (g_str_has_suffix (fn, ".desktop"))
      return g_strdup (fn);
  }

  return NULL;
}

static char *
remove_dir_from_path (const char *path,
                      const char *dir)
{
  const char *env_path = g_getenv ("PATH");
  if (env_path == NULL)
    return NULL;

  g_auto(GStrv) splitpath = g_strsplit (env_path, G_SEARCHPATH_SEPARATOR_S, -1);

  char *res = NULL;
  int i, len = g_strv_length (splitpath);
  for (i = 0; i < len; i++) {
    if (g_strcmp0 (splitpath[i], dir)) {
      continue;
    }

    char *tmp = res;
    res = g_strconcat (G_SEARCHPATH_SEPARATOR_S, tmp, splitpath[i], NULL);
    g_free (tmp);
  }

  return res;
}

static gboolean
make_binary_symlink (const char *bin,
                     const char *exec)
{
  if (!g_file_test (bin, G_FILE_TEST_EXISTS))
    return FALSE;

  g_autofree char *path = g_build_filename (eam_config_appdir (),
                                            eam_fs_get_bundle_system_dir (EAM_BUNDLE_DIRECTORY_BIN),
                                            exec,
                                            NULL);

  if (symlink (bin, path) == 0 || errno == EEXIST)
    return TRUE;

  return FALSE;
}

static char *
app_info_get_executable (const char *desktop_file)
{
  g_autoptr(GKeyFile) keyfile = g_key_file_new ();
  g_autoptr(GError) error = NULL;
  g_key_file_load_from_file (keyfile, desktop_file, G_KEY_FILE_NONE, &error);
  if (error != NULL) {
    eam_log_error_message ("Error loading desktop file '%s': %s",
                           desktop_file,
                           error->message);
    return NULL;
  }

  /* TryExec */
  char *exec = g_key_file_get_string (keyfile, G_KEY_FILE_DESKTOP_GROUP,
                                      G_KEY_FILE_DESKTOP_KEY_TRY_EXEC,
                                      NULL);

  if (exec && exec[0] != '\0')
    return exec;

  /* Exec */
  exec = g_key_file_get_string (keyfile, G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_EXEC,
                                NULL);

  if (exec && exec[0] != '\0') {
    g_auto(GStrv) argv;
    if (!g_shell_parse_argv (exec, NULL, &argv, NULL)) {
      g_free (exec);
      return NULL;
    }

    g_free (exec);
    exec = g_strdup (argv[0]);

    return exec;
  }

  return NULL;
}


static gboolean
do_binaries_symlinks (const char *appid)
{
  g_autofree char *desktopfile = g_strdup_printf ("%s.desktop", appid);
  g_autofree char *appdesktopdir = g_build_filename (eam_config_appdir (),
                                                     appid,
                                                     eam_fs_get_bundle_system_dir (EAM_BUNDLE_DIRECTORY_DESKTOP),
                                                     NULL);
  g_autofree char *appdesktopfile = g_build_filename (appdesktopdir, desktopfile, NULL);

  if (!g_file_test (appdesktopfile, G_FILE_TEST_EXISTS)) {
    g_free (desktopfile);
    g_free (appdesktopfile);

    desktopfile = find_first_desktop_file (appdesktopdir);
    if (!desktopfile) {
      return FALSE; /* none desktop file not found */
    }

    appdesktopfile = g_build_filename (appdesktopdir, desktopfile, NULL);
  }

  g_autofree char *exec = app_info_get_executable (appdesktopfile);

  /* 1. It is an absolute path */
  if (g_path_is_absolute (exec)) {
    if (g_file_test (exec, G_FILE_TEST_EXISTS))
      return TRUE;

    /* @HACK: let's force the things a bit */
    char *tmp = g_path_get_basename (exec);
    if (tmp) {
      exec = tmp;
    } else {
      g_free (tmp);
      return FALSE;
    }
  }

  /* 2. Try in /endless/$appid/bin */
  g_autofree char *bin =
    g_build_filename (eam_config_appdir (),
                      appid,
                      eam_fs_get_bundle_system_dir (EAM_BUNDLE_DIRECTORY_BIN),
                      exec,
                      NULL);

  if (make_binary_symlink (bin, exec))
    return TRUE;

  /* 3. Try in /endless/$appid/games */
  g_free (bin);
  bin = g_build_filename (eam_config_appdir (),
                          appid,
                          "games",
                          exec,
                          NULL);

  if (make_binary_symlink (bin, exec))
    return TRUE;

  /* 4. Look if the command we are trying to link is already in $PATH
   *
   * We don't want to match binaries in ${OS_BIN_DIR} here, so remove it
   * from $PATH first
   */
  const gchar* path = g_getenv ("PATH");

  g_autofree char *safe_path = remove_dir_from_path (path, eam_fs_get_bundle_system_dir (EAM_BUNDLE_DIRECTORY_BIN));
  g_autofree char *full_exec = eam_utils_find_program_in_path (safe_path, exec);
  if (full_exec != NULL)
    return TRUE;

  return FALSE;
}

static int
rmsymlinks_recursive (const char *appid,
                      const char *dir)
{
  g_autoptr(GDir) dp = g_dir_open (dir, 0, NULL);
  if (dp == NULL)
    return -1;

  int ret = 0;

  g_autofree char *path = NULL;
  const char *fn;

  while ((fn = g_dir_read_name (dp)) != NULL) {
    if (is_dot_or_dotdot (fn))
      continue;

    path = g_build_filename (dir, fn, NULL);

    struct stat st;
    if (lstat (path, &st)) {
      if (errno == ENOENT)
        continue;
    }
    else if (S_ISLNK (st.st_mode)) {
      g_autofree char *file = g_file_read_link (path, NULL);
      if (file == NULL) {
        continue;
      }

      if (strstr (file, appid) == NULL) { /* does it belong to appid? */
        continue;
      }

      if (!unlink (path) || errno != ENOENT) /* remove link */
        continue;

      eam_log_error_message ("Couldn't remove link %s: %s", path, strerror (errno));
    }
    else if (S_ISDIR (st.st_mode)) { /* recursive if directory */
      if (!rmsymlinks_recursive (appid, path)) {
        if (!rmdir (path) || errno != ENOTEMPTY) /* remove directory if empty */
          continue;
      }
    }
    else { /* ignore the rest */
      continue;
    }

    /* error */
    ret = -1;
    break;
  }

  return ret;
}


static int
rmsymlinks (const char *prefix,
            const char *appid,
            EamBundleDirectory dir)
{
  g_autofree char *tdir = g_build_filename (prefix, eam_fs_get_bundle_system_dir (dir), NULL);

  return rmsymlinks_recursive (appid, tdir);
}

gboolean
eam_fs_create_symlinks (const char *prefix,
                        const char *appid)
{
  if (do_binaries_symlinks (appid) < 0) {
    /* If command is neither in $PATH nor in one of the binary
     *  directories of the bundle, exit with error exit_error
     */
    g_warning ("Can't find app binary to link");

    rmsymlinks (prefix, appid, EAM_BUNDLE_DIRECTORY_BIN);

    return FALSE;
  }

  int ret = 0;
  for (guint i = 0; i < EAM_BUNDLE_DIRECTORY_MAX; i++) {
    g_autofree char *sdir = g_build_filename (prefix, appid, eam_fs_get_bundle_system_dir (i), NULL);
    g_autofree char *tdir = g_build_filename (prefix, eam_fs_get_bundle_system_dir (i), NULL);

    /* shallow symlinks to EKN data */
    gboolean is_shallow = i == EAM_BUNDLE_DIRECTORY_EKN_DATA;
    ret = symlinkdirs_recursive (sdir, tdir, is_shallow);
    if (ret != 0) {
      rmsymlinks (prefix, appid, i);
      break;
    }
  }

  return ret == 0;
}

void
eam_fs_prune_symlinks (const char *prefix,
                       const char *appid)
{
  for (guint i = 0; i < EAM_BUNDLE_DIRECTORY_MAX; i++) {
    (void) rmsymlinks (prefix, appid, i);
  }
}

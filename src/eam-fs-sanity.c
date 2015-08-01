/* Copyright 2014 Endless Mobile, Inc. */
#include "config.h"

#include "eam-fs-sanity.h"
#include "eam-config.h"
#include "eam-log.h"
#include "eam-utils.h"
#include "eam-error.h"

#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>

#include <gio/gio.h>
#include <glib/gstdio.h>

#define BIN_SUBDIR "bin"
#define DESKTOP_FILES_SUBDIR "share/applications"
#define DESKTOP_ICONS_SUBDIR "share/icons"
#define DBUS_SERVICES_SUBDIR "share/dbus-1/services"
#define EKN_DATA_SUBDIR "share/ekn/data"
#define G_SCHEMAS_SUBDIR "share/glib-2.0/schemas"
#define XDG_AUTOSTART_SUBDIR "xdg/autostart"

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
  [EAM_BUNDLE_DIRECTORY_GAMES] = "games",
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
  const char *app_dir = eam_config_get_applications_dir ();
  return g_build_filename (app_dir, eam_fs_get_bundle_system_dir (dir), NULL);
}

gboolean
eam_fs_init_bundle_dir (EamBundleDirectory dir,
                        GError **error)
{
  g_autofree char *path = get_bundle_path (dir);

  if (g_mkdir_with_parents (path, 0777) != 0) {
    g_set_error (error, EAM_ERROR, EAM_ERROR_FAILED,
                 "Unable to create bundle directory '%s' in '%s'",
                 eam_fs_get_bundle_system_dir (dir),
                 path);
    return FALSE;
  }

  struct passwd *pw = getpwnam (EAM_USER_NAME);
  g_assert (pw != NULL);

  int r;
  do {
    r = chown (path, pw->pw_uid, pw->pw_gid);
  } while (r != 0 && errno == EINTR);

  if (r != 0) {
    g_set_error (error, EAM_ERROR, EAM_ERROR_FAILED,
                 "Unable to assign ownership of '%s' to the app manager user: %s",
                 eam_fs_get_bundle_system_dir (dir),
                 g_strerror (errno));
    return FALSE;
  }

  return TRUE;
}

static gboolean
applications_directory_create (void)
{
  for (guint i = 0; i < EAM_BUNDLE_DIRECTORY_MAX; i++) {
    g_autoptr(GError) error = NULL;
    eam_fs_init_bundle_dir (i, &error);
    if (error != NULL) {
      eam_log_error_message ("%s", error->message);
      return FALSE;
    }
  }

  return TRUE;
}

gboolean
eam_fs_is_app_dir (const char *path)
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

  if (stat (path, &buf) != 0) {
    eam_log_error_message ("Error retrieving information about path '%s'", path);
    retval = FALSE;
    goto bail;
  }

  /* It's impossible to know what the desired permissions are for each file,
   * so we check 'r-x' permissions for "owner" and apply them to "other".
   */
  mode_t mod_mask = buf.st_mode;
  mod_mask |= (((buf.st_mode & S_IRUSR) | (buf.st_mode & S_IXUSR)) >> 6);

  if (chmod (path, mod_mask) != 0) {
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

  if (stat (path, &buf) != 0) {
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
 * @prefix: the installation prefix
 *
 * Guarantees the existance of the applications' root installation directory and the
 * subdirectories required by the Application Manager to work inside @prefix.
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
  const char *app_dir = eam_config_get_applications_dir ();

  /* Ensure the applications installation directory exists */
  if (!applications_directory_create ()) {
    eam_log_error_message ("Failed to create the applications directory '%s'", app_dir);
    return FALSE;
  }

  /* Check if the existing applications directory structure is correct */
  gchar *bin_dir = g_build_filename (app_dir, BIN_SUBDIR, NULL);
  gchar *desktop_files_dir = g_build_filename (app_dir, DESKTOP_FILES_SUBDIR, NULL);
  gchar *desktop_icons_dir = g_build_filename (app_dir, DESKTOP_ICONS_SUBDIR, NULL);
  gchar *dbus_services_dir = g_build_filename (app_dir, DBUS_SERVICES_SUBDIR, NULL);
  gchar *ekn_data_dir = g_build_filename (app_dir, EKN_DATA_SUBDIR, NULL);
  gchar *g_schemas_dir = g_build_filename (app_dir, G_SCHEMAS_SUBDIR, NULL);
  gchar *xdg_autostart_dir = g_build_filename (app_dir, XDG_AUTOSTART_SUBDIR, NULL);

  if (!g_file_test (app_dir, G_FILE_TEST_IS_DIR)) {
    eam_log_error_message ("Missing directory: '%s' does not exist", app_dir);
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
  GFile *file = g_file_new_for_path (app_dir);
  GFileEnumerator *children = g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                         G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                         NULL, &error);
  if (error) {
    eam_log_error_message ("Failed to get the children of '%s': %s", app_dir, error->message);
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

    if (eam_fs_is_app_dir (path))
      fix_application_permissions_if_needed (path);

    g_free (path);
  }

  if (error) {
    eam_log_error_message ("Failure while processing the children of '%s': %s", app_dir, error->message);
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

  const char *fn;

  while ((fn = g_dir_read_name (dir)) != NULL) {
    g_autofree char *epath = g_build_filename (path, fn, NULL);

    struct stat st;
    if (lstat (epath, &st) != 0) {
      /* Bail out, unless the file disappeared */
      if (errno != ENOENT)
        return FALSE;

      continue;
    }

    if (S_ISDIR (st.st_mode)) {
      if (!eam_fs_rmdir_recursive (epath))
        return FALSE;

      continue;
    }
    else {
      if (unlink (epath) != 0) {
        /* Bail out, unless the file disappeared */
        if (errno != ENOENT)
          return FALSE;
      }
    }
  }

  /* The directory at path should now be empty, or not exist */
  if (rmdir (path) == 0)
    return TRUE;
  else {
    if (errno == ENOENT)
      return TRUE;
  }

  return FALSE;
}

gboolean
eam_fs_prune_dir (const char *prefix,
                  const char *appdir)
{
  g_autofree char *path = g_build_filename (prefix, appdir, NULL);

  return eam_fs_rmdir_recursive (path);
}

#define CP_ENUMERATE_ATTRS \
  G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
  G_FILE_ATTRIBUTE_STANDARD_NAME "," \
  G_FILE_ATTRIBUTE_UNIX_UID "," \
  G_FILE_ATTRIBUTE_UNIX_GID "," \
  G_FILE_ATTRIBUTE_UNIX_MODE

#define CP_QUERY_ATTRS \
  G_FILE_ATTRIBUTE_STANDARD_NAME "," \
  G_FILE_ATTRIBUTE_UNIX_MODE "," \
  G_FILE_ATTRIBUTE_UNIX_UID "," \
  G_FILE_ATTRIBUTE_UNIX_GID "," \
  G_FILE_ATTRIBUTE_TIME_MODIFIED "," \
  G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC "," \
  G_FILE_ATTRIBUTE_TIME_ACCESS "," \
  G_FILE_ATTRIBUTE_TIME_ACCESS_USEC

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
  } while (r != 0 && errno == EINTR);

  if (r != 0) {
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
    guint32 src_uid = g_file_info_get_attribute_uint32 (source_info, G_FILE_ATTRIBUTE_UNIX_UID);
    guint32 src_gid = g_file_info_get_attribute_uint32 (source_info, G_FILE_ATTRIBUTE_UNIX_GID);
    r = fchown (target_fd, src_uid, src_gid);
  } while (r != 0 && errno == EINTR);

  if (r != 0) {
    eam_log_error_message ("Unable to set ownership of target directory: %s",
                           g_strerror (errno));
    return FALSE;
  }

  do {
    guint32 src_mode = g_file_info_get_attribute_uint32 (source_info, G_FILE_ATTRIBUTE_UNIX_MODE);
    r = fchmod (target_fd, src_mode);
  } while (r != 0 && errno == EINTR);

  if (r != 0) {
    eam_log_error_message ("Unable to set mode of target directory: %s", g_strerror (errno));
    return FALSE;
  }

  (void) close (target_fd);

  while (TRUE) {
    GFileInfo *file_info = NULL;
    GFile *source_child = NULL;

    if (!g_file_enumerator_iterate (enumerator, &file_info, &source_child, NULL, &error)) {
      eam_log_error_message ("Unable to enumerate source: %s", error->message);
      return FALSE;
    }

    if (file_info == NULL)
      break;

    g_autoptr(GFile) target_child = g_file_get_child (target, g_file_info_get_name (file_info));

    if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY) {
      if (!cp_internal (source_child, target_child)) {
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

  if (rename (sdir, tdir) != 0) {
    /* If the rename() failed because we tried to move across
     * file system boundaries, then we do an explicit recursive
     * copy.
     */
    if (errno == EXDEV)
      ret = eam_fs_cpdir_recursive (sdir, tdir);
  } else {
    ret = TRUE;
  }

  if (!ret) {
    eam_log_error_message ("Moving '%s' from '%s' to '%s' failed", appid, sdir, tdir);
    eam_fs_rmdir_recursive (tdir); /* clean up the appdir */
  }

  eam_fs_rmdir_recursive (sdir);

  return ret;
}

static gboolean
create_symlink (const char *source,
                const char *target)
{
  /* Try removing the link manually first if we had leftover junk from last
   * install
   */
  if (faccessat (0, target, F_OK, AT_SYMLINK_NOFOLLOW | AT_EACCESS) == F_OK &&
      unlink(target) == 0)
    {
      eam_log_error_message ("Doing forced cleanup of link: %s!",
                             target);
    }

  if (symlink (source, target) != 0) {
    eam_log_error_message ("Error while creating link from '%s' to '%s': %s",
                           source,
                           target,
                           g_strerror (errno));
    return FALSE;
  }

  return TRUE;
}

static gboolean
symlinkdirs_recursive (const char *source_dir,
                       const char *target_dir,
                       gboolean    shallow)
{
  if (g_mkdir_with_parents (target_dir, 0755) != 0)
    return FALSE;

  g_autoptr(GDir) dir = g_dir_open (source_dir, 0, NULL);
  if (dir == NULL)
    return TRUE; /* it's OK if the bundle doesn't have that dir */

  const char *fn;
  while ((fn = g_dir_read_name (dir)) != NULL) {
    g_autofree char *spath = g_build_filename (source_dir, fn, NULL);
    g_autofree char *tpath = g_build_filename (target_dir, fn, NULL);

    struct stat st;
    if (lstat (spath, &st) != 0)
      return FALSE;

    if (S_ISLNK (st.st_mode) || S_ISREG (st.st_mode)) {
      if (!create_symlink (spath, tpath))
        return FALSE;
    }
    else if (S_ISDIR (st.st_mode)) {
      /* recursive if directory and not shallow */
      if (!shallow) {
        /* If symlinkdirs_recursive() fails, we fail the whole operation */
        if (!symlinkdirs_recursive (spath, tpath, FALSE))
          return FALSE;
      }
      else {
        if (!create_symlink (spath, tpath))
          return FALSE;
      }
    }
  }

  return TRUE;
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
    if (g_strcmp0 (splitpath[i], dir) != 0)
      continue;

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

  const char *app_dir = eam_config_get_applications_dir ();
  g_autofree char *path = g_build_filename (app_dir,
                                            eam_fs_get_bundle_system_dir (EAM_BUNDLE_DIRECTORY_BIN),
                                            exec,
                                            NULL);

  return create_symlink (bin, path);
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

static char *
ensure_desktop_file (const char *dir,
                     const char *file)
{
  g_autofree char *path = g_build_filename (dir, file, NULL);

  if (g_file_test (path, G_FILE_TEST_EXISTS))
    return g_strdup (path);

  g_autoptr(GDir) dp = g_dir_open (dir, 0, NULL);
  if (dp == NULL)
    return NULL;

  const char *fn;
  while ((fn = g_dir_read_name (dp)) != NULL) {
    if (g_str_has_suffix (fn, ".desktop"))
      return g_build_filename (dir, fn, NULL);
  }

  return NULL;
}

static gboolean
do_binaries_symlinks (const char *prefix,
                      const char *appid)
{
  g_autofree char *desktopfile = g_strdup_printf ("%s.desktop", appid);
  g_autofree char *appdesktopdir = g_build_filename (prefix,
                                                     appid,
                                                     eam_fs_get_bundle_system_dir (EAM_BUNDLE_DIRECTORY_DESKTOP),
                                                     NULL);
  g_autofree char *appdesktopfile = ensure_desktop_file (appdesktopdir, desktopfile);

  if (appdesktopfile == NULL) {
    eam_log_error_message ("Could not find .desktop file for app '%s'", appid);
    return FALSE;
  }

  g_autofree char *exec = app_info_get_executable (appdesktopfile);

  /* 1. It is an absolute path, we don't do anything */
  if (g_path_is_absolute (exec))
    return g_file_test (exec, G_FILE_TEST_EXISTS);

  /* 2. Try in /endless/$appid/bin */
  g_autofree char *bin =
    g_build_filename (prefix,
                      appid,
                      eam_fs_get_bundle_system_dir (EAM_BUNDLE_DIRECTORY_BIN),
                      exec,
                      NULL);

  if (make_binary_symlink (bin, exec))
    return TRUE;

  /* 3. Try in /endless/$appid/games */
  g_free (bin);
  bin = g_build_filename (prefix,
                          appid,
                          eam_fs_get_bundle_system_dir (EAM_BUNDLE_DIRECTORY_GAMES),
                          exec,
                          NULL);

  if (make_binary_symlink (bin, exec))
    return TRUE;

  /* 4. Look if the command we are trying to link is already in $PATH
   *
   * We don't want to match binaries in EAM_BUNDLE_DIRECTORY_BIN here, so
   * remove it from $PATH first.
   */
  const gchar* path = g_getenv ("PATH");

  g_autofree char *bindir =
    g_build_filename (prefix,
                      eam_fs_get_bundle_system_dir (EAM_BUNDLE_DIRECTORY_BIN),
                      NULL);
  g_autofree char *safe_path = remove_dir_from_path (path, bindir);
  g_autofree char *full_exec = eam_utils_find_program_in_path (exec, safe_path);
  if (full_exec != NULL)
    return TRUE;

  eam_log_error_message ("Could not find binary for %s", appid);

  return FALSE;
}

static gboolean
rmsymlinks_recursive (const char *source_dir,
                      const char *target_dir)
{
  g_autoptr(GDir) dp = g_dir_open (source_dir, 0, NULL);
  if (dp == NULL)
    return FALSE;

  const char *fn;

  while ((fn = g_dir_read_name (dp)) != NULL) {
    g_autofree char *spath = g_build_filename (source_dir, fn, NULL);
    g_autofree char *tpath = g_build_filename (target_dir, fn, NULL);

    struct stat st;
    if (lstat (tpath, &st) != 0) {
      /* Bail out, unless the file disappeared */
      if (errno != ENOENT)
        return FALSE;

      continue;
    }

    /* If the file is a link, we remove it */
    if (S_ISLNK (st.st_mode)) {
      g_autofree char *file = g_file_read_link (tpath, NULL);
      if (file == NULL)
        continue;

      /* Remove link unless the file was removed */
      if (unlink (tpath) != 0) {
        /* Bail out, unless the file disappeared */
        if (errno != ENOENT)
          return FALSE;
      }

      continue;
    }
    else if (S_ISDIR (st.st_mode)) {
      /* If the file is a directory, we recurse into it */
      if (!rmsymlinks_recursive (spath, tpath))
        return FALSE;

      /* Try to cleanup empty directories that had links.
       * We intentionally ignore errors.
       */
      rmdir (tpath);
    }
  }

  return TRUE;
}

gboolean
eam_fs_create_symlinks (const char *prefix,
                        const char *appid)
{
  if (!do_binaries_symlinks (prefix, appid)) {
    eam_log_error_message ("Could not create binaries symlinks for app '%s'", appid);
    return FALSE;
  }

  for (guint index = 0; index < EAM_BUNDLE_DIRECTORY_MAX; index++) {
    if (index == EAM_BUNDLE_DIRECTORY_BIN)
      continue;

    const char *sysdir = eam_fs_get_bundle_system_dir (index);

    g_autofree char *sdir = g_build_filename (prefix, appid, sysdir, NULL);
    g_autofree char *tdir = get_bundle_path (index);

    /* shallow symlinks to EKN data */
    gboolean is_shallow = (index == EAM_BUNDLE_DIRECTORY_EKN_DATA);
    if (!symlinkdirs_recursive (sdir, tdir, is_shallow)) {
      return FALSE;
    }
  }

  /* Symlink from e.g. /var/endless/com.endlessm.youvideos to
   * /endless/com.endlessm.youvideos */
  const char *app_dir = eam_config_get_applications_dir ();
  g_autofree char *idir = g_build_filename (prefix, appid, NULL);
  g_autofree char *adir = g_build_filename (app_dir, appid, NULL);
  if (!create_symlink (idir, adir))
    return FALSE;

  return TRUE;
}

void
eam_fs_prune_symlinks (const char *prefix,
                       const char *appid)
{
  const char *app_dir = eam_config_get_applications_dir ();

  for (guint index = 0; index < EAM_BUNDLE_DIRECTORY_MAX; index++) {
    const char *sysdir = eam_fs_get_bundle_system_dir (index);

    g_autofree char *sdir = g_build_filename (prefix, appid, sysdir, NULL);
    g_autofree char *tdir = get_bundle_path (index);

    (void) rmsymlinks_recursive (sdir, tdir);
  }

  /* As a special case, for apps that normally install in /usr/games in their
   * bundle, we need to remove the corresponding link we made in /usr/bin. */
  g_autofree char *sdir = g_build_filename (prefix, appid, eam_fs_get_bundle_system_dir (EAM_BUNDLE_DIRECTORY_GAMES), NULL);
  g_autofree char *tdir = g_build_filename (app_dir, eam_fs_get_bundle_system_dir (EAM_BUNDLE_DIRECTORY_BIN), NULL);
  (void) rmsymlinks_recursive (sdir, tdir);

  /* Unlink /endlessm/com.endlessm.youvideos */
  g_autofree char *adir = g_build_filename (app_dir, appid, NULL);
  (void) unlink (adir);
}

gboolean
eam_fs_backup_app (const char *prefix,
                   const char *appid,
                   char      **backup_dir)
{
  /* Remove symbolic links, to avoid recursion */
  eam_fs_prune_symlinks (prefix, appid);

  g_autofree char *backup_id = g_strconcat (appid, ".backup", NULL);

  g_autofree char *sdir = g_build_filename (prefix, appid, NULL);
  g_autofree char *tdir = g_build_filename (prefix, backup_id, NULL);

  /* We try renaming the directory first */
  if (rename (sdir, tdir) == 0) {
    *backup_dir = g_strdup (tdir);
    return TRUE;
  }

  if (errno != EXDEV)
    return FALSE;

  /* If the rename failed because we tried to cross device boundaries,
   * we fall back to a recursive copy, and then we delete the source
   * directory
   */
  if (eam_fs_cpdir_recursive (sdir, tdir)) {
    eam_fs_rmdir_recursive (sdir);
    *backup_dir = g_strdup (tdir);
    return TRUE;
  }

  eam_log_error_message ("Backing up '%s' from '%s' to '%s' failed", appid, sdir, tdir);

  eam_fs_rmdir_recursive (tdir);

  /* Restore symbolic links */
  eam_fs_create_symlinks (prefix, appid);

  return FALSE;
}

gboolean
eam_fs_restore_app (const char *prefix,
                    const char *appid,
                    const char *backup_dir)
{
  g_autofree char *tdir = g_build_filename (prefix, appid, NULL);

  if (rename (backup_dir, tdir) == 0) {
    eam_fs_create_symlinks (prefix, appid);
    return TRUE;
  }

  if (errno != EXDEV)
    return FALSE;

  if (eam_fs_cpdir_recursive (backup_dir, tdir)) {
    eam_fs_rmdir_recursive (backup_dir);
    eam_fs_create_symlinks (prefix, appid);
    return TRUE;
  }

  return FALSE;
}

static gboolean
eam_fs_ensure_symlink_farm_for_prefix (const char *prefix)
{
  gboolean ret = TRUE;

  if (!eam_fs_sanity_check ())
    return FALSE;

  g_autoptr(GDir) dir = g_dir_open (prefix, 0, NULL);
  if (!dir)
    return ret;

  const char *fn;

  while ((fn = g_dir_read_name (dir)) != NULL) {
    g_autofree char *epath = g_build_filename (prefix, fn, NULL);

    if (!eam_fs_is_app_dir (epath))
      continue;

    ret = eam_fs_create_symlinks (prefix, fn) && ret;
  }

  return ret;
}

gboolean
eam_fs_ensure_symlink_farm (void)
{
  gboolean ret = TRUE;

  ret = eam_fs_ensure_symlink_farm_for_prefix (eam_config_get_primary_storage ()) && ret;
  ret = eam_fs_ensure_symlink_farm_for_prefix (eam_config_get_secondary_storage ()) && ret;

  return ret;
}

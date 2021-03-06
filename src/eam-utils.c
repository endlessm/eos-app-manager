/* eam-utils.c: Utility API
 *
 * This file is part of eos-app-manager.
 * Copyright 2014  Endless Mobile Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <archive.h>
#include <archive_entry.h>
#include <errno.h>
#include <ftw.h>
#include <pwd.h>
#include <grp.h>

#include <libsoup/soup.h>
#include <gio/gio.h>

#include "eam-utils.h"

#include "eam-config.h"
#include "eam-error.h"
#include "eam-fs-utils.h"
#include "eam-log.h"

#define BUNDLE_SIGNATURE_EXT ".asc"

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FILE, fclose)

#define BLOCKSIZE 32768

static gboolean
verify_checksum_hash (const char    *source_file,
                      const char    *checksum_str,
                      gssize         checksum_len,
                      GChecksumType  checksum_type)
{
  if (checksum_len < 0)
    checksum_len = strlen (checksum_str);

  gssize hash_len = g_checksum_type_get_length (checksum_type) * 2;
  if (checksum_len < hash_len)
    return FALSE;

  g_autoptr(FILE) fp = fopen (source_file, "r");
  if (fp == NULL)
    return FALSE;

  g_autoptr(GChecksum) checksum = g_checksum_new (checksum_type);

  guint8 buffer[BLOCKSIZE];
  while (1) {
    size_t n = fread (buffer, 1, BLOCKSIZE, fp);
    if (n > 0) {
      g_checksum_update (checksum, buffer, n);
      continue;
    }

    if (feof (fp))
      break;

    if (ferror (fp))
      return FALSE;
  }

  const char *hash = g_checksum_get_string (checksum);

  return (g_ascii_strncasecmp (checksum_str, hash, hash_len) == 0);
}

static gboolean
run_cmd (const char * const *argv,
         GCancellable *cancellable)
{
  g_autoptr(GError) err = NULL;
  g_autoptr(GSubprocess) sub = g_subprocess_newv (argv, G_SUBPROCESS_FLAGS_STDOUT_SILENCE, &err);

  if (err != NULL) {
    eam_log_error_message ("%s failed: %s", argv[0], err->message);
    return FALSE;
  }

  g_subprocess_wait (sub, cancellable, &err);
  if (err != NULL) {
    eam_log_error_message ("%s failed: %s", argv[0], err->message);
    return FALSE;
  }

  return g_subprocess_get_successful (sub);
}

gboolean
eam_utils_verify_signature (const char *source_file,
                            const char *signature_file,
                            GCancellable *cancellable)
{
  const char *gpgdir = eam_config_get_gpg_keyring ();

  char *argv[9] = {
    "gpgv",
    "--keyring",
    (char *) gpgdir,
    "--logger-fd",
    "1",
    "--quiet",
    (char *) signature_file,
    (char *) source_file,
    NULL
  };

  return run_cmd ((const char * const *) argv, cancellable);
}

GKeyFile *
eam_utils_load_app_info (const char *prefix,
                         const char *appid)
{
  g_autofree char *appdir = g_build_filename (prefix, appid, NULL);
  g_autofree char *info_file = g_build_filename (appdir, ".info", NULL);

  GKeyFile *keyfile = g_key_file_new ();
  if (!g_key_file_load_from_file (keyfile, info_file, G_KEY_FILE_NONE, NULL)) {
    g_key_file_unref (keyfile);
    return NULL;
  }

  return keyfile;
}

gboolean
eam_utils_app_is_installed (const char *prefix,
                            const char *appid)
{
  g_autofree char *appdir = g_build_filename (prefix, appid, NULL);

  if (!g_file_test (appdir, G_FILE_TEST_IS_DIR)) {
    eam_log_error_message ("Unable to inspect path '%s': not a directory", appdir);
    return FALSE;
  }

  g_autofree char *info_file = g_build_filename (appdir, ".info", NULL);

  g_autoptr(GError) error = NULL;
  g_autoptr(GKeyFile) keyfile = g_key_file_new ();
  g_key_file_load_from_file (keyfile, info_file, G_KEY_FILE_NONE, &error);

  if (error != NULL) {
    eam_log_error_message ("Unable to open bundle metadata for '%s': %s",
                           appid,
                           error->message);
    return FALSE;
  }

  g_autofree char *info_appid = g_key_file_get_string (keyfile, "Bundle", "app_id", &error);
  if (error != NULL) {
    eam_log_error_message ("Invalid bundle metadata for '%s': %s",
                           appid,
                           error->message);
    return FALSE;
  }

  if (g_strcmp0 (info_appid, appid) != 0) {
    eam_log_error_message ("Invalid bundle metadata for '%s': unexpected app id '%s' found",
                           appid,
                           info_appid);
    return FALSE;
  }

  return TRUE;
}

static int
copy_data (struct archive *ar,
           struct archive *aw)
{
  const void *buff;
  size_t size;
  off_t offset;

  while (TRUE) {
    int err = archive_read_data_block (ar, &buff, &size, &offset);

    if (err == ARCHIVE_EOF)
      return ARCHIVE_OK;

    if (err != ARCHIVE_OK)
      return err;

    err = archive_write_data_block (aw, buff, size, offset);
    if (err != ARCHIVE_OK)
      return err;
  }

  g_assert_not_reached ();
}

gboolean
eam_utils_bundle_extract (const char *bundle_file,
                          const char *target_prefix,
                          const char *appid,
                          GCancellable *cancellable)
{
#define READ_ARCHIVE_BLOCK_SIZE      8192

  gboolean ret = FALSE;

  struct archive *a = archive_read_new ();
  archive_read_support_filter_all (a);
  archive_read_support_format_all (a);

  struct archive *ext = archive_write_disk_new ();
  int flags = ARCHIVE_EXTRACT_TIME
            | ARCHIVE_EXTRACT_PERM
            | ARCHIVE_EXTRACT_ACL
            | ARCHIVE_EXTRACT_FFLAGS;
  archive_write_disk_set_options (ext, flags);
  archive_write_disk_set_standard_lookup (ext);

  int err = archive_read_open_filename (a, bundle_file, READ_ARCHIVE_BLOCK_SIZE);
  if (err != ARCHIVE_OK)
    goto bail;

  while (TRUE) {
    struct archive_entry *entry;

    /* This is not a libarchive error condition, so we reset the error code
     * and log the cancellation here
     */
    if (g_cancellable_is_cancelled (cancellable)) {
      err = ARCHIVE_OK;
      ret = FALSE;

      eam_log_error_message ("Extracting bundle was cancelled");
      goto bail;
    }

    err = archive_read_next_header (a, &entry);

    if (err == ARCHIVE_EOF) {
      err = ARCHIVE_OK;
      break;
    }

    if (err != ARCHIVE_OK)
      goto bail;

    const char *entpath = archive_entry_pathname (entry);
    g_autofree char *newpath = g_build_filename (target_prefix, entpath, NULL);

    eam_log_info_message ("Extracting '%s' to '%s", entpath, newpath);
    archive_entry_copy_pathname (entry, newpath);

    err = archive_write_header (ext, entry);
    if (err == ARCHIVE_OK && archive_entry_size (entry) > 0) {
      err = copy_data (a, ext);
      if (err != ARCHIVE_OK)
        goto bail;
    }

    err = archive_write_finish_entry (ext);
    if (err != ARCHIVE_OK)
      goto bail;
  }

  ret = TRUE;

bail:
  {
    char errstr[1024];
    memset (errstr, '\0', 1024);

    if (err != ARCHIVE_OK) {
      const gchar *str = archive_error_string (a);

      if (!str)
        str = archive_error_string (ext);

      if (str)
        memcpy (errstr, str, MIN (strlen (str), 1023));
    }

    if (err != ARCHIVE_OK)
      eam_log_error_message ("Unable to extract archive '%s': %s", bundle_file, errstr);
  }

  archive_read_free (a);
  archive_write_free (ext);

  return ret;

#undef READ_ARCHIVE_BLOCK_SIZE
}

static int
has_external_script (const char  *prefix,
                     const char  *appid,
                     char       **url,
                     char       **filename,
                     char       **digest)
{
  g_autofree char *info = g_build_filename (prefix, appid, ".info", NULL);
  g_autoptr(GKeyFile) kf = g_key_file_new ();

  g_autoptr(GError) err = NULL;
  if (!g_key_file_load_from_file (kf, info, G_KEY_FILE_NONE, &err)) {
    eam_log_error_message ("Could not load bundle info %s for app '%s': %s.", info, appid, err->message);
    return -1;
  }

  if (!g_key_file_has_group (kf, "External"))
    return 0;

  *url = g_key_file_get_string (kf, "External", "url", &err);
  if (err) {
    eam_log_error_message ("Could find 'url' key in %s: %s", info, err->message);
    return -1;
  }

  *filename = g_key_file_get_string (kf, "External", "filename", &err);
  if (err) {
    eam_log_error_message ("Could find 'filename' key in %s: %s", info, err->message);
    return -1;
  }

  *digest = g_key_file_get_string (kf, "External", "sha256sum", &err);
  if (err) {
    eam_log_error_message ("Could find 'sha256sum' key in %s: %s", info, err->message);
    return -1;
  }

  return 1;
}

static gboolean
download_external_file (const char *appid,
                        const char *url,
                        const char *dir,
                        const char *filename,
                        GCancellable *cancellable)
{
  g_autoptr(SoupURI) uri = soup_uri_new (url);
  if (!uri) {
    eam_log_error_message ("Could not parse '%s' as a URL", url);
    return FALSE;
  }

  g_autoptr(SoupSession) session =
    soup_session_new_with_options (SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_CONTENT_DECODER,
                                   SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_COOKIE_JAR,
                                   SOUP_SESSION_USER_AGENT, "EOS web get ",
                                   SOUP_SESSION_ACCEPT_LANGUAGE_AUTO, TRUE,
                                   NULL);

  g_autoptr(SoupMessage) msg = soup_message_new_from_uri ("GET", uri);

  g_autoptr(GError) err = NULL;
  g_autoptr(GInputStream) ins = soup_session_send (session, msg, cancellable, &err);
  if (err != NULL) {
    eam_log_error_message ("Couldn't download %s: %s", url, err->message);
    return FALSE;
  }

  goffset len = soup_message_headers_get_content_length (msg->response_headers);

  /* TODO: Verify that we don't have any strange data in filename */
  g_autofree char *path = g_build_filename (dir, filename, NULL);
  g_autoptr(GFile) file = g_file_new_for_path (path);
  g_autoptr(GOutputStream) outs =
    G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE,
                                     G_FILE_CREATE_PRIVATE | G_FILE_CREATE_REPLACE_DESTINATION,
                                     cancellable, &err));
  if (err != NULL) {
    eam_log_error_message ("Could not create %s: %s", path, err->message);
    return FALSE;
  }

  eam_log_info_message ("Downloading %s into %s", url, path);
  gssize siz = g_output_stream_splice (outs, ins,
                                       G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                       cancellable, &err);
  if (err != NULL) {
    eam_log_error_message ("Could not save %s: %s", path, err->message);
    return FALSE;
  }

  if (siz < 0 || siz > len) {
    eam_log_error_message ("Could not save %s: invalid size", path);
    return FALSE;
  }

  return TRUE;
}

static gboolean
run_external_script (const char *prefix,
                     const char *appid,
                     GCancellable *cancellable)
{
  g_autofree char *wd = g_build_filename (prefix, appid, NULL);
  g_autofree char *fn = g_build_filename (wd, ".script.install", NULL);

  if (!g_file_test (fn, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_EXECUTABLE)) {
    eam_log_error_message ("External script '%s' is not a valid executable", fn);
    return FALSE;
  }

  const char *argv[4] = {
    fn,
    appid,
    wd,
    NULL,
  };

  return run_cmd ((const char * const *) argv, cancellable);
}

gboolean
eam_utils_run_external_scripts (const char *prefix,
                                const char *appid,
                                GCancellable *cancellable)
{
  g_autofree char *url = NULL;
  g_autofree char *filename = NULL;
  g_autofree char *digest = NULL;

  int res = has_external_script (prefix, appid, &url, &filename, &digest);
  if (res == 0) {
    /* No external script */
    return TRUE;
  }
  else if (res < 0) {
    /* Error while looking for external scripts */
    return FALSE;
  }
  else {
    /* We need these to be set */
    g_assert (url != NULL && digest != NULL && filename != NULL);
  }

  g_autofree char *dir = g_build_filename (prefix, appid, "external", NULL);
  if (g_mkdir_with_parents (dir, 0755) < 0)
    return FALSE;

  if (!download_external_file (appid, url, dir, filename, cancellable)) {
    eam_fs_rmdir_recursive (dir);
    return FALSE;
  }

  g_autofree char *path = g_build_filename (dir, filename, NULL);
  if (!verify_checksum_hash (path, digest, -1, G_CHECKSUM_SHA256)) {
    eam_fs_rmdir_recursive (dir);
    return FALSE;
  }

  if (!run_external_script (prefix, appid, cancellable)) {
    eam_fs_rmdir_recursive (dir);
    return FALSE;
  }

  eam_fs_rmdir_recursive (dir);

  return TRUE;
}

gboolean
eam_utils_compile_python (const char *prefix,
                          const char *appid)
{
  static const char *sitedir[] = { "dist-packages", "site-packages" };
  const char *cmd[][7] = {
    { "python2", "-m", "compileall", "-f", "-q", NULL, NULL },
    { "python3", "-m", "compileall", "-f", "-q", NULL, NULL },
  };

  g_autofree char *dir = g_build_filename (prefix, appid, "lib", NULL);
  g_autoptr(GDir) dp = g_dir_open (dir, 0, NULL);
  if (!dp)
    return TRUE;

  gboolean compilation_successful = FALSE;
  gboolean found_python = FALSE;
  const char *fn;

  while ((fn = g_dir_read_name (dp)) != NULL) {
    if (!g_str_has_prefix (fn, "python"))
      continue;

    g_autofree char *pydir = g_build_filename (dir, fn, NULL);
    if (!g_file_test (pydir, G_FILE_TEST_IS_DIR))
      continue;

    found_python = TRUE;

    for (guint i = 0; i < G_N_ELEMENTS (sitedir); i++) {
      g_autofree char *pysubdir = g_build_filename (pydir, sitedir[i], NULL);
      guint idx = g_str_has_prefix (fn, "python3") ? 1 : 0;

      if (g_file_test (pysubdir, G_FILE_TEST_IS_DIR)) {
        cmd[idx][5] = pysubdir;

        /* All the commands we run are unrelated, so we only want to
         * be noticed if all of them failed, but we don't prevent each
         * other from running, and we consider the update a success if
         * at least one succeeded.
         */
        compilation_successful |= run_cmd (cmd[idx], NULL);
      }
    }
  }

  if (!found_python)
    return TRUE;

  if (!compilation_successful)
    return FALSE;

  return TRUE;
}

static gboolean
is_python_object_file (const char *path)
{
  return g_str_has_suffix (path, ".pyc") ||
         g_str_has_suffix (path, ".pyo");
}

static gboolean
is_python_cache_directory (const char *path)
{
  g_autofree char *base = g_path_get_basename (path);

  return g_strcmp0 (base, "__pycache__") == 0;
}

static int
rm_python_artifacts (const char *full_path,
                     const struct stat *statbuf,
                     int tflag,
                     struct FTW *ftwbuf)
{
  if (tflag == FTW_F && is_python_object_file (full_path))
    return unlink (full_path);

  if (tflag == FTW_D && is_python_cache_directory (full_path)) {
    if (!eam_fs_rmdir_recursive (full_path))
      return -1;
  }

  return 0;
}

gboolean
eam_utils_cleanup_python (const char *appdir)
{
  g_autofree char *dir = g_build_filename (appdir, "lib", NULL);

  if (!g_file_test (dir, G_FILE_TEST_IS_DIR))
    return TRUE;

  if (nftw (dir, rm_python_artifacts, 64, FTW_DEPTH | FTW_PHYS) == 0)
    return TRUE;

  return FALSE;
}

gboolean
eam_utils_update_desktop (void)
{
  const char *app_dir = eam_config_get_applications_dir ();

  g_autofree char *settingsdir =
    g_build_filename (app_dir, eam_fs_get_bundle_system_dir (EAM_BUNDLE_DIRECTORY_GSETTINGS_SCHEMAS), NULL);
  g_autofree char *iconsdir =
    g_build_filename (app_dir, eam_fs_get_bundle_system_dir (EAM_BUNDLE_DIRECTORY_ICONS), NULL);
  g_autofree char *desktopdir =
    g_build_filename (app_dir, eam_fs_get_bundle_system_dir (EAM_BUNDLE_DIRECTORY_DESKTOP), NULL);

  const char *cmd[][4] = {
    { "glib-compile-schemas", settingsdir, NULL, NULL },
    { "gtk-update-icon-cache-3.0", "--ignore-theme-index", iconsdir, NULL },
    { "update-desktop-database", desktopdir, NULL, NULL },
  };

  gboolean res = FALSE;
  for (int i = 0; i < G_N_ELEMENTS (cmd); i++) {
    /* All the commands we run are unrelated, so we only want to
     * be noticed if all of them failed, but we don't prevent each
     * other from running, and we consider the update a success if
     * at least one succeeded.
     */
    res |= run_cmd (cmd[i], NULL);
  }

  return res;
}

gboolean
eam_utils_apply_xdelta (const char *source_dir,
                        const char *appid,
                        const char *delta_bundle,
                        GCancellable *cancellable)
{
  g_autofree char *target_dir = g_build_filename (eam_config_get_cache_dir (), appid,
                                                  NULL);

  if (!eam_fs_rmdir_recursive (target_dir))
    return FALSE;

  if (g_mkdir (target_dir, 0755) < 0)
    return FALSE;

  const char *cmd[] = {
    "xdelta3-dir-patcher", "apply",
    "--ignore-euid",
    "-d",
    appid,
    source_dir,
    delta_bundle,
    target_dir,
    NULL,
  };

  gboolean res = run_cmd (cmd, cancellable);

  return res;
}

char *
eam_utils_find_program_in_path (const char *program,
                                const char *path)
{
  if (path == NULL)
    return g_find_program_in_path (program);

  if (g_path_is_absolute (program) ||
      strchr (program, G_DIR_SEPARATOR) != NULL) {
    if (g_file_test (program, G_FILE_TEST_IS_EXECUTABLE) &&
        !g_file_test (program, G_FILE_TEST_IS_DIR))
      return g_strdup (program);
    else
      return NULL;
  }

  /* This is a non-portable, naive version of g_find_program_in_path()
   * that does not require us to modify the PATH environment variable.
   */
  g_auto(GStrv) pathv = g_strsplit (path, G_SEARCHPATH_SEPARATOR_S, -1);
  int pathv_len = g_strv_length (pathv);
  for (int i = 0; i < pathv_len; i++) {
    const char *path_c = pathv[i];

    char *res = g_build_filename (path_c, program, NULL);
    if (g_file_test (res, G_FILE_TEST_IS_EXECUTABLE) &&
        !g_file_test (res, G_FILE_TEST_IS_DIR))
      return res;

    g_free (res);
  }

  return NULL;
}

gboolean
eam_utils_can_touch_applications_dir (uid_t user)
{
  if (user == G_MAXUINT)
    return FALSE;

  struct passwd *pw = getpwuid (user);
  if (pw == NULL)
    return FALSE;

  /* Are we root? */
  if (pw->pw_uid == 0 && pw->pw_gid == 0)
    return TRUE;

  /* Are we the app manager user? */
  if (g_strcmp0 (pw->pw_name, EAM_USER_NAME) == 0)
    return TRUE;

  return FALSE;
}

gboolean
eam_utils_can_modify_configuration (uid_t user)
{
  if (user == G_MAXUINT)
    return FALSE;

  struct passwd *pw = getpwuid (user);
  if (pw == NULL)
    return FALSE;

  /* Are we root? */
  if (pw->pw_uid == 0 && pw->pw_gid == 0)
    return TRUE;

  return FALSE;
}

gboolean
eam_utils_check_unix_permissions (uid_t user)
{
  if (user == G_MAXUINT)
    return FALSE;

  struct passwd *pw = getpwuid (user);
  if (pw == NULL)
    return FALSE;

  if (eam_utils_can_touch_applications_dir (user))
    return TRUE;

  /* Are we in the admin group? */
  int n_groups = 10;
  gid_t *groups = g_new0 (gid_t, n_groups);

  while (1)
    {
      int max_n_groups = n_groups;
      int ret = getgrouplist (pw->pw_name, pw->pw_gid, groups, &max_n_groups);

      if (ret >= 0)
        {
          n_groups = max_n_groups;
          break;
        }

      /* some systems fail to update n_groups so we just grow it by approximation */
      if (n_groups == max_n_groups)
        n_groups = 2 * max_n_groups;
      else
        n_groups = max_n_groups;

      groups = g_renew (gid_t, groups, n_groups);
    }

  gboolean retval = FALSE;
  for (int i = 0; i < n_groups; i++)
    {
      struct group *gr = getgrgid (groups[i]);

      if (gr != NULL && g_strcmp0 (gr->gr_name, EAM_ADMIN_GROUP_NAME) == 0)
        {
          retval = TRUE;
          break;
        }
    }

  g_free (groups);

  return retval;
}

const char *
eam_utils_storage_type_to_path (const char *storage_type)
{
  if (g_strcmp0 (storage_type, "primary") == 0)
    return eam_config_get_primary_storage ();

  if (g_strcmp0 (storage_type, "secondary") == 0)
    return eam_config_get_secondary_storage ();

  return NULL;
}

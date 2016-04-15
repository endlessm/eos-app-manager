/* eam-fs-utils.h: File system utility API
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

#pragma once
#include <gio/gio.h>

G_BEGIN_DECLS

typedef enum {
  EAM_BUNDLE_DIRECTORY_BIN,
  EAM_BUNDLE_DIRECTORY_DESKTOP,
  EAM_BUNDLE_DIRECTORY_ICONS,
  EAM_BUNDLE_DIRECTORY_DBUS_SERVICES,
  EAM_BUNDLE_DIRECTORY_GSETTINGS_SCHEMAS,
  EAM_BUNDLE_DIRECTORY_GNOME_HELP,
  EAM_BUNDLE_DIRECTORY_KDE_HELP,
  EAM_BUNDLE_DIRECTORY_EKN_DATA,
  EAM_BUNDLE_DIRECTORY_SHELL_SEARCH,
  EAM_BUNDLE_DIRECTORY_KDE4,
  EAM_BUNDLE_DIRECTORY_XDG_AUTOSTART,

  /*< private >*/
  EAM_BUNDLE_DIRECTORY_MAX
} EamBundleDirectory;

gboolean        eam_fs_sanity_check     (void);

gboolean        eam_fs_init_bundle_dir  (EamBundleDirectory dir,
                                         GError **error);

gboolean        eam_fs_rmdir_recursive  (const char *path);
gboolean        eam_fs_cpdir_recursive  (const char *src,
                                         const char *dst,
                                         GCancellable *cancellable);
gboolean        eam_fs_prune_dir        (const char *prefix,
                                         const char *appdir);

gboolean        eam_fs_deploy_app       (const char *source,
                                         const char *target,
                                         const char *appdir,
                                         GCancellable *cancellable);
gboolean        eam_fs_create_symlinks  (const char *prefix,
                                         const char *appid);
void            eam_fs_prune_symlinks   (const char *prefix,
                                         const char *appid);

gboolean        eam_fs_backup_app       (const char *prefix,
                                         const char *appid,
                                         char      **backup_dir);
gboolean        eam_fs_restore_app      (const char *prefix,
                                         const char *appid,
                                         const char *backup_dir);

gboolean        eam_fs_is_app_dir       (const char *path);

const char *    eam_fs_get_bundle_system_dir    (EamBundleDirectory dir);

gboolean        eam_fs_ensure_symlink_farm (void);

G_END_DECLS

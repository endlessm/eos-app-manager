/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_FS_SANITY_H
#define EAM_FS_SANITY_H

#include <glib.h>

G_BEGIN_DECLS

gboolean        eam_fs_sanity_check     (void);
gboolean        eam_fs_sanity_delete    (const gchar *path);

gboolean        eam_fs_rmdir_recursive  (const char *path);
gboolean        eam_fs_cpdir_recursive  (const char *src,
                                         const char *dst);
gboolean        eam_fs_prune_dir        (const char *prefix,
                                         const char *appdir);

gboolean        eam_fs_deploy_app       (const char *source,
                                         const char *target,
                                         const char *appdir);
gboolean        eam_fs_create_symlinks  (const char *prefix,
                                         const char *appid);
void            eam_fs_prune_symlinks   (const char *prefix,
                                         const char *appid);

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
  EAM_BUNDLE_DIRECTORY_GAMES,

  /*< private >*/
  EAM_BUNDLE_DIRECTORY_MAX
} EamBundleDirectory;

const char *    eam_fs_get_bundle_system_dir    (EamBundleDirectory dir);

G_END_DECLS

#endif /* EAM_FS_SANITY_H */

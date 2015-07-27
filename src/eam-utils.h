/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_UTILS_H
#define EAM_UTILS_H

#include <sys/types.h>
#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

gboolean        eam_utils_verify_checksum       (const char *source_file,
                                                 const char *checksum_file);
gboolean        eam_utils_verify_signature      (const char *source_file,
                                                 const char *signature_file);

gboolean        eam_utils_bundle_extract        (const char *bundle_file,
                                                 const char *prefix,
                                                 const char *appdir);
gboolean        eam_utils_app_is_installed      (const char *prefix,
                                                 const char *appdir);

gboolean        eam_utils_run_external_scripts  (const char *prefix,
                                                 const char *appdir);

gboolean        eam_utils_compile_python        (const char *prefix,
                                                 const char *appdir);
gboolean        eam_utils_cleanup_python        (const char *appdir);
gboolean        eam_utils_update_desktop        (void);

gboolean        eam_utils_apply_xdelta          (const char *prefix,
                                                 const char *appid,
                                                 const char *delta_bundle);

char *          eam_utils_find_program_in_path  (const char *program,
                                                 const char *path);

GKeyFile *      eam_utils_load_app_info         (const char *prefix,
                                                 const char *appid);

gboolean        eam_utils_can_touch_applications_dir    (uid_t user);
gboolean        eam_utils_check_unix_permissions        (uid_t user);

G_END_DECLS

#endif

/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_UTILS_H
#define EAM_UTILS_H

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

void     eam_utils_run_bundle_scripts        (const gchar *appid,
                                              const gchar *filename,
                                              const gchar *scriptdir,
                                              GCancellable *cancellable,
                                              GAsyncReadyCallback callback,
                                              GTask *task);

char *   eam_utils_build_tarball_filename    (const char *bundle_location,
                                              const char *appid,
                                              const char *extension);

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
gboolean        eam_utils_cleanup_python        (const char *prefix,
                                                 const char *appdir);
gboolean        eam_utils_update_desktop        (const char *prefix);

gboolean        eam_utils_apply_xdelta          (const char *prefix,
                                                 const char *appid,
                                                 const char *delta_bundle);

char *          eam_utils_find_program_in_path  (const char *program,
                                                 const char *path);

G_END_DECLS

#endif

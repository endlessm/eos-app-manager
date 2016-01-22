/* eam-utils.h: Utility API
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
#include <sys/types.h>
#include <gio/gio.h>

G_BEGIN_DECLS

gboolean        eam_utils_verify_signature      (const char *source_file,
                                                 const char *signature_file,
                                                 GCancellable *cancellable);

gboolean        eam_utils_bundle_extract        (const char *bundle_file,
                                                 const char *prefix,
                                                 const char *appdir,
                                                 GCancellable *cancellable);
gboolean        eam_utils_app_is_installed      (const char *prefix,
                                                 const char *appdir);

gboolean        eam_utils_run_external_scripts  (const char *prefix,
                                                 const char *appdir,
                                                 GCancellable *cancellable);

gboolean        eam_utils_compile_python        (const char *prefix,
                                                 const char *appdir);
gboolean        eam_utils_cleanup_python        (const char *appdir);
gboolean        eam_utils_update_desktop        (void);

gboolean        eam_utils_apply_xdelta          (const char *prefix,
                                                 const char *appid,
                                                 const char *delta_bundle,
                                                 GCancellable *cancellable);

char *          eam_utils_find_program_in_path  (const char *program,
                                                 const char *path);

GKeyFile *      eam_utils_load_app_info         (const char *prefix,
                                                 const char *appid);

gboolean        eam_utils_can_touch_applications_dir    (uid_t user);
gboolean        eam_utils_check_unix_permissions        (uid_t user);
gboolean        eam_utils_can_modify_configuration      (uid_t user);

const char *    eam_utils_storage_type_to_path          (const char *storage_type);

G_END_DECLS

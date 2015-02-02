/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_UTILS_H
#define EAM_UTILS_H

#include <glib.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

gboolean eam_utils_appid_is_legal            (const char *appid);

void     eam_utils_run_bundle_scripts        (const gchar *appid,
                                              const gchar *filename,
                                              const gchar *scriptdir,
                                              const gboolean external_download,
                                              GCancellable *cancellable,
                                              GAsyncReadyCallback callback,
                                              GTask *task);

void     eam_utils_download_bundle_signature (GTask *task,
                                              GAsyncReadyCallback callback,
                                              const char *signature_url,
                                              const char *bundle_location,
                                              const char *appid);

void     eam_utils_download_bundle           (GTask *task,
                                              GAsyncReadyCallback callback,
                                              const char *download_url,
                                              const char *bundle_location,
                                              const char *appid,
                                              const char *extension);

void     eam_utils_create_bundle_hash_file   (const char *hash,
                                              const char *tarball,
                                              const char *bundle_location,
                                              const char *appid,
                                              GError **error);

char *   eam_utils_build_tarball_filename    (const char *bundle_location,
                                              const char *appid,
                                              const char *extension);

void     eam_utils_request_json_updates      (GTask *task,
                                              GAsyncReadyCallback callback,
                                              const char *appid);

void     eam_utils_parse_json_with_updates   (JsonNode *root,
                                              JsonObject **bundle_json,
                                              JsonObject **xdelta_json,
                                              const char *appid,
                                              const char *from_version,
                                              const gboolean ignore_deltas);

void     eam_utils_parse_json                (JsonNode *root,
                                              JsonObject **bundle_json,
                                              const char *appid);

int      eam_utils_compare_bundle_json_version (JsonObject *a,
                                                JsonObject *b);

gchar *  eam_utils_get_json_updates_filename (void);

G_END_DECLS

#endif

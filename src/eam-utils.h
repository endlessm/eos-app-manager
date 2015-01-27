/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_UTILS_H
#define EAM_UTILS_H

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

gboolean eam_utils_appid_is_legal       (const char *appid);

void     eam_utils_run_bundle_scripts   (const gchar *appid,
                                         const gchar *filename,
                                         const gchar *scriptdir,
                                         const gboolean external_download,
                                         GCancellable *cancellable,
                                         GAsyncReadyCallback callback,
                                         GTask *task);

G_END_DECLS

#endif

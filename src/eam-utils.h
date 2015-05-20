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

G_END_DECLS

#endif

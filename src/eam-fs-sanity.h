/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_FS_SANITY_H
#define EAM_FS_SANITY_H

#include <glib.h>

G_BEGIN_DECLS

gboolean        eam_fs_sanity_check                               (void);
gboolean        eam_fs_sanity_delete                              (const gchar *path);

G_END_DECLS

#endif /* EAM_FS_SANITY_H */

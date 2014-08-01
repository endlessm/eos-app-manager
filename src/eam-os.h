/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_OS_H
#define EAM_OS_H

#include <glib.h>

G_BEGIN_DECLS

const gchar    *eam_os_get_version                                ();
const gchar    *eam_os_get_personality                            ();
const gchar    *eam_os_get_architecture                           ();

G_END_DECLS

#endif /* EAM_OS_H */

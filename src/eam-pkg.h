/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_PKG_H
#define EAM_PKG_H

#include <glib-object.h>
#include <glib.h>
#include <json-glib/json-glib.h>

#include "eam-version.h"

G_BEGIN_DECLS

#define EAM_TYPE_PKG (eam_pkg_get_type())

typedef struct _EamPkg EamPkg;

GType           eam_pkg_get_type                (void) G_GNUC_CONST;

EamPkg         *eam_pkg_copy                    (EamPkg *pkg);

void            eam_pkg_free                    (EamPkg *pkg);

EamPkg         *eam_pkg_new_from_keyfile        (GKeyFile *keyfile,
                                                 GError **error);

EamPkg         *eam_pkg_new_from_filename       (const gchar *filename,
                                                 GError **error);

EamPkg         *eam_pkg_new_from_json_object    (JsonObject *json,
                                                 GError **error);

const gchar    *eam_pkg_get_id                  (const EamPkg *pkg);

const gchar    *eam_pkg_get_name                (const EamPkg *pkg);

EamPkgVersion  *eam_pkg_get_version             (const EamPkg *pkg);

const gchar    *eam_pkg_get_locale              (const EamPkg *pkg);

G_END_DECLS

#endif /* EAM_PKG_H */

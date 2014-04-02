/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_PKG_H
#define EAM_PKG_H

#include <glib-object.h>
#include <glib.h>

#include "eam-version.h"

G_BEGIN_DECLS

#define EAM_TYPE_PKG (eam_pkg_get_type())

typedef struct _EamPkg EamPkg;

/**
 * EamPkg:
 *
 * Boxed struct that represents a Package, installed or to update.
 */
struct _EamPkg
{
  gchar *id;
  gchar *name;
  EamPkgVersion *version;
};

GType           eam_pkg_get_type                                 (void) G_GNUC_CONST;

gpointer        eam_pkg_copy                                     (gpointer data);

void            eam_pkg_free                                     (gpointer data);

EamPkg         *eam_pkg_new_from_keyfile                         (GKeyFile *keyfile,
                                                                  GError **error);

EamPkg         *eam_pkg_new_from_filename                        (const gchar *filename,
                                                                  GError **error);


G_END_DECLS

#endif /* EAM_PKG_H */

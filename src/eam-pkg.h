/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_PKG_H
#define EAM_PKG_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EAM_TYPE_PKG (eam_pkg_get_type())

#define EAM_PKG(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EAM_TYPE_PKG, EamPkg))

#define EAM_PKG_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST((k), EAM_TYPE_PKG, EamPkgClass))

#define EAM_IS_PKG(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAM_TYPE_PKG))

#define EAM_IS_PKG_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EAM_TYPE_PKG))

#define EAM_PKG_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EAM_TYPE_PKG, EamPkgClass))

typedef struct _EamPkgClass EamPkgClass;
typedef struct _EamPkg EamPkg;
typedef struct _EamPkgPrivate EamPkgPrivate;

/**
 * EamPkg:
 *
 * This class structure contains no public members.
 */
struct _EamPkg
{
  GObject parent;

  /*< private >*/
  EamPkgPrivate *priv;
};

struct _EamPkgClass
{
  GObjectClass parent_class;
};

GType           eam_pkg_get_type                                 (void) G_GNUC_CONST;

EamPkg         *eam_pkg_new_from_keyfile                         (GKeyFile *keyfile);

EamPkg         *eam_pkg_new_from_filename                        (const gchar *filename);

G_END_DECLS

#endif /* EAM_PKG_H */

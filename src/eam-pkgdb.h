/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_PKGDB_H
#define EAM_PKGDB_H

#include <gio/gio.h>

#include "eam-pkg.h"

G_BEGIN_DECLS

#define EAM_TYPE_PKGDB (eam_pkgdb_get_type())

#define EAM_PKGDB(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EAM_TYPE_PKGDB, EamPkgdb))

#define EAM_PKGDB_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST((k), EAM_TYPE_PKGDB, EamPkgdbClass))

#define EAM_IS_PKGDB(o)	\
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAM_TYPE_PKGDB))

#define EAM_IS_PKGDB_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EAM_TYPE_PKGDB))

#define EAM_PKGDB_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EAM_TYPE_PKGDB, EamPkgdbClass))

typedef struct _EamPkgdbClass	EamPkgdbClass;
typedef struct _EamPkgdb	EamPkgdb;

struct _EamPkgdb
{
	GInitiallyUnowned parent;
};

struct _EamPkgdbClass
{
	GInitiallyUnownedClass parent_class;
};

GType           eam_pkgdb_get_type                               (void) G_GNUC_CONST;

EamPkgdb       *eam_pkgdb_new                                    (void);

EamPkgdb       *eam_pkgdb_new_with_appdir                        (const gchar *appdir);

gboolean        eam_pkgdb_add                                    (EamPkgdb *pkgdb, const gchar *appid, EamPkg *pkg);

gboolean        eam_pkgdb_del                                    (EamPkgdb *pkgdb, const gchar *appid);

const EamPkg   *eam_pkgdb_get                                    (EamPkgdb *pkgdb, const gchar *appid);

gboolean        eam_pkgdb_exists                                 (EamPkgdb *pkgdb, const gchar *appid);

gboolean        eam_pkgdb_load                                   (EamPkgdb *pkgdb,
                                                                  GError **error);

void            eam_pkgdb_load_async                             (EamPkgdb *pkgdb,
								  GCancellable *cancellable,
								  GAsyncReadyCallback callback,
								  gpointer data);

gboolean        eam_pkgdb_load_finish                            (EamPkgdb *pkgdb,
                                                                  GAsyncResult *res,
                                                                  GError **error);

void            eam_pkgdb_dump                                   (EamPkgdb *pkgdb);

G_END_DECLS

#endif /* EAM_PKGDB_H */

/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_UPDATES_H
#define EAM_UPDATES_H

#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include "eam-pkgdb.h"

G_BEGIN_DECLS

#define EAM_TYPE_UPDATES (eam_updates_get_type())

#define EAM_UPDATES(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EAM_TYPE_UPDATES, EamUpdates))

#define EAM_UPDATES_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST((k), EAM_TYPE_UPDATES, EamUpdatesClass))

#define EAM_IS_UPDATES(o) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAM_TYPE_UPDATES))

#define EAM_IS_UPDATES_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EAM_TYPE_UPDATES))

#define EAM_UPDATES_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EAM_TYPE_UPDATES, EamUpdatesClass))

typedef struct _EamUpdatesClass EamUpdatesClass;
typedef struct _EamUpdates EamUpdates;

/**
 * EamUpdates:
 *
 * This class structure contains no public members.
 */
struct _EamUpdates
{
  GObject parent;
};

struct _EamUpdatesClass
{
  GObjectClass parent_class;
};

GType           eam_updates_get_type                    (void) G_GNUC_CONST;

EamUpdates     *eam_updates_new                         (void);

void            eam_updates_fetch_async                 (EamUpdates *self,
							 GCancellable *cancellable,
							 GAsyncReadyCallback callback,
							 gpointer data);

gssize          eam_updates_fetch_finish                (EamUpdates *self,
                                                         GAsyncResult *result,
                                                         GError **error);

gboolean        eam_updates_parse                       (EamUpdates *self,
                                                         GError **error);

gboolean        eam_updates_load                        (EamUpdates *self,
                                                         JsonNode *root,
                                                         GError **error);

void            eam_updates_filter                      (EamUpdates *self,
                                                         EamPkgdb *db,
                                                         const char *language);

const GList    *eam_updates_get_installables            (EamUpdates *self);

const GList    *eam_updates_get_upgradables             (EamUpdates *self);

gboolean        eam_updates_pkg_is_upgradable           (EamUpdates *self,
                                                         const gchar *appid);

const EamPkg   *eam_updates_pkg_is_installable          (EamUpdates *self,
                                                         const gchar *appid);

G_END_DECLS

#endif /* EAM_UPDATES_H */

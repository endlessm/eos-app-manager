/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_UPDATE_H
#define EAM_UPDATE_H

#include "eam-transaction.h"

G_BEGIN_DECLS

#define EAM_TYPE_UPDATE (eam_update_get_type())

#define EAM_UPDATE(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EAM_TYPE_UPDATE, EamUpdate))

#define EAM_UPDATE_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST((k), EAM_TYPE_UPDATE, EamUpdateClass))

#define EAM_IS_UPDATE(o)        \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAM_TYPE_UPDATE))

#define EAM_IS_UPDATE_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EAM_TYPE_UPDATE))

#define EAM_UPDATE_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EAM_TYPE_UPDATE, EamUpdateClass))

typedef struct _EamUpdateClass        EamUpdateClass;
typedef struct _EamUpdate        EamUpdate;

struct _EamUpdate
{
  GObject parent;
};

struct _EamUpdateClass
{
  GObjectClass parent_class;
};

GType eam_update_get_type (void) G_GNUC_CONST;

EamTransaction *   eam_update_new                 (const gchar *appid,
                                                   const gboolean allow_deltas);

void               eam_update_set_bundle_location (EamUpdate  *update,
                                                    const char  *path);

const char *       eam_update_get_app_id          (EamUpdate  *update);
const gboolean     eam_update_is_delta_update     (EamUpdate  *update);

G_END_DECLS

#endif /* EAM_UPDATE_H */

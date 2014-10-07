/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_REFRESH_H
#define EAM_REFRESH_H

#include <gio/gio.h>

#include "eam-pkgdb.h"
#include "eam-updates.h"
#include "eam-transaction.h"

G_BEGIN_DECLS

#define EAM_TYPE_REFRESH (eam_refresh_get_type())

#define EAM_REFRESH(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EAM_TYPE_REFRESH, EamRefresh))

#define EAM_REFRESH_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST((k), EAM_TYPE_REFRESH, EamRefreshClass))

#define EAM_IS_REFRESH(o)	\
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAM_TYPE_REFRESH))

#define EAM_IS_REFRESH_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EAM_TYPE_REFRESH))

#define EAM_REFRESH_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EAM_TYPE_REFRESH, EamRefreshClass))

typedef struct _EamRefreshClass	EamRefreshClass;
typedef struct _EamRefresh	EamRefresh;

struct _EamRefresh
{
  GObject parent;
};

struct _EamRefreshClass
{
  GObjectClass parent_class;
};

GType           eam_refresh_get_type    (void) G_GNUC_CONST;

EamTransaction *eam_refresh_new         (EamPkgdb *db,
                                         EamUpdates *updates);

G_END_DECLS

#endif /* EAM_REFRESH_H */

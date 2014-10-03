/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_LIST_AVAIL_H
#define EAM_LIST_AVAIL_H

#include <gio/gio.h>

#include "eam-pkgdb.h"
#include "eam-updates.h"
#include "eam-transaction.h"

G_BEGIN_DECLS

#define EAM_TYPE_LIST_AVAIL (eam_list_avail_get_type())

#define EAM_LIST_AVAIL(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EAM_TYPE_LIST_AVAIL, EamListAvail))

#define EAM_LIST_AVAIL_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST((k), EAM_TYPE_LIST_AVAIL, EamListAvailClass))

#define EAM_IS_LIST_AVAIL(o)	\
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAM_TYPE_LIST_AVAIL))

#define EAM_IS_LIST_AVAIL_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EAM_TYPE_LIST_AVAIL))

#define EAM_LIST_AVAIL_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EAM_TYPE_LIST_AVAIL, EamListAvailClass))

typedef struct _EamListAvailClass	EamListAvailClass;
typedef struct _EamListAvail	EamListAvail;

struct _EamListAvail
{
  GObject parent;
};

struct _EamListAvailClass
{
  GObjectClass parent_class;
};

GType           eam_list_avail_get_type (void) G_GNUC_CONST;

EamTransaction *eam_list_avail_new      (gboolean dbreloaded,
                                         EamPkgdb *db,
                                         EamUpdates *updates,
                                         const char *language);

G_END_DECLS

#endif /* EAM_LIST_AVAIL_H */

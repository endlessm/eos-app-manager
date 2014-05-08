/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_LIST_AVAIL_H
#define EAM_LIST_AVAIL_H

#include <gio/gio.h>

#include "eam-pkgdb.h"
#include "eam-updates.h"
#include "eam-transaction.h"

G_BEGIN_DECLS

/**
 * EamListAvailError:
 * @EAM_LIST_AVAIL_ERROR_PROTOCOL_ERROR: Invalid URI.
 * @EAM_LIST_AVAIL_ERROR_INVALID_FILE: Invalid list_avail file.
 * @EAM_LIST_AVAIL_ERROR_NO_NETWORK: The network is not available.
 *
 * These constants identify all the available errors managed by
 * the Endless Application Manager ListAvail.
 */
typedef enum {
  EAM_LIST_AVAIL_ERROR_PROTOCOL_ERROR = 1,
  EAM_LIST_AVAIL_ERROR_INVALID_FILE,
  EAM_LIST_AVAIL_ERROR_NO_NETWORK,
} EamListAvailError;

#define EAM_LIST_AVAIL_ERROR eam_list_avail_error_quark ()

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

GType           eam_list_avail_get_type                               (void) G_GNUC_CONST;

EamTransaction *eam_list_avail_new                                    (gboolean dbreloaded,
                                                                       EamPkgdb *db,
                                                                       EamUpdates *updates);

G_END_DECLS

#endif /* EAM_LIST_AVAIL_H */

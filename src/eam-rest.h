/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_REST_H
#define EAM_REST_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  EAM_REST_API_V1_GET_ALL_UPDATES,
  EAM_REST_API_V1_GET_APP_UPDATES,
  EAM_REST_API_V1_GET_APP_UPDATE_LINK,
  EAM_REST_API_V1_GET_APP_UPDATE_BLOB,
} EamRestMethod;

gchar *         eam_rest_build_uri      (EamRestMethod method,
                                         ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS

#endif /* EAM_REST_H */

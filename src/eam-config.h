/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_CONFIG_H
#define EAM_CONFIG_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _EamConfig EamConfig;

struct _EamConfig
{
  gchar *appdir;
  gchar *serveraddr;
  gchar *dldir;
};

EamConfig      *eam_config_get                                    (void);

void            eam_config_free                                   (EamConfig *cfg);

gboolean        eam_config_load                                   (EamConfig *cfg,
                                                                   GKeyFile *keyfile);

void            eam_config_set                                    (EamConfig *cfg,
                                                                   gchar *appdir,
                                                                   gchar *serveraddr,
                                                                   gchar *dldir);

G_END_DECLS

#endif /* EAM_CONFIG_H */
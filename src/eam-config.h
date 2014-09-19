/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_CONFIG_H
#define EAM_CONFIG_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _EamConfig EamConfig;

EamConfig      *eam_config_get                                    (void);

void            eam_config_free                                   (EamConfig *cfg);

void            eam_config_destroy                                (EamConfig *cfg);

gboolean        eam_config_load                                   (EamConfig *cfg,
                                                                   GKeyFile *keyfile);

gboolean        eam_config_load_with_filename                     (EamConfig *cfg,
                                                                   const gchar *filename);

void            eam_config_set                                    (EamConfig *cfg,
                                                                   gchar *appdir,
                                                                   gchar *dldir,
                                                                   gchar *saddr,
                                                                   gchar *protver,
                                                                   gchar *scriptdir,
                                                                   gchar *gpgkeyring,
                                                                   guint timeout);

void            eam_config_dump                                   (EamConfig *cfg);

guint           eam_config_timeout                                ();

#define PARAMS_LIST(V) \
  V(appdir) \
  V(dldir)  \
  V(saddr)  \
  V(protver) \
  V(scriptdir) \
  V(gpgkeyring)

#define GETTERS(p) const gchar *eam_config_##p ();
PARAMS_LIST(GETTERS)
#undef GETTERS

G_END_DECLS

#endif /* EAM_CONFIG_H */

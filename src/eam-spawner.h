/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_SPAWNER_H
#define EAM_SPAWNER_H

#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * EamSpawnerError:
 * @EAM_SPAWNER_ERROR_SCRIPT_FAILED: The script returned an exit error
 *
 * These constants identify all the available errors managed by
 * the Endless Application Manager Spawner.
 */
typedef enum {
  EAM_SPAWNER_ERROR_SCRIPT_FAILED = 1,
} EamSpawnerError;

#define EAM_SPAWNER_ERROR eam_spawner_error_quark ()

#define EAM_TYPE_SPAWNER (eam_spawner_get_type())

#define EAM_SPAWNER(o) \
  (G_TYPE_CHECK_INSTANCE_CAST ((o), EAM_TYPE_SPAWNER, EamSpawner))

#define EAM_SPAWNER_CLASS(k) \
  (G_TYPE_CHECK_CLASS_CAST((k), EAM_TYPE_SPAWNER, EamSpawnerClass))

#define EAM_IS_SPAWNER(o)	\
  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EAM_TYPE_SPAWNER))

#define EAM_IS_SPAWNER_CLASS(k) \
  (G_TYPE_CHECK_CLASS_TYPE ((k), EAM_TYPE_SPAWNER))

#define EAM_SPAWNER_GET_CLASS(o) \
  (G_TYPE_INSTANCE_GET_CLASS ((o), EAM_TYPE_SPAWNER, EamSpawnerClass))

typedef struct _EamSpawnerClass	EamSpawnerClass;
typedef struct _EamSpawner	EamSpawner;

struct _EamSpawner
{
  GObject parent;
};

struct _EamSpawnerClass
{
  GObjectClass parent_class;
};

GQuark          eam_spawner_error_quark                          (void);

GType           eam_spawner_get_type                             (void) G_GNUC_CONST;

EamSpawner     *eam_spawner_new                                  (const gchar *path,
                                                                  const gchar * const *params);

void            eam_spawner_run_async                            (EamSpawner *self,
                                                                  GCancellable *cancellable,
                                                                  GAsyncReadyCallback callback,
                                                                  gpointer data);

gboolean        eam_spawner_run_finish                           (EamSpawner *self,
                                                                  GAsyncResult *res,
                                                                  GError **error);

G_END_DECLS

#endif /* EAM_SPAWNER_H */

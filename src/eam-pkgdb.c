/* Copyright 2014 Endless Mobile, Inc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>

#include <string.h>
#include <errno.h>

#include "eam-pkgdb.h"

typedef struct _EamPkgdbPrivate	EamPkgdbPrivate;

struct _EamPkgdbPrivate
{
  GHashTable *pkgtable;
  gchar *appdir;
};

G_DEFINE_TYPE_WITH_PRIVATE (EamPkgdb, eam_pkgdb, G_TYPE_INITIALLY_UNOWNED)

enum
{
  PROP_APPDIR = 1,
};

static void
eam_pkgdb_finalize (GObject *obj)
{
  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (EAM_PKGDB (obj));

  g_free (priv->appdir);
  g_hash_table_unref (priv->pkgtable);

  G_OBJECT_CLASS (eam_pkgdb_parent_class)->finalize (obj);
}

static void
eam_pkgdb_set_property (GObject *obj, guint prop_id, const GValue *value,
  GParamSpec *pspec)
{
  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (EAM_PKGDB (obj));

  switch (prop_id) {
  case PROP_APPDIR:
    priv->appdir = g_value_dup_string (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
eam_pkgdb_get_property (GObject *obj, guint prop_id, GValue *value,
  GParamSpec *pspec)
{
  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (EAM_PKGDB (obj));

  switch (prop_id) {
  case PROP_APPDIR:
    g_value_set_string (value, priv->appdir);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
eam_pkgdb_class_init (EamPkgdbClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = eam_pkgdb_finalize;
  object_class->get_property = eam_pkgdb_get_property;
  object_class->set_property = eam_pkgdb_set_property;

  /**
   * EamPkgdb:appdir:
   *
   * The directory where the applications are installed
   */
  g_object_class_install_property (object_class, PROP_APPDIR,
    g_param_spec_string ("appdir", "AppDir", "Apps directory", "/endless",
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
eam_pkgdb_init (EamPkgdb *db)
{
  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (db);

  priv->pkgtable = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
     (GDestroyNotify) eam_pkg_free);
}

/**
 * eam_pkgdb_new:
 *
 * Create a new instance of #EamPkgdb with the default appdir.
 */
EamPkgdb *
eam_pkgdb_new ()
{
  return g_object_new (EAM_TYPE_PKGDB, NULL);
}

/**
 * eam_pkgdb_new_with_appdir:
 * @appdir: the directory where the application bundles live
 *
 * Create a new instance of #EamPkgdb setting an @appdir.
 */
EamPkgdb *
eam_pkgdb_new_with_appdir (const gchar *appdir)
{
  return g_object_new (EAM_TYPE_PKGDB, "appdir", appdir, NULL);
}

static gboolean
appid_is_legal (const char *appid)
{
  static const char alsoallowed[] = "-+.";
  int c;

  if (!appid || appid[0] == '\0')
    return FALSE;

  if (g_strcmp0 (appid, "share") == 0)
    return FALSE;

  if (!g_ascii_isalnum (appid[0]))
    return FALSE; /* must start with an alphanumeric character */

  while ((c = *appid++) != '\0')
    if (!g_ascii_isalnum (c) && !strchr (alsoallowed, c))
      break;

  if (!c)
    return TRUE;

  return FALSE;
}

/**
 * eam_pkgdb_add:
 * @pkgdb: a #EamPkgdb
 * @appid: the application ID
 * @pkg: the #EamPkg of the application
 *
 * Adds @pkg into the @pkgdb
 *
 * Returns: %TRUE if the @pkg was added successfully.
 */
gboolean
eam_pkgdb_add (EamPkgdb *pkgdb, const gchar *appid, EamPkg *pkg)
{
  g_return_val_if_fail (EAM_IS_PKGDB (pkgdb), FALSE);
  g_return_val_if_fail (appid != NULL, FALSE);
  g_return_val_if_fail (pkg, FALSE);

  if (!appid_is_legal (appid))
    return FALSE;

  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (pkgdb);

  /* appid and pkg->id should be the same */
  if (g_strcmp0 (appid, eam_pkg_get_id (pkg)) != 0)
    return FALSE;

  /* appid collision */
  if (g_hash_table_contains (priv->pkgtable, appid))
    return FALSE;

  return g_hash_table_insert (priv->pkgtable, (gpointer) eam_pkg_get_id (pkg),
    pkg);
}

/**
 * eam_pkgdb_del:
 * @pkgdb: a #EamPkgdb
 * @appid: the application ID
 *
 * Removes @pkg from the @pkgdb
 *
 * Returns: %TRUE if the @pkg was deleted successfully.
 */
gboolean
eam_pkgdb_del (EamPkgdb *pkgdb, const gchar *appid)
{
  g_return_val_if_fail (EAM_IS_PKGDB (pkgdb), FALSE);
  g_return_val_if_fail (appid != NULL, FALSE);

  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (pkgdb);

  return g_hash_table_remove (priv->pkgtable, appid);
}

/**
 * eam_pkgdb_del:
 * @pkgdb: a #EamPkgdb
 * @appid: the application ID
 *
 * Gets a @pkg from the @pkgdb.
 *
 * Do not modify the content of the @pkg.
 *
 * Returns: (transfer none): #EamPkg if found or %NULL
 */
const EamPkg *
eam_pkgdb_get (EamPkgdb *pkgdb, const gchar *appid)
{
  g_return_val_if_fail (EAM_IS_PKGDB (pkgdb), FALSE);
  g_return_val_if_fail (appid != NULL, FALSE);

  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (pkgdb);
  return g_hash_table_lookup (priv->pkgtable, appid);
}

/**
 * eam_pkgdb_del:
 * @pkgdb: a #EamPkgdb
 * @appid: the application ID
 *
 * Returns: %TRUE if @appid exist in @pkgdb
 */
gboolean
eam_pkgdb_exists (EamPkgdb *pkgdb, const gchar *appid)
{
  g_return_val_if_fail (EAM_IS_PKGDB (pkgdb), FALSE);
  g_return_val_if_fail (appid != NULL, FALSE);

  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (pkgdb);
  return g_hash_table_contains (priv->pkgtable, appid);
}

/**
 * eam_pkgdb_load:
 * @pkgdb: a #EamPkgdb
 *
 * Loads all the @pkg found in the appdir
 */
gboolean
eam_pkgdb_load (EamPkgdb *pkgdb, GError **error)
{
  g_return_val_if_fail (EAM_IS_PKGDB (pkgdb), FALSE);

  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (pkgdb);

  /* IDEA: Instead of using EamPkg as hash value, we shall use another
   * boxed structure. That structure will contain the EamPkg, a visited
   * counter the parsed timestamp. Hence we could reload the database
   * more efficiently.
   */
  g_hash_table_remove_all (priv->pkgtable);

  GDir *dir = g_dir_open (priv->appdir, 0, error);
  if (!dir)
    return FALSE;

  int saved_errno = 0;
  const gchar *appid;
  while ((appid = g_dir_read_name (dir))) {
    if (errno)
      saved_errno = errno;

    if (!appid_is_legal (appid))
      continue;

    gchar *info = g_build_path (G_DIR_SEPARATOR_S, priv->appdir, appid, ".info", NULL);
    GError *perr = NULL;
    EamPkg *pkg = eam_pkg_new_from_filename (info, &perr);
    g_free (info);
    if (pkg)
      eam_pkgdb_add (pkgdb, appid, pkg);
    if (perr) {
	g_message ("Error loading %s: %s", appid, perr->message);
	g_clear_error (&perr);
    }
  }
  g_dir_close (dir);

  if (saved_errno) {
    g_set_error (error, G_IO_ERROR, g_io_error_from_errno (saved_errno),
                 _("Error when getting information for directory '%s': %s"),
                 priv->appdir,
                 strerror (saved_errno));
    return FALSE;
  }

  return TRUE;
}

static void
load_pkgdb_thread (GTask *task, gpointer source, gpointer data,
  GCancellable *cancellable)
{
  EamPkgdb *pkgdb = g_task_get_source_object (task);
  g_assert (pkgdb);

  if (g_task_return_error_if_cancelled (task))
    return;

  GError *error = NULL;
  eam_pkgdb_load (pkgdb, &error);
  if (error) {
    g_task_return_error (task, error);
    return;
  }

  g_task_return_boolean (task, TRUE);
}

/**
 * eam_pkgdb_load_async:
 * @pkgdb: a #EamPkgdb
 * @cancellable: (allow-none): optional #GCancellable object,
 *     %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the
 *     request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Loads all the @pkg found in the appdir asynchronously
 */
void
eam_pkgdb_load_async (EamPkgdb *pkgdb, GCancellable *cancellable,
  GAsyncReadyCallback callback, gpointer data)
{
  g_return_if_fail (EAM_IS_PKGDB (pkgdb));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback);

  GTask *task = g_task_new (pkgdb, cancellable, callback, data);

  g_task_run_in_thread (task, load_pkgdb_thread);
  g_object_unref (task);
}

/**
 * eam_pkgdb_load_finish:
 * @pkgdb: a #EamPkgdb
 * @res: a #GAsyncResult
 *
 * Finishes an async packages load, see eam_pkgdb_load_async().
 */
gboolean
eam_pkgdb_load_finish (EamPkgdb *pkgdb, GAsyncResult *res, GError **error)
{
  g_return_val_if_fail (g_task_is_valid (res, pkgdb), FALSE);
  return g_task_propagate_boolean (G_TASK (res), error);
}

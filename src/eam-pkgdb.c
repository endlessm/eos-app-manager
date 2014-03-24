/* Copyright 2014 Endless Mobile, Inc. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
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
    if (priv->appdir)
      g_free (priv->appdir);

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

  priv->appdir = g_strdup("/endless");
  priv->pkgtable = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
    g_object_unref);
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
  g_return_val_if_fail (EAM_IS_PKG (pkg), FALSE);

  if (!appid_is_legal (appid))
    return FALSE;

  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (pkgdb);

  /* appid collision */
  if (g_hash_table_contains (priv->pkgtable, appid))
    return FALSE;

  return g_hash_table_insert (priv->pkgtable, g_strdup (appid),
    g_object_ref (pkg));
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
 * Gets a @pkg from the @pkgdb
 *
 * Returns: (transfer full): #EamPkg if found or %NULL
 */
EamPkg *
eam_pkgdb_get (EamPkgdb *pkgdb, const gchar *appid)
{
  g_return_val_if_fail (EAM_IS_PKGDB (pkgdb), FALSE);
  g_return_val_if_fail (appid != NULL, FALSE);

  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (pkgdb);
  EamPkg *pkg = g_hash_table_lookup (priv->pkgtable, appid);

  return (pkg) ? g_object_ref (pkg) : NULL;
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
void
eam_pkgdb_load (EamPkgdb *pkgdb)
{
  g_return_if_fail (EAM_IS_PKGDB (pkgdb));

  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (pkgdb);

  /* IDEA: Instead of using EamPkg as hash value, we shall use another
   * boxed structure. That structure will contain the EamPkg, a visited
   * counter the parsed timestamp. Hence we could reload the database
   * more efficiently.
   */
  g_hash_table_remove_all (priv->pkgtable);

  GDir *dir = g_dir_open (priv->appdir, 0, NULL);
  if (!dir)
    return;

  const gchar *appid;
  while ((appid = g_dir_read_name (dir))) {
    if (!appid_is_legal (appid))
      continue;

    gchar *info = g_build_path (G_DIR_SEPARATOR_S, priv->appdir, appid, ".info", NULL);
    EamPkg *pkg = eam_pkg_new_from_filename (info);
    g_free (info);
    if (pkg) {
      eam_pkgdb_add (pkgdb, appid, pkg);
      g_object_unref (pkg);
    }
  }
  g_dir_close (dir);
}

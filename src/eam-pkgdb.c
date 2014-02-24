/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"
#include "eam-pkgdb.h"

struct _EamPkgdbPrivate
{
  GHashTable *pkgtable;
  gchar *appdir;
};

G_DEFINE_TYPE_WITH_PRIVATE (EamPkgdb, eam_pkgdb, G_TYPE_OBJECT)

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

EamPkgdb *
eam_pkgdb_new ()
{
  return g_object_new (EAM_TYPE_PKGDB, NULL);
}

EamPkgdb *
eam_pkgdb_new_with_appdir (const gchar *appdir)
{
  return g_object_new (EAM_TYPE_PKGDB, "appdir", appdir, NULL);
}

gboolean
eam_pkgdb_add (EamPkgdb *pkgdb, const gchar *appid, EamPkg *pkg)
{
  g_return_val_if_fail (EAM_IS_PKGDB (pkgdb), FALSE);
  g_return_val_if_fail (appid != NULL, FALSE);
  g_return_val_if_fail (EAM_IS_PKG (pkg), FALSE);

  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (pkgdb);

  /* appid collision */
  if (g_hash_table_contains (priv->pkgtable, appid))
    return FALSE;

  return g_hash_table_insert (priv->pkgtable, g_strdup (appid), pkg);
}

gboolean
eam_pkgdb_del (EamPkgdb *pkgdb, const gchar *appid)
{
  g_return_val_if_fail (EAM_IS_PKGDB (pkgdb), FALSE);
  g_return_val_if_fail (appid != NULL, FALSE);

  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (pkgdb);

  return g_hash_table_remove (priv->pkgtable, appid);
}

EamPkg *
eam_pkgdb_get (EamPkgdb *pkgdb, const gchar *appid)
{
  g_return_val_if_fail (EAM_IS_PKGDB (pkgdb), FALSE);
  g_return_val_if_fail (appid != NULL, FALSE);

  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (pkgdb);
  EamPkg *pkg = g_hash_table_lookup (priv->pkgtable, appid);

  return (pkg) ? g_object_ref (pkg) : NULL;
}

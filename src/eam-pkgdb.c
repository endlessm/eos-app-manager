/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"
#include "eam-pkgdb.h"

struct _EamPkgdbPrivate
{
  GHashTable *pkgtable;
};

G_DEFINE_TYPE_WITH_PRIVATE (EamPkgdb, eam_pkgdb, G_TYPE_OBJECT)

static void
eam_pkgdb_finalize (GObject *obj)
{
  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (EAM_PKGDB (obj));

  g_hash_table_unref (priv->pkgtable);
}

static void
eam_pkgdb_class_init (EamPkgdbClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = eam_pkgdb_finalize;
}

static void
eam_pkgdb_init (EamPkgdb *db)
{
  EamPkgdbPrivate *priv = eam_pkgdb_get_instance_private (db);

  priv->pkgtable = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

EamPkgdb *
eam_pkgdb_new ()
{
  return g_object_new (EAM_TYPE_PKGDB, NULL);
}

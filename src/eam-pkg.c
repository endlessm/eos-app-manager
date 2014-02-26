/* Copyright 2014 Endless Mobile, Inc. */

#include "config.h"
#include "eam-pkg.h"
#include "eam-version.h"

typedef struct _EamPkgPrivate EamPkgPrivate;

struct _EamPkgPrivate
{
  gchar *filename;
  EamPkgVersion *version;
  GKeyFile *keyfile;
};

G_DEFINE_TYPE_WITH_PRIVATE (EamPkg, eam_pkg, G_TYPE_OBJECT)

enum
{
  PROP_FILENAME = 1,
  PROP_VERSION
};

static void
eam_pkg_finalize (GObject *obj)
{
  EamPkgPrivate *priv = eam_pkg_get_instance_private (EAM_PKG (obj));

  g_free (priv->filename);

  if (priv->version)
    eam_pkg_version_free (priv->version);

  if (priv->keyfile)
    g_key_file_unref (priv->keyfile);

  G_OBJECT_CLASS (eam_pkg_parent_class)->finalize (obj);
}

static void
eam_pkg_set_property (GObject *obj, guint prop_id, const GValue *value,
  GParamSpec *pspec)
{
  EamPkgPrivate *priv = eam_pkg_get_instance_private (EAM_PKG (obj));

  switch (prop_id) {
  case PROP_FILENAME:
    priv->filename = g_value_dup_string (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
eam_pkg_get_property (GObject *obj, guint prop_id, GValue *value,
  GParamSpec *pspec)
{
  EamPkgPrivate *priv = eam_pkg_get_instance_private (EAM_PKG (obj));

  switch (prop_id) {
  case PROP_FILENAME:
    g_value_set_string (value, priv->filename);
    break;
  case PROP_VERSION:
    g_value_set_boxed (value, priv->version);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
    break;
  }
}

static void
eam_pkg_class_init (EamPkgClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = eam_pkg_finalize;
  object_class->get_property = eam_pkg_get_property;
  object_class->set_property = eam_pkg_set_property;

  /**
   * EamPkg:filename:
   *
   * The manifest filename of this #EamPkg
   */
  g_object_class_install_property (object_class, PROP_FILENAME,
    g_param_spec_string ("filename", "Filename", "", NULL,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * EamPkg:version:
   *
   * The version of this #EamPkg
   */
  g_object_class_install_property (object_class, PROP_VERSION,
    g_param_spec_boxed ("version", "Version", "", EAM_TYPE_PKG_VERSION,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
eam_pkg_init (EamPkg *pkg)
{
}

static gboolean
eam_pkg_load_from_keyfile (EamPkg *pkg, GKeyFile *keyfile)
{
  char *start_group, *version;
  EamPkgPrivate *priv = eam_pkg_get_instance_private (pkg);

  if (!keyfile)
    return FALSE;

  start_group = g_key_file_get_start_group (keyfile);
  if (g_strcmp0 (start_group, "Bundle")) {
    g_free (start_group);
    return FALSE;
  }
  g_free (start_group);

  version = g_key_file_get_string (keyfile, "Bundle", "version", NULL);
  priv->version = eam_pkg_version_new_from_string (version);
  if (!priv->version) {
    g_free (version);
    return FALSE;
  }

  priv->keyfile = g_key_file_ref (keyfile);

  return TRUE;
}

static gboolean
eam_pkg_load_file (EamPkg *pkg)
{
  GKeyFile *keyfile;
  gboolean retval = FALSE;
  EamPkgPrivate *priv = eam_pkg_get_instance_private (pkg);

  if (!priv->filename)
    return FALSE;

  keyfile = g_key_file_new ();

  if (g_key_file_load_from_file (keyfile, priv->filename, G_KEY_FILE_NONE, NULL))
    retval = eam_pkg_load_from_keyfile (pkg, keyfile);

  g_key_file_unref (keyfile);
  return retval;
}

/**
 * eam_pkg_new_from_keyfile:
 * @keyfile: the #GKeyFile to load
 *
 * Creates a new #EamPkg based on a parsed key-file
 *
 * Returns: a new #EamPkg, or %NULL if the key-file is not valid
 */
EamPkg *
eam_pkg_new_from_keyfile (GKeyFile *keyfile)
{
  EamPkg *pkg;

  pkg = g_object_new (EAM_TYPE_PKG, NULL);
  if (!eam_pkg_load_from_keyfile (pkg, keyfile)) {
    g_object_unref (pkg);
    return NULL;
  }

  return pkg;
}

/**
 * eam_pkg_new_from_filename:
 * @filename: the bundle info file
 *
 * Creates a new #EamPkg based on a bundle info file
 *
 * Returns: a new #EamPkg, or %NULL if the info file is not valid
 */
EamPkg *
eam_pkg_new_from_filename (const gchar *filename)
{
  EamPkg *pkg;

  pkg = g_object_new (EAM_TYPE_PKG, "filename", filename, NULL);
  if (!eam_pkg_load_file (pkg)) {
    g_object_unref (pkg);
    return NULL;
  }

  return pkg;
}

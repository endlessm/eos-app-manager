/* Copyright 2014 Endless Mobile, Inc. */

#include "eam-rest.h"
#include "eam-os.h"
#include "eam-config.h"

static const gchar *METHODS_V1_FORMAT[] = {
  "%s/api/v1/updates/%s",       /* ALL_UPDATES     */
  "%s/api/v1/updates/%s/%s",    /* APP_UPDATE      */
  "%s/api/v1/updates/%s/%s",    /* APP_UPDATE_LINK */
  "%s/api/v1/updates/blob/%s",  /* APP_UPDATE_BLOB */
};

static gchar *
build_uri_v1 (EamRestMethod method, gchar *saddr, va_list args)
{
  const gchar *osver = eam_os_get_version ();

  switch (method) {
  case EAM_REST_API_V1_GET_ALL_UPDATES:{
    return g_strdup_printf (METHODS_V1_FORMAT[method], saddr, osver);
  }
  case EAM_REST_API_V1_GET_APP_UPDATES:{
    const gchar *appid = va_arg (args, const gchar *);
    if (appid)
      return g_strdup_printf (METHODS_V1_FORMAT[method], saddr, osver, appid);

    break;
  }
  case EAM_REST_API_V1_GET_APP_UPDATE_LINK:{
    const gchar *appid = va_arg (args, const gchar *);
    const gchar *appver = va_arg (args, const gchar *);
    gchar *ret = NULL;
    if (appid) {
       ret = g_strdup_printf (METHODS_V1_FORMAT[method], saddr, osver, appid);
       if (ret && appver) {
         gchar *tmp = ret;
         ret = g_strconcat (tmp, "/", appver, NULL);
         g_free (tmp);
       }
       return ret;
    }

    break;
  }
  case EAM_REST_API_V1_GET_APP_UPDATE_BLOB:{
    const gchar *hash = va_arg (args, const gchar *);
    if (hash)
      return g_strdup_printf (METHODS_V1_FORMAT[method], saddr, hash);

    break;
  }
  case EAM_REST_API_V1_GET_APP_DOWNLOAD_LINK:{
    const gchar *path = va_arg (args, const gchar *);
    return g_strconcat (saddr, path, NULL);

    break;
  }
  default:
    break;
  }

  return NULL;
}

/**
 * eam_rest_build_uri:
 * @method: The #EamRestMethod to construct
 * @...: The parameters of the rest method
 *
 * Construct the URI string given the @method
 *
 * Returns: a newly-allocated string.
 **/
gchar *
eam_rest_build_uri (EamRestMethod method, ...)
{
  va_list args;
  EamConfig *cfg = eam_config_get ();

  va_start (args, method);

  if (g_strcmp0 (cfg->protver, "v1") == 0)
    return build_uri_v1 (method, cfg->saddr, args);

  return NULL;
}

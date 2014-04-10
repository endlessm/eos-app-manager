/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_WC_H
#define EAM_WC_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define EAM_TYPE_WC (eam_wc_get_type ())

#define EAM_WC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), EAM_TYPE_WC, EamWc))

#define EAM_IS_WC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EAM_TYPE_WC))

#define EAM_WC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), EAM_TYPE_WC, EamWcClass))

#define EAM_IS_WC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), EAM_TYPE_WC))

#define EAM_WC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EAM_TYPE_WC, EamWcClass))

#define EAM_WC_ERROR eam_wc_error_quark ()

typedef struct _EamWc        EamWc;
typedef struct _EamWcClass   EamWcClass;

/**
 * EamWc:
 * @parent: the parent object struct
 */
struct _EamWc
{
  GObject parent;
};

/**
 * EamWcClass:
 * @parent_class: the parent class structure
 *
 * Endless Application Manager web client helper class.
 *
 * It's a simple and thin web client to be used to download content
 * from Internet.
 */
struct _EamWcClass
{
  GObjectClass parent_class;
};

GType           eam_wc_get_type                                  (void) G_GNUC_CONST;

GQuark          eam_wc_error_quark                               (void) G_GNUC_CONST;

EamWc          *eam_wc_new                                       (void);

void            eam_wc_request_async                             (EamWc *self,
								  const gchar *uri,
                                                                  const gchar *filename,
								  GCancellable *cancellable,
								  GAsyncReadyCallback callback,
								  gpointer data);

void            eam_wc_request_with_headers_hash_async           (EamWc *self,
								  const gchar *uri,
                                                                  const gchar *filename,
								  GHashTable *headers,
								  GCancellable *cancellable,
								  GAsyncReadyCallback callback,
								  gpointer data);

void            eam_wc_request_with_headers_async                (EamWc *self,
								  const gchar *uri,
                                                                  const gchar *filename,
								  GCancellable *cancellable,
								  GAsyncReadyCallback callback,
								  gpointer user_data,
								  ...) G_GNUC_NULL_TERMINATED;

gssize          eam_wc_request_finish                            (EamWc *self,
								  GAsyncResult *result,
								  GError **error);

void           eam_wc_set_log_level                              (EamWc *self,
								  guint level);

G_END_DECLS


#endif /* EAM_WC_H */

/* Copyright 2014 Endless Mobile, Inc. */

#ifndef EAM_WC_FILE_H
#define EAM_WC_FILE_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define EAM_TYPE_WC_FILE (eam_wc_file_get_type ())

#define EAM_WC_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), EAM_TYPE_WC_FILE, EamWcFile))

#define EAM_IS_WC_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EAM_TYPE_WC_FILE))

#define EAM_WC_FILE_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), EAM_TYPE_WC_FILE, EamWcFileClass))

#define EAM_IS_WC_FILE_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), EAM_TYPE_WC_FILE))

#define EAM_WC_FILE_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EAM_TYPE_WC_FILE, EamWcFileClass))

typedef struct _EamWcFile        EamWcFile;
typedef struct _EamWcFileClass   EamWcFileClass;

/**
 * EamWcFile:
 * @parent: the parent object struct
 */
struct _EamWcFile
{
  GObject parent;
};

/**
 * EamWcFileClass:
 * @parent_class: the parent class structure
 *
 * Endless Application Manager web client output file helper class.
 *
 * It's the output file of the web client to be used to download
 * content from Internet.
 */
struct _EamWcFileClass
{
  GObjectClass parent_class;
};

GType           eam_wc_file_get_type                             (void) G_GNUC_CONST;

EamWcFile      *eam_wc_file_new                                  (void);

void            eam_wc_file_open_async                           (EamWcFile *self,
								  const char *path,
								  GCancellable *cancellable,
								  GAsyncReadyCallback callback,
								  gpointer data);

gboolean        eam_wc_file_open_finish                          (EamWcFile *self,
								  GAsyncResult *result,
								  GError **error);

void            eam_wc_file_write_bytes_async                    (EamWcFile *self,
								  GBytes *bytes,
								  GCancellable *cancellable,
								  GAsyncReadyCallback callback,
								  gpointer data);

void            eam_wc_file_queue_bytes                          (EamWcFile *self,
                                                                  GBytes *buffer);

gboolean        eam_wc_file_write_bytes_finish                   (EamWcFile *self,
                                                                  GAsyncResult *result,
                                                                  GError **error);


gboolean        eam_wc_file_queueing                             (EamWcFile *self);

G_END_DECLS


#endif /* EAM_WC_FILE_H */

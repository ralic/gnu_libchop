#include <chop/chop.h>
#include <chop/serializable.h>
#include <chop/filters.h>
#include <chop/logs.h>

#include <errno.h>

#include <zlib.h>


/* Define `chop_zlib_zip_filter_t' which inherits from `chop_filter_t'.  */
CHOP_DECLARE_RT_CLASS (zlib_zip_filter, filter,
		       char *input_buffer;
		       size_t input_buffer_size;
		       z_stream zstream;);



static errcode_t
chop_zlib_zip_push (chop_filter_t *filter,
		    const char *buffer, size_t size);

static errcode_t
chop_zlib_zip_pull (chop_filter_t *filter, int flush,
		    char *buffer, size_t size, size_t *pulled);

static void
zlib_zip_filter_ctor (chop_object_t *object,
		      const chop_class_t *class)
{
  chop_zlib_zip_filter_t *zfilter;
  zfilter = (chop_zlib_zip_filter_t *)object;

  zfilter->filter.push = chop_zlib_zip_push;
  zfilter->filter.pull = chop_zlib_zip_pull;
  zfilter->zstream.zalloc = Z_NULL;
  zfilter->zstream.zfree = Z_NULL;
  zfilter->zstream.opaque = Z_NULL;
  chop_log_init ("zlib-zip-filter", &zfilter->filter.log);
}

CHOP_DEFINE_RT_CLASS (zlib_zip_filter, filter,
		      zlib_zip_filter_ctor, NULL,
		      NULL, NULL);

errcode_t
chop_zlib_zip_filter_init (int zlib_compression_level, size_t input_size,
			   chop_filter_t *filter)
{
  chop_zlib_zip_filter_t *zfilter;

  zfilter = (chop_zlib_zip_filter_t *)filter;

  chop_object_initialize ((chop_object_t *)filter,
			  &chop_zlib_zip_filter_class);

  input_size = input_size ? input_size : 1024;
  zfilter->input_buffer = malloc (input_size);
  if (!zfilter->input_buffer)
    return ENOMEM;

  zfilter->input_buffer_size = input_size;

  deflateInit (&zfilter->zstream,
	       (zlib_compression_level >= 0)
	       ? zlib_compression_level : Z_DEFAULT_COMPRESSION);

  zfilter->zstream.next_in = zfilter->input_buffer;
  zfilter->zstream.avail_in = 0;

  return 0;
}


/* The push and pull methods.  */
#define ZIP_TYPE        zlib
#define ZIP_DIRECTION   zip

#define ZIP_FLUSH       Z_FINISH
#define ZIP_NO_FLUSH    0
#define ZIP_STREAM_END  Z_STREAM_END
#define ZIP_OK          Z_OK
#define ZIP_NO_PROGRESS Z_BUF_ERROR

#define ZIP_PROCESS(_zstream, _flush)  deflate ((_zstream), (_flush))
#define ZIP_NEED_MORE_INPUT(_zstream, _zret)  (0)

#include "filter-zip-push-pull.c"


/* libchop -- a utility library for distributed storage and data backup
   Copyright (C) 2008, 2010, 2012  Ludovic Courtès <ludo@gnu.org>
   Copyright (C) 2005, 2006, 2007  Centre National de la Recherche Scientifique (LAAS-CNRS)

   Libchop is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Libchop is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with libchop.  If not, see <http://www.gnu.org/licenses/>.  */

/* Note: LZO 2.03 is GPLv2 or later (unlike previous releases of LZO 2.x,
   which were GPLv2-only), which allows libchop to be GPLv3+.

   <lzo1x.h> corresponds LZO version 1, and <lzo/lzo1x.h> corresponds LZO
   2.x.  */

#include <chop/chop-config.h>

#include <chop/chop.h>
#include <chop/objects.h>
#include <chop/filters.h>
#include <chop/logs.h>

#include <errno.h>
#include <assert.h>

#include <arpa/inet.h>
#include <stdlib.h>  /* For Gnulib's `malloc ()' */

#include <lzo/lzo1x.h>


/* Define `chop_lzo_zip_filter_t' which inherits from `chop_filter_t'.  */
CHOP_DECLARE_RT_CLASS_WITH_METACLASS (lzo_zip_filter, filter,
				      zip_filter_class,

				      lzo_voidp  work_mem;
				      lzo_bytep  input_buffer;
				      size_t     input_buffer_size;
				      size_t     avail_in;
				      size_t     input_offset;
				      lzo_bytep  output_buffer;
				      size_t     output_buffer_size;
				      size_t     avail_out;
				      size_t     output_offset;);


/* LZO global initialization.  */
extern chop_error_t chop_initialize_lzo (void);

chop_error_t
chop_initialize_lzo (void)
{
  static int initialized = 0;
  chop_error_t err = 0;

  if (!initialized)
    {
      err = lzo_init ();
      if (err != LZO_E_OK)
	err = CHOP_FILTER_ERROR;
      else
	{
	  err = 0;
	  initialized = 1;
	}
    }

  return err;
}


static chop_error_t
chop_lzo_zip_pull (chop_filter_t *filter, int flush,
		   char *buffer, size_t size, size_t *pulled)
{
  chop_error_t err = 0;
  chop_lzo_zip_filter_t *zfilter;

  zfilter = (chop_lzo_zip_filter_t *) filter;

  *pulled = 0;
  while ((*pulled < size) && (err == 0))
    {
      if (zfilter->avail_out > 0)
	{
	  /* Pull already compressed data.  */
	  size_t amount;

	  amount = (zfilter->avail_out > size) ? size : zfilter->avail_out;
	  memcpy (buffer, zfilter->output_buffer + zfilter->output_offset,
		  amount);
	  *pulled                += amount;
	  buffer                 += amount;
	  size                   -= amount;
	  zfilter->output_offset += amount;
	  zfilter->avail_out     -= amount;
	  if (zfilter->avail_out == 0)
	    zfilter->output_offset = 0;
	}
      else if ((zfilter->avail_in < zfilter->input_buffer_size)
	       && (!flush))
	{
	  /* Ask for more input data.  */
	  size_t howmuch;

	  howmuch = zfilter->input_buffer_size - zfilter->avail_in;
	  chop_log_printf (&filter->log,
			   "filter is empty, input fault "
			   "(requesting %zu bytes)",
			   howmuch);

	  err = chop_filter_handle_input_fault (filter, howmuch);
	  if (err)
	    {
	      chop_log_printf (&filter->log,
			       "input fault unhandled: %s",
			       chop_error_message (err));
	      if (err == CHOP_FILTER_UNHANDLED_FAULT)
		err = CHOP_FILTER_EMPTY;
	    }
	}
      else if (zfilter->avail_in > 0)
	{
	  /* Compress data to FILTER's output buffer.  */

	  /* Actually compress.  */
	  chop_log_printf (&filter->log,
			   "pull: processing stream (input: %zu, output: %zu, "
			   "flush: %s)",
			   zfilter->avail_in, zfilter->avail_out,
			   flush ? "yes" : "no");

	  zfilter->avail_out = zfilter->output_buffer_size - 8;
	  err = lzo1x_1_compress (zfilter->input_buffer, zfilter->avail_in,
				  zfilter->output_buffer + 8,
				  &zfilter->avail_out,
				  zfilter->work_mem);

	  if (err != LZO_E_OK)
	    err = CHOP_FILTER_ERROR;
	  else
	    {
	      /* Encode `avail_in' and `avail_out' to ease per-block
		 decompression.  */

	      /* Make sure LZO provided us with at most what we asked for.
		 This assertion fails sometimes with LZO 2.03, a problem that
		 LZO 2.06 does not have.  */
	      assert (zfilter->avail_out + 8 <= zfilter->output_buffer_size);

	      if ((zfilter->avail_in & ~0xffffffffUL)
		  || (zfilter->avail_out & ~0xffffffffUL))
		{
		  chop_log_printf (&filter->log,
				   "pull: fatal: buffer sizes are too "
				   "large (%zu in and %zu out)",
				   zfilter->avail_in, zfilter->avail_out);
		  err = CHOP_FILTER_ERROR;
		}
	      else
		{
		  unsigned long in32, out32;

		  in32  = htonl (zfilter->avail_in);
		  out32 = htonl (zfilter->avail_out);
		  memcpy (zfilter->output_buffer, &out32, 4);
		  memcpy (zfilter->output_buffer + 4, &in32, 4);

		  err = 0;
		  zfilter->avail_in = 0;
		  zfilter->avail_out += 8;
		}
	    }
	}
      else
	{
	  /* No more input data, flushing.  */
	  if (*pulled == 0)
	    err = CHOP_FILTER_EMPTY;
	  else
	    break;
	}
    }

  return err;
}



static chop_error_t lzo_zip_filter_ctor (chop_object_t *object,
					 const chop_class_t *class);
static void lzo_zip_filter_dtor (chop_object_t *object);

chop_error_t
chop_lzo_zip_filter_init (size_t input_size, chop_filter_t *filter);

static chop_error_t
lzf_open (int compression_level, size_t input_size,
	  chop_filter_t *filter)
{
  /* FIXME: Handle COMPRESSION_LEVEL.  */

  return (chop_lzo_zip_filter_init (input_size, filter));
}

CHOP_DEFINE_RT_CLASS_WITH_METACLASS (lzo_zip_filter, filter,
				     zip_filter_class, /* Metaclass */

				     /* Metaclass inits.  */
				     .generic_open = lzf_open,

				     lzo_zip_filter_ctor,
				     lzo_zip_filter_dtor,
				     NULL, NULL, /* No copy, equalp */
				     NULL, NULL  /* No serial, deserial */);

chop_error_t
chop_lzo_zip_filter_init (size_t input_size, chop_filter_t *filter)
{
  chop_error_t err;
  chop_lzo_zip_filter_t *zfilter;

  zfilter = (chop_lzo_zip_filter_t *) filter;

  err = chop_initialize_lzo ();
  if (err)
    return err;

  err =
    chop_object_initialize ((chop_object_t *) filter,
			    (chop_class_t *) &chop_lzo_zip_filter_class);
  if (err)
    return err;

  input_size = input_size ? input_size : 8192;
  zfilter->input_buffer =
    (lzo_bytep) chop_malloc (input_size,
			     (chop_class_t *) &chop_lzo_zip_filter_class);
  if (!zfilter->input_buffer)
    goto mem_err;

  zfilter->input_buffer_size = input_size;

  /* The `LZO.TXT' file reads:

       When dealing with uncompressible data, LZO expands the input
       block by a maximum of 16 bytes per 1024 bytes input.

     Thus, we compute that size and add a few bytes for safety.  */
  zfilter->output_buffer_size = input_size + (input_size >> 6) + 100;
  zfilter->output_buffer =
    (lzo_bytep) chop_malloc (zfilter->output_buffer_size,
			     (chop_class_t *) &chop_lzo_zip_filter_class);
  if (!zfilter->output_buffer)
    goto mem_err;

  zfilter->work_mem =
    (lzo_voidp) chop_malloc (LZO1X_1_MEM_COMPRESS,
			     (chop_class_t *) &chop_lzo_zip_filter_class);
  if (!zfilter->work_mem)
    goto mem_err;

  return 0;

 mem_err:
  chop_object_destroy ((chop_object_t *) zfilter);
  return ENOMEM;
}


/* The push and pull methods.  */
#define ZIP_DIRECTION   zip


#include "filter-lzo-common.c"

/* arch-tag: 5f61b3a0-a19a-4dbc-aaf9-bd150e1ac262
 */

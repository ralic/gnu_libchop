/* libchop -- a utility library for distributed storage and data backup
   Copyright (C) 2008, 2010, 2012, 2013  Ludovic Courtès <ludo@gnu.org>
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

/* A filtered block store for debugging purposes.  */

#include <chop/chop-config.h>

#include <chop/chop.h>
#include <chop/stores.h>
#include <chop/buffers.h>
#include <chop/filters.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


/* Class definition (this has to be somewhere).  */

CHOP_DECLARE_RT_CLASS (filtered_block_store, block_store,
		       chop_filter_t *input_filter;
		       chop_filter_t *output_filter;
		       chop_block_store_t *backend;
		       chop_proxy_semantics_t backend_ps;);


static chop_error_t fbs_ctor (chop_object_t *, const chop_class_t *);

CHOP_DEFINE_RT_CLASS (filtered_block_store, block_store,
		      fbs_ctor, NULL, /* No constructor/destructor */
		      NULL, NULL, /* No copy/equalp */
		      NULL, NULL  /* No serializer/deserializer */);



static chop_error_t
chop_filtered_block_store_blocks_exist (chop_block_store_t *store,
					size_t n,
					const chop_block_key_t keys[n],
					bool exists[n])
{
  chop_filtered_block_store_t *filtered =
    (chop_filtered_block_store_t *)store;

  return chop_store_blocks_exist (filtered->backend, n, keys, exists);
}

static chop_error_t
chop_filtered_block_store_read_block (chop_block_store_t *store,
				      const chop_block_key_t *key,
				      chop_buffer_t *buffer,
				      size_t *size)
{
  chop_error_t err;
  chop_buffer_t unfiltered;
  chop_filtered_block_store_t *filtered =
    (chop_filtered_block_store_t *)store;

  err = chop_buffer_init (&unfiltered, 0);
  if (err)
    return err;

  *size = 0;
  err = chop_store_read_block (filtered->backend, key, &unfiltered, size);
  if (err)
    return err;

  /* Filter UNFILTERED through OUTPUT_FILTER and store the result in
     BUFFER.  */
  chop_buffer_clear (buffer);
  err = chop_filter_through (filtered->output_filter,
			     chop_buffer_content (&unfiltered),
			     chop_buffer_size (&unfiltered),
			     buffer);

  chop_buffer_return (&unfiltered);

  *size = chop_buffer_size (buffer);

  return err;
}

static chop_error_t
chop_filtered_block_store_write_block (chop_block_store_t *store,
				       const chop_block_key_t *key,
				       const char *block, size_t size)
{
  chop_error_t err;
  chop_buffer_t filtered_buffer;
  chop_filtered_block_store_t *filtered =
    (chop_filtered_block_store_t *)store;

  err = chop_buffer_init (&filtered_buffer, size);
  if (err)
    return err;

  err = chop_filter_through (filtered->input_filter,
			     block, size, &filtered_buffer);
  if (err)
    return err;

  err = chop_store_write_block (filtered->backend, key,
				chop_buffer_content (&filtered_buffer),
				chop_buffer_size (&filtered_buffer));

  chop_buffer_return (&filtered_buffer);

  return err;
}

static chop_error_t
chop_filtered_block_store_delete_block (chop_block_store_t *store,
					const chop_block_key_t *key)
{
  chop_error_t err;
  chop_filtered_block_store_t *filtered =
    (chop_filtered_block_store_t *)store;

  err = chop_store_delete_block (filtered->backend, key);

  return err;
}

static chop_error_t
chop_filtered_block_store_first_block (chop_block_store_t *store,
				       chop_block_iterator_t *it)
{
  chop_error_t err;
  chop_filtered_block_store_t *filtered =
    (chop_filtered_block_store_t *)store;

  err = chop_store_first_block (filtered->backend, it);

  return err;
}

static chop_error_t
chop_filtered_block_store_sync (chop_block_store_t *store)
{
  chop_error_t err;
  chop_filtered_block_store_t *filtered =
    (chop_filtered_block_store_t *)store;

  err = chop_store_sync (filtered->backend);

  return err;
}

static chop_error_t
chop_filtered_block_store_close (chop_block_store_t *store)
{
  chop_error_t err = 0;
  chop_filtered_block_store_t *filtered;

  filtered = (chop_filtered_block_store_t *)store;
  switch (filtered->backend_ps)
    {
    case CHOP_PROXY_LEAVE_AS_IS:
      break;

    case CHOP_PROXY_EVENTUALLY_CLOSE:
    case CHOP_PROXY_EVENTUALLY_DESTROY:
    case CHOP_PROXY_EVENTUALLY_FREE:
      err = chop_store_close (filtered->backend);
      break;

    default:
      abort ();
    }

  return err;
}


/* The constructor.  */
static chop_error_t
fbs_ctor (chop_object_t *object, const chop_class_t *class)
{
  chop_block_store_t *store;

  store = (chop_block_store_t *)object;

  store->name = NULL;
  store->blocks_exist = chop_filtered_block_store_blocks_exist;
  store->read_block = chop_filtered_block_store_read_block;
  store->write_block = chop_filtered_block_store_write_block;
  store->delete_block = chop_filtered_block_store_delete_block;
  store->first_block = chop_filtered_block_store_first_block;
  store->close = chop_filtered_block_store_close;
  store->sync = chop_filtered_block_store_sync;

  return 0;
}


chop_error_t
chop_filtered_store_open (chop_filter_t *input_filter,
			  chop_filter_t *output_filter,
			  chop_block_store_t *backend,
			  chop_proxy_semantics_t bps,
			  chop_block_store_t *store)
{
  chop_error_t err;
  chop_filtered_block_store_t *filtered =
    (chop_filtered_block_store_t *)store;

  err = chop_object_initialize ((chop_object_t *)store,
				&chop_filtered_block_store_class);
  if (err)
    return err;

  store->iterator_class = chop_store_iterator_class (backend);
  filtered->input_filter = input_filter;
  filtered->output_filter = output_filter;
  filtered->backend = backend;
  filtered->backend_ps = bps;

  return 0;
}

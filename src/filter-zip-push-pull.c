/* Template for the push and pull methods for zip/unzip filters.  This file
   expects a number of macros to be defined.  It is designed in such a way
   that it can be reused for both compression/decompression, and both zlib
   and bzlib.  */

#if (!defined ZIP_TYPE) || (!defined ZIP_DIRECTION)
# error "This file is meant to be included in some other source file."
#endif


#ifndef CONCAT5
# define _CONCAT5(_x, _y, _z, _p, _q)  _x ## _y ## _z ## _p ## _q
# define CONCAT5(_a, _b, _c, _d, _e)  _CONCAT5 (_a, _b, _c, _d, _e)
#endif

#define ZIP_PUSH_METHOD  CONCAT5 (chop_, ZIP_TYPE, _, ZIP_DIRECTION, _push)
#define ZIP_PULL_METHOD  CONCAT5 (chop_, ZIP_TYPE, _, ZIP_DIRECTION, _pull)
#define ZIP_FILTER_TYPE  CONCAT5 (chop_, ZIP_TYPE, _, ZIP_DIRECTION, _filter_t)

static errcode_t
ZIP_PUSH_METHOD (chop_filter_t *filter,
		 const char *buffer, size_t size)
{
  errcode_t err;
  ZIP_FILTER_TYPE *zfilter;

  zfilter = (ZIP_FILTER_TYPE *)filter;

  while (size > 0)
    {
      size_t available, amount;

      if (zfilter->zstream.avail_in >= zfilter->input_buffer_size)
	{
	  chop_log_printf (&filter->log, "filter is full, output fault");
	  err = chop_filter_handle_output_fault (filter,
						 zfilter->input_buffer_size);
	  if (err)
	    {
	      chop_log_printf (&filter->log,
			       "filter-full event unhandled: %s",
			       error_message (err));
	      if ((err != 0) && (err != CHOP_FILTER_UNHANDLED_FAULT))
		return err;

	      return CHOP_FILTER_FULL;
	    }

	  continue;
	}

      available = zfilter->input_buffer_size - zfilter->zstream.avail_in;
      amount = (available > size) ? size : available;
      memcpy (zfilter->zstream.next_in, buffer, amount);

      zfilter->zstream.next_in += amount;
      zfilter->zstream.avail_in += amount;
      buffer += amount;
      size -= amount;
    }

  return 0;
}

static errcode_t
ZIP_PULL_METHOD (chop_filter_t *filter, int flush,
		 char *buffer, size_t size, size_t *pulled)
{
  int zret = ZIP_OK;
  errcode_t err;
  ZIP_FILTER_TYPE *zfilter;

  zfilter = (ZIP_FILTER_TYPE *)filter;

  zfilter->zstream.avail_out = size;
  zfilter->zstream.next_out = buffer;
  *pulled = 0;
  while (size > 0)
    {
      if (((zfilter->zstream.avail_in == 0)
	   || ZIP_NEED_MORE_INPUT (&zfilter->zstream, zret))
	  && (!flush))
	{
	  chop_log_printf (&filter->log, "filter is empty, input fault");
	  err = chop_filter_handle_input_fault (filter,
						zfilter->input_buffer_size);
	  if (err)
	    {
	      chop_log_printf (&filter->log,
			       "input fault unhandled: %s",
			       error_message (err));
	      if ((err != 0) && (err != CHOP_FILTER_UNHANDLED_FAULT))
		return err;

	      return CHOP_FILTER_EMPTY;
	    }

	  continue;
	}


      chop_log_printf (&filter->log,
		       "pull: processing stream (input: %u, output: %u, "
		       "flush: %s)",
		       zfilter->zstream.avail_in, zfilter->zstream.avail_out,
		       flush ? "yes" : "no");
      zret = ZIP_PROCESS (&zfilter->zstream,
			  flush ? ZIP_FLUSH : ZIP_NO_FLUSH);
      if (flush)
	{
	  *pulled += size - zfilter->zstream.avail_out;
	  if (zret == ZIP_STREAM_END)
	    /* We're done with this.  */
	    return CHOP_FILTER_EMPTY;

	  /* There is data remaining to be flushed so the user must call us
	     again (with FLUSH set again).  */
	  chop_log_printf (&filter->log, "pull: flush was requested but "
			   "data is still available; user should call "
			   "again");
	  return 0;
	}
      else
	*pulled += size - zfilter->zstream.avail_out;
    }

  return 0;
}

#undef ZIP_PUSH_METHOD
#undef ZIP_PULL_METHOD
#undef ZIP_FILTER_TYPE
/* This program shows the use of the anchor-based chopper in order to
   discover similarities between two files, following the algorithm described
   by Udi Manber in [1].  It basically uses the anchor-based chopper to
   chop two files into blocks and then stores fingerprints (digests) for each
   of those blocks.  It then traverse both lists of blocks and compute the
   number of common blocks and common bytes between these two files.


   [1] Udi Manber.  Finding similar files in a large file system.
       In Proceedings of the Usenix Winter 1994 Conference, pages 1--10,
       January, 1994, http://www.cs.arizona.edu/research/reports.html.  */


#include <chop/chop.h>
#include <chop/streams.h>
#include <chop/choppers.h>

#include <alloca.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>


static int debug = 0;

static const char *program_name = NULL;

/* Information about a file block.  */
typedef struct
{
  void *digest;
  size_t digest_size;
  size_t offset;
  size_t size;
} block_info_t;

/* Information about a file's blocks.  */
typedef struct
{
  chop_buffer_t vector;
  size_t total_size;
} block_info_vector_t;



/* Read input stream throught CHOPPER and fill in VECTOR with block
   information using METHOD as the block hash method for `block_info_t'
   objects.  */
static errcode_t
read_stream (chop_chopper_t *chopper, chop_hash_method_t method,
	     block_info_vector_t *vector)
{
  errcode_t err;
  block_info_t info;
  chop_buffer_t buffer;
  size_t offset = 0, hash_size;

  hash_size = chop_hash_size (method);
  vector->total_size = 0;
  chop_buffer_init (&buffer, chop_chopper_typical_block_size (chopper));
  chop_buffer_init (&vector->vector, 0);
  while (1)
    {
      size_t size;

      /* Read one block.  */
      chop_buffer_clear (&buffer);
      err = chop_chopper_read_block (chopper, &buffer, &size);
      if ((err) && (err != CHOP_STREAM_END))
	{
	  com_err (program_name, err, "while reading block");
	  return 2;
	}

      if (err == CHOP_STREAM_END)
	break;

      vector->total_size += size;

      /* Append information about this block to VECTOR */
      info.offset = offset;
      info.size = size;
      info.digest_size = hash_size;
      info.digest = malloc (hash_size);
      if (!info.digest)
	return ENOMEM;

      chop_hash_buffer (method, chop_buffer_content (&buffer),
			chop_buffer_size (&buffer), info.digest);

      chop_buffer_append (&vector->vector, (char *)&info, sizeof (info));
    }

  /* Append a zeroed block info.  */
  memset (&info, 0, sizeof (info));
  chop_buffer_append (&vector->vector, (char *)&info, sizeof (info));

  chop_buffer_return (&buffer);

  return err;
}

/* Search a block identical to BLOCK in VECTOR.  Return non-zero if
   found.  */
static int
search_identical_block (const block_info_t *block,
			const block_info_vector_t *vector)
{
  block_info_t *b;

  for (b = (block_info_t *)chop_buffer_content (&vector->vector);
       b->digest;
       b++)
    {
      if (!memcmp (block->digest, b->digest, b->digest_size))
	return 1;
    }

  return 0;
}

#define LONGEST_VECTOR(_v1, _v2) \
(((_v1)->total_size > (_v2)->total_size) ? (_v1) : (_v2))

#define THE_OTHER_ONE(_ref, _a, _b) \
(((_a) == (_ref)) ? (_b) : (_a))


/* Display the level of similarity between the file represented by VECTOR1
   and the one represented by VECTOR2.  */
static void
show_similarities (block_info_vector_t *vector1,
		   block_info_vector_t *vector2)
{
  size_t common_blocks = 0, common_bytes = 0, total_blocks = 0;
  block_info_vector_t *longest = LONGEST_VECTOR (vector1, vector2);
  block_info_vector_t *shortest = THE_OTHER_ONE (longest, vector1, vector2);
  block_info_t *block =
    (block_info_t *)chop_buffer_content (&longest->vector);

  for (; block->digest; block++)
    {
      total_blocks++;
      if (search_identical_block (block, shortest))
	{
	  common_blocks++;
	  common_bytes += block->size;
	}
    }

  printf ("%u/%u common blocks, %u/%u common bytes\n",
	  common_blocks, total_blocks,
	  common_bytes, longest->total_size);
  printf ("similarity (block level): %.0f%%\n",
	  (common_blocks / (float)total_blocks) * 100);
  printf ("similarity (byte level): %.0f%%\n",
	  (common_bytes / (float)longest->total_size) * 100);
}


int
main (int argc, char *argv[])
{
  errcode_t err;
  chop_stream_t *stream1, *stream2;
  chop_chopper_t *chopper1, *chopper2;
  chop_log_t *chopper_log;
  block_info_vector_t blocks1, blocks2;

  program_name = argv[0];
  if (argc < 3)
    return 1;

  err = chop_init ();
  if (err)
    {
      com_err (argv[0], err, "while initializing libchop");
      return 1;
    }

  stream1  = chop_class_alloca_instance (&chop_file_stream_class);
  stream2  = chop_class_alloca_instance (&chop_file_stream_class);
  chopper1 = chop_class_alloca_instance (&chop_anchor_based_chopper_class);
  chopper2 = chop_class_alloca_instance (&chop_anchor_based_chopper_class);

  err = chop_file_stream_open (argv[1], stream1);
  if (err)
    {
      com_err (argv[0], err, "%s", argv[1]);
      return 1;
    }

  err = chop_file_stream_open (argv[2], stream2);
  if (err)
    {
      com_err (argv[0], err, "%s", argv[2]);
      return 1;
    }

  err = chop_anchor_based_chopper_init (stream1, 10, chopper1);
  if (err)
    {
      com_err (argv[0], err, "anchor-based-chopper");
      return 1;
    }

  err = chop_anchor_based_chopper_init (stream2, 10, chopper2);
  if (err)
    {
      com_err (argv[0], err, "anchor-based-chopper");
      return 1;
    }

  if (debug)
    {
      /* Output debugging messages to `stderr'.  */
      chopper_log = chop_anchor_based_chopper_log (chopper1);
      chop_log_attach (chopper_log, 2, 0);
    }

  err = read_stream (chopper1, CHOP_HASH_MD5, &blocks1);
  if ((err) && (err != CHOP_STREAM_END))
    {
      com_err (argv[0], err, "while reading from chopper1");
      return 1;
    }

  err = read_stream (chopper2, CHOP_HASH_MD5, &blocks2);
  if ((err) && (err != CHOP_STREAM_END))
    {
      com_err (argv[0], err, "while reading from chopper2");
      return 1;
    }

  show_similarities (&blocks1, &blocks2);

  return 0;
}
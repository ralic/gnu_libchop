/* Template for the GDBM and TDB block stores.  */

#ifndef DB_TYPE
# error "This file is meant to be included in some other source file."
#endif


#ifndef CONCAT4
# define _CONCAT4(_x, _y, _z, _p) _x ## _y ## _z ## _p
# define CONCAT4(_a, _b, _c, _d)  _CONCAT4 (_a, _b, _c, _d)
#endif

#define DB_BLOCK_EXISTS_METHOD CONCAT4 (chop_, DB_TYPE, _, block_exists)
#define DB_READ_BLOCK_METHOD   CONCAT4 (chop_, DB_TYPE, _, read_block)
#define DB_WRITE_BLOCK_METHOD  CONCAT4 (chop_, DB_TYPE, _, write_block)
#define DB_DELETE_BLOCK_METHOD CONCAT4 (chop_, DB_TYPE, _, delete_block)
#define DB_FIRST_KEY_METHOD    CONCAT4 (chop_, DB_TYPE, _, first_key)
#define DB_NEXT_KEY_METHOD     CONCAT4 (chop_, DB_TYPE, _, next_key)
#define DB_SYNC_METHOD         CONCAT4 (chop_, DB_TYPE, _, sync)
#define DB_CLOSE_METHOD        CONCAT4 (chop_, DB_TYPE, _, close)

#define DB_STORE_TYPE          CONCAT4 (chop_, DB_TYPE, _, block_store_t)


static void
do_free_key (char *content, void *user)
{
  if (content)
    free (content);
}


static errcode_t
DB_BLOCK_EXISTS_METHOD (chop_block_store_t *store,
			const chop_block_key_t *key,
			int *exists)
{
  DB_DATA_TYPE db_key;
  DB_STORE_TYPE *db = (DB_STORE_TYPE *)store;

  CHOP_KEY_TO_DB (&db_key, key);
  *exists = DB_EXISTS (db->db, db_key);

  return 0;
}

static errcode_t
DB_READ_BLOCK_METHOD (chop_block_store_t *store,
		      const chop_block_key_t *key, chop_buffer_t *buffer,
		      size_t *size)
{
  errcode_t err;
  DB_DATA_TYPE db_key, db_content;
  DB_STORE_TYPE *gdbm = (DB_STORE_TYPE *)store;

  CHOP_KEY_TO_DB (&db_key, key);

  DB_READ (gdbm->db, db_key, &db_content);
  if (!db_content.dptr)
    {
      *size = 0;
      return CHOP_STORE_BLOCK_UNAVAIL;
    }

  err = chop_buffer_push (buffer, db_content.dptr, db_content.dsize);
  *size = db_content.dsize;

  free (db_content.dptr);

  return err;
}

static errcode_t
DB_WRITE_BLOCK_METHOD (chop_block_store_t *store,
		       const chop_block_key_t *key,
		       const char *buffer, size_t size)
{
  int err;
  DB_STORE_TYPE *gdbm = (DB_STORE_TYPE *)store;
  DB_DATA_TYPE db_key, db_content;

  CHOP_KEY_TO_DB (&db_key, key);
  db_content.dptr = (char *)buffer;
  db_content.dsize = size;

  err = DB_WRITE (gdbm->db,
		  db_key, db_content,
		  DB_WRITE_REPLACE_FLAG /* FIXME: Really? */);
  if (err)
    return CHOP_STORE_ERROR;

  return 0;
}

static errcode_t
DB_DELETE_BLOCK_METHOD (chop_block_store_t *store,
			const chop_block_key_t *key)
{
  int err;
  DB_STORE_TYPE *db = (DB_STORE_TYPE *)store;
  DB_DATA_TYPE db_key;

  CHOP_KEY_TO_DB (&db_key, key);
  err = DB_DELETE (db->db, db_key);
  if (err)
    return CHOP_STORE_BLOCK_UNAVAIL;

  return 0;
}

static errcode_t
DB_FIRST_KEY_METHOD (chop_block_store_t *store,
		     chop_block_key_t *key)
{
  errcode_t err;
  DB_DATA_TYPE db_key;
  DB_STORE_TYPE *db = (DB_STORE_TYPE *)store;

  db_key = DB_FIRST_KEY (db->db);
  if (db_key.dptr != NULL)
    {
      chop_block_key_init (key, db_key.dptr, db_key.dsize,
			   do_free_key, NULL);
      err = 0;
    }
  else
    err = CHOP_STORE_END;

  return (err);
}

static errcode_t
DB_NEXT_KEY_METHOD (chop_block_store_t *store,
		    const chop_block_key_t *key,
		    chop_block_key_t *next)
{
  errcode_t err;
  DB_DATA_TYPE db_key, db_next_key;
  DB_STORE_TYPE *db = (DB_STORE_TYPE *)store;

  CHOP_KEY_TO_DB (&db_key, key);
  db_next_key = DB_NEXT_KEY (db->db, db_key);
  if (db_next_key.dptr != NULL)
    {
      chop_block_key_init (next, db_next_key.dptr, db_next_key.dsize,
			   do_free_key, NULL);
      err = 0;
    }
  else
    err = CHOP_STORE_END;

  return (err);
}

#ifdef __GNUC__
# define _CHOP_UNUSED __attribute__ ((__unused__))
#else
# define _CHOP_UNUSED
#endif

static errcode_t
DB_SYNC_METHOD (chop_block_store_t *store)
{
  DB_STORE_TYPE *gdbm _CHOP_UNUSED = (DB_STORE_TYPE *)store;

  DB_SYNC (gdbm->db);

  return 0;
}

#undef _CHOP_UNUSED

static errcode_t
DB_CLOSE_METHOD (chop_block_store_t *store)
{
  DB_STORE_TYPE *gdbm = (DB_STORE_TYPE *)store;

  /* `db_close ()' calls `db_sync ()' */
  DB_CLOSE (gdbm->db);

  return 0;
}
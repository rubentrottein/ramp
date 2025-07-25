/* Copyright (c) 2005, 2016, Oracle and/or its affiliates.
   Copyright (c) 2009, 2020, MariaDB Corporation.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1335  USA */

#ifndef LOG_H
#define LOG_H

#include "handler.h"                            /* my_xid */
#include "rpl_constants.h"

class Relay_log_info;
class Gtid_index_writer;

class Format_description_log_event;
class Gtid_log_event;

bool reopen_fstreams(const char *filename, FILE *outstream, FILE *errstream);
void setup_log_handling();
bool trans_has_updated_trans_table(const THD* thd);
bool stmt_has_updated_trans_table(const THD *thd);
bool use_trans_cache(const THD* thd, bool is_transactional);
bool ending_trans(THD* thd, const bool all);
bool ending_single_stmt_trans(THD* thd, const bool all);
bool trans_has_updated_non_trans_table(const THD* thd);
bool stmt_has_updated_non_trans_table(const THD* thd);

/*
  Transaction Coordinator log - a base abstract class
  for two different implementations
*/
class TC_LOG
{
  public:
  int using_heuristic_recover();
  TC_LOG() = default;
  virtual ~TC_LOG() = default;

  virtual int open(const char *opt_name)=0;
  virtual void close()=0;
  /*
    Transaction coordinator 2-phase commit.

    Must invoke the run_prepare_ordered and run_commit_ordered methods, as
    described below for these methods.

    In addition, must invoke THD::wait_for_prior_commit(), or equivalent
    wait, to ensure that one commit waits for another if registered to do so.
  */
  virtual int log_and_order(THD *thd, my_xid xid, bool all,
                            bool need_prepare_ordered,
                            bool need_commit_ordered) = 0;
  virtual int unlog(ulong cookie, my_xid xid)=0;
  virtual int unlog_xa_prepare(THD *thd, bool all)= 0;
  virtual void commit_checkpoint_notify(void *cookie)= 0;

protected:
  /*
    These methods are meant to be invoked from log_and_order() implementations
    to run any prepare_ordered() respectively commit_ordered() methods in
    participating handlers.

    They must be called using suitable thread syncronisation to ensure that
    they are each called in the correct commit order among all
    transactions. However, it is only necessary to call them if the
    corresponding flag passed to log_and_order is set (it is safe, but not
    required, to call them when the flag is false).

    The caller must be holding LOCK_prepare_ordered respectively
    LOCK_commit_ordered when calling these methods.
  */
  void run_prepare_ordered(THD *thd, bool all);
  void run_commit_ordered(THD *thd, bool all);
};

/*
  Locks used to ensure serialised execution of TC_LOG::run_prepare_ordered()
  and TC_LOG::run_commit_ordered(), or any other code that calls handler
  prepare_ordered() or commit_ordered() methods.
*/
extern mysql_mutex_t LOCK_prepare_ordered;
extern mysql_cond_t COND_prepare_ordered;
extern mysql_mutex_t LOCK_after_binlog_sync;
extern mysql_mutex_t LOCK_commit_ordered;
#ifdef HAVE_PSI_INTERFACE
extern PSI_mutex_key key_LOCK_prepare_ordered, key_LOCK_commit_ordered;
extern PSI_mutex_key key_LOCK_after_binlog_sync;
extern PSI_cond_key key_COND_prepare_ordered;
#endif

class TC_LOG_DUMMY: public TC_LOG // use it to disable the logging
{
public:
  TC_LOG_DUMMY() = default;
  int open(const char *opt_name) override        { return 0; }
  void close() override                          { }
  /*
    TC_LOG_DUMMY is only used when there are <= 1 XA-capable engines, and we
    only use internal XA during commit when >= 2 XA-capable engines
    participate.
  */
  int log_and_order(THD *thd, my_xid xid, bool all,
                    bool need_prepare_ordered, bool need_commit_ordered) override
  {
    DBUG_ASSERT(0);
    return 1;
  }
  int unlog(ulong cookie, my_xid xid) override  { return 0; }
  int unlog_xa_prepare(THD *thd, bool all) override
  {
    return 0;
  }
  void commit_checkpoint_notify(void *cookie) override { DBUG_ASSERT(0); };
};

#define TC_LOG_PAGE_SIZE   8192

#ifdef HAVE_MMAP
class TC_LOG_MMAP: public TC_LOG
{
  public:                // only to keep Sun Forte on sol9x86 happy
  typedef enum {
    PS_POOL,                 // page is in pool
    PS_ERROR,                // last sync failed
    PS_DIRTY                 // new xids added since last sync
  } PAGE_STATE;

  struct pending_cookies {
    uint count;
    uint pending_count;
    ulong cookies[1];
  };

  private:
  typedef struct st_page {
    struct st_page *next; // page a linked in a fifo queue
    my_xid *start, *end;  // usable area of a page
    my_xid *ptr;          // next xid will be written here
    int size, free;       // max and current number of free xid slots on the page
    int waiters;          // number of waiters on condition
    PAGE_STATE state;     // see above
    mysql_mutex_t lock; // to access page data or control structure
    mysql_cond_t  cond; // to wait for a sync
  } PAGE;

  /* List of THDs for which to invoke commit_ordered(), in order. */
  struct commit_entry
  {
    struct commit_entry *next;
    THD *thd;
  };

  char logname[FN_REFLEN];
  File fd;
  my_off_t file_length;
  uint npages, inited;
  uchar *data;
  struct st_page *pages, *syncing, *active, *pool, **pool_last_ptr;
  /*
    note that, e.g. LOCK_active is only used to protect
    'active' pointer, to protect the content of the active page
    one has to use active->lock.
    Same for LOCK_pool and LOCK_sync
  */
  mysql_mutex_t LOCK_active, LOCK_pool, LOCK_sync, LOCK_pending_checkpoint;
  mysql_cond_t COND_pool, COND_active;
  /*
    Queue of threads that need to call commit_ordered().
    Access to this queue must be protected by LOCK_prepare_ordered.
  */
  commit_entry *commit_ordered_queue;
  /*
    This flag and condition is used to reserve the queue while threads in it
    each run the commit_ordered() methods one after the other. Only once the
    last commit_ordered() in the queue is done can we start on a new queue
    run.

    Since we start this process in the first thread in the queue and finish in
    the last (and possibly different) thread, we need a condition variable for
    this (we cannot unlock a mutex in a different thread than the one who
    locked it).

    The condition is used together with the LOCK_prepare_ordered mutex.
  */
  mysql_cond_t COND_queue_busy;
  my_bool commit_ordered_queue_busy;
  pending_cookies* pending_checkpoint;

  public:
  TC_LOG_MMAP(): inited(0), pending_checkpoint(0) {}
  int open(const char *opt_name) override;
  void close() override;
  int log_and_order(THD *thd, my_xid xid, bool all,
                    bool need_prepare_ordered, bool need_commit_ordered) override;
  int unlog(ulong cookie, my_xid xid) override;
  int unlog_xa_prepare(THD *thd, bool all) override
  {
    return 0;
  }
  void commit_checkpoint_notify(void *cookie) override;
  int recover();

  private:
  int log_one_transaction(my_xid xid);
  void get_active_from_pool();
  int sync();
  int overflow();
  int delete_entry(ulong cookie);
};
#else
#define TC_LOG_MMAP TC_LOG_DUMMY
#endif

extern TC_LOG *tc_log;
extern TC_LOG_MMAP tc_log_mmap;
extern TC_LOG_DUMMY tc_log_dummy;

/* log info errors */
#define LOG_INFO_EOF -1
#define LOG_INFO_IO  -2
#define LOG_INFO_INVALID -3
#define LOG_INFO_SEEK -4
#define LOG_INFO_MEM -6
#define LOG_INFO_FATAL -7
#define LOG_INFO_IN_USE -8
#define LOG_INFO_EMFILE -9


/* bitmap to SQL_LOG::close() */
#define LOG_CLOSE_INDEX		1
#define LOG_CLOSE_TO_BE_OPENED	2
#define LOG_CLOSE_STOP_EVENT	4
#define LOG_CLOSE_DELAYED_CLOSE 8
#define LOG_CLOSE_SYNC_GTID_INDEX 16

/* 
  Maximum unique log filename extension.
  Note: setting to 0x7FFFFFFF due to atol windows 
        overflow/truncate.
 */
#define MAX_LOG_UNIQUE_FN_EXT 0x7FFFFFFF

/* 
   Number of warnings that will be printed to error log
   before extension number is exhausted.
*/
#define LOG_WARN_UNIQUE_FN_EXT_LEFT 1000

class Relay_log_info;

/*
  Note that we destroy the lock mutex in the desctructor here.
  This means that object instances cannot be destroyed/go out of scope,
  until we have reset thd->current_linfo to NULL;
 */
typedef struct st_log_info
{
  char log_file_name[FN_REFLEN];
  my_off_t index_file_offset, index_file_start_offset;
  my_off_t pos;
  bool fatal; // if the purge happens to give us a negative offset
  st_log_info() : index_file_offset(0), index_file_start_offset(0),
      pos(0), fatal(0)
  {
    DBUG_ENTER("LOG_INFO");
    log_file_name[0] = '\0';
    DBUG_VOID_RETURN;
  }
} LOG_INFO;

/*
  Currently we have only 3 kinds of logging functions: old-fashioned
  logs, stdout and csv logging routines.
*/
#define MAX_LOG_HANDLERS_NUM 3

/* log event handler flags */
#define LOG_NONE       1U
#define LOG_FILE       2U
#define LOG_TABLE      4U

class Log_event;
class Rows_log_event;

enum enum_log_type { LOG_UNKNOWN, LOG_NORMAL, LOG_BIN };
enum enum_log_state { LOG_OPENED, LOG_CLOSED, LOG_TO_BE_OPENED };

/*
  Use larger buffers when reading from and to binary log
  We make it one step smaller than 64K to account for malloc overhead.
*/
#define LOG_BIN_IO_SIZE MY_ALIGN_DOWN(65536-1, IO_SIZE)

/*
  TODO use mmap instead of IO_CACHE for binlog
  (mmap+fsync is two times faster than write+fsync)
*/

class MYSQL_LOG
{
public:
  MYSQL_LOG();
  virtual ~MYSQL_LOG() = default;
  void init_pthread_objects();
  void cleanup();
  bool open(
#ifdef HAVE_PSI_INTERFACE
            PSI_file_key log_file_key,
#endif
            const char *log_name,
            enum_log_type log_type,
            const char *new_name, ulong next_file_number,
            enum cache_type io_cache_type_arg);
  void close(uint exiting);
  inline bool is_open() { return log_state != LOG_CLOSED; }
  const char *generate_name(const char *log_name,
                            const char *suffix,
                            bool strip_ext, char *buff);
  virtual int generate_new_name(char *new_name, const char *log_name,
                                ulong next_log_number);
  inline mysql_mutex_t* get_log_lock() { return &LOCK_log; }
 protected:
  /* LOCK_log is inited by init_pthread_objects() */
  mysql_mutex_t LOCK_log;
  char *name;
  char log_file_name[FN_REFLEN];
  char time_buff[20], db[NAME_LEN + 1];
  bool write_error, inited;
  IO_CACHE log_file;
  enum_log_type log_type;
  volatile enum_log_state log_state;
  enum cache_type io_cache_type;
  friend class Log_event;
#ifdef HAVE_PSI_INTERFACE
  /** Instrumentation key to use for file io in @c log_file */
  PSI_file_key m_log_file_key;
#endif

  bool init_and_set_log_file_name(const char *log_name,
                                  const char *new_name,
                                  ulong next_log_number,
                                  enum_log_type log_type_arg,
                                  enum cache_type io_cache_type_arg);
};

/**
  @struct Rows_event_factory

  Holds an event type code and a callback function to create it.
  Should be created by Rows_event_factory::get.
*/
struct Rows_event_factory
{
  int type_code;

  Rows_log_event *(*create)(THD*, TABLE*, ulonglong, bool is_transactional);

  template<class RowsEventT>
  static Rows_event_factory get()
  {
    return { RowsEventT::TYPE_CODE,
             [](THD* thd, TABLE* table, ulonglong flags, bool is_transactional)
                     -> Rows_log_event*
             {
               return new RowsEventT(thd, table, flags, is_transactional);
             }
    };
  }
};

class Event_log: public MYSQL_LOG
{
protected:
  /* binlog encryption data */
  struct Binlog_crypt_data crypto;

  mysql_mutex_t LOCK_binlog_end_pos;

  /** The instrumentation key to use for LOCK_binlog_end_pos. */
  PSI_mutex_key m_key_LOCK_binlog_end_pos;
  /** The instrumentation key to use for opening the log file. */
  PSI_file_key m_key_file_log, m_key_file_log_cache;
public:
#if !defined(MYSQL_CLIENT)
  Rows_log_event*
  prepare_pending_rows_event(THD *thd, TABLE* table,
                             binlog_cache_data *cache_data,
                             uint32 serv_id, size_t needed,
                             bool is_transactional,
                             Rows_event_factory event_factory);
#endif
  int flush_and_set_pending_rows_event(THD *thd, Rows_log_event* event,
                                       binlog_cache_data *cache_data,
                                       bool is_transactional);
  void set_write_error(THD *thd, bool is_transactional);
  static bool check_write_error(THD *thd);
  static bool check_cache_error(THD *thd, binlog_cache_data *cache_data);
  int write_cache(THD *thd, binlog_cache_data *cache_data);
  int write_cache_raw(THD *thd, IO_CACHE *cache);
  char* get_name() { return name; }
  void cleanup()
  {
    if (inited)
      mysql_mutex_destroy(&LOCK_binlog_end_pos);

    MYSQL_LOG::cleanup();
  }
  void init_pthread_objects()
  {
    MYSQL_LOG::init_pthread_objects();

    mysql_mutex_init(m_key_LOCK_binlog_end_pos, &LOCK_binlog_end_pos,
                     MY_MUTEX_INIT_SLOW);
  }

  bool open(enum cache_type io_cache_type_arg);
  virtual IO_CACHE *get_log_file() { return &log_file; }

  longlong write_description_event(enum_binlog_checksum_alg checksum_alg,
                                   bool encrypt, bool dont_set_created,
                                   bool is_relay_log);

  bool write_event(Log_event *ev, binlog_cache_data *data, IO_CACHE *file);
  bool write_event(Log_event *ev, enum enum_binlog_checksum_alg checksum_alg,
                   binlog_cache_data *data, IO_CACHE *file);
};

/**
  A single-reader, single-writer non-blocking layer for Event_log.
  Provides IO_CACHE for writing and IO_CACHE for reading.

  Writers use an overrided get_log_file version for their writes, while readers
  should use flip() to initiate reading.
  flip() swaps pointers to allow non-blocking reads.

  Writers can block other writers and a reader with a mutex, but a reader only
  swaps two pointers under a lock, so it won't block writers.

  TODO should be unnecessary after MDEV-24676 is done
 */
class Cache_flip_event_log: public Event_log {
  IO_CACHE alt_buf;
  IO_CACHE *current, *alt;
  std::atomic<uint> ref_count;
public:
  Cache_flip_event_log() : Event_log(),
                           current(&log_file), alt(&alt_buf), ref_count(1)
  { bzero(&alt_buf, sizeof(alt_buf)); }

  bool open(enum cache_type io_cache_type_arg)
  {
    log_file.dir= mysql_tmpdir;
    alt_buf.dir= log_file.dir;
    bool res= Event_log::open(io_cache_type_arg);
    if (res)
      return res;

    name= my_strdup(key_memory_MYSQL_LOG_name, "online-alter-binlog",
                    MYF(MY_WME));
    if (!name)
      return false;

    res= init_io_cache(&alt_buf, -1, LOG_BIN_IO_SIZE, io_cache_type_arg, 0, 0,
                       MYF(MY_WME | MY_NABP | MY_WAIT_IF_FULL)) != 0;
    return res;
  }

  /**
    Swaps current and alt_log. Can be called only from the reader thread.
    @return a new IO_CACHE pointer to read from.
   */
  IO_CACHE *flip()
  {
    IO_CACHE *tmp= current;
    reinit_io_cache(alt, WRITE_CACHE, 0, 0, 0);
    mysql_mutex_lock(get_log_lock());
    reinit_io_cache(current, READ_CACHE, 0, 0, 0);
    current= alt;
    mysql_mutex_unlock(get_log_lock());
    alt= tmp;

    return alt;
  }

  IO_CACHE *get_log_file() override
  {
    mysql_mutex_assert_owner(get_log_lock());
    return current;
  }

  void acquire()
  {
    IF_DBUG(auto prev= ,)
    ref_count.fetch_add(1);
    DBUG_ASSERT(prev != 0);
  }

  void release()
  {
    auto prev= ref_count.fetch_add(-1);

    if (prev == 1)
    {
      cleanup();
      delete this;
    }
  }

private:
  void cleanup()
  {
    close_cached_file(&log_file);
    close_cached_file(&alt_buf);
    Event_log::cleanup();
  }
};

/* Tell the io thread if we can delay the master info sync. */
#define SEMI_SYNC_SLAVE_DELAY_SYNC 1
/* Tell the io thread if the current event needs a ack. */
#define SEMI_SYNC_NEED_ACK  2

class MYSQL_QUERY_LOG: public MYSQL_LOG
{
public:
  MYSQL_QUERY_LOG() : last_time(0) {}
  void reopen_file();
  bool write(time_t event_time, const char *user_host, size_t user_host_len, my_thread_id thread_id,
             const char *command_type, size_t command_type_len,
             const char *sql_text, size_t sql_text_len);
  bool write(THD *thd, time_t current_time,
             const char *user_host, size_t user_host_len,
             ulonglong query_utime, ulonglong lock_utime, bool is_command,
             const char *sql_text, size_t sql_text_len);
  bool open_slow_log(const char *log_name)
  {
    char buf[FN_REFLEN];
    return open(
#ifdef HAVE_PSI_INTERFACE
                key_file_slow_log,
#endif
                generate_name(log_name, "-slow.log", 0, buf),
                LOG_NORMAL, 0, 0, WRITE_CACHE);
  }
  bool open_query_log(const char *log_name)
  {
    char buf[FN_REFLEN];
    return open(
#ifdef HAVE_PSI_INTERFACE
                key_file_query_log,
#endif
                generate_name(log_name, ".log", 0, buf),
                LOG_NORMAL, 0, 0, WRITE_CACHE);
  }

private:
  time_t last_time;
};

/*
  We assign each binlog file an internal ID, used to identify them for unlog().
  The IDs start from 0 and increment for each new binlog created.

  In unlog() we need to know the ID of the binlog file that the corresponding
  transaction was written into. We also need a special value for a corner
  case where there is no corresponding binlog id (since nothing was logged).
  And we need an error flag to mark that unlog() must return failure.

  We use the following macros to pack all of this information into the single
  ulong available with log_and_order() / unlog().

  Note that we cannot use the value 0 for cookie, as that is reserved as error
  return value from log_and_order().
  */
#define BINLOG_COOKIE_ERROR_RETURN 0
#define BINLOG_COOKIE_DUMMY_ID 1
#define BINLOG_COOKIE_BASE 2
#define BINLOG_COOKIE_DUMMY(error_flag) \
  ( (BINLOG_COOKIE_DUMMY_ID<<1) | ((error_flag)&1) )
#define BINLOG_COOKIE_MAKE(id, error_flag) \
  ( (((id)+BINLOG_COOKIE_BASE)<<1) | ((error_flag)&1) )
#define BINLOG_COOKIE_GET_ERROR_FLAG(c) ((c) & 1)
#define BINLOG_COOKIE_GET_ID(c) ( ((ulong)(c)>>1) - BINLOG_COOKIE_BASE )
#define BINLOG_COOKIE_IS_DUMMY(c) \
  ( ((ulong)(c)>>1) == BINLOG_COOKIE_DUMMY_ID )


class binlog_cache_mngr;
class binlog_cache_data;
struct rpl_gtid;
struct wait_for_commit;
class Binlog_commit_by_rotate;

class MYSQL_BIN_LOG: public TC_LOG, private Event_log
{
  friend Binlog_commit_by_rotate;

#ifdef HAVE_PSI_INTERFACE
  /** The instrumentation key to use for @ LOCK_index. */
  PSI_mutex_key m_key_LOCK_index;
  /** The instrumentation key to use for @ COND_relay_log_updated */
  PSI_cond_key m_key_relay_log_update;
  /** The instrumentation key to use for @ COND_bin_log_updated */
  PSI_cond_key m_key_bin_log_update;
  /** The instrumentation key to use for opening the log index file. */
  PSI_file_key m_key_file_log_index, m_key_file_log_index_cache;

  PSI_cond_key m_key_COND_queue_busy;
#else
  static constexpr PSI_mutex_key m_key_LOCK_index= 0;
  static constexpr PSI_cond_key m_key_relay_log_update= 0;
  static constexpr PSI_cond_key m_key_bin_log_update= 0;
  static constexpr PSI_file_key m_key_file_log= 0, m_key_file_log_cache= 0;
  static constexpr PSI_file_key m_key_file_log_index= 0;
  static constexpr PSI_file_key m_key_file_log_index_cache= 0;
  static constexpr PSI_cond_key m_key_COND_queue_busy= 0;
  static constexpr PSI_mutex_key m_key_LOCK_binlog_end_pos= 0;
#endif

  struct group_commit_entry
  {
    struct group_commit_entry *next;
    THD *thd;
    binlog_cache_mngr *cache_mngr;
    bool using_stmt_cache;
    bool using_trx_cache;
    /*
      Extra events (COMMIT/ROLLBACK/XID, and possibly INCIDENT) to be
      written during group commit. The incident_event is only valid if
      trx_data->has_incident() is true.
    */
    Log_event *end_event;
    Log_event *incident_event;
    /* Set during group commit to record any per-thread error. */
    int error;
    int commit_errno;
    IO_CACHE *error_cache;
    /* This is the `all' parameter for ha_commit_ordered(). */
    bool all;
    /*
      True if we need to increment xid_count in trx_group_commit_leader() and
      decrement in unlog() (this is needed if there is a participating engine
      that does not implement the commit_checkpoint_request() handlerton
      method).
    */
    bool need_unlog;
    /*
      Fields used to pass the necessary information to the last thread in a
      group commit, only used when opt_optimize_thread_scheduling is not set.
    */
    bool check_purge;
    /* Flag used to optimise around wait_for_prior_commit. */
    bool queued_by_other;
    ulong binlog_id;
    bool ro_1pc;  // passes the binlog_cache_mngr::ro_1pc value to Gtid ctor
  };

  /*
    When this is set, a RESET MASTER is in progress.

    Then we should not write any binlog checkpoints into the binlog (that
    could result in deadlock on LOCK_log, and we will delete all binlog files
    anyway). Instead we should signal COND_xid_list whenever a new binlog
    checkpoint arrives - when all have arrived, RESET MASTER will complete.
  */
  uint reset_master_pending;
  ulong mark_xid_done_waiting;

  /* LOCK_log and LOCK_index are inited by init_pthread_objects() */
  mysql_mutex_t LOCK_index;
  mysql_mutex_t LOCK_xid_list;
  mysql_cond_t  COND_xid_list;
  mysql_cond_t  COND_relay_log_updated, COND_bin_log_updated;
  ulonglong bytes_written;
  ulonglong binlog_space_total;
  IO_CACHE index_file;
  char index_file_name[FN_REFLEN];
  /*
    purge_file is a temp file used in purge_logs so that the index file
    can be updated before deleting files from disk, yielding better crash
    recovery. It is created on demand the first time purge_logs is called
    and then reused for subsequent calls. It is cleaned up in cleanup().
  */
  IO_CACHE purge_index_file;
  char purge_index_file_name[FN_REFLEN];
  /*
     The max size before rotation (usable only if log_type == LOG_BIN: binary
     logs and relay logs).
     For a binlog, max_size should be max_binlog_size.
     max_size is set in init(), and dynamically changed (when one does SET
     GLOBAL MAX_BINLOG_SIZE|MAX_RELAY_LOG_SIZE) from sys_vars.cc
  */
  ulong max_size;
  /*
    Number generated by last call of find_uniq_filename(). Corresponds
    closely with current_binlog_id
  */
  ulong last_used_log_number;
  // current file sequence number for load data infile binary logging
  uint file_id;
  uint open_count;				// For replication
  int readers_count;
  /* Queue of transactions queued up to participate in group commit. */
  group_commit_entry *group_commit_queue;
  /*
    Condition variable to mark that the group commit queue is busy.
    Used when each thread does it's own commit_ordered() (when
    binlog_optimize_thread_scheduling=1).
    Used with the LOCK_commit_ordered mutex.
  */
  my_bool group_commit_queue_busy;
  mysql_cond_t COND_queue_busy;
  /* Total number of committed transactions. */
  ulonglong num_commits;
  /* Number of group commits done. */
  ulonglong num_group_commits;
  /* The reason why the group commit was grouped */
  ulonglong group_commit_trigger_count, group_commit_trigger_timeout;
  ulonglong group_commit_trigger_lock_wait;

  /* Binlog GTID index. */
  Gtid_index_writer *gtid_index;

  /* pointer to the sync period variable, for binlog this will be
     sync_binlog_period, for relay log this will be
     sync_relay_log_period
  */
  uint *sync_period_ptr;
  uint sync_counter;
  bool state_file_deleted;
  bool binlog_state_recover_done;

  Gtid_index_writer *recover_gtid_index_start(const char *base_name,
                                              my_off_t offset);
  void recover_gtid_index_process(Gtid_index_writer *gi, my_off_t offset,
                                  const rpl_gtid *gtid);
  void recover_gtid_index_end(Gtid_index_writer *gi);
  void recover_gtid_index_abort(Gtid_index_writer *gi);

  inline uint get_sync_period()
  {
    return *sync_period_ptr;
  }

  int write_to_file(IO_CACHE *cache);
  /*
    This is used to start writing to a new log file. The difference from
    new_file() is locking. new_file_without_locking() does not acquire
    LOCK_log.
  */
  int new_file_impl(bool commit_by_rotate);
  void do_checkpoint_request(ulong binlog_id);
  int write_transaction_or_stmt(group_commit_entry *entry, uint64 commit_id,
                                bool commit_by_rotate);
  int queue_for_group_commit(group_commit_entry *entry);
  bool write_transaction_to_binlog_events(group_commit_entry *entry);
  bool write_transaction_with_group_commit(group_commit_entry *entry);
  void write_transaction_handle_error(group_commit_entry *entry);
  void trx_group_commit_leader(group_commit_entry *leader);
  void trx_group_commit_with_engines(group_commit_entry *leader,
                                     group_commit_entry *tail,
                                     bool commit_by_rotate);
  bool is_xidlist_idle_nolock();
  void update_gtid_index(uint32 offset, rpl_gtid gtid);

public:
  void purge(bool all);
  int new_file_without_locking(bool commit_by_rotate);
  /*
    A list of struct xid_count_per_binlog is used to keep track of how many
    XIDs are in prepared, but not committed, state in each binlog. And how
    many commit_checkpoint_request()'s are pending.

    When count drops to zero in a binlog after rotation, it means that there
    are no more XIDs in prepared state, so that binlog is no longer needed
    for XA crash recovery, and we can log a new binlog checkpoint event.

    The list is protected against simultaneous access from multiple
    threads by LOCK_xid_list.
  */
  struct xid_count_per_binlog : public ilink {
    char *binlog_name;
    uint binlog_name_len;
    ulong binlog_id;
    /* Total prepared XIDs and pending checkpoint requests in this binlog. */
    long xid_count;
    xid_count_per_binlog(char *log_file_name, uint log_file_name_len)
      :binlog_id(0), xid_count(0)
    {
      binlog_name_len= log_file_name_len;
      binlog_name= (char *) my_malloc(PSI_INSTRUMENT_ME, binlog_name_len, MYF(MY_ZEROFILL));
      if (binlog_name)
        memcpy(binlog_name, log_file_name, binlog_name_len);
    }
    ~xid_count_per_binlog()
    {
      my_free(binlog_name);
    }
  };
  I_List<xid_count_per_binlog> binlog_xid_count_list;
  mysql_mutex_t LOCK_binlog_background_thread;
  mysql_cond_t COND_binlog_background_thread;
  mysql_cond_t COND_binlog_background_thread_end;

  void stop_background_thread();

  using MYSQL_LOG::generate_name;
  using MYSQL_LOG::is_open;

  /* This is relay log */
  bool is_relay_log;
  ulong relay_signal_cnt;  // update of the counter is checked by heartbeat
  enum enum_binlog_checksum_alg checksum_alg_reset; // to contain a new value when binlog is rotated
  /*
    Holds the last seen in Relay-Log FD's checksum alg value.
    The initial value comes from the slave's local FD that heads
    the very first Relay-Log file. In the following the value may change
    with each received master's FD_m.
    Besides to be used in verification events that IO thread receives
    (except the 1st fake Rotate, see @c Master_info:: checksum_alg_before_fd), 
    the value specifies if/how to compute checksum for slave's local events
    and the first fake Rotate (R_f^1) coming from the master.
    R_f^1 needs logging checksum-compatibly with the RL's heading FD_s.

    Legends for the checksum related comments:

    FD     - Format-Description event,
    R      - Rotate event
    R_f    - the fake Rotate event
    E      - an arbirary event

    The underscore indexes for any event
    `_s'   indicates the event is generated by Slave
    `_m'   - by Master

    Two special underscore indexes of FD:
    FD_q   - Format Description event for queuing   (relay-logging)
    FD_e   - Format Description event for executing (relay-logging)

    Upper indexes:
    E^n    - n:th event is a sequence

    RL     - Relay Log
    (A)    - checksum algorithm descriptor value
    FD.(A) - the value of (A) in FD
  */
  enum enum_binlog_checksum_alg relay_log_checksum_alg;
  /*
    These describe the log's format. This is used only for relay logs.
    _for_exec is used by the SQL thread, _for_queue by the I/O thread. It's
    necessary to have 2 distinct objects, because the I/O thread may be reading
    events in a different format from what the SQL thread is reading (consider
    the case of a master which has been upgraded from 5.0 to 5.1 without doing
    RESET MASTER, or from 4.x to 5.0).
  */
  Format_description_log_event *description_event_for_exec,
    *description_event_for_queue;
  /*
    Binlog position of last commit (or non-transactional write) to the binlog.
    Access to this is protected by LOCK_commit_ordered.
  */
  char last_commit_pos_file[FN_REFLEN];
  my_off_t last_commit_pos_offset;
  ulong current_binlog_id;

  /*
    Tracks the number of times that the master has been reset
  */
  Atomic_counter<uint64> reset_master_count;

  MYSQL_BIN_LOG(uint *sync_period);
  /*
    note that there's no destructor ~MYSQL_BIN_LOG() !
    The reason is that we don't want it to be automatically called
    on exit() - but only during the correct shutdown process
  */

#ifdef HAVE_PSI_INTERFACE
  void set_psi_keys(PSI_mutex_key key_LOCK_index,
                    PSI_cond_key key_relay_log_update,
                    PSI_cond_key key_bin_log_update,
                    PSI_file_key key_file_log,
                    PSI_file_key key_file_log_cache,
                    PSI_file_key key_file_log_index,
                    PSI_file_key key_file_log_index_cache,
                    PSI_cond_key key_COND_queue_busy,
                    PSI_mutex_key key_LOCK_binlog_end_pos)
  {
    m_key_LOCK_index= key_LOCK_index;
    m_key_relay_log_update=  key_relay_log_update;
    m_key_bin_log_update=    key_bin_log_update;
    m_key_file_log= key_file_log;
    m_key_file_log_cache= key_file_log_cache;
    m_key_file_log_index= key_file_log_index;
    m_key_file_log_index_cache= key_file_log_index_cache;
    m_key_COND_queue_busy= key_COND_queue_busy;
    m_key_LOCK_binlog_end_pos= key_LOCK_binlog_end_pos;
  }
#endif

  Event_log *as_event_log()
  {
    return this;
  }

  int open(const char *opt_name) override;
  void close() override;
  int generate_new_name(char *new_name, const char *log_name,
                        ulong next_log_number) override;
  int log_and_order(THD *thd, my_xid xid, bool all,
                    bool need_prepare_ordered, bool need_commit_ordered) override;
  int unlog(ulong cookie, my_xid xid) override;
  int unlog_xa_prepare(THD *thd, bool all) override;
  void commit_checkpoint_notify(void *cookie) override;
  int recover(LOG_INFO *linfo, const char *last_log_name, IO_CACHE *first_log,
              Format_description_log_event *fdle, bool do_xa);
  int do_binlog_recovery(const char *opt_name, bool do_xa_recovery);
#if !defined(MYSQL_CLIENT)
  static int remove_pending_rows_event(THD *thd, binlog_cache_data *cache_data);

#endif /* !defined(MYSQL_CLIENT) */
  void reset_bytes_written()
  {
    bytes_written = 0;
  }
  void harvest_bytes_written(Atomic_counter<uint64> *counter)
  {
#ifdef DBUG_TRACE
    char buf1[22],buf2[22];
#endif
    DBUG_ENTER("harvest_bytes_written");
    (*counter)+=bytes_written;
    DBUG_PRINT("info",("counter: %s  bytes_written: %s", llstr(*counter,buf1),
		       llstr(bytes_written,buf2)));
    bytes_written=0;
    DBUG_VOID_RETURN;
  }
  void set_max_size(ulong max_size_arg);

  /* Handle signaling that relay has been updated */
  void signal_relay_log_update()
  {
    mysql_mutex_assert_owner(&LOCK_log);
    DBUG_ASSERT(is_relay_log);
    DBUG_ENTER("MYSQL_BIN_LOG::signal_relay_log_update");
    relay_signal_cnt++;
    mysql_cond_broadcast(&COND_relay_log_updated);
    DBUG_VOID_RETURN;
  }
  void signal_bin_log_update()
  {
    mysql_mutex_assert_owner(&LOCK_binlog_end_pos);
    DBUG_ASSERT(!is_relay_log);
    DBUG_ENTER("MYSQL_BIN_LOG::signal_bin_log_update");
    mysql_cond_broadcast(&COND_bin_log_updated);
    DBUG_VOID_RETURN;
  }
  void update_binlog_end_pos()
  {
    if (is_relay_log)
      signal_relay_log_update();
    else
    {
      lock_binlog_end_pos();
      binlog_end_pos= my_b_safe_tell(&log_file);
      signal_bin_log_update();
      unlock_binlog_end_pos();
    }
  }
  void update_binlog_end_pos(my_off_t pos)
  {
    mysql_mutex_assert_owner(&LOCK_log);
    mysql_mutex_assert_not_owner(&LOCK_binlog_end_pos);
    lock_binlog_end_pos();
    /*
      Note: it would make more sense to assert(pos > binlog_end_pos)
      but there are two places triggered by mtr that has pos == binlog_end_pos
      i didn't investigate but accepted as it should do no harm
    */
    DBUG_ASSERT(pos >= binlog_end_pos);
    binlog_end_pos= pos;
    signal_bin_log_update();
    unlock_binlog_end_pos();
  }

  void wait_for_sufficient_commits();
  void binlog_trigger_immediate_group_commit();
  void wait_for_update_relay_log(THD* thd);
  void init(ulong max_size);
  void init_pthread_objects();
  void cleanup();
  bool open(const char *log_name,
            const char *new_name,
            ulong next_log_number,
	    enum cache_type io_cache_type_arg,
	    ulong max_size,
            bool null_created,
            bool need_mutex,
            bool commit_by_rotate = false);
  bool open_index_file(const char *index_file_name_arg,
                       const char *log_name, bool need_mutex);
  /* Use this to start writing a new log file */
  int new_file();

  bool write(Log_event* event_info,
             my_bool *with_annotate= 0); // binary log write
  bool write_transaction_to_binlog(THD *thd, binlog_cache_mngr *cache_mngr,
                                   Log_event *end_ev, bool all,
                                   bool using_stmt_cache, bool using_trx_cache,
                                   bool is_ro_1pc);

  bool write_incident_already_locked(THD *thd);
  bool write_incident(THD *thd);
  void write_binlog_checkpoint_event_already_locked(const char *name, uint len);
  bool write_table_map(THD *thd, TABLE *table, bool with_annotate);

  void start_union_events(THD *thd, query_id_t query_id_param);
  void stop_union_events(THD *thd);
  bool is_query_in_union(THD *thd, query_id_t query_id_param);

  using Event_log::write_event;

  bool write_event(Log_event *ev, enum enum_binlog_checksum_alg checksum_alg)
  {
    return write_event(ev, checksum_alg, 0, &log_file);
  }
  bool write_event(Log_event *ev);

  bool write_event_buffer(uchar* buf,uint len);
  bool append(Log_event* ev, enum enum_binlog_checksum_alg checksum_alg);
  bool append_no_lock(Log_event* ev, enum enum_binlog_checksum_alg checksum_alg);

  void mark_xids_active(ulong cookie, uint xid_count);
  void mark_xid_done(ulong cookie, bool write_checkpoint);
  void make_log_name(char* buf, const char* log_ident);
  bool is_active(const char* log_file_name);
  bool can_purge_log(const char *log_file_name, bool interactive);
  int update_log_index(LOG_INFO* linfo, bool need_update_threads);
  int rotate(bool force_rotate, bool *check_purge,
             bool commit_by_rotate= false);
  void checkpoint_and_purge(ulong binlog_id);
  int rotate_and_purge(bool force_rotate, DYNAMIC_ARRAY* drop_gtid_domain= NULL);
  /**
     Flush binlog cache and synchronize to disk.

     This function flushes events in binlog cache to binary log file,
     it will do synchronizing according to the setting of system
     variable 'sync_binlog'. If file is synchronized, @c synced will
     be set to 1, otherwise 0.

     @param[out] synced if not NULL, set to 1 if file is synchronized, otherwise 0

     @retval 0 Success
     @retval other Failure
  */
  bool flush_and_sync(bool *synced);
  int purge_logs(THD *thd, const char *to_log, bool included,
                 bool need_mutex, bool need_update_threads, bool interactive,
                 ulonglong *decrease_log_space);
  int purge_logs_before_date(THD *thd, time_t purge_time, bool interactive);
  int purge_first_log(Relay_log_info* rli, bool included);
  int count_binlog_space();
  void count_binlog_space_with_mutex()
  {
    mysql_mutex_lock(&LOCK_index);
    count_binlog_space();
    mysql_mutex_unlock(&LOCK_index);
  }
  ulonglong get_binlog_space_total();
  int real_purge_logs_by_size(ulonglong binlog_pos);
  inline int purge_logs_by_size(ulonglong binlog_pos)
  {
    if (is_relay_log || ! binlog_space_limit ||
        binlog_space_total + binlog_pos <= binlog_space_limit)
      return 0;
    return real_purge_logs_by_size(binlog_pos);
  }
  int set_purge_index_file_name(const char *base_file_name);
  int open_purge_index_file(bool destroy);
  bool truncate_and_remove_binlogs(const char *truncate_file,
                                   my_off_t truncate_pos,
                                   rpl_gtid *gtid);
  bool is_inited_purge_index_file();
  int close_purge_index_file();
  int clean_purge_index_file();
  int sync_purge_index_file();
  int register_purge_index_entry(const char* entry);
  int register_create_index_entry(const char* entry);
  int purge_index_entry(THD *thd, ulonglong *decrease_log_space,
                        bool need_mutex);
  bool reset_logs(THD* thd, bool create_new_log,
                  rpl_gtid *init_state, uint32 init_state_len,
                  ulong next_log_number);
  void wait_for_last_checkpoint_event();
  void close(uint exiting);
  void clear_inuse_flag_when_closing(File file);

  // iterating through the log index file
  int find_log_pos(LOG_INFO* linfo, const char* log_name,
		   bool need_mutex);
  int find_next_log(LOG_INFO* linfo, bool need_mutex);
  int get_current_log(LOG_INFO* linfo);
  int raw_get_current_log(LOG_INFO* linfo);
  uint next_file_id();
  inline char* get_index_fname() { return index_file_name;}
  inline char* get_log_fname() { return log_file_name; }
  using MYSQL_LOG::get_log_lock;
  inline mysql_cond_t* get_bin_log_cond() { return &COND_bin_log_updated; }
  inline IO_CACHE* get_log_file() override { return &log_file; }
  inline uint64 get_reset_master_count() { return reset_master_count; }

  inline void lock_index() { mysql_mutex_lock(&LOCK_index);}
  inline void unlock_index() { mysql_mutex_unlock(&LOCK_index);}
  inline IO_CACHE *get_index_file() { return &index_file;}
  inline uint32 get_open_count() { return open_count; }
  void set_status_variables(THD *thd);
  bool is_xidlist_idle();
  bool write_gtid_event(THD *thd, bool standalone, bool is_transactional,
                        uint64 commit_id,
                        bool commit_by_rotate,
                        bool has_xid= false, bool ro_1pc= false);
  int read_state_from_file();
  int write_state_to_file();
  int get_most_recent_gtid_list(rpl_gtid **list, uint32 *size);
  bool append_state_pos(String *str);
  bool append_state(String *str);
  bool is_empty_state();
  bool find_in_binlog_state(uint32 domain_id, uint32 server_id,
                            rpl_gtid *out_gtid);
  bool lookup_domain_in_binlog_state(uint32 domain_id, rpl_gtid *out_gtid);
  int bump_seq_no_counter_if_needed(uint32 domain_id, uint64 seq_no);
  bool check_strict_gtid_sequence(uint32 domain_id, uint32 server_id,
                                  uint64 seq_no, bool no_error= false);

  /**
   * used when opening new file, and binlog_end_pos moves backwards
   */
  void reset_binlog_end_pos(const char file_name[FN_REFLEN], my_off_t pos)
  {
    mysql_mutex_assert_owner(&LOCK_log);
    mysql_mutex_assert_not_owner(&LOCK_binlog_end_pos);
    lock_binlog_end_pos();
    binlog_end_pos= pos;
    safe_strcpy(binlog_end_pos_file, sizeof(binlog_end_pos_file), file_name);
    signal_bin_log_update();
    unlock_binlog_end_pos();
  }

  /*
    It is called by the threads(e.g. dump thread) which want to read
    log without LOCK_log protection.
  */
  my_off_t get_binlog_end_pos(char file_name_buf[FN_REFLEN]) const
  {
    mysql_mutex_assert_not_owner(&LOCK_log);
    mysql_mutex_assert_owner(&LOCK_binlog_end_pos);
    safe_strcpy(file_name_buf, FN_REFLEN, binlog_end_pos_file);
    return binlog_end_pos;
  }
  void lock_binlog_end_pos() { mysql_mutex_lock(&LOCK_binlog_end_pos); }
  void unlock_binlog_end_pos() { mysql_mutex_unlock(&LOCK_binlog_end_pos); }
  mysql_mutex_t* get_binlog_end_pos_lock() { return &LOCK_binlog_end_pos; }

  /*
    Ensures the log's state is either LOG_OPEN or LOG_CLOSED. If something
    failed along the desired path and left the log in invalid state, i.e.
    LOG_TO_BE_OPENED, forces the state to be LOG_CLOSED.
  */
  void try_fix_log_state()
  {
    mysql_mutex_lock(get_log_lock());
    /* Only change the log state if it is LOG_TO_BE_OPENED */
    if (log_state == LOG_TO_BE_OPENED)
      log_state= LOG_CLOSED;
    mysql_mutex_unlock(get_log_lock());
  }

  int wait_for_update_binlog_end_pos(THD* thd, struct timespec * timeout);

  /*
    Binlog position of end of the binlog.
    Access to this is protected by LOCK_binlog_end_pos

    The difference between this and last_commit_pos_{file,offset} is that
    the commit position is updated later. If semi-sync wait point is set
    to WAIT_AFTER_SYNC, the commit pos is update after semi-sync-ack has
    been received and the end point is updated after the write as it's needed
    for the dump threads to be able to semi-sync the event.
  */
  my_off_t binlog_end_pos;
  char binlog_end_pos_file[FN_REFLEN];
};

class Log_event_handler
{
public:
  Log_event_handler() = default;
  virtual bool init()= 0;
  virtual void cleanup()= 0;

  virtual bool log_slow(THD *thd, my_hrtime_t current_time,
                        const char *user_host, size_t user_host_len, ulonglong query_utime,
                        ulonglong lock_utime, bool is_command,
                        const char *sql_text, size_t sql_text_len)= 0;
  ATTRIBUTE_FORMAT(printf, 3, 0)
  virtual bool log_error(enum loglevel level, const char *format,
                         va_list args)= 0;
  virtual bool log_general(THD *thd, my_hrtime_t event_time, const char *user_host, size_t user_host_len, my_thread_id thread_id,
                           const char *command_type, size_t command_type_len,
                           const char *sql_text, size_t sql_text_len,
                           CHARSET_INFO *client_cs)= 0;
  virtual ~Log_event_handler() = default;
};


int check_if_log_table(const TABLE_LIST *table, bool check_if_opened,
                       const char *errmsg);

class Log_to_csv_event_handler: public Log_event_handler
{
  friend class LOGGER;

public:
  Log_to_csv_event_handler();
  ~Log_to_csv_event_handler();
  bool init() override;
  void cleanup() override;

  bool log_slow(THD *thd, my_hrtime_t current_time,
                        const char *user_host, size_t user_host_len, ulonglong query_utime,
                        ulonglong lock_utime, bool is_command,
                        const char *sql_text, size_t sql_text_len) override;
  ATTRIBUTE_FORMAT(printf, 3, 0)
  bool log_error(enum loglevel level, const char *format,
                         va_list args) override;
  bool log_general(THD *thd, my_hrtime_t event_time, const char *user_host, size_t user_host_len, my_thread_id thread_id,
                           const char *command_type, size_t command_type_len,
                           const char *sql_text, size_t sql_text_len,
                           CHARSET_INFO *client_cs) override;

  int activate_log(THD *thd, uint log_type);
};


/* type of the log table */
#define QUERY_LOG_SLOW 1
#define QUERY_LOG_GENERAL 2

class Log_to_file_event_handler: public Log_event_handler
{
  MYSQL_QUERY_LOG mysql_log;
  MYSQL_QUERY_LOG mysql_slow_log;
  bool is_initialized;
public:
  Log_to_file_event_handler(): is_initialized(FALSE)
  {}
  bool init() override;
  void cleanup() override;

  bool log_slow(THD *thd, my_hrtime_t current_time,
                        const char *user_host, size_t user_host_len, ulonglong query_utime,
                        ulonglong lock_utime, bool is_command,
                        const char *sql_text, size_t sql_text_len) override;
  ATTRIBUTE_FORMAT(printf, 3, 0)
  bool log_error(enum loglevel level, const char *format,
                         va_list args) override;
  bool log_general(THD *thd, my_hrtime_t event_time, const char *user_host, size_t user_host_len, my_thread_id thread_id,
                           const char *command_type, size_t command_type_len,
                           const char *sql_text, size_t sql_text_len,
                           CHARSET_INFO *client_cs) override;
  void flush();
  void init_pthread_objects();
  MYSQL_QUERY_LOG *get_mysql_slow_log() { return &mysql_slow_log; }
  MYSQL_QUERY_LOG *get_mysql_log() { return &mysql_log; }
};


/* Class which manages slow, general and error log event handlers */
class LOGGER
{
  mysql_rwlock_t LOCK_logger;
  /* flag to check whether logger mutex is initialized */
  uint inited;

  /* available log handlers */
  Log_to_csv_event_handler *table_log_handler;
  Log_to_file_event_handler *file_log_handler;

  /* NULL-terminated arrays of log handlers */
  Log_event_handler *error_log_handler_list[MAX_LOG_HANDLERS_NUM + 1];
  Log_event_handler *slow_log_handler_list[MAX_LOG_HANDLERS_NUM + 1];
  Log_event_handler *general_log_handler_list[MAX_LOG_HANDLERS_NUM + 1];

public:

  bool is_log_tables_initialized;

  LOGGER() : inited(0), table_log_handler(NULL),
             file_log_handler(NULL), is_log_tables_initialized(FALSE)
  {}
  void lock_shared() { mysql_rwlock_rdlock(&LOCK_logger); }
  void lock_exclusive() { mysql_rwlock_wrlock(&LOCK_logger); }
  void unlock() { mysql_rwlock_unlock(&LOCK_logger); }
  bool is_log_table_enabled(uint log_table_type);
  bool log_command(THD *thd, enum enum_server_command command);

  /*
    We want to initialize all log mutexes as soon as possible,
    but we cannot do it in constructor, as safe_mutex relies on
    initialization, performed by MY_INIT(). This why this is done in
    this function.
  */
  void init_base();
  void init_log_tables();
  bool flush_slow_log();
  bool flush_general_log();
  /* Perform basic logger cleanup. this will leave e.g. error log open. */
  void cleanup_base();
  /* Free memory. Nothing could be logged after this function is called */
  void cleanup_end();
  ATTRIBUTE_FORMAT(printf, 3, 0) // 1st arg is the implicit `this`
  bool error_log_print(enum loglevel level, const char *format,
                      va_list args);
  bool slow_log_print(THD *thd, const char *query, size_t query_length,
                      ulonglong current_utime);
  ATTRIBUTE_FORMAT(printf, 4, 0)
  bool general_log_print(THD *thd,enum enum_server_command command,
                         const char *format, va_list args);
  bool general_log_write(THD *thd, enum enum_server_command command,
                         const char *query, size_t query_length);

  /* we use this function to setup all enabled log event handlers */
  int set_handlers(ulonglong slow_log_printer,
                   ulonglong general_log_printer);
  void init_error_log(ulonglong error_log_printer);
  void init_slow_log(ulonglong slow_log_printer);
  void init_general_log(ulonglong general_log_printer);
  void deactivate_log_handler(THD* thd, uint log_type);
  bool activate_log_handler(THD* thd, uint log_type);
  MYSQL_QUERY_LOG *get_slow_log_file_handler() const
  { 
    if (file_log_handler)
      return file_log_handler->get_mysql_slow_log();
    return NULL;
  }
  MYSQL_QUERY_LOG *get_log_file_handler() const
  { 
    if (file_log_handler)
      return file_log_handler->get_mysql_log();
    return NULL;
  }
};

enum enum_binlog_format {
  BINLOG_FORMAT_MIXED= 0, ///< statement if safe, otherwise row - autodetected
  BINLOG_FORMAT_STMT=  1, ///< statement-based
  BINLOG_FORMAT_ROW=   2, ///< row-based
  BINLOG_FORMAT_UNSPEC=3  ///< thd_binlog_format() returns it when binlog is closed
};

int query_error_code(THD *thd, bool not_killed);
uint purge_log_get_error_code(int res);

int vprint_msg_to_log(enum loglevel level, const char *format, va_list args)
  ATTRIBUTE_FORMAT(printf, 2, 0);
void sql_print_error(const char *format, ...) ATTRIBUTE_FORMAT(printf, 1, 2);
void sql_print_warning(const char *format, ...) ATTRIBUTE_FORMAT(printf, 1, 2);
void sql_print_information(const char *format, ...)
  ATTRIBUTE_FORMAT(printf, 1, 2);
void sql_print_information_v(const char *format, va_list ap)
  ATTRIBUTE_FORMAT(printf, 1, 0);
typedef void (*sql_print_message_func)(const char *format, ...)
  ATTRIBUTE_FORMAT_FPTR(printf, 1, 2);
extern sql_print_message_func sql_print_message_handlers[];

int error_log_print(enum loglevel level, const char *format,
                    va_list args) ATTRIBUTE_FORMAT(printf, 2, 0);

bool slow_log_print(THD *thd, const char *query, uint query_length,
                    ulonglong current_utime);

bool general_log_print(THD *thd, enum enum_server_command command,
                       const char *format,...) ATTRIBUTE_FORMAT(printf, 3, 4);

bool general_log_write(THD *thd, enum enum_server_command command,
                       const char *query, size_t query_length);

void binlog_report_wait_for(THD *thd, THD *other_thd);
void sql_perror(const char *message);
bool flush_error_log();

File open_binlog(IO_CACHE *log, const char *log_file_name,
                 const char **errmsg);

void make_default_log_name(char **out, const char* log_ext, bool once);
void binlog_reset_cache(THD *thd);
void binlog_clear_incident(THD *thd);
bool write_annotated_row(THD *thd);
int binlog_flush_pending_rows_event(THD *thd, bool stmt_end,
                                    bool is_transactional,
                                    Event_log *bin_log,
                                    binlog_cache_data *cache_data);
Rows_log_event* binlog_get_pending_rows_event(binlog_cache_mngr *cache_mngr,
                                              bool use_trans_cache);
int online_alter_log_row(TABLE* table, const uchar *before_record,
                         const uchar *after_record, Log_func *log_func);
binlog_cache_data* binlog_get_cache_data(binlog_cache_mngr *cache_mngr,
                                         bool use_trans_cache);

extern MYSQL_PLUGIN_IMPORT MYSQL_BIN_LOG mysql_bin_log;
extern transaction_participant binlog_tp;
extern LOGGER logger;

extern const char *log_bin_index;
extern const char *log_bin_basename;

/**
  Turns a relative log binary log path into a full path, based on the
  opt_bin_logname or opt_relay_logname.

  @param from         The log name we want to make into an absolute path.
  @param to           The buffer where to put the results of the 
                      normalization.
  @param is_relay_log Switch that makes is used inside to choose which
                      option (opt_bin_logname or opt_relay_logname) to
                      use when calculating the base path.

  @returns true if a problem occurs, false otherwise.
 */

inline bool normalize_binlog_name(char *to, const char *from, bool is_relay_log)
{
  DBUG_ENTER("normalize_binlog_name");
  bool error= false;
  char buff[FN_REFLEN];
  char *ptr= (char*) from;
  char *opt_name= is_relay_log ? opt_relay_logname : opt_bin_logname;

  DBUG_ASSERT(from);

  /* opt_name is not null and not empty and from is a relative path */
  if (opt_name && opt_name[0] && from && !test_if_hard_path(from))
  {
    // take the path from opt_name
    // take the filename from from 
    char log_dirpart[FN_REFLEN], log_dirname[FN_REFLEN];
    size_t log_dirpart_len, log_dirname_len;
    dirname_part(log_dirpart, opt_name, &log_dirpart_len);
    dirname_part(log_dirname, from, &log_dirname_len);

    /* log may be empty => relay-log or log-bin did not 
        hold paths, just filename pattern */
    if (log_dirpart_len > 0)
    {
      /* create the new path name */
      if(fn_format(buff, from+log_dirname_len, log_dirpart, "",
                   MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH)) == NULL)
      {
        error= true;
        goto end;
      }

      ptr= buff;
    }
  }

  DBUG_ASSERT(ptr);

  if (ptr)
    strmake(to, ptr, strlen(ptr));

end:
  DBUG_RETURN(error);
}

static inline TC_LOG *get_tc_log_implementation()
{
  if (total_ha_2pc <= 2) // online_alter_tp and MHNSW_Trx::tp
    return &tc_log_dummy;
  if (opt_bin_log)
    return &mysql_bin_log;
  return &tc_log_mmap;
}

#ifdef WITH_WSREP
IO_CACHE* wsrep_get_cache(THD *, bool);
bool wsrep_is_binlog_cache_empty(THD *);
void wsrep_thd_binlog_trx_reset(THD * thd);
void wsrep_thd_binlog_stmt_rollback(THD * thd);
#endif /* WITH_WSREP */

class Gtid_list_log_event;
const char *
get_gtid_list_event(IO_CACHE *cache, Gtid_list_log_event **out_gtid_list);

int binlog_commit(THD *thd, bool all, bool is_ro_1pc= false);
int binlog_commit_by_xid(XID *xid);
int binlog_rollback_by_xid(XID *xid);
bool write_bin_log_start_alter(THD *thd, bool& partial_alter,
                               uint64 start_alter_id, bool log_if_exists);
#endif /* LOG_H */

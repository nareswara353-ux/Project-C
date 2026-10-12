#ifndef LSM_WAL_H
#define LSM_WAL_H

#include "lsm_types.h"

typedef struct Wal Wal;

typedef void (*WalRecoverCallback)(uint8_t op, LsmSlice key, LsmSlice value, SequenceNumber seq,
                                   void *user);

LsmStatus wal_open(const char *path, bool sync, Wal **out);
LsmStatus wal_append_put(Wal *wal, LsmSlice key, LsmSlice value, SequenceNumber seq);
LsmStatus wal_append_delete(Wal *wal, LsmSlice key, SequenceNumber seq);
LsmStatus wal_sync(Wal *wal);
void wal_close(Wal *wal);
LsmStatus wal_recover(Wal *wal, WalRecoverCallback cb, void *user);
LsmStatus wal_replay_into_memtable(const char *wal_path, void *memtable);

#endif

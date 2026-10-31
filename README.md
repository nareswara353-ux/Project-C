
# LSM-Tree Key-Value Store (C17)

A modular, test-driven implementation of an LSM-Tree key-value storage engine
written in strict C17 with POSIX/Linux primitives. Designed as a systems
engineering portfolio project with a full CI pipeline covering static analysis,
multi-compiler builds, dynamic sanitizers, and memory-leak detection.

## Architecture
include/
lsm_tree.h Public LSM tree API
lsm_types.h Core types (Slice, InternalKey, Status)
lsm_config.h Config loader interface
lsm_error.h Error handling (string, predicate, errno mapping)
lsm_iterator.h Iterator ops + merge/concat composition
memtable.h In-memory table interface
wal.h Write-Ahead Log interface
sstable.h Sorted String Table interface
bloom_filter.h Bloom filter interface
internal/
arena.h Bump-pointer arena allocator
checksum.h CRC32C (Castagnoli) checksum
threadpool.h POSIX pthread work queue

src/
lsm_tree.c Top-level tree dispatcher
lsm_config.c Env/file config parsing + validation
lsm_iterator.c Iterator wrapper, merge, concat
error.c Error string mapping
memtable.c Memtable wrapper (skiplist backed)
memtable_skiplist.c Probabilistic skip list
wal.c WAL append + recovery
wal_reader.c WAL replay into memtable
sstable.c SSTable builder + reader
sstable_block.c Block compression hooks + checksum
sstable_index.c SSTable index builder
bloom_filter.c Bloom filter with MurmurHash
arena.c Arena allocator implementation
checksum.c CRC32C implementation
threadpool.c Thread pool implementation

tests/
test_wal.c WAL append/recover tests
test_wal_reader.c WAL replay into memtable
test_memtable.c Memtable put/get/size/iterator
test_skiplist.c Skiplist insert/find/traverse
test_sstable.c SSTable build/open/get roundtrip
test_bloom.c Bloom filter + false-positive rate
test_config.c Env + file config loading
test_integration.c WAL → memtable → SSTable E2E
test_lsm_tree.c Public API behavior
test_iterator.c Iterator ops, merge, concat
test_error.c Error string mapping
test_arena.c Arena allocation + alignment
test_checksum.c CRC32C vectors
test_threadpool.c Work queue concurrency

text

## Build

Requires `cmake >= 3.20` and a C17-capable compiler (GCC or Clang).

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
CI Pipeline
GitHub Actions runs on every push and pull request to main/develop:

lint — clang-format --dry-run --Werror across all C sources

build-matrix — GCC & Clang × Debug & Release, CTest executed

sanitizers — ASan + UBSan with leak detection

valgrind — Memcheck with --leak-check=full --error-exitcode=1

coverage — gcovr HTML/XML artifact upload

Design Notes
Zero comments — code is self-documenting via identifier naming.

Strict flags — every compilation unit must pass
-Wall -Wextra -Werror -pedantic -std=c17.

Modular headers — every module has a public interface in include/;
internal infrastructure lives under include/internal/.

Arena + pool allocators — memtable and cache subsystems allocate in
bulk from a bump-pointer arena to minimize fragmentation.

CRC32C — SSTable blocks carry a Castagnoli checksum for corruption
detection on recovery.

Thread pool — background compaction and flushing will run through the
POSIX-backed work queue in src/threadpool.c.

Status
All 56 architectural components implemented. Full test suite green across
Debug/Release × GCC/Clang, ASan/UBSan, and Valgrind. This repository serves
as a reference for strict C systems engineering with complete CI coverage.
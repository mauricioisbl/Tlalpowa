#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "tlalpowa_hotdata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stddef.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <direct.h>
#define TLALPOWA_PATH_SEP '\\'
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sched.h>
#define TLALPOWA_PATH_SEP '/'
#endif

#ifndef TLALPOWA_HOT_PATH_MAX
#define TLALPOWA_HOT_PATH_MAX 4096u
#endif

#define TLAL_IX_TRAILER_BYTES 40u
#define TLAL_IX_DIR_HEADER_BYTES 24u
#define TLAL_IX_DIR_ENTRY_BYTES 104u
#define TLAL_TOUCH_BLOCK_BYTES (256u * 1024u)
#define TLAL_MAX_PAYLOAD_BYTES (512ull * 1024ull * 1024ull)
#define TLAL_HOT_DEFAULT_BUDGET (96ull * 1024ull * 1024ull)
#define TLAL_HOT_MAX_BUDGET (384ull * 1024ull * 1024ull)
#define TLAL_HOT_DEFAULT_CACHE_BYTES (32ull * 1024ull * 1024ull)
#define TLAL_HOT_MAX_CACHE_BYTES (256ull * 1024ull * 1024ull)
#define TLAL_HOT_DEFAULT_RETAINED_MAP_BYTES (128ull * 1024ull * 1024ull)
#define TLAL_HOT_MAX_RETAINED_MAP_BYTES (256ull * 1024ull * 1024ull)
#define TLAL_HOT_CACHE_LINES_MAX 8192u
#define TLAL_HOT_CACHE_LINES_DEFAULT 4096u
#define TLAL_HOT_STARTUP_GATE_RECORDS_DEFAULT 10u
#define TLAL_HOT_STARTUP_GATE_RECORDS_MAX 16u
#define TLAL_HOT_STARTUP_GATE_BYTES_DEFAULT (64u * 1024u)
#define TLAL_HOT_STARTUP_GATE_BYTES_MAX (2u * 1024u * 1024u)
#define TLAL_HOT_STARTUP_CATEGORY_LIMIT_DEFAULT 512u
#define TLAL_HOT_STARTUP_CATEGORY_LIMIT_MAX 512u
#define TLAL_PAGE_PROBE_BYTES 4096u
#define TLAL_CORE_SLOT_COUNT 5u
#define TLAL_CORE_SLOT_ANY 0u
#define TLAL_CORE_SLOT_EPI 1u
#define TLAL_CORE_SLOT_MET 2u
#define TLAL_CORE_SLOT_CON 3u
#define TLAL_CORE_SLOT_OTH 4u

static const unsigned char TLAL_IX_MAGIC[8] = {'I','X','I','P','T','L','A','H'};
static const unsigned char TLAL_IX_DIR_MAGIC[8] = {'I','X','D','I','R','V','1','A'};
static const unsigned char TLAL_IX_DIR_END_MAGIC[8] = {'I','X','D','I','R','E','N','D'};

#ifdef _WIN32
typedef struct TlalMappedFile {
    unsigned char* data;
    uint64_t size;
    HANDLE file;
    HANDLE mapping;
    int mapped;
} TlalMappedFile;
#else
typedef struct TlalMappedFile {
    unsigned char* data;
    uint64_t size;
    int fd;
    int mapped;
} TlalMappedFile;
#endif

typedef struct TlalHotFile {
    char path[TLALPOWA_HOT_PATH_MAX];
    uint64_t size;
    uint32_t kind;
    TlalMappedFile map;
} TlalHotFile;

typedef struct TlalHotRecord {
    uint64_t temporal_key;
    uint64_t payload_offset;
    uint64_t stored_size;
    uint64_t raw_size;
    uint64_t layer_hash;
    uint64_t narrow_bucket;
    uint64_t hour_bucket;
    uint64_t week_bucket;
    uint64_t wide_bucket;
    uint32_t type;
    uint32_t schema;
    uint32_t codec;
    uint32_t core_group;
    uint32_t quality_flags;
    uint32_t file_index;
} TlalHotRecord;

typedef struct TlalHotCandidate {
    uint64_t temporal_key;
    uint64_t payload_offset;
    uint64_t stored_size;
    uint32_t type;
    uint32_t schema;
    uint32_t core_group;
    uint32_t file_index;
    uint32_t record_index;
} TlalHotCandidate;

typedef struct TlalHotCacheLine {
    unsigned char* data;
    uint64_t bytes;
    uint64_t tick;
    uint64_t temporal_key;
    uint64_t payload_offset;
    uint64_t layer_hash;
    uint32_t file_index;
    uint32_t record_index;
    uint32_t core_group;
} TlalHotCacheLine;

typedef struct TlalHotRuntimeIndex {
    TlalHotFile* files;
    size_t file_count;
    size_t file_cap;
    TlalHotRecord* records;
    size_t record_count;
    size_t record_cap;
    uint32_t* temporal_order;
    size_t temporal_order_count;
    uint32_t* core_order[TLAL_CORE_SLOT_COUNT];
    size_t core_order_count[TLAL_CORE_SLOT_COUNT];
    TlalHotCacheLine cache[TLAL_HOT_CACHE_LINES_MAX];
    uint64_t cache_bytes;
    uint64_t cache_limit_bytes;
    uint64_t cache_tick;
    uint64_t mapped_file_bytes;
    uint64_t mapped_file_limit_bytes;
    uint32_t cache_line_count;
} TlalHotRuntimeIndex;

typedef struct TlalHotState {
    TlalpowaHotDataConfig cfg;
    TlalpowaHotDataStats* stats;
    uint64_t budget_left;
    uint32_t ix_seen_limit;
    unsigned char* touch_buffer;
    TlalHotRuntimeIndex index;
    TlalHotCandidate latest_any;
    TlalHotCandidate latest_epi;
    TlalHotCandidate latest_met;
    TlalHotCandidate latest_con;
    TlalHotCandidate latest_oth;
} TlalHotState;

typedef struct TlalTopHit {
    uint64_t score;
    uint32_t record_index;
} TlalTopHit;

typedef struct TlalStartupCategory {
    uint32_t core_group;
    uint32_t type;
    uint32_t schema;
    uint64_t layer_hash;
    uint64_t temporal_key;
    uint32_t record_count;
    uint32_t record_indices[TLAL_HOT_STARTUP_GATE_RECORDS_MAX];
} TlalStartupCategory;

static TlalHotRuntimeIndex g_tlal_hot_index;
static volatile unsigned char g_tlal_hot_sink;

/*
CONTRATO FIJO DE HOT DATA TLALPOWA:
1) La bienvenida NO busca la fecha civil actual. Casi nunca los datos regionales
   estan al dia; por tanto el primer plano toma los ULTIMOS DIEZ registros
   IXIPTLAH realmente disponibles por cada categoria fisica encontrada.
2) Categoria fisica significa nucleo/tipo/esquema/capa; asi contaminantes,
   meteorologia, epidemiologia y otros grupos no se colapsan en un unico
   resumen ni en una fecha inventada.
3) La bienvenida puede durar un poco mas: solo se desvanece cuando esa hotdata
   inicial de ultimos diez registros por categoria queda realmente en cache Y
   cuando la primera fecha visible ya fue preparada; no debe existir pausa de
   varios segundos despues del fade para que aparezcan datos.
4) Tras entrar a la aplicacion, la fecha/hora activa solicitada tiene prioridad
   absoluta y sincronica: se sirve antes que cualquier vecino o barrido amplio.
5) Los vecinos cronologicos se precalientan despues, adelante/atras por distancia,
   del mas cercano al mas lejano, en segundo plano y sin robar la ruta activa.
6) El hilo progresivo posterior nunca bloquea bienvenida y corre con menor
   prioridad; el primer plano puede elevar prioridad temporalmente si pasan 2 s.
7) Nunca se sustituyen payloads por resumenes, sidecars ni agregados falsos.
*/
#ifdef _WIN32
static volatile LONG g_tlal_hot_lock_word = 0;
static void tlal_hot_lock(void) { while (InterlockedCompareExchange(&g_tlal_hot_lock_word, 1, 0) != 0) Sleep(0); }
static void tlal_hot_unlock(void) { InterlockedExchange(&g_tlal_hot_lock_word, 0); }
#else
static volatile int g_tlal_hot_lock_word = 0;
static void tlal_hot_lock(void) { while (__sync_lock_test_and_set(&g_tlal_hot_lock_word, 1)) sched_yield(); }
static void tlal_hot_unlock(void) { __sync_lock_release(&g_tlal_hot_lock_word); }
#endif

static void tlal_mapped_file_init(TlalMappedFile* mf) {
    if (!mf) return;
    memset(mf, 0, sizeof(*mf));
#ifdef _WIN32
    mf->file = INVALID_HANDLE_VALUE;
#else
    mf->fd = -1;
#endif
}

static uint32_t tlal_rd_u32_le(const unsigned char* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t tlal_rd_u64_le(const unsigned char* p) {
    return ((uint64_t)tlal_rd_u32_le(p)) | (((uint64_t)tlal_rd_u32_le(p + 4)) << 32);
}

static uint64_t tlal_u64_abs_diff(uint64_t a, uint64_t b) {
    return a >= b ? a - b : b - a;
}

static int tlal_has_suffix_ascii(const char* path, const char* suffix) {
    size_t n, m;
    if (!path || !suffix) return 0;
    n = strlen(path);
    m = strlen(suffix);
    if (m > n) return 0;
#ifdef _WIN32
    return _stricmp(path + n - m, suffix) == 0;
#else
    return strcmp(path + n - m, suffix) == 0;
#endif
}

static int tlal_join_path(char* out, size_t out_cap, const char* a, const char* b) {
    size_t na, nb;
    if (!out || out_cap == 0u || !a || !b) return 0;
    na = strlen(a);
    nb = strlen(b);
    if (na + nb + 2u > out_cap) return 0;
    memcpy(out, a, na);
    if (na > 0u && a[na - 1u] != '/' && a[na - 1u] != '\\') out[na++] = TLALPOWA_PATH_SEP;
    memcpy(out + na, b, nb + 1u);
    return 1;
}

static uint64_t tlal_file_size_stream(FILE* f) {
    long long end_pos;
    if (!f) return 0ull;
#ifdef _WIN32
    if (_fseeki64(f, 0, SEEK_END) != 0) return 0ull;
    end_pos = _ftelli64(f);
    if (_fseeki64(f, 0, SEEK_SET) != 0) return 0ull;
#else
    if (fseeko(f, 0, SEEK_END) != 0) return 0ull;
    end_pos = (long long)ftello(f);
    if (fseeko(f, 0, SEEK_SET) != 0) return 0ull;
#endif
    return end_pos > 0 ? (uint64_t)end_pos : 0ull;
}

static int tlal_seek_stream(FILE* f, uint64_t off) {
#ifdef _WIN32
    return _fseeki64(f, (long long)off, SEEK_SET) == 0;
#else
    return fseeko(f, (off_t)off, SEEK_SET) == 0;
#endif
}

static void tlal_unmap_file(TlalMappedFile* mf) {
    if (!mf) return;
#ifdef _WIN32
    if (mf->data) UnmapViewOfFile(mf->data);
    if (mf->mapping) CloseHandle(mf->mapping);
    if (mf->file && mf->file != INVALID_HANDLE_VALUE) CloseHandle(mf->file);
    mf->data = NULL;
    mf->mapping = NULL;
    mf->file = INVALID_HANDLE_VALUE;
    mf->size = 0ull;
    mf->mapped = 0;
#else
    if (mf->data && mf->data != MAP_FAILED) munmap(mf->data, (size_t)mf->size);
    if (mf->fd >= 0) close(mf->fd);
    mf->data = NULL;
    mf->fd = -1;
    mf->size = 0ull;
    mf->mapped = 0;
#endif
}

static int tlal_map_file_readonly(const char* path, TlalMappedFile* mf) {
    if (!path || !mf) return 0;
    tlal_mapped_file_init(mf);
#ifdef _WIN32
    mf->file = INVALID_HANDLE_VALUE;
    mf->mapping = NULL;
    mf->file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (mf->file == INVALID_HANDLE_VALUE) return 0;
    {
        LARGE_INTEGER li;
        if (!GetFileSizeEx(mf->file, &li) || li.QuadPart <= 0) { tlal_unmap_file(mf); return 0; }
        mf->size = (uint64_t)li.QuadPart;
    }
    if (mf->size > TLAL_MAX_PAYLOAD_BYTES) { tlal_unmap_file(mf); return 0; }
    mf->mapping = CreateFileMappingA(mf->file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!mf->mapping) { tlal_unmap_file(mf); return 0; }
    mf->data = (unsigned char*)MapViewOfFile(mf->mapping, FILE_MAP_READ, 0, 0, 0);
    if (!mf->data) { tlal_unmap_file(mf); return 0; }
    mf->mapped = 1;
    return 1;
#else
    mf->fd = open(path, O_RDONLY);
    if (mf->fd < 0) return 0;
    {
        struct stat sb;
        if (fstat(mf->fd, &sb) != 0 || sb.st_size <= 0) { tlal_unmap_file(mf); return 0; }
        mf->size = (uint64_t)sb.st_size;
    }
    if (mf->size > TLAL_MAX_PAYLOAD_BYTES) { tlal_unmap_file(mf); return 0; }
    mf->data = (unsigned char*)mmap(NULL, (size_t)mf->size, PROT_READ, MAP_PRIVATE, mf->fd, 0);
    if (mf->data == MAP_FAILED) { mf->data = NULL; tlal_unmap_file(mf); return 0; }
    mf->mapped = 1;
    return 1;
#endif
}

static void tlal_runtime_index_free(TlalHotRuntimeIndex* ix) {
    size_t i;
    if (!ix) return;
    for (i = 0; i < ix->cache_line_count && i < TLAL_HOT_CACHE_LINES_MAX; ++i) {
        free(ix->cache[i].data);
        ix->cache[i].data = NULL;
    }
    {
        size_t slot;
        for (slot = 0u; slot < TLAL_CORE_SLOT_COUNT; ++slot) free(ix->core_order[slot]);
    }
    for (i = 0; i < ix->file_count; ++i) {
        tlal_unmap_file(&ix->files[i].map);
    }
    ix->mapped_file_bytes = 0ull;
    free(ix->files);
    free(ix->records);
    free(ix->temporal_order);
    memset(ix, 0, sizeof(*ix));
}

void tlalpowa_hotdata_release_runtime_index(void) {
    tlal_hot_lock();
    tlal_runtime_index_free(&g_tlal_hot_index);
    tlal_hot_unlock();
}

static int tlal_runtime_add_file(TlalHotRuntimeIndex* ix, const char* path, uint64_t size, uint32_t kind, uint32_t* out_index) {
    TlalHotFile* nf;
    size_t nc;
    if (!ix || !path || !out_index) return 0;
    if (ix->file_count == ix->file_cap) {
        nc = ix->file_cap ? ix->file_cap * 2u : 64u;
        nf = (TlalHotFile*)realloc(ix->files, nc * sizeof(*nf));
        if (!nf) return 0;
        ix->files = nf;
        ix->file_cap = nc;
    }
    *out_index = (uint32_t)ix->file_count;
    memset(&ix->files[ix->file_count], 0, sizeof(ix->files[ix->file_count]));
    tlal_mapped_file_init(&ix->files[ix->file_count].map);
    snprintf(ix->files[ix->file_count].path, sizeof(ix->files[ix->file_count].path), "%s", path);
    ix->files[ix->file_count].size = size;
    ix->files[ix->file_count].kind = kind;
    ix->file_count++;
    return 1;
}

static int tlal_runtime_add_record(TlalHotRuntimeIndex* ix, const TlalHotRecord* rec, uint32_t* out_index) {
    TlalHotRecord* nr;
    size_t nc;
    if (!ix || !rec || !out_index) return 0;
    if (ix->record_count == ix->record_cap) {
        nc = ix->record_cap ? ix->record_cap * 2u : 4096u;
        nr = (TlalHotRecord*)realloc(ix->records, nc * sizeof(*nr));
        if (!nr) return 0;
        ix->records = nr;
        ix->record_cap = nc;
    }
    *out_index = (uint32_t)ix->record_count;
    ix->records[ix->record_count++] = *rec;
    return 1;
}

static int tlal_type_is_epi(uint32_t t) {
    return t == 1u || t == 2u || t == 101u || t == 106u;
}

static int tlal_type_is_atm(uint32_t t) {
    return t == 10u || t == 11u || t == 12u || t == 13u || t == 14u || t == 15u ||
           t == 16u || t == 102u || t == 103u || t == 104u || t == 105u;
}

static uint32_t tlal_core_group_for_type(uint32_t t) {
    if (tlal_type_is_epi(t)) return TLALPOWA_HOTDATA_CORE_EPIDEMIOLOGY;
    if (t == 105u) return TLALPOWA_HOTDATA_CORE_METEOROLOGY;
    if (tlal_type_is_atm(t)) return TLALPOWA_HOTDATA_CORE_CONTAMINANT;
    return TLALPOWA_HOTDATA_CORE_OTHER;
}

static void tlal_consider_candidate(TlalHotCandidate* slot, const TlalHotRecord* rec, uint32_t record_index) {
    if (!slot || !rec || rec->temporal_key == 0ull) return;
    if (rec->temporal_key < slot->temporal_key) return;
    slot->temporal_key = rec->temporal_key;
    slot->payload_offset = rec->payload_offset;
    slot->stored_size = rec->stored_size;
    slot->type = rec->type;
    slot->schema = rec->schema;
    slot->core_group = rec->core_group;
    slot->file_index = rec->file_index;
    slot->record_index = record_index;
}

static void tlal_touch_mapped_span(TlalHotState* st, const unsigned char* data, uint64_t file_size, uint64_t offset, uint64_t requested, uint64_t* out_touched) {
    uint64_t remain, pos, end;
    if (out_touched) *out_touched = 0ull;
    if (!st || !data || offset >= file_size || requested == 0ull || st->budget_left == 0ull) return;
    remain = file_size - offset;
    if (remain > requested) remain = requested;
    if (remain > st->budget_left) remain = st->budget_left;
    end = offset + remain;
    pos = offset;
    while (pos < end) {
        g_tlal_hot_sink ^= data[pos];
        pos += TLAL_PAGE_PROBE_BYTES;
    }
    if (end > offset) g_tlal_hot_sink ^= data[end - 1ull];
    st->budget_left -= remain;
    if (st->stats) st->stats->touched_bytes += remain;
    if (out_touched) *out_touched = remain;
}

static void tlal_touch_file_span(TlalHotState* st, const char* path, uint64_t offset, uint64_t requested) {
    FILE* f;
    uint64_t size, remain, touched_here;
    if (!st || !path || !*path || !st->touch_buffer || st->budget_left == 0ull || requested == 0ull) return;
    f = fopen(path, "rb");
    if (!f) return;
    size = tlal_file_size_stream(f);
    if (offset >= size) { fclose(f); return; }
    remain = size - offset;
    if (remain > requested) remain = requested;
    if (remain > st->budget_left) remain = st->budget_left;
    if (!tlal_seek_stream(f, offset)) { fclose(f); return; }
    touched_here = 0ull;
    while (remain > 0ull) {
        size_t step = remain > TLAL_TOUCH_BLOCK_BYTES ? TLAL_TOUCH_BLOCK_BYTES : (size_t)remain;
        size_t got = fread(st->touch_buffer, 1u, step, f);
        if (got == 0u) break;
        g_tlal_hot_sink ^= st->touch_buffer[0];
        remain -= (uint64_t)got;
        st->budget_left -= (uint64_t)got;
        touched_here += (uint64_t)got;
        if (got < step || st->budget_left == 0ull) break;
    }
    if (st->stats) st->stats->touched_bytes += touched_here;
    fclose(f);
}

static uint64_t tlal_read_path_span(const char* path, uint64_t offset, void* out, uint64_t requested) {
    FILE* f;
    uint64_t size, remain;
    size_t got;
    if (!path || !*path || !out || requested == 0ull) return 0ull;
    f = fopen(path, "rb");
    if (!f) return 0ull;
    size = tlal_file_size_stream(f);
    if (offset >= size) { fclose(f); return 0ull; }
    remain = size - offset;
    if (remain > requested) remain = requested;
    if (remain > (uint64_t)SIZE_MAX) remain = (uint64_t)SIZE_MAX;
    if (!tlal_seek_stream(f, offset)) { fclose(f); return 0ull; }
    got = fread(out, 1u, (size_t)remain, f);
    fclose(f);
    return got;
}

static uint64_t tlal_read_hot_file_span(const TlalHotFile* file, uint64_t offset, void* out, uint64_t requested) {
    uint64_t remain;
    if (!file || !out || requested == 0ull) return 0ull;
    if (file->map.data && offset < file->map.size) {
        remain = file->map.size - offset;
        if (remain > requested) remain = requested;
        if (remain > (uint64_t)SIZE_MAX) remain = (uint64_t)SIZE_MAX;
        memcpy(out, file->map.data + offset, (size_t)remain);
        return remain;
    }
    return tlal_read_path_span(file->path, offset, out, requested);
}

static uint64_t tlal_runtime_cache_limit(const TlalHotRuntimeIndex* ix) {
    if (!ix || ix->cache_limit_bytes == 0ull) return TLAL_HOT_DEFAULT_CACHE_BYTES;
    return ix->cache_limit_bytes;
}

static uint32_t tlal_runtime_cache_lines(const TlalHotRuntimeIndex* ix) {
    if (!ix || ix->cache_line_count == 0u) return TLAL_HOT_CACHE_LINES_DEFAULT;
    return ix->cache_line_count > TLAL_HOT_CACHE_LINES_MAX ? TLAL_HOT_CACHE_LINES_MAX : ix->cache_line_count;
}

static int tlal_cache_line_matches(const TlalHotCacheLine* ln, const TlalHotRecord* rec, uint64_t need) {
    return ln && ln->data && rec && ln->file_index == rec->file_index &&
           ln->payload_offset == rec->payload_offset && ln->temporal_key == rec->temporal_key &&
           ln->layer_hash == rec->layer_hash && ln->bytes >= need;
}

static void tlal_cache_evict_line(TlalHotRuntimeIndex* ix, size_t pos) {
    if (!ix || pos >= TLAL_HOT_CACHE_LINES_MAX) return;
    if (ix->cache[pos].data) {
        if (ix->cache_bytes >= ix->cache[pos].bytes) ix->cache_bytes -= ix->cache[pos].bytes;
        else ix->cache_bytes = 0ull;
        free(ix->cache[pos].data);
    }
    memset(&ix->cache[pos], 0, sizeof(ix->cache[pos]));
}

static size_t tlal_cache_pick_slot(TlalHotRuntimeIndex* ix) {
    uint32_t limit, i;
    size_t oldest = 0u;
    uint64_t oldest_tick = UINT64_MAX;
    if (!ix) return 0u;
    limit = tlal_runtime_cache_lines(ix);
    for (i = 0u; i < limit; ++i) {
        if (!ix->cache[i].data) return (size_t)i;
        if (ix->cache[i].tick < oldest_tick) { oldest_tick = ix->cache[i].tick; oldest = (size_t)i; }
    }
    return oldest;
}

static uint64_t tlal_cache_load_record(TlalHotState* st, const TlalHotRecord* rec, uint64_t bytes) {
    const TlalHotFile* file;
    unsigned char* mem;
    uint64_t need, limit, room, got;
    size_t pos;
    uint32_t line_limit;
    if (!st || !rec || rec->file_index >= st->index.file_count || bytes == 0ull) return 0ull;
    file = &st->index.files[rec->file_index];
    if (!file->path[0]) return 0ull;
    need = rec->stored_size < bytes ? rec->stored_size : bytes;
    if (need == 0ull) return 0ull;
    limit = tlal_runtime_cache_limit(&st->index);
    if (limit == 0ull) return 0ull;
    if (need > (uint64_t)st->cfg.max_payload_bytes_per_record && st->cfg.max_payload_bytes_per_record != 0u)
        need = (uint64_t)st->cfg.max_payload_bytes_per_record;
    if (need > limit / 2ull && limit >= 2ull) need = limit / 2ull;
    if (st->budget_left != 0ull && need > st->budget_left) need = st->budget_left;
    if (need == 0ull) return 0ull;
    line_limit = tlal_runtime_cache_lines(&st->index);
    for (pos = 0u; pos < (size_t)line_limit; ++pos) {
        if (tlal_cache_line_matches(&st->index.cache[pos], rec, need)) {
            st->index.cache[pos].tick = ++st->index.cache_tick;
            if (st->stats) { st->stats->cache_hits += 1ull; st->stats->cache_bytes = st->index.cache_bytes; }
            g_tlal_hot_sink ^= st->index.cache[pos].data[0];
            return need;
        }
    }
    if (st->stats) st->stats->cache_misses += 1ull;
    room = limit;
    while (st->index.cache_bytes + need > room) {
        uint64_t before = st->index.cache_bytes;
        tlal_cache_evict_line(&st->index, tlal_cache_pick_slot(&st->index));
        if (st->index.cache_bytes == before) break;
    }
    if (need > room || st->index.cache_bytes + need > room) return 0ull;
    mem = (unsigned char*)malloc((size_t)need);
    if (!mem) return 0ull;
    got = tlal_read_hot_file_span(file, rec->payload_offset, mem, need);
    if (got == 0ull) { free(mem); return 0ull; }
    pos = tlal_cache_pick_slot(&st->index);
    tlal_cache_evict_line(&st->index, pos);
    st->index.cache[pos].data = mem;
    st->index.cache[pos].bytes = got;
    st->index.cache[pos].tick = ++st->index.cache_tick;
    st->index.cache[pos].temporal_key = rec->temporal_key;
    st->index.cache[pos].payload_offset = rec->payload_offset;
    st->index.cache[pos].layer_hash = rec->layer_hash;
    st->index.cache[pos].file_index = rec->file_index;
    st->index.cache[pos].record_index = (uint32_t)(rec - st->index.records);
    st->index.cache[pos].core_group = rec->core_group;
    st->index.cache_bytes += got;
    st->budget_left = st->budget_left > got ? st->budget_left - got : 0ull;
    if (st->stats) { st->stats->touched_bytes += got; st->stats->cache_bytes = st->index.cache_bytes; }
    g_tlal_hot_sink ^= mem[0];
    return (uint64_t)got;
}

static void tlal_touch_record(TlalHotState* st, const TlalHotRecord* rec, uint64_t bytes) {
    const TlalHotFile* file;
    uint64_t n;
    if (!st || !rec || rec->file_index >= st->index.file_count || bytes == 0ull) return;
    n = rec->stored_size < bytes ? rec->stored_size : bytes;
    if (tlal_cache_load_record(st, rec, n) != 0ull) return;
    file = &st->index.files[rec->file_index];
    if (file->map.data) {
        tlal_touch_mapped_span(st, file->map.data, file->map.size, rec->payload_offset, n, NULL);
        return;
    }
    tlal_touch_file_span(st, file->path, rec->payload_offset, n);
}

static int tlal_parse_ixiptlah_directory_mapped(TlalHotState* st, const char* path) {
    TlalMappedFile mf;
    uint64_t size, count, dir_off, entries_bytes, expected_end;
    uint32_t version, entry_size, dir_version, file_index;
    const unsigned char* trailer;
    const unsigned char* dir;
    const unsigned char* entry;
    uint64_t i;
    if (!st || !path) return 0;
    if (!tlal_map_file_readonly(path, &mf)) return 0;
    size = mf.size;
    if (st->stats) st->stats->mapped_files += 1ull;
    if (size < 12ull + TLAL_IX_TRAILER_BYTES) { tlal_unmap_file(&mf); return 0; }
    if (memcmp(mf.data, TLAL_IX_MAGIC, sizeof(TLAL_IX_MAGIC)) != 0) { tlal_unmap_file(&mf); return 0; }
    version = tlal_rd_u32_le(mf.data + 8);
    if (version != 1u) { tlal_unmap_file(&mf); return 0; }
    trailer = mf.data + (size - TLAL_IX_TRAILER_BYTES);
    if (memcmp(trailer, TLAL_IX_DIR_END_MAGIC, sizeof(TLAL_IX_DIR_END_MAGIC)) != 0) { tlal_unmap_file(&mf); return 0; }
    dir_version = tlal_rd_u32_le(trailer + 8);
    entry_size = tlal_rd_u32_le(trailer + 12);
    count = tlal_rd_u64_le(trailer + 16);
    dir_off = tlal_rd_u64_le(trailer + 24);
    if (dir_version != 1u || entry_size < 48u || entry_size > TLAL_IX_DIR_ENTRY_BYTES) { tlal_unmap_file(&mf); return 0; }
    if (count > 16777216ull) { tlal_unmap_file(&mf); return 0; }
    entries_bytes = count * (uint64_t)entry_size;
    if (count != 0ull && entries_bytes / count != (uint64_t)entry_size) { tlal_unmap_file(&mf); return 0; }
    expected_end = dir_off + TLAL_IX_DIR_HEADER_BYTES + entries_bytes;
    if (expected_end != size - TLAL_IX_TRAILER_BYTES) { tlal_unmap_file(&mf); return 0; }
    if (dir_off + TLAL_IX_DIR_HEADER_BYTES > size) { tlal_unmap_file(&mf); return 0; }
    dir = mf.data + dir_off;
    if (memcmp(dir, TLAL_IX_DIR_MAGIC, sizeof(TLAL_IX_DIR_MAGIC)) != 0) { tlal_unmap_file(&mf); return 0; }
    if (tlal_rd_u32_le(dir + 8) != 1u || tlal_rd_u32_le(dir + 12) != entry_size || tlal_rd_u64_le(dir + 16) != count) { tlal_unmap_file(&mf); return 0; }
    if (!tlal_runtime_add_file(&st->index, path, size, 1u, &file_index)) { tlal_unmap_file(&mf); return 0; }

    if (st->stats) {
        st->stats->ixiptlah_records += count;
        st->stats->ixiptlah_directories += 1ull;
    }

    entry = dir + TLAL_IX_DIR_HEADER_BYTES;
    for (i = 0; i < count; ++i, entry += entry_size) {
        TlalHotRecord rec;
        uint32_t record_index;
        uint64_t probed = 0ull;
        memset(&rec, 0, sizeof(rec));
        rec.type = tlal_rd_u32_le(entry + 0);
        rec.schema = tlal_rd_u32_le(entry + 4);
        rec.payload_offset = tlal_rd_u64_le(entry + 8);
        rec.stored_size = tlal_rd_u64_le(entry + 16);
        rec.raw_size = tlal_rd_u64_le(entry + 24);
        rec.codec = tlal_rd_u32_le(entry + 32);
        rec.quality_flags = tlal_rd_u32_le(entry + 36);
        rec.layer_hash = tlal_rd_u64_le(entry + 40);
        rec.temporal_key = tlal_rd_u64_le(entry + 48);
        rec.file_index = file_index;
        if (entry_size >= 104u) {
            rec.narrow_bucket = tlal_rd_u64_le(entry + 64);
            rec.hour_bucket = tlal_rd_u64_le(entry + 72);
            rec.week_bucket = tlal_rd_u64_le(entry + 80);
            rec.wide_bucket = tlal_rd_u64_le(entry + 88);
            rec.core_group = tlal_rd_u32_le(entry + 96);
            rec.quality_flags = tlal_rd_u32_le(entry + 100);
        }
        if (rec.core_group == 0u) rec.core_group = tlal_core_group_for_type(rec.type);
        if (rec.payload_offset > dir_off || rec.stored_size > dir_off - rec.payload_offset) continue;
        if (rec.stored_size > TLAL_MAX_PAYLOAD_BYTES || rec.raw_size > TLAL_MAX_PAYLOAD_BYTES) continue;
        if (rec.codec != 0u && rec.codec != 1u && rec.codec != 2u) continue;
        if (!tlal_runtime_add_record(&st->index, &rec, &record_index)) break;
        if (st->stats) st->stats->indexed_records += 1ull;
        tlal_consider_candidate(&st->latest_any, &rec, record_index);
        if (rec.core_group == TLALPOWA_HOTDATA_CORE_EPIDEMIOLOGY) tlal_consider_candidate(&st->latest_epi, &rec, record_index);
        else if (rec.core_group == TLALPOWA_HOTDATA_CORE_METEOROLOGY) tlal_consider_candidate(&st->latest_met, &rec, record_index);
        else if (rec.core_group == TLALPOWA_HOTDATA_CORE_CONTAMINANT) tlal_consider_candidate(&st->latest_con, &rec, record_index);
        else tlal_consider_candidate(&st->latest_oth, &rec, record_index);
        if (st->cfg.probe_bytes_per_record != 0u && st->budget_left != 0ull) {
            uint64_t n = rec.stored_size < (uint64_t)st->cfg.probe_bytes_per_record ? rec.stored_size : (uint64_t)st->cfg.probe_bytes_per_record;
            tlal_touch_mapped_span(st, mf.data, mf.size, rec.payload_offset, n, &probed);
            if (st->stats) st->stats->record_probe_bytes += probed;
        }
    }
    if (st->cfg.keep_runtime_index && file_index < st->index.file_count && mf.mapped &&
        st->index.mapped_file_bytes <= st->index.mapped_file_limit_bytes &&
        mf.size <= st->index.mapped_file_limit_bytes - st->index.mapped_file_bytes) {
        st->index.files[file_index].map = mf;
        st->index.mapped_file_bytes += mf.size;
        if (st->stats) st->stats->retained_mapped_file_bytes = st->index.mapped_file_bytes;
        tlal_mapped_file_init(&mf);
    }
    tlal_unmap_file(&mf);
    return 1;
}

static void tlal_prewarm_candidate(TlalHotState* st, const TlalHotCandidate* c, uint64_t per) {
    if (!st || !c || c->temporal_key == 0ull || c->record_index >= st->index.record_count) return;
    tlal_touch_record(st, &st->index.records[c->record_index], per);
}

static void tlal_prewarm_latest_candidates(TlalHotState* st) {
    uint64_t per;
    if (!st) return;
    per = st->cfg.max_payload_bytes_per_record ? (uint64_t)st->cfg.max_payload_bytes_per_record : (2ull * 1024ull * 1024ull);
    tlal_prewarm_candidate(st, &st->latest_con, per);
    tlal_prewarm_candidate(st, &st->latest_met, per);
    tlal_prewarm_candidate(st, &st->latest_epi, per);
    tlal_prewarm_candidate(st, &st->latest_oth, per / 2ull);
    if (st->stats) {
        st->stats->latest_contaminant_key = st->latest_con.temporal_key;
        st->stats->latest_meteorology_key = st->latest_met.temporal_key;
        st->stats->latest_epidemiology_key = st->latest_epi.temporal_key;
        st->stats->latest_atmosphere_key = st->latest_con.temporal_key ? st->latest_con.temporal_key : st->latest_met.temporal_key;
        st->stats->latest_temporal_key = st->latest_any.temporal_key;
    }
}

static const TlalHotRecord* g_sort_records = NULL;
static int tlal_cmp_record_order(const void* a, const void* b) {
    const uint32_t ia = *(const uint32_t*)a;
    const uint32_t ib = *(const uint32_t*)b;
    const TlalHotRecord* ra = &g_sort_records[ia];
    const TlalHotRecord* rb = &g_sort_records[ib];
    if (ra->temporal_key != rb->temporal_key) return ra->temporal_key < rb->temporal_key ? -1 : 1;
    if (ra->core_group != rb->core_group) return ra->core_group < rb->core_group ? -1 : 1;
    if (ra->type != rb->type) return ra->type < rb->type ? -1 : 1;
    if (ra->file_index != rb->file_index) return ra->file_index < rb->file_index ? -1 : 1;
    if (ra->payload_offset != rb->payload_offset) return ra->payload_offset < rb->payload_offset ? -1 : 1;
    return 0;
}

static uint32_t tlal_core_slot(uint32_t core_group) {
    if (core_group == TLALPOWA_HOTDATA_CORE_EPIDEMIOLOGY) return TLAL_CORE_SLOT_EPI;
    if (core_group == TLALPOWA_HOTDATA_CORE_METEOROLOGY) return TLAL_CORE_SLOT_MET;
    if (core_group == TLALPOWA_HOTDATA_CORE_CONTAMINANT) return TLAL_CORE_SLOT_CON;
    if (core_group == TLALPOWA_HOTDATA_CORE_OTHER) return TLAL_CORE_SLOT_OTH;
    return TLAL_CORE_SLOT_ANY;
}

static int tlal_runtime_alloc_order(uint32_t** out, size_t n) {
    if (!out) return 0;
    *out = NULL;
    if (n == 0u) return 1;
    *out = (uint32_t*)malloc(n * sizeof(**out));
    return *out != NULL;
}

static int tlal_runtime_build_temporal_order(TlalHotRuntimeIndex* ix) {
    size_t i, n;
    size_t counts[TLAL_CORE_SLOT_COUNT];
    size_t write_pos[TLAL_CORE_SLOT_COUNT];
    if (!ix || ix->record_count == 0u) return 0;
    free(ix->temporal_order);
    ix->temporal_order = NULL;
    ix->temporal_order_count = 0u;
    for (i = 0u; i < TLAL_CORE_SLOT_COUNT; ++i) {
        free(ix->core_order[i]);
        ix->core_order[i] = NULL;
        ix->core_order_count[i] = 0u;
        counts[i] = 0u;
        write_pos[i] = 0u;
    }
    n = ix->record_count;
    if (!tlal_runtime_alloc_order(&ix->temporal_order, n)) return 0;
    for (i = 0u; i < n; ++i) {
        uint32_t slot = tlal_core_slot(ix->records[i].core_group);
        ix->temporal_order[i] = (uint32_t)i;
        if (slot != TLAL_CORE_SLOT_ANY && slot < TLAL_CORE_SLOT_COUNT) counts[slot] += 1u;
    }
    for (i = 1u; i < TLAL_CORE_SLOT_COUNT; ++i) {
        if (!tlal_runtime_alloc_order(&ix->core_order[i], counts[i])) {
            size_t j;
            for (j = 1u; j < TLAL_CORE_SLOT_COUNT; ++j) { free(ix->core_order[j]); ix->core_order[j] = NULL; ix->core_order_count[j] = 0u; }
            free(ix->temporal_order); ix->temporal_order = NULL; ix->temporal_order_count = 0u;
            return 0;
        }
        ix->core_order_count[i] = counts[i];
    }
    for (i = 0u; i < n; ++i) {
        uint32_t slot = tlal_core_slot(ix->records[i].core_group);
        if (slot != TLAL_CORE_SLOT_ANY && slot < TLAL_CORE_SLOT_COUNT && ix->core_order[slot]) {
            ix->core_order[slot][write_pos[slot]++] = (uint32_t)i;
        }
    }
    g_sort_records = ix->records;
    qsort(ix->temporal_order, n, sizeof(*ix->temporal_order), tlal_cmp_record_order);
    ix->temporal_order_count = n;
    for (i = 1u; i < TLAL_CORE_SLOT_COUNT; ++i) {
        if (ix->core_order[i] && ix->core_order_count[i] > 1u)
            qsort(ix->core_order[i], ix->core_order_count[i], sizeof(*ix->core_order[i]), tlal_cmp_record_order);
    }
    g_sort_records = NULL;
    return 1;
}

static int tlal_record_matches_core(const TlalHotRecord* r, uint32_t core_group) {
    return r && (core_group == TLALPOWA_HOTDATA_CORE_ANY || r->core_group == core_group);
}

static const uint32_t* tlal_runtime_order_for_core(const TlalHotRuntimeIndex* ix, uint32_t core_group, size_t* out_count) {
    uint32_t slot;
    if (out_count) *out_count = 0u;
    if (!ix) return NULL;
    slot = tlal_core_slot(core_group);
    if (slot != TLAL_CORE_SLOT_ANY && slot < TLAL_CORE_SLOT_COUNT && ix->core_order[slot] && ix->core_order_count[slot] != 0u) {
        if (out_count) *out_count = ix->core_order_count[slot];
        return ix->core_order[slot];
    }
    if (ix->temporal_order && ix->temporal_order_count != 0u) {
        if (out_count) *out_count = ix->temporal_order_count;
        return ix->temporal_order;
    }
    return NULL;
}

static size_t tlal_temporal_lower_bound_order(const TlalHotRuntimeIndex* ix, const uint32_t* order, size_t count, uint64_t key) {
    size_t lo = 0u, hi = count;
    if (!ix || !order || count == 0u) return 0u;
    while (lo < hi) {
        size_t mid = lo + ((hi - lo) >> 1u);
        const TlalHotRecord* r = &ix->records[order[mid]];
        if (r->temporal_key < key) lo = mid + 1u;
        else hi = mid;
    }
    return lo;
}

static int tlal_runtime_find_nearest_record_index(const TlalHotRuntimeIndex* ix, uint32_t core_group, uint64_t temporal_key, uint32_t* out_record_index) {
    const uint32_t* order;
    size_t order_count;
    size_t right, left;
    uint64_t best = UINT64_MAX;
    uint32_t best_index = UINT32_MAX;
    uint32_t guard = 0u;
    if (out_record_index) *out_record_index = UINT32_MAX;
    if (!ix || !out_record_index || !ix->records || ix->record_count == 0u || temporal_key == 0ull) return 0;
    order = tlal_runtime_order_for_core(ix, core_group, &order_count);
    if (!order || order_count == 0u) {
        size_t i;
        for (i = 0; i < ix->record_count; ++i) {
            const TlalHotRecord* r = &ix->records[i];
            uint64_t d;
            if (!tlal_record_matches_core(r, core_group) || r->temporal_key == 0ull) continue;
            d = tlal_u64_abs_diff(r->temporal_key, temporal_key);
            if (d < best) { best = d; best_index = (uint32_t)i; if (d == 0ull) break; }
        }
        if (best_index == UINT32_MAX) return 0;
        *out_record_index = best_index;
        return 1;
    }
    right = tlal_temporal_lower_bound_order(ix, order, order_count, temporal_key);
    left = right;
    while ((left > 0u || right < order_count) && guard < 1024u) {
        int take_left = 0;
        if (left > 0u && right < order_count) {
            const TlalHotRecord* rl = &ix->records[order[left - 1u]];
            const TlalHotRecord* rr = &ix->records[order[right]];
            take_left = tlal_u64_abs_diff(rl->temporal_key, temporal_key) <= tlal_u64_abs_diff(rr->temporal_key, temporal_key);
        } else take_left = left > 0u;
        if (take_left) {
            const uint32_t ri = order[--left];
            const TlalHotRecord* r = &ix->records[ri];
            uint64_t d = tlal_u64_abs_diff(r->temporal_key, temporal_key);
            if (d > best && best_index != UINT32_MAX) break;
            if (tlal_record_matches_core(r, core_group)) { best = d; best_index = ri; if (d == 0ull) break; }
        } else {
            const uint32_t ri = order[right++];
            const TlalHotRecord* r = &ix->records[ri];
            uint64_t d = tlal_u64_abs_diff(r->temporal_key, temporal_key);
            if (d > best && best_index != UINT32_MAX) break;
            if (tlal_record_matches_core(r, core_group)) { best = d; best_index = ri; if (d == 0ull) break; }
        }
        ++guard;
    }
    if (best_index == UINT32_MAX) return 0;
    *out_record_index = best_index;
    return 1;
}

static void tlal_hit_from_record(const TlalHotRuntimeIndex* ix, const TlalHotRecord* r, TlalpowaHotDataHit* hit) {
    if (!ix || !r || !hit || r->file_index >= ix->file_count) return;
    memset(hit, 0, sizeof(*hit));
    hit->temporal_key = r->temporal_key;
    hit->payload_offset = r->payload_offset;
    hit->stored_size = r->stored_size;
    hit->layer_hash = r->layer_hash;
    hit->type = r->type;
    hit->schema = r->schema;
    hit->core_group = r->core_group;
    hit->file_index = r->file_index;
    snprintf(hit->path, sizeof(hit->path), "%s", ix->files[r->file_index].path);
}

/*
Orden temporal estricto: la salida inicia con la llave solicitada si existe; si no,
con el registro real mas cercano. Despues alterna izquierda/derecha segun
distancia absoluta, por lo que el prefetch avanza desde el vecino mas cercano
hacia el mas lejano sin barrer registros ajenos a la familia solicitada.
Durante la bienvenida esta rutina se usa solo con los ultimos registros por
categoria; los vecinos cronologicos amplios se cargan ya con la interfaz viva.
*/
static uint32_t tlal_collect_record_indices_near(const TlalHotRuntimeIndex* ix,
                                                 uint32_t core_group,
                                                 uint64_t temporal_key,
                                                 uint32_t max_hits,
                                                 uint32_t* out_indices,
                                                 uint64_t* out_exact_hits) {
    const uint32_t* order;
    size_t order_count;
    size_t right, left;
    uint32_t out = 0u;
    uint32_t guard = 0u;
    uint64_t exact = 0ull;
    if (out_exact_hits) *out_exact_hits = 0ull;
    if (!ix || !out_indices || max_hits == 0u || temporal_key == 0ull || !ix->records || ix->record_count == 0u) return 0u;
    order = tlal_runtime_order_for_core(ix, core_group, &order_count);
    if (!order || order_count == 0u) return 0u;
    right = tlal_temporal_lower_bound_order(ix, order, order_count, temporal_key);
    left = right;
    while (out < max_hits && (left > 0u || right < order_count) && guard < 4096u) {
        int take_left = 0;
        const TlalHotRecord* r;
        uint32_t ri;
        if (left > 0u && right < order_count) {
            const TlalHotRecord* rl = &ix->records[order[left - 1u]];
            const TlalHotRecord* rr = &ix->records[order[right]];
            take_left = tlal_u64_abs_diff(rl->temporal_key, temporal_key) <= tlal_u64_abs_diff(rr->temporal_key, temporal_key);
        } else {
            take_left = left > 0u;
        }
        ri = take_left ? order[--left] : order[right++];
        r = &ix->records[ri];
        if (tlal_record_matches_core(r, core_group)) {
            out_indices[out++] = ri;
            if (r->temporal_key == temporal_key) exact += 1ull;
        }
        ++guard;
    }
    if (out_exact_hits) *out_exact_hits = exact;
    return out;
}

/*
Prefetch progresivo: temporal_key es la fecha activa o la ultima fecha real de
una categoria. Se toca primero esa llave o su vecino fisico mas cercano; luego
los registros adelante/atras por cercania temporal real, sin bloquear la
bienvenida cuando se invoca desde el hilo de fondo.
*/
static void tlal_prewarm_temporal_near(TlalHotState* st, uint32_t core_group, uint64_t temporal_key, uint32_t want, uint64_t bytes) {
    const uint32_t* order;
    size_t order_count;
    size_t right, left;
    uint32_t touched = 0u, guard = 0u;
    if (!st || temporal_key == 0ull || want == 0u || bytes == 0ull || st->index.record_count == 0u) return;
    if (!st->index.temporal_order || st->index.temporal_order_count == 0u) (void)tlal_runtime_build_temporal_order(&st->index);
    order = tlal_runtime_order_for_core(&st->index, core_group, &order_count);
    if (!order || order_count == 0u) return;
    right = tlal_temporal_lower_bound_order(&st->index, order, order_count, temporal_key);
    left = right;
    if (st->stats) st->stats->binary_searches += 1ull;
    while (touched < want && (left > 0u || right < order_count) && guard < 4096u && st->budget_left != 0ull) {
        int take_left = 0;
        if (left > 0u && right < order_count) {
            const TlalHotRecord* rl = &st->index.records[order[left - 1u]];
            const TlalHotRecord* rr = &st->index.records[order[right]];
            take_left = tlal_u64_abs_diff(rl->temporal_key, temporal_key) <= tlal_u64_abs_diff(rr->temporal_key, temporal_key);
        } else take_left = left > 0u;
        if (take_left) {
            const TlalHotRecord* r = &st->index.records[order[--left]];
            if (tlal_record_matches_core(r, core_group)) { tlal_touch_record(st, r, bytes); ++touched; }
        } else {
            const TlalHotRecord* r = &st->index.records[order[right++]];
            if (tlal_record_matches_core(r, core_group)) { tlal_touch_record(st, r, bytes); ++touched; }
        }
        ++guard;
    }
    if (st->stats) st->stats->progressive_records_touched += touched;
}

static int tlal_startup_category_same(const TlalStartupCategory* c, const TlalHotRecord* r) {
    return c && r && c->core_group == r->core_group && c->type == r->type &&
           c->schema == r->schema && c->layer_hash == r->layer_hash;
}

static void tlal_startup_category_insert_record(TlalStartupCategory* c,
                                                const TlalHotRuntimeIndex* ix,
                                                uint32_t record_index,
                                                uint32_t per_category) {
    uint32_t pos, i;
    uint64_t key;
    if (!c || !ix || record_index >= ix->record_count || per_category == 0u) return;
    if (per_category > TLAL_HOT_STARTUP_GATE_RECORDS_MAX) per_category = TLAL_HOT_STARTUP_GATE_RECORDS_MAX;
    key = ix->records[record_index].temporal_key;
    if (key == 0ull) return;
    for (i = 0u; i < c->record_count; ++i) {
        if (c->record_indices[i] == record_index) return;
    }
    pos = c->record_count;
    for (i = 0u; i < c->record_count; ++i) {
        const uint32_t ri = c->record_indices[i];
        const uint64_t old_key = ri < ix->record_count ? ix->records[ri].temporal_key : 0ull;
        if (key > old_key || (key == old_key && record_index > ri)) { pos = i; break; }
    }
    if (c->record_count < per_category) {
        for (i = c->record_count; i > pos; --i) c->record_indices[i] = c->record_indices[i - 1u];
        c->record_indices[pos] = record_index;
        c->record_count += 1u;
    } else if (pos < per_category) {
        for (i = per_category - 1u; i > pos; --i) c->record_indices[i] = c->record_indices[i - 1u];
        c->record_indices[pos] = record_index;
    }
    if (c->record_count != 0u) {
        const uint32_t top = c->record_indices[0];
        c->temporal_key = top < ix->record_count ? ix->records[top].temporal_key : key;
    }
}

static void tlal_startup_category_consider(TlalStartupCategory* cats,
                                           uint32_t* count,
                                           uint32_t limit,
                                           const TlalHotRuntimeIndex* ix,
                                           uint32_t record_index,
                                           uint32_t per_category) {
    uint32_t i;
    uint32_t weakest = UINT32_MAX;
    uint64_t weakest_key = UINT64_MAX;
    const TlalHotRecord* r;
    if (!cats || !count || !ix || record_index >= ix->record_count || limit == 0u) return;
    r = &ix->records[record_index];
    if (r->temporal_key == 0ull) return;
    for (i = 0u; i < *count; ++i) {
        if (tlal_startup_category_same(&cats[i], r)) {
            tlal_startup_category_insert_record(&cats[i], ix, record_index, per_category);
            return;
        }
    }
    if (*count < limit) {
        TlalStartupCategory* c = &cats[(*count)++];
        memset(c, 0, sizeof(*c));
        c->core_group = r->core_group;
        c->type = r->type;
        c->schema = r->schema;
        c->layer_hash = r->layer_hash;
        tlal_startup_category_insert_record(c, ix, record_index, per_category);
        return;
    }
    for (i = 0u; i < *count; ++i) {
        if (cats[i].temporal_key < weakest_key) { weakest_key = cats[i].temporal_key; weakest = i; }
    }
    if (weakest != UINT32_MAX && r->temporal_key > cats[weakest].temporal_key) {
        TlalStartupCategory* c = &cats[weakest];
        memset(c, 0, sizeof(*c));
        c->core_group = r->core_group;
        c->type = r->type;
        c->schema = r->schema;
        c->layer_hash = r->layer_hash;
        tlal_startup_category_insert_record(c, ix, record_index, per_category);
    }
}

static int tlal_startup_category_cmp_desc(const void* a, const void* b) {
    const TlalStartupCategory* ca = (const TlalStartupCategory*)a;
    const TlalStartupCategory* cb = (const TlalStartupCategory*)b;
    if (ca->core_group != cb->core_group) return ca->core_group < cb->core_group ? -1 : 1;
    if (ca->type != cb->type) return ca->type < cb->type ? -1 : 1;
    if (ca->temporal_key != cb->temporal_key) return ca->temporal_key > cb->temporal_key ? -1 : 1;
    if (ca->schema != cb->schema) return ca->schema < cb->schema ? -1 : 1;
    if (ca->layer_hash != cb->layer_hash) return ca->layer_hash < cb->layer_hash ? -1 : 1;
    return 0;
}

static void tlal_prewarm_latest_categories(TlalHotState* st) {
    TlalStartupCategory cats[TLAL_HOT_STARTUP_CATEGORY_LIMIT_MAX];
    uint32_t count = 0u;
    uint32_t limit, per_category, i, j;
    uint64_t bytes, expected;
    if (!st || st->index.record_count == 0u) return;
    limit = st->cfg.startup_gate_category_limit;
    if (limit == 0u) limit = TLAL_HOT_STARTUP_CATEGORY_LIMIT_DEFAULT;
    if (limit > TLAL_HOT_STARTUP_CATEGORY_LIMIT_MAX) limit = TLAL_HOT_STARTUP_CATEGORY_LIMIT_MAX;
    per_category = st->cfg.startup_gate_records_per_core;
    if (per_category == 0u) per_category = TLAL_HOT_STARTUP_GATE_RECORDS_DEFAULT;
    if (per_category > TLAL_HOT_STARTUP_GATE_RECORDS_MAX) per_category = TLAL_HOT_STARTUP_GATE_RECORDS_MAX;
    memset(cats, 0, sizeof(cats));
    for (i = 0u; i < st->index.record_count; ++i) {
        const TlalHotRecord* r = &st->index.records[i];
        if (r->core_group == TLALPOWA_HOTDATA_CORE_ANY) continue;
        tlal_startup_category_consider(cats, &count, limit, &st->index, i, per_category);
    }
    if (count > 1u) qsort(cats, count, sizeof(cats[0]), tlal_startup_category_cmp_desc);
    expected = 0ull;
    for (i = 0u; i < count; ++i) expected += (uint64_t)cats[i].record_count;
    if (st->stats) st->stats->startup_gate_expected_hits = expected;
    bytes = st->cfg.startup_gate_bytes_per_record;
    if (bytes == 0ull) bytes = TLAL_HOT_STARTUP_GATE_BYTES_DEFAULT;
    for (i = 0u; i < count && st->budget_left != 0ull; ++i) {
        for (j = 0u; j < cats[i].record_count && j < per_category && st->budget_left != 0ull; ++j) {
            const uint32_t ri = cats[i].record_indices[j];
            const TlalHotRecord* r;
            uint64_t got;
            if (ri >= st->index.record_count) continue;
            r = &st->index.records[ri];
            got = tlal_cache_load_record(st, r, bytes);
            if (got != 0ull && st->stats) {
                st->stats->startup_gate_hits += 1ull;
                st->stats->startup_gate_bytes += got;
                st->stats->prepared_hits += 1ull;
                st->stats->prepared_bytes += got;
            }
        }
    }
    if (st->stats) {
        st->stats->startup_gate_categories = count;
        st->stats->progressive_records_touched += st->stats->startup_gate_hits;
        st->stats->cache_bytes = st->index.cache_bytes;
    }
}

static void tlal_startup_gate_core(TlalHotState* st, uint32_t core_group, uint64_t temporal_key) {
    uint32_t indices[TLAL_HOT_STARTUP_GATE_RECORDS_MAX];
    uint32_t cap, count, i;
    uint64_t exact_hits = 0ull;
    uint64_t bytes;
    if (!st || temporal_key == 0ull || st->index.record_count == 0u) return;
    if (!st->index.temporal_order || st->index.temporal_order_count == 0u) (void)tlal_runtime_build_temporal_order(&st->index);
    cap = st->cfg.startup_gate_records_per_core;
    if (cap == 0u) return;
    if (cap > TLAL_HOT_STARTUP_GATE_RECORDS_MAX) cap = TLAL_HOT_STARTUP_GATE_RECORDS_MAX;
    bytes = st->cfg.startup_gate_bytes_per_record;
    if (bytes == 0ull) bytes = TLAL_HOT_STARTUP_GATE_BYTES_DEFAULT;
    count = tlal_collect_record_indices_near(&st->index, core_group, temporal_key, cap, indices, &exact_hits);
    if (st->stats) {
        st->stats->binary_searches += 1ull;
        st->stats->startup_gate_exact_hits += exact_hits;
        st->stats->startup_gate_expected_hits += count;
        st->stats->progressive_records_touched += count;
    }
    for (i = 0u; i < count && st->budget_left != 0ull; ++i) {
        const TlalHotRecord* r = &st->index.records[indices[i]];
        uint64_t got = tlal_cache_load_record(st, r, bytes);
        if (got != 0ull && st->stats) {
            st->stats->startup_gate_hits += 1ull;
            st->stats->startup_gate_bytes += got;
            st->stats->prepared_hits += 1ull;
            st->stats->prepared_bytes += got;
        }
    }
}

static void tlal_prewarm_startup_gate(TlalHotState* st) {
    uint64_t before;
    if (!st || st->index.record_count == 0u) return;
    if (!st->index.temporal_order || st->index.temporal_order_count == 0u) (void)tlal_runtime_build_temporal_order(&st->index);
    before = st->stats ? st->stats->startup_gate_hits : 0ull;
    tlal_prewarm_latest_categories(st);
    if (st->stats && st->stats->startup_gate_hits > before) { st->stats->cache_bytes = st->index.cache_bytes; return; }
    tlal_startup_gate_core(st, TLALPOWA_HOTDATA_CORE_CONTAMINANT, st->latest_con.temporal_key);
    tlal_startup_gate_core(st, TLALPOWA_HOTDATA_CORE_METEOROLOGY, st->latest_met.temporal_key);
    tlal_startup_gate_core(st, TLALPOWA_HOTDATA_CORE_EPIDEMIOLOGY, st->latest_epi.temporal_key);
    tlal_startup_gate_core(st, TLALPOWA_HOTDATA_CORE_OTHER, st->latest_oth.temporal_key);
    if (st->stats) st->stats->cache_bytes = st->index.cache_bytes;
}

static void tlal_touch_3d_file(TlalHotState* st, const char* path) {
    FILE* f;
    uint64_t size, head, tail;
    if (!st || !path || !st->cfg.enable_3d_touch || st->budget_left == 0ull) return;
    f = fopen(path, "rb");
    if (!f) return;
    size = tlal_file_size_stream(f);
    fclose(f);
    if (size == 0ull) return;
    head = 2ull * 1024ull * 1024ull;
    tail = 2ull * 1024ull * 1024ull;
    tlal_touch_file_span(st, path, 0ull, head);
    if (size > tail) tlal_touch_file_span(st, path, size - tail, tail);
    if (st->stats) st->stats->latest_3d_bytes = size;
}

#ifdef _WIN32
static int tlal_is_dot_dir(const char* s) { return !s || strcmp(s, ".") == 0 || strcmp(s, "..") == 0; }
static void tlal_scan_dir(TlalHotState* st, const char* root, uint32_t depth) {
    char pattern[TLALPOWA_HOT_PATH_MAX];
    intptr_t h;
    struct _finddatai64_t fd;
    if (!st || !root || depth > st->cfg.max_depth) return;
    if (!tlal_join_path(pattern, sizeof(pattern), root, "*")) return;
    h = _findfirsti64(pattern, &fd);
    if (h == -1) return;
    do {
        char path[TLALPOWA_HOT_PATH_MAX];
        if (tlal_is_dot_dir(fd.name)) continue;
        if (!tlal_join_path(path, sizeof(path), root, fd.name)) continue;
        if (fd.attrib & _A_SUBDIR) {
            tlal_scan_dir(st, path, depth + 1u);
        } else {
            if (st->stats) st->stats->files_seen += 1ull;
            if (tlal_has_suffix_ascii(path, ".ixiptlah")) {
                if (st->cfg.max_ixiptlah_files && st->ix_seen_limit >= st->cfg.max_ixiptlah_files) continue;
                ++st->ix_seen_limit;
                if (st->stats) st->stats->ixiptlah_files += 1ull;
                if (!tlal_parse_ixiptlah_directory_mapped(st, path) && st->stats) st->stats->failed_files += 1ull;
            } else if (tlal_has_suffix_ascii(path, ".tlalpowa3d")) {
                tlal_touch_3d_file(st, path);
            } else if (tlal_has_suffix_ascii(path, ".tlalpowa3d.json")) {
                tlal_touch_file_span(st, path, 0ull, 512ull * 1024ull);
            }
        }
    } while (_findnexti64(h, &fd) == 0);
    _findclose(h);
}
#else
static int tlal_is_dot_dir(const char* s) { return !s || strcmp(s, ".") == 0 || strcmp(s, "..") == 0; }
static void tlal_scan_dir(TlalHotState* st, const char* root, uint32_t depth) {
    DIR* d;
    struct dirent* de;
    if (!st || !root || depth > st->cfg.max_depth) return;
    d = opendir(root);
    if (!d) return;
    while ((de = readdir(d)) != NULL) {
        char path[TLALPOWA_HOT_PATH_MAX];
        struct stat sb;
        if (tlal_is_dot_dir(de->d_name)) continue;
        if (!tlal_join_path(path, sizeof(path), root, de->d_name)) continue;
        if (stat(path, &sb) != 0) continue;
        if (S_ISDIR(sb.st_mode)) {
            tlal_scan_dir(st, path, depth + 1u);
        } else if (S_ISREG(sb.st_mode)) {
            if (st->stats) st->stats->files_seen += 1ull;
            if (tlal_has_suffix_ascii(path, ".ixiptlah")) {
                if (st->cfg.max_ixiptlah_files && st->ix_seen_limit >= st->cfg.max_ixiptlah_files) continue;
                ++st->ix_seen_limit;
                if (st->stats) st->stats->ixiptlah_files += 1ull;
                if (!tlal_parse_ixiptlah_directory_mapped(st, path) && st->stats) st->stats->failed_files += 1ull;
            } else if (tlal_has_suffix_ascii(path, ".tlalpowa3d")) {
                tlal_touch_3d_file(st, path);
            } else if (tlal_has_suffix_ascii(path, ".tlalpowa3d.json")) {
                tlal_touch_file_span(st, path, 0ull, 512ull * 1024ull);
            }
        }
    }
    closedir(d);
}
#endif

static void tlal_config_normalize(TlalpowaHotDataConfig* c) {
    if (!c) return;
    if (c->max_total_touch_bytes == 0ull) c->max_total_touch_bytes = TLAL_HOT_DEFAULT_BUDGET;
    if (c->max_total_touch_bytes > TLAL_HOT_MAX_BUDGET) c->max_total_touch_bytes = TLAL_HOT_MAX_BUDGET;
    if (c->max_payload_bytes_per_record == 0u) c->max_payload_bytes_per_record = 2u * 1024u * 1024u;
    if (c->max_payload_bytes_per_record > 32u * 1024u * 1024u) c->max_payload_bytes_per_record = 32u * 1024u * 1024u;
    if (c->max_depth == 0u) c->max_depth = 1u;
    if (c->probe_bytes_per_record > 64u * 1024u) c->probe_bytes_per_record = 64u * 1024u;
    if (c->progressive_neighbor_records > 256u) c->progressive_neighbor_records = 256u;
    if (c->neighbor_bytes_per_record == 0u) c->neighbor_bytes_per_record = 256u * 1024u;
    if (c->neighbor_bytes_per_record > 4u * 1024u * 1024u) c->neighbor_bytes_per_record = 4u * 1024u * 1024u;
    if (c->max_runtime_cache_bytes == 0ull) c->max_runtime_cache_bytes = TLAL_HOT_DEFAULT_CACHE_BYTES;
    if (c->max_runtime_cache_bytes > TLAL_HOT_MAX_CACHE_BYTES) c->max_runtime_cache_bytes = TLAL_HOT_MAX_CACHE_BYTES;
    if (c->runtime_cache_lines == 0u) c->runtime_cache_lines = TLAL_HOT_CACHE_LINES_DEFAULT;
    if (c->runtime_cache_lines > TLAL_HOT_CACHE_LINES_MAX) c->runtime_cache_lines = TLAL_HOT_CACHE_LINES_MAX;
    if (c->startup_gate_records_per_core == 0u) c->startup_gate_records_per_core = TLAL_HOT_STARTUP_GATE_RECORDS_DEFAULT;
    if (c->startup_gate_records_per_core > TLAL_HOT_STARTUP_GATE_RECORDS_MAX) c->startup_gate_records_per_core = TLAL_HOT_STARTUP_GATE_RECORDS_MAX;
    if (c->startup_gate_bytes_per_record == 0u) c->startup_gate_bytes_per_record = TLAL_HOT_STARTUP_GATE_BYTES_DEFAULT;
    if (c->startup_gate_bytes_per_record > TLAL_HOT_STARTUP_GATE_BYTES_MAX) c->startup_gate_bytes_per_record = TLAL_HOT_STARTUP_GATE_BYTES_MAX;
    if (c->startup_gate_category_limit == 0u) c->startup_gate_category_limit = TLAL_HOT_STARTUP_CATEGORY_LIMIT_DEFAULT;
    if (c->startup_gate_category_limit > TLAL_HOT_STARTUP_CATEGORY_LIMIT_MAX) c->startup_gate_category_limit = TLAL_HOT_STARTUP_CATEGORY_LIMIT_MAX;
}

TlalpowaHotDataConfig tlalpowa_hotdata_default_config(void) {
    TlalpowaHotDataConfig c;
    c.max_total_touch_bytes = TLAL_HOT_DEFAULT_BUDGET;
    c.max_depth = 6u;
    c.max_ixiptlah_files = 4096u;
    c.max_payload_bytes_per_record = 2u * 1024u * 1024u;
    c.enable_3d_touch = 0u;
    c.probe_bytes_per_record = 0u;
    c.progressive_neighbor_records = 0u;
    c.neighbor_bytes_per_record = 256u * 1024u;
    c.keep_runtime_index = 1u;
    c.max_runtime_cache_bytes = TLAL_HOT_DEFAULT_CACHE_BYTES;
    c.runtime_cache_lines = TLAL_HOT_CACHE_LINES_DEFAULT;
    c.startup_gate_records_per_core = TLAL_HOT_STARTUP_GATE_RECORDS_DEFAULT;
    c.startup_gate_bytes_per_record = TLAL_HOT_STARTUP_GATE_BYTES_DEFAULT;
    c.startup_gate_category_limit = TLAL_HOT_STARTUP_CATEGORY_LIMIT_DEFAULT;
    return c;
}

int tlalpowa_hotdata_prewarm_root(const char* root_utf8,
                                  const TlalpowaHotDataConfig* config,
                                  TlalpowaHotDataStats* stats) {
    TlalHotState st;
    int ok = 1;
    if (stats) memset(stats, 0, sizeof(*stats));
    if (!root_utf8 || !*root_utf8) return 0;
    memset(&st, 0, sizeof(st));
    st.cfg = config ? *config : tlalpowa_hotdata_default_config();
    tlal_config_normalize(&st.cfg);
    st.stats = stats;
    st.budget_left = st.cfg.max_total_touch_bytes;
    st.index.cache_limit_bytes = st.cfg.max_runtime_cache_bytes;
    st.index.cache_line_count = st.cfg.runtime_cache_lines;
    st.index.mapped_file_limit_bytes = st.cfg.max_runtime_cache_bytes >= TLAL_HOT_DEFAULT_RETAINED_MAP_BYTES / 4ull ?
        st.cfg.max_runtime_cache_bytes * 4ull : TLAL_HOT_DEFAULT_RETAINED_MAP_BYTES;
    if (st.index.mapped_file_limit_bytes > TLAL_HOT_MAX_RETAINED_MAP_BYTES)
        st.index.mapped_file_limit_bytes = TLAL_HOT_MAX_RETAINED_MAP_BYTES;
    st.touch_buffer = (unsigned char*)malloc(TLAL_TOUCH_BLOCK_BYTES);
    if (!st.touch_buffer) return 0;
    tlal_scan_dir(&st, root_utf8, 0u);
    (void)tlal_runtime_build_temporal_order(&st.index);
    /*
    REGLA DE BIENVENIDA DE PRIMER PLANO:
    se ignora la fecha civil actual y se toman los ultimos registros IXIPTLAH
    disponibles por categoria fisica. La pantalla puede permanecer mas tiempo
    para asegurar ese primer plano real, pero NO espera vecinos cronologicos;
    ellos se cargan despues, en segundo plano, del mas cercano al mas lejano.
    */
    tlal_prewarm_latest_candidates(&st);
    tlal_prewarm_startup_gate(&st);
    if (stats) stats->retained_mapped_file_bytes = st.index.mapped_file_bytes;
    if (st.cfg.keep_runtime_index) {
        if (stats) stats->cache_bytes = st.index.cache_bytes;
        tlal_hot_lock();
        tlal_runtime_index_free(&g_tlal_hot_index);
        g_tlal_hot_index = st.index;
        memset(&st.index, 0, sizeof(st.index));
        tlal_hot_unlock();
    } else {
        tlal_runtime_index_free(&st.index);
        if (stats) stats->cache_bytes = 0ull;
    }
    free(st.touch_buffer);
    if (stats && stats->failed_files > 0ull && stats->ixiptlah_directories == 0ull) ok = 0;
    return ok;
}

int tlalpowa_hotdata_prefetch_temporal(uint32_t core_group,
                                       uint64_t temporal_key,
                                       uint32_t neighbor_records,
                                       uint32_t bytes_per_record,
                                       TlalpowaHotDataStats* stats) {
    TlalHotState st;
    uint32_t want;
    uint64_t bytes;
    int ok;
    if (stats) memset(stats, 0, sizeof(*stats));
    if (temporal_key == 0ull) return 0;
    tlal_hot_lock();
    if (!g_tlal_hot_index.records || g_tlal_hot_index.record_count == 0u) { tlal_hot_unlock(); return 0; }
    memset(&st, 0, sizeof(st));
    st.cfg = tlalpowa_hotdata_default_config();
    st.cfg.max_total_touch_bytes = 32ull * 1024ull * 1024ull;
    st.cfg.neighbor_bytes_per_record = bytes_per_record ? bytes_per_record : 256u * 1024u;
    st.cfg.progressive_neighbor_records = neighbor_records ? neighbor_records : 12u;
    st.cfg.max_runtime_cache_bytes = g_tlal_hot_index.cache_limit_bytes ? g_tlal_hot_index.cache_limit_bytes : TLAL_HOT_DEFAULT_CACHE_BYTES;
    st.cfg.runtime_cache_lines = g_tlal_hot_index.cache_line_count ? g_tlal_hot_index.cache_line_count : TLAL_HOT_CACHE_LINES_DEFAULT;
    tlal_config_normalize(&st.cfg);
    st.stats = stats;
    st.budget_left = st.cfg.max_total_touch_bytes;
    st.index = g_tlal_hot_index;
    if (!st.index.temporal_order || st.index.temporal_order_count == 0u) (void)tlal_runtime_build_temporal_order(&st.index);
    st.touch_buffer = (unsigned char*)malloc(TLAL_TOUCH_BLOCK_BYTES);
    if (!st.touch_buffer) { tlal_hot_unlock(); return 0; }
    want = st.cfg.progressive_neighbor_records ? st.cfg.progressive_neighbor_records : 1u;
    bytes = st.cfg.neighbor_bytes_per_record ? (uint64_t)st.cfg.neighbor_bytes_per_record : (256ull * 1024ull);
    tlal_prewarm_temporal_near(&st, core_group, temporal_key, want, bytes);
    if (stats) stats->cache_bytes = st.index.cache_bytes;
    g_tlal_hot_index = st.index;
    memset(&st.index, 0, sizeof(st.index));
    free(st.touch_buffer);
    ok = stats ? (stats->progressive_records_touched > 0ull) : 1;
    tlal_hot_unlock();
    return ok;
}

int tlalpowa_hotdata_find_nearest(uint32_t core_group,
                                  uint64_t temporal_key,
                                  TlalpowaHotDataHit* hit) {
    uint32_t record_index;
    int ok = 0;
    if (hit) memset(hit, 0, sizeof(*hit));
    if (!hit || temporal_key == 0ull) return 0;
    tlal_hot_lock();
    if (g_tlal_hot_index.records && g_tlal_hot_index.record_count != 0u &&
        g_tlal_hot_index.temporal_order && g_tlal_hot_index.temporal_order_count != 0u &&
        tlal_runtime_find_nearest_record_index(&g_tlal_hot_index, core_group, temporal_key, &record_index)) {
        tlal_hit_from_record(&g_tlal_hot_index, &g_tlal_hot_index.records[record_index], hit);
        ok = 1;
    }
    tlal_hot_unlock();
    return ok;
}

uint32_t tlalpowa_hotdata_collect_window(uint32_t core_group,
                                         uint64_t temporal_key,
                                         uint32_t max_hits,
                                         TlalpowaHotDataHit* hits) {
    uint32_t stack_indices[256];
    uint32_t local_indices_small[32];
    uint32_t* indices;
    uint32_t count = 0u, i, cap;
    uint64_t exact_hits = 0ull;
    if (!hits || max_hits == 0u || temporal_key == 0ull) return 0u;
    cap = max_hits;
    if (cap > 256u) cap = 256u;
    memset(hits, 0, (size_t)cap * sizeof(*hits));
    tlal_hot_lock();
    if (!g_tlal_hot_index.records || g_tlal_hot_index.record_count == 0u) { tlal_hot_unlock(); return 0u; }
    indices = cap <= 32u ? local_indices_small : stack_indices;
    count = tlal_collect_record_indices_near(&g_tlal_hot_index, core_group, temporal_key, cap, indices, &exact_hits);
    for (i = 0u; i < count; ++i) tlal_hit_from_record(&g_tlal_hot_index, &g_tlal_hot_index.records[indices[i]], &hits[i]);
    (void)exact_hits;
    tlal_hot_unlock();
    return count;
}

uint64_t tlalpowa_hotdata_read_hit(const TlalpowaHotDataHit* hit,
                                   void* out_buffer,
                                   uint64_t out_capacity,
                                   uint64_t payload_relative_offset) {
    uint64_t remain, absolute_offset, got = 0ull;
    uint32_t i, limit;
    if (!hit || !out_buffer || out_capacity == 0ull || !hit->path[0]) return 0ull;
    if (payload_relative_offset >= hit->stored_size) return 0ull;
    if (hit->payload_offset > UINT64_MAX - payload_relative_offset) return 0ull;
    remain = hit->stored_size - payload_relative_offset;
    if (remain > out_capacity) remain = out_capacity;
    if (remain > (uint64_t)SIZE_MAX) remain = (uint64_t)SIZE_MAX;
    absolute_offset = hit->payload_offset + payload_relative_offset;
    tlal_hot_lock();
    limit = tlal_runtime_cache_lines(&g_tlal_hot_index);
    for (i = 0u; i < limit; ++i) {
        TlalHotCacheLine* ln = &g_tlal_hot_index.cache[i];
        if (ln->data && ln->file_index == hit->file_index && ln->payload_offset == hit->payload_offset &&
            ln->temporal_key == hit->temporal_key && ln->layer_hash == hit->layer_hash &&
            payload_relative_offset < ln->bytes) {
            uint64_t cached = ln->bytes - payload_relative_offset;
            if (cached > remain) cached = remain;
            memcpy(out_buffer, ln->data + payload_relative_offset, (size_t)cached);
            ln->tick = ++g_tlal_hot_index.cache_tick;
            tlal_hot_unlock();
            return cached;
        }
    }
    if (hit->file_index < g_tlal_hot_index.file_count) {
        got = tlal_read_hot_file_span(&g_tlal_hot_index.files[hit->file_index], absolute_offset, out_buffer, remain);
        if (got != 0ull) { tlal_hot_unlock(); return got; }
    }
    tlal_hot_unlock();
    return tlal_read_path_span(hit->path, absolute_offset, out_buffer, remain);
}

uint32_t tlalpowa_hotdata_prepare_active_temporal_view(uint32_t core_group,
                                                       uint64_t temporal_key,
                                                       uint32_t active_hits,
                                                       uint32_t active_bytes_per_hit,
                                                       uint32_t neighbor_hits,
                                                       uint32_t neighbor_bytes_per_hit,
                                                       TlalpowaHotDataHit* hits,
                                                       TlalpowaHotDataStats* stats) {
    TlalHotState st;
    uint32_t indices[256];
    uint32_t cap, count = 0u, i;
    uint64_t exact_hits = 0ull;
    uint64_t active_bytes, neighbor_bytes, max_bytes;
    if (stats) memset(stats, 0, sizeof(*stats));
    if (hits) memset(hits, 0, (size_t)((active_hits + neighbor_hits) > 256u ? 256u : (active_hits + neighbor_hits)) * sizeof(*hits));
    if (temporal_key == 0ull) return 0u;
    if (active_hits == 0u) active_hits = 1u;
    cap = active_hits + neighbor_hits;
    if (cap == 0u) return 0u;
    if (cap > 256u) cap = 256u;
    if (active_hits > cap) active_hits = cap;
    active_bytes = active_bytes_per_hit ? (uint64_t)active_bytes_per_hit : 96ull * 1024ull;
    neighbor_bytes = neighbor_bytes_per_hit ? (uint64_t)neighbor_bytes_per_hit : 64ull * 1024ull;
    if (active_bytes > 4ull * 1024ull * 1024ull) active_bytes = 4ull * 1024ull * 1024ull;
    if (neighbor_bytes > 4ull * 1024ull * 1024ull) neighbor_bytes = 4ull * 1024ull * 1024ull;
    tlal_hot_lock();
    if (!g_tlal_hot_index.records || g_tlal_hot_index.record_count == 0u) { tlal_hot_unlock(); return 0u; }
    count = tlal_collect_record_indices_near(&g_tlal_hot_index, core_group, temporal_key, cap, indices, &exact_hits);
    if (count == 0u) { tlal_hot_unlock(); return 0u; }
    memset(&st, 0, sizeof(st));
    st.cfg = tlalpowa_hotdata_default_config();
    max_bytes = active_bytes > neighbor_bytes ? active_bytes : neighbor_bytes;
    st.cfg.max_total_touch_bytes = active_bytes * (uint64_t)(active_hits < count ? active_hits : count);
    if (count > active_hits) st.cfg.max_total_touch_bytes += neighbor_bytes * (uint64_t)(count - active_hits);
    if (st.cfg.max_total_touch_bytes > 64ull * 1024ull * 1024ull) st.cfg.max_total_touch_bytes = 64ull * 1024ull * 1024ull;
    st.cfg.max_payload_bytes_per_record = (uint32_t)(max_bytes > 32ull * 1024ull * 1024ull ? 32ull * 1024ull * 1024ull : max_bytes);
    st.cfg.max_runtime_cache_bytes = g_tlal_hot_index.cache_limit_bytes ? g_tlal_hot_index.cache_limit_bytes : TLAL_HOT_DEFAULT_CACHE_BYTES;
    st.cfg.runtime_cache_lines = g_tlal_hot_index.cache_line_count ? g_tlal_hot_index.cache_line_count : TLAL_HOT_CACHE_LINES_DEFAULT;
    tlal_config_normalize(&st.cfg);
    st.stats = stats;
    st.budget_left = st.cfg.max_total_touch_bytes;
    st.index = g_tlal_hot_index;
    for (i = 0u; i < count; ++i) {
        const TlalHotRecord* r = &st.index.records[indices[i]];
        const uint64_t bytes = i < active_hits ? active_bytes : neighbor_bytes;
        uint64_t got = tlal_cache_load_record(&st, r, bytes);
        if (hits) tlal_hit_from_record(&st.index, r, &hits[i]);
        if (stats && got != 0ull) {
            stats->prepared_hits += 1ull;
            stats->prepared_bytes += got;
        }
    }
    if (stats) {
        stats->exact_window_hits = exact_hits;
        stats->binary_searches += 1ull;
        stats->cache_bytes = st.index.cache_bytes;
        stats->progressive_records_touched += count;
    }
    g_tlal_hot_index = st.index;
    memset(&st.index, 0, sizeof(st.index));
    tlal_hot_unlock();
    return count;
}

uint32_t tlalpowa_hotdata_prepare_temporal_view(uint32_t core_group,
                                                uint64_t temporal_key,
                                                uint32_t max_hits,
                                                uint32_t bytes_per_hit,
                                                TlalpowaHotDataHit* hits,
                                                TlalpowaHotDataStats* stats) {
    return tlalpowa_hotdata_prepare_active_temporal_view(core_group,
                                                        temporal_key,
                                                        max_hits,
                                                        bytes_per_hit,
                                                        0u,
                                                        bytes_per_hit,
                                                        hits,
                                                        stats);
}

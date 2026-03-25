/**
 * Caravel Test Framework
 *
 * Header-only C test framework wrapping QuasarSVM for local Solana program testing.
 * Compile with host compiler (not SBF) and link against libquasar_svm.
 *
 * Usage:
 *   #include <caravel/test.h>
 *
 *   CVL_TEST(test_my_feature) {
 *       QuasarSvm *svm = quasar_svm_new();
 *       // ... setup, send transactions, assert results ...
 *       quasar_svm_free(svm);
 *       return 0;
 *   }
 *
 *   CVL_TEST_MAIN(test_my_feature)
 */
#ifndef CVL_TEST_H
#define CVL_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ===== QuasarSVM FFI declarations ===== */

typedef struct QuasarSvm QuasarSvm;

extern QuasarSvm *quasar_svm_new(void);
extern void       quasar_svm_free(QuasarSvm *svm);
extern const char *quasar_last_error(void);

extern int32_t quasar_svm_add_program(QuasarSvm *svm,
                                      const uint8_t (*program_id)[32],
                                      const uint8_t *elf_data,
                                      uint64_t elf_len,
                                      uint8_t loader_version);

extern int32_t quasar_svm_process_transaction(QuasarSvm *svm,
                                              const uint8_t *instructions,
                                              uint64_t instructions_len,
                                              const uint8_t *accounts,
                                              uint64_t accounts_len,
                                              uint8_t **result_out,
                                              uint64_t *result_len_out);

extern void quasar_result_free(uint8_t *result, uint64_t result_len);

extern int32_t quasar_svm_set_clock(QuasarSvm *svm, uint64_t slot,
                                    int64_t epoch_start_timestamp,
                                    uint64_t epoch,
                                    uint64_t leader_schedule_epoch,
                                    int64_t unix_timestamp);
extern int32_t quasar_svm_warp_to_slot(QuasarSvm *svm, uint64_t slot);
extern int32_t quasar_svm_set_rent(QuasarSvm *svm, uint64_t lamports_per_byte_year);
extern int32_t quasar_svm_set_compute_budget(QuasarSvm *svm, uint64_t max_units);

/* ===== Types ===== */

typedef struct {
    uint8_t  pubkey[32];
    uint8_t  owner[32];
    uint64_t lamports;
    uint8_t *data;
    uint32_t data_len;
    bool     executable;
} CvlTestAccount;

typedef struct {
    uint8_t pubkey[32];
    bool    is_signer;
    bool    is_writable;
} CvlTestMeta;

typedef struct {
    uint8_t      program_id[32];
    uint8_t     *data;
    uint32_t     data_len;
    CvlTestMeta *metas;
    uint32_t     num_metas;
} CvlTestInstruction;

typedef struct {
    int32_t  status;
    uint64_t compute_units;
    uint8_t *return_data;      /* points into _raw, valid until result_free */
    uint32_t return_data_len;
    uint8_t *_raw;
    uint64_t _raw_len;
} CvlTestResult;

/* ===== Constants ===== */

static const uint8_t CVL_TEST_SYSTEM_PROGRAM[32] = {0};

/* ===== Internal: little-endian helpers ===== */

static inline void _cvl_w32(uint8_t *b, uint32_t v) {
    b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8);
    b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24);
}

static inline void _cvl_w64(uint8_t *b, uint64_t v) {
    for (int i = 0; i < 8; i++) b[i] = (uint8_t)(v >> (i * 8));
}

static inline uint32_t _cvl_r32(const uint8_t *b) {
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static inline int32_t _cvl_ri32(const uint8_t *b) {
    return (int32_t)_cvl_r32(b);
}

static inline uint64_t _cvl_r64(const uint8_t *b) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)b[i] << (i * 8);
    return v;
}

/* Public read helpers for interpreting account data */
#define cvl_test_read_u64(buf) _cvl_r64(buf)
#define cvl_test_read_u32(buf) _cvl_r32(buf)

/* ===== Wire format serialization ===== */

static uint8_t *_cvl_serialize_ixs(const CvlTestInstruction *ixs, uint32_t n,
                                   uint64_t *out_len) {
    uint64_t size = 4;
    for (uint32_t i = 0; i < n; i++)
        size += 32 + 4 + ixs[i].data_len + 4 + (uint64_t)ixs[i].num_metas * 34;

    uint8_t *buf = (uint8_t *)malloc(size);
    if (!buf) return NULL;
    uint8_t *p = buf;

    _cvl_w32(p, n); p += 4;
    for (uint32_t i = 0; i < n; i++) {
        memcpy(p, ixs[i].program_id, 32); p += 32;
        _cvl_w32(p, ixs[i].data_len); p += 4;
        if (ixs[i].data_len > 0) {
            memcpy(p, ixs[i].data, ixs[i].data_len);
            p += ixs[i].data_len;
        }
        _cvl_w32(p, ixs[i].num_metas); p += 4;
        for (uint32_t j = 0; j < ixs[i].num_metas; j++) {
            memcpy(p, ixs[i].metas[j].pubkey, 32); p += 32;
            *p++ = ixs[i].metas[j].is_signer ? 1 : 0;
            *p++ = ixs[i].metas[j].is_writable ? 1 : 0;
        }
    }

    *out_len = size;
    return buf;
}

static uint8_t *_cvl_serialize_accts(const CvlTestAccount *accts, uint32_t n,
                                     uint64_t *out_len) {
    uint64_t size = 4;
    for (uint32_t i = 0; i < n; i++)
        size += 32 + 32 + 8 + 4 + accts[i].data_len + 1;

    uint8_t *buf = (uint8_t *)malloc(size);
    if (!buf) return NULL;
    uint8_t *p = buf;

    _cvl_w32(p, n); p += 4;
    for (uint32_t i = 0; i < n; i++) {
        memcpy(p, accts[i].pubkey, 32); p += 32;
        memcpy(p, accts[i].owner, 32); p += 32;
        _cvl_w64(p, accts[i].lamports); p += 8;
        _cvl_w32(p, accts[i].data_len); p += 4;
        if (accts[i].data_len > 0) {
            memcpy(p, accts[i].data, accts[i].data_len);
            p += accts[i].data_len;
        }
        *p++ = accts[i].executable ? 1 : 0;
    }

    *out_len = size;
    return buf;
}

/* ===== Result deserialization ===== */

/*
 * Result wire format:
 *   [4]  status (i32)
 *   [8]  compute_units (u64)
 *   [8]  execution_time_us (u64)
 *   [4]  return_data_len (u32)  +  [N] return_data
 *   [4]  num_accounts (u32)     +  accounts...
 *   [4]  num_logs (u32)         +  logs...
 *   [4]  error_message_len (u32) + error_message
 *   ... (balances, traces)
 */
static void _cvl_parse_result(uint8_t *raw, uint64_t raw_len, CvlTestResult *r) {
    r->_raw = raw;
    r->_raw_len = raw_len;

    if (raw_len < 24) {
        r->status = -1;
        r->compute_units = 0;
        r->return_data = NULL;
        r->return_data_len = 0;
        return;
    }

    r->status = _cvl_ri32(raw);
    r->compute_units = _cvl_r64(raw + 4);
    /* skip execution_time_us at offset 12 */
    r->return_data_len = _cvl_r32(raw + 20);
    r->return_data = (r->return_data_len > 0) ? (raw + 24) : NULL;
}

/* ===== Public API ===== */

/** Load an ELF binary from a file path. Caller must free() the returned buffer. */
static uint8_t *cvl_test_load_elf(const char *path, uint64_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "err: cannot open %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = (uint8_t *)malloc(size);
    if (buf) {
        if (fread(buf, 1, size, f) != (size_t)size) {
            free(buf);
            fclose(f);
            return NULL;
        }
    }
    fclose(f);
    if (len) *len = (uint64_t)size;
    return buf;
}

/** Generate a random 32-byte key. */
static void cvl_test_keygen(uint8_t key[32]) {
#if defined(__APPLE__) || defined(__FreeBSD__)
    arc4random_buf(key, 32);
#else
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t n = fread(key, 1, 32, f);
        (void)n;
        fclose(f);
    }
#endif
}

/**
 * Send a single-instruction transaction.
 *
 * Serializes the instruction and accounts into QuasarSVM wire format,
 * processes the transaction, and parses the result. The result's status
 * field reflects the program return value (0 = success).
 *
 * Pass accounts=NULL, num_accounts=0 for transactions where the SVM
 * already has the required account state from prior transactions.
 */
static int cvl_test_send(QuasarSvm *svm, CvlTestInstruction *ix,
                         CvlTestAccount *accounts, uint32_t num_accounts,
                         CvlTestResult *result) {
    uint64_t ix_len, acct_len;
    uint8_t *ix_buf = _cvl_serialize_ixs(ix, 1, &ix_len);
    uint8_t *acct_buf = _cvl_serialize_accts(accounts, num_accounts, &acct_len);

    if (!ix_buf || !acct_buf) {
        free(ix_buf);
        free(acct_buf);
        memset(result, 0, sizeof(*result));
        result->status = -1;
        return -1;
    }

    uint8_t *raw = NULL;
    uint64_t raw_len = 0;
    int32_t rc = quasar_svm_process_transaction(svm,
        ix_buf, ix_len, acct_buf, acct_len, &raw, &raw_len);

    free(ix_buf);
    free(acct_buf);

    memset(result, 0, sizeof(*result));

    if (raw && raw_len >= 4) {
        _cvl_parse_result(raw, raw_len, result);
    } else {
        result->status = rc ? rc : -1;
        const char *err = quasar_last_error();
        if (err) fprintf(stderr, "  quasar: %s\n", err);
    }

    return 0;
}

/** Free the raw result buffer. Call after you're done reading result data. */
static void cvl_test_result_free(CvlTestResult *result) {
    if (result->_raw) {
        quasar_result_free(result->_raw, result->_raw_len);
        result->_raw = NULL;
        result->return_data = NULL;
    }
}

/**
 * Find a modified account in the transaction result by pubkey.
 *
 * Returns a pointer to the account's data (within the result buffer),
 * or NULL if not found. Fills lamports and data_len if non-NULL.
 * The returned pointer is valid until cvl_test_result_free() is called.
 */
static const uint8_t *cvl_test_result_account(const CvlTestResult *result,
                                              const uint8_t pubkey[32],
                                              uint64_t *lamports,
                                              uint32_t *data_len) {
    if (!result->_raw || result->_raw_len < 24) return NULL;

    const uint8_t *p = result->_raw + 20;
    uint32_t rd_len = _cvl_r32(p); p += 4 + rd_len;

    /* modified accounts section */
    if (p + 4 > result->_raw + result->_raw_len) return NULL;
    uint32_t n = _cvl_r32(p); p += 4;

    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *pk = p; p += 32;
        p += 32; /* owner */
        uint64_t lam = _cvl_r64(p); p += 8;
        uint32_t dl = _cvl_r32(p); p += 4;
        const uint8_t *d = p; p += dl;
        p += 1; /* executable */

        if (memcmp(pk, pubkey, 32) == 0) {
            if (lamports) *lamports = lam;
            if (data_len) *data_len = dl;
            return d;
        }
    }
    return NULL;
}

/** Print transaction logs from the result to stderr. */
static void cvl_test_result_print_logs(const CvlTestResult *result) {
    if (!result->_raw || result->_raw_len < 24) return;

    const uint8_t *p = result->_raw + 20;
    const uint8_t *end = result->_raw + result->_raw_len;

    /* skip return_data */
    if (p + 4 > end) return;
    uint32_t rd_len = _cvl_r32(p); p += 4 + rd_len;

    /* skip modified accounts */
    if (p + 4 > end) return;
    uint32_t na = _cvl_r32(p); p += 4;
    for (uint32_t i = 0; i < na; i++) {
        p += 32 + 32 + 8; /* pubkey + owner + lamports */
        if (p + 4 > end) return;
        uint32_t dl = _cvl_r32(p); p += 4 + dl + 1;
    }

    /* logs section */
    if (p + 4 > end) return;
    uint32_t nl = _cvl_r32(p); p += 4;
    for (uint32_t i = 0; i < nl; i++) {
        if (p + 4 > end) return;
        uint32_t ll = _cvl_r32(p); p += 4;
        if (p + ll > end) return;
        fprintf(stderr, "    %.*s\n", (int)ll, (const char *)p);
        p += ll;
    }
}

/* ===== Test runner macros ===== */

/** Declare a test function. Returns 0 on success, non-zero on failure. */
#define CVL_TEST(name) static int name(void)

/** Assert a condition, printing file/line on failure. */
#define CVL_ASSERT(cond) \
    do { if (!(cond)) { \
        fprintf(stderr, "    ASSERT FAILED: %s\n      at %s:%d\n", \
                #cond, __FILE__, __LINE__); \
        return 1; \
    }} while(0)

/** Assert two values are equal. */
#define CVL_ASSERT_EQ(a, b) \
    do { if ((a) != (b)) { \
        fprintf(stderr, "    ASSERT_EQ FAILED: %s != %s\n      at %s:%d\n", \
                #a, #b, __FILE__, __LINE__); \
        return 1; \
    }} while(0)

/** Assert transaction succeeded (status == 0). Prints logs on failure. */
#define CVL_ASSERT_OK(result) \
    do { if ((result).status != 0) { \
        fprintf(stderr, "    ASSERT_OK FAILED: status = %d\n      at %s:%d\n", \
                (result).status, __FILE__, __LINE__); \
        cvl_test_result_print_logs(&(result)); \
        return 1; \
    }} while(0)

/** Assert transaction failed (status != 0). */
#define CVL_ASSERT_FAIL(result) \
    do { if ((result).status == 0) { \
        fprintf(stderr, "    ASSERT_FAIL: expected failure, got success\n      at %s:%d\n", \
                __FILE__, __LINE__); \
        return 1; \
    }} while(0)

/* --- FOR_EACH machinery (supports up to 16 test functions) --- */

#define _CVL_CAT_(a, b) a##b
#define _CVL_CAT(a, b)  _CVL_CAT_(a, b)

#define _CVL_NARGS_(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N,...) N
#define _CVL_NARGS(...) _CVL_NARGS_(__VA_ARGS__,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1)

#define _CVL_FE_1(m, x)      m(x)
#define _CVL_FE_2(m, x, ...) m(x) _CVL_FE_1(m, __VA_ARGS__)
#define _CVL_FE_3(m, x, ...) m(x) _CVL_FE_2(m, __VA_ARGS__)
#define _CVL_FE_4(m, x, ...) m(x) _CVL_FE_3(m, __VA_ARGS__)
#define _CVL_FE_5(m, x, ...) m(x) _CVL_FE_4(m, __VA_ARGS__)
#define _CVL_FE_6(m, x, ...) m(x) _CVL_FE_5(m, __VA_ARGS__)
#define _CVL_FE_7(m, x, ...) m(x) _CVL_FE_6(m, __VA_ARGS__)
#define _CVL_FE_8(m, x, ...) m(x) _CVL_FE_7(m, __VA_ARGS__)
#define _CVL_FE_9(m, x, ...) m(x) _CVL_FE_8(m, __VA_ARGS__)
#define _CVL_FE_10(m, x, ...) m(x) _CVL_FE_9(m, __VA_ARGS__)
#define _CVL_FE_11(m, x, ...) m(x) _CVL_FE_10(m, __VA_ARGS__)
#define _CVL_FE_12(m, x, ...) m(x) _CVL_FE_11(m, __VA_ARGS__)
#define _CVL_FE_13(m, x, ...) m(x) _CVL_FE_12(m, __VA_ARGS__)
#define _CVL_FE_14(m, x, ...) m(x) _CVL_FE_13(m, __VA_ARGS__)
#define _CVL_FE_15(m, x, ...) m(x) _CVL_FE_14(m, __VA_ARGS__)
#define _CVL_FE_16(m, x, ...) m(x) _CVL_FE_15(m, __VA_ARGS__)

#define _CVL_FOR_EACH(m, ...) \
    _CVL_CAT(_CVL_FE_, _CVL_NARGS(__VA_ARGS__))(m, __VA_ARGS__)

#define _CVL_RUN_TEST(fn) \
    do { \
        _cvl_total++; \
        printf("  %-50s", #fn); \
        fflush(stdout); \
        if (fn() == 0) { _cvl_passed++; printf("pass\n"); } \
        else { printf("FAIL\n"); } \
    } while(0);

/**
 * Generate main() that runs listed test functions and prints a summary.
 * Returns 0 if all tests pass, 1 otherwise.
 *
 * Usage: CVL_TEST_MAIN(test_a, test_b, test_c)
 */
#define CVL_TEST_MAIN(...) \
    int main(void) { \
        int _cvl_total = 0, _cvl_passed = 0; \
        printf("\n"); \
        _CVL_FOR_EACH(_CVL_RUN_TEST, __VA_ARGS__) \
        printf("\n  %d/%d passed\n\n", _cvl_passed, _cvl_total); \
        return (_cvl_passed == _cvl_total) ? 0 : 1; \
    }

#endif /* CVL_TEST_H */

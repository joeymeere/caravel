/**
 * Builds a program input buffer in the runtime's aligned (loader v2/v3)
 * serialization format, so entrypoints can be exercised on the host.
 *
 *   u64 num_accounts
 *   per account:
 *     non-dup: u8 0xFF, u8 is_signer, u8 is_writable, u8 executable,
 *              u32 original_data_len, [32] key, [32] owner, u64 lamports,
 *              u64 data_len, data, 10240 bytes realloc space, pad to 8,
 *              u64 rent_epoch
 *     dup:     u8 index_of_original, 7 bytes padding
 *   u64 instruction_data_len, instruction data, [32] program_id
 */
#ifndef CVL_INPUT_H
#define CVL_INPUT_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_REALLOC_SPACE 10240

typedef struct {
    uint8_t key[32];
    uint8_t owner[32];
    uint64_t lamports;
    uint64_t data_len;
    const uint8_t *data; /* may be NULL (zero-filled) */
    uint64_t rent_epoch;
    uint8_t is_signer;
    uint8_t is_writable;
    uint8_t executable;
    int dup_of; /* -1, or index of an earlier account */
} InputAccount;

typedef struct {
    uint8_t *buf;     /* 16-byte aligned, free with input_free() */
    uint8_t *ix_data; /* what the runtime passes in r2 (SIMD-0321) */
    uint64_t account_offsets[64]; /* offset of each account's dup marker */
    uint64_t data_offsets[64];    /* offset of each account's data */
} Input;

static inline void input_put_u64(uint8_t **p, uint64_t v) {
    memcpy(*p, &v, 8);
    *p += 8;
}

static inline Input input_build(
    const InputAccount *accounts,
    uint64_t n,
    const uint8_t *ix_data,
    uint64_t ix_len,
    const uint8_t program_id[32]
) {
    uint64_t size = 8 + 8 + ix_len + 32;
    for (uint64_t i = 0; i < n; i++)
        size += accounts[i].dup_of >= 0
                    ? 8
                    : 8 + 32 + 32 + 8 + 8 + accounts[i].data_len +
                          INPUT_REALLOC_SPACE + 8 + 8;

    Input in;
    memset(&in, 0, sizeof(in));
    in.buf = (uint8_t *)aligned_alloc(16, (size + 15) & ~(uint64_t)15);
    memset(in.buf, 0, (size + 15) & ~(uint64_t)15);

    uint8_t *p = in.buf;
    input_put_u64(&p, n);

    for (uint64_t i = 0; i < n; i++) {
        const InputAccount *a = &accounts[i];
        in.account_offsets[i] = (uint64_t)(p - in.buf);

        if (a->dup_of >= 0) {
            *p = (uint8_t)a->dup_of;
            p += 8;
            continue;
        }

        *p++              = 0xFF;
        *p++              = a->is_signer;
        *p++              = a->is_writable;
        *p++              = a->executable;
        uint32_t orig_len = (uint32_t)a->data_len;
        memcpy(p, &orig_len, 4);
        p += 4;
        memcpy(p, a->key, 32);
        p += 32;
        memcpy(p, a->owner, 32);
        p += 32;
        input_put_u64(&p, a->lamports);
        input_put_u64(&p, a->data_len);

        in.data_offsets[i] = (uint64_t)(p - in.buf);
        if (a->data) memcpy(p, a->data, a->data_len);
        p += a->data_len + INPUT_REALLOC_SPACE;
        p += (8 - ((uint64_t)(p - in.buf) & 7)) & 7;

        input_put_u64(&p, a->rent_epoch);
    }

    input_put_u64(&p, ix_len);
    in.ix_data = p;
    if (ix_len) memcpy(p, ix_data, ix_len);
    p += ix_len;
    memcpy(p, program_id, 32);

    return in;
}

static inline void input_free(Input *in) {
    free(in->buf);
    in->buf = NULL;
}

/* an account with key[0..7] = id, everything else zero */
static inline InputAccount
input_account(uint64_t id, uint8_t signer, uint8_t writable, uint8_t exec) {
    InputAccount a;
    memset(&a, 0, sizeof(a));
    memcpy(a.key, &id, 8);
    a.is_signer   = signer;
    a.is_writable = writable;
    a.executable  = exec;
    a.dup_of      = -1;
    return a;
}

static inline InputAccount input_dup(int of) {
    InputAccount a;
    memset(&a, 0, sizeof(a));
    a.dup_of = of;
    return a;
}

#endif /* CVL_INPUT_H */

#include "stubs.h"

#include <stdio.h>
#include <string.h>

StubCpi stub_cpi;
char stub_last_log[256];
int stub_log_calls;
Rent stub_rent;
Clock stub_clock;

void stub_reset(void) {
    memset(&stub_cpi, 0, sizeof(stub_cpi));
    memset(stub_last_log, 0, sizeof(stub_last_log));
    stub_log_calls = 0;
}

void sol_log_(const char *message, uint64_t len) {
    size_t n =
        len < sizeof(stub_last_log) - 1 ? len : sizeof(stub_last_log) - 1;
    memcpy(stub_last_log, message, n);
    stub_last_log[n] = '\0';
    stub_log_calls++;
}

void sol_log_64_(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e) {
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    stub_log_calls++;
}

void sol_log_pubkey(const Pubkey *pubkey) {
    (void)pubkey;
    stub_log_calls++;
}

void sol_log_compute_units_(void) {
    stub_log_calls++;
}

void *sol_memcpy_(void *dest, const void *src, uint64_t n) {
    return memcpy(dest, src, n);
}

void *sol_memmove_(void *dest, const void *src, uint64_t n) {
    return memmove(dest, src, n);
}

void *sol_memset_(void *dest, uint8_t val, uint64_t n) {
    return memset(dest, val, n);
}

int sol_memcmp_(const void *a, const void *b, uint64_t n, int *result) {
    *result = memcmp(a, b, n);
    return 0;
}

uint64_t sol_get_rent_sysvar(void *rent) {
    memcpy(rent, &stub_rent, sizeof(stub_rent));
    return SUCCESS;
}

uint64_t sol_get_clock_sysvar(void *clock) {
    memcpy(clock, &stub_clock, sizeof(stub_clock));
    return SUCCESS;
}

uint64_t sol_invoke_signed_c(
    const Instruction *ix,
    const AccountInfo *accounts,
    int accounts_len,
    const SignerSeeds *signers,
    int signers_len
) {
    (void)accounts;
    (void)accounts_len;

    stub_cpi.calls++;
    stub_cpi.program_id = *ix->program_id;

    stub_cpi.metas_len = ix->accounts_len;
    for (uint64_t i = 0; i < ix->accounts_len && i < STUB_MAX_METAS; i++) {
        stub_cpi.meta_keys[i]     = *ix->accounts[i].pubkey;
        stub_cpi.meta_writable[i] = ix->accounts[i].is_writable;
        stub_cpi.meta_signer[i]   = ix->accounts[i].is_signer;
    }

    stub_cpi.data_len = ix->data_len;
    if (ix->data_len)
        memcpy(
            stub_cpi.data, ix->data,
            ix->data_len < STUB_MAX_DATA ? ix->data_len : STUB_MAX_DATA
        );

    stub_cpi.signers_len = (uint64_t)signers_len;
    for (int i = 0; i < signers_len && i < STUB_MAX_SEEDS; i++)
        stub_cpi.seeds_len[i] = signers[i].len;

    return stub_cpi.return_value;
}

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

typedef struct {
    uint32_t h[8];
    uint8_t buf[64];
    uint64_t len;
} Sha256;

static void sha256_block(Sha256 *s, const uint8_t *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 |
               (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i]        = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3];
    uint32_t e = s->h[4], f = s->h[5], g = s->h[6], h = s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) +
                      ((e & f) ^ (~e & g)) + K[i] + w[i];
        uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) +
                      ((a & b) ^ (a & c) ^ (b & c));
        h           = g;
        g           = f;
        f           = e;
        e           = d + t1;
        d           = c;
        c           = b;
        b           = a;
        a           = t1 + t2;
    }
    s->h[0] += a;
    s->h[1] += b;
    s->h[2] += c;
    s->h[3] += d;
    s->h[4] += e;
    s->h[5] += f;
    s->h[6] += g;
    s->h[7] += h;
}

static void sha256_update(Sha256 *s, const uint8_t *p, uint64_t n) {
    while (n--) {
        s->buf[s->len++ % 64] = *p++;
        if (s->len % 64 == 0) sha256_block(s, s->buf);
    }
}

uint64_t sol_sha256(const SignerSeed *vals, uint64_t len, uint8_t out[32]) {
    Sha256 s = {
        .h = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f,
            0x9b05688c, 0x1f83d9ab, 0x5be0cd19
        }
    };
    for (uint64_t i = 0; i < len; i++)
        sha256_update(&s, vals[i].addr, vals[i].len);

    uint64_t bits = s.len * 8;
    uint8_t pad = 0x80, zero = 0;
    sha256_update(&s, &pad, 1);
    while (s.len % 64 != 56) sha256_update(&s, &zero, 1);
    for (int i = 7; i >= 0; i--) {
        uint8_t b = (uint8_t)(bits >> (8 * i));
        sha256_update(&s, &b, 1);
    }
    for (int i = 0; i < 8; i++) {
        out[4 * i]     = (uint8_t)(s.h[i] >> 24);
        out[4 * i + 1] = (uint8_t)(s.h[i] >> 16);
        out[4 * i + 2] = (uint8_t)(s.h[i] >> 8);
        out[4 * i + 3] = (uint8_t)(s.h[i]);
    }
    return SUCCESS;
}

#ifndef CARAVEL_TEST_TUTILS_H
#define CARAVEL_TEST_TUTILS_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

#define CVL_TEST_MAX_ACCOUNTS 64
#define CVL_TEST_MAX_DATA     1232

#define _CVLT_API static __attribute__((unused))

/** lamports in sol */
#define SOL(n) ((uint64_t)(n) * 1000000000ULL)

typedef struct {
    uint8_t seed[32];
    Pubkey pubkey;
} Keypair;

static struct {
    jmp_buf *jump;
    int failed_in_test;
    struct {
        void *ptr;
        void (*free)(void *);
    } owned[2048];
    int owned_len;
    char detail[16384]; /* failure output printed under the test's name */
    size_t detail_len;
} _cvlt;

/** implemented by the selected backend (caravel/test/<backend>/) */
static void _cvlt_pubkey_from_seed(const uint8_t seed[32], Pubkey *out);
static bool _cvlt_find_pda_raw(
    const uint8_t *const *seeds,
    const size_t *lens,
    size_t n,
    const Pubkey *program_id,
    Pubkey *out,
    uint8_t *bump_out
);
static void _cvlt_print_logs(const void *logs);

static Pubkey PROGRAM_ID;

#define FAIL(...)   _cvlt_fail(__FILE__, __LINE__, __VA_ARGS__)

__attribute__((format(printf, 1, 0))) _CVLT_API void
_cvlt_vout(const char *fmt, va_list ap) {
    if (!_cvlt.jump) {
        vfprintf(stderr, fmt, ap);
        return;
    }
    size_t room = sizeof _cvlt.detail - _cvlt.detail_len;
    int n       = vsnprintf(_cvlt.detail + _cvlt.detail_len, room, fmt, ap);
    if (n > 0) _cvlt.detail_len += (size_t)n < room ? (size_t)n : room - 1;
}

__attribute__((format(printf, 1, 2))) _CVLT_API void
_cvlt_out(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _cvlt_vout(fmt, ap);
    va_end(ap);
}

__attribute__((format(printf, 3, 4), noreturn)) _CVLT_API void
_cvlt_fail(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    _cvlt_out("        %s:%d: ", file, line);
    va_start(ap, fmt);
    _cvlt_vout(fmt, ap);
    va_end(ap);
    _cvlt_out("\n");
    if (!_cvlt.jump) exit(1);
    _cvlt.failed_in_test = 1;
    longjmp(*_cvlt.jump, 1);
}

/** free `ptr` with `free_fn` when the running test ends - returns `ptr` */
_CVLT_API void *
_cvlt_own(void *ptr, void (*free_fn)(void *), const char *file, int line) {
    if (_cvlt.owned_len == (int)(sizeof _cvlt.owned / sizeof *_cvlt.owned)) {
        free_fn(ptr);
        _cvlt_fail(file, line, "too many allocations in one test");
    }
    _cvlt.owned[_cvlt.owned_len].ptr  = ptr;
    _cvlt.owned[_cvlt.owned_len].free = free_fn;
    _cvlt.owned_len++;
    return ptr;
}

_CVLT_API void _cvlt_free_owned(void) {
    while (_cvlt.owned_len > 0) {
        _cvlt.owned_len--;
        _cvlt.owned[_cvlt.owned_len].free(_cvlt.owned[_cvlt.owned_len].ptr);
    }
}

/** todo @joey move this somewhere else */

static const char _CVLT_B58[] =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

_CVLT_API bool pubkey_from_base58(const char *s, Pubkey *out) {
    uint8_t buf[64] = {0};
    size_t zeros = 0, len = 0;
    while (s[zeros] == '1') zeros++;
    for (const char *p = s + zeros; *p; p++) {
        const char *d = strchr(_CVLT_B58, *p);
        if (!d || !*p) return false;
        unsigned carry = (unsigned)(d - _CVLT_B58);
        for (size_t i = 0; i < len; i++) {
            carry += 58u * buf[i];
            buf[i] = (uint8_t)carry;
            carry >>= 8;
        }
        while (carry) {
            if (len == sizeof buf) return false;
            buf[len++] = (uint8_t)carry;
            carry >>= 8;
        }
    }
    if (zeros + len != 32) return false;
    memset(out->bytes, 0, 32);
    for (size_t i = 0; i < len; i++) out->bytes[31 - i] = buf[i];
    return true;
}

_CVLT_API const char *pubkey_to_base58(const Pubkey *key, char out[45]) {
    uint8_t digits[64] = {0};
    size_t len = 0, zeros = 0;
    while (zeros < 32 && key->bytes[zeros] == 0) zeros++;
    for (size_t i = zeros; i < 32; i++) {
        unsigned carry = key->bytes[i];
        for (size_t j = 0; j < len; j++) {
            carry += (unsigned)digits[j] << 8;
            digits[j] = (uint8_t)(carry % 58);
            carry /= 58;
        }
        while (carry) {
            digits[len++] = (uint8_t)(carry % 58);
            carry /= 58;
        }
    }
    size_t n = 0;
    for (size_t i = 0; i < zeros; i++) out[n++] = '1';
    for (size_t i = len; i > 0; i--) out[n++] = _CVLT_B58[digits[i - 1]];
    out[n] = '\0';
    return out;
}

#define PUBKEY(b58) _cvlt_pubkey((b58), __FILE__, __LINE__)

_CVLT_API Pubkey _cvlt_pubkey(const char *b58, const char *file, int line) {
    Pubkey k;
    if (!pubkey_from_base58(b58, &k))
        _cvlt_fail(file, line, "not a base58 address: %s", b58);
    return k;
}

_CVLT_API Keypair keypair(uint8_t n) {
    Keypair kp;
    memset(kp.seed, n, sizeof kp.seed);
    _cvlt_pubkey_from_seed(kp.seed, &kp.pubkey);
    return kp;
}

/**
 * Canonical PDA for the given SignerSeed values (SEED_STR, SEED_PUBKEY, ...).
 * @code
 * uint8_t bump;
 * Pubkey vault = FIND_PDA(&PROGRAM_ID, &bump, SEED_STR("vault"),
 *                         SEED_PUBKEY(&user.pubkey));
 * @endcode
 */
#define FIND_PDA(program_id, bump_out, ...) \
    _cvlt_find_pda( \
        (program_id), (bump_out), (const SignerSeed[]){__VA_ARGS__}, \
        sizeof((const SignerSeed[]){__VA_ARGS__}) / sizeof(SignerSeed), \
        __FILE__, __LINE__ \
    )

_CVLT_API Pubkey _cvlt_find_pda(
    const Pubkey *program_id,
    uint8_t *bump_out,
    const SignerSeed *seeds,
    size_t n,
    const char *file,
    int line
) {
    const uint8_t *ptrs[16];
    size_t lens[16];
    if (n > 16) _cvlt_fail(file, line, "FIND_PDA: at most 16 seeds");
    for (size_t i = 0; i < n; i++) {
        ptrs[i] = seeds[i].addr;
        lens[i] = (size_t)seeds[i].len;
    }
    Pubkey out;
    if (!_cvlt_find_pda_raw(ptrs, lens, n, program_id, &out, bump_out))
        _cvlt_fail(file, line, "FIND_PDA: no viable bump");
    return out;
}

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_TUTILS_H */

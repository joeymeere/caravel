#ifndef CVL_TYPES_H
#define CVL_TYPES_H

#include <stdbool.h>

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

typedef struct {
    uint8_t bytes[32];
} CvlPubkey;

typedef struct {
    CvlPubkey *key;           /* Account public key */
    uint64_t  *lamports;      /* Lamport balance (writable pointer) */
    uint64_t   data_len;      /* Length of account data */
    uint8_t   *data;          /* Account data (writable pointer) */
    CvlPubkey *owner;         /* Program that owns this account */
    uint64_t   rent_epoch;    /* Rent epoch */
    bool       is_signer;     /* Did this account sign the tx? */
    bool       is_writable;   /* Is this account writable? */
    bool       executable;    /* Is this account executable? */
} CvlAccountInfo;

typedef struct {
    CvlAccountInfo *accounts;      /* Array of account infos */
    uint64_t        accounts_len;  /* Number of accounts */
    uint8_t        *data;          /* Instruction data */
    uint64_t        data_len;      /* Length of instruction data */
    CvlPubkey      *program_id;    /* This program's ID */
} CvlParameters;

typedef struct {
    CvlPubkey *pubkey;
    bool       is_writable;
    bool       is_signer;
} CvlAccountMeta;

typedef struct {
    CvlPubkey        *program_id;
    CvlAccountMeta   *accounts;
    uint64_t          accounts_len;
    uint8_t          *data;
    uint64_t          data_len;
} CvlInstruction;

typedef struct {
    const uint8_t *addr;
    uint64_t       len;
} CvlSignerSeed;

typedef struct {
    const CvlSignerSeed *seeds;
    uint64_t              len;
} CvlSignerSeeds;

static const CvlPubkey CVL_SYSTEM_PROGRAM_ID = {{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
}};

static const CvlPubkey CVL_TOKEN_PROGRAM_ID = {{
    6, 221, 246, 225, 215, 101, 161, 147, 217, 203, 225, 70, 206, 235, 121, 172,
    28, 180, 133, 237, 95, 91, 55, 145, 58, 140, 245, 133, 126, 255, 0, 169
}};

static const CvlPubkey CVL_ASSOCIATED_TOKEN_PROGRAM_ID = {{
    140, 151, 37, 143, 78, 36, 137, 241, 187, 61, 16, 41, 20, 142, 13, 131,
    11, 90, 19, 153, 218, 255, 16, 132, 4, 142, 123, 216, 219, 233, 248, 89
}};

static const CvlPubkey CVL_RENT_SYSVAR_ID = {{
    6, 167, 213, 23, 25, 44, 86, 142, 224, 138, 132, 95, 115, 210, 151, 136,
    207, 3, 92, 49, 69, 178, 26, 179, 68, 216, 6, 46, 169, 64, 0, 0
}};

static const CvlPubkey CVL_CLOCK_SYSVAR_ID = {{
    6, 167, 213, 23, 24, 199, 116, 201, 40, 86, 99, 152, 105, 29, 94, 182,
    139, 94, 184, 163, 155, 75, 109, 92, 115, 85, 91, 33, 0, 0, 0, 0
}};


/* Byte-by-byte pubkey comparison */
static inline bool cvl_pubkey_equals(const CvlPubkey *a, const CvlPubkey *b) {
    const uint8_t *pa = a->bytes;
    const uint8_t *pb = b->bytes;
    for (int i = 0; i < 32; i++) {
        if (pa[i] != pb[i]) return false;
    }
    return true;
}

static inline bool cvl_pubkey_is_default(const CvlPubkey *key) {
    const uint8_t *p = key->bytes;
    for (int i = 0; i < 32; i++) {
        if (p[i] != 0) return false;
    }
    return true;
}

#define CVL_PUBKEY_SIZE  32
#ifndef CVL_MAX_ACCOUNTS
#define CVL_MAX_ACCOUNTS 16
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#define CVL_LIKELY(x)   __builtin_expect(!!(x), 1)
#define CVL_UNLIKELY(x) __builtin_expect(!!(x), 0)

#endif /* CVL_TYPES_H */

# Caravel

A C framework/toolkit for building Solana programs

## Prerequisites

- **Platform Tools** — install via `solana-install` or download from the [Solana releases](https://github.com/anza-xyz/agave/releases).
- **Node.js** (v18+, for testing)
- **Solana CLI**

## Quick Start

Build the CLI:

```sh
cd cli
make install
```

Then create a new project, build, and test:

```sh
caravel init my_program
cd my_program
caravel build # if needed, prepend `CARAVEL_INCLUDE=<path-to-/include>`
caravel test # if needed, prepend `CARAVEL_INCLUDE=<path-to-/include>`
```

### Entrypoint & Instructions

Define instruction accounts, optional args, handlers, and an entrypoint — all routed by a single-byte discriminator:

```c
#include <caravel.h>

#define INIT_ACCOUNTS(X) \
    X(payer,          SIGNER | WRITABLE) \
    X(counter,        SIGNER | WRITABLE) \
    X(system_program, PROGRAM)

IX(0, initialize, INIT_ACCOUNTS)

static uint64_t initialize(
    initialize_accounts_t *ctx, initialize_args_t *args, Parameters *params
) {
    (void)args;
    // ...
    return SUCCESS;
}

#define INC_ACCOUNTS(X) \
    X(counter,   WRITABLE) \
    X(authority, SIGNER)

IX(1, increment, INC_ACCOUNTS)

static uint64_t increment(
    increment_accounts_t *ctx, increment_args_t *args, Parameters *params
) {
    (void)args; (void)params;
    // ...
    return SUCCESS;
}

ENTRYPOINT(
    HANDLER(initialize)
    HANDLER(increment)
)
```

`IX(disc, prefix, accounts_table)` generates `prefix_accounts_t` (struct with named `AccountInfo *` fields), `prefix_validate()` (validates signer/writable flags), and an empty `prefix_args_t`. To accept instruction data, pass a packed struct as a 4th argument:

```c
typedef struct __attribute__((packed)) {
    uint64_t amount;
} deposit_args_t;

IX(0, deposit, DEPOSIT_ACCOUNTS, deposit_args_t)
```

`HANDLER(prefix)` generates a switch case that validates accounts, casts instruction data to `prefix_args_t *`, and calls your `prefix(ctx, args, params)` function.

`ENTRYPOINT(...)` deserializes accounts and dispatches via the handlers. For programs that need manual dispatch logic, use `LEGACY_ENTRYPOINT(handler)` with `DISPATCH`:

```c
static uint64_t process(Parameters *params) {
    DISPATCH(params,
        HANDLER(deposit)
        HANDLER(withdraw)
    );
    return ERROR_UNKNOWN_INSTRUCTION;
}

LEGACY_ENTRYPOINT(process);
```

### Account Context

You can also define account contexts standalone with `DEFINE_ACCOUNTS`:

```c
#define DEPOSIT_ACCOUNTS(X) \
    X(user,           SIGNER | WRITABLE) \
    X(vault,          WRITABLE) \
    X(vault_state,    WRITABLE) \
    X(system_program, PROGRAM)

DEFINE_ACCOUNTS(deposit, DEPOSIT_ACCOUNTS)
```

This generates `deposit_accounts_t` and `deposit_parse()` (validates flags and populates the struct by position):

```c
deposit_accounts_t ctx;
TRY(deposit_parse(params, &ctx));
// ctx.user, ctx.vault, ctx.vault_state, ctx.system_program
```

### State Access

```c
Rent rent;
get_rent(&rent);
uint64_t lamports = minimum_balance(&rent, sizeof(CounterState));

TRY(system_create_account(
    ctx->payer, ctx->counter,
    lamports, sizeof(CounterState), params->program_id,
    params->accounts, (int)params->accounts_len
));

CounterState *state = ACCOUNT_STATE(ctx->counter, CounterState);
state->count = 0;
```

### CPIs

Normal transfer:

```c
TRY(system_transfer(
    ctx->from, ctx->to, amount,
    params->accounts, (int)params->accounts_len
));
```

PDA-signed transfer:

```c
SignerSeed seeds[] = {
    SEED_STR("vault"),
    SEED_PUBKEY(ctx->user->key),
    SEED_U8(&state->bump),
};
SignerSeeds signer = { .seeds = seeds, .len = 3 };

TRY(system_transfer_signed(
    ctx->vault, ctx->user, amount,
    params->accounts, (int)params->accounts_len,
    &signer, 1
));
```

### SPL Token

Token transfer:

```c
TRY(token_transfer(
    ctx->user_token, ctx->vault_token, ctx->authority, amount,
    params->accounts, (int)params->accounts_len
));
```

PDA-signed token transfer:

```c
SignerSeed seeds[] = {
    SEED_STR("token_vault"),
    SEED_PUBKEY(ctx->mint->key),
    SEED_PUBKEY(ctx->authority->key),
    SEED_U8(&state->bump),
};
SignerSeeds signer = { .seeds = seeds, .len = 4 };

TRY(token_transfer_signed(
    ctx->vault_token, ctx->user_token, ctx->vault_state, amount,
    params->accounts, (int)params->accounts_len,
    &signer, 1
));
```

### PDAs

```c
SignerSeed seeds[] = { SEED_STR("vault"), SEED_PUBKEY(auth_key) };
Pubkey address;
uint8_t bump;
TRY(find_program_address(seeds, 2, program_id, &address, &bump));

// assert an account matches a PDA in one step:
ASSERT_PDA(vault_acc, seeds, 2, program_id, &bump);

// single-pass derivation:
derive_address(seeds_with_bump, 3, program_id, &address);
```

### Account Assertions

```c
ASSERT_SIGNER(acc);
ASSERT_WRITABLE(acc);
ASSERT_OWNER(acc, &expected_owner);
ASSERT_KEY(acc, &expected_key);
ASSERT_INITIALIZED(acc);
ASSERT_DATA_LEN(acc, min_len);
```

### Math

```c
uint64_t result;
TRY(checked_add_u64(a, b, &result));
TRY(checked_sub_u64(a, b, &result));
TRY(checked_mul_u64(a, b, &result));

uint64_t clamped = saturating_add_u64(a, b);
uint64_t floored = saturating_sub_u64(a, b);
uint64_t lo = min_u64(a, b);
uint64_t hi = max_u64(a, b);
```

### Sysvars

```c
Clock clock;
get_clock(&clock);

Rent rent;
get_rent(&rent);
uint64_t lamports = minimum_balance(&rent, data_size);
```

Instructions sysvar:

```c
InstructionsSysvar sysvar;
TRY(instructions_sysvar_init(&sysvar, instructions_account));
TRY(assert_no_reentrancy(&sysvar, params->program_id));

uint16_t count = instructions_count(&sysvar);
LoadedInstruction ix;
TRY(instructions_get(&sysvar, 0, &ix));
```

### Heap

The built-in bump allocator is included by default:

```c
void *buf = alloc(1024);  // 8-byte aligned, zero-initialized
heap_reset();              // reclaim all allocations
```

### Logging

```c
log("hello");                   // runtime strlen
log_literal("hello");           // compile-time strlen
log_u64(42);                    // log a single u64
log_64(a, b, c, d, e);         // log up to 5 u64 values
log_pubkey(key);                // base58 in validator logs
log_compute_units();            // remaining CUs

// debug logging (enabled with -DDEBUG):
debug("checkpoint");
debug_u64(val);
debug_pubkey(key);
```

### Utilities

```c
bool eq = pubkey_eq(&a, &b);
pubkey_cpy(dst, src);

// Stack-allocated growable array:
Vec(uint64_t, 10) prices = VEC_INIT;
vec_push(&prices, 42);
uint64_t last = vec_pop(&prices);

// Little-endian readers (advance pointer):
uint8_t  b = READ_U8(ptr);
uint64_t v = READ_U64(ptr);
```

## Vault Benchmark

| Benchmark | Caravel | Quasar |
|-----------|---------|--------|
| Deposit CUs | 1577 | 1576 |
| Withdraw CUs | 353 | 411 |
| Binary Size | 3,968 bytes | 5,848 bytes |

## CLI Reference

| Command | Description |
|---------|-------------|
| `caravel init <name>` | Scaffold a new project with entrypoint, state, tests, and config |
| `caravel build` | Compile C sources to SBF and link into `build/program.so` |
| `caravel test` | Build, then run TypeScript tests via `npm test` |
| `caravel deploy` | Deploy `build/program.so` via `solana program deploy` |
| `caravel clean` | Remove the `build/` directory |
| `caravel idl` | Generate Anchor-compatible `build/idl.json` from macro definitions |

## Compile Options

Define these before `#include <caravel.h>` (or pass via `-D` flags) to customize certain characteristics:

| Option | Default | Description |
|--------|---------|-------------|
| `MAX_ACCOUNTS` | `16` | Max accounts per instruction |
| `NO_HEAP` | *(not set)* | Exclude the built-in bump allocator entirely. |
| `CUSTOM_HEAP` | *(not set)* | Keep heap constants (`HEAP_START`, `HEAP_SZ`) but skip the default bump allocator so you can provide your own. |
| `HEAP_SZ` | `32768` | Override the heap size (bytes). |
| `NO_SYSTEM` | *(not set)* | Exclude System program CPI helpers (`system_transfer`, `system_create_account`, etc.). |
| `NO_TOKEN` | *(not set)* | Exclude SPL Token CPI helpers (`token_transfer`, `token_mint_to`, etc.). |
| `DEBUG` | *(not set)* | Enable `debug()`, `debug_u64()`, and `debug_pubkey()` macros. When not set, these compile to no-ops. |

## API Reference

### Types

| Type | Description |
|------|-------------|
| `Parameters` | Deserialized instruction: accounts, data, program_id |
| `AccountInfo` | Account with key, lamports, data, owner, flags |
| `Pubkey` | 32-byte public key |
| `AccountMeta` | CPI account meta (pubkey, is_writable, is_signer) |
| `Instruction` | CPI instruction (program_id, accounts, data) |
| `SignerSeed` | Single PDA seed (addr + len) |
| `SignerSeeds` | Array of seeds for PDA signing |
| `Clock` | Clock sysvar fields |
| `Rent` | Rent sysvar fields |
| `TokenAccount` | SPL token account layout (packed) |
| `MintAccount` | SPL mint account layout (packed) |

### Constants

| Constant | Description |
|----------|-------------|
| `SYSTEM_PROGRAM_ID` | System program |
| `TOKEN_PROGRAM_ID` | SPL Token program |
| `ASSOCIATED_TOKEN_PROGRAM_ID` | Associated Token program |
| `RENT_SYSVAR_ID` | Rent sysvar |
| `CLOCK_SYSVAR_ID` | Clock sysvar |
| `INSTRUCTIONS_SYSVAR_ID` | Instructions sysvar |

### System Program (`system.h`)

| Function | Description |
|----------|-------------|
| `system_transfer(from, to, lamports, accounts, len)` | SOL transfer |
| `system_transfer_signed(...)` | PDA-signed SOL transfer |
| `system_create_account(payer, new, lamports, space, owner, accounts, len)` | Create account |
| `system_create_account_signed(...)` | PDA-signed create account |
| `system_allocate(account, space, accounts, len)` | Allocate space |
| `system_assign(account, owner, accounts, len)` | Assign owner |
| `system_assign_signed(...)` | PDA-signed assign |

### SPL Token (`token.h`)

| Function | Description |
|----------|-------------|
| `token_transfer(src, dst, auth, amount, accounts, len)` | Token transfer |
| `token_transfer_signed(...)` | PDA-signed token transfer |
| `token_mint_to(mint, dst, auth, amount, accounts, len)` | Mint tokens |
| `token_mint_to_signed(...)` | PDA-signed mint |
| `token_burn(token, mint, auth, amount, accounts, len)` | Burn tokens |
| `token_close_account(token, dst, auth, accounts, len)` | Close token account |
| `token_close_account_signed(...)` | PDA-signed close |
| `token_approve(token, delegate, owner, amount, accounts, len)` | Approve delegate |
| `token_sync_native(token, accounts, len)` | Sync native SOL balance |
| `TOKEN_ACCOUNT(acc)` | Cast to `TokenAccount *` |
| `MINT_ACCOUNT(acc)` | Cast to `MintAccount *` |

### CPI (`cpi.h`)

| Function / Macro | Description |
|----------|-------------|
| `invoke(ix, accounts, len)` | Invoke a CPI with no seeds |
| `invoke_signed(ix, accounts, len, seeds, seeds_len)` | Signed CPI |
| `CPI_INVOKE(prog, metas, n, data, dlen, accounts, len)` | Build + invoke on stack |
| `CPI_INVOKE_SIGNED(...)` | Build + invoke signed on stack |
| `meta_writable_signer(key)` | Writable signer meta |
| `meta_writable(key)` | Writable non-signer meta |
| `meta_readonly_signer(key)` | Readonly signer meta |
| `meta_readonly(key)` | Readonly non-signer meta |

## Disclaimer

Under no circumstances will there ever be a financial asset involved with this project. This includes, but is not limited to: 

 - Tokens
 - NFTs
 - Securities, or security-like instruments

Anyone claiming such is a scammer.

## License

Apache 2.0

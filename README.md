# Caravel

A C library for building efficient Solana programs

## Prerequisites

- **Platform Tools** — install via `solana-install` or download from the [Agave releases](https://github.com/anza-xyz/agave/releases).
- **A host C compiler** (`cc`, for tests) — or **Node.js** (v20+) for TypeScript tests
- **Agave CLI**

## Quick Start

Build and install the CLI and SDK headers:

```sh
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

`IX(disc, prefix, accounts_table)` generates `prefix_accounts_t` (struct with named `AccountInfo *` fields), `prefix_validate()` (validates `SIGNER` / `WRITABLE` / `PROGRAM` flags), and an empty `prefix_args_t`. To accept instruction data, pass a packed struct as a 4th argument:

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

System transfer:

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
ASSERT_EXECUTABLE(acc);
```

Account flags in X-macro tables are checked by `prefix_validate()` / `prefix_parse()`:

| Flag | Check | Error |
|------|-------|-------|
| `SIGNER` | `is_signer` | `ERROR_MISSING_REQUIRED_SIGNATURE` |
| `WRITABLE` | `is_writable` | `ERROR_ACCOUNT_NOT_WRITABLE` |
| `PROGRAM` | `executable` (not *which* program — use `ASSERT_KEY` for that) | `ERROR_ACCOUNT_NOT_EXECUTABLE` |

### Errors

Builtin errors (`ERROR_INVALID_ARGUMENT`, `ERROR_MISSING_REQUIRED_SIGNATURE`, ...) are encoded the way the runtime expects (`TO_BUILTIN(n) = n << 32`), so they surface as the matching `ProgramError` variant (e.g. `InvalidArgument`). Your own errors use `ERROR_CUSTOM(n)`, which surfaces as `Custom(0x100 + n)`. Caravel's own custom errors occupy `ERROR_CUSTOM(0)`..`ERROR_CUSTOM(7)`.

```c
#define MY_ERROR_TOO_LATE ERROR_CUSTOM(32)

if (clock.unix_timestamp > state->deadline) return MY_ERROR_TOO_LATE;
```

### Duplicate Accounts

By default, every entrypoint rejects instructions that pass the same account more than once (`ERROR_DUPLICATE_ACCOUNT`). This prevents aliasing bugs, e.g. a context where `from` and `to` are secretly the same account. Define `ALLOW_DUPLICATE_ACCOUNTS` to accept duplicates; your handlers then need to check keys wherever aliasing matters.

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

### Logging

```c
log_str("hello");               // runtime strlen
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

## Vault Benchmark

| Benchmark | Caravel | Quasar |
|-----------|---------|--------|
| Deposit CUs | 1577 | 1576 |
| Withdraw CUs | 353 | 411 |
| Binary Size | 3,968 bytes | 5,848 bytes |

## Testing

`caravel test` builds the program, then compiles each `tests/<name>.c` with
the host compiler and runs it on [LiteSVM](https://github.com/LiteSVM/litesvm)
through [litesvm-cxx](https://github.com/joeymeere/litesvm-cxx), which it
downloads on first use. Tests build instructions from the program's own
`IX()` declarations (keep them in a header such as `src/instructions.h`):

```c
#include <caravel/test.h>
#include "../src/instructions.h"
#include "../src/state.h"

static Keypair payer, counter;

SETUP {
    payer   = keypair(1);
    counter = keypair(2);
    svm_airdrop(svm, &payer.pubkey, SOL(10));
}

TEST(initializes_a_counter) {
    Ix ix = MAKE_IX(initialize, .payer = payer.pubkey,
                    .counter = counter.pubkey,
                    .system_program = SYSTEM_PROGRAM_ID);
    ASSERT_OK(SEND(svm, ix, &payer, &counter));
    ASSERT_EQ(READ_ACCOUNT(svm, &counter.pubkey, CounterState).count, 0);
}

TEST(rejects_a_duplicate_account) {
    Ix ix = MAKE_IX(increment, .counter = payer.pubkey,
                    .authority = payer.pubkey);
    ASSERT_ERR(SEND(svm, ix, &payer), ERROR_DUPLICATE_ACCOUNT);
}
```

Tests in a file share one VM and run in order. `caravel test <filter>` runs
only tests whose name contains `<filter>`. See `include/caravel/test.h` for
the full API (PDAs, SPL Token setup, account reads, clock and compute
budget) and `examples/` for complete suites.

### Backends

C tests run on LiteSVM by default: signed transactions against a bank,
through `SEND()` and `svm_*()`. They can run on
[Mollusk](https://github.com/anza-xyz/mollusk) instead (through
[mollusk-cxx](https://github.com/joeymeere/mollusk-cxx), also downloaded on
first use), which processes instructions against an account store with no
signing or fees. Start a project on it with
`caravel init <name> --backend=mollusk` (or `seashell`), set `[test] backend = "mollusk"` in
`Caravel.toml`, or pick per file:

```c
#define CARAVEL_TEST_MOLLUSK
#include <caravel/test.h>

SETUP { ctx_airdrop(ctx, &payer, SOL(10)); }

TEST(initializes_a_counter) {
    Ix ix = MAKE_IX(initialize, .payer = payer, .counter = counter,
                    .system_program = SYSTEM_PROGRAM_ID);
    ASSERT_OK(PROCESS(ctx, ix));
}
```

`PROCESS_CHAIN()` runs instructions one after another and stores the result
only if all succeed; `PROCESS_TX()` runs them as one transaction.

[Seashell](https://github.com/soundsonacid/seashell) (through
[seashell-cxx](https://github.com/joeymeere/seashell-cxx)) runs instructions
against real mainnet state. `SCENARIO("name")` in `SETUP` replays the accounts
recorded in `tests/scenarios/name.json.gz`; with `RPC_URL` set, accounts the
test touches that nobody has are fetched from it and recorded there, so
commit the file and CI stays offline:

```c
#define CARAVEL_TEST_SEASHELL
#include <caravel/test.h>

SETUP {
    SCENARIO("usdc");
    usdc = PUBKEY("EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v");
}

TEST(reads_the_usdc_mint) {
    ASSERT_EQ(READ_ACCOUNT(shell, &usdc, MintAccount).decimals, 6);
}
```

Its handle is `Shell *shell`, with `PROCESS()` and `shell_*()` helpers.
Nothing is signed, so a test can act as any authority, such as a real mint's.
While recording, every address is looked up on mainnet, so accounts a test
creates need addresses that are unused there (`keypair(n)` ones are not).

`MAKE_IX`, `ASSERT_*`, `READ_ACCOUNT` and `report_cu` are the same on every
backend. `examples/counter` has its suite written for each, and
`examples/amm/tests/amm_seashell.c` runs the AMM against the real USDC mint.

TypeScript tests (litesvm + `@solana/kit`) remain available:
`caravel init <name> --tests=ts`, or `[test] runner = "ts"` in
`Caravel.toml`. `examples/token-vault` uses them.

## CLI Reference

| Command | Description |
|---------|-------------|
| `caravel init <name>` | Scaffold a new project with entrypoint, state, tests, and config (`--backend=mollusk` or `--backend=seashell` for those test backends, `--tests=ts` for TypeScript tests) |
| `caravel build` | Compile C sources to SBF and link into `build/program.so` |
| `caravel test [filter]` | Build, then run the C tests in `tests/` (or TypeScript tests via `npm test`) |
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
| `ALLOW_DUPLICATE_ACCOUNTS` | *(not set)* | Accept instructions that pass the same account more than once. By default they are rejected with `ERROR_DUPLICATE_ACCOUNT`. |
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
| `token_initialize_mint(mint, rent, mint_auth, freeze_auth, decimals, accounts, len)` | Initialize mint (`freeze_auth` may be `NULL`) |
| `token_initialize_account(token, mint, owner, accounts, len)` | Initialize token account |
| `token_initialize_account_signed(...)` | PDA-signed initialize token account |
| `token_transfer(src, dst, auth, amount, accounts, len)` | Token transfer |
| `token_transfer_signed(...)` | PDA-signed token transfer |
| `token_mint_to(mint, dst, auth, amount, accounts, len)` | Mint tokens |
| `token_mint_to_signed(...)` | PDA-signed mint |
| `token_burn(token, mint, auth, amount, accounts, len)` | Burn tokens |
| `token_burn_signed(...)` | PDA-signed burn |
| `token_close_account(token, dst, auth, accounts, len)` | Close token account |
| `token_close_account_signed(...)` | PDA-signed close |
| `token_approve(token, delegate, owner, amount, accounts, len)` | Approve delegate |
| `token_approve_signed(...)` | PDA-signed approve |
| `token_revoke(token, owner, accounts, len)` | Revoke delegate |
| `token_revoke_signed(...)` | PDA-signed revoke |
| `token_set_authority(owned, auth, type, new_auth, accounts, len)` | Set/clear authority (`TOKEN_AUTHORITY_*`, `new_auth` may be `NULL`) |
| `token_set_authority_signed(...)` | PDA-signed set authority |
| `token_freeze_account(token, mint, freeze_auth, accounts, len)` | Freeze token account |
| `token_freeze_account_signed(...)` | PDA-signed freeze |
| `token_thaw_account(token, mint, freeze_auth, accounts, len)` | Thaw token account |
| `token_thaw_account_signed(...)` | PDA-signed thaw |
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

This library is unaudited, and thus is not recommended to be used within production programs.

Under no circumstances will there ever be a financial asset involved with this project. This includes, but is not limited to: 

 - Tokens
 - NFTs
 - Securities, or security-like instruments

Anyone claiming such is a scammer.

## License

Apache 2.0

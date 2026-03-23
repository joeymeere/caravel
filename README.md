# Caravel

A header-only C framework for building Solana programs

## Prerequisites

- **Solana platform-tools** — Provides `clang` and `ld.lld` targeting SBF. Install via `solana-install` or download from the [Solana releases](https://github.com/anza-xyz/agave/releases).
- **Node.js** (v18+, for testing)
- **Solana CLI**

## Quick Start

Build the CLI:

```sh
cd cli
make
```

Then create a new project, build, and test:

```sh
caravel init my_program
cd my_program
caravel build
caravel test
```

### Entrypoint

Define your program entrypoint and route instructions with a single-byte discriminator:

```c
#include <caravel.h>

static uint64_t process(CvlParameters *params) {
    CVL_DISPATCH(params,
        CVL_INSTRUCTION(0, handle_initialize)
        CVL_INSTRUCTION(1, handle_increment)
        CVL_INSTRUCTION(2, handle_decrement)
    );
    return CVL_ERROR_UNKNOWN_INSTRUCTION;
}

CVL_ENTRYPOINT(process);
```

### Account Context

Declare expected accounts with flags, and Caravel generates a typed struct and a validation/parsing function:

```c
#define DEPOSIT_ACCOUNTS(X) \
    X(user,           CVL_SIGNER | CVL_WRITABLE) \
    X(vault,          CVL_WRITABLE) \
    X(vault_state,    CVL_WRITABLE) \
    X(system_program, CVL_PROGRAM)

CVL_DEFINE_ACCOUNTS(deposit, DEPOSIT_ACCOUNTS)
```

This generates `deposit_accounts_t` (struct with named `CvlAccountInfo *` fields) and `deposit_parse()` (validates signer/writable flags and populates the struct by position):

```c
static uint64_t handle_deposit(CvlParameters *params) {
    deposit_accounts_t ctx;
    CVL_TRY(deposit_parse(params, &ctx));
    // ctx.user, ctx.vault, ctx.vault_state, ctx.system_program
    ...
}
```

### State Access

```c
CvlRent rent;
cvl_get_rent(&rent);
uint64_t lamports = cvl_minimum_balance(&rent, sizeof(CounterState));

CVL_TRY(cvl_system_create_account(
    ctx.payer, ctx.counter,
    lamports, sizeof(CounterState), params->program_id,
    params->accounts, (int)params->accounts_len
));

CounterState *state = CVL_ACCOUNT_STATE(ctx.counter, CounterState);
state->count = 0;
```

### CPIs

Normal transfer:

```c
CVL_TRY(cvl_system_transfer(
    ctx.from, ctx.to, amount,
    params->accounts, (int)params->accounts_len
));
```

PDA-signed transfer:

```c
CvlSignerSeed seeds[] = {
    CVL_SEED_STR("vault"),
    CVL_SEED_PUBKEY(ctx.user->key),
    CVL_SEED_U8(&state->bump),
};
CvlSignerSeeds signer = { .seeds = seeds, .len = 3 };

CVL_TRY(cvl_system_transfer_signed(
    ctx.vault, ctx.user, amount,
    params->accounts, (int)params->accounts_len,
    &signer, 1
));
```

### SPL Token

Token transfer:

```c
CVL_TRY(cvl_token_transfer(
    ctx.user_token, ctx.vault_token, ctx.authority, amount,
    params->accounts, (int)params->accounts_len
));
```

PDA-signed token transfer:

```c
CvlSignerSeed seeds[] = {
    CVL_SEED_STR("token_vault"),
    CVL_SEED_PUBKEY(ctx.mint->key),
    CVL_SEED_PUBKEY(ctx.authority->key),
    CVL_SEED_U8(&state->bump),
};
CvlSignerSeeds signer = { .seeds = seeds, .len = 4 };

CVL_TRY(cvl_token_transfer_signed(
    ctx.vault_token, ctx.user_token, ctx.vault_state, amount,
    params->accounts, (int)params->accounts_len,
    &signer, 1
));
```

## CLI Commands

| Command | Description |
|---------|-------------|
| `caravel init <name>` | Scaffold a new project with entrypoint, state, tests, and config |
| `caravel build` | Compile C sources to SBF and link into `build/program.so` |
| `caravel test` | Build, then run TypeScript tests via `npm test` |
| `caravel deploy` | Deploy `build/program.so` via `solana program deploy` |
| `caravel clean` | Remove the `build/` directory |
| `caravel idl` | Generate Anchor-compatible `build/idl.json` from `@cvl:` annotations |


## Disclaimer

Under no circumstances will there ever be a financial asset involved with this project. This includes, but is not limited to: 

 - Tokens
 - NFTs
 - Securities, or security-like instruments

Anyone claiming such is a scammer.

## License

Apache 2.0
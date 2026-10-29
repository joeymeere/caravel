/** these are generic across all backends */

#ifndef CARAVEL_TEST_TERR_H
#define CARAVEL_TEST_TERR_H

#ifndef CARAVEL_TEST_H
#error "include <caravel/test.h>"
#else

#define TEST_ERR_NONE        0
#define TEST_ERR_INSTRUCTION 1 /* an ix failed - see `code` */
#define TEST_ERR_TRANSACTION 2 /* rejected before or around execution */

/* ix error variant indices used most in tests. A program that runs out of compute units fails with PROGRAM_FAILED_TO_COMPLETE */
#define TEST_IX_ERR_CUSTOM   25
#define TEST_IX_ERR_COMPUTATIONAL_BUDGET_EXCEEDED 37
#define TEST_IX_ERR_PROGRAM_FAILED_TO_COMPLETE    40

/** why a tx or ix failed */
typedef struct {
    int32_t kind;              /* TEST_ERR_* */
    uint32_t code;             /* TransactionError / InstructionError index */
    uint8_t instruction_index; /* which instruction, for TEST_ERR_INSTRUCTION */
    uint64_t program_error;    /* the program's u64 error (ERROR_* values) */
} TestError;

/** TODO @joey i hate this */

/* variant names in consensus (bincode) order */
static const char *const _CVLT_IX_ERRORS[] = {
    "GenericError",
    "InvalidArgument",
    "InvalidInstructionData",
    "InvalidAccountData",
    "AccountDataTooSmall",
    "InsufficientFunds",
    "IncorrectProgramId",
    "MissingRequiredSignature",
    "AccountAlreadyInitialized",
    "UninitializedAccount",
    "UnbalancedInstruction",
    "ModifiedProgramId",
    "ExternalAccountLamportSpend",
    "ExternalAccountDataModified",
    "ReadonlyLamportChange",
    "ReadonlyDataModified",
    "DuplicateAccountIndex",
    "ExecutableModified",
    "RentEpochModified",
    "NotEnoughAccountKeys",
    "AccountDataSizeChanged",
    "AccountNotExecutable",
    "AccountBorrowFailed",
    "AccountBorrowOutstanding",
    "DuplicateAccountOutOfSync",
    "Custom",
    "InvalidError",
    "ExecutableDataModified",
    "ExecutableLamportChange",
    "ExecutableAccountNotRentExempt",
    "UnsupportedProgramId",
    "CallDepth",
    "MissingAccount",
    "ReentrancyNotAllowed",
    "MaxSeedLengthExceeded",
    "InvalidSeeds",
    "InvalidRealloc",
    "ComputationalBudgetExceeded",
    "PrivilegeEscalation",
    "ProgramEnvironmentSetupFailure",
    "ProgramFailedToComplete",
    "ProgramFailedToCompile",
    "Immutable",
    "IncorrectAuthority",
    "BorshIoError",
    "AccountNotRentExempt",
    "InvalidAccountOwner",
    "ArithmeticOverflow",
    "UnsupportedSysvar",
    "IllegalOwner",
    "MaxAccountsDataAllocationsExceeded",
    "MaxAccountsExceeded",
    "MaxInstructionTraceLengthExceeded",
    "BuiltinProgramsMustConsumeComputeUnits",
};

static const char *const _CVLT_TX_ERRORS[] = {
    "AccountInUse",
    "AccountLoadedTwice",
    "AccountNotFound",
    "ProgramAccountNotFound",
    "InsufficientFundsForFee",
    "InvalidAccountForFee",
    "AlreadyProcessed",
    "BlockhashNotFound",
    "InstructionError",
    "CallChainTooDeep",
    "MissingSignatureForFee",
    "InvalidAccountIndex",
    "SignatureFailure",
    "InvalidProgramForExecution",
    "SanitizeFailure",
    "ClusterMaintenance",
    "AccountBorrowOutstanding",
    "WouldExceedMaxBlockCostLimit",
    "UnsupportedVersion",
    "InvalidWritableAccount",
    "WouldExceedMaxAccountCostLimit",
    "WouldExceedAccountDataBlockLimit",
    "TooManyAccountLocks",
    "AddressLookupTableNotFound",
    "InvalidAddressLookupTableOwner",
    "InvalidAddressLookupTableData",
    "InvalidAddressLookupTableIndex",
    "InvalidRentPayingAccount",
    "WouldExceedMaxVoteCostLimit",
    "WouldExceedAccountDataTotalLimit",
    "DuplicateInstruction",
    "InsufficientFundsForRent",
    "MaxLoadedAccountsDataSizeExceeded",
    "InvalidLoadedAccountsDataSizeLimit",
    "ResanitizationNeeded",
    "ProgramExecutionTemporarilyRestricted",
    "UnbalancedTransaction",
    "ProgramCacheHitMaxLimit",
    "CommitCancelled",
};

#define _CVLT_NAME(table, i) \
    ((i) < sizeof(table) / sizeof(*(table)) ? (table)[i] : "unknown error")

_CVLT_API const char *_cvlt_describe(const TestError *e, char *buf, size_t n) {
    if (e->kind == TEST_ERR_NONE)
        snprintf(buf, n, "it succeeded");
    else if (e->kind == TEST_ERR_TRANSACTION)
        snprintf(
            buf, n, "transaction failed: %s",
            _CVLT_NAME(_CVLT_TX_ERRORS, e->code)
        );
    else if (e->code == TEST_IX_ERR_CUSTOM)
        /* Custom(0) travels as 1 << 32, since 0 means success */
        snprintf(
            buf, n, "instruction %u failed: Custom(0x%llx)",
            e->instruction_index,
            (unsigned long long)(e->program_error == (1ULL << 32)
                                     ? 0
                                     : e->program_error)
        );
    else if (e->program_error)
        snprintf(
            buf, n, "instruction %u failed: %s (0x%llx)", e->instruction_index,
            _CVLT_NAME(_CVLT_IX_ERRORS, e->code),
            (unsigned long long)e->program_error
        );
    else
        snprintf(
            buf, n, "instruction %u failed: %s", e->instruction_index,
            _CVLT_NAME(_CVLT_IX_ERRORS, e->code)
        );
    return buf;
}

#define ASSERT(cond) \
    do { \
        if (!(cond)) FAIL("ASSERT(%s)", #cond); \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        uint64_t _a = (uint64_t)(a), _b = (uint64_t)(b); \
        if (_a != _b) \
            FAIL( \
                "ASSERT_EQ(%s, %s): %llu != %llu", #a, #b, \
                (unsigned long long)_a, (unsigned long long)_b \
            ); \
    } while (0)

#define ASSERT_KEY_EQ(a, b) \
    do { \
        const Pubkey *_a = (a), *_b = (b); \
        char _x[45], _y[45]; \
        if (memcmp(_a, _b, 32) != 0) \
            FAIL( \
                "ASSERT_KEY_EQ(%s, %s): %s != %s", #a, #b, \
                pubkey_to_base58(_a, _x), pubkey_to_base58(_b, _y) \
            ); \
    } while (0)

_CVLT_API void _cvlt_expect(
    bool ok,
    const TestError *error,
    const void *logs,
    bool want_ok,
    uint64_t want,
    const char *file,
    int line
) {
    char got[128];
    if (want_ok && ok) return;
    if (!want_ok && !ok &&
        (want == 0 ||
         (error->kind == TEST_ERR_INSTRUCTION && error->program_error == want)))
        return;
    if (logs) _cvlt_print_logs(logs);
    if (want_ok)
        _cvlt_fail(
            file, line, "expected success, %s",
            _cvlt_describe(error, got, sizeof got)
        );
    else if (want == 0)
        _cvlt_fail(
            file, line, "expected the transaction to fail, it succeeded"
        );
    else
        _cvlt_fail(
            file, line, "expected %s0x%llx%s, %s", want >> 32 ? "" : "Custom(",
            (unsigned long long)want, want >> 32 ? "" : ")",
            _cvlt_describe(error, got, sizeof got)
        );
}

#define _CVLT_EXPECT(r, want_ok, want) \
    do { \
        __typeof__(r) _r = (r); \
        _cvlt_expect( \
            _r.ok, &_r.error, _r.logs, (want_ok), (uint64_t)(want), __FILE__, \
            __LINE__ \
        ); \
    } while (0)

/** it succeeded - prints the logs if not */
#define ASSERT_OK(r)        _CVLT_EXPECT((r), true, 0)

/** the program returned `code` (any ERROR_* value, e.g. ERROR_CUSTOM(3)) */
#define ASSERT_ERR(r, code) _CVLT_EXPECT((r), false, (code))

/** it failed, for any reason */
#define ASSERT_FAILS(r)     _CVLT_EXPECT((r), false, 0)

/** print "<label>: <n> CU", the line `make bench` collects */
#define report_cu(label, r) \
    printf( \
        "      %s: %llu CU\n", (label), (unsigned long long)(r).compute_units \
    )

#endif /* CARAVEL_TEST_H */
#endif /* CARAVEL_TEST_TERR_H */

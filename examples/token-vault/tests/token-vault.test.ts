import { expect } from "chai";
import { LiteSVM, FailedTransactionMetadata, TransactionMetadata } from "litesvm";
import {
  address,
  type Address,
  AccountRole,
  appendTransactionMessageInstruction,
  appendTransactionMessageInstructions,
  compileTransaction,
  createNoopSigner,
  createTransactionMessage,
  generateKeyPair,
  getAddressEncoder,
  getAddressFromPublicKey,
  getProgramDerivedAddress,
  lamports,
  pipe,
  setTransactionMessageFeePayer,
  signTransaction,
  type Instruction,
  type Transaction,
} from "@solana/kit";
import { getCreateAccountInstruction } from "@solana-program/system";
import {
  TOKEN_PROGRAM_ADDRESS,
  findAssociatedTokenPda,
  getCreateAssociatedTokenInstruction,
  getInitializeMint2Instruction,
  getMintSize,
  getMintToInstruction,
} from "@solana-program/token";
import * as fs from "fs";
import * as path from "path";
import { fileURLToPath } from "url";
const __dirname = path.dirname(fileURLToPath(import.meta.url));

const LAMPORTS_PER_SOL = 1_000_000_000n;
const SYSTEM_PROGRAM = address("11111111111111111111111111111111");

async function sendAndConfirm(
  svm: LiteSVM,
  tx: Transaction,
  label?: string,
): Promise<TransactionMetadata> {
  const result = svm.sendTransaction(tx);
  if (result instanceof FailedTransactionMetadata) {
    throw new Error(`Transaction failed: ${result.toString()}`);
  }
  if (label) {
    console.log(`      ${label}: ${result.computeUnitsConsumed()} CU`);
  }
  return result;
}

async function buildTx(
  svm: LiteSVM,
  feePayer: Address,
  signerKeys: CryptoKeyPair[],
  ixs: Instruction | Instruction[],
): Promise<Transaction> {
  const list = Array.isArray(ixs) ? ixs : [ixs];
  const msg = pipe(
    createTransactionMessage({ version: "legacy" }),
    (m) => setTransactionMessageFeePayer(feePayer, m),
    (m) => svm.setTransactionMessageLifetimeUsingLatestBlockhash(m),
    (m) => appendTransactionMessageInstructions(list, m),
  );
  return await signTransaction(signerKeys, compileTransaction(msg));
}

function readTokenBalance(svm: LiteSVM, account: Address): bigint {
  const info = svm.getAccount(account);
  if (!info.exists) throw new Error(`Account ${account} not found`);
  const view = new DataView(
    info.data.buffer,
    info.data.byteOffset,
    info.data.byteLength,
  );
  return view.getBigUint64(64, true);
}

function encodeInstruction(disc: number, amount: bigint): Uint8Array {
  const buf = new ArrayBuffer(9);
  const view = new DataView(buf);
  view.setUint8(0, disc);
  view.setBigUint64(1, amount, true);
  return new Uint8Array(buf);
}

describe("Token Vault Program", () => {
  const programPath = path.join(__dirname, "..", "build", "program.so");

  let programId: Address;
  let svm: LiteSVM;
  let authorityKeys: CryptoKeyPair;
  let authorityAddr: Address;
  let mintKeys: CryptoKeyPair;
  let mintAddr: Address;
  let userTokenAccount: Address;
  let vaultTokenAccount: Address;
  let vaultStatePda: Address;

  const DEPOSIT_AMOUNT = 1_000_000n; // 1M tokens
  const WITHDRAW_AMOUNT = 500_000n;

  before(async () => {
    const programKeys = await generateKeyPair();
    programId = await getAddressFromPublicKey(programKeys.publicKey);

    svm = new LiteSVM().withDefaultPrograms();
    svm.addProgram(programId, fs.readFileSync(programPath));

    authorityKeys = await generateKeyPair();
    authorityAddr = await getAddressFromPublicKey(authorityKeys.publicKey);
    svm.airdrop(authorityAddr, lamports(10n * LAMPORTS_PER_SOL));

    mintKeys = await generateKeyPair();
    mintAddr = await getAddressFromPublicKey(mintKeys.publicKey);

    const encoder = getAddressEncoder();
    const [pda] = await getProgramDerivedAddress({
      programAddress: programId,
      seeds: [
        new TextEncoder().encode("token_vault"),
        encoder.encode(mintAddr),
        encoder.encode(authorityAddr),
      ],
    });
    vaultStatePda = pda;

    [userTokenAccount] = await findAssociatedTokenPda({
      owner: authorityAddr,
      tokenProgram: TOKEN_PROGRAM_ADDRESS,
      mint: mintAddr,
    });

    [vaultTokenAccount] = await findAssociatedTokenPda({
      owner: vaultStatePda,
      tokenProgram: TOKEN_PROGRAM_ADDRESS,
      mint: mintAddr,
    });
  });

  it("sets up mint and token accounts", async () => {
    const mintSize = BigInt(getMintSize());
    const rentExempt = BigInt(svm.minimumBalanceForRentExemption(mintSize));

    const createMintIx = getCreateAccountInstruction({
      payer: createNoopSigner(authorityAddr),
      newAccount: createNoopSigner(mintAddr),
      lamports: rentExempt,
      space: mintSize,
      programAddress: TOKEN_PROGRAM_ADDRESS,
    });
    const initMintIx = getInitializeMint2Instruction({
      mint: mintAddr,
      decimals: 6,
      mintAuthority: authorityAddr,
      freezeAuthority: null,
    });
    let tx = await buildTx(svm, authorityAddr, [authorityKeys, mintKeys], [
      createMintIx,
      initMintIx,
    ]);
    await sendAndConfirm(svm, tx, "create mint");

    const createUserAtaIx = getCreateAssociatedTokenInstruction({
      payer: createNoopSigner(authorityAddr),
      ata: userTokenAccount,
      owner: authorityAddr,
      mint: mintAddr,
    });
    const mintToIx = getMintToInstruction({
      mint: mintAddr,
      token: userTokenAccount,
      mintAuthority: authorityAddr,
      amount: DEPOSIT_AMOUNT,
    });
    tx = await buildTx(svm, authorityAddr, [authorityKeys], [
      createUserAtaIx,
      mintToIx,
    ]);
    await sendAndConfirm(svm, tx, "user ATA + mintTo");

    const createVaultAtaIx = getCreateAssociatedTokenInstruction({
      payer: createNoopSigner(authorityAddr),
      ata: vaultTokenAccount,
      owner: vaultStatePda,
      mint: mintAddr,
    });
    tx = await buildTx(svm, authorityAddr, [authorityKeys], createVaultAtaIx);
    await sendAndConfirm(svm, tx, "vault ATA");

    expect(readTokenBalance(svm, userTokenAccount)).to.equal(DEPOSIT_AMOUNT);
    expect(readTokenBalance(svm, vaultTokenAccount)).to.equal(0n);
  });

  it("deposits tokens into the vault", async () => {
    const ix: Instruction = {
      programAddress: programId,
      accounts: [
        { address: authorityAddr, role: AccountRole.READONLY_SIGNER },
        { address: userTokenAccount, role: AccountRole.WRITABLE },
        { address: vaultTokenAccount, role: AccountRole.WRITABLE },
        { address: vaultStatePda, role: AccountRole.WRITABLE },
        { address: mintAddr, role: AccountRole.READONLY },
        { address: SYSTEM_PROGRAM, role: AccountRole.READONLY },
        { address: TOKEN_PROGRAM_ADDRESS, role: AccountRole.READONLY },
      ],
      data: encodeInstruction(0, DEPOSIT_AMOUNT),
    };

    const tx = await buildTx(svm, authorityAddr, [authorityKeys], ix);
    await sendAndConfirm(svm, tx, "deposit");

    expect(readTokenBalance(svm, vaultTokenAccount)).to.equal(DEPOSIT_AMOUNT);
    expect(readTokenBalance(svm, userTokenAccount)).to.equal(0n);
  });

  it("withdraws tokens from the vault", async () => {
    const ix: Instruction = {
      programAddress: programId,
      accounts: [
        { address: authorityAddr, role: AccountRole.READONLY_SIGNER },
        { address: userTokenAccount, role: AccountRole.WRITABLE },
        { address: vaultTokenAccount, role: AccountRole.WRITABLE },
        { address: vaultStatePda, role: AccountRole.READONLY },
        { address: mintAddr, role: AccountRole.READONLY },
        { address: TOKEN_PROGRAM_ADDRESS, role: AccountRole.READONLY },
      ],
      data: encodeInstruction(1, WITHDRAW_AMOUNT),
    };

    const tx = await buildTx(svm, authorityAddr, [authorityKeys], ix);
    await sendAndConfirm(svm, tx, "withdraw");

    expect(readTokenBalance(svm, vaultTokenAccount)).to.equal(
      DEPOSIT_AMOUNT - WITHDRAW_AMOUNT,
    );
    expect(readTokenBalance(svm, userTokenAccount)).to.equal(WITHDRAW_AMOUNT);
  });

  it("rejects withdrawal from wrong authority", async () => {
    const wrongKeys = await generateKeyPair();
    const wrongAddr = await getAddressFromPublicKey(wrongKeys.publicKey);
    svm.airdrop(wrongAddr, lamports(1n * LAMPORTS_PER_SOL));

    const [wrongUserToken] = await findAssociatedTokenPda({
      owner: wrongAddr,
      tokenProgram: TOKEN_PROGRAM_ADDRESS,
      mint: mintAddr,
    });

    const createWrongAtaIx = getCreateAssociatedTokenInstruction({
      payer: createNoopSigner(wrongAddr),
      ata: wrongUserToken,
      owner: wrongAddr,
      mint: mintAddr,
    });
    const setupTx = await buildTx(svm, wrongAddr, [wrongKeys], createWrongAtaIx);
    await sendAndConfirm(svm, setupTx);

    const ix: Instruction = {
      programAddress: programId,
      accounts: [
        { address: wrongAddr, role: AccountRole.READONLY_SIGNER },
        { address: wrongUserToken, role: AccountRole.WRITABLE },
        { address: vaultTokenAccount, role: AccountRole.WRITABLE },
        { address: vaultStatePda, role: AccountRole.READONLY },
        { address: mintAddr, role: AccountRole.READONLY },
        { address: TOKEN_PROGRAM_ADDRESS, role: AccountRole.READONLY },
      ],
      data: encodeInstruction(1, WITHDRAW_AMOUNT),
    };

    const tx = await buildTx(svm, wrongAddr, [wrongKeys], ix);
    const result = svm.sendTransaction(tx);
    expect(result).to.be.instanceOf(FailedTransactionMetadata);
  });
});

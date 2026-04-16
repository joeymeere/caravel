import { expect } from "chai";
import { LiteSVM, FailedTransactionMetadata, TransactionMetadata } from "litesvm";
import {
  address,
  type Address,
  AccountRole,
  appendTransactionMessageInstruction,
  appendTransactionMessageInstructions,
  compileTransaction,
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
import * as fs from "fs";
import * as path from "path";
import { fileURLToPath } from "url";
const __dirname = path.dirname(fileURLToPath(import.meta.url));

const LAMPORTS_PER_SOL = 1_000_000_000n;
const SYSTEM_PROGRAM = address("11111111111111111111111111111111");
const TOKEN_PROGRAM_ID = address("TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA");
const ASSOCIATED_TOKEN_PROGRAM_ID = address("ATokenGPvbdGVxr1b2hvZbsiqW5xWH25efTNsLJA8knL");
const SYSVAR_RENT = address("SysvarRent111111111111111111111111111111111");
const MINT_SIZE = 82;

async function sendAndConfirm(
  svm: LiteSVM,
  tx: Transaction,
  label?: string,
): Promise<TransactionMetadata> {
  const result = svm.sendTransaction(tx);
  if (result instanceof FailedTransactionMetadata) {
    throw new Error(`Transaction failed: ${result.toString()}`);
  }
  result.logs().forEach((log) => console.log(`      ${log}`));
  if (label) {
    console.log(`      ${label}: ${result.computeUnitsConsumed()} CU`);
  }
  return result;
}

async function buildTx(
  svm: LiteSVM,
  feePayer: Address,
  signerKeys: CryptoKeyPair[],
  ix: Instruction,
): Promise<Transaction> {
  const msg = pipe(
    createTransactionMessage({ version: "legacy" }),
    (m) => setTransactionMessageFeePayer(feePayer, m),
    (m) => svm.setTransactionMessageLifetimeUsingLatestBlockhash(m),
    (m) => appendTransactionMessageInstruction(ix, m),
  );
  const compiled = compileTransaction(msg);
  return await signTransaction(signerKeys, compiled);
}

async function buildMultiIxTx(
  svm: LiteSVM,
  feePayer: Address,
  signerKeys: CryptoKeyPair[],
  ixs: Instruction[],
): Promise<Transaction> {
  const msg = pipe(
    createTransactionMessage({ version: "legacy" }),
    (m) => setTransactionMessageFeePayer(feePayer, m),
    (m) => svm.setTransactionMessageLifetimeUsingLatestBlockhash(m),
    (m) => appendTransactionMessageInstructions(ixs, m),
  );
  const compiled = compileTransaction(msg);
  return await signTransaction(signerKeys, compiled);
}

async function getAssociatedTokenAddress(
  mint: Address,
  owner: Address,
  allowOwnerOffCurve: boolean = false,
): Promise<Address> {
  const encoder = getAddressEncoder();
  const [ata] = await getProgramDerivedAddress({
    programAddress: ASSOCIATED_TOKEN_PROGRAM_ID,
    seeds: [
      encoder.encode(owner),
      encoder.encode(TOKEN_PROGRAM_ID),
      encoder.encode(mint),
    ],
  });
  return ata;
}

function createAccountIx(
  fromPubkey: Address,
  newAccountPubkey: Address,
  lamportsAmount: bigint,
  space: bigint,
  owner: Address,
): Instruction {
  const buf = new ArrayBuffer(52);
  const view = new DataView(buf);
  view.setUint32(0, 0, true);
  view.setBigUint64(4, lamportsAmount, true);
  view.setBigUint64(12, space, true);
  const data = new Uint8Array(buf);
  const encoder = getAddressEncoder();
  data.set(encoder.encode(owner), 20);

  return {
    programAddress: SYSTEM_PROGRAM,
    accounts: [
      { address: fromPubkey, role: AccountRole.WRITABLE_SIGNER },
      { address: newAccountPubkey, role: AccountRole.WRITABLE_SIGNER },
    ],
    data,
  };
}

function initializeMintIx(
  mint: Address,
  decimals: number,
  mintAuthority: Address,
  freezeAuthority: Address | null,
): Instruction {
  const data = new Uint8Array(67);
  const encoder = getAddressEncoder();
  data[0] = 0; 
  data[1] = decimals;
  data.set(encoder.encode(mintAuthority), 2);
  if (freezeAuthority) {
    data[34] = 1;
    data.set(encoder.encode(freezeAuthority), 35);
  } else {
    data[34] = 0;
  }

  return {
    programAddress: TOKEN_PROGRAM_ID,
    accounts: [
      { address: mint, role: AccountRole.WRITABLE },
      { address: SYSVAR_RENT, role: AccountRole.READONLY },
    ],
    data,
  };
}

function createAssociatedTokenAccountIx(
  payer: Address,
  associatedToken: Address,
  owner: Address,
  mint: Address,
): Instruction {
  return {
    programAddress: ASSOCIATED_TOKEN_PROGRAM_ID,
    accounts: [
      { address: payer, role: AccountRole.WRITABLE_SIGNER },
      { address: associatedToken, role: AccountRole.WRITABLE },
      { address: owner, role: AccountRole.READONLY },
      { address: mint, role: AccountRole.READONLY },
      { address: SYSTEM_PROGRAM, role: AccountRole.READONLY },
      { address: TOKEN_PROGRAM_ID, role: AccountRole.READONLY },
    ],
    data: new Uint8Array(0),
  };
}

function mintToIx(
  mint: Address,
  destination: Address,
  authority: Address,
  amount: bigint,
): Instruction {
  const buf = new ArrayBuffer(9);
  const view = new DataView(buf);
  view.setUint8(0, 7);
  view.setBigUint64(1, amount, true);

  return {
    programAddress: TOKEN_PROGRAM_ID,
    accounts: [
      { address: mint, role: AccountRole.WRITABLE },
      { address: destination, role: AccountRole.WRITABLE },
      { address: authority, role: AccountRole.READONLY_SIGNER },
    ],
    data: new Uint8Array(buf),
  };
}

function readTokenBalance(svm: LiteSVM, account: Address): bigint {
  const info = svm.getAccount(account);
  if (!info.exists) throw new Error(`Account ${account} not found`);
  const view = new DataView(info.data.buffer, info.data.byteOffset, info.data.byteLength);
  return view.getBigUint64(64, true);
}

/** Read the mint supply (supply field at offset 36) from a mint account. */
function readMintSupply(svm: LiteSVM, mint: Address): bigint {
  const info = svm.getAccount(mint);
  if (!info.exists) throw new Error(`Account ${mint} not found`);
  const view = new DataView(info.data.buffer, info.data.byteOffset, info.data.byteLength);
  return view.getBigUint64(36, true);
}

describe("AMM Program", () => {
  const programPath = path.join(__dirname, "..", "build", "program.so");

  let programId: Address;
  let svm: LiteSVM;
  let userKeys: CryptoKeyPair;
  let userAddr: Address;
  let mintAKeys: CryptoKeyPair;
  let mintAAddr: Address;
  let mintBKeys: CryptoKeyPair;
  let mintBAddr: Address;
  let lpMintKeys: CryptoKeyPair;
  let lpMintAddr: Address;
  let poolPda: Address;
  let poolBump: number;
  let vaultA: Address;
  let vaultB: Address;
  let userTokenA: Address;
  let userTokenB: Address;
  let userLp: Address;

  before(async () => {
    const programKeys = await generateKeyPair();
    programId = await getAddressFromPublicKey(programKeys.publicKey);

    svm = new LiteSVM().withDefaultPrograms();
    svm.addProgram(programId, fs.readFileSync(programPath));

    userKeys = await generateKeyPair();
    userAddr = await getAddressFromPublicKey(userKeys.publicKey);
    svm.airdrop(userAddr, lamports(100n * LAMPORTS_PER_SOL));

    mintAKeys = await generateKeyPair();
    mintAAddr = await getAddressFromPublicKey(mintAKeys.publicKey);
    mintBKeys = await generateKeyPair();
    mintBAddr = await getAddressFromPublicKey(mintBKeys.publicKey);
    lpMintKeys = await generateKeyPair();
    lpMintAddr = await getAddressFromPublicKey(lpMintKeys.publicKey);

    const encoder = getAddressEncoder();

    const [pdaAddr, bump] = await getProgramDerivedAddress({
      programAddress: programId,
      seeds: [
        new TextEncoder().encode("amm"),
        encoder.encode(mintAAddr),
        encoder.encode(mintBAddr),
      ],
    });
    poolPda = pdaAddr;
    poolBump = bump;

    vaultA = await getAssociatedTokenAddress(mintAAddr, poolPda, true);
    vaultB = await getAssociatedTokenAddress(mintBAddr, poolPda, true);
    userTokenA = await getAssociatedTokenAddress(mintAAddr, userAddr);
    userTokenB = await getAssociatedTokenAddress(mintBAddr, userAddr);
    userLp = await getAssociatedTokenAddress(lpMintAddr, userAddr);

    const rentExempt = BigInt(1_461_600);

    let tx = await buildMultiIxTx(svm, userAddr, [userKeys, mintAKeys], [
      createAccountIx(userAddr, mintAAddr, rentExempt, BigInt(MINT_SIZE), TOKEN_PROGRAM_ID),
      initializeMintIx(mintAAddr, 6, userAddr, null),
    ]);
    await sendAndConfirm(svm, tx);

    tx = await buildMultiIxTx(svm, userAddr, [userKeys, mintBKeys], [
      createAccountIx(userAddr, mintBAddr, rentExempt, BigInt(MINT_SIZE), TOKEN_PROGRAM_ID),
      initializeMintIx(mintBAddr, 6, userAddr, null),
    ]);
    await sendAndConfirm(svm, tx);

    tx = await buildMultiIxTx(svm, userAddr, [userKeys, lpMintKeys], [
      createAccountIx(userAddr, lpMintAddr, rentExempt, BigInt(MINT_SIZE), TOKEN_PROGRAM_ID),
      initializeMintIx(lpMintAddr, 6, poolPda, null),
    ]);
    await sendAndConfirm(svm, tx);

    tx = await buildMultiIxTx(svm, userAddr, [userKeys], [
      createAssociatedTokenAccountIx(userAddr, userTokenA, userAddr, mintAAddr),
      createAssociatedTokenAccountIx(userAddr, userTokenB, userAddr, mintBAddr),
      createAssociatedTokenAccountIx(userAddr, vaultA, poolPda, mintAAddr),
      createAssociatedTokenAccountIx(userAddr, vaultB, poolPda, mintBAddr),
      createAssociatedTokenAccountIx(userAddr, userLp, userAddr, lpMintAddr),
    ]);
    await sendAndConfirm(svm, tx);

    const INITIAL = 10_000_000_000_000_000n;
    tx = await buildMultiIxTx(svm, userAddr, [userKeys], [
      mintToIx(mintAAddr, userTokenA, userAddr, INITIAL),
      mintToIx(mintBAddr, userTokenB, userAddr, INITIAL),
    ]);
    await sendAndConfirm(svm, tx);
  });

  it("initializes the pool", async () => {
    const data = new Uint8Array(1);
    data[0] = 0; // disc = 0

    const ix: Instruction = {
      programAddress: programId,
      accounts: [
        { address: userAddr, role: AccountRole.WRITABLE_SIGNER },
        { address: poolPda, role: AccountRole.WRITABLE },
        { address: vaultA, role: AccountRole.READONLY },
        { address: vaultB, role: AccountRole.READONLY },
        { address: lpMintAddr, role: AccountRole.READONLY },
        { address: mintAAddr, role: AccountRole.READONLY },
        { address: mintBAddr, role: AccountRole.READONLY },
        { address: SYSTEM_PROGRAM, role: AccountRole.READONLY },
      ],
      data,
    };

    const tx = await buildTx(svm, userAddr, [userKeys], ix);
    await sendAndConfirm(svm, tx, "initialize_pool");

    const account = svm.getAccount(poolPda);
    expect(account.exists).to.be.true;
    if (account.exists) {
      const stateData = account.data;
      const encoder = getAddressEncoder();

      expect(stateData[0]).to.equal(1); // is_initialized
      expect(stateData[1]).to.equal(poolBump); // bump
      expect(Buffer.from(stateData.subarray(8, 40))).to.deep.equal(
        Buffer.from(encoder.encode(mintAAddr))
      );
      expect(Buffer.from(stateData.subarray(40, 72))).to.deep.equal(
        Buffer.from(encoder.encode(mintBAddr))
      );
    }
  });

  it("adds initial liquidity (100T A + 400T B)", async () => {
    const buf = new ArrayBuffer(25);
    const view = new DataView(buf);
    view.setUint8(0, 1); // disc = 1 (add_liquidity)
    view.setBigUint64(1, 100_000_000_000_000n, true); // amount_a
    view.setBigUint64(9, 400_000_000_000_000n, true); // amount_b
    view.setBigUint64(17, 0n, true); // min_lp

    const ix: Instruction = {
      programAddress: programId,
      accounts: [
        { address: userAddr, role: AccountRole.READONLY_SIGNER },
        { address: poolPda, role: AccountRole.READONLY },
        { address: vaultA, role: AccountRole.WRITABLE },
        { address: vaultB, role: AccountRole.WRITABLE },
        { address: userTokenA, role: AccountRole.WRITABLE },
        { address: userTokenB, role: AccountRole.WRITABLE },
        { address: lpMintAddr, role: AccountRole.WRITABLE },
        { address: userLp, role: AccountRole.WRITABLE },
        { address: TOKEN_PROGRAM_ID, role: AccountRole.READONLY },
      ],
      data: new Uint8Array(buf),
    };

    const tx = await buildTx(svm, userAddr, [userKeys], ix);
    await sendAndConfirm(svm, tx, "add_liquidity");

    expect(readTokenBalance(svm, userLp)).to.equal(200_000_000_000_000n);
    expect(readTokenBalance(svm, vaultA)).to.equal(100_000_000_000_000n);
    expect(readTokenBalance(svm, vaultB)).to.equal(400_000_000_000_000n);
  });

  it("swaps 10T A -> B", async () => {
    const buf = new ArrayBuffer(18);
    const view = new DataView(buf);
    view.setUint8(0, 3); // disc = 3 (swap)
    view.setBigUint64(1, 10_000_000_000_000n, true); // amount_in
    view.setBigUint64(9, 0n, true); // min_amount_out
    view.setUint8(17, 0); // direction = 0 (A->B)

    const ix: Instruction = {
      programAddress: programId,
      accounts: [
        { address: userAddr, role: AccountRole.READONLY_SIGNER },
        { address: poolPda, role: AccountRole.READONLY },
        { address: vaultA, role: AccountRole.WRITABLE },
        { address: vaultB, role: AccountRole.WRITABLE },
        { address: userTokenA, role: AccountRole.WRITABLE },
        { address: userTokenB, role: AccountRole.WRITABLE },
        { address: TOKEN_PROGRAM_ID, role: AccountRole.READONLY },
      ],
      data: new Uint8Array(buf),
    };

    const tx = await buildTx(svm, userAddr, [userKeys], ix);
    await sendAndConfirm(svm, tx, "swap A->B");

    expect(readTokenBalance(svm, vaultA)).to.equal(110_000_000_000_000n);
    expect(readTokenBalance(svm, vaultB)).to.equal(363_735_564_244_795n);
  });

  it("swaps 20T B -> A", async () => {
    const buf = new ArrayBuffer(18);
    const view = new DataView(buf);
    view.setUint8(0, 3);
    view.setBigUint64(1, 20_000_000_000_000n, true);
    view.setBigUint64(9, 0n, true);
    view.setUint8(17, 1); // direction = 1 (B->A)

    const ix: Instruction = {
      programAddress: programId,
      accounts: [
        { address: userAddr, role: AccountRole.READONLY_SIGNER },
        { address: poolPda, role: AccountRole.READONLY },
        { address: vaultA, role: AccountRole.WRITABLE },
        { address: vaultB, role: AccountRole.WRITABLE },
        { address: userTokenA, role: AccountRole.WRITABLE },
        { address: userTokenB, role: AccountRole.WRITABLE },
        { address: TOKEN_PROGRAM_ID, role: AccountRole.READONLY },
      ],
      data: new Uint8Array(buf),
    };

    const tx = await buildTx(svm, userAddr, [userKeys], ix);
    await sendAndConfirm(svm, tx, "swap B->A");

    expect(readTokenBalance(svm, vaultA)).to.equal(104_283_190_788_245n);
    expect(readTokenBalance(svm, vaultB)).to.equal(383_735_564_244_795n);
  });

  it("removes 50T LP", async () => {
    const buf = new ArrayBuffer(25);
    const view = new DataView(buf);
    view.setUint8(0, 2); // disc = 2 (remove_liquidity)
    view.setBigUint64(1, 50_000_000_000_000n, true); // lp_amount
    view.setBigUint64(9, 0n, true); // min_a
    view.setBigUint64(17, 0n, true); // min_b

    const ix: Instruction = {
      programAddress: programId,
      accounts: [
        { address: userAddr, role: AccountRole.READONLY_SIGNER },
        { address: poolPda, role: AccountRole.READONLY },
        { address: vaultA, role: AccountRole.WRITABLE },
        { address: vaultB, role: AccountRole.WRITABLE },
        { address: userTokenA, role: AccountRole.WRITABLE },
        { address: userTokenB, role: AccountRole.WRITABLE },
        { address: lpMintAddr, role: AccountRole.WRITABLE },
        { address: userLp, role: AccountRole.WRITABLE },
        { address: TOKEN_PROGRAM_ID, role: AccountRole.READONLY },
      ],
      data: new Uint8Array(buf),
    };

    const tx = await buildTx(svm, userAddr, [userKeys], ix);
    await sendAndConfirm(svm, tx, "remove_liquidity");

    expect(readTokenBalance(svm, vaultA)).to.equal(78_212_393_091_184n);
    expect(readTokenBalance(svm, vaultB)).to.equal(287_801_673_183_597n);
    expect(readMintSupply(svm, lpMintAddr)).to.equal(150_000_000_000_000n);
  });

  it("rejects swap with excessive slippage", async () => {
    const buf = new ArrayBuffer(18);
    const view = new DataView(buf);
    view.setUint8(0, 3);
    view.setBigUint64(1, 10_000_000_000_000n, true);
    view.setBigUint64(9, 999_999_999_999_999n, true); // this aint gonna happen
    view.setUint8(17, 0);

    const ix: Instruction = {
      programAddress: programId,
      accounts: [
        { address: userAddr, role: AccountRole.READONLY_SIGNER },
        { address: poolPda, role: AccountRole.READONLY },
        { address: vaultA, role: AccountRole.WRITABLE },
        { address: vaultB, role: AccountRole.WRITABLE },
        { address: userTokenA, role: AccountRole.WRITABLE },
        { address: userTokenB, role: AccountRole.WRITABLE },
        { address: TOKEN_PROGRAM_ID, role: AccountRole.READONLY },
      ],
      data: new Uint8Array(buf),
    };

    const tx = await buildTx(svm, userAddr, [userKeys], ix);
    const result = svm.sendTransaction(tx);
    expect(result).to.be.instanceOf(FailedTransactionMetadata);
  });
});

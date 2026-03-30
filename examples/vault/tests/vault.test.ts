import { expect } from "chai";
import { LiteSVM, FailedTransactionMetadata, TransactionMetadata } from "litesvm";
import {
  address,
  Address,
  AccountRole,
  appendTransactionMessageInstruction,
  compileTransaction,
  createTransactionMessage,
  generateKeyPair,
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

const LAMPORTS_PER_SOL = 1_000_000_000n;
const SYSTEM_PROGRAM = address("11111111111111111111111111111111");
const PROGRAM_ID = address("33333333333333333333333333333333333333333333");

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

function encodeAmount(disc: number, amount: bigint): Uint8Array {
  const buf = new ArrayBuffer(9);
  const view = new DataView(buf);
  view.setUint8(0, disc);
  view.setBigUint64(1, amount, true);
  return new Uint8Array(buf);
}

describe("Vault Program", () => {
  const programPath = path.join(__dirname, "..", "build", "program.so");

  let programId: Address;
  let svm: LiteSVM;
  let userKeys: CryptoKeyPair;
  let userAddr: Address;
  let vaultPda: Address;

  const DEPOSIT_AMOUNT = lamports(1n * LAMPORTS_PER_SOL);
  const WITHDRAW_AMOUNT = lamports(LAMPORTS_PER_SOL / 2n);

  before(async () => {
    svm = new LiteSVM();
    svm.addProgram(PROGRAM_ID, fs.readFileSync(programPath));

    userKeys = await generateKeyPair();
    userAddr = await getAddressFromPublicKey(userKeys.publicKey);
    svm.airdrop(userAddr, lamports(10n * LAMPORTS_PER_SOL));

    const { getAddressEncoder } = await import("@solana/kit");
    const userPubkeyBytes = getAddressEncoder().encode(userAddr);

    const [pdaAddr] = await getProgramDerivedAddress({
      programAddress: PROGRAM_ID,
      seeds: [new TextEncoder().encode("vault"), userPubkeyBytes],
    });
    vaultPda = pdaAddr;

    svm.setAccount({
      address: vaultPda,
      lamports: lamports(BigInt(svm.minimumBalanceForRentExemption(0n))),
      data: new Uint8Array(0),
      programAddress: PROGRAM_ID,
      executable: false,
      space: 0n,
    });
  });

  it("deposits SOL into the vault (initial)", async () => {
    const ix: Instruction = {
      programAddress: PROGRAM_ID,
      accounts: [
        { address: userAddr, role: AccountRole.WRITABLE_SIGNER },
        { address: vaultPda, role: AccountRole.WRITABLE },
        { address: SYSTEM_PROGRAM, role: AccountRole.READONLY },
      ],
      data: encodeAmount(0, DEPOSIT_AMOUNT as unknown as bigint),
    };

    const tx = await buildTx(svm, userAddr, [userKeys], ix);
    await sendAndConfirm(svm, tx, "deposit (initial)");

    const acct = svm.getAccount(vaultPda);
    expect(acct.exists).to.be.true;
    if (acct.exists) {
      expect(BigInt(acct.lamports) >= BigInt(DEPOSIT_AMOUNT)).to.be.true;
    }
  });

  it("deposits SOL into the vault (subsequent)", async () => {
    const secondDeposit = lamports(2n * LAMPORTS_PER_SOL);
    const ix: Instruction = {
      programAddress: PROGRAM_ID,
      accounts: [
        { address: userAddr, role: AccountRole.WRITABLE_SIGNER },
        { address: vaultPda, role: AccountRole.WRITABLE },
        { address: SYSTEM_PROGRAM, role: AccountRole.READONLY },
      ],
      data: encodeAmount(0, secondDeposit as unknown as bigint),
    };

    const tx = await buildTx(svm, userAddr, [userKeys], ix);
    await sendAndConfirm(svm, tx, "deposit (subsequent)");

    const acct = svm.getAccount(vaultPda);
    if (acct.exists) {
      expect(
        BigInt(acct.lamports) >=
          BigInt(DEPOSIT_AMOUNT) + BigInt(secondDeposit),
      ).to.be.true;
    }
  });

  it("withdraws SOL from the vault", async () => {
    const vaultBefore = svm.getBalance(vaultPda)!;

    const ix: Instruction = {
      programAddress: PROGRAM_ID,
      accounts: [
        { address: userAddr, role: AccountRole.WRITABLE_SIGNER },
        { address: vaultPda, role: AccountRole.WRITABLE },
      ],
      data: encodeAmount(1, WITHDRAW_AMOUNT as unknown as bigint),
    };

    const tx = await buildTx(svm, userAddr, [userKeys], ix);
    await sendAndConfirm(svm, tx, "withdraw");

    const vaultAfter = svm.getBalance(vaultPda)!;
    expect(BigInt(vaultBefore) - BigInt(vaultAfter)).to.equal(
      BigInt(WITHDRAW_AMOUNT),
    );
  });

  it("rejects withdrawal from wrong authority", async () => {
    const wrongKeys = await generateKeyPair();
    const wrongAddr = await getAddressFromPublicKey(wrongKeys.publicKey);
    svm.airdrop(wrongAddr, lamports(1n * LAMPORTS_PER_SOL));

    const ix: Instruction = {
      programAddress: PROGRAM_ID,
      accounts: [
        { address: wrongAddr, role: AccountRole.WRITABLE_SIGNER },
        { address: vaultPda, role: AccountRole.WRITABLE },
      ],
      data: encodeAmount(1, WITHDRAW_AMOUNT as unknown as bigint),
    };

    const tx = await buildTx(svm, wrongAddr, [wrongKeys], ix);
    const result = svm.sendTransaction(tx);
    expect(result).to.be.instanceOf(FailedTransactionMetadata);
  });
});

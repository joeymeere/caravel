import { expect } from "chai";
import { LiteSVM, FailedTransactionMetadata, TransactionMetadata } from "litesvm";
import {
  AccountRole,
  address,
  type Address,
  appendTransactionMessageInstruction,
  compileTransaction,
  createTransactionMessage,
  createKeyPairFromPrivateKeyBytes,
  getAddressEncoder,
  getAddressFromPublicKey,
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

// Deterministic keypairs: PDA bump searches (and so compute units) depend on
// addresses, so fixed keys keep CU numbers comparable run to run.
let keySeed = 0;
const generateKeyPair = (): Promise<CryptoKeyPair> =>
  createKeyPairFromPrivateKeyBytes(new Uint8Array(32).fill(++keySeed));
const __dirname = path.dirname(fileURLToPath(import.meta.url));

const SYSTEM_PROGRAM = address("11111111111111111111111111111111");

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
  return await signTransaction(signerKeys, compileTransaction(msg));
}

function disc(d: number): Uint8Array {
  return new Uint8Array([d]);
}

const INVALID_ARGUMENT = 1;
const ERROR_ACCOUNT_NOT_EXECUTABLE = 0x100 + 6;
const ERROR_DUPLICATE_ACCOUNT = 0x100 + 7;

function ixError(result: unknown): number | { custom: number } {
  expect(result).to.be.instanceOf(FailedTransactionMetadata);
  const txErr = (result as FailedTransactionMetadata).err() as any;
  expect(txErr, "expected an InstructionError").to.have.property("index");
  const err = txErr.err();
  return typeof err === "number" ? err : { custom: err.code };
}

describe("Counter Program", () => {
  const programPath = path.join(__dirname, "..", "build", "program.so");

  let programId: Address;
  let svm: LiteSVM;
  let payerKeys: CryptoKeyPair;
  let payerAddr: Address;
  let counterKeys: CryptoKeyPair;
  let counterAddr: Address;

  before(async () => {
    const programKeys = await generateKeyPair();
    programId = await getAddressFromPublicKey(programKeys.publicKey);

    svm = new LiteSVM();
    svm.addProgram(programId, fs.readFileSync(programPath));

    payerKeys = await generateKeyPair();
    payerAddr = await getAddressFromPublicKey(payerKeys.publicKey);
    svm.airdrop(payerAddr, lamports(10_000_000_000n));

    counterKeys = await generateKeyPair();
    counterAddr = await getAddressFromPublicKey(counterKeys.publicKey);
  });

  it("initializes a counter", async () => {
    const ix: Instruction = {
      programAddress: programId,
      accounts: [
        { address: payerAddr, role: AccountRole.WRITABLE_SIGNER },
        { address: counterAddr, role: AccountRole.WRITABLE_SIGNER },
        { address: SYSTEM_PROGRAM, role: AccountRole.READONLY },
      ],
      data: disc(0),
    };

    const tx = await buildTx(svm, payerAddr, [payerKeys, counterKeys], ix);
    const result = svm.sendTransaction(tx);
    if (result instanceof FailedTransactionMetadata)
      throw new Error(`Transaction failed: ${result.toString()}`);
    result.logs().forEach((l) => console.log(`      ${l}`));
    console.log(`      initialize: ${result.computeUnitsConsumed()} CU`);

    const acct = svm.getAccount(counterAddr);
    expect(acct.exists).to.be.true;
    if (acct.exists) {
      const data = acct.data;
      const count = new DataView(data.buffer, data.byteOffset).getBigUint64(0, true);
      expect(count).to.equal(0n);

      const encoder = getAddressEncoder();
      const authorityBytes = data.slice(8, 40);
      const payerBytes = encoder.encode(payerAddr);
      expect(Buffer.from(authorityBytes).equals(Buffer.from(payerBytes))).to.be.true;
    }
  });

  it("increments the counter", async () => {
    const ix: Instruction = {
      programAddress: programId,
      accounts: [
        { address: counterAddr, role: AccountRole.WRITABLE },
        { address: payerAddr, role: AccountRole.READONLY_SIGNER },
      ],
      data: disc(1),
    };

    const tx = await buildTx(svm, payerAddr, [payerKeys], ix);
    const result = svm.sendTransaction(tx);
    if (result instanceof FailedTransactionMetadata)
      throw new Error(`Transaction failed: ${result.toString()}`);
    result.logs().forEach((l) => console.log(`      ${l}`));
    console.log(`      increment: ${result.computeUnitsConsumed()} CU`);

    const acct = svm.getAccount(counterAddr);
    if (acct.exists) {
      const count = new DataView(acct.data.buffer, acct.data.byteOffset).getBigUint64(0, true);
      expect(count).to.equal(1n);
    }
  });

  it("decrements the counter", async () => {
    const ix: Instruction = {
      programAddress: programId,
      accounts: [
        { address: counterAddr, role: AccountRole.WRITABLE },
        { address: payerAddr, role: AccountRole.READONLY_SIGNER },
      ],
      data: disc(2),
    };

    const tx = await buildTx(svm, payerAddr, [payerKeys], ix);
    const result = svm.sendTransaction(tx);
    if (result instanceof FailedTransactionMetadata)
      throw new Error(`Transaction failed: ${result.toString()}`);
    result.logs().forEach((l) => console.log(`      ${l}`));
    console.log(`      decrement: ${result.computeUnitsConsumed()} CU`);

    const acct = svm.getAccount(counterAddr);
    if (acct.exists) {
      const count = new DataView(acct.data.buffer, acct.data.byteOffset).getBigUint64(0, true);
      expect(count).to.equal(0n);
    }
  });

  it("rejects decrement when counter is zero", async () => {
    const ix: Instruction = {
      programAddress: programId,
      accounts: [
        { address: counterAddr, role: AccountRole.WRITABLE },
        { address: payerAddr, role: AccountRole.READONLY_SIGNER },
      ],
      data: disc(2),
    };


    svm.expireBlockhash();
    const tx = await buildTx(svm, payerAddr, [payerKeys], ix);
    const result = svm.sendTransaction(tx);

    expect(ixError(result)).to.equal(INVALID_ARGUMENT);
  });

  it("rejects a non-executable account in a PROGRAM slot", async () => {
    const fakeKeys = await generateKeyPair();
    const fakeAddr = await getAddressFromPublicKey(fakeKeys.publicKey);
    const newCounterKeys = await generateKeyPair();
    const newCounterAddr = await getAddressFromPublicKey(newCounterKeys.publicKey);

    const ix: Instruction = {
      programAddress: programId,
      accounts: [
        { address: payerAddr, role: AccountRole.WRITABLE_SIGNER },
        { address: newCounterAddr, role: AccountRole.WRITABLE_SIGNER },
        { address: fakeAddr, role: AccountRole.READONLY },
      ],
      data: disc(0),
    };

    const tx = await buildTx(svm, payerAddr, [payerKeys, newCounterKeys], ix);
    const result = svm.sendTransaction(tx);
    expect(ixError(result)).to.deep.equal({ custom: ERROR_ACCOUNT_NOT_EXECUTABLE });
  });

  it("rejects duplicate accounts", async () => {
    const ix: Instruction = {
      programAddress: programId,
      accounts: [
        { address: payerAddr, role: AccountRole.WRITABLE_SIGNER },
        { address: payerAddr, role: AccountRole.WRITABLE_SIGNER },
      ],
      data: disc(1),
    };

    const tx = await buildTx(svm, payerAddr, [payerKeys], ix);
    const result = svm.sendTransaction(tx);
    expect(ixError(result)).to.deep.equal({ custom: ERROR_DUPLICATE_ACCOUNT });
  });

  it("rejects increment from wrong authority", async () => {
    const wrongKeys = await generateKeyPair();
    const wrongAddr = await getAddressFromPublicKey(wrongKeys.publicKey);
    svm.airdrop(wrongAddr, lamports(1_000_000_000n));

    const ix: Instruction = {
      programAddress: programId,
      accounts: [
        { address: counterAddr, role: AccountRole.WRITABLE },
        { address: wrongAddr, role: AccountRole.READONLY_SIGNER },
      ],
      data: disc(1),
    };

    const tx = await buildTx(svm, wrongAddr, [wrongKeys], ix);
    const result = svm.sendTransaction(tx);
    expect(ixError(result)).to.equal(INVALID_ARGUMENT);
  });
});

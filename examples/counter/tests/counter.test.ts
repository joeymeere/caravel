import { expect } from "chai";
import { LiteSVM, FailedTransactionMetadata } from "litesvm";
import {
  Keypair,
  PublicKey,
  SystemProgram,
  Transaction,
  TransactionInstruction,
} from "@solana/web3.js";
import * as fs from "fs";
import * as path from "path";

function sendAndConfirm(svm: LiteSVM, tx: Transaction): void {
  const result = svm.sendTransaction(tx);
  if (result instanceof FailedTransactionMetadata) {
    throw new Error(`Transaction failed: ${result.toString()}`);
  }

  result.logs().forEach(log => {
    console.log(`      ${log}`);
  });
}

describe("Counter Program", () => {
  const programPath = path.join(__dirname, "..", "build", "program.so");
  const programId = Keypair.generate().publicKey;

  let svm: LiteSVM;
  let payer: Keypair;
  let counterAccount: Keypair;

  before(() => {
    svm = new LiteSVM();
    svm.addProgram(programId, fs.readFileSync(programPath));

    payer = Keypair.generate();
    svm.airdrop(payer.publicKey, BigInt(10_000_000_000));

    counterAccount = Keypair.generate();
  });

  it("initializes a counter", () => {
    const data = Buffer.alloc(1);
    data.writeUInt8(0, 0); // discriminator = 0 (initialize)

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: payer.publicKey, isSigner: true, isWritable: true },
        { pubkey: counterAccount.publicKey, isSigner: true, isWritable: true },
        { pubkey: SystemProgram.programId, isSigner: false, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(payer, counterAccount);

    sendAndConfirm(svm, tx);

    const account = svm.getAccount(counterAccount.publicKey);
    expect(account).to.not.be.null;

    const accountData = Buffer.from(account!.data);
    const count = accountData.readBigUInt64LE(0);
    expect(count).to.equal(BigInt(0));

    const authority = new PublicKey(accountData.subarray(8, 40));
    expect(authority.equals(payer.publicKey)).to.be.true;
  });

  it("increments the counter", () => {
    const data = Buffer.alloc(1);
    data.writeUInt8(1, 0); // discriminator = 1 (increment)

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: counterAccount.publicKey, isSigner: false, isWritable: true },
        { pubkey: payer.publicKey, isSigner: true, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(payer);

    sendAndConfirm(svm, tx);

    const account = svm.getAccount(counterAccount.publicKey);
    const accountData = Buffer.from(account!.data);
    const count = accountData.readBigUInt64LE(0);
    expect(count).to.equal(BigInt(1));
  });

  it("decrements the counter", () => {
    const data = Buffer.alloc(1);
    data.writeUInt8(2, 0); // discriminator = 2 (decrement)

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: counterAccount.publicKey, isSigner: false, isWritable: true },
        { pubkey: payer.publicKey, isSigner: true, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(payer);

    sendAndConfirm(svm, tx);

    const account = svm.getAccount(counterAccount.publicKey);
    const accountData = Buffer.from(account!.data);
    const count = accountData.readBigUInt64LE(0);
    expect(count).to.equal(BigInt(0));
  });

  it("rejects decrement when counter is zero", () => {
    const data = Buffer.alloc(1);
    data.writeUInt8(2, 0); // discriminator = 2 (decrement)

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: counterAccount.publicKey, isSigner: false, isWritable: true },
        { pubkey: payer.publicKey, isSigner: true, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(payer);

    const result = svm.sendTransaction(tx);
    expect(result).to.be.instanceOf(FailedTransactionMetadata);
  });

  it("rejects increment from wrong authority", () => {
    const wrongAuthority = Keypair.generate();
    svm.airdrop(wrongAuthority.publicKey, BigInt(1_000_000_000));

    const data = Buffer.alloc(1);
    data.writeUInt8(1, 0); // discriminator = 1 (increment)

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: counterAccount.publicKey, isSigner: false, isWritable: true },
        { pubkey: wrongAuthority.publicKey, isSigner: true, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(wrongAuthority);

    const result = svm.sendTransaction(tx);
    expect(result).to.be.instanceOf(FailedTransactionMetadata);
  });
});

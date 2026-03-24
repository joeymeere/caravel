import { expect } from "chai";
import { LiteSVM, FailedTransactionMetadata, TransactionMetadata } from "litesvm";
import {
  Keypair,
  PublicKey,
  SystemProgram,
  Transaction,
  TransactionInstruction,
  LAMPORTS_PER_SOL,
} from "@solana/web3.js";
import * as fs from "fs";
import * as path from "path";

function sendAndConfirm(svm: LiteSVM, tx: Transaction, label?: string): TransactionMetadata {
  const result = svm.sendTransaction(tx);
  if (result instanceof FailedTransactionMetadata) {
    throw new Error(`Transaction failed: ${result.toString()}`);
  }

  result.logs().forEach(log => {
    console.log(`      ${log}`);
  });
  if (label) {
    console.log(`      ${label}: ${result.computeUnitsConsumed()} CU`);
  }
  return result;
}

describe("Vault Program", () => {
  const programPath = path.join(__dirname, "..", "build", "program.so");
  const programId = Keypair.generate().publicKey;

  let svm: LiteSVM;
  let user: Keypair;
  let vaultPda: PublicKey;
  let vaultBump: number;

  const DEPOSIT_AMOUNT = BigInt(1 * LAMPORTS_PER_SOL);
  const WITHDRAW_AMOUNT = BigInt(LAMPORTS_PER_SOL / 2);

  before(() => {
    svm = new LiteSVM();
    svm.addProgram(programId, fs.readFileSync(programPath));

    user = Keypair.generate();
    svm.airdrop(user.publicKey, BigInt(10 * LAMPORTS_PER_SOL));

    [vaultPda, vaultBump] = PublicKey.findProgramAddressSync(
      [Buffer.from("vault"), user.publicKey.toBuffer()],
      programId
    );

    svm.setAccount(vaultPda, {
      lamports: 0,
      data: new Uint8Array(0),
      owner: programId,
      executable: false,
    });
  });

  it("deposits SOL into the vault (initial)", () => {
    const data = Buffer.alloc(10);
    data.writeUInt8(0, 0);
    data.writeBigUInt64LE(DEPOSIT_AMOUNT, 1);
    data.writeUInt8(vaultBump, 9);

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: user.publicKey, isSigner: true, isWritable: true },
        { pubkey: vaultPda, isSigner: false, isWritable: true },
        { pubkey: SystemProgram.programId, isSigner: false, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(user);

    sendAndConfirm(svm, tx, "deposit (initial)");

    const vaultAccount = svm.getAccount(vaultPda);
    expect(vaultAccount).to.not.be.null;
    expect(BigInt(vaultAccount!.lamports) >= DEPOSIT_AMOUNT).to.be.true;
  });

  it("deposits SOL into the vault (subsequent)", () => {
    const secondDeposit = BigInt(2 * LAMPORTS_PER_SOL);
    const data = Buffer.alloc(10);
    data.writeUInt8(0, 0);
    data.writeBigUInt64LE(secondDeposit, 1);
    data.writeUInt8(vaultBump, 9);

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: user.publicKey, isSigner: true, isWritable: true },
        { pubkey: vaultPda, isSigner: false, isWritable: true },
        { pubkey: SystemProgram.programId, isSigner: false, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(user);

    sendAndConfirm(svm, tx, "deposit (subsequent)");

    const vaultAccount = svm.getAccount(vaultPda);
    expect(BigInt(vaultAccount!.lamports) >= DEPOSIT_AMOUNT + secondDeposit).to.be.true;
  });

  it("withdraws SOL from the vault", () => {
    const data = Buffer.alloc(10);
    data.writeUInt8(1, 0);
    data.writeBigUInt64LE(WITHDRAW_AMOUNT, 1);
    data.writeUInt8(vaultBump, 9);

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: user.publicKey, isSigner: true, isWritable: true },
        { pubkey: vaultPda, isSigner: false, isWritable: true },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(user);

    const vaultBefore = BigInt(svm.getAccount(vaultPda)!.lamports);
    sendAndConfirm(svm, tx, "withdraw");
    const vaultAfter = BigInt(svm.getAccount(vaultPda)!.lamports);

    expect(vaultBefore - vaultAfter).to.equal(WITHDRAW_AMOUNT);
  });

  it("rejects withdrawal from wrong authority", () => {
    const wrongUser = Keypair.generate();
    svm.airdrop(wrongUser.publicKey, BigInt(1 * LAMPORTS_PER_SOL));

    const data = Buffer.alloc(10);
    data.writeUInt8(1, 0);
    data.writeBigUInt64LE(WITHDRAW_AMOUNT, 1);
    data.writeUInt8(vaultBump, 9); // bump won't match for wrong user's derivation

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: wrongUser.publicKey, isSigner: true, isWritable: true },
        { pubkey: vaultPda, isSigner: false, isWritable: true },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(wrongUser);

    const result = svm.sendTransaction(tx);
    expect(result).to.be.instanceOf(FailedTransactionMetadata);
  });
});

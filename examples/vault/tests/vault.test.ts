import { expect } from "chai";
import { LiteSVM, FailedTransactionMetadata } from "litesvm";
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

function sendAndConfirm(svm: LiteSVM, tx: Transaction): void {
  const result = svm.sendTransaction(tx);
  if (result instanceof FailedTransactionMetadata) {
    throw new Error(`Transaction failed: ${result.toString()}`);
  }
}

describe("Vault Program", () => {
  const programPath = path.join(__dirname, "..", "build", "program.so");
  const programId = Keypair.generate().publicKey;

  let svm: LiteSVM;
  let user: Keypair;
  let vaultPda: PublicKey;
  let vaultBump: number;
  let vaultStatePda: PublicKey;
  let vaultStateBump: number;

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

    [vaultStatePda, vaultStateBump] = PublicKey.findProgramAddressSync(
      [Buffer.from("vault_state"), user.publicKey.toBuffer()],
      programId
    );
  });

  it("deposits SOL into the vault", () => {
    const data = Buffer.alloc(9);
    data.writeUInt8(0, 0); // discriminator = 0 (deposit)
    data.writeBigUInt64LE(DEPOSIT_AMOUNT, 1);

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: user.publicKey, isSigner: true, isWritable: true },
        { pubkey: vaultPda, isSigner: false, isWritable: true },
        { pubkey: vaultStatePda, isSigner: false, isWritable: true },
        { pubkey: SystemProgram.programId, isSigner: false, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(user);

    sendAndConfirm(svm, tx);

    const vaultAccount = svm.getAccount(vaultPda);
    expect(vaultAccount).to.not.be.null;
    expect(BigInt(vaultAccount!.lamports) >= DEPOSIT_AMOUNT).to.be.true;

    // Verify vault_state was initialized
    const stateAccount = svm.getAccount(vaultStatePda);
    expect(stateAccount).to.not.be.null;
    const stateData = Buffer.from(stateAccount!.data);
    const authority = new PublicKey(stateData.subarray(0, 32));
    expect(authority.equals(user.publicKey)).to.be.true;
  });

  it("withdraws SOL from the vault", () => {
    const userBefore = svm.getAccount(user.publicKey);
    const userLamportsBefore = BigInt(userBefore!.lamports);

    const data = Buffer.alloc(9);
    data.writeUInt8(1, 0); // discriminator = 1 (withdraw)
    data.writeBigUInt64LE(WITHDRAW_AMOUNT, 1);

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: user.publicKey, isSigner: true, isWritable: true },
        { pubkey: vaultPda, isSigner: false, isWritable: true },
        { pubkey: vaultStatePda, isSigner: false, isWritable: false },
        { pubkey: SystemProgram.programId, isSigner: false, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(user);

    sendAndConfirm(svm, tx);

    const userAfter = svm.getAccount(user.publicKey);
    const userLamportsAfter = BigInt(userAfter!.lamports);

    expect(userLamportsAfter > userLamportsBefore - BigInt(10_000)).to.be.true;
  });

  it("rejects withdrawal from wrong authority", () => {
    const wrongUser = Keypair.generate();
    svm.airdrop(wrongUser.publicKey, BigInt(1 * LAMPORTS_PER_SOL));

    const data = Buffer.alloc(9);
    data.writeUInt8(1, 0); // discriminator = 1 (withdraw)
    data.writeBigUInt64LE(WITHDRAW_AMOUNT, 1);

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: wrongUser.publicKey, isSigner: true, isWritable: true },
        { pubkey: vaultPda, isSigner: false, isWritable: true },
        { pubkey: vaultStatePda, isSigner: false, isWritable: false },
        { pubkey: SystemProgram.programId, isSigner: false, isWritable: false },
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

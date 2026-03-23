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
import {
  TOKEN_PROGRAM_ID,
  MINT_SIZE,
  createInitializeMintInstruction,
  createAssociatedTokenAccountInstruction,
  createMintToInstruction,
  getAssociatedTokenAddressSync,
} from "@solana/spl-token";
import * as fs from "fs";
import * as path from "path";

function sendAndConfirm(svm: LiteSVM, tx: Transaction): void {
  const result = svm.sendTransaction(tx);
  if (result instanceof FailedTransactionMetadata) {
    throw new Error(`Transaction failed: ${result.toString()}`);
  }
}

describe("Token Vault Program", () => {
  const programPath = path.join(__dirname, "..", "build", "program.so");
  const programId = Keypair.generate().publicKey;

  let svm: LiteSVM;
  let authority: Keypair;
  let mint: Keypair;
  let userTokenAccount: PublicKey;
  let vaultTokenAccount: PublicKey;
  let vaultStatePda: PublicKey;
  let vaultStateBump: number;

  const DEPOSIT_AMOUNT = BigInt(1_000_000); // 1M tokens
  const WITHDRAW_AMOUNT = BigInt(500_000);

  before(() => {
    svm = new LiteSVM();
    svm.addProgram(programId, fs.readFileSync(programPath));

    authority = Keypair.generate();
    svm.airdrop(authority.publicKey, BigInt(10 * LAMPORTS_PER_SOL));

    mint = Keypair.generate();

    [vaultStatePda, vaultStateBump] = PublicKey.findProgramAddressSync(
      [
        Buffer.from("token_vault"),
        mint.publicKey.toBuffer(),
        authority.publicKey.toBuffer(),
      ],
      programId
    );

    userTokenAccount = getAssociatedTokenAddressSync(
      mint.publicKey,
      authority.publicKey
    );

    vaultTokenAccount = getAssociatedTokenAddressSync(
      mint.publicKey,
      vaultStatePda,
      true
    );
  });

  it("sets up mint and token accounts", () => {
    const rentExempt = BigInt(1_461_600);
    const createMintIx = SystemProgram.createAccount({
      fromPubkey: authority.publicKey,
      newAccountPubkey: mint.publicKey,
      lamports: Number(rentExempt),
      space: MINT_SIZE,
      programId: TOKEN_PROGRAM_ID,
    });
    const initMintIx = createInitializeMintInstruction(
      mint.publicKey,
      6,
      authority.publicKey,
      null
    );

    const tx1 = new Transaction().add(createMintIx, initMintIx);
    tx1.recentBlockhash = svm.latestBlockhash();
    tx1.sign(authority, mint);
    sendAndConfirm(svm, tx1);

    const createUserAtaIx = createAssociatedTokenAccountInstruction(
      authority.publicKey,
      userTokenAccount,
      authority.publicKey,
      mint.publicKey
    );
    const tx2 = new Transaction().add(createUserAtaIx);
    tx2.recentBlockhash = svm.latestBlockhash();
    tx2.sign(authority);
    sendAndConfirm(svm, tx2);

    const mintToIx = createMintToInstruction(
      mint.publicKey,
      userTokenAccount,
      authority.publicKey,
      DEPOSIT_AMOUNT
    );
    const tx3 = new Transaction().add(mintToIx);
    tx3.recentBlockhash = svm.latestBlockhash();
    tx3.sign(authority);
    sendAndConfirm(svm, tx3);

    const createVaultAtaIx = createAssociatedTokenAccountInstruction(
      authority.publicKey,
      vaultTokenAccount,
      vaultStatePda,
      mint.publicKey
    );
    const tx4 = new Transaction().add(createVaultAtaIx);
    tx4.recentBlockhash = svm.latestBlockhash();
    tx4.sign(authority);
    sendAndConfirm(svm, tx4);
  });

  it("deposits tokens into the vault", () => {
    const data = Buffer.alloc(9);
    data.writeUInt8(0, 0); // discriminator = 0 (deposit)
    data.writeBigUInt64LE(DEPOSIT_AMOUNT, 1);

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: authority.publicKey, isSigner: true, isWritable: false },
        { pubkey: userTokenAccount, isSigner: false, isWritable: true },
        { pubkey: vaultTokenAccount, isSigner: false, isWritable: true },
        { pubkey: vaultStatePda, isSigner: false, isWritable: true },
        { pubkey: mint.publicKey, isSigner: false, isWritable: false },
        {
          pubkey: SystemProgram.programId,
          isSigner: false,
          isWritable: false,
        },
        { pubkey: TOKEN_PROGRAM_ID, isSigner: false, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(authority);

    sendAndConfirm(svm, tx);

    const vaultAccount = svm.getAccount(vaultTokenAccount);
    expect(vaultAccount).to.not.be.null;
    const vaultData = Buffer.from(vaultAccount!.data);
    const vaultBalance = vaultData.readBigUInt64LE(64);
    expect(vaultBalance).to.equal(DEPOSIT_AMOUNT);

    const userAccount = svm.getAccount(userTokenAccount);
    const userData = Buffer.from(userAccount!.data);
    const userBalance = userData.readBigUInt64LE(64);
    expect(userBalance).to.equal(BigInt(0));
  });

  it("withdraws tokens from the vault", () => {
    const data = Buffer.alloc(9);
    data.writeUInt8(1, 0); // discriminator = 1 (withdraw)
    data.writeBigUInt64LE(WITHDRAW_AMOUNT, 1);

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: authority.publicKey, isSigner: true, isWritable: false },
        { pubkey: userTokenAccount, isSigner: false, isWritable: true },
        { pubkey: vaultTokenAccount, isSigner: false, isWritable: true },
        { pubkey: vaultStatePda, isSigner: false, isWritable: false },
        { pubkey: mint.publicKey, isSigner: false, isWritable: false },
        { pubkey: TOKEN_PROGRAM_ID, isSigner: false, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(authority);

    sendAndConfirm(svm, tx);

    const vaultAccount = svm.getAccount(vaultTokenAccount);
    const vaultData = Buffer.from(vaultAccount!.data);
    const vaultBalance = vaultData.readBigUInt64LE(64);
    expect(vaultBalance).to.equal(DEPOSIT_AMOUNT - WITHDRAW_AMOUNT);

    const userAccount = svm.getAccount(userTokenAccount);
    const userData = Buffer.from(userAccount!.data);
    const userBalance = userData.readBigUInt64LE(64);
    expect(userBalance).to.equal(WITHDRAW_AMOUNT);
  });

  it("rejects withdrawal from wrong authority", () => {
    const wrongAuthority = Keypair.generate();
    svm.airdrop(wrongAuthority.publicKey, BigInt(1 * LAMPORTS_PER_SOL));

    const wrongUserToken = getAssociatedTokenAddressSync(
      mint.publicKey,
      wrongAuthority.publicKey
    );

    const data = Buffer.alloc(9);
    data.writeUInt8(1, 0); // discriminator = 1 (withdraw)
    data.writeBigUInt64LE(WITHDRAW_AMOUNT, 1);

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: wrongAuthority.publicKey, isSigner: true, isWritable: false },
        { pubkey: wrongUserToken, isSigner: false, isWritable: true },
        { pubkey: vaultTokenAccount, isSigner: false, isWritable: true },
        { pubkey: vaultStatePda, isSigner: false, isWritable: false },
        { pubkey: mint.publicKey, isSigner: false, isWritable: false },
        { pubkey: TOKEN_PROGRAM_ID, isSigner: false, isWritable: false },
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

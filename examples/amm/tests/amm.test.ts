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

function readTokenBalance(svm: LiteSVM, account: PublicKey): bigint {
  const info = svm.getAccount(account);
  return Buffer.from(info!.data).readBigUInt64LE(64);
}

function readMintSupply(svm: LiteSVM, mint: PublicKey): bigint {
  const info = svm.getAccount(mint);
  return Buffer.from(info!.data).readBigUInt64LE(36);
}

describe("AMM Program", () => {
  const programPath = path.join(__dirname, "..", "build", "program.so");
  const programId = Keypair.generate().publicKey;

  let svm: LiteSVM;
  let user: Keypair;
  let mintA: Keypair;
  let mintB: Keypair;
  let lpMint: Keypair;
  let poolPda: PublicKey;
  let poolBump: number;
  let vaultA: PublicKey;
  let vaultB: PublicKey;
  let userTokenA: PublicKey;
  let userTokenB: PublicKey;
  let userLp: PublicKey;

  before(() => {
    svm = new LiteSVM();
    svm.addProgram(programId, fs.readFileSync(programPath));

    user = Keypair.generate();
    svm.airdrop(user.publicKey, BigInt(100 * LAMPORTS_PER_SOL));

    mintA = Keypair.generate();
    mintB = Keypair.generate();
    lpMint = Keypair.generate();

    [poolPda, poolBump] = PublicKey.findProgramAddressSync(
      [
        Buffer.from("amm"),
        mintA.publicKey.toBuffer(),
        mintB.publicKey.toBuffer(),
      ],
      programId
    );

    vaultA = getAssociatedTokenAddressSync(mintA.publicKey, poolPda, true);
    vaultB = getAssociatedTokenAddressSync(mintB.publicKey, poolPda, true);
    userTokenA = getAssociatedTokenAddressSync(mintA.publicKey, user.publicKey);
    userTokenB = getAssociatedTokenAddressSync(mintB.publicKey, user.publicKey);
    userLp = getAssociatedTokenAddressSync(lpMint.publicKey, user.publicKey);

    const rentExempt = BigInt(1_461_600);

    let tx = new Transaction().add(
      SystemProgram.createAccount({
        fromPubkey: user.publicKey,
        newAccountPubkey: mintA.publicKey,
        lamports: Number(rentExempt),
        space: MINT_SIZE,
        programId: TOKEN_PROGRAM_ID,
      }),
      createInitializeMintInstruction(mintA.publicKey, 6, user.publicKey, null)
    );
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(user, mintA);
    sendAndConfirm(svm, tx);

    tx = new Transaction().add(
      SystemProgram.createAccount({
        fromPubkey: user.publicKey,
        newAccountPubkey: mintB.publicKey,
        lamports: Number(rentExempt),
        space: MINT_SIZE,
        programId: TOKEN_PROGRAM_ID,
      }),
      createInitializeMintInstruction(mintB.publicKey, 6, user.publicKey, null)
    );
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(user, mintB);
    sendAndConfirm(svm, tx);

    tx = new Transaction().add(
      SystemProgram.createAccount({
        fromPubkey: user.publicKey,
        newAccountPubkey: lpMint.publicKey,
        lamports: Number(rentExempt),
        space: MINT_SIZE,
        programId: TOKEN_PROGRAM_ID,
      }),
      createInitializeMintInstruction(lpMint.publicKey, 6, poolPda, null)
    );
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(user, lpMint);
    sendAndConfirm(svm, tx);

    tx = new Transaction().add(
      createAssociatedTokenAccountInstruction(
        user.publicKey, userTokenA, user.publicKey, mintA.publicKey
      ),
      createAssociatedTokenAccountInstruction(
        user.publicKey, userTokenB, user.publicKey, mintB.publicKey
      ),
      createAssociatedTokenAccountInstruction(
        user.publicKey, vaultA, poolPda, mintA.publicKey
      ),
      createAssociatedTokenAccountInstruction(
        user.publicKey, vaultB, poolPda, mintB.publicKey
      ),
      createAssociatedTokenAccountInstruction(
        user.publicKey, userLp, user.publicKey, lpMint.publicKey
      )
    );
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(user);
    sendAndConfirm(svm, tx);

    const INITIAL = BigInt(100_000_000);
    tx = new Transaction().add(
      createMintToInstruction(
        mintA.publicKey, userTokenA, user.publicKey, INITIAL
      ),
      createMintToInstruction(
        mintB.publicKey, userTokenB, user.publicKey, INITIAL
      )
    );
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(user);
    sendAndConfirm(svm, tx);
  });

  it("initializes the pool", () => {
    const data = Buffer.alloc(1);
    data.writeUInt8(0, 0); // disc = 0

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: user.publicKey, isSigner: true, isWritable: true },
        { pubkey: poolPda, isSigner: false, isWritable: true },
        { pubkey: vaultA, isSigner: false, isWritable: false },
        { pubkey: vaultB, isSigner: false, isWritable: false },
        { pubkey: lpMint.publicKey, isSigner: false, isWritable: false },
        { pubkey: mintA.publicKey, isSigner: false, isWritable: false },
        { pubkey: mintB.publicKey, isSigner: false, isWritable: false },
        { pubkey: SystemProgram.programId, isSigner: false, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(user);
    sendAndConfirm(svm, tx, "initialize_pool");

    const account = svm.getAccount(poolPda);
    expect(account).to.not.be.null;
    const stateData = Buffer.from(account!.data);

    expect(stateData.readUInt8(0)).to.equal(1); // is_initialized
    expect(stateData.readUInt8(1)).to.equal(poolBump); // bump
    expect(stateData.subarray(8, 40)).to.deep.equal(mintA.publicKey.toBuffer());
    expect(stateData.subarray(40, 72)).to.deep.equal(
      mintB.publicKey.toBuffer()
    );
  });

  it("adds initial liquidity (1M A + 4M B)", () => {
    const data = Buffer.alloc(25);
    data.writeUInt8(1, 0); // disc = 1 (add_liquidity)
    data.writeBigUInt64LE(BigInt(1_000_000), 1); // amount_a
    data.writeBigUInt64LE(BigInt(4_000_000), 9); // amount_b
    data.writeBigUInt64LE(BigInt(0), 17); // min_lp

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: user.publicKey, isSigner: true, isWritable: false },
        { pubkey: poolPda, isSigner: false, isWritable: false },
        { pubkey: vaultA, isSigner: false, isWritable: true },
        { pubkey: vaultB, isSigner: false, isWritable: true },
        { pubkey: userTokenA, isSigner: false, isWritable: true },
        { pubkey: userTokenB, isSigner: false, isWritable: true },
        { pubkey: lpMint.publicKey, isSigner: false, isWritable: true },
        { pubkey: userLp, isSigner: false, isWritable: true },
        { pubkey: TOKEN_PROGRAM_ID, isSigner: false, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(user);
    sendAndConfirm(svm, tx, "add_liquidity");

    // LP minted = sqrt(1M * 4M) = 2,000,000
    expect(readTokenBalance(svm, userLp)).to.equal(BigInt(2_000_000));
    expect(readTokenBalance(svm, vaultA)).to.equal(BigInt(1_000_000));
    expect(readTokenBalance(svm, vaultB)).to.equal(BigInt(4_000_000));
  });

  it("swaps 100K A → B", () => {
    const data = Buffer.alloc(18);
    data.writeUInt8(3, 0); // disc = 3 (swap)
    data.writeBigUInt64LE(BigInt(100_000), 1); // amount_in
    data.writeBigUInt64LE(BigInt(0), 9); // min_amount_out
    data.writeUInt8(0, 17); // direction = 0 (A→B)

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: user.publicKey, isSigner: true, isWritable: false },
        { pubkey: poolPda, isSigner: false, isWritable: false },
        { pubkey: vaultA, isSigner: false, isWritable: true },
        { pubkey: vaultB, isSigner: false, isWritable: true },
        { pubkey: userTokenA, isSigner: false, isWritable: true },
        { pubkey: userTokenB, isSigner: false, isWritable: true },
        { pubkey: TOKEN_PROGRAM_ID, isSigner: false, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(user);
    sendAndConfirm(svm, tx, "swap A→B");

    // out = (4M * 100K * 997) / (1M * 1000 + 100K * 997) = 362,644
    expect(readTokenBalance(svm, vaultA)).to.equal(BigInt(1_100_000));
    expect(readTokenBalance(svm, vaultB)).to.equal(BigInt(3_637_356));
  });

  it("swaps 200K B → A", () => {
    const data = Buffer.alloc(18);
    data.writeUInt8(3, 0);
    data.writeBigUInt64LE(BigInt(200_000), 1);
    data.writeBigUInt64LE(BigInt(0), 9);
    data.writeUInt8(1, 17); // direction = 1 (B→A)

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: user.publicKey, isSigner: true, isWritable: false },
        { pubkey: poolPda, isSigner: false, isWritable: false },
        { pubkey: vaultA, isSigner: false, isWritable: true },
        { pubkey: vaultB, isSigner: false, isWritable: true },
        { pubkey: userTokenA, isSigner: false, isWritable: true },
        { pubkey: userTokenB, isSigner: false, isWritable: true },
        { pubkey: TOKEN_PROGRAM_ID, isSigner: false, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(user);
    sendAndConfirm(svm, tx, "swap B→A");

    // out = (1.1M * 200K * 997) / (3,637,356 * 1000 + 200K * 997) = 57,168
    expect(readTokenBalance(svm, vaultA)).to.equal(BigInt(1_042_832));
    expect(readTokenBalance(svm, vaultB)).to.equal(BigInt(3_837_356));
  });

  it("removes 500K LP", () => {
    const data = Buffer.alloc(25);
    data.writeUInt8(2, 0); // disc = 2 (remove_liquidity)
    data.writeBigUInt64LE(BigInt(500_000), 1); // lp_amount
    data.writeBigUInt64LE(BigInt(0), 9); // min_a
    data.writeBigUInt64LE(BigInt(0), 17); // min_b

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: user.publicKey, isSigner: true, isWritable: false },
        { pubkey: poolPda, isSigner: false, isWritable: false },
        { pubkey: vaultA, isSigner: false, isWritable: true },
        { pubkey: vaultB, isSigner: false, isWritable: true },
        { pubkey: userTokenA, isSigner: false, isWritable: true },
        { pubkey: userTokenB, isSigner: false, isWritable: true },
        { pubkey: lpMint.publicKey, isSigner: false, isWritable: true },
        { pubkey: userLp, isSigner: false, isWritable: true },
        { pubkey: TOKEN_PROGRAM_ID, isSigner: false, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(user);
    sendAndConfirm(svm, tx, "remove_liquidity");

    // amount_a = 500K * 1,042,832 / 2M = 260,708
    // amount_b = 500K * 3,837,356 / 2M = 959,339
    expect(readTokenBalance(svm, vaultA)).to.equal(
      BigInt(1_042_832 - 260_708)
    );
    expect(readTokenBalance(svm, vaultB)).to.equal(
      BigInt(3_837_356 - 959_339)
    );
    expect(readMintSupply(svm, lpMint.publicKey)).to.equal(BigInt(1_500_000));
  });

  it("rejects swap with excessive slippage", () => {
    const data = Buffer.alloc(18);
    data.writeUInt8(3, 0);
    data.writeBigUInt64LE(BigInt(100_000), 1);
    data.writeBigUInt64LE(BigInt(999_999_999), 9); // this aint gonna happen
    data.writeUInt8(0, 17);

    const ix = new TransactionInstruction({
      keys: [
        { pubkey: user.publicKey, isSigner: true, isWritable: false },
        { pubkey: poolPda, isSigner: false, isWritable: false },
        { pubkey: vaultA, isSigner: false, isWritable: true },
        { pubkey: vaultB, isSigner: false, isWritable: true },
        { pubkey: userTokenA, isSigner: false, isWritable: true },
        { pubkey: userTokenB, isSigner: false, isWritable: true },
        { pubkey: TOKEN_PROGRAM_ID, isSigner: false, isWritable: false },
      ],
      programId,
      data,
    });

    const tx = new Transaction().add(ix);
    tx.recentBlockhash = svm.latestBlockhash();
    tx.sign(user);

    const result = svm.sendTransaction(tx);
    expect(result).to.be.instanceOf(FailedTransactionMetadata);
  });
});

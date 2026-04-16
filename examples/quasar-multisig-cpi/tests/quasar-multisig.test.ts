import { expect } from "chai";
import { LiteSVM, FailedTransactionMetadata } from "litesvm";
import {
  type Address,
  appendTransactionMessageInstruction,
  compileTransaction,
  createTransactionMessage,
  generateKeyPair,
  getAddressEncoder,
  getAddressFromPublicKey,
  lamports,
  pipe,
  setTransactionMessageFeePayer,
  signTransaction,
  type Instruction,
} from "@solana/kit";
import * as fs from "fs";
import * as path from "path";
import {
  MULTISIG_PROGRAM_ADDRESS,
  getConfigPda,
  getVaultPda,
  createProxyCreateIx,
  createProxyDepositIx,
  createProxyExecuteTransferIx,
} from "./client";

const wrapperProgramPath = path.join(__dirname, "..", "build", "program.so");
const multisigProgramPath = path.join(__dirname, "..", "quasar_multisig.so");

async function buildTx(
  svm: LiteSVM,
  feePayer: Address,
  signerKeys: CryptoKeyPair[],
  ix: Instruction,
) {
  const msg = pipe(
    createTransactionMessage({ version: "legacy" }),
    (m) => setTransactionMessageFeePayer(feePayer, m),
    (m) => svm.setTransactionMessageLifetimeUsingLatestBlockhash(m),
    (m) => appendTransactionMessageInstruction(ix, m),
  );
  return await signTransaction(signerKeys, compileTransaction(msg));
}

describe("Quasar Multisig CPI Wrapper", () => {
  let wrapperId: Address;
  let svm: LiteSVM;
  let creatorKeys: CryptoKeyPair;
  let creatorAddr: Address;
  let signerKeys: CryptoKeyPair;
  let signerAddr: Address;

  before(async () => {
    svm = new LiteSVM();
    svm.addProgram(
      MULTISIG_PROGRAM_ADDRESS,
      fs.readFileSync(multisigProgramPath),
    );

    const wrapperKeys = await generateKeyPair();
    wrapperId = await getAddressFromPublicKey(wrapperKeys.publicKey);
    svm.addProgram(wrapperId, fs.readFileSync(wrapperProgramPath));

    creatorKeys = await generateKeyPair();
    creatorAddr = await getAddressFromPublicKey(creatorKeys.publicKey);
    svm.airdrop(creatorAddr, lamports(10_000_000_000n));

    signerKeys = await generateKeyPair();
    signerAddr = await getAddressFromPublicKey(signerKeys.publicKey);
    svm.airdrop(signerAddr, lamports(1_000_000_000n));
  });

  it("creates a multisig config via CPI", async () => {
    const ix = await createProxyCreateIx(wrapperId, creatorAddr, 1, [
      signerAddr,
    ]);
    const tx = await buildTx(svm, creatorAddr, [creatorKeys, signerKeys], ix);
    const result = svm.sendTransaction(tx);
    if (result instanceof FailedTransactionMetadata)
      throw new Error(`Transaction failed: ${result.toString()}`);
    result.logs().forEach((l) => console.log(`      ${l}`));
    console.log(`      proxy_create: ${result.computeUnitsConsumed()} CU`);

    const [configAddr] = await getConfigPda(creatorAddr);
    const acct = svm.getAccount(configAddr);
    expect(acct.exists).to.be.true;
    if (acct.exists) {
      const encoder = getAddressEncoder();
      const creatorBytes = encoder.encode(creatorAddr);
      expect(
        Buffer.from(acct.data.slice(1, 33)).equals(Buffer.from(creatorBytes)),
      ).to.be.true;
      expect(acct.data[33]).to.equal(1);
    }
  });

  it("deposits SOL into the multisig vault via CPI", async () => {
    const [configAddr] = await getConfigPda(creatorAddr);
    const [vaultAddr] = await getVaultPda(configAddr);
    const depositAmount = 1_000_000_000n;

    const ix = await createProxyDepositIx(
      wrapperId,
      creatorAddr,
      configAddr,
      depositAmount,
    );
    const tx = await buildTx(svm, creatorAddr, [creatorKeys], ix);
    const result = svm.sendTransaction(tx);
    if (result instanceof FailedTransactionMetadata)
      throw new Error(`Transaction failed: ${result.toString()}`);
    result.logs().forEach((l) => console.log(`      ${l}`));
    console.log(`      proxy_deposit: ${result.computeUnitsConsumed()} CU`);

    const vaultAcct = svm.getAccount(vaultAddr);
    expect(vaultAcct.exists).to.be.true;
    if (vaultAcct.exists) {
      expect(BigInt(vaultAcct.lamports) >= depositAmount).to.be.true;
    }
  });

  it("executes transfer from vault via CPI", async () => {
    const recipientKeys = await generateKeyPair();
    const recipientAddr = await getAddressFromPublicKey(
      recipientKeys.publicKey,
    );

    const [configAddr] = await getConfigPda(creatorAddr);
    const [vaultAddr] = await getVaultPda(configAddr);
    const vaultBefore = svm.getAccount(vaultAddr);
    const vaultLamportsBefore = vaultBefore.exists
      ? BigInt(vaultBefore.lamports)
      : 0n;

    const transferAmount = 500_000_000n;
    const ix = await createProxyExecuteTransferIx(
      wrapperId,
      creatorAddr,
      recipientAddr,
      transferAmount,
      [signerAddr],
    );
    const tx = await buildTx(
      svm,
      creatorAddr,
      [creatorKeys, signerKeys],
      ix,
    );
    const result = svm.sendTransaction(tx);
    if (result instanceof FailedTransactionMetadata)
      throw new Error(`Transaction failed: ${result.toString()}`);
    result.logs().forEach((l) => console.log(`      ${l}`));
    console.log(
      `      proxy_execute_transfer: ${result.computeUnitsConsumed()} CU`,
    );

    const recipientAcct = svm.getAccount(recipientAddr);
    expect(recipientAcct.exists).to.be.true;
    if (recipientAcct.exists) {
      expect(BigInt(recipientAcct.lamports)).to.equal(transferAmount);
    }

    const vaultAfter = svm.getAccount(vaultAddr);
    if (vaultAfter.exists) {
      expect(BigInt(vaultAfter.lamports)).to.equal(
        vaultLamportsBefore - transferAmount,
      );
    }
  });
});

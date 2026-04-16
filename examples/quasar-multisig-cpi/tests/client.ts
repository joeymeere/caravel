import {
  type Address,
  address,
  AccountRole,
  type Instruction,
  getProgramDerivedAddress,
  getAddressCodec,
  getAddressEncoder,
} from "@solana/kit";
import { getU64Codec } from "@solana/codecs";

const MULTISIG_PROGRAM_ID_BYTES = new Uint8Array([
  0x2d, 0x5b, 0x41, 0x3c, 0x65, 0x40, 0xde, 0x15,
  0x0c, 0x93, 0x73, 0x14, 0x4d, 0x51, 0x33, 0xca,
  0x4c, 0xb8, 0x30, 0xba, 0x0f, 0x75, 0x67, 0x16,
  0xac, 0xea, 0x0e, 0x50, 0xd7, 0x94, 0x35, 0xe5,
]);

export const MULTISIG_PROGRAM_ADDRESS: Address =
  getAddressCodec().decode(MULTISIG_PROGRAM_ID_BYTES);

const SYSTEM_PROGRAM = address("11111111111111111111111111111111");
const RENT_SYSVAR = address("SysvarRent111111111111111111111111111111111");

const SEED_MULTISIG = new Uint8Array([
  0x6d, 0x75, 0x6c, 0x74, 0x69, 0x73, 0x69, 0x67,
]);
const SEED_VAULT = new Uint8Array([0x76, 0x61, 0x75, 0x6c, 0x74]);

export async function getConfigPda(creator: Address) {
  return getProgramDerivedAddress({
    programAddress: MULTISIG_PROGRAM_ADDRESS,
    seeds: [SEED_MULTISIG, getAddressEncoder().encode(creator)],
  });
}

export async function getVaultPda(config: Address) {
  return getProgramDerivedAddress({
    programAddress: MULTISIG_PROGRAM_ADDRESS,
    seeds: [SEED_VAULT, getAddressEncoder().encode(config)],
  });
}

export async function createProxyCreateIx(
  programId: Address,
  creator: Address,
  threshold: number,
  signers: Address[],
): Promise<Instruction> {
  const [config] = await getConfigPda(creator);
  return {
    programAddress: programId,
    accounts: [
      { address: creator, role: AccountRole.WRITABLE_SIGNER },
      { address: config, role: AccountRole.WRITABLE },
      { address: RENT_SYSVAR, role: AccountRole.READONLY },
      { address: SYSTEM_PROGRAM, role: AccountRole.READONLY },
      { address: MULTISIG_PROGRAM_ADDRESS, role: AccountRole.READONLY },
      ...signers.map((s) => ({
        address: s,
        role: AccountRole.READONLY_SIGNER as const,
      })),
    ],
    data: new Uint8Array([0x00, threshold]),
  };
}

export async function createProxyDepositIx(
  programId: Address,
  depositor: Address,
  config: Address,
  amount: bigint,
): Promise<Instruction> {
  const [vault] = await getVaultPda(config);
  const amountBytes = getU64Codec().encode(amount);
  const data = new Uint8Array(1 + amountBytes.length);
  data[0] = 0x01;
  data.set(amountBytes, 1);
  return {
    programAddress: programId,
    accounts: [
      { address: depositor, role: AccountRole.WRITABLE_SIGNER },
      { address: config, role: AccountRole.READONLY },
      { address: vault, role: AccountRole.WRITABLE },
      { address: SYSTEM_PROGRAM, role: AccountRole.READONLY },
      { address: MULTISIG_PROGRAM_ADDRESS, role: AccountRole.READONLY },
    ],
    data,
  };
}

export async function createProxyExecuteTransferIx(
  programId: Address,
  creator: Address,
  recipient: Address,
  amount: bigint,
  signers: Address[],
): Promise<Instruction> {
  const [config] = await getConfigPda(creator);
  const [vault] = await getVaultPda(config);
  const amountBytes = getU64Codec().encode(amount);
  const data = new Uint8Array(1 + amountBytes.length);
  data[0] = 0x02;
  data.set(amountBytes, 1);
  return {
    programAddress: programId,
    accounts: [
      { address: creator, role: AccountRole.READONLY_SIGNER },
      { address: config, role: AccountRole.READONLY },
      { address: vault, role: AccountRole.WRITABLE },
      { address: recipient, role: AccountRole.WRITABLE },
      { address: SYSTEM_PROGRAM, role: AccountRole.READONLY },
      { address: MULTISIG_PROGRAM_ADDRESS, role: AccountRole.READONLY },
      ...signers.map((s) => ({
        address: s,
        role: AccountRole.READONLY_SIGNER as const,
      })),
    ],
    data,
  };
}

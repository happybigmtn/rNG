# Transaction Specification

## Topic
The format, validation rules, and fee policies for RNG transactions.

## Behavioral Requirements

### Transaction Format
RNG uses **Bitcoin's transaction format** unchanged:

**Legacy Transaction (pre-SegWit):**
| Field | Size | Description |
|-------|------|-------------|
| version | 4 bytes | Transaction version |
| input_count | varint | Number of inputs |
| inputs | variable | Transaction inputs |
| output_count | varint | Number of outputs |
| outputs | variable | Transaction outputs |
| locktime | 4 bytes | Block height or timestamp |

**SegWit Transaction (BIP141):**
| Field | Size | Description |
|-------|------|-------------|
| version | 4 bytes | Transaction version |
| marker | 1 byte | 0x00 |
| flag | 1 byte | 0x01 |
| input_count | varint | Number of inputs |
| inputs | variable | Transaction inputs |
| output_count | varint | Number of outputs |
| outputs | variable | Transaction outputs |
| witness | variable | Witness data for each input |
| locktime | 4 bytes | Block height or timestamp |

### Transaction Version
- **Version 1**: Standard transactions
- **Version 2**: Transactions using BIP68 (relative locktime)
- Transactions with unknown versions are valid (forward compatibility)

### Size Limits

| Limit | Value | Notes |
|-------|-------|-------|
| Max transaction weight | **400,000 WU** | 10% of block |
| Max standard tx size | **100,000 vbytes** | Policy limit for relay |
| Min transaction size | **82 bytes** | Smallest valid tx |

### Input Rules
- Input must reference valid unspent output (UTXO)
- Signature must satisfy previous output's script
- Sequence number enables RBF and relative timelocks

### Output Rules
- Value must be ≥ 0 and within RNG's consensus `MoneyRange` sanity bound
- Sum of outputs must not exceed sum of inputs
- Standard output types: P2PKH, P2SH, P2WPKH, P2WSH, P2TR

### Fee Policy

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Minimum relay fee** | **1 roshi/vbyte** | Required for node relay |
| **Dust threshold** | **546 roshis** | Outputs below this are "dust" |
| **Dust relay fee** | **3 roshis/vbyte** | For dust limit calculation |

**Fee calculation**:
- Fee = sum(inputs) - sum(outputs)
- Fee rate = fee / virtual_size
- Virtual size = max(weight/4, base_size)

### Dust Threshold Calculation
An output is dust if spending it costs more than its value:
```
dust_limit = (input_size + 34) × 3 × dust_relay_fee
```
For P2PKH: (148 + 34) × 3 × 3 = 1,638 roshis ≈ 546 (simplified)

### Replace-By-Fee (BIP125)
- Transactions may opt-in to RBF via sequence number < 0xFFFFFFFE
- Replacement must pay higher absolute fee
- Replacement must pay higher fee rate
- Replacement must not add new unconfirmed inputs

### Locktime Rules
- locktime = 0: No restriction
- locktime < 500,000,000: Block height
- locktime ≥ 500,000,000: Unix timestamp
- Transaction invalid until locktime reached

### Relative Locktime (BIP68)
- Version 2 transactions only
- Sequence number encodes relative delay
- Bit 31 clear: enforce relative locktime
- Bits 0-15: delay value (blocks or 512-second intervals)
- Bit 22: interpret as time (vs blocks)

### Signature Hashing
- SIGHASH_ALL (0x01): Sign all inputs and outputs
- SIGHASH_NONE (0x02): Sign all inputs, no outputs
- SIGHASH_SINGLE (0x03): Sign corresponding output only
- SIGHASH_ANYONECANPAY (0x80): Sign only this input

### Transaction Validation Order
1. Check syntax and size limits
2. Verify no duplicate inputs
3. Verify all inputs exist and are unspent
4. Verify total input value ≥ total output value
5. Execute input scripts
6. Verify signatures
7. Check locktime constraints

## Acceptance Criteria

1. [ ] Standard Bitcoin transaction format is valid
2. [ ] SegWit transactions are valid
3. [ ] Transactions exceeding 400,000 WU are rejected
4. [ ] Double-spend attempts are rejected
5. [ ] Insufficient fee transactions are not relayed
6. [ ] Dust outputs are rejected by policy
7. [ ] RBF replacements work correctly
8. [ ] Locktime enforcement works correctly
9. [ ] Relative locktime (BIP68) works correctly
10. [ ] All SIGHASH types function correctly
11. [ ] Transaction fees are correctly calculated

## Test Scenarios

- Submit transaction with insufficient fee, verify rejection
- Submit dust output, verify rejection
- Double-spend same UTXO, verify only one confirms
- Use RBF to replace transaction with higher fee
- Test locktime with future block height
- Test relative locktime with BIP68 sequence
- Create SegWit transaction, verify smaller witness discount

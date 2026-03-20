# Block Structure Specification

## Topic
The structure, size limits, and validation rules for RNG blocks.

## Behavioral Requirements

### Block Header Format
The block header is **80 bytes** (identical to Bitcoin):

| Field | Size | Description |
|-------|------|-------------|
| version | 4 bytes | Block version (little-endian) |
| prev_block | 32 bytes | Hash of previous block header |
| merkle_root | 32 bytes | Merkle root of transactions |
| timestamp | 4 bytes | Unix timestamp (seconds) |
| bits | 4 bytes | Difficulty target (compact format) |
| nonce | 4 bytes | Mining nonce |

**Note**: RandomX hashes the full 80-byte header. No header format changes needed.

### Block Version
- **Current version**: 0x20000000 (BIP9 version bits enabled)
- Version bits reserved for future soft forks
- Blocks with unknown version bits are valid (forward compatibility)

### Block Size Limits

| Limit | Value | Rationale |
|-------|-------|-----------|
| Max block weight | **4,000,000 WU** | Same as Bitcoin post-SegWit |
| Max block size (legacy) | ~1 MB | Implied by weight limit |
| Max block size (SegWit) | ~4 MB | Theoretical max with all witness |

**Weight calculation**:
- Non-witness data: 4 WU per byte
- Witness data: 1 WU per byte
- Block weight = (base size × 3) + total size

### Timestamp Rules
- Must be greater than median of last 11 blocks (MTP)
- Must not be more than 2 hours in the future
- Target spacing: 120 seconds on mainnet (see consensus.md)

### Difficulty Target (bits)
- Uses Bitcoin's compact encoding
- `target = coefficient × 2^(8 × (exponent - 3))`
- Block valid if: `RandomX_hash(header) <= target`

### Coinbase Transaction Rules
- Must be first transaction in block
- Must have exactly one input with null prevout (0x00...00, 0xFFFFFFFF)
- Coinbase script must be 2-100 bytes
- Must include block height (BIP34) in coinbase script
- Output value ≤ block reward + total fees
- Extra nonce space available in coinbase for RandomX nonce extension

### Merkle Tree
- Binary hash tree using double-SHA256 (standard Bitcoin merkle)
- Single transaction: merkle_root = txid
- Odd number of leaves: duplicate last leaf

### Block Validation Order
1. Check block header syntax
2. Verify RandomX hash meets difficulty target
3. Check timestamp validity
4. Verify merkle root matches transactions
5. Validate all transactions
6. Verify coinbase value ≤ reward + fees
7. Apply UTXO changes

### Orphan Block Handling
- Blocks with unknown parent are held as orphans
- Re-validated when parent arrives
- Orphan pool limited to prevent memory exhaustion

## Acceptance Criteria

1. [ ] Block header is exactly 80 bytes
2. [ ] Blocks exceeding 4M weight units are rejected
3. [ ] Timestamp must exceed median-time-past
4. [ ] Timestamp cannot be >2 hours in future
5. [ ] Coinbase must include block height (BIP34)
6. [ ] Coinbase value cannot exceed reward + fees
7. [ ] Merkle root must match transaction hashes
8. [ ] RandomX hash must meet difficulty target
9. [ ] Block version defaults to 0x20000000
10. [ ] Invalid blocks are rejected with specific error codes

## Test Scenarios

- Submit block exceeding weight limit, verify rejection
- Submit block with future timestamp (+3h), verify rejection
- Submit block with incorrect merkle root, verify rejection
- Submit block with excess coinbase reward, verify rejection
- Submit valid block, verify acceptance and chain extension
- Generate block with many transactions, verify weight calculation

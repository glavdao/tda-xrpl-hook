# XRP-Hook: Automated Pay-Out Splitter with Memo Router

A lightweight [XRPL Hook](https://xrpl-hooks.readme.io/) written in C that automatically:

1. Charges **0.2 %** (2 basis points) from every incoming **Payment** transaction.
2. Forwards the fee to the **owner wallet**.
3. Immediately relays the remaining amount to either
   - the **sender** (if no valid memo is supplied) **or**
   - a **destination address** extracted from a Base64-encoded JSON memo.

The hook is **stateless** – no local storage, no external calls – and guarantees atomic execution within a single ledger close.

---

## How It Works

### 1. Incoming Payment Check

The hook only processes `ttPAYMENT` transactions. Any other transaction type is accepted without further action.

### 2. Fee Calculation

`amount * 0.002` is calculated with XRPL’s fixed-point XFL helpers.  
Minimum fee is **1 drop** (0.000001 XRP).

### 3. Memo Parsing (optional)

- A **Memo** field is expected to carry a JSON object encoded in **Base64**.
- The JSON must contain four keys:

  ```json
  {
    "wallet": "rDestinationAddressHere_______________",
    "amount": "123456789",        // same as tx amount (drops)
    "memo":   "some text ≤ 180 B",
    "ts":     "2024-05-12T10:00:00Z"
  }

    If the memo is syntactically correct and the amount matches the actual payment, the hook:
        sends the remaining 99.8 % to the specified wallet,
        re-attaches the plain memo field.
  ```

Otherwise the surplus is simply refunded to the sender.

    The date string (ts) is only checked for basic structure (YYYY-MM-DDThh:mm:ssZ).
    It is not validated against the transaction time.

4. Atomic Emissions

Both outgoing legs (fee → owner, remainder → destination/sender) are emitted before the hook returns. The entire operation succeeds or rolls back with the transaction.

Building & Installing

    Install the Hooks SDK v2 (docs).
    Compile:
    bash

hook-clean
hook-cc hook.c -o hook.wasm -O2

Deploy to any XRPL account:
bash

    hook-api --install hook.wasm --account <your-account> --secret <your-secret>

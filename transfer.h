#include "hookapi.h"

int64t hook(uint32t reserved) {
    // 1. Getting the transaction data
    uint8t amount[8];
    hookparam(AMOUNT, amount, 8); // Amount in drops

    uint8t dest[20];
    int64t destlen = hookparam(DESTINATION, dest, 20); // Recipient's address

    uint8t memo[256];
    int64t memolen = hookparam(MEMO, memo, 256); // Message

    if (destlen != 20 || memolen <= 0)
        ROLLBACK(0, "Invalid dest or memo");

    // Converting the amount from the buffer to int64
    int64t amt = buftoint64(amount);
    if (amt <= 0)
        ROLLBACK(0, "Invalid amount");

    // 2. Calculation: 99.8% to the main recipient, 0.2% of the balance
    int64t mainamt = (amt * 998) / 1000; // 99.8%
    int64t feeamt = amt - mainamt;      // 0.2%

    // Preparation of the first transaction (99.8% with Memo)
    uint8t tx1[256];
    int64t tx1len = preparepayment(mainamt, dest, memo, memolen, tx1, 256);
    if (tx1len <= 0)
        ROLLBACK(0, "Failed to prepare main tx");

    // Sending the first transaction
    emit(tx1, tx1len);

    // 3. The balance to a fixed wallet
    uint8t feedest = {
        0x72, 0x70, 0x73, 0x68, 0x6E, 0x61, 0x66, 0x33, 0x39, 0x77,
        0x42, 0x55, 0x44, 0x4E, 0x45, 0x47, 0x48, 0x4A, 0x4B, 0x4C
    }; // rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz
    uint8t tx2[256];
    int64t tx2len = preparepayment(feeamt, feedest, 0, 0, tx2, 256);
    if (tx2len <= 0)
        ROLLBACK(0, "Failed to prepare fee tx");

    // Sending a second transaction
    emit(tx2, tx2len);

    ACCEPT(0, "Payments processed");
}

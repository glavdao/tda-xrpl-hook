/* hook.c */
#include <stdint.h>
#include <string.h>
#include "hookapi.h"

#define OWNER_ADDRESS  "rOwnerAddressHere_______________"
#define MIN(a,b)       ((a) < (b) ? (a) : (b))

/* ----------  util helpers  ---------- */
static int64_t float_multiply_xfl(const uint8_t *xfl, float factor, uint8_t *out)
{
    int64_t v = INT64_FROM_BUF(xfl);
    v = float_int64((float)v * factor);
    INT64_TO_BUF(out, v);
    return v;
}

static int64_t float_subtract_xfl(const uint8_t *a, const uint8_t *b, uint8_t *out)
{
    int64_t va = INT64_FROM_BUF(a);
    int64_t vb = INT64_FROM_BUF(b);
    int64_t r  = va - vb;
    INT64_TO_BUF(out, r);
    return r;
}

/* ----------  dictionary parser  ---------- */
typedef struct {
    const char *key;
    uint8_t    *dst;
    int         max;
} dict_slot_t;

static int find_key(const uint8_t *d, int d_len, const char *k, uint8_t *v, int v_max)
{
    const uint8_t *p   = d;
    const uint8_t *end = d + d_len;
    int klen = strlen(k); 

    while (p + 1 < end) {
        uint8_t kl = *p++;
        if (p + kl > end) break;
        if (kl == klen && !__builtin_memcmp(p, k, kl)) {
            p += kl;
            if (p + 1 > end) break;
            uint8_t vl = *p++;
            if (p + vl > end) break;
            int copy = MIN(vl, v_max);
            __builtin_memcpy(v, p, copy);
            if (copy < v_max) __builtin_memset(v + copy, 0, v_max - copy);
            return copy;
        }
        p += kl;
        if (p + 1 > end) break;
        p += 1 + *p;
    }
    return 0;
}

static int parse_dict(const uint8_t *dict, int dict_len, dict_slot_t *slots, int cnt)
{
    for (int i = 0; i < cnt; ++i)
        if (!find_key(dict, dict_len, slots[i].key, slots[i].dst, slots[i].max))
            return 0;
    return 1;
}

/* ----------  memo / payment helpers  ---------- */
static int64_t prepare_memo(const uint8_t *txt, uint8_t *out)
{
    int64_t len = 0;
    while (len < 255 && txt[len]) ++len;
    __builtin_memcpy(out, txt, len);
    return len;
}

static void emit_payment(const uint8_t *dest, const uint8_t *amount, int amt_len,
                         const uint8_t *memo, int memo_len)
{
    trace_num("Emitting payment", (const char*)dest, 20);
    trace_num("Amount", (const char*)amount, amt_len);
    if (memo && memo_len) trace("Memo", (const char*)memo, memo_len);
}

static void emit_payment_no_memo(const uint8_t *dest, const uint8_t *amount, int amt_len)
{
    emit_payment(dest, amount, amt_len, NULL, 0);
}

/* ----------  hook entry points  ---------- */
int64_t cbak(uint32_t reserved) { returnc

    return 0;
}

int64_t hook(uint32_t reserved)
{
    uint8_t otxn[4096];
    int64_t otxn_size = otxn_field(otxn, sizeof(otxn), sfTransaction, 0, 0);
    if (otxn_size < 0) rollback(SBUF("could not load otxn"), 0);

    uint8_t tt[4];
    otxn_field(tt, 4, sfTransactionType, 0, 0);
    if (BUFFER_EQUAL_20(tt, ttPAYMENT) != 4) accept(SBUF("not a payment"), 0);

    uint8_t amount[8];
    otxn_field(amount, 8, sfAmount, 0, 0);

    uint8_t memos[2048];
    int64_t memos_size = otxn_field(memos, sizeof(memos), sfMemos, 0, 0);

    uint8_t  dest_acc[20];
    uint8_t  memo_text[181];
    uint8_t  expected_amount[8];
    uint8_t  ts_str[25];
    int      valid_memo = 0;

    if (memos_size > 0) {
        uint8_t *memo_data = memos + 3;
        uint32_t memo_len  = (memos[1] << 8) | memos[2];
        if (memo_len > 256) memo_len = 256;

        uint8_t decoded[256];
        int64_t decoded_len = base64_decode(memo_data, memo_len, decoded, sizeof(decoded));
        if (decoded_len > 0) {
            dict_slot_t slots[] = {
                {"wallet",  dest_acc,         20},
                {"memo",    memo_text,        180},
                {"amount",  expected_amount,  8},
                {"ts",      ts_str,           24}
            };

            if (parse_dict(decoded, decoded_len, slots, 4)) {
                uint8_t addr_type;
                if (util_verify(dest_acc, 20, &addr_type) == 1 && addr_type == 28) {
                    if (BUFFER_EQUAL_20(amount, expected_amount) == 8) {
                        valid_memo = (ts_str[4] == '-' && ts_str[7] == '-' && ts_str[10] == 'T');
                    }
                }
            }
        }
    }

    uint8_t fee[8];
    int64_t fee_amt = float_multiply_xfl(amount, 0.002f, fee);
    if (fee_amt <= 0) fee_amt = 1;  
    uint8_t sender[20];
    otxn_field(sender, 20, sfAccount, 0, 0);
    uint8_t owner[20];
    util_accid(OWNER_ADDRESS, owner, 20);
    uint8_t remaining[8];
    float_subtract_xfl(amount, fee, remaining);

    emit_payment(owner, fee, 8); 

    if (!valid_memo) {
        emit_payment_no_memo(sender, remaining, 8);
    } else {
        uint8_t new_memo[256];
        int64_t new_memo_len = prepare_memo(memo_text, new_memo);
        emit_payment(dest_acc, remaining, 8, new_memo, new_memo_len);
    }

    accept(SBUF("success"), 0);
    return 0;
}
/* ------------  end of hook.c  ------------ */

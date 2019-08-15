#include "sci/Utils/Crypt.h"

typedef struct Decrypt3Info {
    struct tokenlist {
        uint8_t data;
        int16_t next;
    } tokens[0x1004];

    int8_t   stak[0x1014];
    int8_t   lastchar;
    int16_t  stakptr;
    uint16_t numbits, bitstring, lastbits, decryptstart;
    int16_t  curtoken, endtoken;

    uint32_t whichbit;
} Decrypt3Info;

static uint32_t gbits(Decrypt3Info *info, int numbits, uint8_t *data, int dlen)
{
    int      place; // indicates location within byte
    uint32_t bitstring;
    int      i;

    if (numbits == 0) {
        info->whichbit = 0;
        return 0;
    }

    place     = info->whichbit >> 3;
    bitstring = 0;
    for (i = (numbits >> 3) + 1; i >= 0; i--) {
        if (i + place < dlen)
            bitstring |= data[place + i] << (8 * (2 - i));
    }
//     bitstring = data[place + 2] | (long)(data[place + 1]) << 8 |
//                 (long)(data[place]) << 16;
    bitstring >>= 24 - (info->whichbit & 7) - numbits;
    bitstring &= (0xffffffff >> (32 - numbits));
    // Okay, so this could be made faster with a table lookup.
    // It doesn't matter. It's fast enough as it is.
    info->whichbit += numbits;
    return bitstring;
}

static void decryptinit3(Decrypt3Info *info)
{
    memset(info, 0, sizeof(Decrypt3Info));
    info->numbits  = 9;
    info->curtoken = 0x102;
    info->endtoken = 0x1ff;
    gbits(info, 0, 0, 0);
}

static int decryptComp3Helper(Decrypt3Info *info,
                              uint8_t      *dest,
                              uint8_t      *src,
                              int           length,
                              int           complength,
                              int16_t      *token)
{
    // while(length != 0) {
    while (length >= 0) {
        switch (info->decryptstart) {
            case 0:
            case 1:
                // bitstring = gbits(numbits, src, complength);
                info->bitstring =
                  (uint16_t)gbits(info, info->numbits, src, complength);
                if (info->bitstring == 0x101) { // found end-of-data signal
                    info->decryptstart = 4;
                    return 0;
                }
                if (info->decryptstart == 0) { // first char
                    info->decryptstart = 1;
                    info->lastbits     = info->bitstring;
                    *(dest++) = info->lastchar = (info->bitstring & 0xff);
                    if (--length != 0)
                        continue;
                    return 0;
                }
                if (info->bitstring == 0x100) { // start-over signal
                    info->numbits      = 9;
                    info->endtoken     = 0x1ff;
                    info->curtoken     = 0x102;
                    info->decryptstart = 0;
                    continue;
                }
                *token = info->bitstring;
                if (*token >= info->curtoken) { // index past current point
                    *token = info->lastbits;
                    if (info->stakptr >= ARRAYSIZE(info->stak)) {
                        return -1;
                    }
                    info->stak[info->stakptr++] = info->lastchar;
                }
                while ((*token > 0xff) &&
                       (*token < 0x1004)) { // follow links back in data
                    if (info->stakptr >= ARRAYSIZE(info->stak)) {
                        return -1;
                    }
                    info->stak[info->stakptr++] = info->tokens[*token].data;
                    *token                      = info->tokens[*token].next;
                }
                if (info->stakptr >= ARRAYSIZE(info->stak)) {
                    return -1;
                }
                info->lastchar = info->stak[info->stakptr++] = *token & 0xff;
            case 2:
                while (info->stakptr > 0) { // put stack in buffer
                    if (info->stakptr >= ARRAYSIZE(info->stak)) {
                        return -1;
                    }
                    *(dest++) = info->stak[--info->stakptr];
                    length--;
                    if (length == 0) {
                        info->decryptstart = 2;
                        return 0;
                    }
                }
                info->decryptstart = 1;
                if (info->curtoken <= info->endtoken) { // put token into record
                    info->tokens[info->curtoken].data = info->lastchar;
                    info->tokens[info->curtoken].next = info->lastbits;
                    info->curtoken++;
                    if (info->curtoken == info->endtoken &&
                        info->numbits != 12) {
                        info->numbits++;
                        info->endtoken <<= 1;
                        info->endtoken++;
                    }
                }
                info->lastbits = info->bitstring;
                continue; // When are "break" and "continue" synonymous?
            case 4:
                return 0;
        }
    }
    return 0; // [DJ] shut up compiler warning
}

bool DecompressLZW_1(uint8_t *dest, uint8_t *src, int length, int complength)
{
    int           res;
    int16_t       token;
    Decrypt3Info *info = (Decrypt3Info *)malloc(sizeof(Decrypt3Info));
    decryptinit3(info);
    res = decryptComp3Helper(info, dest, src, length, complength, &token);
    free(info);
    return (res == 0);
}

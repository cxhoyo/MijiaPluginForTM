// MiioDevice.cpp - miIO协议C++实现
// 参考 Python 版 miio_proto.py，实现 AES-128-CBC + MD5 + UDP
#include "pch.h"
#include "MiioDevice.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdexcept>

// ════════════════════════════════════════
// MD5 实现（RFC 1321）
// ════════════════════════════════════════
namespace MiioMD5 {

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

static void encode(unsigned char* output, const unsigned int* input, unsigned int len) {
    for (unsigned int i = 0, j = 0; j < len; i++, j += 4) {
        output[j]   = (unsigned char)(input[i] & 0xFF);
        output[j+1] = (unsigned char)((input[i] >> 8) & 0xFF);
        output[j+2] = (unsigned char)((input[i] >> 16) & 0xFF);
        output[j+3] = (unsigned char)((input[i] >> 24) & 0xFF);
    }
}
static void decode(unsigned int* output, const unsigned char* input, unsigned int len) {
    for (unsigned int i = 0, j = 0; j < len; i++, j += 4)
        output[i] = ((unsigned int)input[j]) | (((unsigned int)input[j+1]) << 8) |
                    (((unsigned int)input[j+2]) << 16) | (((unsigned int)input[j+3]) << 24);
}

#define F(x,y,z) (((x)&(y))|((~x)&(z)))
#define G(x,y,z) (((x)&(z))|((y)&(~z)))
#define H(x,y,z) ((x)^(y)^(z))
#define I(x,y,z) ((y)^((x)|(~z)))
#define ROTATE_LEFT(x,n) (((x)<<(n))|((x)>>(32-(n))))
#define FF(a,b,c,d,x,s,ac) { (a)+=F((b),(c),(d))+(x)+(unsigned int)(ac); (a)=ROTATE_LEFT((a),(s)); (a)+=(b); }
#define GG(a,b,c,d,x,s,ac) { (a)+=G((b),(c),(d))+(x)+(unsigned int)(ac); (a)=ROTATE_LEFT((a),(s)); (a)+=(b); }
#define HH(a,b,c,d,x,s,ac) { (a)+=H((b),(c),(d))+(x)+(unsigned int)(ac); (a)=ROTATE_LEFT((a),(s)); (a)+=(b); }
#define II(a,b,c,d,x,s,ac) { (a)+=I((b),(c),(d))+(x)+(unsigned int)(ac); (a)=ROTATE_LEFT((a),(s)); (a)+=(b); }

static unsigned char PADDING[64] = {
    0x80,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

void Init(MD5Context* ctx) {
    ctx->count[0] = ctx->count[1] = 0;
    ctx->state[0] = 0x67452301; ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe; ctx->state[3] = 0x10325476;
}

static void Transform(unsigned int state[4], const unsigned char block[64]) {
    unsigned int a=state[0],b=state[1],c=state[2],d=state[3];
    unsigned int x[16];
    decode(x, block, 64);
    FF(a,b,c,d,x[0],S11,0xd76aa478); FF(d,a,b,c,x[1],S12,0xe8c7b756);
    FF(c,d,a,b,x[2],S13,0x242070db); FF(b,c,d,a,x[3],S14,0xc1bdceee);
    FF(a,b,c,d,x[4],S11,0xf57c0faf); FF(d,a,b,c,x[5],S12,0x4787c62a);
    FF(c,d,a,b,x[6],S13,0xa8304613); FF(b,c,d,a,x[7],S14,0xfd469501);
    FF(a,b,c,d,x[8],S11,0x698098d8); FF(d,a,b,c,x[9],S12,0x8b44f7af);
    FF(c,d,a,b,x[10],S13,0xffff5bb1); FF(b,c,d,a,x[11],S14,0x895cd7be);
    FF(a,b,c,d,x[12],S11,0x6b901122); FF(d,a,b,c,x[13],S12,0xfd987193);
    FF(c,d,a,b,x[14],S13,0xa679438e); FF(b,c,d,a,x[15],S14,0x49b40821);
    GG(a,b,c,d,x[1],S21,0xf61e2562); GG(d,a,b,c,x[6],S22,0xc040b340);
    GG(c,d,a,b,x[11],S23,0x265e5a51); GG(b,c,d,a,x[0],S24,0xe9b6c7aa);
    GG(a,b,c,d,x[5],S21,0xd62f105d); GG(d,a,b,c,x[10],S22,0x2441453);
    GG(c,d,a,b,x[15],S23,0xd8a1e681); GG(b,c,d,a,x[4],S24,0xe7d3fbc8);
    GG(a,b,c,d,x[9],S21,0x21e1cde6); GG(d,a,b,c,x[14],S22,0xc33707d6);
    GG(c,d,a,b,x[3],S23,0xf4d50d87); GG(b,c,d,a,x[8],S24,0x455a14ed);
    GG(a,b,c,d,x[13],S21,0xa9e3e905); GG(d,a,b,c,x[2],S22,0xfcefa3f8);
    GG(c,d,a,b,x[7],S23,0x676f02d9); GG(b,c,d,a,x[12],S24,0x8d2a4c8a);
    HH(a,b,c,d,x[5],S31,0xfffa3942); HH(d,a,b,c,x[8],S32,0x8771f681);
    HH(c,d,a,b,x[11],S33,0x6d9d6122); HH(b,c,d,a,x[14],S34,0xfde5380c);
    HH(a,b,c,d,x[1],S31,0xa4beea44); HH(d,a,b,c,x[4],S32,0x4bdecfa9);
    HH(c,d,a,b,x[7],S33,0xf6bb4b60); HH(b,c,d,a,x[10],S34,0xbebfbc70);
    HH(a,b,c,d,x[13],S31,0x289b7ec6); HH(d,a,b,c,x[0],S32,0xeaa127fa);
    HH(c,d,a,b,x[3],S33,0xd4ef3085); HH(b,c,d,a,x[6],S34,0x4881d05);
    HH(a,b,c,d,x[9],S31,0xd9d4d039); HH(d,a,b,c,x[12],S32,0xe6db99e5);
    HH(c,d,a,b,x[15],S33,0x1fa27cf8); HH(b,c,d,a,x[2],S34,0xc4ac5665);
    II(a,b,c,d,x[0],S41,0xf4292244); II(d,a,b,c,x[7],S42,0x432aff97);
    II(c,d,a,b,x[14],S43,0xab9423a7); II(b,c,d,a,x[5],S44,0xfc93a039);
    II(a,b,c,d,x[12],S41,0x655b59c3); II(d,a,b,c,x[3],S42,0x8f0ccc92);
    II(c,d,a,b,x[10],S43,0xffeff47d); II(b,c,d,a,x[1],S44,0x85845dd1);
    II(a,b,c,d,x[8],S41,0x6fa87e4f); II(d,a,b,c,x[15],S42,0xfe2ce6e0);
    II(c,d,a,b,x[6],S43,0xa3014314); II(b,c,d,a,x[13],S44,0x4e0811a1);
    II(a,b,c,d,x[4],S41,0xf7537e82); II(d,a,b,c,x[11],S42,0xbd3af235);
    II(c,d,a,b,x[2],S43,0x2ad7d2bb); II(b,c,d,a,x[9],S44,0xeb86d391);
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
}

void Update(MD5Context* ctx, const unsigned char* input, unsigned int inputLen) {
    unsigned int index = (ctx->count[0] >> 3) & 0x3F;
    if ((ctx->count[0] += ((unsigned int)inputLen << 3)) < ((unsigned int)inputLen << 3))
        ctx->count[1]++;
    ctx->count[1] += ((unsigned int)inputLen >> 29);
    unsigned int partLen = 64 - index;
    unsigned int i = 0;
    if (inputLen >= partLen) {
        memcpy(&ctx->buffer[index], input, partLen);
        Transform(ctx->state, ctx->buffer);
        for (i = partLen; i + 63 < inputLen; i += 64)
            Transform(ctx->state, &input[i]);
        index = 0;
    }
    memcpy(&ctx->buffer[index], &input[i], inputLen - i);
}

void Final(unsigned char digest[16], MD5Context* ctx) {
    unsigned char bits[8];
    encode(bits, ctx->count, 8);
    unsigned int index = (ctx->count[0] >> 3) & 0x3f;
    unsigned int padLen = (index < 56) ? (56 - index) : (120 - index);
    Update(ctx, PADDING, padLen);
    Update(ctx, bits, 8);
    encode(digest, ctx->state, 16);
}

void Compute(const unsigned char* data, size_t len, unsigned char out[16]) {
    MD5Context ctx;
    Init(&ctx);
    Update(&ctx, data, (unsigned int)len);
    Final(out, &ctx);
}

} // namespace MiioMD5

// ════════════════════════════════════════
// AES-128-CBC 实现（FIPS 197）
// ════════════════════════════════════════
namespace MiioAES {

// S-box
static const unsigned char sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

// Inverse S-box
static const unsigned char rsbox[256] = {
    0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
    0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
    0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
    0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
    0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
    0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
    0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
    0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
    0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
    0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
    0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
    0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
    0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
    0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
    0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
    0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
};

static const unsigned int Rcon[11] = {
    0x00000000,0x01000000,0x02000000,0x04000000,0x08000000,
    0x10000000,0x20000000,0x40000000,0x80000000,0x1b000000,0x36000000
};

static unsigned char xtime(unsigned char x) { return ((x << 1) ^ (x & 0x80 ? 0x1b : 0)); }
static unsigned char gmul(unsigned char a, unsigned char b) {
    unsigned char p = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1) p ^= a;
        bool carry = (a & 0x80) != 0;
        a <<= 1;
        if (carry) a ^= 0x1b;
        b >>= 1;
    }
    return p;
}

struct AESCtx {
    unsigned char roundKey[176]; // 11 * 16
};

static void KeyExpansion(AESCtx* ctx, const unsigned char key[16]) {
    memcpy(ctx->roundKey, key, 16);
    for (int i = 4; i < 44; i++) {
        unsigned char temp[4];
        memcpy(temp, ctx->roundKey + (i-1)*4, 4);
        if (i % 4 == 0) {
            // RotWord
            unsigned char t = temp[0]; temp[0]=temp[1]; temp[1]=temp[2]; temp[2]=temp[3]; temp[3]=t;
            // SubWord
            for (int j=0;j<4;j++) temp[j] = sbox[temp[j]];
            // XOR Rcon
            temp[0] ^= (Rcon[i/4] >> 24) & 0xFF;
        }
        for (int j=0;j<4;j++)
            ctx->roundKey[i*4+j] = ctx->roundKey[(i-4)*4+j] ^ temp[j];
    }
}

static void AddRoundKey(unsigned char state[4][4], const unsigned char* roundKey) {
    for (int r=0;r<4;r++) for (int c=0;c<4;c++)
        state[r][c] ^= roundKey[c*4+r];
}
static void SubBytes(unsigned char state[4][4]) {
    for (int r=0;r<4;r++) for (int c=0;c<4;c++) state[r][c] = sbox[state[r][c]];
}
static void InvSubBytes(unsigned char state[4][4]) {
    for (int r=0;r<4;r++) for (int c=0;c<4;c++) state[r][c] = rsbox[state[r][c]];
}
static void ShiftRows(unsigned char state[4][4]) {
    unsigned char t;
    t=state[1][0]; state[1][0]=state[1][1]; state[1][1]=state[1][2]; state[1][2]=state[1][3]; state[1][3]=t;
    t=state[2][0]; state[2][0]=state[2][2]; state[2][2]=t; t=state[2][1]; state[2][1]=state[2][3]; state[2][3]=t;
    t=state[3][3]; state[3][3]=state[3][2]; state[3][2]=state[3][1]; state[3][1]=state[3][0]; state[3][0]=t;
}
static void InvShiftRows(unsigned char state[4][4]) {
    unsigned char t;
    t=state[1][3]; state[1][3]=state[1][2]; state[1][2]=state[1][1]; state[1][1]=state[1][0]; state[1][0]=t;
    t=state[2][0]; state[2][0]=state[2][2]; state[2][2]=t; t=state[2][1]; state[2][1]=state[2][3]; state[2][3]=t;
    t=state[3][0]; state[3][0]=state[3][1]; state[3][1]=state[3][2]; state[3][2]=state[3][3]; state[3][3]=t;
}
static void MixColumns(unsigned char state[4][4]) {
    for (int c=0;c<4;c++) {
        unsigned char a=state[0][c],b=state[1][c],cc=state[2][c],d=state[3][c];
        state[0][c] = gmul(2,a)^gmul(3,b)^cc^d;
        state[1][c] = a^gmul(2,b)^gmul(3,cc)^d;
        state[2][c] = a^b^gmul(2,cc)^gmul(3,d);
        state[3][c] = gmul(3,a)^b^cc^gmul(2,d);
    }
}
static void InvMixColumns(unsigned char state[4][4]) {
    for (int c=0;c<4;c++) {
        unsigned char a=state[0][c],b=state[1][c],cc=state[2][c],d=state[3][c];
        state[0][c] = gmul(0x0e,a)^gmul(0x0b,b)^gmul(0x0d,cc)^gmul(0x09,d);
        state[1][c] = gmul(0x09,a)^gmul(0x0e,b)^gmul(0x0b,cc)^gmul(0x0d,d);
        state[2][c] = gmul(0x0d,a)^gmul(0x09,b)^gmul(0x0e,cc)^gmul(0x0b,d);
        state[3][c] = gmul(0x0b,a)^gmul(0x0d,b)^gmul(0x09,cc)^gmul(0x0e,d);
    }
}

static void BlockEncrypt(AESCtx* ctx, unsigned char in[16], unsigned char out[16]) {
    unsigned char state[4][4];
    for (int r=0;r<4;r++) for (int c=0;c<4;c++) state[r][c] = in[c*4+r];
    AddRoundKey(state, ctx->roundKey);
    for (int round=1; round<10; round++) {
        SubBytes(state); ShiftRows(state); MixColumns(state);
        AddRoundKey(state, ctx->roundKey + round*16);
    }
    SubBytes(state); ShiftRows(state);
    AddRoundKey(state, ctx->roundKey + 10*16);
    for (int r=0;r<4;r++) for (int c=0;c<4;c++) out[c*4+r] = state[r][c];
}

static void BlockDecrypt(AESCtx* ctx, unsigned char in[16], unsigned char out[16]) {
    unsigned char state[4][4];
    for (int r=0;r<4;r++) for (int c=0;c<4;c++) state[r][c] = in[c*4+r];
    AddRoundKey(state, ctx->roundKey + 10*16);
    for (int round=9; round>0; round--) {
        InvShiftRows(state); InvSubBytes(state);
        AddRoundKey(state, ctx->roundKey + round*16);
        InvMixColumns(state);
    }
    InvShiftRows(state); InvSubBytes(state);
    AddRoundKey(state, ctx->roundKey);
    for (int r=0;r<4;r++) for (int c=0;c<4;c++) out[c*4+r] = state[r][c];
}

bool Encrypt(const unsigned char key[16], const unsigned char iv[16],
             const unsigned char* plaintext, size_t plainLen,
             std::vector<unsigned char>& ciphertext) {
    // PKCS7 padding
    size_t padLen  = 16 - (plainLen % 16);
    size_t totalLen = plainLen + padLen;
    std::vector<unsigned char> buf(totalLen);
    memcpy(buf.data(), plaintext, plainLen);
    memset(buf.data() + plainLen, (unsigned char)padLen, padLen);

    ciphertext.resize(totalLen);
    AESCtx ctx;
    KeyExpansion(&ctx, key);
    unsigned char prev[16];
    memcpy(prev, iv, 16);
    for (size_t i = 0; i < totalLen; i += 16) {
        unsigned char block[16];
        for (int j = 0; j < 16; j++) block[j] = buf[i+j] ^ prev[j];
        BlockEncrypt(&ctx, block, ciphertext.data() + i);
        memcpy(prev, ciphertext.data() + i, 16);
    }
    return true;
}

bool Decrypt(const unsigned char key[16], const unsigned char iv[16],
             const unsigned char* ciphertext, size_t cipherLen,
             std::vector<unsigned char>& plaintext) {
    if (cipherLen == 0 || cipherLen % 16 != 0) return false;
    plaintext.resize(cipherLen);
    AESCtx ctx;
    KeyExpansion(&ctx, key);
    unsigned char prev[16];
    memcpy(prev, iv, 16);
    for (size_t i = 0; i < cipherLen; i += 16) {
        unsigned char block[16];
        BlockDecrypt(&ctx, (unsigned char*)(ciphertext + i), block);
        for (int j = 0; j < 16; j++) plaintext[i+j] = block[j] ^ prev[j];
        memcpy(prev, ciphertext + i, 16);
    }
    // 去 PKCS7 padding
    if (!plaintext.empty()) {
        unsigned char padLen = plaintext.back();
        if (padLen > 0 && padLen <= 16 && padLen <= plaintext.size()) {
            plaintext.resize(plaintext.size() - padLen);
        }
    }
    return true;
}

} // namespace MiioAES

// ════════════════════════════════════════
// MiioDevice 实现
// ════════════════════════════════════════

// HEX 字符串转字节
static bool HexToBytes(const std::string& hex, unsigned char* out, size_t outLen) {
    if (hex.size() != outLen * 2) return false;
    for (size_t i = 0; i < outLen; i++) {
        unsigned int v = 0;
        if (sscanf_s(hex.c_str() + i * 2, "%2x", &v) != 1) return false;
        out[i] = (unsigned char)v;
    }
    return true;
}

// Big-endian write
static void WriteBE16(unsigned char* p, unsigned short v) { p[0]=(v>>8)&0xFF; p[1]=v&0xFF; }
static void WriteBE32(unsigned char* p, unsigned int v) { p[0]=(v>>24)&0xFF; p[1]=(v>>16)&0xFF; p[2]=(v>>8)&0xFF; p[3]=v&0xFF; }
static unsigned int ReadBE32(const unsigned char* p) { return ((unsigned int)p[0]<<24)|((unsigned int)p[1]<<16)|((unsigned int)p[2]<<8)|p[3]; }

static bool ExtractJsonStringField(const std::string& json, const char* key, std::string& outValue) {
    std::string needle = "\"";
    needle += key;
    needle += "\":\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return false;
    pos += needle.size();
    auto end = json.find('"', pos);
    if (end == std::string::npos) return false;
    outValue = json.substr(pos, end - pos);
    return true;
}

static const MiioModelProfile MODEL_PROFILES[] = {
    { "chuangmi.plug.212a01", {5, 6}, 0.01 },
};

MiioDevice::MiioDevice(const std::string& ip, const std::string& token, int timeoutMs)
    : m_ip(ip), m_timeoutMs(timeoutMs), m_handshaked(false) {
    // token hex -> bytes
    memset(m_token, 0, 16);
    HexToBytes(token, m_token, 16);
    // key = MD5(token)
    MiioMD5::Compute(m_token, 16, m_key);
    // iv = MD5(key + token)
    unsigned char keyToken[32];
    memcpy(keyToken,    m_key,   16);
    memcpy(keyToken+16, m_token, 16);
    MiioMD5::Compute(keyToken, 32, m_iv);
}

unsigned int MiioDevice::CurrentStamp() const {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    long long sec = std::chrono::duration_cast<std::chrono::seconds>(now).count();
    return (unsigned int)(sec - m_stampDelta);
}

bool MiioDevice::UdpSendRecv(const std::vector<unsigned char>& sendBuf,
                              std::vector<unsigned char>& recvBuf, int recvMax) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) return false;

    DWORD timeout = (DWORD)m_timeoutMs;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(54321);
    inet_pton(AF_INET, m_ip.c_str(), &addr.sin_addr);

    sendto(sock, (char*)sendBuf.data(), (int)sendBuf.size(), 0, (sockaddr*)&addr, sizeof(addr));

    recvBuf.resize(recvMax);
    int n = recv(sock, (char*)recvBuf.data(), recvMax, 0);
    closesocket(sock);

    if (n <= 0) return false;
    recvBuf.resize(n);
    return true;
}

bool MiioDevice::Handshake() {
    // 发送 HELLO 包
    static const unsigned char HELLO[] = {
        0x21,0x31,0x00,0x20,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
    };
    std::vector<unsigned char> req(HELLO, HELLO+32);
    std::vector<unsigned char> resp;
    if (!UdpSendRecv(req, resp)) return false;
    if (resp.size() < 32) return false;

    m_deviceId    = ReadBE32(resp.data() + 8);
    m_serverStamp = ReadBE32(resp.data() + 12);
    auto now = std::chrono::system_clock::now().time_since_epoch();
    long long sec = std::chrono::duration_cast<std::chrono::seconds>(now).count();
    m_stampDelta  = sec - (long long)m_serverStamp;
    m_handshaked  = true;
    return true;
}

std::vector<unsigned char> MiioDevice::Encrypt(const std::string& plaintext) {
    std::vector<unsigned char> cipher;
    MiioAES::Encrypt(m_key, m_iv,
                     (const unsigned char*)plaintext.c_str(), plaintext.size(),
                     cipher);
    return cipher;
}

std::string MiioDevice::Decrypt(const std::vector<unsigned char>& ciphertext) {
    std::vector<unsigned char> plain;
    if (!MiioAES::Decrypt(m_key, m_iv, ciphertext.data(), ciphertext.size(), plain))
        return "";
    // 去掉尾部 \0
    while (!plain.empty() && plain.back() == 0) plain.pop_back();
    return std::string(plain.begin(), plain.end());
}

std::vector<unsigned char> MiioDevice::BuildPacket(const std::string& payloadJson) {
    auto payload = Encrypt(payloadJson);
    unsigned int stamp  = CurrentStamp();
    unsigned short pktLen = (unsigned short)(32 + payload.size());

    // header without checksum
    unsigned char header[16];
    WriteBE16(header+0, 0x2131);
    WriteBE16(header+2, pktLen);
    WriteBE32(header+4, 0);
    WriteBE32(header+8, m_deviceId);
    WriteBE32(header+12, stamp);

    // checksum = MD5(header + token + payload)
    std::vector<unsigned char> checksumBuf;
    checksumBuf.insert(checksumBuf.end(), header, header+16);
    checksumBuf.insert(checksumBuf.end(), m_token, m_token+16);
    checksumBuf.insert(checksumBuf.end(), payload.begin(), payload.end());
    unsigned char checksum[16];
    MiioMD5::Compute(checksumBuf.data(), checksumBuf.size(), checksum);

    // final packet
    std::vector<unsigned char> pkt;
    pkt.insert(pkt.end(), header, header+16);
    pkt.insert(pkt.end(), checksum, checksum+16);
    pkt.insert(pkt.end(), payload.begin(), payload.end());
    return pkt;
}

std::string MiioDevice::ParsePacket(const std::vector<unsigned char>& data) {
    if (data.size() <= 32) return "";
    std::vector<unsigned char> payloadEnc(data.begin()+32, data.end());
    return Decrypt(payloadEnc);
}

bool MiioDevice::Send(const std::string& method, const std::string& paramsJson, std::string& outResult) {
    if (!m_handshaked) {
        if (!Handshake()) return false;
    }
    int msgId = m_msgId++;
    // 构建 JSON
    std::ostringstream oss;
    oss << "{\"id\":" << msgId << ",\"method\":\"" << method << "\",\"params\":" << paramsJson << "}";
    std::string payload = oss.str();

    auto pkt = BuildPacket(payload);
    std::vector<unsigned char> resp;
    if (!UdpSendRecv(pkt, resp)) return false;

    std::string respJson = ParsePacket(resp);
    if (respJson.empty()) return false;

    // 简单解析 "result" 字段
    // 找到 "result": 后面的内容
    auto pos = respJson.find("\"result\":");
    if (pos == std::string::npos) {
        // 可能是错误，返回整个响应
        outResult = respJson;
        return true;
    }
    pos += 9;
    // 跳过空格
    while (pos < respJson.size() && respJson[pos] == ' ') pos++;
    outResult = respJson.substr(pos);
    // 去掉末尾的 }
    if (!outResult.empty() && outResult.back() == '}')
        outResult.pop_back();
    return true;
}

bool MiioDevice::QueryDeviceModel() {
    if (m_modelQueried) return !m_model.empty();

    m_modelQueried = true;
    std::string result;
    if (!Send("miIO.info", "[]", result)) return false;
    return ExtractJsonStringField(result, "model", m_model);
}

bool MiioDevice::ReadNumericProperty(const MiioProperty& prop, double scale, double& outValue) {
    std::ostringstream params;
    params << "[{\"did\":\"prop\",\"siid\":" << prop.siid << ",\"piid\":" << prop.piid << "}]";

    std::string result;
    if (!Send("get_properties", params.str(), result)) return false;
    // 在结果中找 "value": 数字
    auto pos = result.find("\"value\":");
    if (pos == std::string::npos) return false;
    pos += 8;
    while (pos < result.size() && (result[pos]==' ' || result[pos]=='\t')) pos++;
    double val = 0.0;
    try { val = std::stod(result.substr(pos)); }
    catch (...) { return false; }
    outValue = val * scale;
    return true;
}

bool MiioDevice::GetPower(double& outWatts) {
    if (QueryDeviceModel()) {
        for (const auto& profile : MODEL_PROFILES) {
            if (m_model == profile.model) {
                return ReadNumericProperty(profile.powerProperty, profile.powerScale, outWatts);
            }
        }
    }

    // Legacy default path for existing supported models.
    if (ReadNumericProperty({11, 2}, 1.0, outWatts)) return true;

    return false;
}

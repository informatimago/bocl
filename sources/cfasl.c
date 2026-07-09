/* cfasl.c — compact binary fast-load file (issue plan-1-compact-fasl).
   cfasl-1: the low-level byte / varint / float I/O primitives (see cfasl.h for the
   format spec).  The object-graph writer (cfasl-2) and loader (cfasl-3) build on
   these.  All multibyte integers are LEB128 varints and floats are little-endian,
   so the file is endianness-independent. */

#include "cfasl.h"
#include <stdlib.h>
#include <string.h>

/* ---- Output buffer ------------------------------------------------------ */

void cfasl_out_init(cfasl_out* b){ b->data=NULL; b->len=0; b->cap=0; b->error=0; }
void cfasl_out_free(cfasl_out* b){ free(b->data); b->data=NULL; b->len=b->cap=0; }

static void cfasl_out_reserve(cfasl_out* b, size_t extra){
    if(b->error){ return; }
    if(b->len + extra <= b->cap){ return; }
    size_t ncap = b->cap ? b->cap*2 : 1024;
    while(ncap < b->len + extra){ ncap *= 2; }
    uint8_t* nd = (uint8_t*)realloc(b->data, ncap);
    if(!nd){ b->error = 1; return; }
    b->data = nd; b->cap = ncap;
}

void cfasl_put_u8(cfasl_out* b, uint8_t v){
    cfasl_out_reserve(b, 1);
    if(b->error){ return; }
    b->data[b->len++] = v;
}

void cfasl_put_bytes(cfasl_out* b, const void* p, size_t n){
    cfasl_out_reserve(b, n);
    if(b->error){ return; }
    memcpy(b->data + b->len, p, n);
    b->len += n;
}

void cfasl_put_uvarint(cfasl_out* b, uint64_t v){
    uint8_t tmp[10]; int n = 0;
    do { uint8_t byte = (uint8_t)(v & 0x7fu); v >>= 7;
         if(v){ byte |= 0x80u; } tmp[n++] = byte; } while(v);
    cfasl_put_bytes(b, tmp, (size_t)n);
}

void cfasl_put_svarint(cfasl_out* b, int64_t v){
    /* zig-zag */
    uint64_t zz = ((uint64_t)v << 1) ^ (uint64_t)(v >> 63);
    cfasl_put_uvarint(b, zz);
}

void cfasl_put_f32(cfasl_out* b, float v){
    uint32_t bits; memcpy(&bits, &v, 4);
    uint8_t le[4];
    for(int i=0;i<4;i++){ le[i] = (uint8_t)(bits & 0xff); bits >>= 8; }
    cfasl_put_bytes(b, le, 4);
}

void cfasl_put_f64(cfasl_out* b, double v){
    uint64_t bits; memcpy(&bits, &v, 8);
    uint8_t le[8];
    for(int i=0;i<8;i++){ le[i] = (uint8_t)(bits & 0xff); bits >>= 8; }
    cfasl_put_bytes(b, le, 8);
}

/* ---- Input cursor ------------------------------------------------------- */

void cfasl_in_init(cfasl_in* c, const uint8_t* data, size_t len){
    c->data = data; c->len = len; c->pos = 0; c->error = 0;
}

int cfasl_at_end(cfasl_in* c){ return c->pos >= c->len; }

uint8_t cfasl_get_u8(cfasl_in* c){
    if(c->pos >= c->len){ c->error = 1; return 0; }
    return c->data[c->pos++];
}

void cfasl_get_bytes(cfasl_in* c, void* dst, size_t n){
    if(c->pos + n > c->len){ c->error = 1; return; }
    memcpy(dst, c->data + c->pos, n);
    c->pos += n;
}

uint64_t cfasl_get_uvarint(cfasl_in* c){
    uint64_t v = 0; int shift = 0;
    for(;;){
        if(c->pos >= c->len){ c->error = 1; return v; }
        uint8_t byte = c->data[c->pos++];
        v |= (uint64_t)(byte & 0x7fu) << shift;
        if(!(byte & 0x80u)){ break; }
        shift += 7;
        if(shift >= 64){ c->error = 1; break; }
    }
    return v;
}

int64_t cfasl_get_svarint(cfasl_in* c){
    uint64_t zz = cfasl_get_uvarint(c);
    return (int64_t)(zz >> 1) ^ -(int64_t)(zz & 1);   /* un-zig-zag */
}

float cfasl_get_f32(cfasl_in* c){
    uint8_t le[4]; cfasl_get_bytes(c, le, 4);
    uint32_t bits = 0;
    for(int i=3;i>=0;i--){ bits = (bits << 8) | le[i]; }
    float v; memcpy(&v, &bits, 4); return v;
}

double cfasl_get_f64(cfasl_in* c){
    uint8_t le[8]; cfasl_get_bytes(c, le, 8);
    uint64_t bits = 0;
    for(int i=7;i>=0;i--){ bits = (bits << 8) | le[i]; }
    double v; memcpy(&v, &bits, 8); return v;
}

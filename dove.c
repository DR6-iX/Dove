#include "dove.h"
#include <string.h>

#define STATE_SIZE 16
#define ROUNDS 20
#define BLOCK_SIZE 64
#define WORD_SIZE 32

static uint32_t state[STATE_SIZE];
static uint32_t counter_high = 0;
static uint32_t counter_low = 0;

static uint32_t rotleft(uint32_t value, int shift) {
    return (value << shift) | (value >> (WORD_SIZE - shift));
}

static uint32_t rotright(uint32_t value, int shift) {
    return (value >> shift) | (value << (WORD_SIZE - shift));
}

// Enhanced non-linear mixing function
static uint32_t dove_mix(uint32_t a, uint32_t b, uint32_t c) {
    a ^= rotleft(b, 7) ^ rotright(c, 13);
    b ^= rotleft(c, 9) ^ rotright(a, 11);
    c ^= rotleft(a, 15) ^ rotright(b, 5);
    
    // Additional non-linear operations
    a *= 0x6170766f;  // Large prime constant
    b += rotleft(c, 13);
    c ^= rotright(a, 11);
    
    return a ^ b ^ c;
}

// Enhanced transform with better non-linearity
static uint32_t dove_transform(uint32_t x) {
    x ^= rotleft(x, 5) ^ rotright(x, 7);
    x *= 0x6170766f;  // Prime constant for multiplication diffusion
    x ^= rotleft(x, 13);
    x += rotright(x, 11);
    x ^= (x >> 17) | (x << 15);  // Additional mixing
    return x;
}

static void key_schedule(const uint8_t* key, size_t key_length, const uint8_t* nonce) {
    uint32_t temp[STATE_SIZE];
    
    // Initial state filling with key material
    for (size_t i = 0; i < STATE_SIZE; i++) {
        temp[i] = ((uint32_t)key[i % key_length] << 24) |
                 ((uint32_t)key[(i + 1) % key_length] << 16) |
                 ((uint32_t)key[(i + 2) % key_length] << 8) |
                 ((uint32_t)key[(i + 3) % key_length]);
    }
    
    // Mix nonce with complex diffusion
    for (size_t i = 0; i < 4; i++) {
        uint32_t nonce_word = ((uint32_t)nonce[i*4] << 24) |
                             ((uint32_t)nonce[i*4+1] << 16) |
                             ((uint32_t)nonce[i*4+2] << 8) |
                             ((uint32_t)nonce[i*4+3]);
        
        for (size_t j = 0; j < STATE_SIZE; j++) {
            temp[j] ^= dove_transform(nonce_word + j);
        }
    }
    
    // Multiple mixing rounds for better diffusion
    for (int round = 0; round < ROUNDS; round++) {
        for (size_t i = 0; i < STATE_SIZE; i++) {
            temp[i] = dove_transform(temp[i]);
            temp[i] ^= dove_mix(
                temp[(i+1) % STATE_SIZE],
                temp[(i+7) % STATE_SIZE],
                temp[(i+13) % STATE_SIZE]
            );
        }
    }
    
    memcpy(state, temp, sizeof(state));
}

void dove_init(const uint8_t* key, size_t key_length, const uint8_t* nonce) {
    key_schedule(key, key_length, nonce);
    counter_high = 0;
    counter_low = 0;
}

void dove_crypt(uint8_t* data, size_t length) {
    uint32_t keystream[BLOCK_SIZE/4];
    size_t processed = 0;
    
    while (processed < length) {
        // Copy state with counter incorporation
        memcpy(keystream, state, sizeof(keystream));
        
        // Enhanced counter incorporation affecting all state words
        for (int i = 0; i < STATE_SIZE; i++) {
            keystream[i] ^= dove_transform(counter_low + i);
            keystream[i] ^= dove_transform(counter_high + i);
        }
        
        // Main encryption rounds with improved diffusion
        for (int r = 0; r < ROUNDS; r++) {
            // Column rounds with full state mixing
            for (int i = 0; i < STATE_SIZE; i++) {
                keystream[i] = dove_transform(keystream[i]);
                keystream[i] ^= dove_mix(
                    keystream[(i + 1) % STATE_SIZE],
                    keystream[(i + 7) % STATE_SIZE],
                    keystream[(i + 13) % STATE_SIZE]
                );
            }
            
            // Diagonal rounds with additional mixing
            for (int i = 0; i < STATE_SIZE; i++) {
                int idx = (i * 5) % STATE_SIZE;
                keystream[idx] = dove_transform(keystream[idx]);
                keystream[idx] ^= dove_mix(
                    keystream[(idx + 3) % STATE_SIZE],
                    keystream[(idx + 9) % STATE_SIZE],
                    keystream[(idx + 14) % STATE_SIZE]
                );
            }
        }
        
        // XOR keystream with data
        size_t block_size = (length - processed < BLOCK_SIZE) ? 
                            length - processed : BLOCK_SIZE;
        
        for (size_t i = 0; i < block_size; i++) {
            data[processed + i] ^= ((uint8_t*)keystream)[i];
        }
        
        // Update counters with overflow handling
        counter_low++;
        if (counter_low == 0) {
            counter_high++;
        }
        
        // Enhanced state update after each block
        for (int i = 0; i < STATE_SIZE; i++) {
            state[i] = dove_transform(state[i] ^ keystream[i]);
            state[i] ^= dove_mix(
                state[(i+1) % STATE_SIZE],
                state[(i+7) % STATE_SIZE],
                state[(i+13) % STATE_SIZE]
            );
        }
        
        processed += block_size;
    }
}

void dove_reset(void) {
    memset(state, 0, sizeof(state));
    counter_high = 0;
    counter_low = 0;
}

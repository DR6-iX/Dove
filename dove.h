#ifndef DOVE_H
#define DOVE_H

#include <stdint.h>
#include <stddef.h>

void dove_init(const uint8_t* key, size_t key_length, const uint8_t* nonce);
void dove_crypt(uint8_t* data, size_t length);
void dove_reset(void);

#endif // DOVE_H

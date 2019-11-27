#pragma once

#define OP_NOOP        0x0
#define OP_DIGIT0      0x1
#define OP_DIGIT1      0x2
#define OP_DIGIT2      0x3
#define OP_DIGIT3      0x4
#define OP_DIGIT4      0x5
#define OP_DIGIT5      0x6
#define OP_DIGIT6      0x7
#define OP_DIGIT7      0x8
#define OP_DECODEMODE  0x9
#define OP_INTENSITY   0xa
#define OP_SCANLIMIT   0Xb
#define OP_SHUTDOWN    0xc
#define OP_DISPLAYTEST 0xf

#define NUM_MATS 4

esp_err_t led_init();
void led_send(int channel, uint8_t op, uint8_t data);
void led_send_all(uint8_t op, uint8_t data);
void led_send_matrix(const uint8_t mat[NUM_MATS][8]);
int led_set_shutdown(int);

#ifndef OPCODES_H_
#define OPCODES_H_

#define kMaxOpcodeValue 0xffff
#define kNumPackedOpcodes 0x100

#define kPackedSwitchSignature  0x0100
#define kSparseSwitchSignature  0x0200
#define kArrayDataSignature     0x0300

enum Opcode {
    OP_ADD_DOUBLE                          = 0x000000ab,
    OP_ADD_DOUBLE_2ADDR                    = 0x000000cb,
    OP_ADD_FLOAT                           = 0x000000a6,
    OP_ADD_FLOAT_2ADDR                     = 0x000000c6,
    OP_ADD_INT                             = 0x00000090,
    OP_ADD_INT_2ADDR                       = 0x000000b0,
    OP_ADD_INT_LIT16                       = 0x000000d0,
    OP_ADD_INT_LIT8                        = 0x000000d8,
    OP_ADD_LONG                            = 0x0000009b,
    OP_ADD_LONG_2ADDR                      = 0x000000bb,
    OP_AGET                                = 0x00000044,
    OP_AGET_BOOLEAN                        = 0x00000047,
    OP_AGET_BYTE                           = 0x00000048,
    OP_AGET_CHAR                           = 0x00000049,
    OP_AGET_OBJECT                         = 0x00000046,
    OP_AGET_SHORT                          = 0x0000004a,
    OP_AGET_WIDE                           = 0x00000045,
    OP_AND_INT                             = 0x00000095,
    OP_AND_INT_2ADDR                       = 0x000000b5,
    OP_AND_INT_LIT16                       = 0x000000d5,
    OP_AND_INT_LIT8                        = 0x000000dd,
    OP_AND_LONG                            = 0x000000a0,

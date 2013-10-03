#ifndef OPCODES_H_
#define OPCODES_H_

#include "dalvik/DexFile.h"

#define kMaxOpcodeValue 0xffff
#define kNumPackedOpcodes 0x100

#define kPackedSwitchSignature  0x0100
#define kSparseSwitchSignature  0x0200
#define kArrayDataSignature     0x0300

enum Opcode {
    OP_ADD_DOUBLE                          = 0x000000ab,

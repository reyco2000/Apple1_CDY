/*
 * -------------------------------------------------------------------
 * Project:     Apple I Simulator for ESP32 CYD
 * File:        cpu6502.h
 * Repository:  https://github.com/reyco2000/Apple-1-CYD
 * Author:      Reinaldo Torres
 * Year:        2026
 * License:     MIT License
 *
 * Description: Header for the MOS 6502 CPU emulator. Declares the
 *              CPU state (registers, flags, interrupt vectors) and
 *              public interface for stepping, resetting, and
 *              debugging the processor.
 *
 * Connect with the Author:
 * YouTube:     ChipShift (https://www.youtube.com/@ChipShift)
 * -------------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * -------------------------------------------------------------------
 */

#ifndef CPU6502_H
#define CPU6502_H

#include <stdint.h>
#include "memory.h"

// Status flag bit positions
#define FLAG_C 0x01
#define FLAG_Z 0x02
#define FLAG_I 0x04
#define FLAG_D 0x08
#define FLAG_B 0x10
#define FLAG_U 0x20
#define FLAG_V 0x40
#define FLAG_N 0x80

#define NMI_VECTOR   0xFFFA
#define RESET_VECTOR 0xFFFC
#define IRQ_VECTOR   0xFFFE

class CPU6502 {
public:
    CPU6502(Apple1Memory* mem);
    
    void reset();
    int step();
    
    // Interrupts
    void raiseIRQ();
    void raiseNMI();

    // Registers
    uint8_t A, X, Y, SP;
    uint16_t PC;
    
    // Status flags
    bool C, Z, I, D, B, V, N;

    bool halted;
    bool debug;           // Set true to enable Serial tracing
    uint32_t stepCount;   // Total steps executed
    void dumpState();     // Print PC, A, X, Y, SP, flags

private:
    Apple1Memory* memory;
    
    bool irq_pending;
    bool nmi_pending;
    uint32_t cycles;

    // Helpers
    uint8_t readByte(uint16_t addr);
    void writeByte(uint16_t addr, uint8_t val);
    uint16_t readWord(uint16_t addr);
    uint8_t fetchByte();
    uint16_t fetchWord();

    void pushByte(uint8_t val);
    uint8_t popByte();
    void pushWord(uint16_t val);
    uint16_t popWord();

    uint8_t getStatus();
    void setStatus(uint8_t status);
    void setZN(uint8_t val);

    // Addressing modes - return the address
    uint16_t addr_imm();
    uint16_t addr_zp();
    uint16_t addr_zpx();
    uint16_t addr_zpy();
    uint16_t addr_abs();
    uint16_t addr_abx();
    uint16_t addr_aby();
    uint16_t addr_ind();
    uint16_t addr_izx();
    uint16_t addr_izy();
    uint16_t addr_rel();

    // ALU ops
    void op_adc(uint8_t val);
    void op_sbc(uint8_t val);
    void op_cmp(uint8_t reg, uint8_t val);
    void op_and(uint8_t val);
    void op_ora(uint8_t val);
    void op_eor(uint8_t val);
    uint8_t op_asl(uint8_t val);
    uint8_t op_lsr(uint8_t val);
    uint8_t op_rol(uint8_t val);
    uint8_t op_ror(uint8_t val);
    uint8_t op_inc(uint8_t val);
    uint8_t op_dec(uint8_t val);
    void op_bit(uint8_t val);
    void branch(bool condition);
    void brk();
    void handle_irq();
    void handle_nmi();

    int execute(uint8_t opcode);
};

#endif // CPU6502_H

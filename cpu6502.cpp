/*
 * -------------------------------------------------------------------
 * Project:     Apple I Simulator for ESP32 CYD
 * File:        cpu6502.cpp
 * Repository:  https://github.com/reyco2000/Apple-1-CYD
 * Author:      Reinaldo Torres
 * Year:        2026
 * License:     MIT License
 *
 * Description: Full MOS 6502 CPU emulation — implements all
 *              documented opcodes, addressing modes, interrupts
 *              (IRQ, NMI, BRK), and the stack. Includes optional
 *              debug tracing for illegal opcodes, stack anomalies,
 *              and periodic CPU state dumps.
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

#include "cpu6502.h"
#include <Arduino.h>

CPU6502::CPU6502(Apple1Memory* mem) : memory(mem) {
    debug = false;
    stepCount = 0;
    reset();
}

void CPU6502::dumpState() {
    char buf[120];
    snprintf(buf, sizeof(buf),
        "[CPU] step=%lu PC=%04X A=%02X X=%02X Y=%02X SP=%02X P=%02X [%c%c%c%c%c%c%c]",
        (unsigned long)stepCount, PC, A, X, Y, SP, getStatus(),
        N?'N':'.', V?'V':'.', B?'B':'.',
        D?'D':'.', I?'I':'.', Z?'Z':'.', C?'C':'.');
    Serial.println(buf);
}

void CPU6502::reset() {
    A = 0x00;
    X = 0x00;
    Y = 0x00;
    SP = 0xFD;
    I = true;
    D = false;
    B = false;
    halted = false;
    irq_pending = false;
    nmi_pending = false;
    stepCount = 0;

    uint8_t lo = memory->read(RESET_VECTOR);
    uint8_t hi = memory->read(RESET_VECTOR + 1);
    PC = (hi << 8) | lo;
}

uint8_t CPU6502::readByte(uint16_t addr) { return memory->read(addr); }
void CPU6502::writeByte(uint16_t addr, uint8_t val) { memory->write(addr, val); }
uint16_t CPU6502::readWord(uint16_t addr) {
    uint8_t lo = readByte(addr);
    uint8_t hi = readByte(addr + 1);
    return (hi << 8) | lo;
}
uint8_t CPU6502::fetchByte() {
    uint8_t val = readByte(PC);
    PC++;
    return val;
}
uint16_t CPU6502::fetchWord() {
    uint8_t lo = fetchByte();
    uint8_t hi = fetchByte();
    return (hi << 8) | lo;
}

void CPU6502::pushByte(uint8_t val) {
    writeByte(0x0100 + SP, val);
    SP--;
}
uint8_t CPU6502::popByte() {
    SP++;
    return readByte(0x0100 + SP);
}
void CPU6502::pushWord(uint16_t val) {
    pushByte((val >> 8) & 0xFF);
    pushByte(val & 0xFF);
}
uint16_t CPU6502::popWord() {
    uint16_t lo = popByte();
    uint16_t hi = popByte();
    return (hi << 8) | lo;
}

uint8_t CPU6502::getStatus() {
    uint8_t status = FLAG_U;
    if (C) status |= FLAG_C;
    if (Z) status |= FLAG_Z;
    if (I) status |= FLAG_I;
    if (D) status |= FLAG_D;
    if (B) status |= FLAG_B;
    if (V) status |= FLAG_V;
    if (N) status |= FLAG_N;
    return status;
}

void CPU6502::setStatus(uint8_t value) {
    C = (value & FLAG_C) != 0;
    Z = (value & FLAG_Z) != 0;
    I = (value & FLAG_I) != 0;
    D = (value & FLAG_D) != 0;
    B = (value & FLAG_B) != 0;
    V = (value & FLAG_V) != 0;
    N = (value & FLAG_N) != 0;
}

void CPU6502::setZN(uint8_t value) {
    Z = (value == 0);
    N = (value & 0x80) != 0;
}

uint16_t CPU6502::addr_imm() {
    uint16_t addr = PC;
    PC++;
    return addr;
}
uint16_t CPU6502::addr_zp() { return fetchByte(); }
uint16_t CPU6502::addr_zpx() { return (fetchByte() + X) & 0xFF; }
uint16_t CPU6502::addr_zpy() { return (fetchByte() + Y) & 0xFF; }
uint16_t CPU6502::addr_abs() { return fetchWord(); }
uint16_t CPU6502::addr_abx() { return (fetchWord() + X) & 0xFFFF; }
uint16_t CPU6502::addr_aby() { return (fetchWord() + Y) & 0xFFFF; }
uint16_t CPU6502::addr_ind() {
    uint16_t ptr = fetchWord();
    uint16_t lo = readByte(ptr);
    uint16_t hi;
    if ((ptr & 0xFF) == 0xFF) {
        hi = readByte(ptr & 0xFF00);
    } else {
        hi = readByte(ptr + 1);
    }
    return (hi << 8) | lo;
}
uint16_t CPU6502::addr_izx() {
    uint16_t zp = (fetchByte() + X) & 0xFF;
    uint16_t lo = readByte(zp);
    uint16_t hi = readByte((zp + 1) & 0xFF);
    return (hi << 8) | lo;
}
uint16_t CPU6502::addr_izy() {
    uint16_t zp = fetchByte();
    uint16_t lo = readByte(zp);
    uint16_t hi = readByte((zp + 1) & 0xFF);
    uint16_t base = (hi << 8) | lo;
    return (base + Y) & 0xFFFF;
}
uint16_t CPU6502::addr_rel() {
    int8_t offset = (int8_t)fetchByte();
    return (PC + offset) & 0xFFFF;
}

void CPU6502::op_adc(uint8_t value) {
    if (D) {
        uint8_t lo = (A & 0x0F) + (value & 0x0F) + (C ? 1 : 0);
        uint8_t hi = (A >> 4) + (value >> 4);
        if (lo > 9) {
            lo -= 10;
            hi += 1;
        }
        Z = ((A + value + (C ? 1 : 0)) & 0xFF) == 0;
        N = (hi & 0x08) != 0;
        V = !(((A ^ value) & 0x80) != 0) && (((A ^ (hi << 4)) & 0x80) != 0);
        if (hi > 9) {
            hi -= 10;
            C = true;
        } else {
            C = false;
        }
        A = ((hi << 4) | (lo & 0x0F)) & 0xFF;
    } else {
        uint16_t result = A + value + (C ? 1 : 0);
        C = result > 0xFF;
        V = (((A ^ result) & (value ^ result) & 0x80)) != 0;
        A = result & 0xFF;
        setZN(A);
    }
}

void CPU6502::op_sbc(uint8_t value) {
    if (D) {
        int lo = (A & 0x0F) - (value & 0x0F) - (C ? 0 : 1);
        int hi = (A >> 4) - (value >> 4);
        if (lo < 0) {
            lo += 10;
            hi -= 1;
        }
        if (hi < 0) {
            hi += 10;
            C = false;
        } else {
            C = true;
        }
        uint16_t result = A - value - (C ? 0 : 1);
        V = (((A ^ result) & ((255 - value) ^ result) & 0x80)) != 0;
        setZN(result & 0xFF);
        A = ((hi << 4) | (lo & 0x0F)) & 0xFF;
    } else {
        op_adc(value ^ 0xFF);
    }
}

void CPU6502::op_cmp(uint8_t reg, uint8_t value) {
    uint16_t result = reg - value;
    C = reg >= value;
    setZN(result & 0xFF);
}

void CPU6502::op_and(uint8_t value) {
    A = A & value;
    setZN(A);
}

void CPU6502::op_ora(uint8_t value) {
    A = A | value;
    setZN(A);
}

void CPU6502::op_eor(uint8_t value) {
    A = A ^ value;
    setZN(A);
}

uint8_t CPU6502::op_asl(uint8_t value) {
    C = (value & 0x80) != 0;
    uint8_t result = (value << 1) & 0xFF;
    setZN(result);
    return result;
}

uint8_t CPU6502::op_lsr(uint8_t value) {
    C = (value & 0x01) != 0;
    uint8_t result = value >> 1;
    setZN(result);
    return result;
}

uint8_t CPU6502::op_rol(uint8_t value) {
    uint8_t old_c = C ? 1 : 0;
    C = (value & 0x80) != 0;
    uint8_t result = ((value << 1) | old_c) & 0xFF;
    setZN(result);
    return result;
}

uint8_t CPU6502::op_ror(uint8_t value) {
    uint8_t old_c = C ? 0x80 : 0;
    C = (value & 0x01) != 0;
    uint8_t result = (value >> 1) | old_c;
    setZN(result);
    return result;
}

uint8_t CPU6502::op_inc(uint8_t value) {
    uint8_t result = (value + 1) & 0xFF;
    setZN(result);
    return result;
}

uint8_t CPU6502::op_dec(uint8_t value) {
    uint8_t result = (value - 1) & 0xFF;
    setZN(result);
    return result;
}

void CPU6502::op_bit(uint8_t value) {
    N = (value & 0x80) != 0;
    V = (value & 0x40) != 0;
    Z = (A & value) == 0;
}

void CPU6502::branch(bool condition) {
    uint16_t addr = addr_rel();
    if (condition) {
        PC = addr;
    }
}

void CPU6502::raiseIRQ() { irq_pending = true; }
void CPU6502::raiseNMI() { nmi_pending = true; }

void CPU6502::handle_irq() {
    if (!I) {
        pushWord(PC);
        pushByte(getStatus() & ~FLAG_B);
        I = true;
        uint8_t lo = readByte(IRQ_VECTOR);
        uint8_t hi = readByte(IRQ_VECTOR + 1);
        PC = (hi << 8) | lo;
        irq_pending = false;
    }
}

void CPU6502::handle_nmi() {
    pushWord(PC);
    pushByte(getStatus() & ~FLAG_B);
    I = true;
    uint8_t lo = readByte(NMI_VECTOR);
    uint8_t hi = readByte(NMI_VECTOR + 1);
    PC = (hi << 8) | lo;
    nmi_pending = false;
}

void CPU6502::brk() {
    PC = (PC + 1) & 0xFFFF;
    pushWord(PC);
    pushByte(getStatus() | FLAG_B);
    I = true;
    uint8_t lo = readByte(IRQ_VECTOR);
    uint8_t hi = readByte(IRQ_VECTOR + 1);
    PC = (hi << 8) | lo;
}

int CPU6502::step() {
    if (halted) return 0;
    if (nmi_pending) handle_nmi();
    else if (irq_pending && !I) handle_irq();

    stepCount++;
    uint16_t savedPC = PC;
    uint8_t opcode = fetchByte();
    int c = execute(opcode);
    cycles += c;

    // Debug: periodic state dump every 50000 steps (after BASIC is entered)
    if (debug && (stepCount % 50000 == 0)) {
        Serial.print("[PERIODIC] ");
        dumpState();
    }

    return c;
}

int CPU6502::execute(uint8_t opcode) {
    switch (opcode) {
        case 0x00: {
            if (debug) {
                char buf[80];
                snprintf(buf, sizeof(buf), "[BRK] at PC=%04X SP=%02X", PC-1, SP);
                Serial.println(buf);
                dumpState();
            }
            brk();
            return 7;
        }
        case 0x01: {
            op_ora(readByte(addr_izx()));
            return 6;
        }
        case 0x05: {
            op_ora(readByte(addr_zp()));
            return 3;
        }
        case 0x06: {
            uint16_t addr = addr_zp();
            writeByte(addr, op_asl(readByte(addr)));
            return 5;
        }
        case 0x08: {
            pushByte(getStatus() | FLAG_B);
            return 3;
        }
        case 0x09: {
            op_ora(readByte(addr_imm()));
            return 2;
        }
        case 0x0A: {
            A = op_asl(A);
            return 2;
        }
        case 0x0D: {
            op_ora(readByte(addr_abs()));
            return 4;
        }
        case 0x0E: {
            uint16_t addr = addr_abs();
            writeByte(addr, op_asl(readByte(addr)));
            return 6;
        }
        case 0x10: {
            branch(!N);
            return 2;
        }
        case 0x11: {
            op_ora(readByte(addr_izy()));
            return 5;
        }
        case 0x15: {
            op_ora(readByte(addr_zpx()));
            return 4;
        }
        case 0x16: {
            uint16_t addr = addr_zpx();
            writeByte(addr, op_asl(readByte(addr)));
            return 6;
        }
        case 0x18: {
            C = false;
            return 2;
        }
        case 0x19: {
            op_ora(readByte(addr_aby()));
            return 4;
        }
        case 0x1D: {
            op_ora(readByte(addr_abx()));
            return 4;
        }
        case 0x1E: {
            uint16_t addr = addr_abx();
            writeByte(addr, op_asl(readByte(addr)));
            return 7;
        }
        case 0x20: {
            uint16_t addr = fetchWord();
            if (debug && (SP < 0xE0)) {
                char buf[80];
                snprintf(buf, sizeof(buf), "[JSR] %04X -> %04X SP=%02X (LOW!)",
                    (PC-3)&0xFFFF, addr, SP);
                Serial.println(buf);
            }
            pushWord((PC - 1) & 0xFFFF);
            PC = addr;
            return 6;
        }
        case 0x21: {
            op_and(readByte(addr_izx()));
            return 6;
        }
        case 0x24: {
            op_bit(readByte(addr_zp()));
            return 3;
        }
        case 0x25: {
            op_and(readByte(addr_zp()));
            return 3;
        }
        case 0x26: {
            uint16_t addr = addr_zp();
            writeByte(addr, op_rol(readByte(addr)));
            return 5;
        }
        case 0x28: {
            setStatus(popByte());
            return 4;
        }
        case 0x29: {
            op_and(readByte(addr_imm()));
            return 2;
        }
        case 0x2A: {
            A = op_rol(A);
            return 2;
        }
        case 0x2C: {
            op_bit(readByte(addr_abs()));
            return 4;
        }
        case 0x2D: {
            op_and(readByte(addr_abs()));
            return 4;
        }
        case 0x2E: {
            uint16_t addr = addr_abs();
            writeByte(addr, op_rol(readByte(addr)));
            return 6;
        }
        case 0x30: {
            branch(N);
            return 2;
        }
        case 0x31: {
            op_and(readByte(addr_izy()));
            return 5;
        }
        case 0x35: {
            op_and(readByte(addr_zpx()));
            return 4;
        }
        case 0x36: {
            uint16_t addr = addr_zpx();
            writeByte(addr, op_rol(readByte(addr)));
            return 6;
        }
        case 0x38: {
            C = true;
            return 2;
        }
        case 0x39: {
            op_and(readByte(addr_aby()));
            return 4;
        }
        case 0x3D: {
            op_and(readByte(addr_abx()));
            return 4;
        }
        case 0x3E: {
            uint16_t addr = addr_abx();
            writeByte(addr, op_rol(readByte(addr)));
            return 7;
        }
        case 0x40: {
            setStatus(popByte());
            PC = popWord();
            return 6;
        }
        case 0x41: {
            op_eor(readByte(addr_izx()));
            return 6;
        }
        case 0x45: {
            op_eor(readByte(addr_zp()));
            return 3;
        }
        case 0x46: {
            uint16_t addr = addr_zp();
            writeByte(addr, op_lsr(readByte(addr)));
            return 5;
        }
        case 0x48: {
            pushByte(A);
            return 3;
        }
        case 0x49: {
            op_eor(readByte(addr_imm()));
            return 2;
        }
        case 0x4A: {
            A = op_lsr(A);
            return 2;
        }
        case 0x4C: {
            PC = fetchWord();
            return 3;
        }
        case 0x4D: {
            op_eor(readByte(addr_abs()));
            return 4;
        }
        case 0x4E: {
            uint16_t addr = addr_abs();
            writeByte(addr, op_lsr(readByte(addr)));
            return 6;
        }
        case 0x50: {
            branch(!V);
            return 2;
        }
        case 0x51: {
            op_eor(readByte(addr_izy()));
            return 5;
        }
        case 0x55: {
            op_eor(readByte(addr_zpx()));
            return 4;
        }
        case 0x56: {
            uint16_t addr = addr_zpx();
            writeByte(addr, op_lsr(readByte(addr)));
            return 6;
        }
        case 0x58: {
            I = false;
            return 2;
        }
        case 0x59: {
            op_eor(readByte(addr_aby()));
            return 4;
        }
        case 0x5D: {
            op_eor(readByte(addr_abx()));
            return 4;
        }
        case 0x5E: {
            uint16_t addr = addr_abx();
            writeByte(addr, op_lsr(readByte(addr)));
            return 7;
        }
        case 0x60: {
            uint8_t oldSP = SP;
            PC = (popWord() + 1) & 0xFFFF;
            if (debug && (PC < 0x0200 || PC > 0xFFFF)) {
                char buf[80];
                snprintf(buf, sizeof(buf), "[RTS] -> %04X SP=%02X->%02X (SUSPICIOUS!)",
                    PC, oldSP, SP);
                Serial.println(buf);
            }
            return 6;
        }
        case 0x61: {
            op_adc(readByte(addr_izx()));
            return 6;
        }
        case 0x65: {
            op_adc(readByte(addr_zp()));
            return 3;
        }
        case 0x66: {
            uint16_t addr = addr_zp();
            writeByte(addr, op_ror(readByte(addr)));
            return 5;
        }
        case 0x68: {
            A = popByte();
            setZN(A);
            return 4;
        }
        case 0x69: {
            op_adc(readByte(addr_imm()));
            return 2;
        }
        case 0x6A: {
            A = op_ror(A);
            return 2;
        }
        case 0x6C: {
            PC = addr_ind();
            return 5;
        }
        case 0x6D: {
            op_adc(readByte(addr_abs()));
            return 4;
        }
        case 0x6E: {
            uint16_t addr = addr_abs();
            writeByte(addr, op_ror(readByte(addr)));
            return 6;
        }
        case 0x70: {
            branch(V);
            return 2;
        }
        case 0x71: {
            op_adc(readByte(addr_izy()));
            return 5;
        }
        case 0x75: {
            op_adc(readByte(addr_zpx()));
            return 4;
        }
        case 0x76: {
            uint16_t addr = addr_zpx();
            writeByte(addr, op_ror(readByte(addr)));
            return 6;
        }
        case 0x78: {
            I = true;
            return 2;
        }
        case 0x79: {
            op_adc(readByte(addr_aby()));
            return 4;
        }
        case 0x7D: {
            op_adc(readByte(addr_abx()));
            return 4;
        }
        case 0x7E: {
            uint16_t addr = addr_abx();
            writeByte(addr, op_ror(readByte(addr)));
            return 7;
        }
        case 0x81: {
            writeByte(addr_izx(), A);
            return 6;
        }
        case 0x84: {
            writeByte(addr_zp(), Y);
            return 3;
        }
        case 0x85: {
            writeByte(addr_zp(), A);
            return 3;
        }
        case 0x86: {
            writeByte(addr_zp(), X);
            return 3;
        }
        case 0x88: {
            Y = (Y - 1) & 0xFF;
            setZN(Y);
            return 2;
        }
        case 0x8A: {
            A = X;
            setZN(A);
            return 2;
        }
        case 0x8C: {
            writeByte(addr_abs(), Y);
            return 4;
        }
        case 0x8D: {
            writeByte(addr_abs(), A);
            return 4;
        }
        case 0x8E: {
            writeByte(addr_abs(), X);
            return 4;
        }
        case 0x90: {
            branch(!C);
            return 2;
        }
        case 0x91: {
            writeByte(addr_izy(), A);
            return 6;
        }
        case 0x94: {
            writeByte(addr_zpx(), Y);
            return 4;
        }
        case 0x95: {
            writeByte(addr_zpx(), A);
            return 4;
        }
        case 0x96: {
            writeByte(addr_zpy(), X);
            return 4;
        }
        case 0x98: {
            A = Y;
            setZN(A);
            return 2;
        }
        case 0x99: {
            writeByte(addr_aby(), A);
            return 5;
        }
        case 0x9A: {
            SP = X;
            return 2;
        }
        case 0x9D: {
            writeByte(addr_abx(), A);
            return 5;
        }
        case 0xA0: {
            Y = readByte(addr_imm());
            setZN(Y);
            return 2;
        }
        case 0xA1: {
            A = readByte(addr_izx());
            setZN(A);
            return 6;
        }
        case 0xA2: {
            X = readByte(addr_imm());
            setZN(X);
            return 2;
        }
        case 0xA4: {
            Y = readByte(addr_zp());
            setZN(Y);
            return 3;
        }
        case 0xA5: {
            A = readByte(addr_zp());
            setZN(A);
            return 3;
        }
        case 0xA6: {
            X = readByte(addr_zp());
            setZN(X);
            return 3;
        }
        case 0xA8: {
            Y = A;
            setZN(Y);
            return 2;
        }
        case 0xA9: {
            A = readByte(addr_imm());
            setZN(A);
            return 2;
        }
        case 0xAA: {
            X = A;
            setZN(X);
            return 2;
        }
        case 0xAC: {
            Y = readByte(addr_abs());
            setZN(Y);
            return 4;
        }
        case 0xAD: {
            A = readByte(addr_abs());
            setZN(A);
            return 4;
        }
        case 0xAE: {
            X = readByte(addr_abs());
            setZN(X);
            return 4;
        }
        case 0xB0: {
            branch(C);
            return 2;
        }
        case 0xB1: {
            A = readByte(addr_izy());
            setZN(A);
            return 5;
        }
        case 0xB4: {
            Y = readByte(addr_zpx());
            setZN(Y);
            return 4;
        }
        case 0xB5: {
            A = readByte(addr_zpx());
            setZN(A);
            return 4;
        }
        case 0xB6: {
            X = readByte(addr_zpy());
            setZN(X);
            return 4;
        }
        case 0xB8: {
            V = false;
            return 2;
        }
        case 0xB9: {
            A = readByte(addr_aby());
            setZN(A);
            return 4;
        }
        case 0xBA: {
            X = SP;
            setZN(X);
            return 2;
        }
        case 0xBC: {
            Y = readByte(addr_abx());
            setZN(Y);
            return 4;
        }
        case 0xBD: {
            A = readByte(addr_abx());
            setZN(A);
            return 4;
        }
        case 0xBE: {
            X = readByte(addr_aby());
            setZN(X);
            return 4;
        }
        case 0xC0: {
            op_cmp(Y, readByte(addr_imm()));
            return 2;
        }
        case 0xC1: {
            op_cmp(A, readByte(addr_izx()));
            return 6;
        }
        case 0xC4: {
            op_cmp(Y, readByte(addr_zp()));
            return 3;
        }
        case 0xC5: {
            op_cmp(A, readByte(addr_zp()));
            return 3;
        }
        case 0xC6: {
            uint16_t addr = addr_zp();
            writeByte(addr, op_dec(readByte(addr)));
            return 5;
        }
        case 0xC8: {
            Y = (Y + 1) & 0xFF;
            setZN(Y);
            return 2;
        }
        case 0xC9: {
            op_cmp(A, readByte(addr_imm()));
            return 2;
        }
        case 0xCA: {
            X = (X - 1) & 0xFF;
            setZN(X);
            return 2;
        }
        case 0xCC: {
            op_cmp(Y, readByte(addr_abs()));
            return 4;
        }
        case 0xCD: {
            op_cmp(A, readByte(addr_abs()));
            return 4;
        }
        case 0xCE: {
            uint16_t addr = addr_abs();
            writeByte(addr, op_dec(readByte(addr)));
            return 6;
        }
        case 0xD0: {
            branch(!Z);
            return 2;
        }
        case 0xD1: {
            op_cmp(A, readByte(addr_izy()));
            return 5;
        }
        case 0xD5: {
            op_cmp(A, readByte(addr_zpx()));
            return 4;
        }
        case 0xD6: {
            uint16_t addr = addr_zpx();
            writeByte(addr, op_dec(readByte(addr)));
            return 6;
        }
        case 0xD8: {
            D = false;
            return 2;
        }
        case 0xD9: {
            op_cmp(A, readByte(addr_aby()));
            return 4;
        }
        case 0xDD: {
            op_cmp(A, readByte(addr_abx()));
            return 4;
        }
        case 0xDE: {
            uint16_t addr = addr_abx();
            writeByte(addr, op_dec(readByte(addr)));
            return 7;
        }
        case 0xE0: {
            op_cmp(X, readByte(addr_imm()));
            return 2;
        }
        case 0xE1: {
            op_sbc(readByte(addr_izx()));
            return 6;
        }
        case 0xE4: {
            op_cmp(X, readByte(addr_zp()));
            return 3;
        }
        case 0xE5: {
            op_sbc(readByte(addr_zp()));
            return 3;
        }
        case 0xE6: {
            uint16_t addr = addr_zp();
            writeByte(addr, op_inc(readByte(addr)));
            return 5;
        }
        case 0xE8: {
            X = (X + 1) & 0xFF;
            setZN(X);
            return 2;
        }
        case 0xE9: {
            op_sbc(readByte(addr_imm()));
            return 2;
        }
        case 0xEA: {
            return 2;
        }
        case 0xEC: {
            op_cmp(X, readByte(addr_abs()));
            return 4;
        }
        case 0xED: {
            op_sbc(readByte(addr_abs()));
            return 4;
        }
        case 0xEE: {
            uint16_t addr = addr_abs();
            writeByte(addr, op_inc(readByte(addr)));
            return 6;
        }
        case 0xF0: {
            branch(Z);
            return 2;
        }
        case 0xF1: {
            op_sbc(readByte(addr_izy()));
            return 5;
        }
        case 0xF5: {
            op_sbc(readByte(addr_zpx()));
            return 4;
        }
        case 0xF6: {
            uint16_t addr = addr_zpx();
            writeByte(addr, op_inc(readByte(addr)));
            return 6;
        }
        case 0xF8: {
            D = true;
            return 2;
        }
        case 0xF9: {
            op_sbc(readByte(addr_aby()));
            return 4;
        }
        case 0xFD: {
            op_sbc(readByte(addr_abx()));
            return 4;
        }
        case 0xFE: {
            uint16_t addr = addr_abx();
            writeByte(addr, op_inc(readByte(addr)));
            return 7;
        }

        default: {
            if (debug) {
                char buf[80];
                snprintf(buf, sizeof(buf),
                    "[ILLEGAL] opcode=%02X at PC=%04X SP=%02X",
                    opcode, (PC-1)&0xFFFF, SP);
                Serial.println(buf);
                dumpState();
            }
            return 2;
        }
    }
}

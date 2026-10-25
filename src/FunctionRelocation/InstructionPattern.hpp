#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <frida-gum.h>

namespace function_relocation {

    // Register saves and stack-frame allocation carry no function identity.
    // Only the contiguous entry prefix is classified this way by the caller.
    inline bool is_prologue_instruction(const cs_insn &insn) {
        const auto &x = insn.detail->x86;
        if (insn.id == X86_INS_PUSH && x.op_count == 1 && x.operands[0].type == X86_OP_REG) return true;
        if (x.op_count != 2) return false;
        const auto &dst = x.operands[0];
        const auto &src = x.operands[1];
        if (insn.id == X86_INS_MOV && dst.type == X86_OP_MEM && dst.mem.base == X86_REG_RSP &&
            dst.mem.index == X86_REG_INVALID && src.type == X86_OP_REG) return true;
        if (dst.type != X86_OP_REG) return false;
        if (insn.id == X86_INS_SUB && dst.reg == X86_REG_RSP && src.type == X86_OP_IMM) return true;
        if (dst.reg != X86_REG_RBP) return false;
        return (insn.id == X86_INS_MOV && src.type == X86_OP_REG && src.reg == X86_REG_RSP) ||
               (insn.id == X86_INS_LEA && src.type == X86_OP_MEM && src.mem.base == X86_REG_RSP &&
                src.mem.index == X86_REG_INVALID);
    }

    // Preserve prefixes, opcodes and addressing modes. Mask only fields identified
    // by the decoder, including zero displacements and prefixed/imm64 instructions.
    inline std::string instruction_pattern(const cs_insn &insn, bool module_immediate = false) {
        std::array<bool, 24> wildcard{};
        const auto &x86 = insn.detail->x86;
        const auto mask = [&](uint8_t offset, uint8_t size) {
            if (size != 0 && offset != 0 && offset + size <= insn.size)
                for (size_t i = offset; i < offset + size; ++i) wildcard[i] = true;
        };
        for (uint8_t i = 0; i < x86.op_count; ++i) {
            if (x86.operands[i].type == X86_OP_MEM && x86.operands[i].mem.base == X86_REG_RIP)
                mask(x86.encoding.disp_offset, x86.encoding.disp_size);
        }
        bool relative_branch = false;
        for (uint8_t i = 0; i < insn.detail->groups_count; ++i)
            relative_branch |= insn.detail->groups[i] == CS_GRP_BRANCH_RELATIVE;
        if (relative_branch || module_immediate) mask(x86.encoding.imm_offset, x86.encoding.imm_size);

        constexpr char hex[] = "0123456789abcdef";
        std::string pattern;
        for (size_t i = 0; i < insn.size; ++i) {
            if (i != 0) pattern += ' ';
            if (wildcard[i]) pattern += "??";
            else {
                pattern += hex[insn.bytes[i] >> 4];
                pattern += hex[insn.bytes[i] & 15];
            }
        }
        return pattern;
    }

    inline uintptr_t instruction_address_reference(const cs_insn &insn) {
        const auto &x86 = insn.detail->x86;
        if (x86.op_count != 2 || x86.operands[0].type != X86_OP_REG) return 0;
        const auto &op = x86.operands[1];
        if (insn.id == X86_INS_LEA && op.type == X86_OP_MEM &&
            op.mem.base == X86_REG_RIP && op.mem.index == X86_REG_INVALID &&
            op.mem.segment == X86_REG_INVALID)
            return insn.address + insn.size + op.mem.disp;
        if ((insn.id == X86_INS_MOV || insn.id == X86_INS_MOVABS) && op.type == X86_OP_IMM)
            return static_cast<uintptr_t>(op.imm);
        return 0;
    }

}// namespace function_relocation

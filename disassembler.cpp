#include "disassembler.h"
#include <sstream>
#include <iostream>
#include <stdexcept>
#include "binary_stream.h"
#include "instructions.h"

//                      "\x1b[38;2;40;177;249mTEXT\x1b[0m", byte, ist.name.c_str());
#define COL(r, g, b, s) "\x1b[38;2;"#r";"#g";"#b"m" s "\x1b[0m"


std::string AbcInstruction::ToString() const
{
    std::stringstream ss;
    ss << ABC::Instructions[static_cast<uint8_t>(opcode)].name << " ";
    for (auto param : operands)
    {
        ss << std::hex << param << " ";
    }
    return ss.str();
}
Disassembler::Disassembly Disassembler::Disassemble(const uint8_t *data)
{
    Disassembly result;

    BinaryStream code {  data };

    code.read_u32();
    code.read_u32();
    code.read_u32();
    code.read_u32();
    uint32_t code_end = code.read_u32();
    code_end += code.position;

    while (code.position != code_end)
    {
        size_t start_pos = code.position;
        uint8_t byte = code.read<uint8_t>();

        if (!ABC::Instructions.count(byte))
        {
            result.instructions.clear();
            break;
        }

        ABC::Instruction ist = ABC::Instructions[byte];

        std::vector<int> indexes;

        for (ABC::Operand op : ist.operands)
        {
            switch(op)
            {
                case ABC::Operand::U30:
                {
                    indexes.push_back(code.read_u30());
                    break;
                }
                case ABC::Operand::S24:
                {
                    indexes.push_back(code.read_s24());
                    break;
                }
                case ABC::Operand::Byte:
                {
                    indexes.push_back(static_cast<int>(code.read<uint8_t>()));
                    break;
                }
                case ABC::Operand::Dynamic:
                {
                    switch (static_cast<ABC::OpCode>(byte))
                    {
                        case ABC::OpCode::OP_lookupswitch:
                        {
                            uint32_t case_count = code.read_u30();
                            for (uint32_t i = 0; i <= case_count; i++)
                                indexes.push_back(code.read_s24());
                            break;
                        }
                        default:
                            break;
                    }
                    break;
                }
                default:
                    break;
            }
        }

        result.instructions.emplace_back(static_cast<ABC::OpCode>(byte), indexes, start_pos, code.position - start_pos);

    }
    return result;
}

std::vector<uint32_t> Disassembler::Disassembly::GetXrefs()
{
    std::vector<uint32_t> result;
    using namespace ABC;
    for (auto &inst : instructions)
    {
        switch (inst.opcode)
        {
            case OpCode::OP_findpropstrict:
            case OpCode::OP_constructprop:
            case OpCode::OP_astype:
            case OpCode::OP_callsuper:
            case OpCode::OP_callsupervoid:
            case OpCode::OP_coerce:
            case OpCode::OP_finddef:
            case OpCode::OP_getdescendants:
            case OpCode::OP_getlex:
            case OpCode::OP_getsuper:
            case OpCode::OP_istype:
            case OpCode::OP_setsuper:
                result.push_back(inst.operands[0]);
                break;
            default:
                break;
        }
    }
    return result;
}


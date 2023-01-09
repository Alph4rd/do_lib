#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H
#include "instructions.h"
#include "avm.h"

class AbcInstruction
{
public:
	AbcInstruction(ABC::OpCode op, std::vector<int> indexes, size_t pos, size_t inst_size = 1)
		: opcode(op), operands(indexes), size(inst_size), position(pos)
	{
	}

	std::string ToString() const;

	ABC::OpCode opcode;
	std::vector<int> operands;
	size_t size = 1, position = 0;
};


class Disassembler
{
public:
	Disassembler() = default;

	class Disassembly
	{
	public:
		Disassembly() { }

		std::vector<uint32_t> GetXrefs();

		std::vector<AbcInstruction> instructions;
	};


	static Disassembly Disassemble(const uint8_t *data);

	inline static Disassembly Disassemble(avm::MethodInfo *method)
	{
		return Disassemble(method->abc_code);
	}
};


#endif /* DISASSEMBLER_H */


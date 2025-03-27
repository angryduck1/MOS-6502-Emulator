#include <iostream>
#include <cstdint>
#include "Opcodes.hpp"

using namespace std;

using Byte = uint8_t;
using Word = uint16_t;

struct Mem {
	static constexpr uint32_t MAX_MEM = 1024 * 64;
	Byte Data[MAX_MEM];

	void init() {
		for (uint32_t i = 0; i < MAX_MEM; ++i) {
			Data[i] = 0;
		}
	}

	Byte operator[](uint32_t address) const {
		return Data[address];
	}

	Byte & operator[](uint32_t address) {
		return Data[address];
	}

	void WriteWordToStack(Word Value, Word& SP, uint32_t& Cycles, Mem& memory) {
		memory[SP] = Value & 0xFF;
		--SP;
		memory[SP] = (Value >> 8);
		--SP;
		Cycles -= 2;
	}
};

class CPU {
public:
	Word PC = 0;
	Word SP;

	Byte A, X, Y;

	Byte C : 1; // Carry
	Byte Z : 1; // Is zero
	Byte I : 1; // Interrupt
	Byte D : 1; // Decimal
	Byte B : 1; // Break
	Byte V : 1; // Carry from 6 to 7
	Byte N : 1; // Result op

	void Reset(Mem& memory) {
		PC = 0xFFFC;
		SP = 0x01FF;
		C = Z = I = D = B = V = N = 0;
		A = X = Y = 0;

		memory.init();
	}

	void SET_LDA_STATUS() {
		Z = (A == 0);
		N = (A & 0b10000000) > 0;
	}

	Byte Fetch_Byte(uint32_t& Cycles, Mem& memory) {
		Byte Data = memory[PC];
		++PC;
		--Cycles;
		return Data;
	}

	Word Fetch_Word(uint32_t& Cycles, Mem& memory) {
		Word Data = memory[PC];
		++PC;

		Data |= (memory[PC] << 8);
		++PC;

		Cycles -= 2;
		return Data;
	}

	Byte Read_Byte(uint32_t& Cycles, Byte Address, Mem& memory) {
		Byte Data = memory[Address];
		--Cycles;
		return Data;
	}

	Word Read_Word(uint32_t& Cycles, Word Address, Mem& memory) {
		Word Data = memory[Address];
		--Cycles;
		return Data;
	}

	Word Pop_WordFromStack(uint32_t& Cycles, Mem& memory) {
		Word Data = memory[++SP];
		Data |= (memory[++SP] << 8);
		Cycles -= 2;
		return Data;
	}

	void Execute(uint32_t Cycles, Mem& memory) {
		while (Cycles > 0) {
			Byte Ins = Fetch_Byte(Cycles, memory);

			switch (Ins) {
			case IMM_LDA: {
				Byte value = Fetch_Byte(Cycles, memory);
				A = value;
				SET_LDA_STATUS();
			} break;

			case ZP_LDA: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				A = Read_Byte(Cycles, ZpAddress, memory);
				SET_LDA_STATUS();
			} break;

			case ZPX_LDA: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				ZpAddress += X;
				--Cycles;
				A = Read_Byte(Cycles, ZpAddress, memory);
				SET_LDA_STATUS();
			} break;

			case ABS_LDA: {
				Word Address = Fetch_Word(Cycles, memory);
				A = Read_Word(Cycles, Address, memory);
				SET_LDA_STATUS();
			} break;

			case ABSX_LDA: {
				Word Address = Fetch_Word(Cycles, memory);
				Address += X;
				--Cycles;
				A = Read_Word(Cycles, Address, memory);
				SET_LDA_STATUS();
			} break;

			case ABSY_LDA: {
				Word Address = Fetch_Word(Cycles, memory);
				Address += Y;
				--Cycles;
				A = Read_Word(Cycles, Address, memory);
				SET_LDA_STATUS();
			} break;

			case ABS_JSR: {
				Word SubrAddress = Fetch_Word(Cycles, memory);
				memory.WriteWordToStack(PC, SP, Cycles, memory);
				PC = SubrAddress;
				--Cycles;
			} break;

			case IMP_RTS: {
				Word RetAddress = Pop_WordFromStack(Cycles, memory);
				PC = RetAddress;
				Cycles -= 4;
			} break;

			default:
				cout << "Unknown instruction: " << static_cast<int>(Ins) << endl;
			}
		}
	}
};
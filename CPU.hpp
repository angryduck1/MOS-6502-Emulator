#include <iostream>
#include <cstdint>
#include <fstream>
#include "Opcodes.hpp"

using namespace std;

using Byte = uint8_t;
using NUByte = int8_t;
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

	Byte& operator[](uint32_t address) {
		return Data[address];
	}

	void WriteWordToStack(Word Value, Word& SP, uint32_t& Cycles, Mem& memory) {
		memory[SP] = Value & 0xFF;
		--SP;
		memory[SP] = (Value >> 8);
		--SP;
		Cycles -= 2;
	}

	void IncValueInMem(uint32_t Address, uint32_t& Cycles, Mem& memory) {
		++memory[Address];
		Cycles -= 2;
	}
};

class CPU {
public:
	ifstream bin;
	CPU(string file) : bin(file, ios::binary) {
		if (!bin.is_open()) {
			cout << "Failed to open " << file << endl;
		}
	}
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

	void Load_BIN(Mem& memory) {
		char ch;
		Word i = 0;
		while (bin.get(ch)) {
			memory[(PC + (i++)) & 0xFFFF] = static_cast<Byte>(ch);
		}
	}

	void SET_LDA_STATUS() {
		Z = (A == 0);
		N = (A & 0b10000000) > 0;
	}

	void SET_C_V_N_Z_STATUS_ADC(Byte LACC) {
		Z = (A == 0);
		N = (A & 0b10000000) > 0;
		C = (LACC > A);
		V = (LACC & 0b10000000) != (A & 0b10000000);
	}

	void SET_N_Z_STATUS(Byte& Operand) {
		Z = (Operand == 0);
		N = (Operand & 0b10000000) > 0;
	}

	void CheckPageIntersection(uint32_t& Cycles, uint32_t Address, Byte& Reg) {
		if ((Address & 0xFF00) != ((Address - Reg) & 0xFF00)) {
			--Cycles;
		}
	}

	void CheckPageIntersection(uint32_t& Cycles, uint32_t Address, NUByte& Reg) {
		if ((Address & 0xFF00) != ((Address - Reg) & 0xFF00)) {
			--Cycles;
		}
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

	Word ReadIND_Word(uint32_t& Cycles, uint32_t Address, Mem& memory) {
		Word Data = memory[Address];

		Data |= (memory[Address + 1] << 8);

		Cycles -= 2;
		return Data;
	}

	Word Pop_WordFromStack(uint32_t& Cycles, Mem& memory) {
		Word Data = memory[++SP];
		Data = (Data << 8) | memory[++SP];
		Cycles -= 2;
		return Data;
	}

	void Execute(uint32_t Cycles, Mem& memory) {
		Load_BIN(memory);
		while (Cycles > 0) {
			Byte Ins = Fetch_Byte(Cycles, memory);

			switch (Ins) {
			case IMP_NOP: {
				--Cycles;
			} break;
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
				CheckPageIntersection(Cycles, Address, X);
				A = Read_Word(Cycles, Address, memory);
				SET_LDA_STATUS();
			} break;

			case ABSY_LDA: {
				Word Address = Fetch_Word(Cycles, memory);
				Address += Y;
				--Cycles;
				CheckPageIntersection(Cycles, Address, Y);
				A = Read_Word(Cycles, Address, memory);
				SET_LDA_STATUS();
			} break;

			case ABS_JSR: {
				Word SubrAddress = Fetch_Word(Cycles, memory);
				memory.WriteWordToStack(PC - 1, SP, Cycles, memory);
				PC = SubrAddress;
				--Cycles;
			} break;

			case IMP_RTS: {
				Word RetAddress = Pop_WordFromStack(Cycles, memory);
				PC = RetAddress + 1;
				Cycles -= 4;
			} break;

			case INDX_LDA: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				--Cycles;
				Word Address = ReadIND_Word(Cycles, ZpAddress, memory);
				Address += X;
				A = Read_Word(Cycles, Address, memory);
				SET_LDA_STATUS();
			} break;

			case INDY_LDA: {
				Byte ZpAddress = (Fetch_Byte(Cycles, memory));
				Word Address = ReadIND_Word(Cycles, ZpAddress, memory);
				Address += Y;
				CheckPageIntersection(Cycles, Address, Y);
				A = Read_Word(Cycles, Address, memory);
				SET_LDA_STATUS();
			} break;

			case IMP_INX: {
				++X;
				SET_N_Z_STATUS(X);
				Cycles -= 2;
			} break;

			case IMP_INY: {
				++Y;
				SET_N_Z_STATUS(Y);
				Cycles -= 2;
			} break;

			case IMP_DEX: {
				--X;
				SET_N_Z_STATUS(X);
				Cycles -= 2;
			} break;

			case IMP_DEY: {
				--Y;
				SET_N_Z_STATUS(Y);
				Cycles -= 2;
			} break;

			case ABS_JMP: {
				Word Address = Fetch_Word(Cycles, memory);
				PC = Address;
			} break;

			case IND_JMP: {
				Byte ZpAddress = (Fetch_Byte(Cycles, memory));
				Word Address = ReadIND_Word(Cycles, ZpAddress, memory);
				PC = Address;
				--Cycles;
			} break;

			case ZP_INC: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				memory.IncValueInMem(ZpAddress, Cycles, memory);
				SET_N_Z_STATUS(memory[ZpAddress]);
				--Cycles;
			} break;

			case ZPX_INC: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				ZpAddress += X;
				--Cycles;
				memory.IncValueInMem(ZpAddress, Cycles, memory);
				SET_N_Z_STATUS(memory[ZpAddress]);
				--Cycles;
			} break;

			case ABS_INC: {
				Word Address = Fetch_Word(Cycles, memory);
				memory.IncValueInMem(Address, Cycles, memory);
				SET_N_Z_STATUS(memory[Address]);
				--Cycles;
			} break;

			case ABSX_INC: {
				Word Address = Fetch_Word(Cycles, memory);
				Address += X;
				--Cycles;
				memory.IncValueInMem(Address, Cycles, memory);
				SET_N_Z_STATUS(memory[Address]);
				--Cycles;
			} break;

			case IMM_ADC: {
				Byte operand = Fetch_Byte(Cycles, memory);
				Byte LAA = A;
				A = A + operand + C;
				SET_C_V_N_Z_STATUS_ADC(LAA);
			} break;

			case ZP_ADC: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				Byte operand = Read_Byte(Cycles, ZpAddress, memory);
				Byte LAA = A;
				A = A + operand + C;
				SET_C_V_N_Z_STATUS_ADC(LAA);
			} break;

			case ZPX_ADC: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				ZpAddress += X;
				--Cycles;
				Byte operand = Read_Byte(Cycles, ZpAddress, memory);
				Byte LAA = A;
				A = A + operand + C;
				SET_C_V_N_Z_STATUS_ADC(LAA);
			} break;

			case ABS_ADC: {
				Word Address = Fetch_Word(Cycles, memory);
				Byte operand = Read_Word(Cycles, Address, memory);
				Byte LAA = A;
				A = A + operand + C;
				SET_C_V_N_Z_STATUS_ADC(LAA);
			} break;

			case ABSX_ADC: {
				Word Address = Fetch_Word(Cycles, memory);
				Address += X;
				CheckPageIntersection(Cycles, Address, X);
				Byte operand = Read_Word(Cycles, Address, memory);
				Byte LAA = A;
				A = A + operand + C;
				SET_C_V_N_Z_STATUS_ADC(LAA);
			} break;

			case ABSY_ADC: {
				Word Address = Fetch_Word(Cycles, memory);
				Address += Y;
				CheckPageIntersection(Cycles, Address, X);
				Byte operand = Read_Word(Cycles, Address, memory);
				Byte LAA = A;
				A = A + operand + C;
				SET_C_V_N_Z_STATUS_ADC(LAA);
			} break;

			case INDX_ADC: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				Word Address = ReadIND_Word(Cycles, ZpAddress, memory);
				Address += X;
				--Cycles;
				Byte operand = Read_Word(Cycles, Address, memory);
				Byte LAA = A;
				A = A + operand + C;
				SET_C_V_N_Z_STATUS_ADC(LAA);
			} break;

			case INDY_ADC: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				Word Address = ReadIND_Word(Cycles, ZpAddress, memory);
				Address += Y;
				CheckPageIntersection(Cycles, Address, Y);
				Byte operand = Read_Word(Cycles, Address, memory);
				Byte LAA = A;
				A = A + operand + C;
				SET_C_V_N_Z_STATUS_ADC(LAA);
			} break;

			case IMM_LDX: {
				X = Fetch_Byte(Cycles, memory);
				SET_N_Z_STATUS(X);
			} break;

			case ZP_LDX: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				X = Read_Byte(Cycles, ZpAddress, memory);
				SET_N_Z_STATUS(X);
			} break;

			case ZPY_LDX: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				ZpAddress += Y;
				--Cycles;
				X = Read_Byte(Cycles, ZpAddress, memory);
				SET_N_Z_STATUS(X);
			} break;

			case ABS_LDX: {
				Word Address = Fetch_Word(Cycles, memory);
				X = Read_Word(Cycles, Address, memory);
				SET_N_Z_STATUS(X);
			} break;

			case ABSY_LDX: {
				Word Address = Fetch_Word(Cycles, memory);
				Address += Y;
				CheckPageIntersection(Cycles, Address, Y);
				X = Read_Word(Cycles, Address, memory);
				SET_N_Z_STATUS(X);
			} break;

			case IMM_LDY: {
				Y = Fetch_Byte(Cycles, memory);
				SET_N_Z_STATUS(Y);
			} break;

			case ZP_LDY: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				Y = Read_Byte(Cycles, ZpAddress, memory);
				SET_N_Z_STATUS(Y);
			} break;

			case ZPX_LDY: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				ZpAddress += X;
				--Cycles;
				Y = Read_Byte(Cycles, ZpAddress, memory);
				SET_N_Z_STATUS(Y);
			} break;

			case ABS_LDY: {
				Word Address = Fetch_Word(Cycles, memory);
				Y = Read_Word(Cycles, Address, memory);
				SET_N_Z_STATUS(Y);
			} break;

			case ABSX_LDY: {
				Word Address = Fetch_Word(Cycles, memory);
				Address += X;
				CheckPageIntersection(Cycles, Address, X);
				Y = Read_Word(Cycles, Address, memory);
				SET_N_Z_STATUS(Y);
			} break;

			case IMM_AND: {
				Byte operand = Fetch_Byte(Cycles, memory);
				A &= operand;
				SET_LDA_STATUS();
			} break;

			case ZP_AND: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				Byte operand = Read_Byte(Cycles, ZpAddress, memory);
				A &= operand;
				SET_LDA_STATUS();
			} break;

			case ZPX_AND: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				ZpAddress += X;
				--Cycles;
				Byte operand = Read_Byte(Cycles, ZpAddress, memory);
				A &= operand;
				SET_LDA_STATUS();
			} break;

			case ABS_AND: {
				Word Address = Fetch_Word(Cycles, memory);
				Byte operand = Read_Word(Cycles, Address, memory);
				A &= operand;
				SET_LDA_STATUS();
			} break;

			case ABSX_AND: {
				Word Address = Fetch_Word(Cycles, memory);
				Address += X;
				CheckPageIntersection(Cycles, Address, X);
				Byte operand = Read_Word(Cycles, Address, memory);
				A &= operand;
				SET_LDA_STATUS();
			} break;

			case ABSY_AND: {
				Word Address = Fetch_Word(Cycles, memory);
				Address += Y;
				CheckPageIntersection(Cycles, Address, Y);
				Byte operand = Read_Word(Cycles, Address, memory);
				A &= operand;
				SET_LDA_STATUS();
			} break;

			case INDX_AND: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				Word Address = ReadIND_Word(Cycles, ZpAddress, memory);
				Address += X;
				--Cycles;
				Byte operand = Read_Word(Cycles, Address, memory);
				A &= operand;
				SET_LDA_STATUS();
			} break;

			case INDY_AND: {
				Byte ZpAddress = Fetch_Byte(Cycles, memory);
				Word Address = ReadIND_Word(Cycles, ZpAddress, memory);
				Address += Y;
				CheckPageIntersection(Cycles, Address, Y);
				Byte operand = Read_Word(Cycles, Address, memory);
				A &= operand;
				SET_LDA_STATUS();
			} break;

			case REL_BCC: {
				NUByte operand = Fetch_Byte(Cycles, memory);
				switch (C) {
				case 0: {
					PC += operand;
					Cycles -= 2;
					CheckPageIntersection(Cycles, PC, operand);
				} break;
				}
			} break;

			case REL_BCS: {
				NUByte operand = Fetch_Byte(Cycles, memory);
				switch (C) {
				case 1: {
					PC += operand;
					Cycles -= 2;
					CheckPageIntersection(Cycles, PC, operand);
				} break;
				}
			} break;

			case REL_BNE: {
				NUByte operand = Fetch_Byte(Cycles, memory);
				switch (Z) {
				case 0: {
					PC += operand;
					Cycles -= 2;
					CheckPageIntersection(Cycles, PC, operand);
				} break;
				}
			} break;

			case REL_BEQ: {
				NUByte operand = Fetch_Byte(Cycles, memory);
				switch (Z) {
				case 1: {
					PC += operand;
					Cycles -= 2;
					CheckPageIntersection(Cycles, PC, operand);
				} break;
				}
			} break;

			case IMP_TAX: {
				X = A;
				--Cycles;
				SET_N_Z_STATUS(X);
			} break;

			case IMP_TAY: {
				Y = A;
				--Cycles;
				SET_N_Z_STATUS(Y);
			} break;

			case IMP_TSX: {
				X = SP;
				--Cycles;
				SET_N_Z_STATUS(X);
			} break;

			case IMP_TXA: {
				A = X;
				--Cycles;
				SET_LDA_STATUS();
			} break;

			case IMP_TXS: {
				SP = X;
				--Cycles;
			} break;

			case IMP_TYA: {
				A = Y;
				--Cycles;
				SET_LDA_STATUS();
			} break;

			case IMP_SEC: {
				C = 1;
				--Cycles;
			} break;

			case IMP_SED: {
				D = 1;
				--Cycles;
			} break;

			case IMP_SEI: {
				I = 1;
				--Cycles;
			} break;

			case IMP_BRK: {
				Word Address = PC + 1;
				memory.WriteWordToStack(Address, SP, Cycles, memory);
				B = 1;
				Byte Flags = (N << 7) | (V << 6) | (B << 4) | (D << 3) | (I << 2) | (Z << 1) | C;
				memory[SP--] = Flags;
				I = 1;
				PC = Read_Word(Cycles, 0xFFFE, memory);
				Cycles -= 3;
			} break;

			case IMP_RTI: {
				++SP;
				N = (memory[SP]) >> 7; V = (memory[SP]) >> 6; B = (memory[SP]) >> 4; D = (memory[SP]) >> 3; I = (memory[SP]) >> 2; Z = (memory[SP]) >> 1; C = (memory[SP] & 0b00000001);
				PC = Pop_WordFromStack(Cycles, memory);
				Cycles -= 3;
			} break;

			default:
				switch (static_cast<int>(Ins)) {
				case 0: {} break;
				default: {
					cout << "Unknown instruction: " << static_cast<int>(Ins) << endl;}
				}
			}
		}
	}
};
#include <iostream>

#include "CPU.hpp"
#include "Opcodes.hpp"

using namespace std;

int main() {
	CPU cpu("pacman.bin");
	Mem memory; 

	cpu.Reset(memory);

    cpu.Execute(200000, memory);

	cout << "Accum: " << static_cast<int>(cpu.A) << endl;
	cout << "X: " << static_cast<int>(cpu.X) << endl;
	cout << "Y: " << static_cast<int>(cpu.Y) << endl;
	cout << "SP: " << static_cast<int>(cpu.SP) << endl;
	cout << "Carry: " << static_cast<int>(cpu.C) << endl;
	cout << "Verb: " << static_cast<int>(cpu.V) << endl;
	cout << "Zero: " << static_cast<int>(cpu.Z) << endl;
	cout << "Negative: " << static_cast<int>(cpu.N) << endl;

}
#include <iostream>

#include "CPU.hpp"

using namespace std;

int main() {
	CPU cpu;
	Mem memory;

	cpu.Reset(memory);

	cpu.X = 5;

    memory[0xFFFC] = 0x20;

    memory[0xFFFD] = 0xF;
    memory[0xFFFE] = 0xFF;

    memory[65295] = 0xA9;
    memory[65296] = 2;
    memory[65297] = 0x60;

    cpu.Execute(40, memory);

	cout << static_cast<int>(cpu.A) << endl;
}
#include<vector>
#include<fstream>
#include<stack>
#include<exception>
#include<iostream>

// this is going to be included in only main.cpp so definations are fine

enum struct InstructionType : char {
	INC,     //INC(n) add n to mem[mptr]
	MOV,     //MOV(n) add n to mptr
	JMZ,     //JMZ(n) pc = n if mem[mptr] is zero
	JNZ,     //JNZ(n) pc = n if mem[mptr] is non zero
	IN,      //IN()   mem[mptr] = input
	OUT,     //OUT()  output += mem[mptr]
	COMMENT, //does nothing
	//optimizing instructions
	MEMSET,  //MEMSET(n)   set mem[mptr] to n
	MEMMOV,  //MEMMOV(n,m) mem[mptr+n] += mem[mptr]*m
	INVERT   //INVERT()    mem[mptr] = 256 - mem[mptr]
};

struct Instruction {
	InstructionType type;
	int data;
	int data2;// extra data field for MEMMOV(relative location, multiplier)
//	Instruction* next = NULL;

	// default overloading
	Instruction() : type(InstructionType::COMMENT), data(0), data2(0) {};
	Instruction(InstructionType t) : type(t), data(0), data2(0) {};
	Instruction(InstructionType t, int d) : type(t), data(d), data2(0) {};
	Instruction(InstructionType t, int d, int d2) : type(t), data(d), data2(d2) {};

	bool is_dead() {
		return (type == InstructionType::COMMENT || data == 0);
	}
};

int get_tenative_data(InstructionType type, const char i) {
	switch (type) {
	case InstructionType::INC:
		return 44 - i;
	case InstructionType::MOV:
		return i - 61;
	case InstructionType::OUT:
	case InstructionType::IN:
		return 1;
	default:
		return 0;
	}
}

InstructionType classify(const char t) {
	switch (t) {
		case '<':
		case '>':
			return InstructionType::MOV;
		case '+':
		case '-':
			return InstructionType::INC;
		case '[':
			return InstructionType::JMZ;
		case ']': 
			return InstructionType::JNZ;
		case '.':
			return InstructionType::OUT;
		case ',':
			return InstructionType::IN;
		default:
			return InstructionType::COMMENT;
	}
}

std::vector<Instruction> make_instructions(std::fstream &fp) {
	char i;
	std::stack<size_t> loops;
	std::vector<Instruction> ret;

	while (fp.get(i)) {
		InstructionType t = classify(i);
		if (t == InstructionType::COMMENT) { continue; }

		if (t == InstructionType::JMZ) {
			loops.push(ret.size());
			ret.push_back(Instruction(t, 0));
		}
		else if (t != InstructionType::JNZ) {
			if (!ret.empty() && ret.back().type == t) {
				ret.back().data += get_tenative_data(t, i);
			}
			else {
				ret.push_back(Instruction(t, get_tenative_data(t, i)));
			}
		}
		else { // InstructionType::JNZ
			if (!loops.empty()) {
				ret[loops.top()].data = ret.size();
				ret.push_back(Instruction(t, loops.top()));
				//opportunity for optimization here
				//if the loop is innermost, and the sum of MOV == 0,
				//we can convert them into MEMMOV() amd MEMSET() instructions

				//for example, [-] or [+] will be MEMSET(0)
				//and [-]+++ will be MEMSET(3)
				//[->>>++>>>+<<<<<<]+++++ will be MEMMOV(3,2)  MEMMOV(6,1) MEMSET(5)
				//MEMMOV will add the current memory content to mptr+3 and mptr+6
				loops.pop();
			}
			else {
				throw std::invalid_argument("Encountered unmatched ']' in source");
			}
		}
	}

	if (!loops.empty()) {
		throw std::invalid_argument("Encountered unmatched '[' in source");
	}

	return ret;
}

void brainfuck(const std::vector<Instruction> &is, unsigned int len) {
	size_t mptr = 0;
	size_t pc = 0;
	size_t _s = is.size();
	std::vector<uint8_t> mem(len);

	while (pc < _s) {
		Instruction i = is[pc];
		switch (i.type) {
			case InstructionType::INC:
				mem[mptr] += i.data;
				break;
			case InstructionType::MOV:
				mptr = (i.data + mptr) % len;
				break;
			case InstructionType::JMZ:
				if (mem[mptr] == 0) {
					pc = i.data;
				}
				break;
			case InstructionType::JNZ:
				if (mem[mptr] != 0) {
					pc = i.data;
				}
				break;
			case InstructionType::IN: {
				char inp = '\0'; // emulate ...
				for (int j = 0; j < i.data; j++) {
					std::cin >> inp;
				}
				mem[mptr] = inp;
				break;
			}
			case InstructionType::OUT:
				for (int j = 0; j < i.data; j++) {
					std::cout << mem[mptr];
				}
				break;
			case InstructionType::MEMSET:
				mem[mptr] = i.data;
			case InstructionType::MEMMOV:
				mem[mptr+i.data] += (uint8_t)((int)mem[mptr] * i.data2);
			case InstructionType::INVERT:
				mem[mptr] = 0-mem[mptr];
			default:
				break;
		}
		pc++;
	}
	return;
}
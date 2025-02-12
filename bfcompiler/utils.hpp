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
	bool innermost_flag = true;

	while (fp.get(i)) {
		InstructionType t = classify(i);
		if (t == InstructionType::COMMENT) { continue; }

		if (t == InstructionType::JMZ) {
		    innermost_flag = true;
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
				//opportunity for optimization here
				//if the loop is innermost, and the sum of MOV == 0,
				//we can convert them into MEMMOV() amd MEMSET() instructions

				//for example, [-] or [+] will be MEMSET(0)
				//and [-]+++ will be MEMSET(3)
				//[->>>++>>>+<<<<<<]+++++ will be MEMMOV(3,2)  MEMMOV(6,1) MEMSET(5)
				//MEMMOV will add the current memory content to mptr+3 and mptr+6
				size_t loop_start = loops.top();//points to "["
				size_t loop_contnet_start = loop_start+1;//points to the instr after to "["
				size_t loop_end = ret.size();
				loops.pop();
				bool optimized_flag = false;
				while(innermost_flag){//only runs once, breakout loop
					innermost_flag = false;
					int mptr = 0;
					int min_mptr = 0;
					int max_mptr = 0;
					//check if inner MOV adds up to 0
					//as well as calculate max and min relative mptr
					for(size_t pc = loop_contnet_start; pc < loop_end; pc++){
						if(ret[pc].type == InstructionType::MOV){
							mptr += ret[pc].data;
							if(mptr < min_mptr){
								min_mptr = mptr;
							}else if(mptr > max_mptr){
								max_mptr = mptr;
							}
						}else if(
							//no IO in optimized chunk
							ret[pc].type == InstructionType::IN ||
							ret[pc].type == InstructionType::OUT
						){
							mptr = 1;
							break;
						}
					}
					if(mptr != 0)break;
					std::vector<uint8_t> memfield(max_mptr - min_mptr + 1);
					//tally up all the pluses and minuses
					for(size_t pc = loop_contnet_start; pc < loop_end; pc++){
						if(ret[pc].type == InstructionType::MOV){
							mptr += ret[pc].data;
						}else if(ret[pc].type == InstructionType::INC){
							memfield[mptr-min_mptr] += ret[pc].data;
						}
					}
					if(memfield[-min_mptr] != 255 && memfield[-min_mptr] != 1){
						//result unpredictable, not optimizing
						break;
					}
					//all conditions cleared, optimization possible
					std::vector<Instruction> block;
					
					//handling cases like [+>++<]
					//[+>++<] is equivalent to INVERT() MEMMOV(1,2) MEMSET(0)
					if( memfield[-min_mptr] == 1 && 
						//in case of [+], no INVERT is needed
						max_mptr - min_mptr != 0){
						//converting [+>+<] to [->+<] equivalent
						block.push_back(Instruction(InstructionType::INVERT));
					}
					for(mptr = min_mptr; mptr <= max_mptr; mptr++){
						if(mptr == 0 || memfield[mptr-min_mptr] == 0)continue;
						block.push_back(Instruction(InstructionType::MEMMOV,mptr,memfield[mptr-min_mptr]));
					}
					//todo: lookahead cases like [-]+++++ and convert it to MEMSET(5)
					block.push_back(Instruction(InstructionType::MEMSET,0));
					
					//discard the existing instruction field
					ret.resize(loop_start);
					//append block to ret
					ret.insert(ret.end(), block.begin(), block.end());
					optimized_flag = true;
				}
				if(!optimized_flag){
					ret[loop_start].data = loop_end;
					ret.push_back(Instruction(t, loop_start));
				}
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
				break;
			case InstructionType::MEMMOV:
				mem[mptr+i.data] += (uint8_t)((int)mem[mptr] * i.data2);
				break;
			case InstructionType::INVERT:
				mem[mptr] = 0-mem[mptr];
				break;
			default:
				break;
		}
		pc++;
	}
	return;
}
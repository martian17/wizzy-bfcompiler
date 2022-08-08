#include<vector>
#include<fstream>
#include<stack>
#include<exception>
#include<iostream>
#include<sstream>
#ifdef DEBUG
#define DEBUG_PRINT(x) std::cout << x << std::endl
#else
#define DEBUG_PRINT(x)
#endif

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

#ifdef DEBUG
namespace fmt{
	std::string to_string(const std::vector<uint8_t> vec){
		
		std::stringstream ss;
		ss << "[ ";
    	for (auto i = vec.begin(); i != vec.end(); i++){
			if(i != vec.begin()){
				 ss << ", ";
			}
			ss << (int)*i;
    	}
		ss << " ]";
        return ss.str();
    }
	std::string to_string(const std::vector<Instruction> instructions){
		
		std::stringstream ss;
		ss << "[ ";
    	for (auto i = instructions.begin(); i != instructions.end(); i++){
			if(i != instructions.begin()){
				 ss << ", ";
			}
			Instruction ins = *i;
			switch(ins.type){
				case InstructionType::INC:
				ss << "INC(" << ins.data << ")";
				break;
				case InstructionType::MOV:
				ss << "MOV(" << ins.data << ")";
				break;
				case InstructionType::JMZ:
				ss << "JMZ(" << ins.data << ")";
				break;
				case InstructionType::JNZ:
				ss << "JNZ(" << ins.data << ")";
				break;
				case InstructionType::IN:
				ss << "IN";
				break;
				case InstructionType::OUT:
				ss << "OUT";
				break;
				case InstructionType::COMMENT:
				ss << "COMMENT";
				break;
				case InstructionType::MEMSET:
				ss << "MEMSET(" << ins.data << ")";
				break;
				case InstructionType::MEMMOV:
				ss << "MEMMOV(" << ins.data << "," << ins.data2 << ")";
				break;
				case InstructionType::INVERT:
				ss << "INVERT";
				break;
			}
    	}
		ss << " ]";
        return ss.str();
    }
}

#include"fmt.hpp"
#endif


int lookahead_incr(std::string::iterator &it, std::string &str){
	int v = 0;
	it++;
	DEBUG_PRINT(*it);
	for(;it != str.end(); it++){
		char i = *it;
		InstructionType t = classify(i);
		if(t == InstructionType::INC){
			v += get_tenative_data(t,i);
		}else if(t == InstructionType::INC){
			continue;
		}else{
			break;
		}
	}
	it--;
	return v;
}

std::vector<Instruction> make_instructions(std::fstream &fp) {
	std::stringstream ss;
	ss << fp.rdbuf();
	std::string str = ss.str();
	
	std::stack<size_t> loops;
	std::vector<Instruction> ret;
	bool innermost_flag = true;
	
	for(auto it = str.begin(); it != str.end(); it++){
		char i = *it;
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
					
					//lookahead_incr(it,str) finds cases like [-]+++++ and converts them to MEMSET(5)
					//arguments are passed as reference
					//`std::str::iterator it` gets overwritten to a new position as a side effect
					//returns the number of total increments
					block.push_back(Instruction(InstructionType::MEMSET,lookahead_incr(it,str)));
					
					DEBUG_PRINT(fmt::format("memfield:        {} {} {} {}",memfield,min_mptr,-min_mptr,max_mptr));
					DEBUG_PRINT(fmt::format("Optimized block: {}",block));
					
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
	DEBUG_PRINT(fmt::format("\n\n\n*Debug mode*\nDisplaying bytecode:\nret: {}\n\n\n",ret));
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
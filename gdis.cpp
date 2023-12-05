/*
  GDIS

  Disassembler for GVM bytecode.

  This takes one parameter, which is an input GASM bytecode file, and writes to
  stdout a GASM program that can be compiled to that bytecode.
*/

#include "gvm.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <cstring>
#include <iomanip>

class GDisassembler {
public:
   std::vector<uint8_t>& code;
   uint64_t pc;

   GDisassembler(std::vector<uint8_t>& code) : code(code), pc(0) {}

   void disassemble() {
      pc = 0;
      while (pc < code.size()) {
         std::cout << "L" << std::setfill('0') << std::setw(5) << pc << std::setfill(' ') << std::setw(0) << ": ";
         uint8_t opcode = code[pc++];

         bool stk = (opcode & OP_ISTACK) || (opcode & OP_OSTACK); // stk will modify the expected operands
         opcode &= ~(OP_ISTACK | OP_OSTACK); // get rid of STACK bits to simplify case

         switch (opcode) {
         case OP_NOP:
            disasm("NOP", 0);
            break;
         case OP_TERM:
            disasm("TERM", 0);
            break;
         case OP_SET:
            disasm("SET", 2);
            break;
         case OP_JMP:
            disasm("JMP", 1, true);
            break;
         case OP_ADD:
            disasm("ADD", stk ? 0 : 2);
            break;
         case OP_SUB:
            disasm("SUB", stk ? 0 : 2);
            break;
         case OP_MUL:
            disasm("MUL", stk ? 0 : 2);
            break;
         case OP_DIV:
            disasm("DIV", stk ? 0 : 2);
            break;
         case OP_MOD:
            disasm("MOD", stk ? 0 : 2);
            break;
         case OP_OR:
            disasm("OR", stk ? 0 : 2);
            break;
         case OP_ANDL:
            disasm("ANDL", stk ? 0 : 2);
            break;
         case OP_XOR:
            disasm("XOR", stk ? 0 : 2);
            break;
         case OP_NOT:
            disasm("NOT", stk ? 0 : 1);
            break;
         case OP_SHL:
            disasm("SHL", stk ? 0 : 2);
            break;
         case OP_SHR:
            disasm("SHR", stk ? 0 : 2);
            break;
         case OP_INC:
            disasm("INC", 1);
            break;
         case OP_DEC:
            disasm("DEC", 1);
            break;
         case OP_PUSH:
            disasm("PUSH", 1);
            break;
         case OP_POP:
            disasm("POP", 1);
            break;
         case OP_AND:
            disasm("AND", stk ? 0 : 2);
            break;
         case OP_HOST:
            disasm("HOST", 0);
            break;
         case OP_VPUSH:
            disasm("VPUSH", 2);
            break;
         case OP_VPOP:
            disasm("VPOP", 2);
            break;
         case OP_CALL:
            disasm("CALL", 1, true);
            break;
         case OP_RET:
            disasm("RET", 1);
            break;
         case OP_JT:
            disasm("JT", stk ? 1 : 2, true); // SHOULD work: value to test is stk, label is still expected (so 1 instead of 0)
            break;
         case OP_JF:
            disasm("JF", stk ? 1 : 2, true); // SHOULD work: value to test is stk, label is still expected (so 1 instead of 0)
            break;
         case OP_EQ:
            disasm("EQ", stk ? 0 : 2);
            break;
         case OP_NE:
            disasm("NE", stk ? 0 : 2);
            break;
         case OP_GT:
            disasm("GT", stk ? 0 : 2);
            break;
         case OP_LT:
            disasm("LT", stk ? 0 : 2);
            break;
         case OP_GE:
            disasm("GE", stk ? 0 : 2);
            break;
         case OP_LE:
            disasm("LE", stk ? 0 : 2);
            break;
         case OP_NEG:
            disasm("NEG", stk ? 0 : 1);
            break;
         case OP_ORL:
            disasm("ORL", stk ? 0 : 2);
            break;
         default:
            std::cout << "UNKNOWN_OPCODE_" << int(opcode) << std::endl;
            break;
         }
      }
   }

private:

   uint64_t read(bool& is_pointer, bool jump_skip_control = false) {
      is_pointer = false;
      if (pc >= code.size()) {
         std::cerr << "Error: Unexpected end of code." << std::endl;
         exit(1);
      }
      uint8_t control;
      if (jump_skip_control)
         control = 2; // WHY was this ever set to 1?
      else
         control = code[pc++];
      uint8_t v = control & MAX_SHORT_VAL;
      bool regptr = control & REG_PTR;
      bool shortval = control & SHORT_VAL;
      uint64_t val;
      if (shortval) {
         val = v;
      } else {
         val = 0;
         if (pc + v > code.size()) {
            std::cerr << "Error: Unexpected end of code." << std::endl;
            exit(1);
         }
         memcpy(&val, &code[pc], v); // Little Endian
         pc += v;
      }
      if (regptr)
         is_pointer = true;
      return val;
   }

   std::string prop(bool is_pointer, uint64_t operand) {
      if (is_pointer)
         return std::string("@") + std::to_string(operand);
      else
         return std::to_string(operand);
   }

   // if isjump, the last operand expected (in count) is the jump
   void disasm(const std::string& operation, int count, bool isjump = false) {
      std::cout << operation << " ";
      bool pop1, pop2;
      uint64_t op1, op2;
      if (count >= 1) {
         bool op1_is_jump = (isjump && count == 1);
         op1 = read(pop1,op1_is_jump);
         if (op1_is_jump) {
            std::cout << "L" << std::setfill('0') << std::setw(5) << prop(pop1,op1) << std::setfill(' ') << std::setw(0) << " ";
         } else {
            std::cout << prop(pop1,op1) << " ";
         }
         if (count >= 2) {
            op2 = read(pop2,isjump);
            if (isjump) {
               std::cout << "L" << std::setfill('0') << std::setw(5) << prop(pop2,op2) << std::setfill(' ') << std::setw(0) << " ";
            } else {
               std::cout << prop(pop2,op2) << " ";
            }
         }
      }
      std::cout << std::endl;
   }
};

int main(int argc, char* argv[]) {
   if (argc != 2) {
      std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
      return 1;
   }

   const char* filename = argv[1];
   std::ifstream file(filename, std::ios::binary);
   if (!file.is_open()) {
      std::cerr << "Error opening file: " << filename << std::endl;
      return 1;
   }
   std::vector<uint8_t> code(std::istreambuf_iterator<char>(file), {});
   file.close();

   GDisassembler disassembler(code);
   disassembler.disassemble();

   return 0;
}

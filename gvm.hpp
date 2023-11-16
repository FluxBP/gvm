/*
  GVM

  Programs are limited to 65,536 bytes, since jump opcodes all expect uint16_t
  absolute addresses.

  At the end of execution, 'term' contains the machine return code (ERR_*).

  Registers are stored in an array 'r' with elements [0, REG_SIZE-1]. 'r' is
  just an alias to the first REG_SIZE elements in 'io', which is the memory
  of the machine that's shared between the host program and the GVM program.

  Special registers:
    r[0]: PC
    r[1]: R
    r[2]: S

  All other registers are free for the GVM host and running bytecode to use
  for whatever purposes.

  The host program can be called by the GVM program via the HOST instruction.
  The void(void) callback function provided to the GVM upon construction can
  then read and write the state of the GVM at will before returning to the GVM.

  -----------------------------------------------------------------------------

  TODO:

  - convert mathematical expressions into GASM instructions (generates
    arithmetic and stack instructions)
  - higher level macros
    - IF expression
      ...
      END
    - IF expression
      ...
      ELSE
      ...
      END
    - WHILE expression
      ...
      REPEAT
    - [@]# = expression
  - #include "xxx.h"
  - parse simple C constant declarations
    - enum {
         NAME = VALUE_OR_NAME ;
      };
    - const uint64_t NAME = VALUE_OR_NAME ;
  - GASM default parameters for instructions
  - use highest 2 bits of the opcode to denote R and/or S operands

*/

#include <cstdint>
#include <cstring>
#include <vector>
#include <functional>

enum {
   ERR_OK         = 0,  // program terminated successfully
   ERR_OPCODE     = 1,  // invalid opcode
   ERR_CODESIZE   = 2,  // unexpectedly ran out of code bytes
   ERR_DIVZERO    = 3,  // division by zero
   ERR_OPLIMIT    = 4,  // reached opcode run limit
   ERR_UNDERFLOW  = 5,  // stack is empty on pop
   ERR_RET        = 6,  // RET without CALL to return from
   ERR_SEGFAULT   = 7   // invalid io address accessed
};

enum : uint8_t {
   OP_NOP         = 0,
   OP_TERM        = 1,
   OP_SET         = 2,
   OP_JMP         = 3,
   OP_ADD         = 4,
   OP_SUB         = 5,
   OP_MUL         = 6,
   OP_DIV         = 7,
   OP_MOD         = 8,
   OP_OR          = 9,
   OP_AND         = 10,
   OP_XOR         = 11,
   OP_NOT         = 12,
   OP_SHL         = 13,
   OP_SHR         = 14,
   OP_INC         = 15,
   OP_DEC         = 16,
   OP_PUSH        = 17,
   OP_POP         = 18,
   OP_BAND        = 19,
   OP_HOST        = 20,
   OP_VPUSH       = 21,
   OP_VPOP        = 22,
   OP_CALL        = 23,
   OP_RET         = 24,
   OP_JF          = 25,
   OP_JT          = 26,
   OP_EQ          = 27,
   OP_NE          = 28,
   OP_GT          = 29,
   OP_LT          = 30,
   OP_GE          = 31,
   OP_LE          = 32
};

const uint8_t REG_PTR = 0x80; // misnomer: this is a pointer to the memory (io)
const uint8_t SHORT_VAL = 0x40;
const uint8_t MAX_SHORT_VAL = 0x3F; // 63 (also control byte bitmask)

const uint64_t IO_SIZE = 1024; // 8 Kb of IO memory
const uint64_t REG_SIZE = 8;
const uint64_t DEFAULT_OP_LIMIT = 50000;

class GVM {
public:

   // type for storing registers in the call stack
   typedef std::array<uint64_t, REG_SIZE> registers_t;

   // type of io array
   typedef uint64_t memory_t[IO_SIZE];

   // call stack (just a stack of saved register sets;
   // CALL pushes r into this, RET pops this back into r)
   std::vector<registers_t>        context;

   // global state (not affected by CALL/RET)
   std::vector<uint64_t>           stack;
   std::vector<uint8_t>&           code;
   memory_t&                       io;

   // named registers PC(0), R(1), S(2): these are used by the vm
   uint64_t&                       PC = io[0];
   uint64_t&                       R  = io[1];
   uint64_t&                       S  = io[2];
   // registers io[3] .. io[7] not used by the opcode impls

   using HostCallback = std::function<void()>;
   HostCallback                    hostCallback;

   uint64_t                        term;   // stores the VM exit code, ==0 OK, >0 error
   uint64_t                        count;  // counts machine instructions executed
   uint8_t                         opcode; // last opcode executed

   GVM(uint64_t (&io)[IO_SIZE], std::vector<uint8_t>& code)
      : io(io), code(code), hostCallback(nullptr) {}

   GVM(uint64_t (&io)[IO_SIZE], std::vector<uint8_t>& code, const HostCallback& hostCallback)
      : io(io), code(code), hostCallback(hostCallback) {}

   void setCode(std::vector<uint8_t>& newCode) { code = newCode; }

   void setHostCallback(const HostCallback& newHostCallback) { hostCallback = newHostCallback; }

   void clearRegisters() { std::memset(&(io[0]), 0, sizeof(uint64_t) * REG_SIZE); }

   uint64_t& get(uint64_t index) {
      if (index < IO_SIZE) {
         return io[index];
      } else {
         term = ERR_SEGFAULT;
         return R; // whatever; program is dead
      }
   }

   void run(uint64_t limit = DEFAULT_OP_LIMIT) {
      term = ERR_OK;
      count = 0;
      uint64_t op1, op2;
      while (!term && PC < code.size()) {
         if (++count > limit) {
            term = ERR_OPLIMIT;
            break;
         }
         opcode = code[PC++];
         switch (opcode) {
         case OP_NOP:
            break;
         case OP_TERM:
            PC = UINT64_MAX;
            break;
         case OP_SET:
            op1 = read();
            op2 = read();
            get(op1) = op2;
            break;
         case OP_JMP:
            op1 = read(true);
            PC = op1;
            break;
         case OP_ADD:
            op1 = read();
            op2 = read();
            R = op1 + op2;
            break;
         case OP_SUB:
            op1 = read();
            op2 = read();
            R = op1 - op2;
            break;
         case OP_MUL:
            op1 = read();
            op2 = read();
            R = op1 * op2;
            break;
         case OP_DIV:
            op1 = read();
            op2 = read();
            if (op2 != 0)
               R = op1 / op2;
            else
               term = ERR_DIVZERO;
            break;
         case OP_MOD:
            op1 = read();
            op2 = read();
            if (op2 != 0)
               R = op1 % op2;
            else
               term = ERR_DIVZERO;
            break;
         case OP_OR:
            op1 = read();
            op2 = read();
            R = op1 | op2;
            break;
         case OP_AND:
            op1 = read();
            op2 = read();
            R = op1 && op2;
            break;
         case OP_XOR:
            op1 = read();
            op2 = read();
            R = op1 ^ op2;
            break;
         case OP_NOT:
            op1 = read();
            R = ~op1;
            break;
         case OP_SHL:
            op1 = read();
            op2 = read();
            R = op1 << op2;
            break;
         case OP_SHR:
            op1 = read();
            op2 = read();
            R = op1 >> op2;
            break;
         case OP_INC:
            op1 = read();
            ++get(op1);
            break;
         case OP_DEC:
            op1 = read();
            --get(op1);
            break;
         case OP_PUSH:
            op1 = read();
            stack.push_back(op1);
            break;
         case OP_POP:
            if (stack.size() == 0) {
               term = ERR_UNDERFLOW;
            } else {
               op1 = read();
               get(op1) = stack.back();
               stack.pop_back();
            }
            break;
         case OP_BAND:
            op1 = read();
            op2 = read();
            R = op1 & op2;
            break;
         case OP_HOST:
            hostCallback();
            break;
         case OP_VPUSH:
            op1 = read();
            op2 = read();
            ++get(op1);
            get(get(op1)) = op2;
            break;
         case OP_VPOP:
            op1 = read();
            op2 = read();
            get(op2) = get(op1);
            --get(op1);
            break;
         case OP_CALL:
            op1 = read(true); // function address
            registers_t regs;
            memcpy(regs.data(), &(io[0]), sizeof(uint64_t) * REG_SIZE);
            context.push_back(regs); // save all registers, including PC for return
            PC = op1;
            break;
         case OP_RET:
            op1 = read(); // convenience return value
            if (context.size() == 0) {
               term = ERR_RET;
            } else {
               memcpy(&(io[0]), context.back().data(), sizeof(uint64_t) * REG_SIZE);
               context.pop_back();
               R = op1; // R is assigned the return value instead of restored
               break;
            }
            break;
         case OP_JF:
            op1 = read();
            op2 = read(true);
            if (!op1)
               PC = op2;
         case OP_JT:
            op1 = read();
            op2 = read(true);
            if (op1)
               PC = op2;
         case OP_EQ:
            op1 = read();
            op2 = read();
            R = op1 == op2;
            break;
         case OP_NE:
            op1 = read();
            op2 = read();
            R = op1 != op2;
            break;
         case OP_GT:
            op1 = read();
            op2 = read();
            R = op1 > op2;
            break;
         case OP_LT:
            op1 = read();
            op2 = read();
            R = op1 < op2;
            break;
         case OP_GE:
            op1 = read();
            op2 = read();
            R = op1 >= op2;
            break;
         case OP_LE:
            op1 = read();
            op2 = read();
            R = op1 <= op2;
            break;
         default:
            term = ERR_OPCODE;
         }
      }
   }

private:

   uint64_t read(bool jump_skip_control = false) {
      if (PC >= code.size()) {
         term = ERR_CODESIZE;
         return 0;
      }
      uint8_t control;
      if (jump_skip_control)
         control = 2;
      else
         control = code[PC++];
      uint8_t v = control & MAX_SHORT_VAL;
      bool regptr = control & REG_PTR;
      bool shortval = control & SHORT_VAL;
      uint64_t val;
      if (shortval) {
         val = v;
      } else {
         val = 0;
         if (PC + v >= code.size()) {
            term = ERR_CODESIZE;
            return 0;
         }
         memcpy(&val, &code[PC], v); // Little-endian
         PC += v;
      }
      if (regptr)
         val = get(val);
      return val;
   }
};

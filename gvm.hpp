/*
  GVM

  Programs are limited to 65,536 bytes, since jump opcodes all expect uint16_t
  absolute addresses.

  At the end of execution, 'term' contains the machine return code (ERR_*).

  Registers are the memory ('io') cell range [0, REG_SIZE-1].

  Special registers:
    io[0]: PC
    io[1]: R
    io[2]: S

  All other registers are free for the GVM host and running bytecode to use
  for whatever purposes.

  The host program can be called by the GVM program via the HOST instruction.
  The void(void) callback function provided to the GVM upon construction can
  then read and write the state of the GVM at will before returning to the GVM.

  Several opcodes can have the STACK bit set to modify their default
  register-based implementation to a stack-based implementation. Since the
  opcode is just 1 byte, this flag limits the opcode range to [0, 127].

*/

#include <cstdint>
#include <cstring>
#include <vector>
#include <functional>

#ifdef DEBUG
#include <iostream>
#endif

enum {
   ERR_OK         = 0,  // program terminated successfully
   ERR_OPCODE     = 1,  // invalid opcode
   ERR_CODESIZE   = 2,  // unexpectedly ran out of code bytes
   ERR_DIVZERO    = 3,  // division by zero
   ERR_OPLIMIT    = 4,  // reached opcode run limit
   ERR_UNDERFLOW  = 5,  // stack is empty on pop
   ERR_RET        = 6,  // RET without CALL to return from
   ERR_SEGFAULT   = 7,  // invalid io address accessed
   ERR_NEGNUM     = 8   // arithmetic underflow
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
   OP_ANDL        = 10,
   OP_XOR         = 11,
   OP_NOT         = 12,
   OP_SHL         = 13,
   OP_SHR         = 14,
   OP_INC         = 15,
   OP_DEC         = 16,
   OP_PUSH        = 17,
   OP_POP         = 18,
   OP_AND         = 19,
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
   OP_LE          = 32,
   OP_NEG         = 33,
   OP_ORL         = 34
};

const uint8_t REG_PTR = 0x80; // misnomer: this is a pointer to the memory (io)
const uint8_t SHORT_VAL = 0x40;
const uint8_t MAX_SHORT_VAL = 0x3F; // 63 (also control byte bitmask)

const uint8_t STACK = 0x80; // opcode reads/writes the stack instead

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
   // CALL pushes register range of io into this and
   // RET pops this back into the register range of io)
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

#ifdef DEBUG
   bool debug;
#endif

   GVM(uint64_t (&io)[IO_SIZE], std::vector<uint8_t>& code)
      : io(io), code(code), hostCallback(nullptr)
      {
#ifdef DEBUG
         debug = false;
#endif
      }

   GVM(uint64_t (&io)[IO_SIZE], std::vector<uint8_t>& code, const HostCallback& hostCallback)
      : io(io), code(code), hostCallback(hostCallback)
      {
#ifdef DEBUG
         debug = false;
#endif
      }

#ifdef DEBUG
   void setDebug(bool newDebug) { debug = newDebug; }
#endif

   void setCode(std::vector<uint8_t>& newCode) { code = newCode; }

   void setHostCallback(const HostCallback& newHostCallback) { hostCallback = newHostCallback; }

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
#ifdef DEBUG
         if (debug) {
            std::cout << "PC=" << std::to_string(PC - 1) << " R=" << std::to_string(R) << " OPC=" << std::to_string(opcode) << " PEEK=[";
            for (int i=1; i<5; ++i)
               if (PC + i < code.size()) {
                  if (i > 1)
                     std::cout << ", ";
                  std::cout << std::to_string(code[PC + i]);
               }
            std::cout << "] STK(" << stack.size() << "): ";
            for (const auto& element : stack)
               std::cout << element << " ";
            std::cout << std::endl;
         }
#endif
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
         case OP_ADD | STACK:
            op2 = pop();
            op1 = pop();
            push(op1 + op2);
            break;
         case OP_SUB:
            op1 = read();
            op2 = read();
            R = op1 - op2;
            if (op1 < op2)
               term = ERR_NEGNUM;
            break;
         case OP_SUB | STACK:
            op2 = pop();
            op1 = pop();
            push(op1 - op2);
            if (op1 < op2)
               term = ERR_NEGNUM;
            break;
         case OP_MUL:
            op1 = read();
            op2 = read();
            R = op1 * op2;
            break;
         case OP_MUL | STACK:
            op2 = pop();
            op1 = pop();
            push(op1 * op2);
            break;
         case OP_DIV:
            op1 = read();
            op2 = read();
            if (op2 != 0) {
               R = op1 / op2;
               break;
            } else {
               term = ERR_DIVZERO;
               break;
            }
         case OP_DIV | STACK:
            op2 = pop();
            op1 = pop();
            if (op2 != 0) {
               push(op1 / op2);
               break;
            } else {
               term = ERR_DIVZERO;
               break;
            }
         case OP_MOD:
            op1 = read();
            op2 = read();
            if (op2 != 0) {
               R = op1 % op2;
               break;
            } else {
               term = ERR_DIVZERO;
               break;
            }
         case OP_MOD | STACK:
            op2 = pop();
            op1 = pop();
            if (op2 != 0) {
               push(op1 % op2);
               break;
            } else {
               term = ERR_DIVZERO;
               break;
            }
         case OP_OR:
            op1 = read();
            op2 = read();
            R = op1 | op2;
            break;
         case OP_OR | STACK:
            op2 = pop();
            op1 = pop();
            push(op1 | op2);
            break;
         case OP_ANDL:
            op1 = read();
            op2 = read();
            R = op1 && op2;
            break;
         case OP_ANDL | STACK:
            op2 = pop();
            op1 = pop();
            R = op1 && op2;
            break;
         case OP_XOR:
            op1 = read();
            op2 = read();
            R = op1 ^ op2;
            break;
         case OP_XOR | STACK:
            op2 = pop();
            op1 = pop();
            push(op1 ^ op2);
            break;
         case OP_NOT:
            op1 = read();
            R = !op1;
            break;
         case OP_NOT | STACK:
            op1 = pop();
            push(!op1);
            break;
         case OP_SHL:
            op1 = read();
            op2 = read();
            R = op1 << op2;
            break;
         case OP_SHL | STACK:
            op2 = pop();
            op1 = pop();
            push(op1 << op2);
            break;
         case OP_SHR:
            op1 = read();
            op2 = read();
            R = op1 >> op2;
            break;
         case OP_SHR | STACK:
            op2 = pop();
            op1 = pop();
            push(op1 >> op2);
            break;
         case OP_INC:
            op1 = read();
            ++get(op1);
            break;
         case OP_DEC: // doesn't do < 0 check
            op1 = read();
            --get(op1);
            break;
         case OP_PUSH:
            op1 = read();
            push(op1);
            break;
         case OP_POP:
            op1 = read();
            get(op1) = pop();
            break;
         case OP_AND:
            op1 = read();
            op2 = read();
            R = op1 & op2;
            break;
         case OP_AND | STACK:
            op2 = pop();
            op1 = pop();
            push(op1 & op2);
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
               break;
            } else {
               memcpy(&(io[0]), context.back().data(), sizeof(uint64_t) * REG_SIZE);
               context.pop_back();
               R = op1; // R is assigned the return value instead of restored
               break;
            }
         case OP_JF:
            op1 = read();
            if (!op1) {
               PC = read(true);
               break;
            } else {
               PC += 2;
               break;
            }
         case OP_JF | STACK:
            op1 = pop();
            if (!op1) {
               PC = read(true);
               break;
            } else {
               PC += 2;
               break;
            }
         case OP_JT:
            op1 = read();
            if (op1) {
               PC = read(true);
               break;
            } else {
               PC += 2;
               break;
            }
         case OP_JT | STACK:
            op1 = pop();
            if (op1) {
               PC = read(true);
               break;
            } else {
               PC += 2;
               break;
            }
         case OP_EQ:
            op1 = read();
            op2 = read();
            R = op1 == op2;
            break;
         case OP_EQ | STACK:
            op2 = pop();
            op1 = pop();
            push(op1 == op2);
            break;
         case OP_NE:
            op1 = read();
            op2 = read();
            R = op1 != op2;
            break;
         case OP_NE | STACK:
            op2 = pop();
            op1 = pop();
            push(op1 != op2);
            break;
         case OP_GT:
            op1 = read();
            op2 = read();
            R = op1 > op2;
            break;
         case OP_GT | STACK:
            op2 = pop();
            op1 = pop();
            push(op1 > op2);
            break;
         case OP_LT:
            op1 = read();
            op2 = read();
            R = op1 < op2;
            break;
         case OP_LT | STACK:
            op2 = pop();
            op1 = pop();
            push(op1 < op2);
            break;
         case OP_GE:
            op1 = read();
            op2 = read();
            R = op1 >= op2;
            break;
         case OP_GE | STACK:
            op2 = pop();
            op1 = pop();
            push(op1 >= op2);
            break;
         case OP_LE:
            op1 = read();
            op2 = read();
            R = op1 <= op2;
            break;
         case OP_LE | STACK:
            op2 = pop();
            op1 = pop();
            push(op1 <= op2);
            break;
         case OP_NEG:
            op1 = read();
            R = ~op1;
            break;
         case OP_NEG | STACK:
            op1 = pop();
            push(~op1);
            break;
         case OP_ORL:
            op1 = read();
            op2 = read();
            R = op1 || op2;
            break;
         case OP_ORL | STACK:
            op2 = pop();
            op1 = pop();
            push(op1 || op2);
            break;
         default:
            term = ERR_OPCODE;
         }
      }
   }

private:

   uint64_t& get(uint64_t index) {
      if (index < IO_SIZE) {
         return io[index];
      } else {
         term = ERR_SEGFAULT;
         return R; // whatever; program is dead
      }
   }

   void push(uint64_t v) { stack.push_back(v); }

   uint64_t pop() {
      if (stack.size() == 0) {
         term = ERR_UNDERFLOW;
         return 0; // whatever; program is dead
      }
      uint64_t operand = stack.back();
      stack.pop_back();
      return operand;
   }

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
         if (PC + v > code.size()) {
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

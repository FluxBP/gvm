/*
  Example host program for the GVM.

  To instead use the method of a class as the host callback:

  MyClass myObject;
  gvm.setCallback(std::bind(&MyClass::myCallbackMethod, &myObject));
*/

#define DEBUG
#include "gvm.hpp"

#include <iostream>
#include <fstream>

GVM* vm = nullptr;
GVM::memory_t io;

void example_host_function() {
   std::cout << "example_host_function() called by the bytecode, pc = " << vm->PC << std::endl;
}

int main(int argc, char* argv[]) {
   std::memset(&(io[0]), 0, sizeof(uint64_t) * IO_SIZE);

   if (argc < 2) {
      std::cerr << "Usage: " << argv[0] << " <filename> [--debug]" << std::endl;
      return 1;
   }

   bool debug = (argc > 2);

   // Load the bytecode
   const char* filename = argv[1];
   std::ifstream file(filename, std::ios::binary);
   if (!file.is_open()) {
      std::cerr << "Error opening file: " << filename << std::endl;
      return 1;
   }
   std::vector<uint8_t> code(std::istreambuf_iterator<char>(file), {});
   file.close();

   // Run the bytecode
   vm = new GVM(io, code, example_host_function);
   vm->setDebug(debug);
   vm->run();
   std::cout << "vm.run() ended, term = " << vm->term << " opcode = " << int(vm->opcode) << std::endl;

   // Dump all io (includes registers)
   bool skipped = false;
   for (size_t i = 0; i < IO_SIZE; ++i) {
      uint64_t v = io[i];
      if (v > 0 || i < REG_SIZE) {
         if (skipped) {
            skipped = false;
            std::cout << "..." << std::endl;
         }
         if (i < REG_SIZE)
            std::cout << "*";
         std::cout << "io[" << i << "] = ";
         if (v == UINT64_MAX)
            std::cout << "(UINT64_MAX)";
         else
            std::cout << v;
         std::cout << std::endl;
      } else {
         skipped = true;
      }
   }

   bool success = vm->term != 0;

   delete vm;
   vm = nullptr;

   return success;
}

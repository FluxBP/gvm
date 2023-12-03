#include "expr.hpp"

int main(int argc, char* argv[]) {

   printf("GASM expression parser\n\n");
   printf("Based on Shunting Yard implementation by Takayuki Matsuoka et al.:\n  https://gist.github.com/t-mat/b9f681b7591cdae712f6\n\n");
   
   printf("Usage:\n");
   printf("  expr [GASM expression]\n\n");

   // Check if there are command-line arguments
   std::string expr;
   if (argc > 1) {
      // Concatenate all command-line arguments into a single std::string
      for (int i = 1; i < argc; ++i) {
         expr += argv[i];
         if (i < argc - 1) {
            expr += ' '; // Add a space between arguments
         }
      }
   } else {
      // default expression

      expr = "88 + ~@99+4*2/(6-5)*2*3";
      //expr = "1 / 2 + 3 / 4 + 8 + @10 + @11 + @12 + @13 + @14 / 9 + @15 / (8 + 6 / (7 / 8 + 1 / @16)) + 9";
   }

   printf("Input (expression):\n  %s\n\n", expr.c_str());

   printf("Output (GASM program):\n\n");

   std::string prog = expressionToGASM(expr, true);

   std::cout << prog;
}

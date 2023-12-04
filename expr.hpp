// ----------------------------------------------------------------------------
// Based on https://gist.github.com/t-mat/b9f681b7591cdae712f6
// By Takayuki Matsuoka et al. ; See original notes below.
// ----------------------------------------------------------------------------
//
// Shunting-yard Algorithm
// https://en.wikipedia.org/wiki/Shunting-yard_algorithm
//
// Implementation notes for unary operators by Austin Taylor
//  https://stackoverflow.com/a/5240912
//
// Example:
//  https://ideone.com/VocUTq
//
// License:
//  If you use this code in binary / compiled / un-commented (removing all text comments) form,
//  you can use it under CC0 license.
//
//  But if you use this code as source code / readable text, since main content of this code is
//  their notes, I recommend you to indicate notices which conform CC-BY-SA.  For example,
//
//  --- ---
//  YOUR-CONTENT uses the following materials.
//  (1) Wikipedia article [Shunting-yard algorithm](https://en.wikipedia.org/wiki/Shunting-yard_algorithm),
//  which is released under the [Creative Commons Attribution-Share-Alike License 3.0](https://creativecommons.org/licenses/by-sa/3.0/).
//  (2) [Implementation notes for unary operators in Shunting-Yard algorithm](https://stackoverflow.com/a/5240912) by Austin Taylor
//  which is released under the [Creative Commons Attribution-Share-Alike License 2.5](https://creativecommons.org/licenses/by-sa/2.5/).
//  --- ---
// ----------------------------------------------------------------------------

/*
  GASM expression parser to GVM stack-based program

  This is basically a hack of the code it is based on to produce the GASM
  program string. There's a bunch of dead code and meaningless values generated
  because we are not interested in actually evaluating the expression.

  -----------------------------------------------------------------------------

  TODO:

*/

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <deque>
#include <tuple>
#include <stdio.h>
#include <math.h>

static const char* reportFmt = "|%-5s|%-32s|%17s| %s\n";

class Token {
public:
   enum class Type {
      Unknown,
      Number,
      Register,
      Operator,
      LeftParen,
      RightParen,
   };

   Token(Type type,
         const std::string& s,
         int precedence = -1,
         bool rightAssociative = false,
         bool unary = false
      )
      : type { type }
      , str ( s )
      , precedence { precedence }
      , rightAssociative { rightAssociative }
      , unary { unary }
      {}

   const Type type;
   std::string str;
   const int precedence;
   const bool rightAssociative;
   const bool unary;
};

std::ostream& operator<<(std::ostream& os, const Token& token) {
   if (token.type == Token::Type::Register)
      os << "@";

   os << token.str;
   return os;
}


// Debug output
template<class T, class U>
void debugReport(
   const Token& token
   , const T& queue
   , const U& stack
   , const std::string& comment = ""
   ) {
   std::ostringstream ossQueue;
   for(const auto& t : queue) {
      ossQueue << " " << t;
   }

   std::ostringstream ossStack;
   for(const auto& t : stack) {
      ossStack << " " << t;
   }

   std::ostringstream ossToken;
   ossToken << token;

#ifdef DEBUG
   printf(reportFmt
          , ossToken.str().c_str() //token.str.c_str()
          , ossQueue.str().c_str()
          , ossStack.str().c_str()
          , comment.c_str()
      );
#endif
}


std::deque<Token> exprToTokens(const std::string& expr) {
   std::deque<Token> tokens;

   for(const auto* p = expr.c_str(); *p; ++p) {
      if(isblank(*p)) {
         // do nothing
      } else if(*p == '@') {
         ++p;
         const auto* b = p;
         while(isdigit(*p)) {
            ++p;
         }
         const auto s = std::string(b, p);
         tokens.push_back(Token { Token::Type::Register, s });
         --p;
      } else if(isdigit(*p)) {
         const auto* b = p;
         while(isdigit(*p)) {
            ++p;
         }
         const auto s = std::string(b, p);
         tokens.push_back(Token { Token::Type::Number, s });
         --p;
      } else {
         Token::Type t = Token::Type::Unknown;
         int precedence = -1;
         bool rightAssociative = false;
         bool unary = false;
         char c = *p;
         char c2 = *(p + 1);

         int opsz = 1;

         switch(c) {
         default:                                    break;
         case '(':   t = Token::Type::LeftParen;     break;
         case ')':   t = Token::Type::RightParen;    break;

            //XOR
         case '^':   t = Token::Type::Operator;      precedence = 4; /*rightAssociative = true;*/  break;

         case '*':   t = Token::Type::Operator;      precedence = 10; break;
         case '/':   t = Token::Type::Operator;      precedence = 10; break;
         case '%':   t = Token::Type::Operator;      precedence = 10; break;
         case '+':   t = Token::Type::Operator;      precedence = 9; break;
         case '-':
            // If current token is '-'
            // and if it is the first token, or preceded by another operator, or left-paren,
            if(   tokens.empty()
                  || tokens.back().type == Token::Type::Operator
                  || tokens.back().type == Token::Type::LeftParen
               ) {
               //std::cout << "ERROR: - is not a unary operator" << std::endl;
               //exit(1);
               throw std::runtime_error("ERROR: - is not a unary operator");
            } else {
               // otherwise, it's binary '-'
               t = Token::Type::Operator;
               precedence = 9;
            }
            break;
         case '~':
            // If current token is '~'
            // and if it is the first token, or preceded by another operator, or left-paren,
            if(   tokens.empty()
                  || tokens.back().type == Token::Type::Operator
                  || tokens.back().type == Token::Type::LeftParen
               ) {
               // it's unary '~'
               // note#1 : 'n' is a special operator name for unary '~'
               // note#2 : It has highest precedence than any of the infix operators
               unary = true;
               //c = '~';
               t = Token::Type::Operator;
               precedence = 11;
            } else {
               //std::cout << "ERROR: ~ is not a binary operator" << std::endl;
               //exit(1);
               throw std::runtime_error("ERROR: ~ is not a binary operator");
            }
            break;
         case '&':
            switch (c2) {
            case '&':   // &&
               opsz = 2;
               t = Token::Type::Operator;      precedence = 2; break;
               break;
            default:    // just &
               t = Token::Type::Operator;      precedence = 5; break;
               break;
            }
            break;
         case '|':
            switch (c2) {
            case '|':   // ||
               opsz = 2;
               //break;   || and | behave identically
            default:    // just |
               t = Token::Type::Operator;      precedence = 3; break;
               break;
            }
            break;
         case '<':
            switch (c2) {
            case '<':   // <<
               opsz = 2;
               t = Token::Type::Operator;      precedence = 8; break;
               break;
            case '=':   // <=
               opsz = 2;
               t = Token::Type::Operator;      precedence = 7; break;
               break;
            default:    // <
               t = Token::Type::Operator;      precedence = 7; break;
               break;
            }
            break;
         case '>':
            switch (c2) {
            case '>':   // >>
               opsz = 2;
               t = Token::Type::Operator;      precedence = 8; break;
               break;
            case '=':   // >=
               opsz = 2;
               t = Token::Type::Operator;      precedence = 7; break;
               break;
            default:    // >
               t = Token::Type::Operator;      precedence = 7; break;
               break;
            }
            break;
         case '=':
            switch (c2) {
            case '=':   // ==
               opsz = 2;
               t = Token::Type::Operator;      precedence = 6; break;
               break;
            default: break;   // '='  (not supported) -- doing the Unknown token case
            }
            break;
         case '!':
            switch (c2) {
            case '=':   // !=
               opsz = 2;
               t = Token::Type::Operator;      precedence = 6; break;
               break;
            default:    // !   not
               // If current token is '!'
               // and if it is the first token, or preceded by another operator, or left-paren,
               if(   tokens.empty()
                     || tokens.back().type == Token::Type::Operator
                     || tokens.back().type == Token::Type::LeftParen
                  )
               {
                  // it's unary '!'
                  // note#1 : 'n' is a special operator name for unary '!'
                  // note#2 : It has highest precedence than any of the infix operators
                  unary = true;
                  //c = '!';
                  t = Token::Type::Operator;
                  precedence = 11;
               } else {
                  throw std::runtime_error("ERROR: ! is not a binary operator");
               }
               break;
            }
            break;
         }
         std::string s;
         if (opsz == 1) {
            s = std::string(1, c);
         } else if (opsz == 2) {
            s = std::string(1, c) + std::string(1, c2);
            ++p; // skip extra char
         } else {
            throw std::runtime_error("ERROR bad opsz");
         }
         tokens.push_back(Token { t, s, precedence, rightAssociative, unary });
      }
   }

   return tokens;
}


std::deque<Token> shuntingYard(const std::deque<Token>& tokens) {
   std::deque<Token> queue;
   std::vector<Token> stack;

   // While there are tokens to be read:
   for(auto token : tokens) {
      // Read a token
      switch(token.type) {
      case Token::Type::Register:
         //std::cout << "pushed register onto stack: " << token << std::endl;
      case Token::Type::Number:
         // If the token is a number, then add it to the output queue
         queue.push_back(token);
         break;

      case Token::Type::Operator:
      {
         // If the token is operator, o1, then:
         const auto o1 = token;

         // while there is an operator token,
         while(!stack.empty()) {
            // o2, at the top of stack, and
            const auto o2 = stack.back();

            // either o1 is left-associative and its precedence is
            // *less than or equal* to that of o2,
            // or o1 if right associative, and has precedence
            // *less than* that of o2,
            if(
               (! o1.rightAssociative && o1.precedence <= o2.precedence)
               ||  (  o1.rightAssociative && o1.precedence <  o2.precedence)
               ) {
               // then pop o2 off the stack,
               stack.pop_back();
               // onto the output queue;
               queue.push_back(o2);

               continue;
            }

            // @@ otherwise, exit.
            break;
         }

         // push o1 onto the stack.
         stack.push_back(o1);
      }
      break;

      case Token::Type::LeftParen:
         // If token is left parenthesis, then push it onto the stack
         stack.push_back(token);
         break;

      case Token::Type::RightParen:
         // If token is right parenthesis:
      {
         bool match = false;

         // Until the token at the top of the stack
         // is a left parenthesis,
         while(! stack.empty() && stack.back().type != Token::Type::LeftParen) {
            // pop operators off the stack
            // onto the output queue.
            queue.push_back(stack.back());
            stack.pop_back();
            match = true;
         }

         if(!match && stack.empty()) {
            // If the stack runs out without finding a left parenthesis,
            // then there are mismatched parentheses.
            //printf("RightParen error (%s)\n", token.str.c_str());

            throw std::runtime_error("ERROR: RightParen error");

            return {};
         }

         // Pop the left parenthesis from the stack,
         // but not onto the output queue.
         stack.pop_back();
      }
      break;

      default:
         //printf("error (%s)\n", token.str.c_str());
         throw std::runtime_error("ERROR: (token): " + token.str);
         return {};
      }

      debugReport(token, queue, stack);
   }

   // When there are no more tokens to read:
   //   While there are still operator tokens in the stack:
   while(! stack.empty()) {
      // If the operator token on the top of the stack is a parenthesis,
      // then there are mismatched parentheses.
      if(stack.back().type == Token::Type::LeftParen) {
         //printf("Mismatched parentheses error\n");
         throw std::runtime_error("ERROR: Mismatched parentheses error");

         return {};
      }

      // Pop the operator onto the output queue.
      queue.push_back(std::move(stack.back()));
      stack.pop_back();
   }

   debugReport(Token { Token::Type::Unknown, "End" }, queue, stack);

   //Exit.
   return queue;
}

// instead of evaluating a result, this prints out the assembly propgram
std::string expressionToGASM(const std::string& expr, bool lf) {

   // gasm output stream
   std::ostringstream ossg;

   std::string zero = "0";

   char sep;
   if (lf) {
      sep = '\n';
   } else {
      sep = ' ';
   }

#ifdef DEBUG
   printf("Tokenize\n");
   printf(reportFmt, "Token", "Queue", "Stack", "");
#endif
   
   const auto tokens = exprToTokens(expr);
   auto queue = shuntingYard(tokens);
   std::vector<Token> stack;

#ifdef DEBUG
   printf("\nCalculation\n");
   printf(reportFmt, "Token", "Queue", "Stack", "");
#endif
   
   while(! queue.empty()) {
      std::string op;

      const auto token = queue.front();
      queue.pop_front();
      switch(token.type) {
      case Token::Type::Register:
         stack.push_back(token);
         op = "Push @" + token.str;
#ifdef DEBUG
         std::cout << "ASM OPERATIO1: PUSH " << token << std::endl;
#endif
         ossg << "PUSH " << token << sep;
         break;

      case Token::Type::Number:
         stack.push_back(token);
         op = "Push " + token.str;
#ifdef DEBUG
         std::cout << "ASM OPERATIO2: PUSH " << token << std::endl;
#endif
         ossg << "PUSH " << token << sep;
         break;

      case Token::Type::Operator:
      {
         if(token.unary) {
            // unray operators
            Token rhs = stack.back();
            stack.pop_back();
            switch(token.str[0]) {
            default:
               //printf("Operator error [%s]\n", token.str.c_str());
               //exit(0);
               throw std::runtime_error("ERROR: Operator error");
               break;
               
               //    VM literally doesn't support an unary -  since everything is uint64_t

            case '~':                   // Special operator name for unary '~'
               rhs.str = std::to_string( ~ std::stoi(rhs.str) );
               stack.push_back( rhs );

               ossg << "NEG" << sep;
               break;
            case '!':                   // Special operator name for unary '~'
               rhs.str = std::to_string( ! std::stoi(rhs.str) );
               stack.push_back( rhs );

               ossg << "NOT" << sep;
               break;
               
            }
            //op = "Push (unary) " + token.str + " " + std::to_string(rhs);
#ifdef DEBUG
            op = "Push (unary) something";
#endif
         } else {
            // binary operators
            auto rhs = stack.back();
            stack.pop_back();
            auto lhs = stack.back();
            stack.pop_back();

            char c2 = ' ';
            if (token.str.size() > 1) {
               c2 = token.str[1];
            }

            switch(token.str[0]) {
            default:
               ossg << "UNKNOWN_OPERATION" << sep;
               throw std::runtime_error("ERROR: Operator error (2)");
               //printf("Operator error [%s]\n", token.str.c_str());
               //exit(0);
               throw std::runtime_error("ERROR: Operator error (2)");
               
               break;
            case '^':
               ossg << "XOR" << sep;
               //stack.push_back(static_cast<int>(pow(lhs, rhs)));
               stack.push_back(Token { Token::Type::Number, zero });
               break;
            case '*':
               ossg << "MUL" << sep;
               //stack.push_back(lhs * rhs);
               stack.push_back(Token { Token::Type::Number, zero });
               break;
            case '/':
               ossg << "DIV" << sep;
               //stack.push_back(lhs / rhs);
               stack.push_back(Token { Token::Type::Number, zero });
               break;
            case '+':
               ossg << "ADD" << sep;
               //stack.push_back(lhs + rhs);
               stack.push_back(Token { Token::Type::Number, zero });
               break;
            case '-':
               ossg << "SUB" << sep;
               //stack.push_back(lhs - rhs);
               stack.push_back(Token { Token::Type::Number, zero });
               break;

            case '%':
               ossg << "MOD" << sep;
               //stack.push_back(lhs / rhs);
               stack.push_back(Token { Token::Type::Number, zero });
               break;


         case '&':
            switch (c2) {
            case '&':   // &&
               ossg << "BAND" << sep;
               stack.push_back(Token { Token::Type::Number, zero });
               break;
            case ' ':    // just &
               ossg << "AND" << sep;
               stack.push_back(Token { Token::Type::Number, zero });
               break;
            default:
               ossg << "UNKNOWN_OPERATION" << sep;
               throw std::runtime_error("ERROR: Operator error (8)");
               break;
            }
            break;
         case '|':
            switch (c2) {
            case '|':   // ||
               //break;   || and | behave identically
            case ' ':    // just |
               ossg << "OR" << sep;
               stack.push_back(Token { Token::Type::Number, zero });
               break;
            default:
               ossg << "UNKNOWN_OPERATION" << sep;
               throw std::runtime_error("ERROR: Operator error (7)");
               break;
            }
            break;
         case '<':
            switch (c2) {
            case '<':   // <<
               ossg << "SHL" << sep;
               stack.push_back(Token { Token::Type::Number, zero });
               break;
            case '=':   // <=
               ossg << "LE" << sep;
               stack.push_back(Token { Token::Type::Number, zero });
               break;
            case ' ':    // <
               ossg << "LT" << sep;
               stack.push_back(Token { Token::Type::Number, zero });
               break;
            default:
               ossg << "UNKNOWN_OPERATION" << sep;
               throw std::runtime_error("ERROR: Operator error (6)");
               break;
            }
            break;
         case '>':
            switch (c2) {
            case '>':   // >>
               ossg << "SHR" << sep;
               stack.push_back(Token { Token::Type::Number, zero });
               break;
            case '=':   // >=
               ossg << "GE" << sep;
               stack.push_back(Token { Token::Type::Number, zero });
               break;
            case ' ':    // >
               ossg << "GT" << sep;
               stack.push_back(Token { Token::Type::Number, zero });
               break;
            default:
               ossg << "UNKNOWN_OPERATION" << sep;
               throw std::runtime_error("ERROR: Operator error (5)");
               break;
            }
            break;
         case '=':
            switch (c2) {
            case '=':   // ==
               ossg << "EQ" << sep;
               stack.push_back(Token { Token::Type::Number, zero });
               break;
            default:    // '='  (not supported) -- doing the Unknown token case
               ossg << "UNKNOWN_OPERATION" << sep;
               throw std::runtime_error("ERROR: Operator error (3)");
               break;
            }
            break;
         case '!':
            switch (c2) {
            case '=':   // !=
               ossg << "NE" << sep;
               stack.push_back(Token { Token::Type::Number, zero });
               break;
            default:
               ossg << "UNKNOWN_OPERATION" << sep;
               throw std::runtime_error("ERROR: Operator error (4)");
               break;
            }
            break;


               
               
            }
            //op = "Push " + std::to_string(lhs) + " " + token.str + " " + std::to_string(rhs);
            op = "Push something";
#ifdef DEBUG            
            std::cout << "ASM OPERATION: " << token.str << "    (garbage: " << lhs << " " << rhs << std::endl;
#endif
         }
      }
      break;

      default:
         //printf("Token error\n");
         //exit(0);
         throw std::runtime_error("ERROR: Token error");
      }
      debugReport(token, queue, stack, op);
   }

   return ossg.str();

   //return stack.back();
}

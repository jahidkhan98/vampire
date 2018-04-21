
/*
 * File TPTP.cpp.
 *
 * This file is part of the source code of the software program
 * Vampire. It is protected by applicable
 * copyright laws.
 *
 * This source code is distributed under the licence found here
 * https://vprover.github.io/license.html
 * and in the source directory
 *
 * In summary, you are allowed to use Vampire for non-commercial
 * purposes but not allowed to distribute, modify, copy, create derivatives,
 * or use in competitions. 
 * For other uses of Vampire please contact developers for a different
 * licence, which we will make an effort to provide. 
 */
/**
 * @file Parse/TPTP.cpp
 * Implements class TPTP for parsing TPTP files
 *
 * @since 08/04/2011 Manchester
 */

#include <fstream>

#include "Debug/Assertion.hpp"
#include "Debug/Tracer.hpp"

#include "Lib/Int.hpp"
#include "Lib/Environment.hpp"

#include "Kernel/Signature.hpp"
#include "Kernel/Inference.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/SortHelper.hpp"
#include "Kernel/Theory.hpp"

#include "Shell/FOOLElimAlt.hpp"
#include "Shell/Options.hpp"
#include "Shell/Statistics.hpp"
#include "Shell/DistinctGroupExpansion.hpp"

#include "Indexing/TermSharing.hpp"

#include "Parse/TPTP.hpp"

using namespace Lib;
using namespace Kernel;
using namespace Shell;
using namespace Parse;

#define DEBUG_SHOW_TOKENS 0
#define DEBUG_SHOW_UNITS 0
#define DEBUG_SOURCE 0

DHMap<unsigned, vstring> TPTP::_axiomNames;

/**
 * Create a parser, parse the input and return the parsed list of units.
 * @since 13/07/2011 Manchester
 */
UnitList* TPTP::parse(istream& input)
{
  Parse::TPTP parser(input);
  try{
    parser.parse();
  }
  catch (UserErrorException& exception) {
    vstring msg = exception.msg();
    throw ParseErrorException(msg,parser.lineNumber());
  }
  return parser.units();
}

/**
 * Initialise a lexer.
 * @since 27/07/2004 Torrevieja
 */
TPTP::TPTP(istream& in)
  : _containsConjecture(false),
    _allowedNames(0),
    _in(&in),
    _includeDirectory(""),
    _currentColor(COLOR_TRANSPARENT),
    _modelDefinition(false),
    _insideEqualityArgument(0),
    _unitSources(0),
    _filterReserved(false),
    _seenConjecture(false)
{
} // TPTP::TPTP

/**
 * The destructor, does nothing.
 * @since 09/07/2012 Manchester
 */
TPTP::~TPTP()
{
} // TPTP::~TPTP

/**
 * Read all tokens one by one 
 * @since 08/04/2011 Manchester
 */
void TPTP::parse()
{
  CALL("TPTP::parse");

  // bulding tokens one by one
  _gpos = 0;
  _cend = 0;
  _tend = 0;
  _lineNumber = 1;
  _states.push(UNIT_LIST);
  while (!_states.isEmpty()) {
    State s = _states.pop();
#ifdef DEBUG_SHOW_STATE
    cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl;
    cout << toString(s) << endl;
#endif
    switch (s) {
    case UNIT_LIST:
      unitList();
      break;
    case FOF:
      _isFof = true;
      fof(true);
      break;
    case THF:
      if(env.options->arityCheck()){ USER_ERROR("thf depends on arity_check being off");};
      _isThf = true;
    case TFF:
      _isFof = false;
      tff();
      break;
    case CNF:
      _isFof = true;
      fof(false);
      break;
    case FORMULA:
      formula();
      break;
    case FUN_APP:
      funApp();
      break;
    case ARGS:
      args();
      break;
    case TERM:
      term();
      break;
    case TERM_INFIX:
      termInfix();
      break;
    case END_TERM:
      endTerm();
      break;
    case END_ARGS:
      endArgs();
      break;
    case FORMULA_INFIX:
      formulaInfix();
      break;
    case END_EQ:
      endEquality();
      break;
    case MID_EQ:
      midEquality();
      break;
    case VAR_LIST:
      varList();
      break;
    case TAG:
      tag();
      break;
    case END_FOF:
      endFof();
      break;
    case SIMPLE_FORMULA:
      simpleFormula();
      break;
    case END_FORMULA:
      endFormula();
      break;
    case HOL_FUNCTION:
      holFunction();
      break;
    case END_HOL_FUNCTION:
      endHolFunction();
      break;
    case HOL_TERM:
      holTerm();
      break;
    case END_HOL_TERM:
      endHolTerm();
      break;
    case HOL_SUB_TERM:
      holSubTerm();
      break;
    case FORMULA_INSIDE_TERM:
      formulaInsideTerm();
      break;
    case END_FORMULA_INSIDE_TERM:
      endFormulaInsideTerm();
      break;
    case END_TERM_AS_FORMULA:
      endTermAsFormula();
      break;
    case INCLUDE:
      include();
      break;
    case TYPE:
      type();
      break;
    case SIMPLE_TYPE:
      simpleType();
      break;
    case END_TYPE:
      endType();
      break;
    case END_TFF:
      endTff();
      break;
    case UNBIND_VARIABLES:
      unbindVariables();
      break;
    case VAMPIRE:
      vampire();
      break;
    case END_ITE:
      endIte();
      break;
    case BINDING:
      binding();
      break;
    case END_BINDING:
      endBinding();
      break;
    case TUPLE_BINDING:
      if(!env.options->newCNF()){ USER_ERROR("Set --newcnf on if using tuples"); }
      tupleBinding();
      break;
    case END_TUPLE_BINDING:
      endTupleBinding();
      break;
    case END_LET:
      endLet();
      break;
    case END_THEORY_FUNCTION:
      endTheoryFunction();
      break;
    case END_TUPLE:
      if(!env.options->newCNF()){ USER_ERROR("Set --newcnf on if using tuples"); }
      endTuple();
      break;
    default:
#if VDEBUG
      throw ParseErrorException(((vstring)"Don't know how to process state ")+toString(s),_lineNumber);
#else
      throw ParseErrorException("Don't know how to process state ",_lineNumber);
#endif
    }
    if(_isThf){ env.signature->setHigherOrder(); }
#ifdef DEBUG_SHOW_STATE
    cout << "----------------------------------------" << endl;
    printStacks();
    cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << endl << endl;
#endif
  }
} // TPTP::parse()

/**
 * Return either the content or the string for this token
 * @since 11/04/2011 Manchester
 */
vstring TPTP::Token::toString() const
{
  vstring str = TPTP::toString(tag);
  return str == "" ? content : str;
} // Token::toString

/**
 * Return the string representation of this tag or "" is the representation
 * is not fixed (e.g. for T_NAME)
 * @since 11/04/2011 Manchester
 */
vstring TPTP::toString(Tag tag)
{
  switch (tag) {
  case T_EOF:
    return "<eof>";
  case T_LPAR:
    return "(";
  case T_RPAR:
    return ")";
  case T_LBRA:
    return "[";
  case T_RBRA:
    return "]";
  case T_COMMA:
    return ",";
  case T_COLON:
    return ":";
  case T_SEMICOLON:
    return ";";
  case T_NOT:
    return "~";
  case T_AND:
    return "&";
  case T_EQUAL:
    return "=";
  case T_NEQ:
    return "!=";
  case T_FORALL:
    return "!";
  case T_EXISTS:
    return "?";
  case T_PI:
    return "??";
  case T_SIGMA:
    return "!!";
  case T_IMPLY:
    return "=>";
  case T_XOR:
    return "<~>";
  case T_IFF:
    return "<=>";
  case T_REVERSE_IMP:
    return "<=";
  case T_DOT:
    return ".";
  case T_OR:
    return "|";
  case T_ASS:
    return ":=";
  case T_LAMBDA:
    return "^";
  case T_APP:
    return "@";
  case T_STAR:
    return "*";
  case T_UNION:
    return "+";
  case T_ARROW:
    return ">";
  case T_SUBTYPE:
    return "<<";
  case T_NOT_OR:
    return "~|";
  case T_NOT_AND:
    return "~&";
  case T_SEQUENT:
    return "-->";
  case T_THF_QUANT_ALL:
    return "!>";
  case T_THF_QUANT_SOME:
    return "?*";
  case T_APP_PLUS:
    return "@+";
  case T_APP_MINUS:
    return "@-";
  case T_TRUE:
    return "$true";
  case T_FALSE:
    return "$false";
  case T_TTYPE:
    return "$tType";
  case T_BOOL_TYPE:
    return "$o";
  case T_DEFAULT_TYPE:
    return "$i";
  case T_RATIONAL_TYPE:
    return "$rat";
  case T_REAL_TYPE:
    return "$real";
  case T_INTEGER_TYPE:
    return "$int";
  case T_TUPLE:
    return "$tuple";
  case T_THEORY_SORT:
    return "";
  case T_THEORY_FUNCTION:
    return "";
  case T_FOT:
    return "$fot";
  case T_FOF:
    return "$fof";
  case T_TFF:
    return "$tff";
  case T_THF:
    return "$thf";
  case T_ITE:
    return "$ite";
  case T_LET:
    return "$let";
  case T_NAME:
  case T_REAL:
  case T_RAT:
  case T_INT:
  case T_VAR:
  case T_DOLLARS:
  case T_STRING:
    return "";
#if VDEBUG
  default:
    ASS(false);
#endif
  }
} // toString(Tag)

/**
 * Read all tokens one by one
 * @since 08/04/2011 Manchester
 */
bool TPTP::readToken(Token& tok)
{
  CALL("TPTP::readToken");

  skipWhiteSpacesAndComments();
  tok.start = _gpos;
  switch (getChar(0)) {
  case 0:
    tok.tag = T_EOF;
    return false;
  case 'a':
  case 'b':
  case 'c':
  case 'd':
  case 'e':
  case 'f':
  case 'g':
  case 'h':
  case 'i':
  case 'j':
  case 'k':
  case 'l':
  case 'm':
  case 'n':
  case 'o':
  case 'p':
  case 'q':
  case 'r':
  case 's':
  case 't':
  case 'u':
  case 'v':
  case 'w':
  case 'x':
  case 'y':
  case 'z':
    tok.tag = T_NAME;
    readName(tok);
    return true;
  case '$':
    readReserved(tok);
    return true;
  case 'A':
  case 'B':
  case 'C':
  case 'D':
  case 'E':
  case 'F':
  case 'G':
  case 'H':
  case 'I':
  case 'J':
  case 'K':
  case 'L':
  case 'M':
  case 'N':
  case 'O':
  case 'P':
  case 'Q':
  case 'R':
  case 'S':
  case 'T':
  case 'U':
  case 'V':
  case 'W':
  case 'X':
  case 'Y':
  case 'Z':
  case '_':
    tok.tag = T_VAR;
    readName(tok);
    return true;
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    tok.tag = readNumber(tok);
    return true;
  case '"':
    tok.tag = T_STRING;
    readString(tok);
    return true;
  case '\'':
    tok.tag = T_NAME;
    readAtom(tok);
    return true;
  case '(':
    tok.tag = T_LPAR;
    resetChars();
    return true;
  case ')':
    tok.tag = T_RPAR;
    resetChars();
    return true;
  case '[':
    tok.tag = T_LBRA;
    resetChars();
    return true;
  case ']':
    tok.tag = T_RBRA;
    resetChars();
    return true;
  case ',':
    tok.tag = T_COMMA;
    resetChars();
    return true;
  case ':':
    if (getChar(1) == '=') {
      tok.tag = T_ASS;
      resetChars();
      return true;
    }
    tok.tag = T_COLON;
    shiftChars(1);
    return true;
  case ';':
    tok.tag = T_SEMICOLON;
    resetChars();
    return true;
  case '~':
    if (getChar(1) == '&') {
      tok.tag = T_NOT_AND;
      resetChars();
      return true;
    }
    if (getChar(1) == '|') {
      tok.tag = T_NOT_OR;
      resetChars();
      return true;
    }
    tok.tag = T_NOT;
    shiftChars(1);
    return true;
  case '=':
    if (getChar(1) == '>') {
      tok.tag = T_IMPLY;
      resetChars();
      return true;
    }
    tok.tag = T_EQUAL;
    shiftChars(1);
    return true;
  case '&':
    tok.tag = T_AND;
    resetChars();
    return true;
  case '^':
    tok.tag = T_LAMBDA;
    resetChars();
    return true;
  case '@':
    if (getChar(1) == '+') {
      tok.tag = T_APP_PLUS;
      resetChars();
      return true;
    }
    if (getChar(1) == '-') {
      tok.tag = T_APP_MINUS;
      resetChars();
      return true;
    }
    tok.tag = T_APP;
    shiftChars(1);
    return true;
  case '*':
    tok.tag = T_STAR;
    resetChars();
    return true;
  case '>':
    tok.tag = T_ARROW;
    resetChars();
    return true;
  case '!':
    if (getChar(1) == '=') {
      tok.tag = T_NEQ;
      resetChars();
      return true;
    }
    if (getChar(1) == '>') {
      tok.tag = T_THF_QUANT_ALL;
      resetChars();
      return true;
    }
    if (getChar(1) == '!') {
      tok.tag = T_SIGMA;
      resetChars();
      return true;
    }
    tok.tag = T_FORALL;
    shiftChars(1);
    return true;
  case '?':
    if (getChar(1) == '?') {
      tok.tag = T_PI;
      resetChars();
      return true;
    }
    if (getChar(1) == '*') {
      tok.tag = T_THF_QUANT_SOME;
      resetChars();
      return true;
    }
    tok.tag = T_EXISTS;
    shiftChars(1);
    return true;
  case '<':
    if (getChar(1) == '<') {
      tok.tag = T_SUBTYPE;
      resetChars();
      return true;
    }
    if (getChar(1) == '~' && getChar(2) == '>') {
      tok.tag = T_XOR;
      resetChars();
      return true;
    }
    if (getChar(1) != '=') {
      PARSE_ERROR("unrecognized symbol",_gpos);
    }
    if (getChar(2) == '>') {
      tok.tag = T_IFF;
      resetChars();
      return true;
    }
    tok.tag = T_REVERSE_IMP;
    shiftChars(2);
    return true;
  case '.':
    tok.tag = T_DOT;
    resetChars();
    return true;
  case '|':
    tok.tag = T_OR;
    resetChars();
    return true;
  case '-':
    if (getChar(1) == '-' && getChar(2) == '>') {
      tok.tag = T_SEQUENT;
      resetChars();
      return true;
    }
    tok.tag = readNumber(tok);
    return true;
  case '+':
    if (getChar(1) < '0' || getChar(1) > '9') {
      tok.tag = T_UNION;
      shiftChars(1);
      return true;
    }
    tok.tag = readNumber(tok);
    return true;
  default:
    PARSE_ERROR("Bad character",_gpos);
  }
} // TPTP::readToken()

/**
 * Skip all white spaces and comments in the input file
 * @since 08/04/2011 Manchester
 */
void TPTP::skipWhiteSpacesAndComments()
{
  CALL("TPTP::skipWhiteSpacesAndComments");

  for (;;) {
    switch (getChar(0)) {
    case 0: // end-of-file
      return;

    case '\n':
    case '\r':
      _lineNumber++;
    case ' ':
    case '\t':
    case '\f':
      resetChars();
      break;

    case '%': // end-of-line comment
    resetChars();
    for (;;) {
      int c = getChar(0);
      if (c == 0) {
    return;
      }
      resetChars();
      if (c == '\n') {
        _lineNumber++;
    break;
      }
    }
    break;

    case '/': // potential comment
      if (getChar(1) != '*') {
    return;
      }
      resetChars();
      // search for the end of this comment
      for (;;) {
    int c = getChar(0);
        if( c == '\n' || c == '\r'){ _lineNumber++; }
    if (!c) {
      return;
    }
    resetChars();
    if (c != '*') {
      continue;
    }
    // c == '*'
    c = getChar(0);
    resetChars();
    if (c != '/') {
      continue;
    }
    break;
      }
      break;

    // skip to the end of comment
    default:
      return;
    }
  }
} // TPTP::skipWhiteSpacesAndComments

/**
 * Read the name
 * @since 08/04/2011 Manchester
 */
void TPTP::readName(Token& tok)
{
  CALL("TPTP::readName");
  for (int n = 1;;n++) {
    switch (getChar(n)) {
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
    case 'G':
    case 'H':
    case 'I':
    case 'J':
    case 'K':
    case 'L':
    case 'M':
    case 'N':
    case 'O':
    case 'P':
    case 'Q':
    case 'R':
    case 'S':
    case 'T':
    case 'U':
    case 'V':
    case 'W':
    case 'X':
    case 'Y':
    case 'Z':
    case '_':
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
    case 'g':
    case 'h':
    case 'i':
    case 'j':
    case 'k':
    case 'l':
    case 'm':
    case 'n':
    case 'o':
    case 'p':
    case 'q':
    case 'r':
    case 's':
    case 't':
    case 'u':
    case 'v':
    case 'w':
    case 'x':
    case 'y':
    case 'z':
    case '$':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      break;
    default:
      ASS(_chars.content()[0] != '$');
      tok.content.assign(_chars.content(),n);
      shiftChars(n);
      return;
    }
  }
} // readName

/**
 * Read a reserved name (starting with a $)
 * @since 10/07/2011 Manchester
 */
void TPTP::readReserved(Token& tok)
{
  CALL("TPTP::readReserved");

  int n = 1;
  for (;;n++) {
    switch (getChar(n)) {
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
    case 'G':
    case 'H':
    case 'I':
    case 'J':
    case 'K':
    case 'L':
    case 'M':
    case 'N':
    case 'O':
    case 'P':
    case 'Q':
    case 'R':
    case 'S':
    case 'T':
    case 'U':
    case 'V':
    case 'W':
    case 'X':
    case 'Y':
    case 'Z':
    case '_':
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
    case 'g':
    case 'h':
    case 'i':
    case 'j':
    case 'k':
    case 'l':
    case 'm':
    case 'n':
    case 'o':
    case 'p':
    case 'q':
    case 'r':
    case 's':
    case 't':
    case 'u':
    case 'v':
    case 'w':
    case 'x':
    case 'y':
    case 'z':
    case '$':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      break;
    default:
      tok.content.assign(_chars.content(),n);
      //shiftChars(n);
      goto out;
    }
  }
 out:
  if (tok.content == "$true") {
    tok.tag = T_TRUE;
  }
  else if (tok.content == "$false") {
    tok.tag = T_FALSE;
  }
  else if (tok.content == "$ite_f" || tok.content == "$ite_t" || tok.content == "$ite") {
    tok.tag = T_ITE;
    // $ite_t and $ite_f are left for compatibility, $ite is a generalisation of them
    tok.content = "$ite";
  }
  else if (tok.content == "$let_tt" || tok.content == "$let_tf" || tok.content == "$let_ft" || tok.content == "$let_ff" || tok.content == "$let") {
    tok.tag = T_LET;
    // all tokens of the form $let_XY are left for compatibility, $let is a generalisation of them
    tok.content = "$let";
  }
  else if (tok.content == "$tType") {
    tok.tag = T_TTYPE;
  }
  else if (tok.content == "$o" || tok.content == "$oType") {
    tok.tag = T_BOOL_TYPE;
  }
  else if (tok.content == "$i" || tok.content == "$iType") {
    tok.tag = T_DEFAULT_TYPE;
  }
  else if (tok.content == "$int") {
    tok.tag = T_INTEGER_TYPE;
  }
  else if (tok.content == "$rat") {
    tok.tag = T_RATIONAL_TYPE;
  }
  else if (tok.content == "$real") {
    tok.tag = T_REAL_TYPE;
  }
  else if (tok.content == "$tuple") {
      tok.tag = T_TUPLE;
  }
  else if (isTheoryFunction(tok.content)) {
    tok.tag = T_THEORY_FUNCTION;
  }
  else if (isTheorySort(tok.content)) {
    tok.tag = T_THEORY_SORT;
  }
  else if (tok.content == "$fot") {
    tok.tag = T_FOT;
  }
  else if (tok.content == "$fof") {
    tok.tag = T_FOF;
  }
  else if (tok.content == "$tff") {
    tok.tag = T_TFF;
  }
  else if (tok.content == "$thf") {
    tok.tag = T_THF;
  }
  else if (tok.content.substr(0,2) == "$$" && !_filterReserved) {
      tok.tag = T_DOLLARS;
  }
  else {
      
      // If _filterReserved is on then filter "$" from content
      if(_filterReserved){
          unsigned c=0;
          for(;;c++){ if(getChar(c)!='$') break;}
          shiftChars(c);
          n=n-c;
          tok.content.assign(_chars.content(),n);
      }
      
      tok.tag = T_NAME;
  }
  // Moved from above so that _filterReserved works
  shiftChars(n);
} // readReserved

/**
 * Read a string
 * @since 08/04/2011 Manchester
 */
void TPTP::readString(Token& tok)
{
  CALL("TPTP::readString");
  for (int n = 1;;n++) {
    int c = getChar(n);
    if (!c) {
      PARSE_ERROR("non-terminated string",_gpos);
    }
    if (c == '\\') { // escape
      c = getChar(++n);
      if (!c) {
    PARSE_ERROR("non-terminated string",_gpos);
      }
      continue;
    }
    if (c == '"') {
      tok.content.assign(_chars.content()+1,n-1);
      resetChars();
      return;
    }
  }
} // readString

/**
 * Read a quoted atom
 * @since 08/04/2011 Manchester
 */
void TPTP::readAtom(Token& tok)
{
  CALL("TPTP::readAtom");

  for (int n = 1;;n++) {
    int c = getChar(n);
    if (!c) {
      PARSE_ERROR("non-terminated quoted atom",_gpos);
    }
    if (c == '\\') { // escape
      c = getChar(++n);
      if (!c) {
    PARSE_ERROR("non-terminated quoted atom",_gpos);
      }
      continue;
    }
    if (c == '\'') {
      tok.content.assign(_chars.content()+1,n-1);
      resetChars();
      return;
    }
  }
} // readAtom

TPTP::ParseErrorException::ParseErrorException(vstring message,int pos, unsigned ln) : _ln(ln)
{
  _message = message + " at position " + Int::toString(pos);
} // TPTP::ParseErrorException::ParseErrorException

TPTP::ParseErrorException::ParseErrorException(vstring message,Token& tok, unsigned ln) : _ln(ln)
{
  _message = message + " at position " + Int::toString(tok.start) + " (text: " + tok.toString() + ')'; 
} // TPTP::ParseErrorException::ParseErrorException

/**
 * Exception printing a message. Currently computing a position is simplified
 * @since 08/04/2011 Manchester
 */
void TPTP::ParseErrorException::cry(ostream& str)
{
  str << "Parsing Error on line " << _ln << "\n";
  str << _message << "\n";
}

/**
 * Read a number
 * @since 08/04/2011 Manchester
 */
TPTP::Tag TPTP::readNumber(Token& tok)
{
  CALL("TPTP::readNumber");

  // skip the sign
  int c = getChar(0);
  ASS(c);
  int pos = decimal((c == '+' || c == '-') ? 1 : 0);
  switch (getChar(pos)) {
  case '/':
    pos = positiveDecimal(pos+1);
    tok.content.assign(_chars.content(),pos);
    shiftChars(pos);
    return T_RAT;
  case 'E':
  case 'e':
    {
      char c = getChar(pos+1);
      pos = decimal((c == '+' || c == '-') ? pos+2 : pos+1);
      tok.content.assign(_chars.content(),pos);
      shiftChars(pos);
    }
    return T_REAL;
  case '.':
    {
      int p = pos;
      do {
        c = getChar(++pos);
      }
      while (c >= '0' && c <= '9');
      if (pos == p+1) {
        // something like 12.
        PARSE_ERROR("wrong number format",_gpos);
      }
      c = getChar(pos);
      if (c == 'e' || c == 'E') {
    c = getChar(pos+1);
    pos = decimal((c == '+' || c == '-') ? pos+2 : pos+1);
      }
      tok.content.assign(_chars.content(),pos);
      shiftChars(pos);
    }
    return T_REAL;
  default:
    tok.content.assign(_chars.content(),pos);
    shiftChars(pos);
    return T_INT;
  }
} // readNumber

/**
 * Read a decimal starting at position pos (see the TPTP grammar),
 * return the position after the last character in the decimal.
 * @since 08/04/2011 Manchester
 */
int TPTP::decimal(int pos)
{
  CALL("TPTP::decimal");

  switch (getChar(pos)) {
  case '0':
    return pos+1;
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    break;
  default:
    ASSERTION_VIOLATION_REP(getChar(pos));
    PARSE_ERROR("wrong number format",_gpos);
  }

  int c;
  do {
    c = getChar(++pos);
  }
  while (c >= '0' && c <= '9');
  return pos;
} // decimal

/**
 * Read a positive decimal starting at position pos (see the TPTP grammar),
 * return the position after the last character in the decimal.
 * @since 08/04/2011 Manchester
 */
int TPTP::positiveDecimal(int pos)
{
  CALL("TPTP::positiveDecimal");

  switch (getChar(pos)) {
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    break;
  default:
    PARSE_ERROR("wrong number format",_gpos);
  }

  int c;
  do {
    c = getChar(++pos);
  }
  while (c >= '0' && c <= '9');
  return pos;
} // positiveDecimal

/**
 * Process unit list declaration. If end of file is reached, terminates. Otherwise,
 * pushes on the state state UNIT_LIST and one of CNF, FOF, VAMPIRE
 * @since 10/04/2011 Manchester
 */
void TPTP::unitList()
{
  CALL("TPTP::unitList");
  if (env.timeLimitReached()) {
    // empty states to avoid infinite loop
    while (!_states.isEmpty()) {
      _states.pop();
    }
    return;
  }

  Token& tok = getTok(0);
  if (tok.tag == T_EOF) {
    resetToks();
    if (_inputs.isEmpty()) {
      return;
    }
    resetChars();
    {
      BYPASSING_ALLOCATOR; // ifstream was allocated by "system new"
      delete _in;
    }
    _in = _inputs.pop();
    _includeDirectory = _includeDirectories.pop();
    delete _allowedNames;
    _allowedNames = _allowedNamesStack.pop();
    _states.push(UNIT_LIST);
    return;
  }
  if (tok.tag != T_NAME) {
    PARSE_ERROR("cnf(), fof(), vampire() or include() expected",tok);
  }
  vstring name(tok.content);
  _states.push(UNIT_LIST);
  if (name == "cnf") {
    _states.push(CNF);
    resetToks();
    return;
  }
  if (name == "fof") {
      _states.push(FOF);
    resetToks();
    return;
  }
  if (name == "tff") {
    _states.push(TFF);
    resetToks();
    return;
  }
  if (name == "thf") {
    _states.push(THF);
    resetToks();
    return;
  }
  if (name == "vampire") {
    _states.push(VAMPIRE);
    resetToks();
    return;
  }
  if (name == "include") {
    _states.push(INCLUDE);
    resetToks();
    return;
  }
  PARSE_ERROR("cnf(), fof(), vampire() or include() expected",tok);
}

/**
 * Process fof() or cnf() declaration. Does the following:
 * <ol>
 *  <li>add 0 to _formulas</li>
 *  <li>save the input type to _lastInputType</li>
 *  <li>add unit name to _strings</li>
 *  <li>add to _states END_FOF,FORMULA</li>
 *  <li>adds to _bools true, if fof and false, if cnf</li>
 * </ol>
 * @since 10/04/2011 Manchester
 */
void TPTP::fof(bool fo)
{
  CALL("TPTP::fof");

  _bools.push(fo);
  consumeToken(T_LPAR);
  // save the name of this unit
  Token& tok = getTok(0);
  switch(tok.tag) {
  case T_NAME:
    _strings.push(tok.content);
    resetToks();
    break;
  case T_INT:
    _strings.push(tok.content);
    resetToks();
    break;
  default:
    PARSE_ERROR("Unit name expected",tok);
  }

  consumeToken(T_COMMA);
  tok = getTok(0);
  int start = tok.start;
  vstring tp = name();
  
  _isQuestion = false;
  if(_modelDefinition){
    _lastInputType = Unit::MODEL_DEFINITION;
  }
  else if (tp == "axiom" || tp == "plain") {
    _lastInputType = Unit::AXIOM;
  }
  else if(tp == "extensionality"){
    // this will be transformed to just AXIOM after clausification
    _lastInputType = Unit::EXTENSIONALITY_AXIOM;
  }
  else if (tp == "definition") {
    _lastInputType = Unit::AXIOM;
  }
  else if (tp == "conjecture") {
    _containsConjecture = true;
    _lastInputType = Unit::CONJECTURE;
  }
  else if (tp == "question") {
    _isQuestion = true;
    _containsConjecture = true;
    _lastInputType = Unit::CONJECTURE;
  }
  else if (tp == "negated_conjecture") {
    _lastInputType = Unit::NEGATED_CONJECTURE;
  }
  else if (tp == "hypothesis" || tp == "theorem" || tp == "lemma") {
    _lastInputType = Unit::ASSUMPTION;
  }
  else if (tp == "assumption" || tp == "unknown") {
    // assumptions are not used, so we assign them a non-existing input type and then
    // not include them in the input
    _lastInputType = -1;
  }
  else if (tp == "claim") {
    _lastInputType = Unit::CLAIM;
  }
  else {
    PARSE_ERROR((vstring)"unit type, such as axiom or definition expected but " + tp + " found",
            start);
  }
  consumeToken(T_COMMA);
  _states.push(END_FOF);
  _states.push(FORMULA);
} // fof()

/**
 * Process fof() or cnf() declaration. Does the following:
 * <ol>
 *  <li>add 0 to _formulas</li>
 *  <li>save the input type to _lastInputType</li>
 *  <li>add unit name to _strings</li>
 *  <li>add to _states END_FOF,FORMULA</li>
 *  <li>adds to _bools true, if fof and false, if cnf</li>
 * </ol>
 * @since 10/04/2011 Manchester
 * @author Andrei Voronkov
 */
void TPTP::tff()
{
  CALL("TPTP::tff");

  consumeToken(T_LPAR);
  // save the name of this unit
  Token& tok = getTok(0);
  switch(tok.tag) {
  case T_NAME:
  case T_INT:
    _strings.push(tok.content);
    resetToks();
    break;
  default:
    PARSE_ERROR("Unit name expected",tok);
  }

  consumeToken(T_COMMA);
  tok = getTok(0);
  int start = tok.start;
  vstring tp = name();
  if (tp == "type") {
    // Read a TPTP type declaration. These declarations are ambiguous: they can
    // either be new type declarations, as in tff(1,type,(t: $ttype)) or sort
    // declarations: tff(2,type,(c:t)). What exactly they represent will be known
    // when $ttype is expected.
    consumeToken(T_COMMA);
    // TPTP syntax allows for an arbitrary number of parentheses around a type
    // declaration
    int lpars = 0;
    for (;;) {
      tok = getTok(0);
      if (tok.tag != T_LPAR) {
    break;
      }
      lpars++;
      resetToks();
    }
    vstring nm = name();
    consumeToken(T_COLON);
    tok = getTok(0);
    if (tok.tag == T_TTYPE) {
      // now we know that this is a new type declaration
      bool added;
      env.sorts->addSort(nm,added,false);
      if (!added) {
        PARSE_ERROR("Sort name must be unique",tok);
      }
      resetToks();
      while (lpars--) {
        consumeToken(T_RPAR);
      }
      consumeToken(T_RPAR);
      consumeToken(T_DOT);
      return;
    }else if(_isThf){
      //We now know that this must be an interpreted costant
      //either a HOL constant or a normal consant.
      Signature::Symbol* symbol;
      bool added; 
      _gpos = 0;
      Stack<unsigned> sorts = readHOLSort();
      
      unsigned returnSort = sorts.pop();
      
      unsigned arity = sorts.size();
      unsigned fun = env.signature->addFunction(nm,arity,added,false,1);
      if (!added) {
        USER_ERROR("Function symbol type is declared after its use: " + nm);
      }
      symbol = env.signature->getFunction(fun);
      symbol->setType(OperatorType::getFunctionType(arity, sorts.begin(), returnSort));

      
      while (lpars--) {
        consumeToken(T_RPAR);
      }
      consumeToken(T_RPAR);
      consumeToken(T_DOT);
      return;
    }
    // the matching number of rpars will be read
    _ints.push(lpars);
    // remember type name
    _strings.push(nm);
    _states.push(END_TFF);
    _states.push(TYPE);
    return;
  }

  _bools.push(true); // to denote that it is an FOF formula
  _isQuestion = false;
  if (tp == "axiom" || tp == "plain") {
    _lastInputType = Unit::AXIOM;
  }
  else if (tp == "extensionality"){
    // this will be transformed to just AXIOM after clausification
    _lastInputType = Unit::EXTENSIONALITY_AXIOM;
  }
  else if (tp == "definition") {
    _lastInputType = Unit::AXIOM;
  }
  else if (tp == "conjecture") {
    _containsConjecture = true;
    _lastInputType = Unit::CONJECTURE;
  }
  else if (tp == "question") {
    _isQuestion = true;
    _containsConjecture = true;
    _lastInputType = Unit::CONJECTURE;
  }
  else if (tp == "negated_conjecture") {
    _lastInputType = Unit::NEGATED_CONJECTURE;
  }
  else if (tp == "hypothesis" || tp == "theorem" || tp == "lemma") {
    _lastInputType = Unit::ASSUMPTION;
  }
  else if (tp == "assumption" || tp == "unknown") {
    // assumptions are not used, so we assign them a non-existing input type and then
    // not include them in the input
    _lastInputType = -1;
  }
  else if (tp == "claim") {
    _lastInputType = Unit::CLAIM;
  }
  else {
    PARSE_ERROR((vstring)"unit type, such as axiom or definition expected but " + tp + " found",
            start);
  }

  consumeToken(T_COMMA);
  _states.push(END_FOF);
  _states.push(FORMULA);
} // tff()


/**
  * Reads a function of a higher order logic. This can either be of type $o
    in which case it is a formula, or any other type.
  * @since 08/11/2017
  * @author Ahmed Bhayat
  */

void TPTP::holFunction()
{
  CALL("TPTP::holFunction");
  Token tok = getTok(0);

  switch (tok.tag) {
  case T_NOT:
    resetToks();
    _connectives.push(NOT);
    _states.push(HOL_FUNCTION);
    return;

  case T_LAMBDA:
  case T_FORALL:
  case T_EXISTS:
   // _states.push(UNBIND_VARIABLES);
    resetToks();
    consumeToken(T_LBRA);
    _connectives.push(tok.tag == T_FORALL ? FORALL : (tok.tag == T_EXISTS ? EXISTS : LAMBDA));
    _states.push(END_HOL_FUNCTION);
    _states.push(HOL_FUNCTION);
    addTagState(T_COLON);
    addTagState(T_RBRA);
    _states.push(VAR_LIST);
    _lastBinder = tok.tag == T_FORALL ? UNIV : (tok.tag == T_EXISTS ? EXIS : LAMB);
    return;

  case T_LPAR:
    resetToks();
    addTagState(T_RPAR);
    _connectives.push(-1);
    _states.push(END_HOL_FUNCTION);
    _states.push(HOL_FUNCTION);
    return;

  case T_STRING:
  case T_INT:
  case T_RAT:
  case T_REAL://TODO update for HOL - AYB
    _states.push(END_EQ);
    _states.push(TERM);
    _states.push(MID_EQ);
    _states.push(TERM);
    return;
  case T_TRUE:
    resetToks();
    _formulas.push(new Formula(true));
    _lastPushed = FORM;
    return;
  case T_FALSE:
    resetToks();
    _formulas.push(new Formula(false));
    _lastPushed = FORM;
    return;
  case T_NAME:
  case T_VAR:
  case T_ITE:
  case T_THEORY_FUNCTION:
  case T_LET:
  case T_LBRA:
    _states.push(END_HOL_TERM);
    _states.push(HOL_TERM);
    return;
  //AYB ADDED, TO BE MODIFIED
  default:
    PARSE_ERROR("formula or term expected",tok);
  }
}

/**
  * Reads a term of a higher order logic. Either a higher-order constant or a variable
  * @since 08/11/2017
  * @author Ahmed Bhayat
  */

void TPTP::holTerm()
{
  CALL("TPTP::holTerm");
  Token tok = getTok(0);

  _strings.push(tok.content);

  switch (tok.tag) {//TODO update this for HOL -AYB
  
    case T_VAR:{//This requires updating for variable heads.
      resetToks();
      _ints.push(-1); //var
      _argsSoFar.push(0);
      _states.push(HOL_SUB_TERM);
      return;
    }
    case T_LAMBDA:{
      _states.push(HOL_FUNCTION);
      return;
    }
    case T_NAME:{
      resetToks();
      bool added;
      unsigned funcNum = env.signature->addFunction(tok.content, 0 , added, false, 1); //dummy arity
      ASS_REP(!added, tok.content);
      unsigned arity = env.signature->functionArity(funcNum);
      _ints.push(arity); // arity
      _argsSoFar.push(0);
      _states.push(HOL_SUB_TERM);
      return;
    }

    default:
      PARSE_ERROR("unexpected token", tok);
  }
}

/**
  * Adds a variable or a HOL constant to the stack of terms
  * @since 08/11/2017
  * @author Ahmed Bhayat
  */
void TPTP::endHolTerm()
{
  CALL("TPTP::endHolTerm");
  
  vstring name = _strings.pop();
  int arity = _ints.pop();

  if(arity == -1){ //that was a variable
    dealWithVar(name, _argsSoFar.pop());
    return;
  }

  bool added;
  unsigned funcNum = env.signature->addFunction(name, 0 , added, false, 1);
  ASS_REP(!added, name);
  OperatorType* type = env.signature->getFunction(funcNum)->fnType();
  
  _termLists.push(etaExpand(type, name, arity, _argsSoFar.pop(), false));
  _lastPushed = TM;
}

void TPTP::holSubTerm(){
  CALL("TPTP::holSubTerm");
  
  Token tok = getTok(0);
  if(tok.tag != T_APP){
    _gpos = 0; 
    return;
  }
  resetToks();
  tok = getTok(0);
  
  _argsSoFar.push(_argsSoFar.pop() + 1);

  switch (tok.tag) {//TODO update this for HOL -AYB
    case T_LPAR:
      resetToks();
      _states.push(HOL_SUB_TERM);
      addTagState(T_RPAR);
      _states.push(END_HOL_TERM);
      _states.push(HOL_TERM);
      return;
    case T_VAR:{
      resetToks();
      dealWithVar(tok.content, 0);
      _states.push(HOL_SUB_TERM);
      return;
    }
    case T_LAMBDA:{
      _states.push(HOL_FUNCTION);
      return;
    }
    case T_NAME:{
      resetToks();
      vstring funcName = tok.content;
      bool added;
      unsigned funcNum = env.signature->addFunction(funcName, 0 , added, false, 1); //dummy arity!
      ASS_REP(!added, funcName);
      unsigned arity = env.signature->functionArity(funcNum);
      OperatorType* type = env.signature->getFunction(funcNum)->fnType();
      
      _termLists.push(etaExpand(type, funcName, arity, 0, false));
      _lastPushed = TM; 
      _states.push(HOL_SUB_TERM);
      return;
    }
    default:
      ASSERTION_VIOLATION;
  }    
  
}

/**
  * Checks whether a variable is a lambda var, higher-order or first-order and 
  * then actions as appropriate.
  * @since 19/03/2018
  * @author Ahmed Bhayat
  */
void TPTP::dealWithVar(vstring name, unsigned argNum){
  CALL("TPTP::dealWithVar");
  
  unsigned var = (unsigned)_vars.insert(name);
  BindList* binders;
  TypeList* types;
  ALWAYS(_varBinders.find(var, binders)); //assuming that all terms are closed.
  ALWAYS(_varTypes.find(var, types));
  OperatorType* varType = types->head();
  if(binders->head() == LAMB){ //DuBruijn index
    vstring indexName = nameToIndex(var); //finding du-bruijn index equivalent to var
    unsigned index = FOOLElimAlt::addDuBruijnIndex(indexName, varType);
    if(varType->arity() == 0){ //constant  
      _termLists.push(TermList(Term::createConstant(index)));
    }else{ //higher-order DU Bruijn index
      _termLists.push(etaExpand(varType, indexName,  varType->arity(), argNum, true));  
    }
  }else{ //existentially or universally quantified var.
    if(varType->arity() == 0){//Classic var (phew!)
      _termLists.push(TermList(var, false));
    }else{
      FuncList* functors;
      ALWAYS(_varFunctors.find(var, functors))
      _termLists.push(etaExpand(varType, name,  varType->arity(), argNum, false, functors->head()));  
    }
  }
  _lastPushed = TM;
}

/**
  * Recursively eta expands a function with name @name, type @type and arity @arity
  * @since 19/03/2018
  * @author Ahmed Bhayat
  */
TermList TPTP::etaExpand(OperatorType* type, vstring name, unsigned arity, unsigned argsOnStack, bool isIndex, unsigned hoVarFunc){
  CALL("TPTP::etaExpand");

  unsigned count = argsOnStack;
   
  if(arity - argsOnStack > 0){
    if(isIndex){
      name = FOOLElimAlt::lift(name, arity - argsOnStack); //head symbol is a Du Bruijn index that requires lifting by arity - argsonstack
      FOOLElimAlt::addDuBruijnIndex(name, type);
    }
    lift(argsOnStack, arity - argsOnStack);//recursively lift the arguments already on the stack
  }
   
  for(int i = arity; i > argsOnStack; i--){
    unsigned sort = type->arg(count);
    OperatorType* subType = toType(sort);
    if(!env.sorts->isOfStructuredSort(sort, Sorts::StructuredSort::FUNCTION)){
      unsigned index = FOOLElimAlt::addDuBruijnIndex(Int::toString(i - argsOnStack) + "_" + Int::toString(sort), subType);
      _termLists.push(TermList(Term::createConstant(index)));
    }else{
      unsigned subArity = subType->arity();
      vstring name2 = Int::toString(i - argsOnStack) + "_" + Int::toString(sort);
      _termLists.push(etaExpand(subType, name2, subArity ,0, true));
    }
    count++;
  }
  TermList expandedTerm;
  if(!hoVarFunc){
    expandedTerm = createFunctionApplication(name, arity);
  }else{
    expandedTerm = createHigherOrderVarApp(hoVarFunc, type);
  }
  count = arity - 1;
  for( unsigned i = argsOnStack; i < arity; i++){
    expandedTerm = abstract(expandedTerm, type->arg(count));
    count--;
  }  
  
  return expandedTerm;
}


/**
  * Recursively lifts all Du Bruijn indices in top @argNum terms on _termLists by @value
  * @since 19/03/2018
  * @author Ahmed Bhayat
  */
void TPTP::lift(unsigned argNum, unsigned value){
  CALL("TPTP::lift(unsigned, unsigned)");

  for(unsigned i = 1; i <= argNum; i ++){ 
    _termLists[_termLists.size() - i] = FOOLElimAlt::lift(_termLists[_termLists.size() - i], value, 0);
  }
  
}

/**
  * Converts int @var to equivalent Du Bruijn index and returns string representation
  * @since 19/03/2018
  * @author Ahmed Bhayat
  */
vstring TPTP::nameToIndex(int var){
  CALL("TPTP::nameToIndex");
  
  unsigned count = _lambdaVars.size() - 1; 
  while( count >= 0 ){
     if( _lambdaVars[count] == var ){
       SortList* sorts;
       ALWAYS(_variableSorts.find(var, sorts));
       vstring name = Int::toString(_lambdaVars.size() - count) + "_" + Int::toString(sorts->head());
       return name;
     }
     count--;
  }  
  ASSERTION_VIOLATION;
}

/**
  * Creates an abstraction of @term with sort sort -> sortOf(term)
  * @since 19/03/2018
  * @author Ahmed Bhayat
  */
TermList TPTP::abstract(TermList term, unsigned sort){
  
   CALL("TPTP::abstract");

   unsigned termSort = sortOf(term);
   unsigned lamSort = env.sorts->addFunctionSort(sort, termSort);

   Stack<unsigned> sorts;
   sorts.push(termSort);
   OperatorType* type = OperatorType::getFunctionType(1, sorts.begin(), lamSort);

   bool added;
   unsigned fun = env.signature->addFunction("lam_" + Int::toString(lamSort),1,added);
   if(added){//first time constant added. Set type
     Signature::Symbol* symbol = env.signature->getFunction(fun);  
     symbol->setType(type);
     symbol->markLambda();   
   } 

   return TermList(Term::create1(fun, term));   
}

OperatorType* TPTP::toType(unsigned sort){
  CALL("TPTP::toType");

  Stack<unsigned> sorts;  
  if(env.sorts->isOfStructuredSort(sort, Sorts::StructuredSort::FUNCTION)){
    while(env.sorts->isOfStructuredSort(sort, Sorts::StructuredSort::FUNCTION)){
      sorts.push(env.sorts->getFuncSort(sort)->getDomainSort());
      sort = env.sorts->getFuncSort(sort)->getRangeSort();
    }
    return OperatorType::getFunctionType(sorts.size(), sorts.begin(), sort);

  }
  return OperatorType::getConstantsType(sort);
}

/**
  * Process the end of a HOL function.
  * If the main connective is any that operates on formulas (!, ?, &, |, -2...) and
  * The top term on the termlist is not of type $o and error is raised. Otherwise, 
  * @since 05/11/2017 Manchester
  * @author Ahmed Bhayat
  */

void TPTP::endHolFunction()
{
  CALL("TPTP::endHolFunction");

  int con = _connectives.pop();

  if (con == -2){
      if(_termLists.size() == 1){
       endTermAsFormula();
      }
      return;
  }  
  
  if ( (con != LAMBDA) && (con != -1) && (_lastPushed == TM)){
    //At the moment, APP and LAMBDA are the only connectives that can take functions of type
    //Other than $o as arguments.
    endTermAsFormula();
  }


  Formula* f;
  TermList fun;
  bool conReverse;
  switch (con) {
  case IMP:
  case AND:
  case OR:
    conReverse = _bools.pop();
    break;
  case IFF:
  case XOR:
  case APP:
  case -2:
  case -1:
    break;
  case NOT:
    f = _formulas.pop();
    _formulas.push(new NegatedFormula(f));
      _lastPushed = FORM;
    _states.push(END_HOL_FUNCTION);
    return;
  case FORALL:
  case EXISTS:{
    f = _formulas.pop();
    _formulas.push(new QuantifiedFormula((Connective)con,_varLists.pop(),_sortLists.pop(),f));
    _lastPushed = FORM;
    //_states.push(END_HOL_FUNCTION);
    _states.push(UNBIND_VARIABLES);
    return;
  }
  case LAMBDA:{
     if(_lastPushed == FORM){
       endFormulaInsideTerm();
     } 
     TermList abstractedTerm = _termLists.pop();
     Formula::VarList* vars = _varLists.pop();
     _sortLists.pop();
     for( int i = Formula::VarList::length(vars) - 1 ; i > -1; i--){
       SortList* sorts;
       ALWAYS(_variableSorts.find(_lambdaVars.pop(), sorts))
       abstractedTerm = abstract(abstractedTerm, sorts->head());
     }
     _termLists.push(abstractedTerm);
     _lastPushed = TM;
     //_states.push(END_HOL_FUNCTION);
     _states.push(UNBIND_VARIABLES);
     return; 
    }
  case LITERAL:
  default:
    throw ::Exception((vstring)"tell me how to handle connective " + Int::toString(con));
  }

  Token& tok = getTok(0);
  Tag tag = tok.tag;
  Connective c;
  bool cReverse = false;
  switch (tag) {
  case T_AND:
    c = AND;
    break;
  case T_NOT_AND:
    cReverse = true;
    c = AND;
    break;
  case T_NOT_OR:
    cReverse = true;
    c = OR;
    break;
  case T_OR:
    c = OR;
    break;
  case T_XOR:
    c = XOR;
    break;
  case T_IFF:
    c = IFF;
    break;
  case T_IMPLY:
    c = IMP;
    break;
  case T_REVERSE_IMP:
    cReverse = true;
    c = IMP;
    break;
  case T_APP:
    c = APP;
    break;
  case T_EQUAL:
  case T_NEQ: {
    // not connectives, but we allow formulas to be arguments to = and !=
    _states.push(END_EQ);
    _connectives.push(-1);
    _states.push(END_HOL_FUNCTION);
    _states.push(HOL_FUNCTION);
    _states.push(MID_EQ);
    if(_lastPushed == FORM){
       endFormulaInsideTerm();
       //equality is evaluated between two terms
    }
    return;
  }
  default:
    // the formula does not end at a binary connective, build the formula and terminate
    switch (con) {
    case IMP:
      f = _formulas.pop();
      if (conReverse) {
        f = new BinaryFormula((Connective)con,f,_formulas.pop());
      }
      else {
        f = new BinaryFormula((Connective)con,_formulas.pop(),f);
      }
      _formulas.push(f);
      _lastPushed = FORM;
      _states.push(END_HOL_FUNCTION);
      return;
    
    case IFF:
    case XOR:
      f = _formulas.pop();
      f = new BinaryFormula((Connective)con,_formulas.pop(),f);
      _formulas.push(f);
      _lastPushed = FORM;
      _states.push(END_HOL_FUNCTION);
      return;

    case AND:
    case OR:
      f = _formulas.pop();
      f = makeJunction((Connective)con,_formulas.pop(),f);
      if (conReverse) {
        f = new NegatedFormula(f);
      }
      _formulas.push(f);
      _lastPushed = FORM;
      _states.push(END_HOL_FUNCTION);
      return;

    case -1:
      return;
#if VDEBUG
    default:
      ASSERTION_VIOLATION;
#endif
    }
  }

  if ((c != APP) & (con == -1) & (_lastPushed == TM)){
      endTermAsFormula();
  }
  
  // con and c are binary connectives
  if (higherPrecedence(con,c)) {
    f = _formulas.pop(); 
    Formula* g = _formulas.pop();
    if (con == AND || con == OR) {
      f = makeJunction((Connective)con,g,f);
      if (conReverse) {
          f = new NegatedFormula(f);
      }
    }
    else if (con == IMP && conReverse) {
      f = new BinaryFormula((Connective)con,f,g); 
    }else {
      f = new BinaryFormula((Connective)con,g,f);
    }
    _formulas.push(f);
      _lastPushed = FORM;
    _states.push(END_HOL_FUNCTION);
    return;
  }

  // c is a binary connective
  _connectives.push(con);
  if (con == IMP || con == AND || con == OR) {
    _bools.push(conReverse);
  }
  _connectives.push(c);
  if (c == IMP || c == AND || c == OR) {
    _bools.push(cReverse);
  }
  resetToks();
  _states.push(END_HOL_FUNCTION);
  _states.push(HOL_FUNCTION);
}

/**
 * Process the end of the $ite expression
 * @since 27/07/2011 Manchester
 * @since 16/04/2015 Gothenburg, major changes to support FOOL
 */
void TPTP::endIte()
{
  CALL("TPTP::endIte");

  TermList elseBranch = _termLists.pop();
  TermList thenBranch = _termLists.pop();
  Formula* condition = _formulas.pop();
  unsigned thenSort = sortOf(thenBranch);
  TermList ts(Term::createITE(condition,thenBranch,elseBranch,thenSort));
  unsigned elseSort = sortOf(elseBranch);
  if (thenSort != elseSort) {
    USER_ERROR("sort mismatch in the if-then-else expression: " +
               thenBranch.toString() + " has the sort " + env.sorts->sortName(thenSort) + ", whereas " +
               elseBranch.toString() + " has the sort " + env.sorts->sortName(elseSort));
  }
  _termLists.push(ts);
} // endIte

/**
 *
 */
void TPTP::endTheoryFunction() {
  CALL("TPTP::endTheoryFunction");

  /**
   * Things get a bit awkward with theories + FOOL, because theory function can
   * return $o in such case be a predicate symbol rather than a function symbol.
   * The current solution is the following -- we always treat application of
   * theory functions as a a term (a formula wrapped inside boolean term, if
   * needed). If later on we discover that we should've taken it as a formula,
   * we simply pull the formula out of the boolean term. This is done in
   * endTermAsFormula().
   */

  Theory::Interpretation itp;
  TermList args[3]; // all theory function use up to 3 arguments as for now
  unsigned arraySort;

  TheoryFunction tf = _theoryFunctions.pop();
  switch (tf) {
    case TF_SELECT: {
      TermList index = _termLists.pop();
      TermList array = _termLists.pop();

      arraySort = sortOf(array);
      if (!env.sorts->isOfStructuredSort(arraySort, Sorts::StructuredSort::ARRAY)) {
        USER_ERROR("$select is being incorrectly used on a type of array " + env.sorts->sortName(arraySort) + " that has not be defined");
      }

      unsigned indexSort = env.sorts->getArraySort(arraySort)->getIndexSort();
      if (sortOf(index) != indexSort) {
        USER_ERROR("sort of index is not the same as the index sort of the array");
      }

      args[0] = array;
      args[1] = index;

      if (env.sorts->getArraySort(arraySort)->getInnerSort() == Sorts::SRT_BOOL) {
        itp = Theory::Interpretation::ARRAY_BOOL_SELECT;
      } else {
        itp = Theory::Interpretation::ARRAY_SELECT;
      }
      break;
    }
    case TF_STORE: {
      TermList value = _termLists.pop();
      TermList index = _termLists.pop();
      TermList array = _termLists.pop();

      arraySort = sortOf(array);
      if (!env.sorts->isOfStructuredSort(arraySort, Sorts::StructuredSort::ARRAY)) {
        USER_ERROR("store is being incorrectly used on a type of array that has not be defined");
      }

      unsigned indexSort = env.sorts->getArraySort(arraySort)->getIndexSort();
      if (sortOf(index) != indexSort) {
        USER_ERROR("sort of index is not the same as the index sort of the array");
      }

      unsigned innerSort = env.sorts->getArraySort(arraySort)->getInnerSort();
      if (sortOf(value) != innerSort) {
        USER_ERROR("sort of value is not the same as the value sort of the array");
      }

      args[0] = array;
      args[1] = index;
      args[2] = value;

      itp = Theory::Interpretation::ARRAY_STORE;

      break;
    }
    default:
      ASSERTION_VIOLATION_REP(tf);
  }

  OperatorType* type = Theory::getArrayOperatorType(arraySort,itp);
  unsigned symbol = env.signature->getInterpretingSymbol(itp, type);
  unsigned arity = Theory::getArity(itp);

  if (Theory::isFunction(itp)) {
    Term* term = Term::create(symbol, arity, args);
    _termLists.push(TermList(term));
  } else {
    Literal* literal = Literal::create(symbol, arity, true, false, args);
    _formulas.push(new AtomicFormula(literal));
    _states.push(END_FORMULA_INSIDE_TERM);
  }
} // endTheoryFunction

/**
 * Process include() declaration
 * @since 07/07/2011 Manchester
 */
void TPTP::include()
{
  CALL("TPTP::include");

  consumeToken(T_LPAR);
  Token& tok = getTok(0);
  if (tok.tag != T_NAME) {
    PARSE_ERROR((vstring)"file name expected",tok);
  }
  vstring relativeName=tok.content;
  resetToks();
  bool ignore = _forbiddenIncludes.contains(relativeName);
  if (!ignore) {
    _allowedNamesStack.push(_allowedNames);
    _allowedNames = 0;
    _inputs.push(_in);
    _includeDirectories.push(_includeDirectory);
  }

  tok = getTok(0);
  if (tok.tag == T_COMMA) {
    if (!ignore) {
      _allowedNames = new Set<vstring>;
    }
    resetToks();
    consumeToken(T_LBRA);
    for(;;) {
      tok = getTok(0);
      if (tok.tag != T_NAME) {
    PARSE_ERROR((vstring)"formula name expected",tok);
      }
      vstring axName=tok.content;
      resetToks();
      if (!ignore) {
    _allowedNames->insert(axName);
      }
      tok = getTok(0);
      if (tok.tag == T_RBRA) {
    resetToks();
    break;
      }
      consumeToken(T_COMMA);
    }
  }
  consumeToken(T_RPAR);
  consumeToken(T_DOT);

  if (ignore) {
    return;
  }
  // here should be a computation of the new include directory according to
  // the TPTP standard, so far we just set it to ""
  _includeDirectory = "";
  vstring fileName(env.options->includeFileName(relativeName));
  {
    BYPASSING_ALLOCATOR; // we cannot make ifstream allocated via Allocator
    _in = new ifstream(fileName.c_str());
  }
  if (!*_in) {
    USER_ERROR((vstring)"cannot open file " + fileName);
  }
} // include

/** add a file name to the list of forbidden includes */
void TPTP::addForbiddenInclude(vstring file)
{
  CALL("TPTP::addForbiddenInclude");
  _forbiddenIncludes.insert(file);
}

/**
 * Read the next token that must be a name.
 * @since 10/04/2011 Manchester
 */
vstring TPTP::name()
{
  CALL("TPTP::name");
  Token& tok = getTok(0);
  if (tok.tag != T_NAME) {
    PARSE_ERROR("name expected",tok);
  }
  vstring nm = tok.content;
  resetToks();
  return nm;
} // name

/**
 * Read the next token that must have a given name.
 * @since 10/04/2011 Manchester
 */
void TPTP::consumeToken(Tag t)
{
  CALL("TPTP::consumeToken");

  Token& tok = getTok(0);
  if (tok.tag != t) {
    vstring expected = toString(t);
    PARSE_ERROR(expected + " expected",tok);
  }
  resetToks();
} // consumeToken

/**
 * Read a formula and save it on the stack of formulas.
 * Adds to _states END_SIMPLE_FORMULA,SIMPLE_FORMULA
 * @since 10/04/2011 Manchester
 */
void TPTP::formula()
{
  CALL("TPTP::formula");
  if(_isThf){
    _connectives.push(-2); //special connective for HOL funcs
    _connectives.push(-1);
    _states.push(END_HOL_FUNCTION);
    _states.push(END_HOL_FUNCTION);
    _states.push(HOL_FUNCTION);
  }else{
    _connectives.push(-1);
    _states.push(END_FORMULA);
    _states.push(SIMPLE_FORMULA);
  }
} // formula

/**
 *
 * @since 26/03/2015 Manchester
 */
void TPTP::termInfix()
{
  CALL("TPTP::termInfix");
  Token tok = getTok(0);
  switch (tok.tag) {
    case T_EQUAL:
    case T_NEQ:
      _states.push(END_FORMULA_INSIDE_TERM);
      _states.push(FORMULA_INFIX);
      return;
    case T_COMMA:
    case T_SEMICOLON:
    case T_RPAR:
    case T_RBRA:
    case T_ASS:
      _states.push(END_TERM);
      return;
    case T_AND:
    case T_NOT_AND:
    case T_NOT_OR:
    case T_OR:
    case T_XOR:
    case T_IFF:
    case T_IMPLY:
    case T_REVERSE_IMP:
      if (_insideEqualityArgument > 0) {
        _states.push(END_TERM);
        return;
      }
      _connectives.push(-1);
      _states.push(END_FORMULA_INSIDE_TERM);
      _states.push(END_FORMULA);
      _states.push(FORMULA_INFIX);
      return;
    default:
      PARSE_ERROR("term or formula expected", tok);
  }
} // termInfix

/**
 * Read a TPTP type expression
 * @since 10/04/2011 Manchester
 * @author Andrei Voronkov
 */
void TPTP::type()
{
  CALL("TPTP::type");

  _typeTags.push(TT_ATOMIC);
  _states.push(END_TYPE);
  _states.push(SIMPLE_TYPE);
} // type


/**
 * Read a function application or a variable and save the resulting literal
 * @since 10/04/2011 Manchester
 */
void TPTP::funApp()
{
  CALL("TPTP::funApp");
  Token tok = getTok(0);
  resetToks();

  if (tok.tag == T_LBRA) {
    _strings.push(toString(T_TUPLE));
  } else {
    _strings.push(tok.content);
  }

  switch (tok.tag) {
    case T_THEORY_FUNCTION:
      consumeToken(T_LPAR);
      addTagState(T_RPAR);
      switch (getTheoryFunction(tok)) {
        case TF_SELECT:
          _states.push(TERM);
          addTagState(T_COMMA);
          _states.push(TERM);
          break;
        case TF_STORE:
          _states.push(TERM);
          addTagState(T_COMMA);
          _states.push(TERM);
          addTagState(T_COMMA);
          _states.push(TERM);
          break;
        default:
          ASSERTION_VIOLATION_REP(tok.content);
      }
      return;

    case T_ITE:
      consumeToken(T_LPAR);
      addTagState(T_RPAR);
      _states.push(TERM);
      addTagState(T_COMMA);
      _states.push(TERM);
      addTagState(T_COMMA);
      _states.push(FORMULA);
      return;

    case T_LET:
      addTagState(T_RPAR);
      _states.push(TERM);
      addTagState(T_COMMA);
      _states.push(BINDING);
      consumeToken(T_LPAR);
      return;

    case T_LBRA:
      _states.push(ARGS);
      _ints.push(1); // the arity of the function symbol is at least 1
      return;

    case T_VAR:
      _ints.push(-1); // dummy arity to indicate a variable
      return;

    case T_NAME:
      if (getTok(0).tag == T_LPAR) {
        resetToks();
        _states.push(ARGS);
        _ints.push(1); // the arity of the function symbol is at least 1
      } else {
        _ints.push(0); // arity
      }
      return;

    default:
      PARSE_ERROR("unexpected token", tok);
  }
} // TPTP::funApp

void TPTP::binding()
{
  CALL("TPTP::binding");

  switch (getTok(0).tag) {
    case T_NAME: {
      _strings.push(getTok(0).content);
      resetToks();

      Token tok = getTok(0);

      switch (tok.tag) {
        case T_ASS:
        case T_LPAR:
          resetToks();
          _states.push(END_BINDING);
          _states.push(TERM);
          if (tok.tag == T_LPAR) {
            addTagState(T_ASS);
            addTagState(T_RPAR);
            _states.push(VAR_LIST);
          } else {
            // empty list of vars
            _varLists.push(0);
            _sortLists.push(0);
            _bindLists.push(0);
          }
          return;

        default:
          PARSE_ERROR(toString(T_LPAR) + " or " + toString(T_ASS) + " expected", tok);
      }
    }
    case T_LBRA: {
      resetToks();
      _states.push(END_TUPLE_BINDING);
      _states.push(TERM);
      addTagState(T_ASS);
      addTagState(T_RBRA);
      _states.push(TUPLE_BINDING);
      break;
    }
    default:
      PARSE_ERROR("name or tuple expected",getTok(0));
  }
}

void TPTP::endBinding() {
  CALL("TPTP::endBinding");

  Formula::VarList* vars = _varLists.top(); // will be poped in endLet()
  _sortLists.pop();
  Stack<unsigned> argSorts(0);
  Formula::VarList::Iterator vit(vars);
  while (vit.hasNext()) {
    unsigned var = (unsigned)vit.next();
    ASS_REP(_variableSorts.find(var), var);
    const SortList* sorts = _variableSorts.get(var);
    ASS_REP(SortList::isNonEmpty(sorts), var);
    argSorts.push(sorts->head());
  }

  unsigned arity = Formula::VarList::length(vars);

  TermList binding = _termLists.top();
  unsigned bindingSort = sortOf(binding);
  bool isPredicate = bindingSort == Sorts::SRT_BOOL;

  vstring name = _strings.pop();

  unsigned symbolNumber = isPredicate ? env.signature->addFreshPredicate(arity,name.c_str())
                                      : env.signature->addFreshFunction (arity,name.c_str());

  if (isPredicate) {
    OperatorType* type = OperatorType::getPredicateType(arity, argSorts.begin());
    env.signature->getPredicate(symbolNumber)->setType(type);
  } else {
    OperatorType* type = OperatorType::getFunctionType(arity, argSorts.begin(), bindingSort);
    env.signature->getFunction(symbolNumber)->setType(type);
  }

  LetFunctionName functionName(name, arity);
  LetFunctionReference functionReference(symbolNumber, isPredicate);

  LetFunctionsScope::Iterator functions(_currentLetScope);
  while (functions.hasNext()) {
    if (functions.next().first == functionName) {
      USER_ERROR("The symbol " + name + " of arity " + Int::toString(arity) + " is defined twice in a $let-expression.");
    }
  }

  _currentLetScope.push(LetFunction(functionName, functionReference));
  _currentBindingScope.push(LetBinding(symbolNumber, false));

  Token tok = getTok(0);
  if (tok.tag == T_SEMICOLON) {
    resetToks();
    _states.push(BINDING);
  } else {
    _letScopes.push(_currentLetScope);
    _currentLetScope = LetFunctionsScope();

    _letBindings.push(_currentBindingScope);
    _currentBindingScope = LetBindingScope();
  }

  _states.push(UNBIND_VARIABLES);
} // endBinding

void TPTP::endTupleBinding() {
  CALL("TPTP::endTupleBinding");

  TermList binding = _termLists.top();
  unsigned bindingSort = sortOf(binding);

  if (!env.sorts->isOfStructuredSort(bindingSort, Sorts::StructuredSort::TUPLE)) {
    USER_ERROR("The binding of a tuple let expression is not a tuple but has the sort " + env.sorts->sortName(bindingSort));
  }

  Sorts::TupleSort* tupleSort = env.sorts->getTupleSort(bindingSort);
  unsigned tupleArity = tupleSort->arity();

  Set<vstring> uniqueSymbolNames;
  IntList* constants = IntList::empty();
  for (unsigned i = 0; i < tupleArity; i++) {
    vstring name = _strings.pop();
    if (uniqueSymbolNames.contains(name)) {
      USER_ERROR("The symbol " + name + " is defined twice in a tuple $let-expression.");
    } else {
      uniqueSymbolNames.insert(name);
    }

    unsigned sort = tupleSort->argument(tupleArity - i - 1);

    bool isPredicate = sort == Sorts::SRT_BOOL;

    unsigned symbol;
    if (isPredicate) {
      symbol = env.signature->addFreshPredicate(0, name.c_str());
      env.signature->getPredicate(symbol)->setType(OperatorType::getPredicateType(0, 0));
    } else {
      symbol = env.signature->addFreshFunction(0, name.c_str());
      env.signature->getFunction(symbol)->setType(OperatorType::getConstantsType(sort));
    }

    IntList::push(symbol, constants);

    LetFunctionName functionName(name, 0);
    LetFunctionReference functionReference(symbol, isPredicate);
    _currentLetScope.push(LetFunction(functionName, functionReference));
  }

  _varLists.push(constants);

  unsigned tupleFunctor = Theory::tuples()->getFunctor(bindingSort);
  _currentBindingScope.push(LetBinding(tupleFunctor, true));

  Token tok = getTok(0);
  if (tok.tag == T_SEMICOLON) {
    resetToks();
    _states.push(BINDING);
  } else {
    _letScopes.push(_currentLetScope);
    _currentLetScope = LetFunctionsScope();

    _letBindings.push(_currentBindingScope);
    _currentBindingScope = LetBindingScope();
  }
} // endTupleBinding

bool TPTP::findLetSymbol(bool isPredicate, vstring name, unsigned arity, unsigned& symbol) {
  CALL("TPTP::findLetSymbol");

  LetFunctionName functionName(name, arity);

  Stack<LetFunctionsScope>::TopFirstIterator scopes(_letScopes);
  while (scopes.hasNext()) {
    LetFunctionsScope scope = scopes.next();
    LetFunctionsScope::Iterator functions(scope);
    while (functions.hasNext()) {
      LetFunction function = functions.next();
      if ((function.first == functionName) && (function.second.second == isPredicate)) {
        symbol = function.second.first;
        return true;
      }
    }
  }
  return false;
} // findLetSymbol

/**
 * Process the end of the $let expression
 * @since 27/07/2011 Manchester
 */
void TPTP::endLet()
{
  CALL("TPTP::endLet");

  TermList let = _termLists.pop();
  unsigned sort = sortOf(let);

  _letScopes.pop();
  LetBindingScope scope = _letBindings.pop(); // TODO: inlining this crashes the program, WTF?
  LetBindingScope::TopFirstIterator bindings(scope);
  while (bindings.hasNext()) {
    LetBinding binding = bindings.next();
    unsigned symbol = binding.first;
    bool isTuple = binding.second;
    if (isTuple) {
      let = TermList(Term::createTupleLet(symbol, _varLists.pop(), _termLists.pop(), let, sort));
    } else {
      let = TermList(Term::createLet(symbol, _varLists.pop(), _termLists.pop(), let, sort));
    }
  }
  _termLists.push(let);
} // endLet

/**
 * Process the end of the tuple expression
 * @since 19/04/2016 Gothenburg
 */
void TPTP::endTuple()
{
  CALL("TPTP::endTuple");

  unsigned arity = (unsigned)_ints.pop();
  ASS_GE(_termLists.size(), arity);

  DArray<TermList> elements(arity);
  DArray<unsigned> sorts(arity);

  for (int i = arity - 1; i >= 0; i--) {
    TermList ts = _termLists.pop();
    elements[i] = ts;
    sorts[i] = sortOf(ts);
  }

  Term* t = Term::createTuple(arity, sorts.begin(), elements.begin());
  _termLists.push(TermList(t));
} // endTuple

/**
 * Read a non-empty sequence of arguments, including the right parentheses
 * and save the resulting sequence of TermList and their number
 * @since 10/04/2011 Manchester
 */
void TPTP::args()
{
  CALL("TPTP::args");
  _states.push(END_ARGS);
  _states.push(TERM);
} // args

/**
 * Read a list of arguments after a term
 * @since 27/07/2011 Manchester
 */
void TPTP::endArgs()
{
  CALL("TPTP::endArgs");
 // check if there is any other term in the argument list
  Token tok = getTok(0);
  switch (tok.tag) {
  case T_COMMA:
    resetToks();
    _ints.push(_ints.pop()+1);
    _states.push(END_ARGS);
    _states.push(TERM);
    return;
  case T_RPAR:
    resetToks();
    return;
  case T_RBRA:
    resetToks();
    return;
  default:
    PARSE_ERROR(", ) or ] expected after an end of a term",tok);
  }
} // endArgs

/**
 * Bind a variable to a sort
 * @since 22/04/2011 Manchester
 */
 //Broken function for FOL AYB - need to replace with old function!
void TPTP::bindVariable(int var,unsigned sortNumber)
{
  CALL("TPTP::bindVariable");

  SortList* bindings;
  if (_variableSorts.find(var,bindings)) {
    _variableSorts.replace(var,new SortList(sortNumber,bindings));
  }
  else {
    _variableSorts.insert(var,new SortList(sortNumber));
  }
} // bindVariable

/**
 * Bind a variable to a type and a binder
 * @since 20/03/2018 Manchester
 */
void TPTP::bindVariable(int var, OperatorType* type)
{
  CALL("TPTP::bindVariable");

  BindList* binders;
  TypeList* types;
  if (_varBinders.find(var, binders)){
    _varBinders.replace(var, new BindList(_lastBinder, binders));
  }
  else {
    _varBinders.insert(var, new BindList(_lastBinder));
  }
  if (_varTypes.find(var,types)) {
    _varTypes.replace(var,new TypeList(type,types));
  }
  else {
    _varTypes.insert(var,new TypeList(type));
  }
} // bindVariable

/**
 * Bind a variable to a type and a binder
 * @since 20/03/2018 Manchester
 */
void TPTP::bindVariableToFunc(int var, unsigned func)
{
  CALL("TPTP::bindVariableToFunc");

  FuncList* functors;
  if (_varFunctors.find(var,functors)) {
    _varFunctors.replace(var,new SortList(func,functors));
  }
  else {
    _varFunctors.insert(var,new SortList(func));
  }
} // bindVariable

/**
 * Read a non-empty sequence of variable and save the resulting
 * sequence of TermList and their number
 * @since 07/07/2011 Manchester
 * @since 16/04/2015 Gothenburg, do not parse the closing ']'
 */
 
 //Broken function for FOL. Only works for HOL AYB. Need to copy over old function at some point.
void TPTP::varList()
{
  CALL("TPTP::varList");

  Stack<int> vars;
  for (;;) {
    Token& tok = getTok(0);

    if (tok.tag != T_VAR) {
      PARSE_ERROR("variable expected",tok);
    }
    int var = _vars.insert(tok.content);
    vars.push(var);
    if(_lastBinder == LAMB){ _lambdaVars.push(var); }
    resetToks();
    bool sortDeclared = false;
    afterVar:
    tok = getTok(0);
    switch (tok.tag) {
    case T_COLON:{ // v: type
      if (sortDeclared) {
        PARSE_ERROR("two declarations of variable sort",tok);
      }
      resetToks();
      Stack<unsigned> sorts = readHOLSort();  
      unsigned sort = foldl(sorts);
      unsigned returnSort = sorts.pop();
      OperatorType* type = OperatorType::getFunctionType(sorts.size(), sorts.begin(), returnSort);
      if(_lastBinder != LAMB && type->arity() != 0){ //higher-order var (not index)
        unsigned functor = env.signature->addFreshHOVar(type, var);
        bindVariableToFunc(var, functor);
      }
      bindVariable(var, type);        
      bindVariable(var, sort);
      //bind every variable to sort and type. Must be better way utilising polymorphism/template class
      sortDeclared = true;
      goto afterVar;
    }
    case T_COMMA:
      if (!sortDeclared) {
        bindVariable(var,Sorts::SRT_DEFAULT); //in lambda var list sorts always declared?
      }
      resetToks();
      break;

    default:{
      if (!sortDeclared) {
        bindVariable(var,Sorts::SRT_DEFAULT);
      }
      Formula::VarList* vs = Formula::VarList::empty();
      SortList* ss = SortList::empty();
      while (!vars.isEmpty()) {
        int v = vars.pop();
        vs = new Formula::VarList(v,vs);
        ss = new SortList(sortOf(TermList(v,false)),ss);
      }
      _varLists.push(vs);
      _sortLists.push(ss);
      _bindLists.push(vs);
      return;
    }
    }
  }
} // varList


/**
 * Read a non-empty sequence of constants and save the resulting
 * sequence of TermList and their number
 * @since 20/04/2016 Gothenburg
 */
void TPTP::tupleBinding()
{
  CALL("TPTP::tupleBinding");

  for (;;) {
    vstring nm = name();
    _strings.push(nm);
    if (getTok(0).tag != T_COMMA) {
      break;
    }
    resetToks();
  }
} // constantList

/**
 * Read a term and save the resulting TermList
 * @since 10/04/2011 Manchester
 * @since 13/04/2015 Gothenburg, major changes to support FOOL
 */
void TPTP::term()
{
  CALL("TPTP::term");
  Token tok = getTok(0);
  switch (tok.tag) {
    case T_NAME:
    case T_THEORY_FUNCTION:
    case T_VAR:
    case T_ITE:
    case T_LET:
    case T_LBRA:
      _states.push(TERM_INFIX);
      _states.push(FUN_APP);
      return;
    case T_LPAR:
      consumeToken(T_LPAR);
      addTagState(T_RPAR);
      _states.push(TERM);
      return;
    case T_STRING:
    case T_INT:
    case T_REAL:
    case T_RAT: {
      resetToks();
      unsigned number;
      switch (tok.tag) {
        case T_STRING:
          number = env.signature->addStringConstant(tok.content);
          break;
        case T_INT:
          number = addIntegerConstant(tok.content,_overflow,_isFof);
          break;
        case T_REAL:
          number = addRealConstant(tok.content,_overflow,_isFof);
          break;
        case T_RAT:
          number = addRationalConstant(tok.content,_overflow,_isFof);
          break;
        default:
          ASSERTION_VIOLATION;
      }
      Term *t = new(0) Term;
      t->makeSymbol(number, 0);
      t = env.sharing->insert(t);
      TermList constant(t);
      _termLists.push(constant);
      return;
    }

    default:
      _states.push(FORMULA_INSIDE_TERM);
  }
} // term

/**
 * Build a term assembled by term()
 * @since 09/07/2011 Manchester
 * @since 14/04/2015 Gothenburg, major changes to support FOOL
 */
void TPTP::endTerm()
{
  CALL("TPTP::endTerm");

  vstring name = _strings.pop();

  if (name == toString(T_ITE)) {
    _states.push(END_ITE);
    return;
  }

  if (name == toString(T_LET)) {
    _states.push(END_LET);
    return;
  }

  if (name == toString(T_TUPLE)) {
    _states.push(END_TUPLE);
    return;
  }

  TheoryFunction tf;
  if (findTheoryFunction(name, tf)) {
    _theoryFunctions.push(tf);
    _states.push(END_THEORY_FUNCTION);
    return;
  }

  int arity = _ints.pop();

  if (arity == -1) {
    // it was a variable
    unsigned var = (unsigned)_vars.insert(name);
    _termLists.push(TermList(var, false));
    return;
  }

  unsigned symbol;
  if (env.signature->predicateExists(name, arity) ||
      findLetSymbol(true, name, arity, symbol) ||
      findInterpretedPredicate(name, arity)) {
    // if the function symbol is actually a predicate,
    // we need to construct a formula and wrap it inside a term
    _formulas.push(createPredicateApplication(name, arity));
    _states.push(END_FORMULA_INSIDE_TERM);
    return;
  }

  _termLists.push(createFunctionApplication(name, arity));
} // endTerm

/**
 * Read after an end of atom or after lhs of an equality or inequality
 * @since 10/04/2011 Manchester
 * @since 13/04/2015 Gothenburg, major changes to support FOOL
 */
void TPTP::formulaInfix()
{
  CALL("TPTP::formulaInfix");

  Token tok = getTok(0);

  if (tok.tag == T_EQUAL || tok.tag == T_NEQ) {
    _states.push(END_EQ);
    _states.push(TERM);
    _states.push(MID_EQ);
    _states.push(END_TERM);
    return;
  }

  vstring name = _strings.pop();

  if (name == toString(T_ITE)) {
    _states.push(END_TERM_AS_FORMULA);
    _states.push(END_ITE);
    return;
  }

  TheoryFunction tf;
  if (findTheoryFunction(name, tf)) {
    switch (tf) {
      case TF_STORE:
        USER_ERROR("$store expression cannot be used as formula");
        break;
      case TF_SELECT:
        _theoryFunctions.push(tf);
        _states.push(END_TERM_AS_FORMULA);
        _states.push(END_THEORY_FUNCTION);
        break;
      default:
        ASSERTION_VIOLATION_REP(name);
    }
    return;
  }

  if (name == toString(T_LET)) {
    _states.push(END_TERM_AS_FORMULA);
    _states.push(END_LET);
    return;
  }

  int arity = _ints.pop();

  if (arity == -1) {
    // that was a variable
    unsigned var = (unsigned)_vars.insert(name);
    _termLists.push(TermList(var, false));
    _states.push(END_TERM_AS_FORMULA);
    return;
  }

  _formulas.push(createPredicateApplication(name, arity));
} // formulaInfix

/**
 * Read after an end of equality or inequality and save the (in)equality formula.
 * @since 09/07/2011 Manchester
 */
void TPTP::endEquality()
{
  CALL("TPTP::endEquality");

  _insideEqualityArgument--;
  
  if((_isThf) & (_lastPushed == FORM)){
    endFormulaInsideTerm();
  }

  TermList rhs = _termLists.pop();
  TermList lhs = _termLists.pop();

  if (sortOf(rhs) != sortOf(lhs)) {
    unsigned rsort = sortOf(rhs); 
    unsigned lsort = sortOf(lhs);
    USER_ERROR("Cannot create equality between terms of different types.\n"+
      rhs.toString()+" is "+env.sorts->sortName(rsort)+"\n"+
      lhs.toString()+" is "+env.sorts->sortName(lsort)
    );
  }

  Literal* l = createEquality(_bools.pop(),lhs,rhs);
   _formulas.push(new AtomicFormula(l));
   _lastPushed = FORM;
} // endEquality

/**
 * Read
 * @since 09/07/2011 Manchester
 */
void TPTP::midEquality()
{
  CALL("TPTP::midEquality");

  _insideEqualityArgument++;

  Token tok = getTok(0);
  switch (tok.tag) {
  case T_EQUAL:
    _bools.push(true);
    break;
  case T_NEQ:
    _bools.push(false);
    break;
  default:
    PARSE_ERROR("either = or != expected",tok);
  }
  resetToks();
} // midEquality

/**
 * Creates an equality literal and takes care of its sort when it
 * is an equality between two variables.
 * @since 21/07/2011 Manchester
 * @since 03/05/2013 Train Manchester-London, bug fix
 */
Literal* TPTP::createEquality(bool polarity,TermList& lhs,TermList& rhs)
{
  TermList masterVar;
  unsigned sortNumber;
  if (!SortHelper::getResultSortOrMasterVariable(lhs, sortNumber, masterVar)) {
    // Master variable is a variable whose sort determines the sort of a term.
    // If term is a variable, the master variable is the variable itself. The
    // trickier case is when we have an if-then-else expression with variable
    // arguments.
    SortList* vs;
    if (_variableSorts.find(masterVar.var(),vs) && vs) {
      sortNumber = vs->head();
    }
    else { // this may happen when free variables appear in the formula (or clause)
      sortNumber = Sorts::SRT_DEFAULT;
    }
  }

  return Literal::createEquality(polarity,lhs,rhs,sortNumber);
} // TPTP::createEquality

/**
 * Creates a formula that is a predicate application literal from
 * provided predicate symbol name and arity. If arity is greater than zero,
 * the arguments are assumed to be on the _termLists stack.
 * @since 27/03/1015 Manchester
 */
Formula* TPTP::createPredicateApplication(vstring name, unsigned arity)
{
  CALL("TPTP::createPredicateApplication");
  ASS_GE(_termLists.size(), arity);

  int pred;
  unsigned letPred;
  if (findLetSymbol(true, name, arity, letPred)) {
    pred = (int)letPred;
  } else {
    if (arity > 0) {
      bool dummy;
      pred = addPredicate(name, arity, dummy, _termLists.top());
    } else {
      pred = env.signature->addPredicate(name, 0);
    }
  }
  if (pred == -1) { // equality
    TermList rhs = _termLists.pop();
    TermList lhs = _termLists.pop();
    return new AtomicFormula(createEquality(true,lhs,rhs));
  }
  if (pred == -2){ // distinct
    // TODO check that we are top-level
    // If fewer than 5 things are distinct then we add the disequalities
    if(arity<5){
      static Stack<unsigned> distincts;
      distincts.reset();
      for(int i=arity-1;i >= 0; i--){
        TermList t = _termLists.pop();
        if(t.term()->arity()!=0) USER_ERROR("$distinct can only be used with constants");
        distincts.push(t.term()->functor());
      }
      Formula* distinct_formula = DistinctGroupExpansion().expand(distincts);
      return distinct_formula;
    }else{
      // Otherwise record them as being in a distinct group
      unsigned grpIdx = env.signature->createDistinctGroup(0);
      for(int i = arity-1;i >=0; i--){
        TermList ts = _termLists.pop();
        if(!ts.isTerm() || ts.term()->arity()!=0){
          USER_ERROR("$distinct should only be used positively with constants");
        }
        env.signature->addToDistinctGroup(ts.term()->functor(),grpIdx);
      }
      return new Formula(true); // we ignore it, it evaluates to true as we have recorded it elsewhere
    }
  }
  // not equality or distinct
  Literal* lit = new(arity) Literal(pred,arity,true,false);
  OperatorType* type = env.signature->getPredicate(pred)->predType();
  bool safe = true;
  for (int i = arity-1;i >= 0;i--) {
    unsigned sort = type->arg(i);
    TermList ts = _termLists.pop();
    unsigned tsSort = sortOf(ts);
    if (sort != tsSort) {
      USER_ERROR("Argument " + Lib::Int::toString(i) +
                 " of predicate " + env.signature->predicateName(pred) +
                 " expected something of sort "+env.sorts->sortName(sort)+
                 " but got something of sort "+env.sorts->sortName(tsSort));
    }
    safe = safe && ts.isSafe();
    *(lit->nthArgument(i)) = ts;
  }
  if (safe) {
    lit = env.sharing->insert(lit);
  }
  return new AtomicFormula(lit);
} // createPredicateApplication

/**
 * Creates a term that is a function application from
 * provided function symbol name and arity. If arity is greater than zero,
 * the arguments are assumed to be on the _termLists stack.
 * @since 13/04/2015 Gothenburg, major changes to support FOOL
 */
TermList TPTP::createFunctionApplication(vstring name, unsigned arity, bool index)
{
  CALL("TPTP::createFunctionApplication");
  ASS_GE(_termLists.size(), arity);

  unsigned fun;
  bool added;
  if (!findLetSymbol(false, name, arity, fun)) {
    if(_isThf){
      //bypassing all string constant, integer constant, real consant etc. Bad, change in the future.
      fun = env.signature->addFunction(name,arity,added, false, index ? 2 : 1);
    }else if (arity > 0) {
      fun = addFunction(name, arity, added, _termLists.top());
    } else {
      fun = addUninterpretedConstant(name, _overflow, added);
    }
  }
  Term* t = new(arity) Term;
  t->makeSymbol(fun,arity);
  OperatorType* type = env.signature->getFunction(fun)->fnType();
  bool safe = true;
  for (int i = arity-1;i >= 0;i--) {
    unsigned sort = type->arg(i);
    TermList ss = _termLists.pop();
    unsigned ssSort = sortOf(ss);
    if (sort != ssSort) {
      USER_ERROR("The sort " + env.sorts->sortName(ssSort) + " of function argument " + ss.toString() + " "
                 "does not match the expected sort " + env.sorts->sortName(sort));
    }
    *(t->nthArgument(i)) = ss;
    safe = safe && ss.isSafe();
  }
  if (safe) {
    t = env.sharing->insert(t);
  }
  TermList ts(t);
  return ts;
}

TermList TPTP::createHigherOrderVarApp(unsigned func, OperatorType* type){
  CALL("TPTP::createHigherOrderVarApp");
  
  unsigned arity = type->arity();
  Term* t = new(arity) Term;
  t->makeSymbol(func,arity);

  bool safe = true;
  for (int i = arity-1;i >= 0;i--) {
    unsigned sort = type->arg(i);
    TermList ss = _termLists.pop();
    unsigned ssSort = sortOf(ss);
    if (sort != ssSort) {
      USER_ERROR("The sort " + env.sorts->sortName(ssSort) + " of function argument " + ss.toString() + " "
                 "does not match the expected sort " + env.sorts->sortName(sort));
    }
    *(t->nthArgument(i)) = ss;
    safe = safe && ss.isSafe();
  }
  if (safe) {
    t = env.sharing->insert(t);
  }
  TermList ts(t);
  return ts;
}

/**
 * Build a formula from previousy built subformulas
 * @since 10/04/2011 Manchester
 */
void TPTP::endFormula()
{
  CALL("TPTP::endFormula");
  
  int con = _connectives.pop();
  Formula* f;
  bool conReverse;
  switch (con) {
  case IMP:
  case AND:
  case OR:
    conReverse = _bools.pop();
    break;
  case IFF:
  case XOR:
  case -1:
    break;
  case NOT:
    f = _formulas.pop();
    // This gets rid of the annoying step in proof output where ~(L) is flattend to (~L)
    if(f->connective()==LITERAL){
      Literal* oldLit = static_cast<AtomicFormula*>(f)->literal();
      Literal* newLit = Literal::create(oldLit,!oldLit->polarity());
      _formulas.push(new AtomicFormula(newLit));
    }
    else{
      _formulas.push(new NegatedFormula(f));
    }
    _states.push(END_FORMULA);
    return;
  case FORALL:
  case EXISTS:
    f = _formulas.pop();
    _formulas.push(new QuantifiedFormula((Connective)con,_varLists.pop(),_sortLists.pop(),f));
    _states.push(END_FORMULA);
    return;
  case LITERAL:
  default:
    throw ::Exception((vstring)"tell me how to handle connective " + Int::toString(con));
  }

  Token& tok = getTok(0);
  Tag tag = tok.tag;
  Connective c;
  bool cReverse = false;
  switch (tag) {
  case T_AND:
    c = AND;
    break;
  case T_NOT_AND:
    cReverse = true;
    c = AND;
    break;
  case T_NOT_OR:
    cReverse = true;
    c = OR;
    break;
  case T_OR:
    c = OR;
    break;
  case T_XOR:
    c = XOR;
    break;
  case T_IFF:
    c = IFF;
    break;
  case T_IMPLY:
    c = IMP;
    break;
  case T_REVERSE_IMP:
    cReverse = true;
    c = IMP;
    break;
  case T_EQUAL:
  case T_NEQ: {
    // not connectives, but we allow formulas to be arguments to = and !=
    _states.push(END_EQ);
    _states.push(TERM);
    _states.push(MID_EQ);
    _states.push(END_FORMULA_INSIDE_TERM);
    return;
  }
  default:
    // the formula does not end at a binary connective, build the formula and terminate
    switch (con) {
    case IMP:
      f = _formulas.pop();
      if (conReverse) {
    f = new BinaryFormula((Connective)con,f,_formulas.pop());
      }
      else {
    f = new BinaryFormula((Connective)con,_formulas.pop(),f);
      }
      _formulas.push(f);
      _states.push(END_FORMULA);
      return;

    case IFF:
    case XOR:
      f = _formulas.pop();
      f = new BinaryFormula((Connective)con,_formulas.pop(),f);
      _formulas.push(f);
      _states.push(END_FORMULA);
      return;

    case AND:
    case OR:
      f = _formulas.pop();
      f = makeJunction((Connective)con,_formulas.pop(),f);
      if (conReverse) {
    f = new NegatedFormula(f);
      }
      _formulas.push(f);
      _states.push(END_FORMULA);
      return;

    case -1:
      return;
#if VDEBUG
    default:
      ASSERTION_VIOLATION;
#endif
    }
  }

  // con and c are binary connectives
  if (higherPrecedence(con,c)) {
    f = _formulas.pop();
    Formula* g = _formulas.pop();
    if (con == AND || con == OR) {
      f = makeJunction((Connective)con,g,f);
      if (conReverse) {
    f = new NegatedFormula(f);
      }
    }
    else if (con == IMP && conReverse) {
      f = new BinaryFormula((Connective)con,f,g);
    }
    else {
      f = new BinaryFormula((Connective)con,g,f);
    }
    _formulas.push(f);
    _states.push(END_FORMULA);
    return;
  }

  // c is a binary connective
  _connectives.push(con);
  if (con == IMP || con == AND || con == OR) {
    _bools.push(conReverse);
  }
  _connectives.push(c);
  if (c == IMP || c == AND || c == OR) {
    _bools.push(cReverse);
  }
  resetToks();
  _states.push(END_FORMULA);
  _states.push(SIMPLE_FORMULA);
} // endFormula

/**
 * Builds a term that really is a formula
 * @author Evgeny Kotelnikov
 * @since 27/03/2015 Manchester
 */
void TPTP::formulaInsideTerm()
{
  CALL("TPTP::formulaInsideTerm");
  _states.push(END_FORMULA_INSIDE_TERM);
  _states.push(FORMULA);
} // formulaInsideTerm

/**
 * Wraps a formula inside a term
 * @author Evgeny Kotelnikov
 * @since 27/03/2015 Manchester
 */
void TPTP::endFormulaInsideTerm()
{
  CALL("TPTP::endFormulaInsideTerm");
  Formula* f = _formulas.pop();
  TermList ts(Term::createFormula(f));
  _termLists.push(ts);
  _lastPushed = TM;
} // endFormulaInsideTerm

/**
 * Makes a boolean term a formula
 * @author Evgeny Kotelnikov
 * @since 27/03/2015 Manchester
 */
void TPTP::endTermAsFormula()
{
  CALL("TPTP::endTermAsFormula");
  TermList t = _termLists.pop();
  if (sortOf(t) != Sorts::SRT_BOOL) {
    vstring sortName = env.sorts->sortName(sortOf(t));
    ASSERTION_VIOLATION;
    USER_ERROR("Non-boolean term " + t.toString() + " of sort " + sortName + " is used in a formula context");
  }
  if (t.isTerm() && t.term()->isFormula()) {
    _formulas.push(t.term()->getSpecialData()->getFormula());
    _lastPushed = FORM;
  } else {
    _formulas.push(new BoolTermFormula(t));
    _lastPushed = FORM;
  }
} // endTermAsFormula

/**
 * Build a type from previousy built types
 * @since 14/07/2011 Manchester
 */
void TPTP::endType()
{
  CALL("TPTP::endType");

  TypeTag tt = _typeTags.pop();
  Type* t = _types.pop();
  switch (tt) {
  case TT_ATOMIC:
    break;
  case TT_PRODUCT:
    t = new ProductType(_types.pop(),t);
    tt = _typeTags.pop();
    break;
  case TT_ARROW:
    t = new ArrowType(_types.pop(),t);
    tt = _typeTags.pop();
    break;
  }
  ASS(tt == TT_ATOMIC);
  _types.push(t);

  Token tok = getTok(0);
  switch (tok.tag) {
  case T_STAR:
    _typeTags.push(tt);
    _typeTags.push(TT_PRODUCT);
    break;
  case T_ARROW:
    _typeTags.push(tt);
    _typeTags.push(TT_ARROW);
    break;
  default:
    return;
  }
  resetToks();
  _states.push(END_TYPE);
  _states.push(SIMPLE_TYPE);
} // endType

/**
 * Skip a tag.
 * @since 10/04/2011 Manchester
 */
void TPTP::tag()
{
  CALL("TPTP::tag");
  consumeToken(_tags.pop());
} // tag

/**
 * Process the end of the fof() definition and build the corresponding unit.
 * @since 10/04/2011 Manchester
 */
void TPTP::endFof()
{
  CALL("TPTP::endFof");

  TPTP::SourceRecord* source = 0;

  // are we interested in collecting sources?
  if (_unitSources) {
    source = getSource();
  }
#if DEBUG_SOURCE
  else{
    // create fake map
    _unitSources = new DHMap<Unit*,SourceRecord*>();
    source = getSource();
  }
#endif

  skipToRPAR();
  consumeToken(T_DOT);

  bool isFof = _bools.pop();
  Formula* f = _formulas.pop();
  vstring nm = _strings.pop(); // unit name
  if (_lastInputType == -1) {
    // assumption, they are not used
    return;
  }
  if (_allowedNames && !_allowedNames->contains(nm)) {
    return;
  }

  Unit* unit;
  if (isFof) { // fof() or tff()
    env.statistics->inputFormulas++;
    unit = new FormulaUnit(f,new Inference(Inference::INPUT),(Unit::InputType)_lastInputType);
    unit->setInheritedColor(_currentColor);
  }
  else { // cnf()
    env.statistics->inputClauses++;
    // convert the input formula f to a clause
    Stack<Formula*> forms;
    Stack<Literal*> lits;
    Formula* g = f;
    forms.push(f);
    while (! forms.isEmpty()) {
      f = forms.pop();
      switch (f->connective()) {
      case OR:
    {
      FormulaList::Iterator fs(static_cast<JunctionFormula*>(f)->getArgs());
      while (fs.hasNext()) {
        forms.push(fs.next());
      }
    }
    break;

      case LITERAL:
      case NOT:
    {
      bool positive = true;
      while (f->connective() == NOT) {
        f = static_cast<NegatedFormula*>(f)->subformula();
        positive = !positive;
      }
      if (f->connective() != LITERAL) {
        USER_ERROR((vstring)"input formula not in CNF: " + g->toString());
      }
      Literal* l = static_cast<AtomicFormula*>(f)->literal();
      lits.push(positive ? l : Literal::complementaryLiteral(l));
    }
    break;

      case TRUE:
    return;
      case FALSE:
    break;
      default:
    USER_ERROR((vstring)"input formula not in CNF: " + g->toString());
      }
    }
    unit = Clause::fromStack(lits,(Unit::InputType)_lastInputType,new Inference(Inference::INPUT));
    unit->setInheritedColor(_currentColor);
  }

  if(source){ 
    ASS(_unitSources);
    _unitSources->insert(unit,source);
  }

  if (env.options->outputAxiomNames()) {
    assignAxiomName(unit,nm);
  }
#if DEBUG_SHOW_UNITS
  cout << "Unit: " << unit->toString() << "\n";
#endif
  if (!_inputs.isEmpty()) {
    unit->markIncluded();
  }

  switch (_lastInputType) {
  case Unit::CONJECTURE:
    if(!isFof) USER_ERROR("conjecture is not allowed in cnf");
    if(_seenConjecture) USER_ERROR("Vampire only supports a single conjecture in a problem");
    _seenConjecture=true;
    if (_isQuestion && ((env.options->mode() == Options::Mode::CLAUSIFY) || (env.options->mode() == Options::Mode::TCLAUSIFY)) && f->connective() == EXISTS) {
      // create an answer predicate
      QuantifiedFormula* g = static_cast<QuantifiedFormula*>(f);
      unsigned arity = Formula::VarList::length(g->vars());
      unsigned pred = env.signature->addPredicate("$$answer",arity);
      env.signature->getPredicate(pred)->markAnswerPredicate();
      Literal* a = new(arity) Literal(pred,arity,true,false);
      Formula::VarList::Iterator vs(g->vars());
      int i = 0;
      while (vs.hasNext()) {
    a->nthArgument(i++)->makeVar(vs.next());
      }
      a = env.sharing->insert(a);
      f = new QuantifiedFormula(FORALL,
                g->vars(),
                                g->sorts(),
                new BinaryFormula(IMP,g->subformula(),new AtomicFormula(a)));
      unit = new FormulaUnit(f,
                 new Inference1(Inference::ANSWER_LITERAL,unit),
                 Unit::CONJECTURE);
    }
    else {
      Formula::VarList* vs = f->freeVariables();
      if (Formula::VarList::isEmpty(vs)) {
    f = new NegatedFormula(f);
      }
      else {
        // TODO can we use sortOf to get the sorts of vs? 
    f = new NegatedFormula(new QuantifiedFormula(FORALL,vs,0,f));
      }
      unit = new FormulaUnit(f,
                 new Inference1(Inference::NEGATED_CONJECTURE,unit),
                 Unit::CONJECTURE);
    }
    break;

  case Unit::CLAIM:
    {
      bool added;
      unsigned pred = env.signature->addPredicate(nm,0,added);
      if (!added) {
    USER_ERROR("Names of claims must be unique: "+nm);
      }
      env.signature->getPredicate(pred)->markLabel();
      Literal* a = new(0) Literal(pred,0,true,false);
      a = env.sharing->insert(a);
      Formula* claim = new AtomicFormula(a);
      Formula::VarList* vs = f->freeVariables();
      if (Formula::VarList::isNonEmpty(vs)) {
        //TODO can we use sortOf to get sorts of vs?
    f = new QuantifiedFormula(FORALL,vs,0,f);
      }
      f = new BinaryFormula(IFF,claim,f);
      unit = new FormulaUnit(f,
                 new Inference1(Inference::CLAIM_DEFINITION,unit),
                 Unit::ASSUMPTION);
    }
    break;

  default:
    break;
  }
  _units.push(unit);
} // tag

/**
 * Add a state just reading a tag and save the tag in _tags.
 * @since 28/07/2011 Manchester
 */
void TPTP::addTagState(Tag t)
{
  CALL("TPTP::addTagState");
  _states.push(TAG);
  _tags.push(t);
} // TPTP::addTagState

/**
 * Process the end of the tff() definition and build the corresponding unit.
 * @since 14/07/2011 Manchester
 */
void TPTP::endTff()
{
  CALL("TPTP::endTff");

  int rpars= _ints.pop();
  while (rpars--) {
    consumeToken(T_RPAR);
  }
  skipToRPAR();
  consumeToken(T_DOT);

  // build a TPTP out of the parse type
  ASS(_typeTags.isEmpty());
  Type* t = _types.pop();
  ASS(_types.isEmpty());
  vstring name = _strings.pop();

  if (t->tag() == TT_PRODUCT) {
    USER_ERROR("product types are not supported");
  }


  //atomic types: 0-ary predicates (propositions) and constants (0-ary functions, eg. int constant, array1 constants)
  if (t->tag() == TT_ATOMIC) {
    unsigned sortNumber = static_cast<AtomicType*>(t)->sortNumber();
    bool added;
    if (sortNumber == Sorts::SRT_BOOL) {
      env.signature->addPredicate(name,0,added);
      if (!added) {
        USER_ERROR("Predicate symbol type is declared after its use: " + name);
      }
      return;
    }
    // a constant
    unsigned fun = addUninterpretedConstant(name,_overflow,added);
    if (!added) {
      USER_ERROR("Function symbol type is declared after its use: " + name);
    }
    env.signature->getFunction(fun)->setType(OperatorType::getConstantsType(sortNumber));
    return;
  }

  //non-atomic types, i.e. with arrows
  ASS(t->tag() == TT_ARROW);
  ArrowType* at = static_cast<ArrowType*>(t);
  Type* rhs = at->returnType();

  if (rhs->tag() != TT_ATOMIC) {
    USER_ERROR("complex return types are not supported");
  }


  unsigned returnSortNumber = static_cast<AtomicType*>(rhs)->sortNumber();
  Stack<unsigned> sorts;
  Stack<Type*> types; 
  types.push(at->argumentType());
  while (!types.isEmpty()) {
    Type* tp = types.pop();
    switch (tp->tag()) {
    case TT_ARROW:
      USER_ERROR("higher-order types are not supported");
    case TT_ATOMIC: {
      unsigned sortNumber = static_cast<AtomicType *>(tp)->sortNumber();
      sorts.push(sortNumber);
      break;
    }
    case TT_PRODUCT:
    {
      ProductType* pt = static_cast<ProductType*>(tp);
      types.push(pt->rhs());
      types.push(pt->lhs());
    }
    break;
#if VDEBUG
    default:
    ASSERTION_VIOLATION;
#endif
   }
  }
  
  unsigned arity = sorts.size();
  bool added;
  Signature::Symbol* symbol;
  if (returnSortNumber == Sorts::SRT_BOOL) {
    unsigned pred = env.signature->addPredicate(name,arity,added);
    if (!added) {
      USER_ERROR("Predicate symbol type is declared after its use: " + name);
    }
    symbol = env.signature->getPredicate(pred);
    symbol->setType(OperatorType::getPredicateType(arity, sorts.begin()));
  }
  else {
    unsigned fun = arity == 0
                   ? addUninterpretedConstant(name,_overflow,added)
                   : env.signature->addFunction(name,arity,added);
    if (!added) {
      USER_ERROR("Function symbol type is declared after its use: " + name);
    }
    symbol = env.signature->getFunction(fun);
    symbol->setType(OperatorType::getFunctionType(arity, sorts.begin(), returnSortNumber));
  }

} // endTff

/**
 *
 * @author Giles
 */
TPTP::SourceRecord* TPTP::getSource()
{
  if (getTok(0).tag != T_COMMA) { // if comma is not there, source was not provided
    return 0;
  }

  consumeToken(T_COMMA);

  //Either source is a file or an inference, otherwise we don't care about it!
  //  therefore failing will return 0
 
  Token& source_kind = getTok(0);
  if(source_kind.tag != T_NAME) return 0;

  resetToks();
  if (getTok(0).tag != T_LPAR) {
    return 0;
  } else {
    resetToks();
  }
  
  //file
  if(source_kind.content == "file"){
    vstring fileName = getTok(0).content;
    resetToks();
    consumeToken(T_COMMA);
    resetToks();
    vstring nameInFile = getTok(0).content;
    resetToks();

    // cout << "Creating file source record for " << fileName << " and " << nameInFile << endl;

    consumeToken(T_RPAR);
    return new FileSourceRecord(fileName,nameInFile);
  }
  // inference
  else if(source_kind.content == "inference" || source_kind.content == "introduced"){
    bool introduced = (source_kind.content == "introduced");
    vstring name = getTok(0).content;
    resetToks();

    // cout << "Creating inference source record for " << name <<  endl;

    InferenceSourceRecord* r = new InferenceSourceRecord(name);

    if(introduced){
      // then we don't expect names and we don't care about middle info 
      resetToks();
      skipToRPAR();
      return r;
    }

    // now skip this middle information that is between [ and ]
    consumeToken(T_COMMA);
    consumeToken(T_LBRA);
    skipToRBRA();
    consumeToken(T_COMMA);
    consumeToken(T_LBRA);

    // read comma separated list of names
    Token tok;
    while((tok=getTok(0)).tag != T_RBRA){
      resetToks();
      if(tok.tag == T_COMMA) continue;
   
      if (tok.tag != T_NAME && tok.tag != T_INT) {
        cout << "read token " << tok.tag << " with content " << tok.content << endl;

        // TODO: parse errors are nice, but maybe we just want to ignore any info which we cannot understand?

        PARSE_ERROR("Source unit name expected",tok);
      }

      vstring premise = tok.content;

      tok = getTok(0);
      if (tok.tag != T_COMMA && tok.tag != T_RBRA) {
        // if the next thing is neither comma not RBRA, it is an ugly info piece we want to skip
        resetToks();
        skipToRPAR();
      } else {
        r->premises.push(premise);
        // cout << "pushed premise " << premise << endl;
      }
    }
    resetToks();

    consumeToken(T_RPAR);
    return r;
  } else {
    
    skipToRPAR();
  }

  return 0;
}


/**
 * Skip any sequence tokens, including matching pairs of left parentheses,
 * until an unmatched right parenthesis is found. Consume this right parenthesis
 * and terminate.
 * @since 15/07/2011 Manchester
 */
void TPTP::skipToRPAR()
{
  int balance = 0;
  for (;;) {
    Token tok = getTok(0);
    switch (tok.tag) {
    case T_EOF:
      PARSE_ERROR(") not found",tok);
    case T_LPAR:
      resetToks();
      balance++;
      break;
    case T_RPAR:
      resetToks();
      balance--;
      if (balance == -1) {
    return;
      }
      break;
    default:
      resetToks();
      break;
    }
  }
} // skipToRPAR

/**
 * A copy of skipToRPAR but for BRA
 */
void TPTP::skipToRBRA()
{
  int balance = 0;
  for (;;) {
    Token tok = getTok(0);
    switch (tok.tag) {
    case T_EOF:
      PARSE_ERROR(") not found",tok);
    case T_LBRA:
      resetToks();
      balance++;
      break;
    case T_RBRA:
      resetToks();
      balance--;
      if (balance == -1) {
        return;
      }
      break;
    default:
      resetToks();
      break;
    }
  }
} // skipToRBRA

/**
 * Read a simple formula (quantified formula, negation,
 * formula in parentheses, true or false).
 * @since 10/04/2011 Manchester
 */
void TPTP::simpleFormula()
{
  CALL("TPTP::simpleFormula");

  Token tok = getTok(0);

  switch (tok.tag) {
  case T_NOT:
    resetToks();
    _connectives.push(NOT);
    _states.push(SIMPLE_FORMULA);
    return;

  case T_FORALL:
  case T_EXISTS:
    resetToks();
    consumeToken(T_LBRA);
    _connectives.push(tok.tag == T_FORALL ? FORALL : EXISTS);
    _states.push(UNBIND_VARIABLES);
    _states.push(SIMPLE_FORMULA);
    addTagState(T_COLON);
    addTagState(T_RBRA);
    _states.push(VAR_LIST);
    return;

  case T_LPAR:
    resetToks();
    addTagState(T_RPAR);
    _states.push(FORMULA);
    return;

  case T_STRING:
  case T_INT:
  case T_RAT:
  case T_REAL:
    _states.push(END_EQ);
    _states.push(TERM);
    _states.push(MID_EQ);
    _states.push(TERM);
    return;
  case T_TRUE:
    resetToks();
    _formulas.push(new Formula(true));
    return;
  case T_FALSE:
    resetToks();
    _formulas.push(new Formula(false));
    return;
  case T_NAME:
  case T_VAR:
  case T_ITE:
  case T_THEORY_FUNCTION:
  case T_LET:
  case T_LBRA:
    _states.push(FORMULA_INFIX);
    _states.push(FUN_APP);
    return;
  default:
    PARSE_ERROR("formula or term expected",tok);
  }
} // simpleFormula

/**
 * Unbind variable sort binding.
 * @since 14/07/2011 Manchester
 */
void TPTP::unbindVariables()
{
  CALL("TPTP::unbindVariables");

  Formula::VarList* varlist = _bindLists.pop();
  
  Formula::VarList::Iterator vs(varlist);
  while (vs.hasNext()) {//really messy. Need to clean up different maps.
    int var = vs.next();
    TypeList* types;
    SortList* sorts;
    BindList* binders;
    ALWAYS(_variableSorts.find(var,sorts));
    _variableSorts.replace(var,sorts->tail());
    ALWAYS(_varTypes.find(var,types)); 
    _varTypes.replace(var, types->tail()); 
    ALWAYS(_varBinders.find(var,binders));
    _varBinders.replace(var,binders->tail());
    if(binders->head() != LAMB && types->head()->arity() != 0){ //higher-order var, not index
      FuncList* functors;
      ALWAYS(_varFunctors.find(var, functors));
      _varFunctors.replace(var, functors->tail());
    }
  }
} // unbindVariables

/**
 * Read a simple type: name or type in parentheses
 * @since 14/07/2011 Manchester
 */
void TPTP::simpleType()
{
  CALL("TPTP::simpleType");

  Token& tok = getTok(0);
  if (tok.tag == T_LPAR) {
    resetToks();
    addTagState(T_RPAR);
    _states.push(TYPE);
    return;
  }
  _types.push(new AtomicType(readSort()));
} // simpleType


/**
 * Read a HOL sort and return its number 
 * @since 10/11/2017 Leicester
 * @author Ahmed Bhayat
 */
 
Stack<unsigned> TPTP::readHOLSort()
{
   CALL("TPTP::readHOLSort");

   int inBrackets = 0;
   Stack<int> subSorts;
   Token tok = getTok(0);
   unsigned sort;
   while((tok.tag != T_COMMA) & (tok.tag != T_RBRA)){
       switch(tok.tag){
           case T_LPAR: //This will need changing when we read tuple types - AYB
              subSorts.push(-1);
              inBrackets += 1;
              break;
           case T_ARROW:
              break;
           case T_RPAR:
              inBrackets -= 1;
              if(inBrackets < 0){
                _gpos = 0;                
                return convertToUnsigned(subSorts);
              }
              foldl(&subSorts);
              break;
           default:{
              sort = readSort();
              subSorts.push(sort);               
           }
       }
       resetToks();
       tok = getTok(0);
   }
   return convertToUnsigned(subSorts);
}
 
Stack<unsigned> TPTP::convertToUnsigned(Stack<int> sorts){
   CALL("convertToUnsigned");
   
   Stack<unsigned> converted;
   for(unsigned i = 0; i < sorts.size(); i ++){
     converted.push((unsigned)sorts[i]);
   }
   return converted;
}  
 
void TPTP::foldl(Stack<int>* sorts)
{
   CALL("TPTP::foldl");
   
   int item1 = sorts->pop();
   int item2 = sorts->pop();
   while(!(sorts->isEmpty()) & (item2 != -1)){
       item1 = env.sorts->addFunctionSort((unsigned)item2, (unsigned)item1);
       item2 = sorts->pop();
   }
   if (item2 != -1){
       item1 = env.sorts->addFunctionSort((unsigned)item2, (unsigned)item1);
   }
   sorts->push(item1);
}   

/* hacky method used in readVar(). To be removed ASAP, AYB */
unsigned TPTP::foldl(Stack<unsigned> sorts)
{
   CALL("TPTP::foldl(Stack<unsigned>)");
   
   if(sorts.size() == 1){
     return sorts.pop();
   }
   unsigned item1 = sorts.pop();
   unsigned item2 = sorts.pop();
   while(!(sorts.isEmpty())){
       item1 = env.sorts->addFunctionSort(item2, item1);
       item2 = sorts.pop();
   }
   item1 = env.sorts->addFunctionSort(item2, item1);
   return item1;
}  
 
/**
 * Read a sort and return its number. If a sort is not built-in, then raise an
 * exception if it has been declared and newSortExpected, or it has not been
 * declared and newSortExpected is false.
 * @since 14/07/2011 Manchester
 */
unsigned TPTP::readSort()
{
  CALL("TPTP::readSort");

  Token tok = getTok(0);
  resetToks();
  switch (tok.tag) {
  case T_NAME:
    {
      bool added;
      unsigned sortNumber = env.sorts->addSort(tok.content,added,false);
      if (added) {
        PARSE_ERROR("undeclared sort",tok);
      }
      return sortNumber;
    }

  case T_DEFAULT_TYPE:
    return Sorts::SRT_DEFAULT;

  case T_BOOL_TYPE:
    return Sorts::SRT_BOOL;

  case T_INTEGER_TYPE:
    return Sorts::SRT_INTEGER;

  case T_RATIONAL_TYPE:
    return Sorts::SRT_RATIONAL;

  case T_REAL_TYPE:
    return Sorts::SRT_REAL;

  case T_LBRA:
  {
    Stack<unsigned> sorts;
    for (;;) {
      unsigned sort = readSort();
      sorts.push(sort);
      if (getTok(0).tag == T_COMMA) {
        resetToks();
      } else {
        consumeToken(T_RBRA);
        break;
      }
    }

    if (sorts.length() < 2) {
      USER_ERROR("Tuple sort with less than two arguments");
    }

    return env.sorts->addTupleSort((unsigned) sorts.length(), sorts.begin());
  }
  case T_THEORY_SORT: {
    unsigned sort;
    consumeToken(T_LPAR);
    switch (getTheorySort(tok)) {
      case TS_ARRAY: {
        unsigned indexSort = readSort();
        consumeToken(T_COMMA);
        unsigned innerSort = readSort();
        sort = env.sorts->addArraySort(indexSort, innerSort);
        break;
      }
      default:
        ASSERTION_VIOLATION;
    }
    consumeToken(T_RPAR);
    return sort;
  }
  default:
    PARSE_ERROR("sort expected",tok);
  }
} // readSort

/**
 * True if c1 has a strictly higher priority than c2.
 * @since 07/07/2011 Manchester
 */
bool TPTP::higherPrecedence(int c1,int c2)
{
  if (c1 == APP) return true;
  if (c1 == c2) return false;
  if (c1 == -1) return false;
  if (c2 == IFF) return true;
  if (c1 == IFF) return false;
  if (c2 == XOR) return true;
  if (c1 == XOR) return false;
  if (c2 == IMP) return true;
  if (c1 == IMP) return false;
  if (c2 == OR) return true;
  if (c1 == OR) return false;
  ASSERTION_VIOLATION;
} // higherPriority

bool TPTP::findInterpretedPredicate(vstring name, unsigned arity) {
  CALL("TPTP::findInterpretedPredicate");

  if (name == "$evaleq" || name == "$equal" || name == "$distinct") {
    return true;
  }

  if (name == "$is_int" || name == "$is_rat") {
    return arity == 1;
  }

  if (name == "$less" || name == "$lesseq" || name == "$greater" || name == "$greatereq" || name == "$divides") {
    return arity == 2;
  }

  return false;
}

/**
 * Create an and- or or-formula flattening its lhs and rhs if necessary.
 * @since 07/07/2011 Manchester
 */
Formula* TPTP::makeJunction (Connective c,Formula* lhs,Formula* rhs)
{
  if (lhs->connective() == c) {
    FormulaList* largs = lhs->args();

    if (rhs->connective() == c) {
      FormulaList::concat(largs,rhs->args());
      delete static_cast<JunctionFormula*>(rhs);
      return lhs;
    }
    // only lhs has c as the main connective
    FormulaList::concat(largs,new FormulaList(rhs));
    return lhs;
  }
  // lhs' connective is not c
  if (rhs->connective() == c) {
    static_cast<JunctionFormula*>(rhs)->setArgs(new FormulaList(lhs,
                                rhs->args()));
    return rhs;
  }
  // both connectives are not c
  return new JunctionFormula(c,
                 new FormulaList(lhs,
                         new FormulaList(rhs)));
} // makeJunction

/** Add a function to the signature
 * @param name the function name
 * @param arity the function arity
 * @param added if the function is new, will be assigned true, otherwise false
 * @param arg some argument of the function, require to resolve its type for overloaded
 *        built-in functions
 */
unsigned TPTP::addFunction(vstring name,int arity,bool& added,TermList& arg)
{
  CALL("TPTP::addFunction");

  if (name == "$sum") {
    return addOverloadedFunction(name,arity,2,added,arg,
                 Theory::INT_PLUS,
                 Theory::RAT_PLUS,
                 Theory::REAL_PLUS);
  }
  if (name == "$difference") {
    return addOverloadedFunction(name,arity,2,added,arg,
                 Theory::INT_MINUS,
                 Theory::RAT_MINUS,
                 Theory::REAL_MINUS);
  }
  if (name == "$product") {
    return addOverloadedFunction(name,arity,2,added,arg,
                 Theory::INT_MULTIPLY,
                 Theory::RAT_MULTIPLY,
                 Theory::REAL_MULTIPLY);
  }
  // An odd leftover, maps to the 'most natural' kind of division
  if (name == "$divide") {
    return addOverloadedFunction(name,arity,2,added,arg,
                 Theory::INT_QUOTIENT_E,
                 Theory::RAT_QUOTIENT,
                 Theory::REAL_QUOTIENT);
  }
  if (name == "$modulo"){
    if(sortOf(arg)!=Sorts::SRT_INTEGER){
      USER_ERROR("$modulo can only be used with integer type");
    }
    return addOverloadedFunction(name,arity,2,added,arg,
                                 Theory::INT_REMAINDER_E,  // $modulo is the always positive remainder, therefore INT_REMAINDER_E
                                 Theory::INT_REMAINDER_E,  // will not be used
                                 Theory::INT_REMAINDER_E); // will not be used
  }
  if (name == "$abs"){
    if(sortOf(arg)!=Sorts::SRT_INTEGER){
      USER_ERROR("$abs can only be used with integer type");
    }
    return addOverloadedFunction(name,arity,1,added,arg,
                                 Theory::INT_ABS,
                                 Theory::INT_ABS,  // will not be used
                                 Theory::INT_ABS); // will not be used
  }
  if (name == "$quotient") {
    if(sortOf(arg)==Sorts::SRT_INTEGER){
      USER_ERROR("$quotient cannot be used with integer type");
    }
    return addOverloadedFunction(name,arity,2,added,arg,
                                 Theory::INT_QUOTIENT_E,// this is a dummy
                                 Theory::RAT_QUOTIENT,
                                 Theory::REAL_QUOTIENT);
  }
  if (name == "$quotient_e") {
    return addOverloadedFunction(name,arity,2,added,arg,
                                 Theory::INT_QUOTIENT_E,
                                 Theory::RAT_QUOTIENT_E,
                                 Theory::REAL_QUOTIENT_E);
  }
  if (name == "$quotient_t") {
    return addOverloadedFunction(name,arity,2,added,arg,
                                 Theory::INT_QUOTIENT_T,
                                 Theory::RAT_QUOTIENT_T,
                                 Theory::REAL_QUOTIENT_T);
  }
  if (name == "$quotient_f") {
    return addOverloadedFunction(name,arity,2,added,arg,
                                 Theory::INT_QUOTIENT_F,
                                 Theory::RAT_QUOTIENT_F,
                                 Theory::REAL_QUOTIENT_F);
  }
  if (name == "$remainder_e") {
    return addOverloadedFunction(name,arity,2,added,arg,
                                 Theory::INT_REMAINDER_E,
                                 Theory::RAT_REMAINDER_E,
                                 Theory::REAL_REMAINDER_E);
  }
  if (name == "$remainder_t") {
    return addOverloadedFunction(name,arity,2,added,arg,
                                 Theory::INT_REMAINDER_T,
                                 Theory::RAT_REMAINDER_T,
                                 Theory::REAL_REMAINDER_T);
  }
  if (name == "$remainder_f") {
    return addOverloadedFunction(name,arity,2,added,arg,
                                 Theory::INT_REMAINDER_F,
                                 Theory::RAT_REMAINDER_F,
                                 Theory::REAL_REMAINDER_F);
  }
  if (name == "$uminus") {
    return addOverloadedFunction(name,arity,1,added,arg,
                 Theory::INT_UNARY_MINUS,
                 Theory::RAT_UNARY_MINUS,
                 Theory::REAL_UNARY_MINUS);
  }
  if (name == "$successor"){
    if(sortOf(arg)!=Sorts::SRT_INTEGER){
      USER_ERROR("$succ can only be used with integer type");
    }
    return addOverloadedFunction(name,arity,1,added,arg,
                                 Theory::INT_SUCCESSOR,
                                 Theory::INT_SUCCESSOR,  // will not be used
                                 Theory::INT_SUCCESSOR); // will not be used
  }
  if (name == "$floor") {
    return addOverloadedFunction(name,arity,1,added,arg,
                                 Theory::INT_FLOOR,
                                 Theory::RAT_FLOOR,
                                 Theory::REAL_FLOOR);
  }
  if (name == "$ceiling") {
    return addOverloadedFunction(name,arity,1,added,arg,
                                 Theory::INT_CEILING,
                                 Theory::RAT_CEILING,
                                 Theory::REAL_CEILING);
  }
  if (name == "$truncate") {
    return addOverloadedFunction(name,arity,1,added,arg,
                                 Theory::INT_TRUNCATE,
                                 Theory::RAT_TRUNCATE,
                                 Theory::REAL_TRUNCATE);
  }
  if (name == "$round") {
    return addOverloadedFunction(name,arity,1,added,arg,
                                 Theory::INT_ROUND,
                                 Theory::RAT_ROUND,
                                 Theory::REAL_ROUND);
  }
  if (name == "$to_int") {
    return addOverloadedFunction(name,arity,1,added,arg,
                 Theory::INT_TO_INT,
                 Theory::RAT_TO_INT,
                 Theory::REAL_TO_INT);
  }
  if (name == "$to_rat") {
    return addOverloadedFunction(name,arity,1,added,arg,
                 Theory::INT_TO_RAT,
                 Theory::RAT_TO_RAT,
                 Theory::REAL_TO_RAT);
  }
  if (name == "$to_real") {
    return addOverloadedFunction(name,arity,1,added,arg,
                 Theory::INT_TO_REAL,
                 Theory::RAT_TO_REAL,
                 Theory::REAL_TO_REAL);
  }

  if (arity > 0) {
    return env.signature->addFunction(name,arity,added);
  }
  return addUninterpretedConstant(name,_overflow,added);
} // addFunction

/** Add a predicate to the signature
 * @param name the predicate name
 * @param arity the predicate arity
 * @param added if the predicate is new, will be assigned true, otherwise false
 * @param arg some argument of the predicate, require to resolve its type for overloaded
 *        built-in predicates
 * @return the predicate number in the signature, or -1 if it is a different name for an equality
 *         predicate
 */
int TPTP::addPredicate(vstring name,int arity,bool& added,TermList& arg)
{
  CALL("TPTP::addPredicate");

  if (name == "$evaleq" || name == "$equal") {
    return -1;
  }
  if (name == "$less") {
    return addOverloadedPredicate(name,arity,2,added,arg,
                  Theory::INT_LESS,
                  Theory::RAT_LESS,
                  Theory::REAL_LESS);
  }
  if (name == "$lesseq") {
    return addOverloadedPredicate(name,arity,2,added,arg,
                  Theory::INT_LESS_EQUAL,
                  Theory::RAT_LESS_EQUAL,
                  Theory::REAL_LESS_EQUAL);
  }
  if (name == "$greater") {
    return addOverloadedPredicate(name,arity,2,added,arg,
                  Theory::INT_GREATER,
                  Theory::RAT_GREATER,
                  Theory::REAL_GREATER);
  }
  if (name == "$greatereq") {
    return addOverloadedPredicate(name,arity,2,added,arg,
                  Theory::INT_GREATER_EQUAL,
                  Theory::RAT_GREATER_EQUAL,
                  Theory::REAL_GREATER_EQUAL);
  }
  if (name == "$is_int") {
    return addOverloadedPredicate(name,arity,1,added,arg,
                  Theory::INT_IS_INT,
                  Theory::RAT_IS_INT,
                  Theory::REAL_IS_INT);
  }
  if (name == "$divides"){
    if(sortOf(arg)!=Sorts::SRT_INTEGER){
      USER_ERROR("$divides can only be used with integer type");
    }
    return addOverloadedPredicate(name,arity,2,added,arg,
                                  Theory::INT_DIVIDES,
                                  Theory::INT_DIVIDES,  // will not be used
                                  Theory::INT_DIVIDES); // will not be used
  }
  if (name == "$is_rat") {
    return addOverloadedPredicate(name,arity,1,added,arg,
                  Theory::INT_IS_RAT,
                  Theory::RAT_IS_RAT,
                  Theory::REAL_IS_RAT);
  }
  if(name == "$distinct"){
    // special case for distinct, dealt with in formulaInfix
    return -2;
  }
  return env.signature->addPredicate(name,arity,added);
} // addPredicate


unsigned TPTP::addOverloadedFunction(vstring name,int arity,int symbolArity,bool& added,TermList& arg,
                     Theory::Interpretation integer,Theory::Interpretation rational,
                     Theory::Interpretation real)
{
  CALL("TPTP::addOverloadedFunction");

  if (arity != symbolArity) {
    USER_ERROR(name + " is used with " + Int::toString(arity) + " argument(s)");
  }
  unsigned srt = sortOf(arg);
  TermList* n = arg.next();
  for(int i=1;i<arity;i++){
    if(sortOf(*n)!=srt) USER_ERROR((vstring)"The symbol " + name + " is not used with a single sort");
    n = n->next();
  }
  if (srt == Sorts::SRT_INTEGER) {
    return env.signature->addInterpretedFunction(integer,name);
  }
  if (srt == Sorts::SRT_RATIONAL) {
    return env.signature->addInterpretedFunction(rational,name);
  }
  if (srt == Sorts::SRT_REAL) {
    return env.signature->addInterpretedFunction(real,name);
  }
  USER_ERROR((vstring)"The symbol " + name + " is used with a non-numeric type");
} // addOverloadedFunction

unsigned TPTP::addOverloadedPredicate(vstring name,int arity,int symbolArity,bool& added,TermList& arg,
                     Theory::Interpretation integer,Theory::Interpretation rational,
                     Theory::Interpretation real)
{
  CALL("TPTP::addOverloadedPredicate");

  if (arity != symbolArity) {
    USER_ERROR(name + " is used with " + Int::toString(arity) + " argument(s)");
  }
  unsigned srt = sortOf(arg);
  TermList* n = arg.next();
  for(int i=1;i<arity;i++){
    if(sortOf(*n)!=srt) USER_ERROR((vstring)"The symbol " + name + " is not used with a single sort");
    n = n->next(); 
  }
  
  if (srt == Sorts::SRT_INTEGER) {
    return env.signature->addInterpretedPredicate(integer,name);
  }
  if (srt == Sorts::SRT_RATIONAL) {
    return env.signature->addInterpretedPredicate(rational,name);
  }
  if (srt == Sorts::SRT_REAL) {
    return env.signature->addInterpretedPredicate(real,name);
  }
  USER_ERROR((vstring)"The symbol " + name + " is used with a non-numeric type");
} // addOverloadedPredicate

/**
 * Return the sort of the term.
 * @since 29/07/2011 Manchester
 * @since 03/05/2013 train Manchester-London bug fix
 * @author Andrei Voronkov
 */
unsigned TPTP::sortOf(TermList t)
{
  CALL("TPTP::sortOf");

  for (;;) {
    if (t.isVar()) {
      SortList* sorts;
      if (_variableSorts.find(t.var(),sorts) && SortList::isNonEmpty(sorts)) {
        return sorts->head();
      }
      // there might be variables whose sort is undeclared,
      // in this case they have the default sort
      bindVariable(t.var(),Sorts::SRT_DEFAULT);
      return Sorts::SRT_DEFAULT;
    }
    unsigned sort;
    TermList mvar;
    if (SortHelper::getResultSortOrMasterVariable(t.term(), sort, mvar)) {
      return sort;
    } else {
      t = mvar;
    }
  }
} // sortOf

/**
 * Add an integer constant by reading it from the vstring name.
 * If it overflows, create an uninterpreted constant of the
 * integer type and the name 'name'. Check that the name of the constant
 * does not collide with user-introduced names of uninterpreted constants.
 * @since 22/07/2011 Manchester
 * @since 03/05/2013 train Manchester-London, bug fix: integers are treated
 *   as terms of the default sort when fof() or cnf() is used
 * @author Andrei Voronkov
 */
unsigned TPTP::addIntegerConstant(const vstring& name, Set<vstring>& overflow, bool defaultSort)
{
  CALL("TPTP::addIntegerConstant");

  try {
    return env.signature->addIntegerConstant(name,defaultSort);
  }
  catch (Kernel::ArithmeticException&) {
    bool added;
    unsigned fun = env.signature->addFunction(name,0,added,true /* overflown constant*/);
    if (added) {
      overflow.insert(name);
      Signature::Symbol* symbol = env.signature->getFunction(fun);
      symbol->setType(OperatorType::getConstantsType(defaultSort ? Sorts::SRT_DEFAULT : Sorts::SRT_INTEGER));
    }
    else if (!overflow.contains(name)) {
      USER_ERROR((vstring)"Cannot use name '" + name + "' as an atom name since it collides with an integer number");
    }
    return fun;
  }
} // TPTP::addIntegerConstant

/**
 * Add an rational constant by reading it from the vstring name.
 * If it overflows, create an uninterpreted constant of the
 * rational type and the name 'name'. Check that the name of the constant
 * does not collide with user-introduced names of uninterpreted constants.
 * @since 22/07/2011 Manchester
 * @since 03/05/2013 train Manchester-London, fix to handle difference
 *    between treating rationals using fof() and tff()
 * @author Andrei Voronkov
 */
unsigned TPTP::addRationalConstant(const vstring& name, Set<vstring>& overflow, bool defaultSort)
{
  CALL("TPTP::addRationalConstant");

  size_t i = name.find_first_of("/");
  ASS(i != vstring::npos);
  try {
    return env.signature->addRationalConstant(name.substr(0,i),
                          name.substr(i+1),
                          defaultSort);
  }
  catch(Kernel::ArithmeticException&) {
    bool added;
    unsigned fun = env.signature->addFunction(name,0,added,true /* overflown constant*/);
    if (added) {
      overflow.insert(name);
      Signature::Symbol* symbol = env.signature->getFunction(fun);
      symbol->setType(OperatorType::getConstantsType(defaultSort ? Sorts::SRT_DEFAULT : Sorts::SRT_RATIONAL));
    }
    else if (!overflow.contains(name)) {
      USER_ERROR((vstring)"Cannot use name '" + name + "' as an atom name since it collides with an rational number");
    }
    return fun;
  }
} // TPTP::addRationalConstant

/**
 * Add an real constant by reading it from the vstring name.
 * If it overflows, create an uninterpreted constant of the
 * real type and the name 'name'. Check that the name of the constant
 * does not collide with user-introduced names of uninterpreted constants.
 * @since 22/07/2011 Manchester
 * @since 03/05/2013 train Manchester-London, fix to handle difference
 *    between treating rationals using fof() and tff()
 * @author Andrei Voronkov
 */
unsigned TPTP::addRealConstant(const vstring& name, Set<vstring>& overflow, bool defaultSort)
{
  CALL("TPTP::addRealConstant");

  try {
    return env.signature->addRealConstant(name,defaultSort);
  }
  catch(Kernel::ArithmeticException&) {
    bool added;
    unsigned fun = env.signature->addFunction(name,0,added,true /* overflown constant*/);
    if (added) {
      overflow.insert(name);
      Signature::Symbol* symbol = env.signature->getFunction(fun);
      symbol->setType(OperatorType::getConstantsType(defaultSort ? Sorts::SRT_DEFAULT : Sorts::SRT_REAL));
    }
    else if (!overflow.contains(name)) {
      USER_ERROR((vstring)"Cannot use name '" + name + "' as an atom name since it collides with an real number");
    }
    return fun;
  }
} // TPTP::addRealConstant


/**
 * Add an uninterpreted constant by reading it from the vstring name.
 * Check that the name of the constant does not collide with uninterpreted constants
 * created by the parser from overflown input numbers.
 * @since 22/07/2011 Manchester
 */
unsigned TPTP::addUninterpretedConstant(const vstring& name, Set<vstring>& overflow, bool& added)
{
  CALL("TPTP::addUninterpretedConstant");

  if (overflow.contains(name)) {
    USER_ERROR((vstring)"Cannot use name '" + name + "' as an atom name since it collides with an integer number");
  }
  return env.signature->addFunction(name,0,added);
} // TPTP::addUninterpretedConstant

/**
 * Associate name @b name with unit @b unit
 * Each formula can have its name assigned at most once
 */
void TPTP::assignAxiomName(const Unit* unit, vstring& name)
{
  CALL("Parser::assignAxiomName");
  ALWAYS(_axiomNames.insert(unit->number(), name));
} // TPTP::assignAxiomName

/**
 * If @b unit has a name associated, assign it into @b result,
 * and return true; otherwise return false
 */
bool TPTP::findAxiomName(const Unit* unit, vstring& result)
{
  CALL("Parser::findAxiomName");
  return _axiomNames.find(unit->number(), result);
} // TPTP::findAxiomName

/**
 * Process vampire() declaration
 * @since 25/08/2009 Redmond
 */
void TPTP::vampire()
{
  CALL("TPTP::vampire");

  consumeToken(T_LPAR);
  vstring nm = name();

  if (nm == "option") { // vampire(option,age_weight_ratio,3)
    consumeToken(T_COMMA);
    vstring opt = name();
    consumeToken(T_COMMA);
    Token tok = getTok(0);
    switch (tok.tag) {
    case T_INT:
    case T_REAL:
    case T_NAME:
      env.options->set(opt,tok.content);
      resetToks();
      break;
    default:
      PARSE_ERROR("either atom or number expected as a value of a Vampire option",tok);
    }
  }
  // Allows us to insert LaTeX templates for predicate and function symbols
  else if(nm == "latex"){
    consumeToken(T_COMMA);
    vstring kind = name();
    bool pred;
    if (kind == "predicate") {
      pred = true;
    }
    else if (kind == "function") {
      pred = false;
    }
    else {
      PARSE_ERROR("either 'predicate' or 'function' expected",getTok(0));
    }
    consumeToken(T_COMMA);
    vstring symb = name();
    consumeToken(T_COMMA);
    Token tok = getTok(0);
    if (tok.tag != T_INT) {
      PARSE_ERROR("a non-negative integer (denoting arity) expected",tok);
    }
    unsigned arity;
    if (!Int::stringToUnsignedInt(tok.content,arity)) {
      PARSE_ERROR("a number denoting arity expected",tok);
    }
    resetToks();
    consumeToken(T_COMMA);
    tok = getTok(0);
    if(tok.tag != T_STRING){
      PARSE_ERROR("a template string expected",tok);
    }
    vstring temp = tok.content;
    resetToks();
    if(pred){
      consumeToken(T_COMMA);
      vstring pol= name();
      bool polarity;
      if(pol=="true"){polarity=true;}else if(pol=="false"){polarity=false;}
      else{ PARSE_ERROR("polarity expected (true/false)",getTok(0)); }
      unsigned f = env.signature->addPredicate(symb,arity);
      theory->registerLaTeXPredName(f,polarity,temp);
    }
    else{
      unsigned f = env.signature->addFunction(symb,arity);
      theory->registerLaTeXFuncName(f,temp);
    }
  }
  else if (nm == "symbol") {
    consumeToken(T_COMMA);
    vstring kind = name();
    bool pred;
    if (kind == "predicate") {
      pred = true;
    }
    else if (kind == "function") {
      pred = false;
    }
    else {
      PARSE_ERROR("either 'predicate' or 'function' expected",getTok(0));
    }
    consumeToken(T_COMMA);
    vstring symb = name();
    consumeToken(T_COMMA);
    Token tok = getTok(0);
    if (tok.tag != T_INT) {
      PARSE_ERROR("a non-negative integer (denoting arity) expected",tok);
    }
    unsigned arity;
    if (!Int::stringToUnsignedInt(tok.content,arity)) {
      PARSE_ERROR("a number denoting arity expected",tok);
    }
    resetToks();
    consumeToken(T_COMMA);
    Color color;
    bool skip = false;
    vstring lr = name();
    if (lr == "left") {
      color=COLOR_LEFT;
    }
    else if (lr == "right") {
      color=COLOR_RIGHT;
    }
    else if (lr == "skip") {
      skip = true;
    }
    else {
      PARSE_ERROR("'left', 'right' or 'skip' expected",getTok(0));
    }
    env.colorUsed = true;
    Signature::Symbol* sym = pred
                             ? env.signature->getPredicate(env.signature->addPredicate(symb,arity))
                             : env.signature->getFunction(env.signature->addFunction(symb,arity));
    if (skip) {
      sym->markSkip();
    }
    else {
      sym->addColor(color);
    }
  }
  else if (nm == "left_formula") { // e.g. vampire(left_formula)
    _currentColor = COLOR_LEFT;
  }
  else if (nm == "right_formula") { // e.g. vampire(left_formula)
    _currentColor = COLOR_RIGHT;
  }
  else if (nm == "end_formula") { // e.g. vampire(left_formula)
    _currentColor = COLOR_TRANSPARENT;
  }
  else if (nm == "model_check"){
    consumeToken(T_COMMA);
    vstring command = name();
    if(command == "formulas_start"){
      _modelDefinition = false;
    }
    else if(command == "formulas_end"){
      // do nothing
    }
    else if(command == "model_start"){
      _modelDefinition = true;
    }
    else if(command == "model_end"){
      _modelDefinition = false;
    }
    else USER_ERROR("Unknown model_check command");
  }
  else {
    USER_ERROR((vstring)"Unknown vampire directive: "+nm);
  }
  consumeToken(T_RPAR);
  consumeToken(T_DOT);
} // vampire

#if VDEBUG
const char* TPTP::toString(State s)
{
  switch (s) {
  case UNIT_LIST:
    return "UNIT_LIST";
  case CNF:
    return "CNF";
  case FOF:
    return "FOF";
  case VAMPIRE:
    return "VAMPIRE";
  case FORMULA:
    return "FORMULA";
  case END_FOF:
    return "END_FOF";
  case SIMPLE_FORMULA:
    return "SIMPLE_FORMULA";
  case END_FORMULA:
    return "END_FORMULA";
  case FORMULA_INSIDE_TERM:
    return "FORMULA_INSIDE_TERM";
  case END_FORMULA_INSIDE_TERM:
    return "END_FORMULA_INSIDE_TERM";
  case END_TERM_AS_FORMULA:
    return "END_TERM_AS_FORMULA";
  case VAR_LIST:
    return "VAR_LIST";
  case FUN_APP:
    return "FUN_APP";
  case FORMULA_INFIX:
    return "FORMULA_INFIX";
  case ARGS:
    return "ARGS";
  case TERM:
    return "TERM";
  case TERM_INFIX:
    return "TERM_INFIX";
  case END_TERM:
    return "END_TERM";
  case TAG:
    return "TAG";
  case INCLUDE:
    return "INCLUDE";
  case END_EQ:
    return "END_EQ";
  case TFF:
    return "TFF";
  case THF:
    return "THF";
  case TYPE:
    return "TYPE";
  case END_TFF:
    return "END_TFF";
  case HOL_FUNCTION:
    return "HOL_FUNCTION";
  case END_HOL_FUNCTION:
    return "END_HOL_FUNCTION";
  case HOL_TERM:
    return "HOL_TERM";
  case END_HOL_TERM:
    return "END_HOL_TERM";
  case HOL_SUB_TERM:
    return "HOL_SUB_TERM";
  case END_TYPE:
    return "END_TYPE";
  case SIMPLE_TYPE:
    return "SIMPLE_TYPE";
  case END_THEORY_FUNCTION:
    return "END_THEORY_FUNCTION";
  case END_ARGS:
    return "END_ARGS";
  case MID_EQ:
    return "MID_EQ";
  case BINDING:
    return "BINDING";
  case TUPLE_BINDING:
    return "TUPLE_BINDING";
  case END_BINDING:
    return "END_BINDING";
  case END_TUPLE_BINDING:
    return "END_TUPLE_BINDING";
  case END_LET:
    return "END_LET";
  case UNBIND_VARIABLES:
    return "UNBIND_VARIABLES";
  case END_ITE:
    return "END_ITE";
  case END_TUPLE:
    return "END_TUPLE";
  default:
    cout << (int)s << "\n";
    ASS(false);
    break;
  }
}
#endif

#ifdef DEBUG_SHOW_STATE
void TPTP::printStacks() {
  Stack<State>::Iterator stit(_states);
  cout << "States:";
  if   (!stit.hasNext()) cout << " <empty>";
  while (stit.hasNext()) cout << " " << toString(stit.next());
  cout << endl;

  Stack<Type*>::Iterator tyit(_types);
  cout << "Types:";
  if   (!tyit.hasNext()) cout << " <empty>";
  while (tyit.hasNext()) cout << " " << tyit.next()->tag();
  cout << endl;

  Stack<TypeTag>::Iterator ttit(_typeTags);
  cout << "Types Tags:";
  if   (!ttit.hasNext()) cout << " <empty>";
  while (ttit.hasNext()){
      cout << " ";
      TypeTag tt = ttit.next();
      switch(tt){
          case TT_ATOMIC: cout << "TT_ATOMIC"; break;        
          case TT_PRODUCT: cout << "TT_PRODUCT"; break;
          case TT_ARROW: cout << "TT_ARROW"; break;
      }
  }
  cout << endl;
  
  
  Stack<int>::Iterator cit(_connectives);
  cout << "Connectives:";
  if   (!cit.hasNext()) cout << " <empty>";
  while (cit.hasNext()) cout << " " << cit.next();
  cout << endl;

  Stack<vstring>::Iterator sit(_strings);
  cout << "Strings:";
  if   (!sit.hasNext()) cout << " <empty>";
  while (sit.hasNext()) cout << " " << sit.next();
  cout << endl;

  Stack<int>::Iterator iit(_ints);
  cout << "Ints:";
  if   (!iit.hasNext()) cout << " <empty>";
  while (iit.hasNext()) cout << " " << iit.next();
  cout << endl;

  Stack<int>::Iterator ags(_argsSoFar);
  cout << "argsSoFar:";
  if   (!ags.hasNext()) cout << " <empty>";
  while (ags.hasNext()) cout << " " << ags.next();
  cout << endl;
  
  /*
  Stack<Tag>::Iterator tags(_tags);
  cout << "Tags:";
  if (!tags.hasNext()) cout << " <empty>";
  while (tags.hasNext()) cout << " " << tags.next();
  cout << endl;
  */
  
  Stack<bool>::Iterator bit(_bools);
  cout << "Bools:";
  if   (!bit.hasNext()) cout << " <empty>";
  while (bit.hasNext()) cout << " " << bit.next();
  cout << endl;
  
  Stack<TermList>::Iterator tit(_termLists);
  cout << "Terms:";
  if   (!tit.hasNext()) cout << " <empty>";
  while (tit.hasNext()) cout << " " << tit.next().toString();
  cout << endl;

  Stack<Formula*>::Iterator fit(_formulas);
  cout << "Formulas:";
  if   (!fit.hasNext()) cout << " <empty>";
  while (fit.hasNext()) cout << " " << fit.next()->toString();
  cout << endl;

  Stack<Formula::VarList*>::Iterator vlit(_varLists);
  cout << "Var lists:";
  if   (!vlit.hasNext()) cout << " <empty>";
  while (vlit.hasNext()) {
    Formula::VarList::Iterator vit(vlit.next());
    if (!vit.hasNext()) {
      cout << " <empty>";
    } else {
      cout << " [";
      while (vit.hasNext()) {
        cout << vit.next();
        if (vit.hasNext()) cout << " ";
      };
      cout << "]";
    }
  }
  cout << endl;

  Map<int, SortList*>::Iterator vsit(_variableSorts);
  cout << "Variables sorts:";
  if   (!vsit.hasNext()) cout << "<empty>";
  int vsitKey;
  SortList* vsitVal;
  while (vsit.hasNext()) {
    vsit.next(vsitKey, vsitVal);
    cout << " {" << vsitKey << " ->";
    SortList::Iterator slit(vsitVal);
    if   (!slit.hasNext()) cout << " <empty>";
    while (slit.hasNext()) cout << " " << env.sorts->sortName(slit.next());
    cout << "}";
  }
  cout << endl;
  
  Stack<SortList*>::Iterator slsit(_sortLists);
  cout << "Sort lists: ";
  if   (!slsit.hasNext()) cout << "<empty>";
  while (slsit.hasNext()) {
    SortList* sl = slsit.next();
    SortList::Iterator slit(sl);
    if   (!slit.hasNext()) cout << "<empty>";
    while (slit.hasNext()) cout << env.sorts->sortName(slit.next()) << " ";
    cout << ";";
  }
  cout << endl;

/*  Stack<TheoryFunction>::Iterator tfit(_theoryFunctions);
  cout << "Theory functions: ";
  if   (!tfit.hasNext()) cout << " <empty>";
  while (tfit.hasNext()) cout << " " << tfit.next();
  cout << endl;

  Stack<LetFunctionsScope>::Iterator lfsit(_letScopes);
  cout << "Let functions scopes: ";
  if (!lfsit.hasNext()) cout << "<empty>";
  while (lfsit.hasNext()) {
    LetFunctionsScope lfs = lfsit.next();
    LetFunctionsScope::Iterator sit(lfs);
    if (!sit.hasNext()) {
      cout << "<empty>";
    } else {
      unsigned i = lfs.length();
      while (sit.hasNext()) {
        LetFunction f    = sit.next();
        vstring name     = f.first.first;
        unsigned arity   = f.first.second;
        unsigned symbol  = f.second.first;
        bool isPredicate = f.second.second;

        vstring symbolName = isPredicate ? env.signature->predicateName(symbol)
                                         : env.signature->functionName (symbol);

        cout << name << "/" << arity << " -> " << symbolName;
        if (--i > 0) {
          cout << ", ";
        }
      };
    }
  }
  cout << endl;

  LetFunctionsScope::Iterator clfsit(_currentLetScope);
  cout << "Current let functions scope:";
  if (!clfsit.hasNext()) {
    cout << " <empty>";
  } else {
    while (clfsit.hasNext()) {
      LetFunction f    = clfsit.next();
      vstring name     = f.first.first;
      unsigned arity   = f.first.second;
      unsigned symbol  = f.second.first;
      bool isPredicate = f.second.second;

      vstring symbolName = isPredicate ? env.signature->predicateName(symbol)
                                       : env.signature->functionName (symbol);

      cout << name << "/" << arity << " -> " << symbolName << " ";
    }
  }
  cout << endl;

  Stack<LetBindingScope>::Iterator lbsit(_letBindings);
  cout << "Let bindings: ";
  if (!lbsit.hasNext()) cout << "<empty>";
  while (lbsit.hasNext()) {
    LetBindingScope lbs = lbsit.next();
    LetBindingScope::Iterator lbit(lbs);
    unsigned i = (unsigned)lbs.length();
    if (lbit.hasNext()) {
      while (lbit.hasNext()) {
        LetBinding b = lbit.next();
        unsigned symbol = b.first;
        bool isTuple = b.second;
        if (isTuple) {
          cout << env.sorts->sortName(env.signature->getFunction(symbol)->fnType()->result());
        } else {
          cout << env.signature->functionName(symbol);
        }
      }
      if (--i > 0) {
        cout << ", ";
      }
    }
  }
  cout << endl;

  LetBindingScope::Iterator clbsit(_currentBindingScope);
  cout << "Current let bindings scope:";
  if (!clbsit.hasNext()) {
    cout << " <empty>";
  } else {
    while (clbsit.hasNext()) {
      LetBinding b    = clbsit.next();
      unsigned symbol = b.first;
      bool isTuple = b.second;
      cout << symbol << "," << isTuple << " ";
    }
  }
  cout << endl;*/
}
#endif

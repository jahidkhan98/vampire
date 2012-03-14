/**
 *  @file LoopAnalyzer.cpp
 *  Simple Loop analysis
 *
 * @since 23/08/2010, Torrevieja
 */

#include "Debug/Tracer.hpp"
#include "Lib/DArray.hpp"
#include "Lib/Int.hpp"
#include "Lib/Environment.hpp"
#include "Lib/DHSet.hpp"
#include "Kernel/Signature.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/Inference.hpp"
#include "Kernel/Unit.hpp"
#include "Api/FormulaBuilder.hpp"
#include "Api/Problem.hpp"


#include "Variable.hpp"
#include "Expression.hpp"
#include "Statement.hpp"
#include "Path.hpp"
#include "LoopAnalyzer.hpp"

#include "Shell/Options.hpp"
#include "Shell/Property.hpp"
#include "Shell/Preprocess.hpp"

using namespace Kernel;
using namespace Program;
using namespace Shell;

/** Constructor, just saves the program */
LoopAnalyzer::LoopAnalyzer(WhileDo* loop)
  : _loop(loop),
    _units(0)
{}

/**
 * Simple loop analysis. Does the following:
 * <ol>
 *   <li></li>
 *   <li></li>
 *   <li></li>
 * </ol>
 */
void LoopAnalyzer::analyze()
{
  CALL("LoopAnalyzer::analyze");

  cout << "Analyzing loop...\n";
  cout << "---------------------\n";
  _loop->prettyPrint(cout);
  cout << "---------------------\n";
  cout << "Analyzing variables...\n";
  cout << "---------------------\n";
  analyzeVariables();
  cout << "\nCollecting paths...\n";
  cout << "---------------------\n";
  collectPaths();
  // output paths
  Stack<Path*>::Iterator it(_paths);
   //cout << "number of paths: " <<length<<"\n";
  while (it.hasNext()) {
    it.next()->prettyPrint(cout);
  }
  generateAxiomsForCounters();
  cout << "\nGenerate Let...In expressions for  next states...\n";
  cout << "---------------------\n";
  generateLetExpressions();
  cout << "\nGenerate update predicates of arrays...\n";
  cout << "---------------------\n";
  generateUpdatePredicates();
  cout << "\nGenerate correspondence between final (initial) values and final (initial) functions of variables...\n";
  cout << "---------------------\n";
  generateValueFunctionRelationsOfVariables();
  cout << "\nGenerate loop condition property...\n";
  cout << "---------------------\n";
  generateLoopConditionProperty();
  cout << "\nCollected first-order loop properties...\n";
  cout << "---------------------\n";
  generateIterationDefinition();
  UnitList::Iterator units(_units);
  while (units.hasNext()) {
    cout << units.next()->toString() << "\n";
  }
}

/**
 * If the assignment has the form x = x + c, where x is an integer variable
 * and c an integer constant, return c. Otherwise, return 0.
 */
int LoopAnalyzer::isScalarIncrement(const Assignment* ass)
{
  CALL("LoopAnalyzer::isScalarIncrement");

  if (ass->lhs()->kind() != Expression::VARIABLE) {
    return 0;
  }
  Variable* x = static_cast<VariableExpression*>(ass->lhs())->variable();
  Expression* rhs = ass->rhs();
  if (rhs->kind() != Expression::FUNCTION_APPLICATION) {
    return 0;
  }
  FunctionApplicationExpression* app = static_cast<FunctionApplicationExpression*>(rhs);
  bool plus = (app->function() == ConstantFunctionExpression::integerPlus());
  if (! plus) {
    if (app->function() != ConstantFunctionExpression::integerMinus()) {
      return 0;
    }
  }
  // the expression is either s+t or s-t
  Expression* e1 = app->getArgument(0);
  Expression* e2 = app->getArgument(1);
  switch (e1->kind()) {
  case Expression::VARIABLE:
    if (e2->kind() != Expression::CONSTANT_INTEGER) {
      return 0;
    }
    break;
  case Expression::CONSTANT_INTEGER:
    if (e2->kind() != Expression::VARIABLE) {
      return 0;
    }
    else {
      Expression* e = e2;
      e2 = e1;
      e1 = e;
    }
    break;

  default:
    return 0;
  }
  // e1 is a variable and e2 is an integer constant
  if (static_cast<VariableExpression*>(e1)->variable() != x) {
    return 0;
  }
  return static_cast<ConstantIntegerExpression*>(e2)->value();
}

/**
 * If the statement is a scalar assignment to a variable x,
 * return this variable. Otherwise, return 0
 */
Variable* LoopAnalyzer::isScalarAssignment(const Statement* st)
{	
  CALL("LoopAnalyzer::isScalarAssignment");
  if (st->kind() != Statement::ASSIGNMENT) {
    return 0;
  }
  Expression* lhs = static_cast<const Assignment*>(st)->lhs();
  if (lhs->kind() != Expression::VARIABLE) {
    return 0;
  }
  return static_cast<VariableExpression*>(lhs)->variable();
}

/**
 * Analyze all variables in the loop:
 * <ol>
 * <li>Collect all updated variables into _updated</li>
 * <li>Collect all counters into _counters</li>
 * </ol>
 */
void LoopAnalyzer::analyzeVariables()
{
  CALL("LoopAnalyzer::analyzeVariables()");

  // find counters
  Map<Variable*,bool>::Iterator vars(*_loop->variables());
  // iterate over variables
  while (vars.hasNext()) {
    Variable* var;
    bool updated;
    vars.next(var,updated);
    VariableInfo* vinfo = new VariableInfo;
    _variableInfo.insert(var,vinfo);
    cout << "Variable: " << var->name() << ": " << (updated ? "(updated)" : "constant") << "\n";
    bool scalar = (var->vtype()->kind() == Type::INT);
    vinfo->scalar = scalar;
    if (updated) {
      vinfo->updated = true;
      vinfo->counter = scalar;
    }
    else {
      vinfo->updated = true;
      vinfo->counter = false;
    }
  }
  Statement::SubstatementIterator sit (_loop->body());
  while (sit.hasNext()) {
    Statement* stat = sit.next();
    Variable* var = isScalarAssignment(stat);
    if (! var) {
      continue;
    }
    if (! isScalarIncrement(static_cast<Assignment*>(stat))) {
      _variableInfo.get(var)->counter = false;
    }
  }
  VariableMap::Iterator vit(_variableInfo);
  while (vit.hasNext()) {
    Variable* v;
    VariableInfo* vinfo;
    vit.next(v,vinfo);

    // adding the symbol to the signature
    unsigned arity = vinfo->scalar ? 0 : 1;
    string name(v->name());
    vinfo->signatureNumber = getIntFunction(name,arity);
    if (arity == 0) {
      vinfo->constant = Term::create(vinfo->signatureNumber,0,0);
    }
    if (vinfo->updated) {
      vinfo->extraSignatureNumber = getIntFunction(name,arity+1);
      // cout << "variable: "<<name <<" with: "<<vinfo->extraSignatureNumber<< " with arity: "<< arity+1<<"\n";
    }
    
    if (vinfo->counter) {
      cout << "Counter: " << v->name() << "\n";
    }
  }
} 

/**
 * Collect all paths in the loop into _paths;
 */
void LoopAnalyzer::collectPaths()
{
  CALL("LoopAnalyzer::collectPaths");
  Stack<Path*> paths; // partial paths
  // statements corresponding to this path, normally then-parts of IfThenElse
  Stack<Statement*> statements;
  Path* path = Path::empty();
  cout << "Paths wrt the loop condition: " << _loop->condition()->toString() << "\n";
  Statement* stat = _loop->body();
  for (;;) {
    switch (stat->kind()) {
    case Statement::ASSIGNMENT:
      { path = path->add(stat);
        stat = stat->nextStatement();
      }
      break;
    case Statement::BLOCK:
      stat = stat->nextStatement();
      break;
    case Statement::ITE:
      {
      	path = path->add(stat);
	IfThenElse* ite = static_cast<IfThenElse*>(stat);
	// save the else-path for further processing
	paths.push(path->add(ite->elsePart()));
	statements.push(ite->elsePart());
	// continue with the then-path
	stat = ite->thenPart();
	path = path->add(stat);
	stat = stat->nextStatement();
      }
      break;

    case Statement::WHILE_DO:
      ASS(false); // cannot yet work with embedded loops
    case Statement::EXPRESSION:
      ASS(false); // cannot yet work with procedure calls
    } //end of switch
    if (stat != _loop) { // end of the body not reached
      continue;
    }
    _paths.push(path);
    if (paths.isEmpty()) {
      return;
    }
    path = paths.pop();
    stat = statements.pop();
  }
}

/*
*
 *  @file let...in analysis in LoopAnalyzer.cpp
 *  Simple let..in and array  analysis
 *
 * @since 01/04/2011, Vienna
 */



/**
 * Translate program term-expressions into Vampire terms (termlists)
*/

TermList LoopAnalyzer::expressionToTerm(Expression* exp)
{
  CALL("LoopAnalyzer::expressionToTerm");
  switch (exp->kind()) {
  case Expression::CONSTANT_INTEGER:
   {
    int val = static_cast<ConstantIntegerExpression*>(exp)->value();
    Theory* theory = Theory::instance();
    TermList var(theory->representConstant(IntegerConstantType(val)));
    // x01(theory->fun1(Theory::SUCCESSOR,x0));
    return var;
   }
  break;
  case Expression::VARIABLE:
    {
      Variable* expVar=  static_cast<VariableExpression*>(exp)->variable();
      string expName=expVar->name() ;
      TermList var(Term::createConstant(getIntConstant(expName)));
      //cout << "expression "<< exp->toString()<< " over variable "<<expName <<" translated to Vampire term "<< var<<"\n";
      return var;
    }
   break;
   case Expression::ARRAY_APPLICATION:
     {
       //create term for array variable
       Expression*  expArray=  static_cast<ArrayApplicationExpression*>(exp)->array();
       string varName=expArray->toString();
       unsigned arrayFct=getIntFunction(varName,1);
       //create term represenation for array arguments
       Expression*  expArrayArguments=  static_cast<ArrayApplicationExpression*>(exp)->argument();
       TermList varArg= expressionToTerm(expArrayArguments);
       //create term for array application
       TermList varArray=TermList(Term::create1(arrayFct, varArg));
       return varArray;
     }
     break;
  //case Expression::CONSTANT_FUNCTION:
     //{}
     //break;
  case Expression::FUNCTION_APPLICATION:
    {
      //cout<<"function application in rhs of Vampire term: "<<exp->toString()<<"\n";
      FunctionApplicationExpression* app = static_cast<FunctionApplicationExpression*>(exp);
      //cout<<"function: "<<(app->function())->toString()<<"\n";
      //switch on app->function did not work (error on non-integer value in switch), so used nested IF
      if ( app->function() == ConstantFunctionExpression::integerPlus() )
	{
	   Expression* e1 = app->getArgument(0);
	   Expression* e2 = app->getArgument(1);
	   TermList e1Term= expressionToTerm(e1); //make recursive call on function arguments
	   TermList e2Term=expressionToTerm(e2);
	   Theory* theory = Theory::instance();
	   TermList fctTerm(theory->fun2(Theory::INT_PLUS,e1Term,e2Term));
	   return fctTerm;
	}
      else 
	{
	  if ( app->function() == ConstantFunctionExpression::integerMinus() )
	    {
	      Expression* e1 = app->getArgument(0);
	      Expression* e2 = app->getArgument(1);
	      TermList e1Term= expressionToTerm(e1); //make recursive call on function arguments
	      TermList e2Term=expressionToTerm(e2);	   
 	      Theory* theory = Theory::instance();
	      TermList fctTerm(theory->fun2(Theory::INT_MINUS,e1Term,e2Term));
	      return fctTerm;
	    }
	  else 
	    {
	       if ( app->function() == ConstantFunctionExpression::integerMult() )
		 {
		   Expression* e1 = app->getArgument(0);
		   Expression* e2 = app->getArgument(1);
		   TermList e1Term= expressionToTerm(e1); //make recursive call on function arguments
		   TermList e2Term=expressionToTerm(e2);		
		   Theory* theory = Theory::instance();
		   TermList fctTerm(theory->fun2(Theory::INT_MULTIPLY,e1Term,e2Term));
		   return fctTerm;
		 }
	       else
		 {
		   if ( app->function() == ConstantFunctionExpression::integerNegation() )
		     {
		        Expression* e1 = app->getArgument(0);
			TermList e1Term= expressionToTerm(e1); //make recursive call on function arguments
			Theory* theory = Theory::instance();
			TermList fctTerm(theory->fun1(Theory::INT_UNARY_MINUS,e1Term));
			return fctTerm;
		     }
   		  else //undefined/not treated theory function. Extend it to later for uninterpreted fct. 
		   {
		     //rhs is an uninterpreted function f(e1), 
		     //create term for uninterpreted function
		     Expression*  uiFct=  app->function();
		     string uiFctName=uiFct->toString();
		     unsigned uiFctNameTerm=getIntFunction(uiFctName,1);
		     //create term representation for arguments e1
		     Expression* e1 = app->getArgument(0);
		     TermList e1Term= expressionToTerm(e1); //make recursive call on function arguments
		     //create term representation of f(e1)
		     TermList fctTerm=TermList(Term::create1(uiFctNameTerm,e1Term));
		     return fctTerm;
		     //string testName="toDo";
		     // TermList fctTerm(Term::createConstant(testName)); 
		     //return fctTerm;
		   }
		 }
	    }
	}
    }
      break;
  }
    
}


/**
 * Translate program predicate-expressions into Vampire literals (termlists)
*/

Formula* LoopAnalyzer::expressionToPred(Expression* exp)
{
  CALL("LoopAnalyzer::expressionToPred");
  FunctionApplicationExpression* app = static_cast<FunctionApplicationExpression*>(exp);
  if ( app->function() == ConstantFunctionExpression::integerEq() )
  {
    Expression* e1 = app->getArgument(0);
    Expression* e2 = app->getArgument(1);
    TermList e1Term= expressionToTerm(e1); //make recursive call on function arguments
    TermList e2Term=expressionToTerm(e2);		
    Theory* theory = Theory::instance();
    Formula* predTerm = new AtomicFormula(theory->pred2(Theory::EQUAL,true,e1Term,e2Term));
    return predTerm;
  }
  if ( app->function() == ConstantFunctionExpression::integerLess() )
  {
    Expression* e1 = app->getArgument(0);
    Expression* e2 = app->getArgument(1);
    TermList e1Term= expressionToTerm(e1); //make recursive call on function arguments
    TermList e2Term=expressionToTerm(e2);		
    Theory* theory = Theory::instance();
    Formula* predTerm= new AtomicFormula(theory->pred2(Theory::INT_LESS,true,e1Term,e2Term));
    return predTerm;
  }
  if ( app->function() == ConstantFunctionExpression::integerLessEq() )
  {
    Expression* e1 = app->getArgument(0);
    Expression* e2 = app->getArgument(1);
    TermList e1Term= expressionToTerm(e1); //make recursive call on function arguments
    TermList e2Term=expressionToTerm(e2);		
    Theory* theory = Theory::instance();
    Formula* predTerm= new AtomicFormula(theory->pred2(Theory::INT_LESS_EQUAL,true,e1Term,e2Term));
    return predTerm;
  }
  if ( app->function() == ConstantFunctionExpression::integerGreater() )
  {
    Expression* e1 = app->getArgument(0);
    Expression* e2 = app->getArgument(1);
    TermList e1Term= expressionToTerm(e1); //make recursive call on function arguments
    TermList e2Term=expressionToTerm(e2);		
    Theory* theory = Theory::instance();
    Formula* predTerm= new AtomicFormula(theory->pred2(Theory::INT_GREATER,true,e1Term,e2Term));
    return predTerm;
   }
   if ( app->function() == ConstantFunctionExpression::integerGreaterEq() ) 
   {
     Expression* e1 = app->getArgument(0);
     Expression* e2 = app->getArgument(1);
     TermList e1Term= expressionToTerm(e1); //make recursive call on function arguments
     TermList e2Term=expressionToTerm(e2);		
     Theory* theory = Theory::instance();
     Formula* predTerm= new AtomicFormula(theory->pred2(Theory::INT_GREATER_EQUAL,true,e1Term,e2Term));
     return predTerm;
   }
   //if ( app->function() == AND )
   //{
   // Expression* e1 = app->getArgument(0);
   //  Expression* e2 = app->getArgument(1);
   //  Formula* e1Pred= expressionToPred(e1); //make recursive call on function arguments
   //  Formula* e2Pred=expressionToPred(e2);		
   //  Theory* theory = Theory::instance();
   //  Formula* predTerm= new BinaryFormula(AND,e1Pred,e2Pred);
   //  return predTerm;
   // }
}


/**
 * Generate let STAT in EXP,
 *  where STAT is path initially
 *  and EXP is a variable initially
*/

TermList LoopAnalyzer::letTranslationOfPath(Path::Iterator &sit, TermList exp)
{
  if (!sit.hasNext()) // end of the recursion
    {return exp;}
  else //make recursive call
    {
      Statement* stat = sit.next();
      exp = letTranslationOfPath(sit,exp);
      switch (stat->kind()) {
	   case Statement::ASSIGNMENT:
	      { 
		Expression* lhs = static_cast<Assignment*>(stat)->lhs();
	        Expression* rhs = static_cast<Assignment*>(stat)->rhs();
	        if (lhs->kind() == Expression::VARIABLE) 
	        {
		  TermList lhsTerm=expressionToTerm(lhs);
		  TermList rhsTerm=expressionToTerm(rhs);
		  exp=TermList(Term::createTermLet(lhsTerm, rhsTerm, exp));
		 }
	        if  (lhs->kind() == Expression::ARRAY_APPLICATION)
		 { 
		   TermList rhsTerm=expressionToTerm(rhs);
		   Expression*  lhsArray=  static_cast<ArrayApplicationExpression*>(lhs)->array();
		   Expression*  lhsArrayArguments=  static_cast<ArrayApplicationExpression*>(lhs)->argument();
		   TermList argTerms = expressionToTerm(lhsArrayArguments);
		   string arrayName=lhsArray->toString();
		   unsigned arrayFct1=getIntFunction(arrayName,1);
		   TermList x1;
		   x1.makeVar(1);
		   TermList arrayX1(Term::create(arrayFct1,1,&x1));
		   Literal* x1EQArgs=createIntEquality(true, x1, argTerms);
		   TermList lhsTerm=TermList(Term::create1(arrayFct1, x1));
		   TermList arrayITE=TermList(Term::createTermITE(new AtomicFormula(x1EQArgs), rhsTerm, arrayX1));
		   exp=TermList(Term::createTermLet(lhsTerm, arrayITE, exp)); 
		 }
		return exp;
              }
	      break;
	   case Statement::BLOCK:
	     {
	       	return exp;
	     }
	        break;
	   case Statement::ITE: {
	        return exp; 
	   }
	    break;
	   }
    }
}

/**
 * Generate let v=v(X) in letFormula,
 *  for each scalar updated variable
*/

Formula* LoopAnalyzer::letTranslationOfVar(VariableMap::Iterator& varit, Formula* letFormula)
{
  if (!varit.hasNext()) //end of the recursion
    {return letFormula;}
  else
    {
      Variable* w;
      VariableInfo* winfo;
      varit.next(w,winfo);
      if (winfo->counter) { // do this only for updated scalar variables
	string warName=w->name();
	TermList war(Term::createConstant(getIntConstant(warName)));
	unsigned warFun = getIntFunction(warName,1);
	// term x0
	TermList x0;
	x0.makeVar(0);
	// term w(x0)
	TermList warX0(Term::create(warFun,1,&x0));
	letFormula= new TermLetFormula(war, warX0, letFormula);
      }
      return letTranslationOfVar(varit,letFormula);
    }

}


/**
 * Generate let A(a)=A(X0,a) in EXP,
 *  for each updated array A
*/


Formula* LoopAnalyzer::letTranslationOfArray(Map<Variable*,bool>::Iterator &sit, Formula* exp)
{
  if (!sit.hasNext()) // end of the recursion
    {return exp;}
  else //make recursive call
    {
      Variable* v;
      bool updated;
      sit.next(v,updated);
      bool array = (v->vtype()->kind() == Type::ARRAY);
      if (updated & array) {//it is an updated array
	      string varName=v->name();
	      unsigned arrayFct1=getIntFunction(varName,1);
	      unsigned arrayFct2=getIntFunction(varName,2);
	      // term x0
	      TermList x0;
	      x0.makeVar(0);
	      // term x1
	      TermList x1;
	      x1.makeVar(1);
	      //term A(x1);
	      TermList arrayX1(Term::create(arrayFct1,1,&x1));
	      // term A(x0,x1)
	      TermList arrayX01(Term::create2(arrayFct2,x0,x1));
 	      exp=new TermLetFormula(arrayX1, arrayX01, exp); 
      }
      return letTranslationOfArray(sit,exp);
    }	       
}


/**For condition Ck in the  path s1;C1;...;sk;Ck;...
 * (where s1,...,sk... are assignments)
 * create the letFormula: let s1 in ... let sk in Ck
 */
Formula* LoopAnalyzer::letCondition(Path::Iterator &sit, Formula* condition, int condPos, int currPos)
{
   if (condPos == currPos)  // current statement is the condition we searched, so stop
    { return condition;  }
  else //search for the condition
    {
      Statement* stat=sit.next();
      switch (stat->kind()) {
	   case Statement::ASSIGNMENT:
	      { 
		Expression* lhs = static_cast<Assignment*>(stat)->lhs();
		Expression* rhs = static_cast<Assignment*>(stat)->rhs();
		 if  (lhs->kind() == Expression::VARIABLE)
		   {
		     TermList lhsTerm=expressionToTerm(lhs);
		     TermList rhsTerm=expressionToTerm(rhs);	
		     condition=new TermLetFormula(lhsTerm, rhsTerm, condition);
		   }
		 if (lhs->kind() == Expression::ARRAY_APPLICATION)
		 { 
		   TermList rhsTerm=expressionToTerm(rhs);
		   Expression*  lhsArray=  static_cast<ArrayApplicationExpression*>(lhs)->array();
		   Expression*  lhsArrayArguments=  static_cast<ArrayApplicationExpression*>(lhs)->argument();
		   TermList argTerms = expressionToTerm(lhsArrayArguments);
		   string arrayName=lhsArray->toString();
		   unsigned arrayFct1=getIntFunction(arrayName,1);
		   TermList x1;
		   x1.makeVar(1);
		   TermList arrayX1(Term::create(arrayFct1,1,&x1));
		   Literal* x1EQArgs=createIntEquality(true, x1, argTerms);
		   TermList lhsTerm=TermList(Term::create1(arrayFct1, x1));
		   TermList arrayITE=TermList(Term::createTermITE(new AtomicFormula(x1EQArgs), rhsTerm, arrayX1));
		    condition=new TermLetFormula(lhsTerm, arrayITE, condition); 
		 }
	      }
	      break;
	   case Statement::BLOCK:
	      break;
	   case Statement::ITE: 
	    break;
	   return letCondition(sit,condition, condPos, currPos+1);
      }
    }
}


/** Process and collect ALL conditions in a stack 
 *
 */

Formula* LoopAnalyzer::letTranslationOfGuards(Path* path, Path::Iterator &sit, Formula* letFormula)
{
    Stack<Formula*> conditions;
    int condPos =0;
    while (sit.hasNext() )
      {
	Statement* stat=sit.next();
	switch (stat->kind()) {
	   case Statement::ASSIGNMENT:
	       break;
	   case Statement::BLOCK:
	       break;
	   case Statement::ITE: {
	     IfThenElse* ite = static_cast<IfThenElse*>(stat);
	     Formula* condition = expressionToPred(ite->condition());
	     Statement* elsePart= ite->elsePart();
	     if (elsePart==(sit.next())) {condition = new NegatedFormula(condition);}
	     Path::Iterator pit(path);
	     condition = letCondition(pit,condition,condPos,0);
	     conditions.push(condition);		     
	   }
	    break;
	}
	condPos = condPos+1;
      }
    while (!(conditions.isEmpty()))
      {
	Formula* cond= conditions.pop();
	letFormula =new BinaryFormula(IMP, cond, letFormula);
      }
    return letFormula;
    
}


/** 
 * Generate let expressions
*/


void LoopAnalyzer::generateLetExpressions()
{
  CALL("LoopAnalyzer::generateLetExpressions");
  Stack<Path*>::Iterator pit(_paths);
  while (pit.hasNext()) {
      Path* path = pit.next();
      //create let..in for each var wrt PATH
      Map<Variable*,bool>::Iterator vars(*_loop->variables());//alternative for updated vars, incl. arrays
      // iterate over variables
       while (vars.hasNext()) {
	 Variable* v;
	 bool updated;
	 vars.next(v,updated);
    	 if (! updated) {
	   continue;
	}
	 bool scalar = (v->vtype()->kind() == Type::INT);
	 bool array = (v->vtype()->kind() == Type::ARRAY);
	 string varName=v->name();
	 TermList var;
	 TermList varX01;
	 Theory* theory = Theory::instance();
	 // term x0
	 TermList x0;
	 x0.makeVar(0);
	 // term x0+1 for next iteration
	 TermList x01(theory->fun1(Theory::INT_SUCCESSOR,x0));
	 //build term representation for scalars and arrays
	 if (scalar){
	   TermList scalarVar(Term::createConstant(getIntConstant(varName)));
	   var =scalarVar;
	   //create Vampire terms for variable v:  v(X0+1)
	   unsigned varFun = getIntFunction(varName,1);
	   // term v(x0)
	   // TermList varX0(Term::create(varFun,1,&x0));
	   //term v(x0+1)
	   TermList scalarX01(Term::create(varFun,1,&x01));
	   varX01=scalarX01;
	 }
	 if (array)
	   {
	     //create Vampire terms for array V: V(X1), V(X0+1,X1)
	      unsigned varFun1 = getIntFunction(varName,1);
	      unsigned varFun2 = getIntFunction(varName,2);
	      // term x1
	      TermList x1;
	      x1.makeVar(1);
	      // term V(x1)
	      TermList arrayX1(Term::create(varFun1,1,&x1));
	      var = arrayX1;
	      // term V(x0+1,x1)
	      Theory* theory = Theory::instance();
	      TermList x01(theory->fun1(Theory::INT_SUCCESSOR,x0));
	      TermList arrayX01(Term::create2(varFun2,x01,x1));
	      varX01=arrayX01;
	   }
	 //create let..in expression for an updated  scalar/array variable v through the path
	 Path::Iterator it(path);
	 TermList letInBody = letTranslationOfPath(it,var);
	 //path is processed, create letFORMULA:
	 // if v is scalar, then let v(X+1)= letInBody 
	 // if v is array, then v(X+1,Y) = letInBody
	 Formula* letFormula = new AtomicFormula(createIntEquality(true, varX01, letInBody));
	 //collect and add conditions from path to the letFormula, so we have cond=>letFormula
	 Path::Iterator pathCondit(path);
	 letFormula = letTranslationOfGuards(path, pathCondit, letFormula);				       
	 //make sequence of let v=v(x) in...letInBody for all vars
	 //process first updated array vars
	 Map<Variable*,bool>::Iterator vars(*_loop->variables());
	 //Path::Iterator pathit(path);
	 letFormula = letTranslationOfArray(vars, letFormula);
       	//process updated scalars 
        VariableMap::Iterator varit(_variableInfo);
        letFormula = letTranslationOfVar(varit,letFormula);
	 _units = _units->cons(new FormulaUnit(letFormula,
					  new Inference(Inference::PROGRAM_ANALYSIS),
					  Unit::ASSUMPTION));
      }
   }
}


/**
 * Given an array update a[v]=RHS at position posCnt of a path SIT, 
 * generate let PATH_STATEMENTS in RHS, 
 * where PATHS_STATEMENTS are all statements that are before posCnt
*/

TermList LoopAnalyzer::arrayUpdateValue(Path::Iterator &sit, TermList exp, int posCnt, int currentCnt)
{
  CALL("LoopAnalyzer::arrayUpdatePosition");
  if (posCnt==currentCnt) // end of the recursion
    {return exp;}
  else //make recursive call
    {
      Statement* stat = sit.next();
      exp = arrayUpdateValue(sit,exp,posCnt,currentCnt+1);
      switch (stat->kind()) {
	   case Statement::ASSIGNMENT:
	      { 
		Expression* lhs = static_cast<Assignment*>(stat)->lhs();
	        Expression* rhs = static_cast<Assignment*>(stat)->rhs();
	        if (lhs->kind() == Expression::VARIABLE) 
	        {
		  TermList lhsTerm=expressionToTerm(lhs);
		  TermList rhsTerm=expressionToTerm(rhs);
		  exp=TermList(Term::createTermLet(lhsTerm, rhsTerm, exp));
		 }
	        if  (lhs->kind() == Expression::ARRAY_APPLICATION)
		 { 
		   TermList rhsTerm=expressionToTerm(rhs);
		   Expression*  lhsArray=  static_cast<ArrayApplicationExpression*>(lhs)->array();
		   Expression*  lhsArrayArguments=  static_cast<ArrayApplicationExpression*>(lhs)->argument();
		   TermList argTerms = expressionToTerm(lhsArrayArguments);
		   string arrayName=lhsArray->toString();
		   unsigned arrayFct1=getIntFunction(arrayName,1);
		   TermList x1;
		   x1.makeVar(1);
		   TermList arrayX1(Term::create(arrayFct1,1,&x1));
		   Literal* x1EQArgs=createIntEquality(true, x1, argTerms);
		   TermList lhsTerm=TermList(Term::create1(arrayFct1, x1));
		   TermList arrayITE=TermList(Term::createTermITE(new AtomicFormula(x1EQArgs), rhsTerm, arrayX1));
		   exp=TermList(Term::createTermLet(lhsTerm, arrayITE, exp)); 
		 }
		return exp;
              }
	      break;
	   case Statement::BLOCK:
	     {
	       	return exp;
	     }
	        break;
	   case Statement::ITE: {
	        return exp; 
	   }
	    break;
	   }
    }
}

/**
 *Check if array v is updated on path PATH
 * if yes, return the nr. of the statement from the path updating the array
 * if array v was not updated, return 0
 */
int LoopAnalyzer::arrayIsUpdatedOnPath(Path* path, Variable *v)
{
  CALL("LoopAnalyzer::arrayIsUpdatedOnPath");
  Path::Iterator it(path);
  int updated = 0;
  while (it.hasNext())
  {
    Statement* stat=it.next();
    switch (stat->kind()) {
    case Statement::ASSIGNMENT:
    {
       Expression* lhs = static_cast<Assignment*>(stat)->lhs();
       if (lhs->kind()==Expression::ARRAY_APPLICATION)
       { 
	   Expression*  lhsArray=  static_cast<ArrayApplicationExpression*>(lhs)->array();
       	   string arrayName=lhsArray->toString();
	   if (arrayName == v->name()) {updated =1;}
       }
    }
    break;
    case Statement::BLOCK:
     break;
    case Statement::ITE:
      break;
    case Statement::WHILE_DO:
      ASS(false); // cannot yet work with embedded loops
    case Statement::EXPRESSION:
      ASS(false); // cannot yet work with procedure calls
    }
  }
  return updated;
}

/**
 *compute the (let...in update position ) expression updPosExp 
 *for one array update v[update_positon]
 */

TermList LoopAnalyzer::arrayUpdatePosition(Path::Iterator &sit, TermList updPosExp, int posCnt, int currentCnt)
{
  CALL("LoopAnalyzer::arrayUpdatePosition");
  if (posCnt == currentCnt) {return updPosExp;}
  else
    {
      Statement* stat = sit.next();
      switch (stat->kind()) {
	   case Statement::ASSIGNMENT:
	      { 
		Expression* lhs = static_cast<Assignment*>(stat)->lhs();
		Expression* rhs = static_cast<Assignment*>(stat)->rhs();
		 if  (lhs->kind() == Expression::VARIABLE)
		   {
		     TermList lhsTerm=expressionToTerm(lhs);
		     TermList rhsTerm=expressionToTerm(rhs);	
		     updPosExp=TermList(Term::createTermLet(lhsTerm, rhsTerm, updPosExp));
		   }
		 if (lhs->kind() == Expression::ARRAY_APPLICATION)
		 { 
		   TermList rhsTerm=expressionToTerm(rhs);
		   Expression*  lhsArray=  static_cast<ArrayApplicationExpression*>(lhs)->array();
		   Expression*  lhsArrayArguments=  static_cast<ArrayApplicationExpression*>(lhs)->argument();
		   TermList argTerms = expressionToTerm(lhsArrayArguments);
		   string arrayName=lhsArray->toString();
		   unsigned arrayFct1=getIntFunction(arrayName,1);
		   TermList x1;
		   x1.makeVar(1);
		   TermList arrayX1(Term::create(arrayFct1,1,&x1));
		   Literal* x1EQArgs=createIntEquality(true, x1, argTerms);
		   TermList lhsTerm=TermList(Term::create1(arrayFct1, x1));
		   TermList arrayITE=TermList(Term::createTermITE(new AtomicFormula(x1EQArgs), rhsTerm, arrayX1));
		   updPosExp=TermList(Term::createTermLet(lhsTerm, arrayITE, updPosExp));
	  	 }
	      }
	      break;
	   case Statement::BLOCK:
	      break;
	   case Statement::ITE: 
	    break;
	   case Statement::WHILE_DO:
	     ASS(false); // cannot yet work with embedded loops
           case Statement::EXPRESSION:
             ASS(false); // cannot yet work with procedure calls
      }
      return arrayUpdatePosition(sit, updPosExp, posCnt, currentCnt+1);
    }
}

/**
 *compute the (let...in condition1 ) &(let...in condition2)... formula 
 *for all conditions that appear before the array update of v[update_expression]
 */

Formula* LoopAnalyzer::arrayUpdateCondition(Path* path, Path::Iterator &sit, int posCnt)
{
  CALL("LoopAnalyzer::arrayUpdateCondition");
  Stack<Formula*> conditions;
  int currentCnt=0;
  int firstCondition=1;
  Formula* arrayUpdCondition;
  while (posCnt != currentCnt) 
    {
      Statement* stat = sit.next();
      switch (stat->kind()) {
	   case Statement::ASSIGNMENT:
	       break;
	   case Statement::BLOCK:
	     break;
           case Statement::ITE: //condition is found on path
	     {
	         IfThenElse* ite = static_cast<IfThenElse*>(stat);
		 Formula* condition = expressionToPred(ite->condition());
		 Statement* elsePart= ite->elsePart();
		 if (elsePart==(sit.next())) {condition = new NegatedFormula(condition);}
       		 Path::Iterator pit(path);
	     	 condition = letCondition(pit,condition,currentCnt,0);
		 conditions.push(condition);
	     }
	     break;
	   case Statement::WHILE_DO:
	     ASS(false); // cannot yet work with embedded loops
           case Statement::EXPRESSION:
             ASS(false); // cannot yet work with procedure calls
      }
      currentCnt=currentCnt+1;
    }
   while (!(conditions.isEmpty()))
      {
	Formula* condition= conditions.pop();
	if (firstCondition) {arrayUpdCondition = condition;firstCondition=0;}
	else {FormulaList* interForm = (new FormulaList(arrayUpdCondition)) -> cons(condition); arrayUpdCondition = new JunctionFormula(AND,interForm);}
      }
  
    return arrayUpdCondition;
}



/**
 * generate all update predicates with 2 arguments over one path (i.e. updV(iteration,position))
 * for ONE array variable v
 * That is, if v(X) is updated on a path, for example, at positions p1 and p2, 
 * then the update predicate should be upd(i,p) = (p=p1 AND cond1) OR (p=p2 AND cond2)
 **/

Formula* LoopAnalyzer::updatePredicateOfArray2(Path* path, Path::Iterator &sit, Variable* v)
{
    CALL("LoopAnalyzer::arrayPredicateOfArray2");
    Stack<Formula*> updPredicates;
    int updPos =0;
    Formula* arrayUpdPredicate; 
    TermList updatePosition;
    TermList updateValue;
    Formula* updPredFormula;    
    int firstUpdFormula=1;
    while (sit.hasNext() )
      {
	Statement* stat=sit.next();
	switch (stat->kind()) {
	   case Statement::ASSIGNMENT:
	     {
	        Expression* lhs = static_cast<Assignment*>(stat)->lhs();
		Expression* rhs = static_cast<Assignment*>(stat)->rhs();
		TermList rhsTerm=expressionToTerm(rhs);
		if (lhs->kind()==Expression::ARRAY_APPLICATION)
		  { 
		    Expression*  lhsArray=  static_cast<ArrayApplicationExpression*>(lhs)->array();
		    Expression*  lhsArrayArguments=  static_cast<ArrayApplicationExpression*>(lhs)->argument();
		    TermList arrayArgs=expressionToTerm(lhsArrayArguments);
		    string arrayName=lhsArray->toString();
		    if (arrayName == v->name()) {
		      //compute an update predicate of v
		      Path::Iterator pit(path);
		      updatePosition=arrayUpdatePosition(pit, arrayArgs, updPos, 0);
		      TermList x2;
		      x2.makeVar(2);
		      updPredFormula =  new AtomicFormula(createIntEquality(true,x2,updatePosition));
		      //create iteration predicate of one argument
		      TermList x0;
		      x0.makeVar(0);//variable for loop iteration
		      unsigned iterPred = env.signature->addPredicate("iter",1);
		      Literal* iter = Literal::create1(iterPred,true,x0);
		      //create iter(X0) && updatedPosition
		      FormulaList* iterPos=(new FormulaList(updPredFormula))->cons((new AtomicFormula(iter)));
		      updPredFormula = new JunctionFormula(AND,iterPos);
		      //start collecting conditions for update predicates only if the loop has multiple paths (at least 2)
		      if (_paths.length()>1)
			{
			  Path::Iterator cit(path);
			  Formula* updCondition  = arrayUpdateCondition(path, cit, updPos);
			  FormulaList* interForm = (new FormulaList(updPredFormula)) ->cons(updCondition);
			  updPredFormula = new JunctionFormula(AND, interForm);
			}
		      updPredicates.push(updPredFormula);//partial update predicate
		    }
		  }
	     }
	     break;
	   case Statement::BLOCK:
	       break;
	   case Statement::ITE: 		     
	    break;
	}
	updPos = updPos+1;
      }
    //construct the update predicate of array V throughout one path
    // for this take OR over all partial update predicates
    while (!(updPredicates.isEmpty()))
      {
	Formula* updPred= updPredicates.pop();
	if (firstUpdFormula) {arrayUpdPredicate = updPred; firstUpdFormula=0;}
	else {FormulaList* interForm = (new FormulaList(arrayUpdPredicate))->cons(updPred); arrayUpdPredicate=new JunctionFormula(OR,interForm);}
      }
    return arrayUpdPredicate;
    
}



/**
 * generate all update predicates with 3 arguments over one path (i.e. updV(iteration,position,value))
 * for ONE array variable v
 * That is, if v(X) is updated on a path, for example, at positions p1 by x1  and at position p2 by x2, 
 * then the update predicate should be upd(i,p,val) = (p=p1 AND cond1 AND val=x1) OR (p=p2 AND cond2 AND val=x2)
 **/

Formula* LoopAnalyzer::updatePredicateOfArray3(Path* path, Path::Iterator &sit, Variable* v)
{
    CALL("LoopAnalyzer::arrayPredicateOfArray3");
    Stack<Formula*> updPredicates;
    int updPos =0;
    Formula* arrayUpdPredicate; 
    TermList updatePosition;
    TermList updateValue;
    Formula* updPredFormula;    
    int firstUpdFormula=1;
    while (sit.hasNext() )
      {
	Statement* stat=sit.next();
	switch (stat->kind()) {
	   case Statement::ASSIGNMENT:
	     {
	        Expression* lhs = static_cast<Assignment*>(stat)->lhs();
		Expression* rhs = static_cast<Assignment*>(stat)->rhs();
		TermList rhsTerm=expressionToTerm(rhs);
		if (lhs->kind()==Expression::ARRAY_APPLICATION)
		  { 
		    Expression*  lhsArray=  static_cast<ArrayApplicationExpression*>(lhs)->array();
		    Expression*  lhsArrayArguments=  static_cast<ArrayApplicationExpression*>(lhs)->argument();
		    TermList arrayArgs=expressionToTerm(lhsArrayArguments);
		    string arrayName=lhsArray->toString();
		    if (arrayName == v->name()) {
		      //compute an update predicate of v
		      Path::Iterator pit(path);
		      updatePosition=arrayUpdatePosition(pit, arrayArgs, updPos, 0);
		      TermList x2;
		      x2.makeVar(2);
		      updPredFormula =  new AtomicFormula(createIntEquality(true,x2,updatePosition));
		      //create iteration predicate of one argument
		      TermList x0;
		      x0.makeVar(0);//variable for loop iteration
		      unsigned iterPred = env.signature->addPredicate("iter",1);
		      Literal* iter = Literal::create1(iterPred,true,x0);
		      //compute the update value wrt lets
		      Path::Iterator vit(path);
		      TermList updateValue=arrayUpdateValue(vit, rhsTerm, updPos,0);
		      TermList x3;
		      x3.makeVar(3);
		      Formula* updValFormula = new AtomicFormula(createIntEquality(true,x3,updateValue));
		      //create iter(X0) && updatedPosition && updValueFormula
		      FormulaList* iterPos=(new FormulaList(updPredFormula))->cons((new AtomicFormula(iter))) -> cons(updValFormula);
		      updPredFormula = new JunctionFormula(AND,iterPos);
		      //start collecting conditions for update predicates only if the loop has multiple paths (at least 2)
		      if (_paths.length()>1)
			{
			  Path::Iterator cit(path);
			  Formula* updCondition  = arrayUpdateCondition(path, cit, updPos);
			  FormulaList* interForm = (new FormulaList(updPredFormula)) ->cons(updCondition);
			  updPredFormula = new JunctionFormula(AND, interForm);
			}
		      updPredicates.push(updPredFormula);//partial update predicate
		    }
		  }
	     }
	     break;
	   case Statement::BLOCK:
	       break;
	   case Statement::ITE: 		     
	    break;
	}
	updPos = updPos+1;
      }
    //construct the update predicate of array V throughout one path
    // for this take OR over all partial update predicates
    while (!(updPredicates.isEmpty()))
      {
	Formula* updPred= updPredicates.pop();
	if (firstUpdFormula) {arrayUpdPredicate = updPred; firstUpdFormula=0;}
	else {FormulaList* interForm = (new FormulaList(arrayUpdPredicate))->cons(updPred); arrayUpdPredicate=new JunctionFormula(OR,interForm);}
      }    

     return arrayUpdPredicate;
    
}

/*
 *Generate a disjunction over all predicates in the udpate stack
 */
Formula* LoopAnalyzer::updPredicateStack(Stack<Formula*> &updStack)
{
 CALL("LoopAnalyzer::generateUpdatePredicates");
 int firstUpdFormula=1;
 Formula* loopUpdPredicateV;
 while (!(updStack.isEmpty()))
 {
   Formula* updPred= updStack.pop();
   if (firstUpdFormula) {loopUpdPredicateV = updPred; firstUpdFormula=0;}
   else {FormulaList* interForm = (new FormulaList(loopUpdPredicateV))->cons(updPred); loopUpdPredicateV=new JunctionFormula(OR,interForm);}
 }
 //make sequence of let v=v(x) in...UPDATE_PREDICATES, for all vars
 //process first updated array vars
 Map<Variable*,bool>::Iterator vars(*_loop->variables());
 //Path::Iterator pathit(path);
 loopUpdPredicateV = letTranslationOfArray(vars, loopUpdPredicateV);
 //process updated scalars 
 VariableMap::Iterator varit(_variableInfo);
 loopUpdPredicateV = letTranslationOfVar(varit,loopUpdPredicateV);
 return loopUpdPredicateV;
}

/**generate last update property of an array ARRAY
 *using the update predicate UpdPRED of the ARRAY
 */

Formula* LoopAnalyzer::lastUpdateProperty(Literal* updPred, string array, TermList position, TermList updValue)
{
   CALL("LoopAnalyzer::lastUpdateProperty");
   unsigned arrayFct1=getIntFunction(array,1);
   TermList arrayX2(Term::create(arrayFct1,1,&position));
   Literal* lastUpdImplies = createIntEquality(true,arrayX2,updValue);
   Formula* lastUpd = new BinaryFormula(IMP,new AtomicFormula(updPred),new AtomicFormula(lastUpdImplies));
   return lastUpd;
}

/**generate the stability property of an array ARRAY
 *using the update predicate UpdPRED of the ARRAY
 *that is: (iter(ITERATION) => ~UpdPred(ITERATION,POSITION)) => ARRAY(POSITION) = ARRAY0(POSITION)
 */
Formula* LoopAnalyzer::stabilityProperty(Literal* updPred, string array, TermList position, TermList iteration)
{
  CALL("LoopAnalyzer::stabilityProperty");
  //create ARRAY[POSITION]
  unsigned arrayFinal=getIntFunction(array,1);
  TermList arrayFinalFct(Term::create(arrayFinal,1,&position));
  //create ARRAY[POSITION]
  unsigned arrayIni = getIntFunction(array + Int::toString(0),1);
  TermList arrayIniFct(Term::create(arrayIni,1,&position));
  //create formula ARRAY(POSITION) = ARRAY0(POSITION)
  Literal* stabilityImplication = createIntEquality(true,arrayFinalFct,arrayIniFct);
  //create formula  (iter(ITERATION) => ~UpdPred(ITERATION,POSITION))
  unsigned iter = env.signature->addPredicate("iter",1);
  Literal* iterPred = Literal::create1(iter,true,iteration);
  Formula* stabilityCondition = new BinaryFormula(IMP,new AtomicFormula(iterPred), new NegatedFormula(new AtomicFormula(updPred)));
  //create stability property
  Formula* stabilityProp = new BinaryFormula(IMP,stabilityCondition,new AtomicFormula(stabilityImplication));
  return stabilityProp;
}

/** Generate update predicates
 *
 */

void LoopAnalyzer::generateUpdatePredicates()
{
  CALL("LoopAnalyzer::generateUpdatePredicates");
  Map<Variable*,bool>::Iterator vars(*_loop->variables());
  // iterate over variables
  while (vars.hasNext()) {
     Variable* v;
     //for one array variable v, visit all paths, and collect all predicates on a path
     bool updated;
     vars.next(v,updated);
     if (! updated) {
       continue;
     }
     bool array = (v->vtype()->kind() == Type::ARRAY);
     if (!array) {continue;}
     //updated array var
     //collect upd predicates with 2 arguments of v through all paths in updPredicatesV2
     //and 
     //collect upd predicates with 3 arguments of v through all paths in updPredicatesV3
     Stack<Formula*> updPredicatesV2;
     Stack<Formula*> updPredicatesV3;
     Stack<Path*>::Iterator pit(_paths);
     while (pit.hasNext()) {
      Path* path = pit.next();
      //check if array v is updated on path
      int pathUpdate = arrayIsUpdatedOnPath(path,v);
      if (!pathUpdate) {continue;}
      // iterate over path		
      Path::Iterator it2(path);
      Formula* updFormula2 = updatePredicateOfArray2(path, it2, v);
      Path::Iterator it3(path);
      Formula* updFormula3 = updatePredicateOfArray3(path, it3, v);
      updPredicatesV2.push(updFormula2);
      updPredicatesV3.push(updFormula3);
     }
     //all paths have been visited, 
     //updated predicates of v are collected in updPredicateV
     //create now update predicate of v for the entire loop by taking ORs over updPredicateV
     Formula* loopUpdPredicateV2=updPredicateStack(updPredicatesV2);
     Formula* loopUpdPredicateV3=updPredicateStack(updPredicatesV3);
     //create updV predicates in the signature
     string updName="upd"+v->name();
     TermList x2;//variable for update position
     x2.makeVar(2);
     TermList x0;
     x0.makeVar(0);//variable for loop iteration
     TermList x3;
     x3.makeVar(3);//variable for value of update
     unsigned updFun1 = env.signature->addPredicate(updName,2);
     unsigned updFun2 = env.signature->addPredicate(updName,3);
     // term updV(x0,x2) 
     Literal* upd1 = Literal::create2(updFun1,true,x0,x2);
     // term updV(x0,x2,x3)
     TermList args[] = {x0, x2, x3};
     Literal* upd2 = Literal::create(updFun2,3,true,false,args);
     Formula* updDefV1 = new BinaryFormula(IFF,new AtomicFormula(upd1),loopUpdPredicateV2);
     Formula* updDefV2 = new BinaryFormula(IFF,new AtomicFormula(upd2),loopUpdPredicateV3);
     //generate last update property: updV(x0,x2,x3) => V(X2)=X3
     Formula* lastUpd = lastUpdateProperty(upd2,v->name(),x2,x3);
     //generate stability property: (iter(x0)=>~updV(x0,x2)) => V(X2)=V0(X2)
     Formula* stabProp = stabilityProperty(upd1, v->name(), x2, x0);
     _units = _units->cons(new FormulaUnit(updDefV1,
					  new Inference(Inference::PROGRAM_ANALYSIS),
					  Unit::ASSUMPTION));
     _units = _units->cons(new FormulaUnit(updDefV2,
					  new Inference(Inference::PROGRAM_ANALYSIS),
					  Unit::ASSUMPTION));
     _units = _units->cons(new FormulaUnit(lastUpd,
					  new Inference(Inference::PROGRAM_ANALYSIS),
					  Unit::ASSUMPTION));	
     _units = _units->cons(new FormulaUnit(stabProp,
					  new Inference(Inference::PROGRAM_ANALYSIS),
					  Unit::ASSUMPTION));	
     //move/loop to the next updated array
  }
}

/**generate axioms describing the final/initial value of updated variables 
 * that is: 
 * V(n,P) = V(P) and V(zero,P)=V0(P), for any updated array V
 * v(n)=v and v(zero)=v0, for any updated scalar
 */
void LoopAnalyzer::generateValueFunctionRelationsOfVariables()
{
   CALL("LoopAnalyzer::generateValueFunctionRelationsOfVariables");
   //create loop counter n and position x2
   TermList n(Term::createConstant(getIntConstant("n")));
   TermList x2;
   x2.makeVar(2);
   Theory* theory = Theory::instance();
   TermList zero(theory->representConstant(IntegerConstantType(0)));
   // iterate over variables
   Map<Variable*,bool>::Iterator vars(*_loop->variables());
   while (vars.hasNext()) {
     Variable* v;
     bool updated;
     vars.next(v,updated);
     if (! updated) {
       continue;
     }
     bool array = (v->vtype()->kind() == Type::ARRAY);
     bool scalar =  (v->vtype()->kind() == Type::INT); //so far we only have INT as scalar
     if (array) 
       {
	 //create final and initial array functions
	 unsigned arrayFinal1=getIntFunction(v->name(),1);
	 unsigned arrayFinal2=getIntFunction(v->name(),2);
	 TermList arrayFinalFct1(Term::create(arrayFinal1,1,&x2));
	 TermList arrayFinalFct2(Term::create2(arrayFinal2,n,x2));
	 TermList arrayIni(Term::createConstant(getIntConstant(v->name()+Int::toString(0))));
	 TermList arrayIniFct1(Term::create(arrayFinal1,1,&zero));
	 //create V(n,X2)=V(X2)
	 Formula* finalValFctCorresp = new AtomicFormula(createIntEquality(true,arrayFinalFct2,arrayFinalFct1));
	 //create V0 =V(zero)
	 Formula* initialValFctCorresp = new AtomicFormula(createIntEquality(true,arrayIniFct1,arrayIni));
	 _units = _units->cons(new FormulaUnit(finalValFctCorresp,
					  new Inference(Inference::PROGRAM_ANALYSIS),
					  Unit::ASSUMPTION));	
	 _units = _units->cons(new FormulaUnit(initialValFctCorresp,
					  new Inference(Inference::PROGRAM_ANALYSIS),
					  Unit::ASSUMPTION));	
       }
     if (scalar)
       {
	 //create final and initial scalar functions
	 ASS(v->vtype()->kind() == Type::INT);
	 unsigned scalarFinal=getIntFunction(v->name(),1);
	 TermList scalarFinalFct(Term::create(scalarFinal,1,&n));
	 TermList scalarFinalConst(Term::createConstant(getIntConstant(v->name())));
	 TermList scalarIni(Term::createConstant(getIntConstant(v->name()+Int::toString(0))));
	 TermList scalarIniFct(Term::create(scalarFinal,1,&zero));
	 //create v(n)=v
	 Formula* finalValFctCorresp = new AtomicFormula(createIntEquality(true,scalarFinalFct,scalarFinalConst));
	 //create V0 =V(zero)
	 Formula* initialValFctCorresp = new AtomicFormula(createIntEquality(true,scalarIniFct,scalarIni));
	 _units = _units->cons(new FormulaUnit(finalValFctCorresp,
					  new Inference(Inference::PROGRAM_ANALYSIS),
					  Unit::ASSUMPTION));	
	 _units = _units->cons(new FormulaUnit(initialValFctCorresp,
					  new Inference(Inference::PROGRAM_ANALYSIS),
					  Unit::ASSUMPTION));	
       }

   }
}


/**
 *Generate the property of the loop condition.
 * that is, if the loop condition is v<M, 
 * where v is a loop variable and M a constant, 
 * the property should be: iter(X0) => v(X0)<M
 * We use let...in constructs, so the property is: iter(X0) => let v:=v(X) in v(X0)<M
 */

void LoopAnalyzer::generateLoopConditionProperty()
{
  CALL("LoopAnalyzer::generateLoopConditionProperty");
  Expression* condExp = _loop->condition();
  Formula* condition = expressionToPred(condExp);
  //make sequence of let v=v(x) in...CONDITION for all vars
  //process first updated array vars
  Map<Variable*,bool>::Iterator vars(*_loop->variables());
  condition  = letTranslationOfArray(vars, condition);
  //process updated scalars 
  VariableMap::Iterator varit(_variableInfo);
  condition = letTranslationOfVar(varit,condition);
  //create iter(X0)
  TermList x0;
  x0.makeVar(0);
  unsigned iter = env.signature->addPredicate("iter",1);
  Literal* iterPred = Literal::create1(iter,true,x0);
  Formula* loopConditionProp = new BinaryFormula(IMP,new AtomicFormula(iterPred),condition);
   _units = _units->cons(new FormulaUnit(loopConditionProp,
					new Inference(Inference::PROGRAM_ANALYSIS),
					Unit::ASSUMPTION));	
}

/**
 *Generate the definition of iteration:
 * iter(X) <=> geq(X,0) && greater(n,X)
 */
void LoopAnalyzer::generateIterationDefinition()
{
  CALL("LoopAnalyzer::generateIterationDefinition");
  //iter(X0)
  TermList x0;
  x0.makeVar(0);
  unsigned iter = env.signature->addPredicate("iter",1);
  Literal* iterPred = Literal::create1(iter,true,x0);
  Theory* theory = Theory::instance();
  TermList zero(theory->representConstant(IntegerConstantType(0)));
  //0<= X0
  Formula* ineqXZero = new AtomicFormula(theory->pred2(Theory::INT_LESS_EQUAL,true,zero,x0));
  //X0<n
  TermList n(Term::createConstant(getIntConstant("n")));
  Formula* ineqXn = new AtomicFormula(theory->pred2(Theory::INT_LESS,true,x0,n));
  //0<= X0 && X0<n
  Formula* iterDef =  new JunctionFormula(AND, ((new FormulaList(ineqXn)) -> cons(ineqXZero)));
  //generate the prop: iter <=> ...
  Formula* iterProp = new BinaryFormula(IFF, new AtomicFormula(iterPred), iterDef);
 _units = _units->cons(new FormulaUnit(iterProp,
					new Inference(Inference::PROGRAM_ANALYSIS),
					Unit::ASSUMPTION));	
  
}


/**
 * Generate axioms for counters.
 */
void LoopAnalyzer::generateAxiomsForCounters()
{
  CALL("LoopAnalyzer::generateAxiomsForCounters");

  unsigned length = _paths.length();
  if (length == 0) {
    return;
  }
  DArray<int> increments(length);
  VariableMap::Iterator vit(_variableInfo);
  while (vit.hasNext()) {
    Variable* v;
    VariableInfo* vinfo;
    vit.next(v,vinfo);
    if (! vinfo->counter) {
      continue;
    }
    for (int i = length-1;i >= 0;i--) {
      increments[i] = 0;
    }
    int pathNumber = 0;
    Stack<Path*>::Iterator pit(_paths);
    while (pit.hasNext()) {
      int inc = 0;
      Path* path = pit.next();
      Path::Iterator sit(path);
      while (sit.hasNext()) {
	Statement* stat = sit.next();
	if (v != isScalarAssignment(stat)) {
	  continue;
	}
	inc += isScalarIncrement(static_cast<Assignment*>(stat));
      }
      increments[pathNumber++] = inc;
    }
    int max = increments[length-1];
    int min = max;
    int gcd = max > 0 ? max : -max;
    //make case distinction between loops with IFs, and no IFs.
    //loops with IFs have at least 2 inner paths, thus length>=2, and hence length-2>=0. Then for-loop starting from length-2 makes sense.
    //loops with no IFs have only one path (the body), thus length=1, and hence length-2<0 makes no sense to start for-loop from. For such cases, for-loop starts from 0.
    for (int i = length>1 ? length-2:0;i >= 0;i--) {
      int inc = increments[i];
      if (inc > max) {
	max = inc;
      }
      if (inc < min) {
	min = inc;
      }
      if (inc > 0) {
	gcd = Int::gcd(gcd,inc);
      }
      else if (inc < 0) {
	gcd = Int::gcd(gcd,-inc);
      }
      cout << "Counter " << v->name() << ": " << min << " min, " << max << " max, " << gcd << " gcd\n";
      generateCounterAxiom(v->name(),min,max,gcd);
    }
  }
}

/**
 * Generate the axioms for a counter for the purpose of symbol elimination.
 * @param name the counter name
 * @param min the minimal increment of the counter over all paths
 * @param max the maximal increment of the counter over all paths
 * @param gcd greatest common divisor of all increments of the counter over all paths
 */
void LoopAnalyzer::generateCounterAxiom(const string& name,int min,int max,int gcd)
{
  CALL("LoopAnalyzer::generateCounterAxiom");

  // value of the counter at position 0
  TermList c(Term::createConstant(getIntConstant(name + Int::toString(0))));
  unsigned fun = getIntFunction(name,1);
  // term x0
  TermList x0;
  x0.makeVar(0);
  // term c(x0)
  TermList cx0(Term::create(fun,1,&x0));
  Theory* theory = Theory::instance();
  if (min == max) {
    Clause* cls = new(1) Clause(1,Unit::ASSUMPTION,new Inference(Inference::PROGRAM_ANALYSIS));
    Literal* eq;
    if (gcd == 1 || gcd == -1) {
      // c +- x0
      TermList sum(theory->fun2(gcd == 1 ? Theory::INT_PLUS : Theory::INT_MINUS,c,x0));
      // create c(x0) = c + x_0
      eq = createIntEquality(true,cx0,sum);
    }
    else {
      // gcd != 1
      // term gcd*x0
      TermList c_gcd(theory->representConstant(IntegerConstantType(gcd)));
      TermList gcd_x0(theory->fun2(Theory::INT_MULTIPLY,c_gcd,x0));
      // c +- gcd*x0
      TermList sum(theory->fun2(gcd > 0 ? Theory::INT_PLUS : Theory::INT_MINUS,c,gcd_x0));
      // create c(x0) = c + gcd*x_0
      eq = createIntEquality(true,cx0,sum);
    }
    (*cls)[0] = eq;
    _units = _units->cons(cls);
    return;
  }

  // min != max
  // Generate the upper bound axiom c(x0) <= c + max*x_0
  if (max == 0) { // c(x0) <= c
    Literal* ineq = theory->pred2(Theory::INT_LESS_EQUAL,true,cx0,c);
    Clause* cls = new(1) Clause(1,Unit::ASSUMPTION,new Inference(Inference::PROGRAM_ANALYSIS));
    (*cls)[0] = ineq;
    _units = _units->cons(cls);
  }
  else if (max == 1 || max == -1) { // c(x0) <= c +- x_0
    TermList sum(theory->fun2(max == 1 ? Theory::INT_PLUS : Theory::INT_MINUS,c,x0));
    Literal* ineq = theory->pred2(Theory::INT_LESS_EQUAL,true,cx0,sum);
    Clause* cls = new(1) Clause(1,Unit::ASSUMPTION,new Inference(Inference::PROGRAM_ANALYSIS));
    (*cls)[0] = ineq;
    _units = _units->cons(cls);
  }
  else {
    TermList c_max(theory->representConstant(IntegerConstantType(max)));
    TermList max_x0(theory->fun2(Theory::INT_MULTIPLY,c_max,x0));
    // c +- max*x0
    TermList sum(theory->fun2(max > 0 ? Theory::INT_PLUS : Theory::INT_MINUS,c,max_x0));
    Literal* ineq = theory->pred2(Theory::INT_LESS_EQUAL,true,cx0,sum);
    Clause* cls = new(1) Clause(1,Unit::ASSUMPTION,new Inference(Inference::PROGRAM_ANALYSIS));
    (*cls)[0] = ineq;
    _units = _units->cons(cls);
  }

  // and the lower bound axiom c(x0) >= c + min*x_0
  if (min == 0) { // c(x0) >= c
    Literal* ineq = theory->pred2(Theory::INT_GREATER_EQUAL,true,cx0,c);
    Clause* cls = new(1) Clause(1,Unit::ASSUMPTION,new Inference(Inference::PROGRAM_ANALYSIS));
    (*cls)[0] = ineq;
    _units = _units->cons(cls);
  }
  else if (min == 1 || min == -1) { // c(x0) >= c +- x_0
    TermList sum(theory->fun2(min == 1 ? Theory::INT_PLUS : Theory::INT_MINUS,c,x0));
    Literal* ineq = theory->pred2(Theory::INT_GREATER_EQUAL,true,cx0,sum);
    Clause* cls = new(1) Clause(1,Unit::ASSUMPTION,new Inference(Inference::PROGRAM_ANALYSIS));
    (*cls)[0] = ineq;
    _units = _units->cons(cls);
  }
  else {
    TermList c_min(theory->representConstant(IntegerConstantType(min)));
    TermList min_x0(theory->fun2(Theory::INT_MULTIPLY,c_min,x0));
    // c +- min*x0
    TermList sum(theory->fun2(min > 0 ? Theory::INT_PLUS : Theory::INT_MINUS,c,min_x0));
    Literal* ineq = theory->pred2(Theory::INT_GREATER_EQUAL,true,cx0,sum);
    Clause* cls = new(1) Clause(1,Unit::ASSUMPTION,new Inference(Inference::PROGRAM_ANALYSIS));
    (*cls)[0] = ineq;
    _units = _units->cons(cls);
  }

  // generate density axioms
  if (max == 1) {
    // generate J > I & c(J) > V & V > c(I) -> (E K)(J > K & K > I & c(K) = V)
    TermList I;
    I.makeVar(0);
    TermList J;
    J.makeVar(1);
    TermList K;
    K.makeVar(2);
    TermList V;
    V.makeVar(3);
    TermList cI(Term::create(fun,1,&I));
    TermList cJ(Term::create(fun,1,&J));
    TermList cK(Term::create(fun,1,&K));
    Formula* JgreaterI = new AtomicFormula(theory->pred2(Theory::INT_GREATER,true,J,I));
    Formula* cJgreaterV = new AtomicFormula(theory->pred2(Theory::INT_GREATER,true,cJ,V));
    Formula* VgreatercI = new AtomicFormula(theory->pred2(Theory::INT_GREATER,true,V,cI));
    FormulaList* left = (new FormulaList(VgreatercI))->cons(cJgreaterV)->cons(JgreaterI);
    Formula* lhs = new JunctionFormula(AND,left);
    Formula* JgreaterK = new AtomicFormula(theory->pred2(Theory::INT_GREATER,true,J,K));
    Formula* KgreaterI = new AtomicFormula(theory->pred2(Theory::INT_GREATER,true,K,I));
    Formula* cKequalV = new AtomicFormula(createIntEquality(true,cK,V));
    FormulaList* right = (new FormulaList(JgreaterK))->cons(KgreaterI)->cons(cKequalV);
    Formula* rhs = new QuantifiedFormula(EXISTS,
					 new Formula::VarList(2),
					 new JunctionFormula(AND,right));
    _units = _units->cons(new FormulaUnit(new BinaryFormula(IMP,lhs,rhs),
					  new Inference(Inference::PROGRAM_ANALYSIS),
					  Unit::ASSUMPTION));
  }
  if (min == -1) {
    // generate J > I & c(J) < V & V < c(I) -> (E K)(J > K & K > I & c(K) = V)
    TermList I;
    I.makeVar(0);
    TermList J;
    J.makeVar(1);
    TermList K;
    K.makeVar(2);
    TermList V;
    V.makeVar(3);
    TermList cI(Term::create(fun,1,&I));
    TermList cJ(Term::create(fun,1,&J));
    TermList cK(Term::create(fun,1,&K));
    Formula* JgreaterI = new AtomicFormula(theory->pred2(Theory::INT_GREATER,true,J,I));
    Formula* cJlessV = new AtomicFormula(theory->pred2(Theory::INT_LESS,true,cJ,V));
    Formula* VlesscI = new AtomicFormula(theory->pred2(Theory::INT_LESS,true,V,cI));
    FormulaList* left = (new FormulaList(VlesscI))->cons(cJlessV)->cons(JgreaterI);
    Formula* lhs = new JunctionFormula(AND,left);
    Formula* JgreaterK = new AtomicFormula(theory->pred2(Theory::INT_GREATER,true,J,K));
    Formula* KgreaterI = new AtomicFormula(theory->pred2(Theory::INT_GREATER,true,K,I));
    Formula* cKequalV = new AtomicFormula(createIntEquality(true,cK,V));
    FormulaList* right = (new FormulaList(JgreaterK))->cons(KgreaterI)->cons(cKequalV);
    Formula* rhs = new QuantifiedFormula(EXISTS,
					 new Formula::VarList(2),
					 new JunctionFormula(AND,right));
    _units = _units->cons(new FormulaUnit(new BinaryFormula(IMP,lhs,rhs),
					  new Inference(Inference::PROGRAM_ANALYSIS),
					  Unit::ASSUMPTION));
  }
}





/**
 * Convert a program expression into a Vampire expression and relativize it to
 * the loop counter. This means that every updatable program variable gets an
 * additional argument: variable x0 and each constant variable is translated
 * into itself
 */
Term* LoopAnalyzer::relativize(Expression* expr)
{
  CALL("LoopAnalyzer::relativize");

  switch (expr->kind()) {
  case Expression::CONSTANT_INTEGER:
    {
      int val = static_cast<ConstantIntegerExpression*>(expr)->value();
      Theory* theory = Theory::instance();
      return theory->representConstant(IntegerConstantType(val));
    }
  
  case Expression::VARIABLE:
    {
      VariableExpression* vexp = static_cast<VariableExpression*>(expr);
      // we can only work with integer variables
      ASS(vexp->etype()->kind() == Type::INT);
      // if (_updatedVariables.contains(
    }

  case Expression::FUNCTION_APPLICATION:
  case Expression::ARRAY_APPLICATION:
    return 0;

  case Expression::CONSTANT_FUNCTION:
    ASS(false);
  }
}

unsigned LoopAnalyzer::getIntFunction(string name, unsigned arity)
{
  bool added;
  unsigned res = env.signature->addFunction(name, arity, added);
  if(added) {
    static DArray<unsigned> domSorts;
    domSorts.init(arity, Sorts::SRT_INTEGER);
    env.signature->getFunction(res)->setType(BaseType::makeType(arity, domSorts.array(), Sorts::SRT_INTEGER));
  }
#if VDEBUG
  else {
    Kernel::FunctionType* t = env.signature->getFunction(res)->fnType();
    ASS_EQ(t->result(), Sorts::SRT_INTEGER);
    for(unsigned i=0; i<arity; ++i) {
      ASS_EQ(t->arg(i), Sorts::SRT_INTEGER);
    }
  }
#endif
  return res;
}

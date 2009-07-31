//===- AsmParser.cpp - Parser for Assembly Files --------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This class implements the parser for assembly files.
//
//===----------------------------------------------------------------------===//

#include "AsmParser.h"

#include "AsmExpr.h"
#include "llvm/ADT/Twine.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetAsmParser.h"
using namespace llvm;

void AsmParser::Warning(SMLoc L, const Twine &Msg) {
  Lexer.PrintMessage(L, Msg.str(), "warning");
}

bool AsmParser::Error(SMLoc L, const Twine &Msg) {
  Lexer.PrintMessage(L, Msg.str(), "error");
  return true;
}

bool AsmParser::TokError(const char *Msg) {
  Lexer.PrintMessage(Lexer.getLoc(), Msg, "error");
  return true;
}

bool AsmParser::Run() {
  // Prime the lexer.
  Lexer.Lex();
  
  bool HadError = false;
  
  // While we have input, parse each statement.
  while (Lexer.isNot(AsmToken::Eof)) {
    if (!ParseStatement()) continue;
  
    // If we had an error, remember it and recover by skipping to the next line.
    HadError = true;
    EatToEndOfStatement();
  }
  
  return HadError;
}

/// EatToEndOfStatement - Throw away the rest of the line for testing purposes.
void AsmParser::EatToEndOfStatement() {
  while (Lexer.isNot(AsmToken::EndOfStatement) &&
         Lexer.isNot(AsmToken::Eof))
    Lexer.Lex();
  
  // Eat EOL.
  if (Lexer.is(AsmToken::EndOfStatement))
    Lexer.Lex();
}


/// ParseParenExpr - Parse a paren expression and return it.
/// NOTE: This assumes the leading '(' has already been consumed.
///
/// parenexpr ::= expr)
///
bool AsmParser::ParseParenExpr(AsmExpr *&Res) {
  if (ParseExpression(Res)) return true;
  if (Lexer.isNot(AsmToken::RParen))
    return TokError("expected ')' in parentheses expression");
  Lexer.Lex();
  return false;
}

/// ParsePrimaryExpr - Parse a primary expression and return it.
///  primaryexpr ::= (parenexpr
///  primaryexpr ::= symbol
///  primaryexpr ::= number
///  primaryexpr ::= ~,+,- primaryexpr
bool AsmParser::ParsePrimaryExpr(AsmExpr *&Res) {
  switch (Lexer.getKind()) {
  default:
    return TokError("unknown token in expression");
  case AsmToken::Exclaim:
    Lexer.Lex(); // Eat the operator.
    if (ParsePrimaryExpr(Res))
      return true;
    Res = new AsmUnaryExpr(AsmUnaryExpr::LNot, Res);
    return false;
  case AsmToken::Identifier: {
    // This is a label, this should be parsed as part of an expression, to
    // handle things like LFOO+4.
    MCSymbol *Sym = Ctx.GetOrCreateSymbol(Lexer.getTok().getString());

    // If this is use of an undefined symbol then mark it external.
    if (!Sym->getSection() && !Ctx.GetSymbolValue(Sym))
      Sym->setExternal(true);
    
    Res = new AsmSymbolRefExpr(Sym);
    Lexer.Lex(); // Eat identifier.
    return false;
  }
  case AsmToken::Integer:
    Res = new AsmConstantExpr(Lexer.getTok().getIntVal());
    Lexer.Lex(); // Eat identifier.
    return false;
  case AsmToken::LParen:
    Lexer.Lex(); // Eat the '('.
    return ParseParenExpr(Res);
  case AsmToken::Minus:
    Lexer.Lex(); // Eat the operator.
    if (ParsePrimaryExpr(Res))
      return true;
    Res = new AsmUnaryExpr(AsmUnaryExpr::Minus, Res);
    return false;
  case AsmToken::Plus:
    Lexer.Lex(); // Eat the operator.
    if (ParsePrimaryExpr(Res))
      return true;
    Res = new AsmUnaryExpr(AsmUnaryExpr::Plus, Res);
    return false;
  case AsmToken::Tilde:
    Lexer.Lex(); // Eat the operator.
    if (ParsePrimaryExpr(Res))
      return true;
    Res = new AsmUnaryExpr(AsmUnaryExpr::Not, Res);
    return false;
  }
}

/// ParseExpression - Parse an expression and return it.
/// 
///  expr ::= expr +,- expr          -> lowest.
///  expr ::= expr |,^,&,! expr      -> middle.
///  expr ::= expr *,/,%,<<,>> expr  -> highest.
///  expr ::= primaryexpr
///
bool AsmParser::ParseExpression(AsmExpr *&Res) {
  Res = 0;
  return ParsePrimaryExpr(Res) ||
         ParseBinOpRHS(1, Res);
}

bool AsmParser::ParseAbsoluteExpression(int64_t &Res) {
  AsmExpr *Expr;
  
  SMLoc StartLoc = Lexer.getLoc();
  if (ParseExpression(Expr))
    return true;

  if (!Expr->EvaluateAsAbsolute(Ctx, Res))
    return Error(StartLoc, "expected absolute expression");

  return false;
}

bool AsmParser::ParseRelocatableExpression(MCValue &Res) {
  AsmExpr *Expr;
  
  SMLoc StartLoc = Lexer.getLoc();
  if (ParseExpression(Expr))
    return true;

  if (!Expr->EvaluateAsRelocatable(Ctx, Res))
    return Error(StartLoc, "expected relocatable expression");

  return false;
}

bool AsmParser::ParseParenRelocatableExpression(MCValue &Res) {
  AsmExpr *Expr;
  
  SMLoc StartLoc = Lexer.getLoc();
  if (ParseParenExpr(Expr))
    return true;

  if (!Expr->EvaluateAsRelocatable(Ctx, Res))
    return Error(StartLoc, "expected relocatable expression");

  return false;
}

static unsigned getBinOpPrecedence(AsmToken::TokenKind K, 
                                   AsmBinaryExpr::Opcode &Kind) {
  switch (K) {
  default: return 0;    // not a binop.

    // Lowest Precedence: &&, ||
  case AsmToken::AmpAmp:
    Kind = AsmBinaryExpr::LAnd;
    return 1;
  case AsmToken::PipePipe:
    Kind = AsmBinaryExpr::LOr;
    return 1;

    // Low Precedence: +, -, ==, !=, <>, <, <=, >, >=
  case AsmToken::Plus:
    Kind = AsmBinaryExpr::Add;
    return 2;
  case AsmToken::Minus:
    Kind = AsmBinaryExpr::Sub;
    return 2;
  case AsmToken::EqualEqual:
    Kind = AsmBinaryExpr::EQ;
    return 2;
  case AsmToken::ExclaimEqual:
  case AsmToken::LessGreater:
    Kind = AsmBinaryExpr::NE;
    return 2;
  case AsmToken::Less:
    Kind = AsmBinaryExpr::LT;
    return 2;
  case AsmToken::LessEqual:
    Kind = AsmBinaryExpr::LTE;
    return 2;
  case AsmToken::Greater:
    Kind = AsmBinaryExpr::GT;
    return 2;
  case AsmToken::GreaterEqual:
    Kind = AsmBinaryExpr::GTE;
    return 2;

    // Intermediate Precedence: |, &, ^
    //
    // FIXME: gas seems to support '!' as an infix operator?
  case AsmToken::Pipe:
    Kind = AsmBinaryExpr::Or;
    return 3;
  case AsmToken::Caret:
    Kind = AsmBinaryExpr::Xor;
    return 3;
  case AsmToken::Amp:
    Kind = AsmBinaryExpr::And;
    return 3;

    // Highest Precedence: *, /, %, <<, >>
  case AsmToken::Star:
    Kind = AsmBinaryExpr::Mul;
    return 4;
  case AsmToken::Slash:
    Kind = AsmBinaryExpr::Div;
    return 4;
  case AsmToken::Percent:
    Kind = AsmBinaryExpr::Mod;
    return 4;
  case AsmToken::LessLess:
    Kind = AsmBinaryExpr::Shl;
    return 4;
  case AsmToken::GreaterGreater:
    Kind = AsmBinaryExpr::Shr;
    return 4;
  }
}


/// ParseBinOpRHS - Parse all binary operators with precedence >= 'Precedence'.
/// Res contains the LHS of the expression on input.
bool AsmParser::ParseBinOpRHS(unsigned Precedence, AsmExpr *&Res) {
  while (1) {
    AsmBinaryExpr::Opcode Kind = AsmBinaryExpr::Add;
    unsigned TokPrec = getBinOpPrecedence(Lexer.getKind(), Kind);
    
    // If the next token is lower precedence than we are allowed to eat, return
    // successfully with what we ate already.
    if (TokPrec < Precedence)
      return false;
    
    Lexer.Lex();
    
    // Eat the next primary expression.
    AsmExpr *RHS;
    if (ParsePrimaryExpr(RHS)) return true;
    
    // If BinOp binds less tightly with RHS than the operator after RHS, let
    // the pending operator take RHS as its LHS.
    AsmBinaryExpr::Opcode Dummy;
    unsigned NextTokPrec = getBinOpPrecedence(Lexer.getKind(), Dummy);
    if (TokPrec < NextTokPrec) {
      if (ParseBinOpRHS(Precedence+1, RHS)) return true;
    }

    // Merge LHS and RHS according to operator.
    Res = new AsmBinaryExpr(Kind, Res, RHS);
  }
}

  
  
  
/// ParseStatement:
///   ::= EndOfStatement
///   ::= Label* Directive ...Operands... EndOfStatement
///   ::= Label* Identifier OperandList* EndOfStatement
bool AsmParser::ParseStatement() {
  switch (Lexer.getKind()) {
  default:
    return TokError("unexpected token at start of statement");
  case AsmToken::EndOfStatement:
    Lexer.Lex();
    return false;
  case AsmToken::Identifier:
    break;
  // TODO: Recurse on local labels etc.
  }
  
  // If we have an identifier, handle it as the key symbol.
  AsmToken ID = Lexer.getTok();
  SMLoc IDLoc = ID.getLoc();
  StringRef IDVal = ID.getString();
  
  // Consume the identifier, see what is after it.
  switch (Lexer.Lex().getKind()) {
  case AsmToken::Colon: {
    // identifier ':'   -> Label.
    Lexer.Lex();

    // Diagnose attempt to use a variable as a label.
    //
    // FIXME: Diagnostics. Note the location of the definition as a label.
    // FIXME: This doesn't diagnose assignment to a symbol which has been
    // implicitly marked as external.
    MCSymbol *Sym = Ctx.GetOrCreateSymbol(IDVal);
    if (Sym->getSection())
      return Error(IDLoc, "invalid symbol redefinition");
    if (Ctx.GetSymbolValue(Sym))
      return Error(IDLoc, "symbol already used as assembler variable");
    
    // Since we saw a label, create a symbol and emit it.
    // FIXME: If the label starts with L it is an assembler temporary label.
    // Why does the client of this api need to know this?
    Out.EmitLabel(Sym);
   
    return ParseStatement();
  }

  case AsmToken::Equal:
    // identifier '=' ... -> assignment statement
    Lexer.Lex();

    return ParseAssignment(IDVal, false);

  default: // Normal instruction or directive.
    break;
  }
  
  // Otherwise, we have a normal instruction or directive.  
  if (IDVal[0] == '.') {
    // FIXME: This should be driven based on a hash lookup and callback.
    if (IDVal == ".section")
      return ParseDirectiveDarwinSection();
    if (IDVal == ".text")
      // FIXME: This changes behavior based on the -static flag to the
      // assembler.
      return ParseDirectiveSectionSwitch("__TEXT,__text",
                                         "regular,pure_instructions");
    if (IDVal == ".const")
      return ParseDirectiveSectionSwitch("__TEXT,__const");
    if (IDVal == ".static_const")
      return ParseDirectiveSectionSwitch("__TEXT,__static_const");
    if (IDVal == ".cstring")
      return ParseDirectiveSectionSwitch("__TEXT,__cstring", 
                                         "cstring_literals");
    if (IDVal == ".literal4")
      return ParseDirectiveSectionSwitch("__TEXT,__literal4", "4byte_literals");
    if (IDVal == ".literal8")
      return ParseDirectiveSectionSwitch("__TEXT,__literal8", "8byte_literals");
    if (IDVal == ".literal16")
      return ParseDirectiveSectionSwitch("__TEXT,__literal16",
                                         "16byte_literals");
    if (IDVal == ".constructor")
      return ParseDirectiveSectionSwitch("__TEXT,__constructor");
    if (IDVal == ".destructor")
      return ParseDirectiveSectionSwitch("__TEXT,__destructor");
    if (IDVal == ".fvmlib_init0")
      return ParseDirectiveSectionSwitch("__TEXT,__fvmlib_init0");
    if (IDVal == ".fvmlib_init1")
      return ParseDirectiveSectionSwitch("__TEXT,__fvmlib_init1");
    if (IDVal == ".symbol_stub") // FIXME: Different on PPC.
      return ParseDirectiveSectionSwitch("__IMPORT,__jump_table,symbol_stubs",
                                    "self_modifying_code+pure_instructions,5");
    // FIXME: .picsymbol_stub on PPC.
    if (IDVal == ".data")
      return ParseDirectiveSectionSwitch("__DATA,__data");
    if (IDVal == ".static_data")
      return ParseDirectiveSectionSwitch("__DATA,__static_data");
    if (IDVal == ".non_lazy_symbol_pointer")
      return ParseDirectiveSectionSwitch("__DATA,__nl_symbol_pointer",
                                         "non_lazy_symbol_pointers");
    if (IDVal == ".lazy_symbol_pointer")
      return ParseDirectiveSectionSwitch("__DATA,__la_symbol_pointer",
                                         "lazy_symbol_pointers");
    if (IDVal == ".dyld")
      return ParseDirectiveSectionSwitch("__DATA,__dyld");
    if (IDVal == ".mod_init_func")
      return ParseDirectiveSectionSwitch("__DATA,__mod_init_func",
                                         "mod_init_funcs");
    if (IDVal == ".mod_term_func")
      return ParseDirectiveSectionSwitch("__DATA,__mod_term_func",
                                         "mod_term_funcs");
    if (IDVal == ".const_data")
      return ParseDirectiveSectionSwitch("__DATA,__const", "regular");
    
    
    // FIXME: Verify attributes on sections.
    if (IDVal == ".objc_class")
      return ParseDirectiveSectionSwitch("__OBJC,__class");
    if (IDVal == ".objc_meta_class")
      return ParseDirectiveSectionSwitch("__OBJC,__meta_class");
    if (IDVal == ".objc_cat_cls_meth")
      return ParseDirectiveSectionSwitch("__OBJC,__cat_cls_meth");
    if (IDVal == ".objc_cat_inst_meth")
      return ParseDirectiveSectionSwitch("__OBJC,__cat_inst_meth");
    if (IDVal == ".objc_protocol")
      return ParseDirectiveSectionSwitch("__OBJC,__protocol");
    if (IDVal == ".objc_string_object")
      return ParseDirectiveSectionSwitch("__OBJC,__string_object");
    if (IDVal == ".objc_cls_meth")
      return ParseDirectiveSectionSwitch("__OBJC,__cls_meth");
    if (IDVal == ".objc_inst_meth")
      return ParseDirectiveSectionSwitch("__OBJC,__inst_meth");
    if (IDVal == ".objc_cls_refs")
      return ParseDirectiveSectionSwitch("__OBJC,__cls_refs");
    if (IDVal == ".objc_message_refs")
      return ParseDirectiveSectionSwitch("__OBJC,__message_refs");
    if (IDVal == ".objc_symbols")
      return ParseDirectiveSectionSwitch("__OBJC,__symbols");
    if (IDVal == ".objc_category")
      return ParseDirectiveSectionSwitch("__OBJC,__category");
    if (IDVal == ".objc_class_vars")
      return ParseDirectiveSectionSwitch("__OBJC,__class_vars");
    if (IDVal == ".objc_instance_vars")
      return ParseDirectiveSectionSwitch("__OBJC,__instance_vars");
    if (IDVal == ".objc_module_info")
      return ParseDirectiveSectionSwitch("__OBJC,__module_info");
    if (IDVal == ".objc_class_names")
      return ParseDirectiveSectionSwitch("__TEXT,__cstring","cstring_literals");
    if (IDVal == ".objc_meth_var_types")
      return ParseDirectiveSectionSwitch("__TEXT,__cstring","cstring_literals");
    if (IDVal == ".objc_meth_var_names")
      return ParseDirectiveSectionSwitch("__TEXT,__cstring","cstring_literals");
    if (IDVal == ".objc_selector_strs")
      return ParseDirectiveSectionSwitch("__OBJC,__selector_strs");
    
    // Assembler features
    if (IDVal == ".set")
      return ParseDirectiveSet();

    // Data directives

    if (IDVal == ".ascii")
      return ParseDirectiveAscii(false);
    if (IDVal == ".asciz")
      return ParseDirectiveAscii(true);

    // FIXME: Target hooks for size? Also for "word", "hword".
    if (IDVal == ".byte")
      return ParseDirectiveValue(1);
    if (IDVal == ".short")
      return ParseDirectiveValue(2);
    if (IDVal == ".long")
      return ParseDirectiveValue(4);
    if (IDVal == ".quad")
      return ParseDirectiveValue(8);

    // FIXME: Target hooks for IsPow2.
    if (IDVal == ".align")
      return ParseDirectiveAlign(/*IsPow2=*/true, /*ExprSize=*/1);
    if (IDVal == ".align32")
      return ParseDirectiveAlign(/*IsPow2=*/true, /*ExprSize=*/4);
    if (IDVal == ".balign")
      return ParseDirectiveAlign(/*IsPow2=*/false, /*ExprSize=*/1);
    if (IDVal == ".balignw")
      return ParseDirectiveAlign(/*IsPow2=*/false, /*ExprSize=*/2);
    if (IDVal == ".balignl")
      return ParseDirectiveAlign(/*IsPow2=*/false, /*ExprSize=*/4);
    if (IDVal == ".p2align")
      return ParseDirectiveAlign(/*IsPow2=*/true, /*ExprSize=*/1);
    if (IDVal == ".p2alignw")
      return ParseDirectiveAlign(/*IsPow2=*/true, /*ExprSize=*/2);
    if (IDVal == ".p2alignl")
      return ParseDirectiveAlign(/*IsPow2=*/true, /*ExprSize=*/4);

    if (IDVal == ".org")
      return ParseDirectiveOrg();

    if (IDVal == ".fill")
      return ParseDirectiveFill();
    if (IDVal == ".space")
      return ParseDirectiveSpace();

    // Symbol attribute directives
    if (IDVal == ".globl" || IDVal == ".global")
      return ParseDirectiveSymbolAttribute(MCStreamer::Global);
    if (IDVal == ".hidden")
      return ParseDirectiveSymbolAttribute(MCStreamer::Hidden);
    if (IDVal == ".indirect_symbol")
      return ParseDirectiveSymbolAttribute(MCStreamer::IndirectSymbol);
    if (IDVal == ".internal")
      return ParseDirectiveSymbolAttribute(MCStreamer::Internal);
    if (IDVal == ".lazy_reference")
      return ParseDirectiveSymbolAttribute(MCStreamer::LazyReference);
    if (IDVal == ".no_dead_strip")
      return ParseDirectiveSymbolAttribute(MCStreamer::NoDeadStrip);
    if (IDVal == ".private_extern")
      return ParseDirectiveSymbolAttribute(MCStreamer::PrivateExtern);
    if (IDVal == ".protected")
      return ParseDirectiveSymbolAttribute(MCStreamer::Protected);
    if (IDVal == ".reference")
      return ParseDirectiveSymbolAttribute(MCStreamer::Reference);
    if (IDVal == ".weak")
      return ParseDirectiveSymbolAttribute(MCStreamer::Weak);
    if (IDVal == ".weak_definition")
      return ParseDirectiveSymbolAttribute(MCStreamer::WeakDefinition);
    if (IDVal == ".weak_reference")
      return ParseDirectiveSymbolAttribute(MCStreamer::WeakReference);

    if (IDVal == ".comm")
      return ParseDirectiveComm(/*IsLocal=*/false);
    if (IDVal == ".lcomm")
      return ParseDirectiveComm(/*IsLocal=*/true);
    if (IDVal == ".zerofill")
      return ParseDirectiveDarwinZerofill();
    if (IDVal == ".desc")
      return ParseDirectiveDarwinSymbolDesc();
    if (IDVal == ".lsym")
      return ParseDirectiveDarwinLsym();

    if (IDVal == ".subsections_via_symbols")
      return ParseDirectiveDarwinSubsectionsViaSymbols();
    if (IDVal == ".abort")
      return ParseDirectiveAbort();
    if (IDVal == ".include")
      return ParseDirectiveInclude();
    if (IDVal == ".dump")
      return ParseDirectiveDarwinDumpOrLoad(IDLoc, /*IsDump=*/true);
    if (IDVal == ".load")
      return ParseDirectiveDarwinDumpOrLoad(IDLoc, /*IsLoad=*/false);

    Warning(IDLoc, "ignoring directive for now");
    EatToEndOfStatement();
    return false;
  }

  MCInst Inst;
  if (getTargetParser().ParseInstruction(IDVal, Inst))
    return true;
  
  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in argument list");

  // Eat the end of statement marker.
  Lexer.Lex();
  
  // Instruction is good, process it.
  Out.EmitInstruction(Inst);
  
  // Skip to end of line for now.
  return false;
}

bool AsmParser::ParseAssignment(const StringRef &Name, bool IsDotSet) {
  // FIXME: Use better location, we should use proper tokens.
  SMLoc EqualLoc = Lexer.getLoc();

  MCValue Value;
  if (ParseRelocatableExpression(Value))
    return true;
  
  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in assignment");

  // Eat the end of statement marker.
  Lexer.Lex();

  // Diagnose assignment to a label.
  //
  // FIXME: Diagnostics. Note the location of the definition as a label.
  // FIXME: This doesn't diagnose assignment to a symbol which has been
  // implicitly marked as external.
  // FIXME: Handle '.'.
  // FIXME: Diagnose assignment to protected identifier (e.g., register name).
  MCSymbol *Sym = Ctx.GetOrCreateSymbol(Name);
  if (Sym->getSection())
    return Error(EqualLoc, "invalid assignment to symbol emitted as a label");
  if (Sym->isExternal())
    return Error(EqualLoc, "invalid assignment to external symbol");

  // Do the assignment.
  Out.EmitAssignment(Sym, Value, IsDotSet);

  return false;
}

/// ParseDirectiveSet:
///   ::= .set identifier ',' expression
bool AsmParser::ParseDirectiveSet() {
  if (Lexer.isNot(AsmToken::Identifier))
    return TokError("expected identifier after '.set' directive");

  StringRef Name = Lexer.getTok().getString();
  
  if (Lexer.Lex().isNot(AsmToken::Comma))
    return TokError("unexpected token in '.set'");
  Lexer.Lex();

  return ParseAssignment(Name, true);
}

/// ParseDirectiveSection:
///   ::= .section identifier (',' identifier)*
/// FIXME: This should actually parse out the segment, section, attributes and
/// sizeof_stub fields.
bool AsmParser::ParseDirectiveDarwinSection() {
  if (Lexer.isNot(AsmToken::Identifier))
    return TokError("expected identifier after '.section' directive");
  
  std::string Section = Lexer.getTok().getString();
  Lexer.Lex();
  
  // Accept a comma separated list of modifiers.
  while (Lexer.is(AsmToken::Comma)) {
    Lexer.Lex();
    
    if (Lexer.isNot(AsmToken::Identifier))
      return TokError("expected identifier in '.section' directive");
    Section += ',';
    Section += Lexer.getTok().getString().str();
    Lexer.Lex();
  }
  
  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.section' directive");
  Lexer.Lex();

  // FIXME: Arch specific.
  MCSection *S = Ctx.GetSection(Section);
  if (S == 0)
    S = MCSection::Create(Section, Ctx);
  
  Out.SwitchSection(S);
  return false;
}

bool AsmParser::ParseDirectiveSectionSwitch(const char *Section,
                                            const char *Directives) {
  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in section switching directive");
  Lexer.Lex();
  
  std::string SectionStr = Section;
  if (Directives && Directives[0]) {
    SectionStr += ","; 
    SectionStr += Directives;
  }
  
  // FIXME: Arch specific.
  MCSection *S = Ctx.GetSection(Section);
  if (S == 0)
    S = MCSection::Create(Section, Ctx);
  
  Out.SwitchSection(S);
  return false;
}

/// ParseDirectiveAscii:
///   ::= ( .ascii | .asciz ) [ "string" ( , "string" )* ]
bool AsmParser::ParseDirectiveAscii(bool ZeroTerminated) {
  if (Lexer.isNot(AsmToken::EndOfStatement)) {
    for (;;) {
      if (Lexer.isNot(AsmToken::String))
        return TokError("expected string in '.ascii' or '.asciz' directive");
      
      // FIXME: This shouldn't use a const char* + strlen, the string could have
      // embedded nulls.
      // FIXME: Should have accessor for getting string contents.
      StringRef Str = Lexer.getTok().getString();
      Out.EmitBytes(Str.substr(1, Str.size() - 2));
      if (ZeroTerminated)
        Out.EmitBytes(StringRef("\0", 1));
      
      Lexer.Lex();
      
      if (Lexer.is(AsmToken::EndOfStatement))
        break;

      if (Lexer.isNot(AsmToken::Comma))
        return TokError("unexpected token in '.ascii' or '.asciz' directive");
      Lexer.Lex();
    }
  }

  Lexer.Lex();
  return false;
}

/// ParseDirectiveValue
///  ::= (.byte | .short | ... ) [ expression (, expression)* ]
bool AsmParser::ParseDirectiveValue(unsigned Size) {
  if (Lexer.isNot(AsmToken::EndOfStatement)) {
    for (;;) {
      MCValue Expr;
      if (ParseRelocatableExpression(Expr))
        return true;

      Out.EmitValue(Expr, Size);

      if (Lexer.is(AsmToken::EndOfStatement))
        break;
      
      // FIXME: Improve diagnostic.
      if (Lexer.isNot(AsmToken::Comma))
        return TokError("unexpected token in directive");
      Lexer.Lex();
    }
  }

  Lexer.Lex();
  return false;
}

/// ParseDirectiveSpace
///  ::= .space expression [ , expression ]
bool AsmParser::ParseDirectiveSpace() {
  int64_t NumBytes;
  if (ParseAbsoluteExpression(NumBytes))
    return true;

  int64_t FillExpr = 0;
  bool HasFillExpr = false;
  if (Lexer.isNot(AsmToken::EndOfStatement)) {
    if (Lexer.isNot(AsmToken::Comma))
      return TokError("unexpected token in '.space' directive");
    Lexer.Lex();
    
    if (ParseAbsoluteExpression(FillExpr))
      return true;

    HasFillExpr = true;

    if (Lexer.isNot(AsmToken::EndOfStatement))
      return TokError("unexpected token in '.space' directive");
  }

  Lexer.Lex();

  if (NumBytes <= 0)
    return TokError("invalid number of bytes in '.space' directive");

  // FIXME: Sometimes the fill expr is 'nop' if it isn't supplied, instead of 0.
  for (uint64_t i = 0, e = NumBytes; i != e; ++i)
    Out.EmitValue(MCValue::get(FillExpr), 1);

  return false;
}

/// ParseDirectiveFill
///  ::= .fill expression , expression , expression
bool AsmParser::ParseDirectiveFill() {
  int64_t NumValues;
  if (ParseAbsoluteExpression(NumValues))
    return true;

  if (Lexer.isNot(AsmToken::Comma))
    return TokError("unexpected token in '.fill' directive");
  Lexer.Lex();
  
  int64_t FillSize;
  if (ParseAbsoluteExpression(FillSize))
    return true;

  if (Lexer.isNot(AsmToken::Comma))
    return TokError("unexpected token in '.fill' directive");
  Lexer.Lex();
  
  int64_t FillExpr;
  if (ParseAbsoluteExpression(FillExpr))
    return true;

  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.fill' directive");
  
  Lexer.Lex();

  if (FillSize != 1 && FillSize != 2 && FillSize != 4)
    return TokError("invalid '.fill' size, expected 1, 2, or 4");

  for (uint64_t i = 0, e = NumValues; i != e; ++i)
    Out.EmitValue(MCValue::get(FillExpr), FillSize);

  return false;
}

/// ParseDirectiveOrg
///  ::= .org expression [ , expression ]
bool AsmParser::ParseDirectiveOrg() {
  MCValue Offset;
  if (ParseRelocatableExpression(Offset))
    return true;

  // Parse optional fill expression.
  int64_t FillExpr = 0;
  if (Lexer.isNot(AsmToken::EndOfStatement)) {
    if (Lexer.isNot(AsmToken::Comma))
      return TokError("unexpected token in '.org' directive");
    Lexer.Lex();
    
    if (ParseAbsoluteExpression(FillExpr))
      return true;

    if (Lexer.isNot(AsmToken::EndOfStatement))
      return TokError("unexpected token in '.org' directive");
  }

  Lexer.Lex();

  // FIXME: Only limited forms of relocatable expressions are accepted here, it
  // has to be relative to the current section.
  Out.EmitValueToOffset(Offset, FillExpr);

  return false;
}

/// ParseDirectiveAlign
///  ::= {.align, ...} expression [ , expression [ , expression ]]
bool AsmParser::ParseDirectiveAlign(bool IsPow2, unsigned ValueSize) {
  int64_t Alignment;
  if (ParseAbsoluteExpression(Alignment))
    return true;

  SMLoc MaxBytesLoc;
  bool HasFillExpr = false;
  int64_t FillExpr = 0;
  int64_t MaxBytesToFill = 0;
  if (Lexer.isNot(AsmToken::EndOfStatement)) {
    if (Lexer.isNot(AsmToken::Comma))
      return TokError("unexpected token in directive");
    Lexer.Lex();

    // The fill expression can be omitted while specifying a maximum number of
    // alignment bytes, e.g:
    //  .align 3,,4
    if (Lexer.isNot(AsmToken::Comma)) {
      HasFillExpr = true;
      if (ParseAbsoluteExpression(FillExpr))
        return true;
    }

    if (Lexer.isNot(AsmToken::EndOfStatement)) {
      if (Lexer.isNot(AsmToken::Comma))
        return TokError("unexpected token in directive");
      Lexer.Lex();

      MaxBytesLoc = Lexer.getLoc();
      if (ParseAbsoluteExpression(MaxBytesToFill))
        return true;
      
      if (Lexer.isNot(AsmToken::EndOfStatement))
        return TokError("unexpected token in directive");
    }
  }

  Lexer.Lex();

  if (!HasFillExpr) {
    // FIXME: Sometimes fill with nop.
    FillExpr = 0;
  }

  // Compute alignment in bytes.
  if (IsPow2) {
    // FIXME: Diagnose overflow.
    Alignment = 1LL << Alignment;
  }

  // Diagnose non-sensical max bytes to fill.
  if (MaxBytesLoc.isValid()) {
    if (MaxBytesToFill < 1) {
      Warning(MaxBytesLoc, "alignment directive can never be satisfied in this "
              "many bytes, ignoring");
      return false;
    }

    if (MaxBytesToFill >= Alignment) {
      Warning(MaxBytesLoc, "maximum bytes expression exceeds alignment and "
              "has no effect");
      MaxBytesToFill = 0;
    }
  }

  // FIXME: Target specific behavior about how the "extra" bytes are filled.
  Out.EmitValueToAlignment(Alignment, FillExpr, ValueSize, MaxBytesToFill);

  return false;
}

/// ParseDirectiveSymbolAttribute
///  ::= { ".globl", ".weak", ... } [ identifier ( , identifier )* ]
bool AsmParser::ParseDirectiveSymbolAttribute(MCStreamer::SymbolAttr Attr) {
  if (Lexer.isNot(AsmToken::EndOfStatement)) {
    for (;;) {
      if (Lexer.isNot(AsmToken::Identifier))
        return TokError("expected identifier in directive");
      
      MCSymbol *Sym = Ctx.GetOrCreateSymbol(Lexer.getTok().getString());
      Lexer.Lex();

      // If this is use of an undefined symbol then mark it external.
      if (!Sym->getSection() && !Ctx.GetSymbolValue(Sym))
        Sym->setExternal(true);

      Out.EmitSymbolAttribute(Sym, Attr);

      if (Lexer.is(AsmToken::EndOfStatement))
        break;

      if (Lexer.isNot(AsmToken::Comma))
        return TokError("unexpected token in directive");
      Lexer.Lex();
    }
  }

  Lexer.Lex();
  return false;  
}

/// ParseDirectiveDarwinSymbolDesc
///  ::= .desc identifier , expression
bool AsmParser::ParseDirectiveDarwinSymbolDesc() {
  if (Lexer.isNot(AsmToken::Identifier))
    return TokError("expected identifier in directive");
  
  // handle the identifier as the key symbol.
  SMLoc IDLoc = Lexer.getLoc();
  MCSymbol *Sym = Ctx.GetOrCreateSymbol(Lexer.getTok().getString());
  Lexer.Lex();

  if (Lexer.isNot(AsmToken::Comma))
    return TokError("unexpected token in '.desc' directive");
  Lexer.Lex();

  SMLoc DescLoc = Lexer.getLoc();
  int64_t DescValue;
  if (ParseAbsoluteExpression(DescValue))
    return true;

  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.desc' directive");
  
  Lexer.Lex();

  // Set the n_desc field of this Symbol to this DescValue
  Out.EmitSymbolDesc(Sym, DescValue);

  return false;
}

/// ParseDirectiveComm
///  ::= ( .comm | .lcomm ) identifier , size_expression [ , align_expression ]
bool AsmParser::ParseDirectiveComm(bool IsLocal) {
  if (Lexer.isNot(AsmToken::Identifier))
    return TokError("expected identifier in directive");
  
  // handle the identifier as the key symbol.
  SMLoc IDLoc = Lexer.getLoc();
  MCSymbol *Sym = Ctx.GetOrCreateSymbol(Lexer.getTok().getString());
  Lexer.Lex();

  if (Lexer.isNot(AsmToken::Comma))
    return TokError("unexpected token in directive");
  Lexer.Lex();

  int64_t Size;
  SMLoc SizeLoc = Lexer.getLoc();
  if (ParseAbsoluteExpression(Size))
    return true;

  int64_t Pow2Alignment = 0;
  SMLoc Pow2AlignmentLoc;
  if (Lexer.is(AsmToken::Comma)) {
    Lexer.Lex();
    Pow2AlignmentLoc = Lexer.getLoc();
    if (ParseAbsoluteExpression(Pow2Alignment))
      return true;
  }
  
  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.comm' or '.lcomm' directive");
  
  Lexer.Lex();

  // NOTE: a size of zero for a .comm should create a undefined symbol
  // but a size of .lcomm creates a bss symbol of size zero.
  if (Size < 0)
    return Error(SizeLoc, "invalid '.comm' or '.lcomm' directive size, can't "
                 "be less than zero");

  // NOTE: The alignment in the directive is a power of 2 value, the assember
  // may internally end up wanting an alignment in bytes.
  // FIXME: Diagnose overflow.
  if (Pow2Alignment < 0)
    return Error(Pow2AlignmentLoc, "invalid '.comm' or '.lcomm' directive "
                 "alignment, can't be less than zero");

  // TODO: Symbol must be undefined or it is a error to re-defined the symbol
  if (Sym->getSection() || Ctx.GetSymbolValue(Sym))
    return Error(IDLoc, "invalid symbol redefinition");

  // Create the Symbol as a common or local common with Size and Pow2Alignment
  Out.EmitCommonSymbol(Sym, Size, Pow2Alignment, IsLocal);

  return false;
}

/// ParseDirectiveDarwinZerofill
///  ::= .zerofill segname , sectname [, identifier , size_expression [
///      , align_expression ]]
bool AsmParser::ParseDirectiveDarwinZerofill() {
  if (Lexer.isNot(AsmToken::Identifier))
    return TokError("expected segment name after '.zerofill' directive");
  std::string Section = Lexer.getTok().getString();
  Lexer.Lex();

  if (Lexer.isNot(AsmToken::Comma))
    return TokError("unexpected token in directive");
  Section += ',';
  Lexer.Lex();
 
  if (Lexer.isNot(AsmToken::Identifier))
    return TokError("expected section name after comma in '.zerofill' "
                    "directive");
  Section += Lexer.getTok().getString().str();
  Lexer.Lex();

  // FIXME: we will need to tell GetSection() that this is to be created with or
  // must have the Mach-O section type of S_ZEROFILL.  Something like the code
  // below could be done but for now it is not as EmitZerofill() does not know
  // how to deal with a section type in the section name like
  // ParseDirectiveDarwinSection() allows.
  // Section += ',';
  // Section += "zerofill";

  // If this is the end of the line all that was wanted was to create the
  // the section but with no symbol.
  if (Lexer.is(AsmToken::EndOfStatement)) {
    // Create the zerofill section but no symbol
    Out.EmitZerofill(Ctx.GetSection(Section.c_str()));
    return false;
  }

  if (Lexer.isNot(AsmToken::Comma))
    return TokError("unexpected token in directive");
  Lexer.Lex();

  if (Lexer.isNot(AsmToken::Identifier))
    return TokError("expected identifier in directive");
  
  // handle the identifier as the key symbol.
  SMLoc IDLoc = Lexer.getLoc();
  MCSymbol *Sym = Ctx.GetOrCreateSymbol(Lexer.getTok().getString());
  Lexer.Lex();

  if (Lexer.isNot(AsmToken::Comma))
    return TokError("unexpected token in directive");
  Lexer.Lex();

  int64_t Size;
  SMLoc SizeLoc = Lexer.getLoc();
  if (ParseAbsoluteExpression(Size))
    return true;

  int64_t Pow2Alignment = 0;
  SMLoc Pow2AlignmentLoc;
  if (Lexer.is(AsmToken::Comma)) {
    Lexer.Lex();
    Pow2AlignmentLoc = Lexer.getLoc();
    if (ParseAbsoluteExpression(Pow2Alignment))
      return true;
  }
  
  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.zerofill' directive");
  
  Lexer.Lex();

  if (Size < 0)
    return Error(SizeLoc, "invalid '.zerofill' directive size, can't be less "
                 "than zero");

  // NOTE: The alignment in the directive is a power of 2 value, the assember
  // may internally end up wanting an alignment in bytes.
  // FIXME: Diagnose overflow.
  if (Pow2Alignment < 0)
    return Error(Pow2AlignmentLoc, "invalid '.zerofill' directive alignment, "
                 "can't be less than zero");

  // TODO: Symbol must be undefined or it is a error to re-defined the symbol
  if (Sym->getSection() || Ctx.GetSymbolValue(Sym))
    return Error(IDLoc, "invalid symbol redefinition");

  // Create the zerofill Symbol with Size and Pow2Alignment
  Out.EmitZerofill(Ctx.GetSection(Section.c_str()), Sym, Size, Pow2Alignment);

  return false;
}

/// ParseDirectiveDarwinSubsectionsViaSymbols
///  ::= .subsections_via_symbols
bool AsmParser::ParseDirectiveDarwinSubsectionsViaSymbols() {
  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.subsections_via_symbols' directive");
  
  Lexer.Lex();

  Out.EmitAssemblerFlag(MCStreamer::SubsectionsViaSymbols);

  return false;
}

/// ParseDirectiveAbort
///  ::= .abort [ "abort_string" ]
bool AsmParser::ParseDirectiveAbort() {
  // FIXME: Use loc from directive.
  SMLoc Loc = Lexer.getLoc();

  StringRef Str = "";
  if (Lexer.isNot(AsmToken::EndOfStatement)) {
    if (Lexer.isNot(AsmToken::String))
      return TokError("expected string in '.abort' directive");
    
    Str = Lexer.getTok().getString();

    Lexer.Lex();
  }

  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.abort' directive");
  
  Lexer.Lex();

  // FIXME: Handle here.
  if (Str.empty())
    Error(Loc, ".abort detected. Assembly stopping.");
  else
    Error(Loc, ".abort '" + Str + "' detected. Assembly stopping.");

  return false;
}

/// ParseDirectiveLsym
///  ::= .lsym identifier , expression
bool AsmParser::ParseDirectiveDarwinLsym() {
  if (Lexer.isNot(AsmToken::Identifier))
    return TokError("expected identifier in directive");
  
  // handle the identifier as the key symbol.
  SMLoc IDLoc = Lexer.getLoc();
  MCSymbol *Sym = Ctx.GetOrCreateSymbol(Lexer.getTok().getString());
  Lexer.Lex();

  if (Lexer.isNot(AsmToken::Comma))
    return TokError("unexpected token in '.lsym' directive");
  Lexer.Lex();

  MCValue Expr;
  if (ParseRelocatableExpression(Expr))
    return true;

  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.lsym' directive");
  
  Lexer.Lex();

  // Create the Sym with the value of the Expr
  Out.EmitLocalSymbol(Sym, Expr);

  return false;
}

/// ParseDirectiveInclude
///  ::= .include "filename"
bool AsmParser::ParseDirectiveInclude() {
  if (Lexer.isNot(AsmToken::String))
    return TokError("expected string in '.include' directive");
  
  std::string Filename = Lexer.getTok().getString();
  SMLoc IncludeLoc = Lexer.getLoc();
  Lexer.Lex();

  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.include' directive");
  
  // Strip the quotes.
  Filename = Filename.substr(1, Filename.size()-2);
  
  // Attempt to switch the lexer to the included file before consuming the end
  // of statement to avoid losing it when we switch.
  if (Lexer.EnterIncludeFile(Filename)) {
    Lexer.PrintMessage(IncludeLoc,
                       "Could not find include file '" + Filename + "'",
                       "error");
    return true;
  }

  return false;
}

/// ParseDirectiveDarwinDumpOrLoad
///  ::= ( .dump | .load ) "filename"
bool AsmParser::ParseDirectiveDarwinDumpOrLoad(SMLoc IDLoc, bool IsDump) {
  if (Lexer.isNot(AsmToken::String))
    return TokError("expected string in '.dump' or '.load' directive");
  
  Lexer.Lex();

  if (Lexer.isNot(AsmToken::EndOfStatement))
    return TokError("unexpected token in '.dump' or '.load' directive");
  
  Lexer.Lex();

  // FIXME: If/when .dump and .load are implemented they will be done in the
  // the assembly parser and not have any need for an MCStreamer API.
  if (IsDump)
    Warning(IDLoc, "ignoring directive .dump for now");
  else
    Warning(IDLoc, "ignoring directive .load for now");

  return false;
}

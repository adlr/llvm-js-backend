//===-- llvm/Assembly/Parser.h - Parser for VM assembly files ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file was developed by the LLVM research group and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  These classes are implemented by the lib/AsmParser library.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ASSEMBLY_PARSER_H
#define LLVM_ASSEMBLY_PARSER_H

#include <string>

namespace llvm {

class Module;
class ParseException;


// The useful interface defined by this file... Parse an ascii file, and return
// the internal representation in a nice slice'n'dice'able representation.  Note
// that this does not verify that the generated LLVM is valid, so you should run
// the verifier after parsing the file to check that it's ok.
//
Module *ParseAssemblyFile(const std::string &Filename);// throw (ParseException)

//===------------------------------------------------------------------------===
//                              Helper Classes
//===------------------------------------------------------------------------===

// ParseException - For when an exceptional event is generated by the parser.
// This class lets you print out the exception message
//
class ParseException {
public:
  ParseException(const std::string &filename, const std::string &message,
                 int LineNo = -1, int ColNo = -1);

  ParseException(const ParseException &E);

  // getMessage - Return the message passed in at construction time plus extra
  // information extracted from the options used to parse with...
  //
  const std::string getMessage() const;

  inline const std::string &getRawMessage() const {   // Just the raw message...
    return Message;
  }

  inline const std::string &getFilename() const {
    return Filename;
  }

  // getErrorLocation - Return the line and column number of the error in the
  // input source file.  The source filename can be derived from the
  // ParserOptions in effect.  If positional information is not applicable,
  // these will return a value of -1.
  //
  inline const void getErrorLocation(int &Line, int &Column) const {
    Line = LineNo; Column = ColumnNo;
  }

private :
  std::string Filename;
  std::string Message;
  int LineNo, ColumnNo;                               // -1 if not relevant

  ParseException &operator=(const ParseException &E); // objects by reference
};

} // End llvm namespace

#endif

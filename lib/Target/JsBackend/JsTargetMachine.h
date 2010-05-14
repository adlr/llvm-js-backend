//== JsTargetMachine.h - TargetMachine for the Javscript backend -*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//==------------------------------------------------------------------------==//
//
// This file declares the TargetMachine that is used by the Javascript backend.
//
//===----------------------------------------------------------------------===//

#ifndef JSTARGETMACHINE_H
#define JSTARGETMACHINE_H

#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetData.h"

namespace llvm {

class formatted_raw_ostream;

struct JsTargetMachine : public TargetMachine {
  JsTargetMachine(const Target &T, const std::string &TT,
                   const std::string &FS)
    : TargetMachine(T) {}

  virtual bool addPassesToEmitFile(PassManagerBase &PM,
				   formatted_raw_ostream &Out,
				   CodeGenFileType FileType,
				   CodeGenOpt::Level OptLevel,
				   bool DisableVerify);

  virtual const TargetData *getTargetData() const { return 0; }
};

extern Target TheJsBackendTarget;

} // End llvm namespace


#endif

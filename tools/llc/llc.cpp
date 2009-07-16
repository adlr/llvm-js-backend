//===-- llc.cpp - Implement the LLVM Native Code Generator ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is the llc code generator driver. It provides a convenient
// command-line interface for generating native assembly-language code
// or C code, given LLVM bitcode.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/CodeGen/FileWriters.h"
#include "llvm/CodeGen/LinkAllCodegenComponents.h"
#include "llvm/CodeGen/LinkAllAsmWriterComponents.h"
#include "llvm/CodeGen/ObjectCodeEmitter.h"
#include "llvm/Target/SubtargetFeature.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetRegistry.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/ModuleProvider.h"
#include "llvm/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/System/Signals.h"
#include "llvm/Config/config.h"
#include "llvm/LinkAllVMCore.h"
#include "llvm/Target/TargetSelect.h"
#include <memory>
using namespace llvm;

// General options for llc.  Other pass-specific options are specified
// within the corresponding llc passes, and target-specific options
// and back-end code generation options are specified with the target machine.
//
static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));

static cl::opt<std::string>
OutputFilename("o", cl::desc("Output filename"), cl::value_desc("filename"));

static cl::opt<bool> Force("f", cl::desc("Overwrite output files"));

// Determine optimization level.
static cl::opt<char>
OptLevel("O",
         cl::desc("Optimization level. [-O0, -O1, -O2, or -O3] "
                  "(default = '-O2')"),
         cl::Prefix,
         cl::ZeroOrMore,
         cl::init(' '));

static cl::opt<std::string>
TargetTriple("mtriple", cl::desc("Override target triple for module"));

static cl::opt<std::string>
MArch("march", cl::desc("Architecture to generate code for (see --version)"));

static cl::opt<std::string>
MCPU("mcpu",
  cl::desc("Target a specific cpu type (-mcpu=help for details)"),
  cl::value_desc("cpu-name"),
  cl::init(""));

static cl::list<std::string>
MAttrs("mattr",
  cl::CommaSeparated,
  cl::desc("Target specific attributes (-mattr=help for details)"),
  cl::value_desc("a1,+a2,-a3,..."));

cl::opt<TargetMachine::CodeGenFileType>
FileType("filetype", cl::init(TargetMachine::AssemblyFile),
  cl::desc("Choose a file type (not all types are supported by all targets):"),
  cl::values(
       clEnumValN(TargetMachine::AssemblyFile, "asm",
                  "Emit an assembly ('.s') file"),
       clEnumValN(TargetMachine::ObjectFile, "obj",
                  "Emit a native object ('.o') file [experimental]"),
       clEnumValN(TargetMachine::DynamicLibrary, "dynlib",
                  "Emit a native dynamic library ('.so') file"
                  " [experimental]"),
       clEnumValEnd));

cl::opt<bool> NoVerify("disable-verify", cl::Hidden,
                       cl::desc("Do not verify input module"));


static cl::opt<bool>
DisableRedZone("disable-red-zone",
  cl::desc("Do not emit code that uses the red zone."),
  cl::init(false));

static cl::opt<bool>
NoImplicitFloats("no-implicit-float",
  cl::desc("Don't generate implicit floating point instructions (x86-only)"),
  cl::init(false));

// GetFileNameRoot - Helper function to get the basename of a filename.
static inline std::string
GetFileNameRoot(const std::string &InputFilename) {
  std::string IFN = InputFilename;
  std::string outputFilename;
  int Len = IFN.length();
  if ((Len > 2) &&
      IFN[Len-3] == '.' && IFN[Len-2] == 'b' && IFN[Len-1] == 'c') {
    outputFilename = std::string(IFN.begin(), IFN.end()-3); // s/.bc/.s/
  } else {
    outputFilename = IFN;
  }
  return outputFilename;
}

static formatted_raw_ostream *GetOutputStream(const char *TargetName, 
                                              const char *ProgName) {
  if (OutputFilename != "") {
    if (OutputFilename == "-")
      return &fouts();

    // Make sure that the Out file gets unlinked from the disk if we get a
    // SIGINT
    sys::RemoveFileOnSignal(sys::Path(OutputFilename));

    std::string error;
    raw_fd_ostream *FDOut = new raw_fd_ostream(OutputFilename.c_str(),
                                               /*Binary=*/true, Force, error);
    if (!error.empty()) {
      errs() << error << '\n';
      if (!Force)
        errs() << "Use -f command line argument to force output\n";
      delete FDOut;
      return 0;
    }
    formatted_raw_ostream *Out =
      new formatted_raw_ostream(*FDOut, formatted_raw_ostream::DELETE_STREAM);

    return Out;
  }

  if (InputFilename == "-") {
    OutputFilename = "-";
    return &fouts();
  }

  OutputFilename = GetFileNameRoot(InputFilename);

  bool Binary = false;
  switch (FileType) {
  case TargetMachine::AssemblyFile:
    if (TargetName[0] == 'c') {
      if (TargetName[1] == 0)
        OutputFilename += ".cbe.c";
      else if (TargetName[1] == 'p' && TargetName[2] == 'p')
        OutputFilename += ".cpp";
      else
        OutputFilename += ".s";
    } else
      OutputFilename += ".s";
    break;
  case TargetMachine::ObjectFile:
    OutputFilename += ".o";
    Binary = true;
    break;
  case TargetMachine::DynamicLibrary:
    OutputFilename += LTDL_SHLIB_EXT;
    Binary = true;
    break;
  }

  // Make sure that the Out file gets unlinked from the disk if we get a
  // SIGINT
  sys::RemoveFileOnSignal(sys::Path(OutputFilename));

  std::string error;
  raw_fd_ostream *FDOut = new raw_fd_ostream(OutputFilename.c_str(),
                                             Binary, Force, error);
  if (!error.empty()) {
    errs() << error << '\n';
    if (!Force)
      errs() << "Use -f command line argument to force output\n";
    delete FDOut;
    return 0;
  }

  formatted_raw_ostream *Out =
    new formatted_raw_ostream(*FDOut, formatted_raw_ostream::DELETE_STREAM);

  return Out;
}

// main - Entry point for the llc compiler.
//
int main(int argc, char **argv) {
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);
  LLVMContext &Context = getGlobalContext();
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.

  // Initialize targets first.
  InitializeAllTargets();
  InitializeAllAsmPrinters();

  cl::ParseCommandLineOptions(argc, argv, "llvm system compiler\n");
  
  // Load the module to be compiled...
  std::string ErrorMessage;
  std::auto_ptr<Module> M;

  std::auto_ptr<MemoryBuffer> Buffer(
                   MemoryBuffer::getFileOrSTDIN(InputFilename, &ErrorMessage));
  if (Buffer.get())
    M.reset(ParseBitcodeFile(Buffer.get(), Context, &ErrorMessage));
  if (M.get() == 0) {
    errs() << argv[0] << ": bitcode didn't read correctly.\n";
    errs() << "Reason: " << ErrorMessage << "\n";
    return 1;
  }
  Module &mod = *M.get();

  // If we are supposed to override the target triple, do so now.
  if (!TargetTriple.empty())
    mod.setTargetTriple(TargetTriple);

  // Allocate target machine.  First, check whether the user has
  // explicitly specified an architecture to compile for.
  const Target *TheTarget = 0;
  if (!MArch.empty()) {
    for (TargetRegistry::iterator it = TargetRegistry::begin(),
           ie = TargetRegistry::end(); it != ie; ++it) {
      if (MArch == it->getName()) {
        TheTarget = &*it;
        break;
      }
    }

    if (!TheTarget) {
      errs() << argv[0] << ": error: invalid target '" << MArch << "'.\n";
      return 1;
    }        
  } else {
    std::string Err;
    TheTarget = TargetRegistry::getClosestStaticTargetForModule(mod, Err);
    if (TheTarget == 0) {
      errs() << argv[0] << ": error auto-selecting target for module '"
             << Err << "'.  Please use the -march option to explicitly "
             << "pick a target.\n";
      return 1;
    }
  }

  // Package up features to be passed to target/subtarget
  std::string FeaturesStr;
  if (MCPU.size() || MAttrs.size()) {
    SubtargetFeatures Features;
    Features.setCPU(MCPU);
    for (unsigned i = 0; i != MAttrs.size(); ++i)
      Features.AddFeature(MAttrs[i]);
    FeaturesStr = Features.getString();
  }

  std::auto_ptr<TargetMachine> 
    target(TheTarget->createTargetMachine(mod, FeaturesStr));
  assert(target.get() && "Could not allocate target machine!");
  TargetMachine &Target = *target.get();

  // Figure out where we are going to send the output...
  formatted_raw_ostream *Out = GetOutputStream(TheTarget->getName(), argv[0]);
  if (Out == 0) return 1;

  CodeGenOpt::Level OLvl = CodeGenOpt::Default;
  switch (OptLevel) {
  default:
    errs() << argv[0] << ": invalid optimization level.\n";
    return 1;
  case ' ': break;
  case '0': OLvl = CodeGenOpt::None; break;
  case '1':
  case '2': OLvl = CodeGenOpt::Default; break;
  case '3': OLvl = CodeGenOpt::Aggressive; break;
  }

  // If this target requires addPassesToEmitWholeFile, do it now.  This is
  // used by strange things like the C backend.
  if (Target.WantsWholeFile()) {
    PassManager PM;
    PM.add(new TargetData(*Target.getTargetData()));
    if (!NoVerify)
      PM.add(createVerifierPass());

    // Ask the target to add backend passes as necessary.
    if (Target.addPassesToEmitWholeFile(PM, *Out, FileType, OLvl)) {
      errs() << argv[0] << ": target does not support generation of this"
             << " file type!\n";
      if (Out != &fouts()) delete Out;
      // And the Out file is empty and useless, so remove it now.
      sys::Path(OutputFilename).eraseFromDisk();
      return 1;
    }
    PM.run(mod);
  } else {
    // Build up all of the passes that we want to do to the module.
    ExistingModuleProvider Provider(M.release());
    FunctionPassManager Passes(&Provider);
    Passes.add(new TargetData(*Target.getTargetData()));

#ifndef NDEBUG
    if (!NoVerify)
      Passes.add(createVerifierPass());
#endif

    // Ask the target to add backend passes as necessary.
    ObjectCodeEmitter *OCE = 0;

    // Override default to generate verbose assembly.
    Target.setAsmVerbosityDefault(true);

    switch (Target.addPassesToEmitFile(Passes, *Out, FileType, OLvl)) {
    default:
      assert(0 && "Invalid file model!");
      return 1;
    case FileModel::Error:
      errs() << argv[0] << ": target does not support generation of this"
             << " file type!\n";
      if (Out != &fouts()) delete Out;
      // And the Out file is empty and useless, so remove it now.
      sys::Path(OutputFilename).eraseFromDisk();
      return 1;
    case FileModel::AsmFile:
      break;
    case FileModel::MachOFile:
      OCE = AddMachOWriter(Passes, *Out, Target);
      break;
    case FileModel::ElfFile:
      OCE = AddELFWriter(Passes, *Out, Target);
      break;
    }

    if (Target.addPassesToEmitFileFinish(Passes, OCE, OLvl)) {
      errs() << argv[0] << ": target does not support generation of this"
             << " file type!\n";
      if (Out != &fouts()) delete Out;
      // And the Out file is empty and useless, so remove it now.
      sys::Path(OutputFilename).eraseFromDisk();
      return 1;
    }

    Passes.doInitialization();

    // Run our queue of passes all at once now, efficiently.
    // TODO: this could lazily stream functions out of the module.
    for (Module::iterator I = mod.begin(), E = mod.end(); I != E; ++I)
      if (!I->isDeclaration()) {
        if (DisableRedZone)
          I->addFnAttr(Attribute::NoRedZone);
        if (NoImplicitFloats)
          I->addFnAttr(Attribute::NoImplicitFloat);
        Passes.run(*I);
      }

    Passes.doFinalization();
  }

  Out->flush();

  // Delete the ostream if it's not a stdout stream
  if (Out != &fouts()) delete Out;

  return 0;
}

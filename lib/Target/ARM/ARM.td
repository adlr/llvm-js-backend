//===- ARM.td - Describe the ARM Target Machine -----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Target-independent interfaces which we are implementing
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// ARM Subtarget features.
//

def ArchV4T     : SubtargetFeature<"v4t", "ARMArchVersion", "V4T",
                                   "ARM v4T">;
def ArchV5T     : SubtargetFeature<"v5t", "ARMArchVersion", "V5T",
                                   "ARM v5T">;
def ArchV5TE    : SubtargetFeature<"v5te", "ARMArchVersion", "V5TE",
                                   "ARM v5TE, v5TEj, v5TExp">;
def ArchV6      : SubtargetFeature<"v6", "ARMArchVersion", "V6",
                                   "ARM v6">;
def ArchV6T2    : SubtargetFeature<"v6t2", "ARMArchVersion", "V6T2",
                                   "ARM v6t2">;
def ArchV7A     : SubtargetFeature<"v7a", "ARMArchVersion", "V7A",
                                   "ARM v7A">;
def ArchV7M     : SubtargetFeature<"v7m", "ARMArchVersion", "V7M",
                                   "ARM v7M">;
def FeatureVFP2 : SubtargetFeature<"vfp2", "ARMFPUType", "VFPv2",
                                   "Enable VFP2 instructions">;
def FeatureVFP3 : SubtargetFeature<"vfp3", "ARMFPUType", "VFPv3",
                                   "Enable VFP3 instructions">;
def FeatureNEON : SubtargetFeature<"neon", "ARMFPUType", "NEON",
                                   "Enable NEON instructions">;
def FeatureThumb2 : SubtargetFeature<"thumb2", "ThumbMode", "Thumb2",
                                     "Enable Thumb2 instructions">;
def FeatureFP16   : SubtargetFeature<"fp16", "HasFP16", "true",
                                     "Enable half-precision floating point">;
def FeatureHWDiv  : SubtargetFeature<"hwdiv", "HasHardwareDivide", "true",
                                     "Enable divide instructions">;
def FeatureT2ExtractPack: SubtargetFeature<"t2xtpk", "HasT2ExtractPack", "true",
                                 "Enable Thumb2 extract and pack instructions">;

// Some processors have multiply-accumulate instructions that don't
// play nicely with other VFP instructions, and it's generally better
// to just not use them.
// FIXME: Currently, this is only flagged for Cortex-A8. It may be true for
// others as well. We should do more benchmarking and confirm one way or
// the other.
def FeatureHasSlowVMLx   : SubtargetFeature<"vmlx", "SlowVMLx", "true",
                                            "Disable VFP MAC instructions">;
// Some processors benefit from using NEON instructions for scalar
// single-precision FP operations.
def FeatureNEONForFP : SubtargetFeature<"neonfp", "UseNEONForSinglePrecisionFP",
                                        "true",
                                        "Use NEON for single precision FP">;


//===----------------------------------------------------------------------===//
// ARM Processors supported.
//

include "ARMSchedule.td"

class ProcNoItin<string Name, list<SubtargetFeature> Features>
 : Processor<Name, GenericItineraries, Features>;

// V4 Processors.
def : ProcNoItin<"generic",         []>;
def : ProcNoItin<"arm8",            []>;
def : ProcNoItin<"arm810",          []>;
def : ProcNoItin<"strongarm",       []>;
def : ProcNoItin<"strongarm110",    []>;
def : ProcNoItin<"strongarm1100",   []>;
def : ProcNoItin<"strongarm1110",   []>;

// V4T Processors.
def : ProcNoItin<"arm7tdmi",        [ArchV4T]>;
def : ProcNoItin<"arm7tdmi-s",      [ArchV4T]>;
def : ProcNoItin<"arm710t",         [ArchV4T]>;
def : ProcNoItin<"arm720t",         [ArchV4T]>;
def : ProcNoItin<"arm9",            [ArchV4T]>;
def : ProcNoItin<"arm9tdmi",        [ArchV4T]>;
def : ProcNoItin<"arm920",          [ArchV4T]>;
def : ProcNoItin<"arm920t",         [ArchV4T]>;
def : ProcNoItin<"arm922t",         [ArchV4T]>;
def : ProcNoItin<"arm940t",         [ArchV4T]>;
def : ProcNoItin<"ep9312",          [ArchV4T]>;

// V5T Processors.
def : ProcNoItin<"arm10tdmi",       [ArchV5T]>;
def : ProcNoItin<"arm1020t",        [ArchV5T]>;

// V5TE Processors.
def : ProcNoItin<"arm9e",           [ArchV5TE]>;
def : ProcNoItin<"arm926ej-s",      [ArchV5TE]>;
def : ProcNoItin<"arm946e-s",       [ArchV5TE]>;
def : ProcNoItin<"arm966e-s",       [ArchV5TE]>;
def : ProcNoItin<"arm968e-s",       [ArchV5TE]>;
def : ProcNoItin<"arm10e",          [ArchV5TE]>;
def : ProcNoItin<"arm1020e",        [ArchV5TE]>;
def : ProcNoItin<"arm1022e",        [ArchV5TE]>;
def : ProcNoItin<"xscale",          [ArchV5TE]>;
def : ProcNoItin<"iwmmxt",          [ArchV5TE]>;

// V6 Processors.
def : Processor<"arm1136j-s",       ARMV6Itineraries, [ArchV6]>;
def : Processor<"arm1136jf-s",      ARMV6Itineraries, [ArchV6, FeatureVFP2,
                                                       FeatureHasSlowVMLx]>;
def : Processor<"arm1176jz-s",      ARMV6Itineraries, [ArchV6]>;
def : Processor<"arm1176jzf-s",     ARMV6Itineraries, [ArchV6, FeatureVFP2]>;
def : Processor<"mpcorenovfp",      ARMV6Itineraries, [ArchV6]>;
def : Processor<"mpcore",           ARMV6Itineraries, [ArchV6, FeatureVFP2]>;

// V6T2 Processors.
def : Processor<"arm1156t2-s",     ARMV6Itineraries,
                 [ArchV6T2, FeatureThumb2]>;
def : Processor<"arm1156t2f-s",    ARMV6Itineraries,
                 [ArchV6T2, FeatureThumb2, FeatureVFP2]>;

// V7 Processors.
def : Processor<"cortex-a8",        CortexA8Itineraries,
                [ArchV7A, FeatureThumb2, FeatureNEON, FeatureHasSlowVMLx,
                 FeatureNEONForFP, FeatureT2ExtractPack]>;
def : Processor<"cortex-a9",        CortexA9Itineraries,
                [ArchV7A, FeatureThumb2, FeatureNEON, FeatureT2ExtractPack]>;
def : ProcNoItin<"cortex-m3",       [ArchV7M, FeatureThumb2, FeatureHWDiv]>;
def : ProcNoItin<"cortex-m4",       [ArchV7M, FeatureThumb2, FeatureHWDiv]>;

//===----------------------------------------------------------------------===//
// Register File Description
//===----------------------------------------------------------------------===//

include "ARMRegisterInfo.td"

include "ARMCallingConv.td"

//===----------------------------------------------------------------------===//
// Instruction Descriptions
//===----------------------------------------------------------------------===//

include "ARMInstrInfo.td"

def ARMInstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// Declare the target which we are implementing
//===----------------------------------------------------------------------===//

def ARM : Target {
  // Pull in Instruction Info:
  let InstructionSet = ARMInstrInfo;
}

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_PPC

#include "src/codegen.h"
#include "src/ic/ic.h"
#include "src/ic/ic-compiler.h"
#include "src/ic/stub-cache.h"

namespace v8 {
namespace internal {


// ----------------------------------------------------------------------------
// Static IC stub generators.
//

#define __ ACCESS_MASM(masm)

// Helper function used from LoadIC GenerateNormal.
//
// elements: Property dictionary. It is not clobbered if a jump to the miss
//           label is done.
// name:     Property name. It is not clobbered if a jump to the miss label is
//           done
// result:   Register for the result. It is only updated if a jump to the miss
//           label is not done. Can be the same as elements or name clobbering
//           one of these in the case of not jumping to the miss label.
// The two scratch registers need to be different from elements, name and
// result.
// The generated code assumes that the receiver has slow properties,
// is not a global object and does not have interceptors.
static void GenerateDictionaryLoad(MacroAssembler* masm, Label* miss,
                                   Register elements, Register name,
                                   Register result, Register scratch1,
                                   Register scratch2) {
  // Main use of the scratch registers.
  // scratch1: Used as temporary and to hold the capacity of the property
  //           dictionary.
  // scratch2: Used as temporary.
  Label done;

  // Probe the dictionary.
  NameDictionaryLookupStub::GeneratePositiveLookup(masm, miss, &done, elements,
                                                   name, scratch1, scratch2);

  // If probing finds an entry check that the value is a normal
  // property.
  __ bind(&done);  // scratch2 == elements + 4 * index
  const int kElementsStartOffset =
      NameDictionary::kHeaderSize +
      NameDictionary::kElementsStartIndex * kPointerSize;
  const int kDetailsOffset = kElementsStartOffset + 2 * kPointerSize;
  __ LoadP(scratch1, FieldMemOperand(scratch2, kDetailsOffset));
  __ mr(r0, scratch2);
  __ LoadSmiLiteral(scratch2, Smi::FromInt(PropertyDetails::TypeField::kMask));
  __ and_(scratch2, scratch1, scratch2, SetRC);
  __ bne(miss, cr0);
  __ mr(scratch2, r0);

  // Get the value at the masked, scaled index and return.
  __ LoadP(result,
           FieldMemOperand(scratch2, kElementsStartOffset + 1 * kPointerSize));
}


// Helper function used from StoreIC::GenerateNormal.
//
// elements: Property dictionary. It is not clobbered if a jump to the miss
//           label is done.
// name:     Property name. It is not clobbered if a jump to the miss label is
//           done
// value:    The value to store.
// The two scratch registers need to be different from elements, name and
// result.
// The generated code assumes that the receiver has slow properties,
// is not a global object and does not have interceptors.
static void GenerateDictionaryStore(MacroAssembler* masm, Label* miss,
                                    Register elements, Register name,
                                    Register value, Register scratch1,
                                    Register scratch2) {
  // Main use of the scratch registers.
  // scratch1: Used as temporary and to hold the capacity of the property
  //           dictionary.
  // scratch2: Used as temporary.
  Label done;

  // Probe the dictionary.
  NameDictionaryLookupStub::GeneratePositiveLookup(masm, miss, &done, elements,
                                                   name, scratch1, scratch2);

  // If probing finds an entry in the dictionary check that the value
  // is a normal property that is not read only.
  __ bind(&done);  // scratch2 == elements + 4 * index
  const int kElementsStartOffset =
      NameDictionary::kHeaderSize +
      NameDictionary::kElementsStartIndex * kPointerSize;
  const int kDetailsOffset = kElementsStartOffset + 2 * kPointerSize;
  int kTypeAndReadOnlyMask =
      PropertyDetails::TypeField::kMask |
      PropertyDetails::AttributesField::encode(READ_ONLY);
  __ LoadP(scratch1, FieldMemOperand(scratch2, kDetailsOffset));
  __ mr(r0, scratch2);
  __ LoadSmiLiteral(scratch2, Smi::FromInt(kTypeAndReadOnlyMask));
  __ and_(scratch2, scratch1, scratch2, SetRC);
  __ bne(miss, cr0);
  __ mr(scratch2, r0);

  // Store the value at the masked, scaled index and return.
  const int kValueOffset = kElementsStartOffset + kPointerSize;
  __ addi(scratch2, scratch2, Operand(kValueOffset - kHeapObjectTag));
  __ StoreP(value, MemOperand(scratch2));

  // Update the write barrier. Make sure not to clobber the value.
  __ mr(scratch1, value);
  __ RecordWrite(elements, scratch2, scratch1, kLRHasNotBeenSaved,
                 kDontSaveFPRegs);
}

void LoadIC::GenerateNormal(MacroAssembler* masm) {
  Register dictionary = r3;
  DCHECK(!dictionary.is(LoadDescriptor::ReceiverRegister()));
  DCHECK(!dictionary.is(LoadDescriptor::NameRegister()));

  Label slow;

  __ LoadP(dictionary, FieldMemOperand(LoadDescriptor::ReceiverRegister(),
                                       JSObject::kPropertiesOffset));
  GenerateDictionaryLoad(masm, &slow, dictionary,
                         LoadDescriptor::NameRegister(), r3, r6, r7);
  __ Ret();

  // Dictionary load failed, go slow (but don't miss).
  __ bind(&slow);
  GenerateRuntimeGetProperty(masm);
}


// A register that isn't one of the parameters to the load ic.
static const Register LoadIC_TempRegister() { return r6; }

void LoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // The return address is in lr.

  __ mr(LoadIC_TempRegister(), LoadDescriptor::ReceiverRegister());
  __ Push(LoadIC_TempRegister(), LoadDescriptor::NameRegister());

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kGetProperty);
}

static void StoreIC_PushArgs(MacroAssembler* masm) {
  __ Push(StoreWithVectorDescriptor::ValueRegister(),
          StoreWithVectorDescriptor::SlotRegister(),
          StoreWithVectorDescriptor::VectorRegister(),
          StoreWithVectorDescriptor::ReceiverRegister(),
          StoreWithVectorDescriptor::NameRegister());
}


void KeyedStoreIC::GenerateMiss(MacroAssembler* masm) {
  StoreIC_PushArgs(masm);

  __ TailCallRuntime(Runtime::kKeyedStoreIC_Miss);
}

void KeyedStoreIC::GenerateSlow(MacroAssembler* masm) {
  StoreIC_PushArgs(masm);

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  __ TailCallRuntime(Runtime::kKeyedStoreIC_Slow);
}

void StoreIC::GenerateMiss(MacroAssembler* masm) {
  StoreIC_PushArgs(masm);

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kStoreIC_Miss);
}


void StoreIC::GenerateNormal(MacroAssembler* masm) {
  Label miss;
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register name = StoreDescriptor::NameRegister();
  Register value = StoreDescriptor::ValueRegister();
  Register dictionary = r8;
  DCHECK(receiver.is(r4));
  DCHECK(name.is(r5));
  DCHECK(value.is(r3));
  DCHECK(StoreWithVectorDescriptor::VectorRegister().is(r6));
  DCHECK(StoreWithVectorDescriptor::SlotRegister().is(r7));

  __ LoadP(dictionary, FieldMemOperand(receiver, JSObject::kPropertiesOffset));

  GenerateDictionaryStore(masm, &miss, dictionary, name, value, r9, r10);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->ic_store_normal_hit(), 1, r9, r10);
  __ Ret();

  __ bind(&miss);
  __ IncrementCounter(counters->ic_store_normal_miss(), 1, r9, r10);
  GenerateMiss(masm);
}


#undef __


Condition CompareIC::ComputeCondition(Token::Value op) {
  switch (op) {
    case Token::EQ_STRICT:
    case Token::EQ:
      return eq;
    case Token::LT:
      return lt;
    case Token::GT:
      return gt;
    case Token::LTE:
      return le;
    case Token::GTE:
      return ge;
    default:
      UNREACHABLE();
      return kNoCondition;
  }
}


bool CompareIC::HasInlinedSmiCode(Address address) {
  // The address of the instruction following the call.
  Address cmp_instruction_address =
      Assembler::return_address_from_call_start(address);

  // If the instruction following the call is not a cmp rx, #yyy, nothing
  // was inlined.
  Instr instr = Assembler::instr_at(cmp_instruction_address);
  return Assembler::IsCmpImmediate(instr);
}


//
// This code is paired with the JumpPatchSite class in full-codegen-ppc.cc
//
void PatchInlinedSmiCode(Isolate* isolate, Address address,
                         InlinedSmiCheck check) {
  Address cmp_instruction_address =
      Assembler::return_address_from_call_start(address);

  // If the instruction following the call is not a cmp rx, #yyy, nothing
  // was inlined.
  Instr instr = Assembler::instr_at(cmp_instruction_address);
  if (!Assembler::IsCmpImmediate(instr)) {
    return;
  }

  // The delta to the start of the map check instruction and the
  // condition code uses at the patched jump.
  int delta = Assembler::GetCmpImmediateRawImmediate(instr);
  delta += Assembler::GetCmpImmediateRegister(instr).code() * kOff16Mask;
  // If the delta is 0 the instruction is cmp r0, #0 which also signals that
  // nothing was inlined.
  if (delta == 0) {
    return;
  }

  if (FLAG_trace_ic) {
    PrintF("[  patching ic at %p, cmp=%p, delta=%d\n",
           static_cast<void*>(address),
           static_cast<void*>(cmp_instruction_address), delta);
  }

  Address patch_address =
      cmp_instruction_address - delta * Instruction::kInstrSize;
  Instr instr_at_patch = Assembler::instr_at(patch_address);
  Instr branch_instr =
      Assembler::instr_at(patch_address + Instruction::kInstrSize);
  // This is patching a conditional "jump if not smi/jump if smi" site.
  // Enabling by changing from
  //   cmp cr0, rx, rx
  // to
  //  rlwinm(r0, value, 0, 31, 31, SetRC);
  //  bc(label, BT/BF, 2)
  // and vice-versa to be disabled again.
  CodePatcher patcher(isolate, patch_address, 2);
  Register reg = Assembler::GetRA(instr_at_patch);
  if (check == ENABLE_INLINED_SMI_CHECK) {
    DCHECK(Assembler::IsCmpRegister(instr_at_patch));
    DCHECK_EQ(Assembler::GetRA(instr_at_patch).code(),
              Assembler::GetRB(instr_at_patch).code());
    patcher.masm()->TestIfSmi(reg, r0);
  } else {
    DCHECK(check == DISABLE_INLINED_SMI_CHECK);
    DCHECK(Assembler::IsAndi(instr_at_patch));
    patcher.masm()->cmp(reg, reg, cr0);
  }
  DCHECK(Assembler::IsBranch(branch_instr));

  // Invert the logic of the branch
  if (Assembler::GetCondition(branch_instr) == eq) {
    patcher.EmitCondition(ne);
  } else {
    DCHECK(Assembler::GetCondition(branch_instr) == ne);
    patcher.EmitCondition(eq);
  }
}
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/v8.h"

#include "src/debug/debug.h"
#include "src/disasm.h"
#include "src/disassembler.h"
#include "src/ic/ic.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/ostreams.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/c-signature.h"
#include "test/cctest/compiler/call-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ assm.

static int32_t DummyStaticFunction(Object* result) { return 1; }

TEST(WasmRelocationX64ContextReference) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  v8::internal::byte buffer[4096];
  Assembler assm(isolate, buffer, sizeof buffer);
  DummyStaticFunction(nullptr);
  int64_t imm = 1234567;

  __ movq(rax, imm, RelocInfo::WASM_CONTEXT_REFERENCE);
  __ nop();
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
  USE(code);

  CSignatureOf<int64_t> csig;
  CodeRunner<int64_t> runnable(isolate, code, &csig);
  int64_t ret_value = runnable.Call();
  CHECK_EQ(ret_value, imm);

#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
  byte* begin = code->raw_instruction_start();
  byte* end = begin + code->raw_instruction_size();
  disasm::Disassembler::Disassemble(stdout, begin, end);
#endif
  int offset = 1234;

  // Relocating references by offset
  int mode_mask = (1 << RelocInfo::WASM_CONTEXT_REFERENCE);
  for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
    // TODO(6792): No longer needed once WebAssembly code is off heap.
    CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
    DCHECK(RelocInfo::IsWasmContextReference(it.rinfo()->rmode()));
    it.rinfo()->set_wasm_context_reference(
        it.rinfo()->wasm_context_reference() + offset, SKIP_ICACHE_FLUSH);
  }

  // Check if immediate is updated correctly
  ret_value = runnable.Call();
  CHECK_EQ(ret_value, imm + offset);

#ifdef OBJECT_PRINT
  code->Print(os);
  begin = code->raw_instruction_start();
  end = begin + code->raw_instruction_size();
  disasm::Disassembler::Disassemble(stdout, begin, end);
#endif
}

#undef __

}  // namespace compiler
}  // namespace internal
}  // namespace v8

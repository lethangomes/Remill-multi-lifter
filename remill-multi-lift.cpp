#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <glog/logging.h>
#include <gflags/gflags.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>


#include <remill/BC/Optimizer.h>
#include <remill/Arch/Arch.h>
#include <remill/Arch/Instruction.h>
#include <remill/Arch/Name.h>
#include <remill/BC/IntrinsicTable.h>
#include <remill/BC/Lifter.h>
#include <remill/BC/Util.h>
#include <remill/BC/Version.h>
#include <remill/OS/OS.h>
#include "extract-instructions.cpp"

using Memory = std::map<uint64_t, uint8_t>;

//function UnhexlifyInputBytes taken from the example script Lift.cpp from the remill library
static Memory UnhexlifyInputBytes(std::string bytes) {
  Memory memory;

  for (size_t i = 0; i < bytes.size(); i += 2) {
    char nibbles[] = {bytes[i], bytes[i + 1], '\0'};
    char *parsed_to = nullptr;
    auto byte_val = strtol(nibbles, &parsed_to, 16);

    if (parsed_to != &(nibbles[2])) {
      std::cerr << "Invalid hex byte value '" << nibbles
                << "' specified in --bytes." << std::endl;
    }

    auto byte_addr = (i / 2);


    memory[byte_addr] = static_cast<uint8_t>(byte_val);
  }

  return memory;
}

//class SimpleTraceManager taken from the example script Lift.cpp from the remill library
class SimpleTraceManager : public remill::TraceManager {
 public:
  virtual ~SimpleTraceManager(void) = default;

  explicit SimpleTraceManager(Memory &memory_) : memory(memory_) {}

 protected:
  // Called when we have lifted, i.e. defined the contents, of a new trace.
  // The derived class is expected to do something useful with this.
  void SetLiftedTraceDefinition(uint64_t addr,
                                llvm::Function *lifted_func) override {
    traces[addr] = lifted_func;
  }

  // Get a declaration for a lifted trace. The idea here is that a derived
  // class might have additional global info available to them that lets
  // them declare traces ahead of time. In order to distinguish between
  // stuff we've lifted, and stuff we haven't lifted, we allow the lifter
  // to access "defined" vs. "declared" traces.
  //
  // NOTE: This is permitted to return a function from an arbitrary module.
  llvm::Function *GetLiftedTraceDeclaration(uint64_t addr) override {
    auto trace_it = traces.find(addr);
    if (trace_it != traces.end()) {
      return trace_it->second;
    } else {
      return nullptr;
    }
  }

  // Get a definition for a lifted trace.
  //
  // NOTE: This is permitted to return a function from an arbitrary module.
  llvm::Function *GetLiftedTraceDefinition(uint64_t addr) override {
    return GetLiftedTraceDeclaration(addr);
  }

  // Try to read an executable byte of memory. Returns `true` of the byte
  // at address `addr` is executable and readable, and updates the byte
  // pointed to by `byte` with the read value.
  bool TryReadExecutableByte(uint64_t addr, uint8_t *byte) override {
    auto byte_it = memory.find(addr);
    if (byte_it != memory.end()) {
      *byte = byte_it->second;
      return true;
    } else {
      return false;
    }
  }

 public:
  Memory &memory;
  std::unordered_map<uint64_t, llvm::Function *> traces;
};

int main(int argc, char *argv[])
{
    std::ifstream inputFile(argv[1]);
    std::vector<std::string> instructions = extract_instructions(inputFile); 
    
    llvm::LLVMContext context;
    auto arch = remill::Arch::Get(context, REMILL_OS, REMILL_ARCH);

    std::unique_ptr<llvm::Module> module(remill::LoadArchSemantics(arch.get()));


    std::string currentline;
    llvm::Module dest_module("lifted_code", context);
    arch->PrepareModuleDataLayout(&dest_module);

    for(int i = 0; i < (int)instructions.size(); i++)
    {
        currentline = instructions[0];
        currentline = instructions[0].substr(0, currentline.find(" "));

        if(currentline[currentline.length() -1] == '\r' || currentline[currentline.length() -1] == '\n')
        {
            currentline.pop_back();
        }
        Memory memory = UnhexlifyInputBytes(currentline);
        SimpleTraceManager manager(memory);
        remill::IntrinsicTable intrinsics(module.get());
        auto inst_lifter = arch->DefaultLifter(intrinsics);
        remill::Instruction I;
        remill::TraceLifter trace_lifter(arch.get(), manager);
        trace_lifter.Lift(0);

        remill::OptimizationGuide guide = {};
        remill::OptimizeModule(arch, module, manager.traces, guide);

        for (auto &lifted_entry : manager.traces) {
            remill::MoveFunctionIntoModule(lifted_entry.second, &dest_module);
        }
        
    }
    llvm::outs() << dest_module;
    return 0;
}



#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <glog/logging.h>
#include <gflags/gflags.h>

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>

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
      std::cerr << "Invalid hex byte value '" << (int)nibbles[0]
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
  //get input js file and extract instructions
  std::ifstream inputFile(argv[1]);
  std::vector<std::string> instructions = extract_instructions(inputFile); 
  
  //set up module
  llvm::LLVMContext context;
  auto arch = remill::Arch::Get(context, REMILL_OS, "amd64");
  std::unique_ptr<llvm::Module> module(remill::LoadArchSemantics(arch.get()));

  //set up destination module
  llvm::Module dest_module("lifted_code", context);
  arch->PrepareModuleDataLayout(&dest_module);

  std::string currentline;

  int lastCount = 0;
  std::vector<std::string> problemInstructions;

  
  remill::IntrinsicTable intrinsics(module.get());
  remill::InstructionLifter inst_lifter(arch, intrinsics);

  for(int i = 0; i < (int)instructions.size(); i++)
  {
    //remove any whitespace from current instruction
    currentline = instructions[i];
    currentline = instructions[i].substr(0, currentline.find(" "));
    //std::cout << currentline << std::endl;
    
    //lift instruction
    Memory memory = UnhexlifyInputBytes(currentline);
    SimpleTraceManager manager(memory);
    remill::TraceLifter trace_lifter(inst_lifter, manager);
    trace_lifter.Lift(0);

    //optimize module
    remill::OptimizationGuide guide = {};
    remill::OptimizeModule(arch, module, manager.traces, guide);

    //place lifted function in destination module
    for (auto &lifted_entry : manager.traces) {
      remill::MoveFunctionIntoModule(lifted_entry.second, &dest_module);
    }
  
    int count = 0;
    for(auto argumentIterator = dest_module.begin(); argumentIterator != dest_module.end(); argumentIterator++)
    {
      std::string name = argumentIterator->getName().data();
      if(name.find("sub_0") != std::string::npos)
      {
        //std::cout << name <<std::endl;
        count++;
      }
    }

    //if program doesn't generate a new function try again
    if(lastCount == count)
    {
      problemInstructions.push_back(currentline);
      //std::cout << "problem found" << std::endl;
      i--;
    }
    lastCount = count;


  }
  
  //create main function type
  llvm::FunctionType *funcType = llvm::FunctionType::get(
    llvm::Type::getInt32Ty(context), // Return type
    dest_module.getFunction("sub_0")->getFunctionType()->params(),
    false                           // Variadic argument
  );

  //create main function
  llvm::Function *mainFunc = llvm::Function::Create(
    funcType,
    llvm::Function::ExternalLinkage,
    "main",
    dest_module
  );
  
  //put main function into module
  llvm::BasicBlock *entryBlock = llvm::BasicBlock::Create(context, "entry", mainFunc);
  llvm::IRBuilder<> builder(entryBlock);
    
  llvm::Function* currentFunction;

  //get function arguments
  std::vector<llvm::Value*> ArgsV;
  for(auto argumentIterator = mainFunc->arg_begin(); argumentIterator != mainFunc->arg_end(); argumentIterator++)
  {
    ArgsV.push_back(argumentIterator);
  }
  llvm::ArrayRef<llvm::Value*> args = ArgsV;

  //add function calls
  for(int i = 0; i < (int)dest_module.getFunctionList().size(); i++)
  {
    //determine function name
    std::string functionName;
    if(i == 0)
    {
      functionName = "sub_0";
    }
    else
    {
      functionName = "sub_0." + std::to_string(i);
      
    }
    currentFunction = dest_module.getFunction(functionName);

    //make sure function actuall exists
    if(currentFunction !=  NULL)
    {
      //create call
      builder.CreateCall(currentFunction, args, llvm::Twine(functionName)); 
    }
  }
  //add return
  builder.CreateRet(llvm::ConstantInt::get(context, llvm::APInt(32, 0)));

  //send destination module to default output
  llvm::outs() << dest_module;
  return 0;
}



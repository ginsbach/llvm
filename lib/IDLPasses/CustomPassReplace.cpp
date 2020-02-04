#include "llvm/IDL/CustomPasses.hpp"
#include "llvm/IDL/Solution.hpp"
#include "llvm/IR/ModuleSlotTracker.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>

using namespace llvm;

class ResearchReplacerBase : public ModulePass
{
public:
    using Pivot  = std::function<Value*(const Solution&)>;
    using Action = std::function<void(Function&, Solution)>;

    static char ID;

    ResearchReplacerBase(std::vector<std::tuple<std::string,Pivot,Action>> i) : ModulePass(ID), idioms(i) { }

    bool runOnModule(Module& module) override;

private:
    std::vector<std::tuple<std::string,Pivot,Action>> idioms;
};

bool ResearchReplacerBase::runOnModule(Module& module)
{
    ModuleSlotTracker slot_tracker(&module);
    auto print_module = [&slot_tracker](Module& m)->std::string {
        std::string string_representation;
        llvm::raw_string_ostream out_stream(string_representation);
        out_stream<<m;
        return string_representation;
    };

    std::string filename = module.getName();
    for(char& c : filename) if(c == '/') c = '_';

    std::ofstream ofs("replace-source-"+filename+".ll");
    ofs<<print_module(module);
    ofs.close();

    ofs.open("replace-report-"+filename+".json");
    ofs<<"{ \"filename\": \""<<(std::string)module.getName()<<"\",\n  \"detected\": [";

    char first_hit1 = true;
    for(Function& function : module.getFunctionList())
    {
        if(!function.isDeclaration())
        {
            if(function.getBasicBlockList().size() > 1000) continue;

            for(auto& idiom : idioms)
            {
                for(auto& solution : GenerateAnalysis(std::get<0>(idiom))(function, 99))
                {
                    int pivot_begin = -1;
                    if(const auto& pivot_lookup = std::get<1>(idiom))
                        if(auto pivot = dyn_cast_or_null<Instruction>(pivot_lookup(solution)))
                            if(auto& debugloc = pivot->getDebugLoc())
                                pivot_begin = debugloc.getLine();

                    ofs<<(first_hit1?"{\n":", {\n");
                    ofs<<"    \"function\": \""<<(std::string)function.getName()<<"\",\n";
                    if(pivot_begin > 0)
                        ofs<<"    \"line\": "<<pivot_begin<<",\n";
                    ofs<<"    \"type\": \""<<std::get<0>(idiom)<<"\",\n";
                    ofs<<"    \"solution\":\n     ";
                    for(char c : solution.prune().print_json(slot_tracker))
                    {
                        ofs.put(c);
                        if(c == '\n') ofs<<"     ";
                    }
                    ofs<<"\n  }";
                    first_hit1 = false;

                    if(std::get<2>(idiom))
                        std::get<2>(idiom)(function, solution);
                }
            }
        }
    }

    ofs<<"]\n}\n";
    ofs.close();

    return false;
}

char ResearchReplacerBase::ID = 0;

void replace_idiom(Function& function, Solution solution, std::string harness_name,
                   Value* insertion, std::vector<Value*> variables, std::vector<Value*> removeeffects)
{
    Instruction* insertion_point = dyn_cast_or_null<Instruction>(insertion);

    std::vector<Value*> variable_values;
    std::vector<Type*>  variable_types;
    size_t i = 0;
    for(auto value : variables)
    {
        if(value)
        {
            variable_values.push_back(value);
            variable_types.push_back(value->getType());
        }
        else std::cerr<<harness_name<<" argument "<<i<<" is zero!\n";
        i++;
    }

    FunctionType* func_type = FunctionType::get(Type::getVoidTy(function.getContext()), variable_types, false);
    Constant*          func = function.getParent()->getOrInsertFunction(harness_name, func_type);

    CallInst::Create(func, variable_values, "", insertion_point);

    for(auto& value : removeeffects)
    {
        if(auto inst = dyn_cast_or_null<Instruction>(value))
        {
            inst->eraseFromParent();
        }
    }
}

class ResearchReplacer : public ResearchReplacerBase
{
public:
    ResearchReplacer() : ResearchReplacerBase({
        #include "CustomPassReplacerLiLACGenerated.h"
    }) { }
};

static RegisterPass<ResearchReplacer> X("research-replacer", "Research replacer", false, false);

ModulePass *llvm::createResearchReplacerPass() {
  return new ResearchReplacer();
}
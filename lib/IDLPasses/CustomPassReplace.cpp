#include "llvm/IDL/CustomPasses.hpp"
#include "llvm/IDL/FunctionWrap.hpp"
#include "llvm/IDL/Solution.hpp"
#include "llvm/IR/ModuleSlotTracker.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>

using namespace llvm;

class ResearchReplacer : public ModulePass
{
public:
    static char ID;

    ResearchReplacer() : ModulePass(ID) { }

    bool runOnModule(Module& module) override;
};

bool ResearchReplacer::runOnModule(Module& module)
{
    ModuleSlotTracker slot_tracker(&module);

    std::string filename = module.getName();
    for(char& c : filename) if(c == '/') c = '_';

    std::stringstream sstr;
    sstr<<filename<<".idl";
    std::ofstream ofs(sstr.str().c_str());

    char first_hit1 = true;
    for(Function& function : module.getFunctionList())
    {
        if(!function.isDeclaration())
        {
            ofs<<(first_hit1?"":"\n")<<"Export Constraint ";
            for(char c : (std::string)function.getName())
                ofs<<((isalpha(c) || isdigit(c))?c:'_');
            ofs<<"\n(";

            std::map<Value*,std::string> varnames;
            std::set<std::string>        used_names;
            size_t                       anon_counter = 0;

            auto wrap = FunctionWrap(function);
            for(Value* value : wrap)
            {
                std::string string_value;
                raw_string_ostream out_stream(string_value);
                value->printAsOperand(out_stream, false, slot_tracker);

                std::string varname;
                for(char c : (std::string)out_stream.str())
                {
                    char cleaned_c = (isalpha(c) || isdigit(c))?c:'_';
                    if(cleaned_c != '_' || (!varname.empty() && varname.back() != '_'))
                        varname.push_back(cleaned_c);
                }

                if(!varname.empty() && varname.back() == '_')
                    varname.pop_back();

                if(varname == "badref")
                {
                    std::stringstream sstr;
                    sstr<<"anonymous["<<(anon_counter++)<<"]";
                    varname = sstr.str();
                }

                if(varname.empty() || (!isalpha(varname.front()) && varname.front() != '_'))
                    varname.insert(varname.begin(), '_');

                while(used_names.find(varname) != used_names.end())
                    varname.push_back('_');
                used_names.insert(varname);
                varnames[value] = varname;
            }

            std::vector<size_t> solver_order;
            for(size_t i = 0; i < wrap.size(); i++)
                if(isa<Instruction>(wrap[i]))
                    solver_order.push_back(i);
            for(size_t i = 0; i < wrap.size(); i++)
                if(!isa<Instruction>(wrap[i]))
                    solver_order.push_back(i);

            for(size_t i : solver_order)
            {
                std::string string_value;
                llvm::raw_string_ostream out_stream(string_value);
                out_stream<<*wrap[i];

                ofs<<((i==0)?" #":"\n  #");
                for(char c : out_stream.str())
                    if(c != '\n') ofs <<c;
                    else break;
                ofs<<"\n";

                if(isa<Constant>(wrap[i]))      ofs<<"  {"<<varnames[wrap[i]]<<"} is a constant and\n";
                else if(isa<Argument>(wrap[i])) ofs<<"  {"<<varnames[wrap[i]]<<"} is an argument and\n";
                else if(auto inst = dyn_cast<Instruction>(wrap[i]))
                {
                    std::string opcode;
            
                    // This mess is necessary because of capitalisation in class names but not opcodes :(
                    switch (inst->getOpcode()) {
                    // Br would actually work with "br" and "Br" as well, this is just aesthetics!
                    case Instruction::Br:             opcode = "branch";         break;
                    case Instruction::IndirectBr:     opcode = "IndirectBr";     break;
                    case Instruction::CleanupRet:     opcode = "CleanupRet";     break;
                    case Instruction::CatchRet:       opcode = "CatchRet";       break;
                    case Instruction::CatchPad:       opcode = "CatchPad";       break;
                    case Instruction::CatchSwitch:    opcode = "CatchSwitch";    break;
//                  case Instruction::CallBr:         opcode = "CallBr";         break;
//                  case Instruction::FNeg:           opcode = "fneg";           break;
                    case Instruction::URem:           opcode = "urem";           break;
                    case Instruction::SRem:           opcode = "srem";           break;
                    case Instruction::FRem:           opcode = "frem";           break;
                    case Instruction::Xor:            opcode = "xor";            break;
                    case Instruction::AtomicCmpXchg:  opcode = "AtomicCmpXchg";  break;
                    case Instruction::AtomicRMW:      opcode = "AtomicRMW";      break;
                    // GetElementPtr would actually work with "GetElementPtr" as well, this is just aesthetics!
                    case Instruction::GetElementPtr:  opcode = "gep";            break;
                    case Instruction::FPToUI:         opcode = "FPToUI";         break;
                    case Instruction::FPToSI:         opcode = "FPToSI";         break;
                    case Instruction::UIToFP:         opcode = "UIToFP";         break;
                    case Instruction::SIToFP:         opcode = "SIToFP";         break;
                    case Instruction::IntToPtr:       opcode = "IntToPtr";       break;
                    case Instruction::PtrToInt:       opcode = "PtrToInt";       break;
                    case Instruction::BitCast:        opcode = "BitCast";        break;
                    case Instruction::AddrSpaceCast:  opcode = "AddrSpaceCast";  break;
                    case Instruction::LShr:           opcode = "LShr";           break;
                    case Instruction::AShr:           opcode = "AShr";           break;
                    case Instruction::VAArg:          opcode = "VAArg";          break;
                    case Instruction::ExtractElement: opcode = "ExtractElement"; break;
                    case Instruction::InsertElement:  opcode = "InsertElement";  break;
                    case Instruction::ShuffleVector:  opcode = "ShuffleVector";  break;
                    case Instruction::ExtractValue:   opcode = "ExtractValue";   break;
                    case Instruction::InsertValue:    opcode = "InsertValue";    break;
                    case Instruction::LandingPad:     opcode = "LandingPad";     break;
                    case Instruction::CleanupPad:     opcode = "CleanupPad";     break;
                    // Fall through when only the first character is capitalised.
                    default:                          opcode = inst->getOpcodeName();
                    }

                    if(!opcode.empty())
                        ofs<<"  {"<<varnames[wrap[i]]<<"} is "<<opcode<<" instruction and\n";
                    else // should never happen
                        ofs<<"  {"<<varnames[wrap[i]]<<"} is an instruction and\n";

                    for(size_t idx : wrap.rodfg[0][i])
                        ofs<<"  {"<<varnames[wrap[idx]]<<"} is first argument of {"<<varnames[inst]<<"} and\n";
                    for(size_t idx : wrap.rodfg[1][i])
                        ofs<<"  {"<<varnames[wrap[idx]]<<"} is second argument of {"<<varnames[inst]<<"} and\n";
                    for(size_t idx : wrap.rodfg[2][i])
                        ofs<<"  {"<<varnames[wrap[idx]]<<"} is third argument of {"<<varnames[inst]<<"} and\n";
                    for(size_t idx : wrap.rodfg[3][i])
                        ofs<<"  {"<<varnames[wrap[idx]]<<"} is fourth argument of {"<<varnames[inst]<<"} and\n";

                    for(size_t idx : wrap.ocfg[0][i])
                        ofs<<"  {"<<varnames[wrap[idx]]<<"} is first successor of {"<<varnames[inst]<<"} and\n";
                    for(size_t idx : wrap.ocfg[1][i])
                        ofs<<"  {"<<varnames[wrap[idx]]<<"} is second successor of {"<<varnames[inst]<<"} and\n";
                    for(size_t idx : wrap.ocfg[2][i])
                        ofs<<"  {"<<varnames[wrap[idx]]<<"} is third successor of {"<<varnames[inst]<<"} and\n";
                    for(size_t idx : wrap.ocfg[3][i])
                        ofs<<"  {"<<varnames[wrap[idx]]<<"} is fourth successor of {"<<varnames[inst]<<"} and\n";
                }
                else ofs<<"  {"<<varnames[wrap[i]]<<"} is preexecution and\n";
            }

            ofs<<"  {dummy} is unused)\nEnd\n";
            first_hit1 = false;
        }
    }

    ofs.close();

    return false;
}

char ResearchReplacer::ID = 0;

static RegisterPass<ResearchReplacer> X("research-replacer", "Research replacer", false, false);

ModulePass *llvm::createResearchReplacerPass() {
  return new ResearchReplacer();
}

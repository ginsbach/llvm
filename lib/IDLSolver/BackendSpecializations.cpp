#include "llvm/IDL/BackendSpecializations.hpp"
#include <unordered_map>

BackendConstantValue::BackendConstantValue(const FunctionWrap& wrap, unsigned c)
                    : BackendSingle(std::vector<unsigned>{c}) { }

BackendLLVMSingleBase::BackendLLVMSingleBase(const FunctionWrap& wrap, std::function<bool(llvm::Value&)> pred)
                     : BackendSingle(compute_hits(wrap, pred)) { }

std::vector<unsigned> BackendLLVMSingleBase::compute_hits(const FunctionWrap& wrap, std::function<bool(llvm::Value&)> pred)
{
    std::vector<unsigned> inst_hits;
    std::vector<unsigned> val_hits;
    unsigned inst_it = 0;
    unsigned vals_it = 0;
    
    std::unordered_map<llvm::Instruction*,unsigned> instr_hash;
    std::unordered_map<llvm::Value*,unsigned>       value_hash;
    for(auto& block : wrap.function.getBasicBlockList())
    {
        for(auto& instruction : block)
        {
            if(pred == nullptr || pred(instruction)) inst_hits.push_back(inst_it);

            instr_hash[&instruction] = inst_it++;

            for(auto& operand : instruction.operands())
            {
                if(llvm::isa<llvm::Constant>(operand.get()) ||
                   llvm::isa<llvm::Argument>(operand.get()))
                {
                    if(value_hash.find(operand.get()) == value_hash.end())
                    {
                        if(pred == nullptr || pred(*operand.get())) val_hits.push_back(vals_it);
                        value_hash[operand.get()] = vals_it++;
                    }
                }
            }
        }
    }

    for(auto hit : inst_hits) {
        val_hits.push_back(hit + vals_it);
    }
    return val_hits;
}

BackendNotNumericConstant::BackendNotNumericConstant(const FunctionWrap& wrap)
      : BackendLLVMSingle<llvm::Value>(wrap, [](llvm::Value& value)
                                              { return !llvm::isa<llvm::ConstantInt>(value) &&
                                                       !llvm::isa<llvm::ConstantFP>(value)  &&
                                                       !llvm::isa<llvm::Function>(value)  &&
                                                       !llvm::isa<llvm::ConstantPointerNull>(value); }) { }


BackendConstant::BackendConstant(const FunctionWrap& wrap)
               : BackendLLVMSingle<llvm::Constant>(wrap, nullptr) { }

BackendPreexecution::BackendPreexecution(const FunctionWrap& wrap)
      : BackendLLVMSingle<llvm::Value>(wrap, [](llvm::Value& value)
                                             { return !llvm::isa<llvm::Instruction>(value); }) { }


std::vector<std::string> function_whitelist = {"log", "exp", "sqrt", "cos", "sin", "tan", "logf", "expf", "sqrtf",
                                               "cosf", "sinf", "tanf", "llvm.fabs.f64", "llvm.fabs.f32",
                                               "@llvm.log.f64", "@llvm.log.f32", "@llvm.sqrt.f64", "@llvm.sqrt.f32",
                                               "llvm.dbg.value", "llvm.dbg.declare", "__log_finite"};

BackendFunctionAttribute::BackendFunctionAttribute(const FunctionWrap& wrap)
      : BackendLLVMSingle<llvm::Function>(wrap, [](llvm::Function& value)
{
    std::string function_name = value.getName();

    if(std::find(function_whitelist.begin(), function_whitelist.end(), function_name) != function_whitelist.end())
        return true;

    auto attributes = value.getAttributes();

    if(!(attributes.hasAttribute(llvm::AttributeList::FunctionIndex, llvm::Attribute::ReadOnly) ||
         attributes.hasAttribute(llvm::AttributeList::FunctionIndex, llvm::Attribute::ReadNone)) ||
       !attributes.hasAttribute(llvm::AttributeList::FunctionIndex, llvm::Attribute::NoUnwind))
    {
        return false;
    }

    return true;
}) { }


BackendArgument::BackendArgument(const FunctionWrap& wrap)
               : BackendLLVMSingle<llvm::Argument>(wrap, nullptr) { }

BackendFloatZero::BackendFloatZero(const FunctionWrap& wrap)
                : BackendLLVMSingle<llvm::ConstantFP>(wrap, [](llvm::ConstantFP& value) { return value.isZero(); }) { }

BackendIntZero::BackendIntZero(const FunctionWrap& wrap)
              : BackendLLVMSingle<llvm::ConstantInt>(wrap, [](llvm::ConstantInt& value) { return value.isZero(); }) { }

BackendOpcode::BackendOpcode(const FunctionWrap& wrap)
      : BackendLLVMSingleBase(wrap, [](llvm::Value& val)
{
    if(llvm::isa<llvm::Instruction>(&val) )
        return true;
    else return false;
}) { }

BackendOpcode::BackendOpcode(const FunctionWrap& wrap, unsigned op)
      : BackendLLVMSingleBase(wrap, [op](llvm::Value& val)
{
    if(auto inst = llvm::dyn_cast<llvm::Instruction>(&val) )
        return inst->getOpcode() == op;
    else return false;
}) { }

template<bool(llvm::Type::*predicate)() const>
BackendLLVMType<predicate>::BackendLLVMType(const FunctionWrap& wrap)
      : BackendLLVMSingle<llvm::Value>(wrap, [](llvm::Value& value)
                                              { return (value.getType()->*predicate)(); }) { }

template<std::vector<std::vector<unsigned>> FunctionWrap::* forw_graph,
         std::vector<std::vector<unsigned>> FunctionWrap::* back_graph>
BackendLLVMEdge<forw_graph,back_graph>::BackendLLVMEdge(const FunctionWrap& wrap)
                                      : BackendEdge(wrap.*forw_graph, wrap.*back_graph) { }

template<unsigned i>
BackendLLVMOperand<i>::BackendLLVMOperand(const FunctionWrap& wrap)
                     : BackendEdge(wrap.odfg[i], wrap.rodfg[i]) { }

template<unsigned i>
BackendLLVMSuccessor<i>::BackendLLVMSuccessor(const FunctionWrap& wrap)
                       : BackendEdge(wrap.rocfg[i], wrap.ocfg[i]) { }

template<bool lt, bool eq, bool gt>
BackendOrderWrap<lt,eq,gt>::BackendOrderWrap(const FunctionWrap&)
                          : BackendOrdering<lt,eq,gt>() { }

template<bool inverted, bool unstrict, unsigned origin_calc,
         std::vector<std::vector<unsigned>> FunctionWrap::* forw_graph>
BackendLLVMDominate<inverted,unstrict,origin_calc,forw_graph>::BackendLLVMDominate(std::array<unsigned,3> sizes,
                                                                                   const FunctionWrap& wrap)
      : BackendDominate<inverted,unstrict>({{(unsigned)std::get<0>(sizes) + (unsigned)get_origins(wrap).size(),
                                             (unsigned)std::get<1>(sizes), (unsigned)std::get<2>(sizes)}},
                                            wrap.*forw_graph),
        number_origins(get_origins(wrap).size())
{
    std::vector<unsigned> origins = get_origins(wrap);

    for(unsigned i = 0; i < number_origins; i++)
    {
        BackendDominate<inverted,unstrict>::template begin<0>       (i);
        BackendDominate<inverted,unstrict>::template skip_invalid<0>(i, origins[i]);
        BackendDominate<inverted,unstrict>::template fixate<0>      (i, origins[i]);
    }
}

template<bool inverted, bool unstrict, unsigned origin_calc,
         std::vector<std::vector<unsigned>> FunctionWrap::* forw_graph>
std::vector<unsigned> BackendLLVMDominate<inverted,unstrict,origin_calc,forw_graph>::get_origins(const FunctionWrap& wrap)
{
    bool data_origins    = origin_calc==0 || origin_calc==4;
    bool data_sinks      = origin_calc==1 || origin_calc==5;
    bool control_origins = origin_calc==2 || origin_calc==4;
    bool control_sinks   = origin_calc==3 || origin_calc==5;

    std::vector<unsigned> origins;

    for(unsigned i = 0; i < wrap.size(); i++)
    {
        if((data_sinks || data_origins) && llvm::isa<llvm::CallInst>(wrap[i]))
        {
            if(auto call_inst = llvm::dyn_cast<llvm::CallInst>(wrap[i]))
            {
                if(call_inst->getCalledFunction() == nullptr)
                {
                    //WOW, wait a second, this is overly optimistic!
                    continue;
                }

                std::string function_name = call_inst->getCalledFunction()->getName();

                if(std::find(function_whitelist.begin(), function_whitelist.end(), function_name)
                   != function_whitelist.end())
                    continue;

                auto attributes = call_inst->getCalledFunction()->getAttributes();

                if(!(attributes.hasAttribute(llvm::AttributeList::FunctionIndex, llvm::Attribute::ReadOnly) ||
                     attributes.hasAttribute(llvm::AttributeList::FunctionIndex, llvm::Attribute::ReadNone)) ||
                   !attributes.hasAttribute(llvm::AttributeList::FunctionIndex, llvm::Attribute::NoUnwind))
                {
                    origins.push_back(i);
                }
            }
        }
        else if((data_origins    && llvm::isa<llvm::LoadInst>(wrap[i]))   ||
                (data_origins    && llvm::isa<llvm::Argument>(wrap[i])) ||
                (data_sinks      && llvm::isa<llvm::StoreInst>(wrap[i]))  ||
                (data_sinks      && llvm::isa<llvm::ReturnInst>(wrap[i]))    ||
                (control_origins && wrap.rcfg[i].empty() && !wrap.cfg[i].empty()) ||
                (control_sinks   && wrap.cfg[i].empty()  && !wrap.rcfg[i].empty()))
        {
            origins.push_back(i);
        }
    }

    return origins;
}


template class BackendLLVMType<&llvm::Type::isIntegerTy>;
template class BackendLLVMType<&llvm::Type::isFloatTy>;
template class BackendLLVMType<&llvm::Type::isVectorTy>;
template class BackendLLVMType<&llvm::Type::isPointerTy>;

template class BackendOrderWrap<false,true,false>;
template class BackendOrderWrap<true,false,true>;
template class BackendOrderWrap<true,false,false>;

template class BackendLLVMEdge<&FunctionWrap::blocks, &FunctionWrap::rblocks>;

template class BackendLLVMEdge<&FunctionWrap::dfg, &FunctionWrap::rdfg>;
template class BackendLLVMEdge<&FunctionWrap::cfg, &FunctionWrap::rcfg>;
template class BackendLLVMEdge<&FunctionWrap::cdg, &FunctionWrap::rcdg>;
template class BackendLLVMEdge<&FunctionWrap::pdg, &FunctionWrap::rpdg>;

template class BackendLLVMOperand<0>;
template class BackendLLVMOperand<1>;
template class BackendLLVMOperand<2>;
template class BackendLLVMOperand<3>;

template class BackendLLVMSuccessor<0>;
template class BackendLLVMSuccessor<1>;
template class BackendLLVMSuccessor<2>;
template class BackendLLVMSuccessor<3>;

template class BackendLLVMDominate<false, true,  0, &FunctionWrap:: dfg>;
template class BackendLLVMDominate<false, true,  1, &FunctionWrap::rdfg>;
template class BackendLLVMDominate<false, false, 0, &FunctionWrap:: dfg>;
template class BackendLLVMDominate<false, false, 1, &FunctionWrap::rdfg>;

template class BackendLLVMDominate<false, true,  2, &FunctionWrap:: cfg>;
template class BackendLLVMDominate<false, true,  3, &FunctionWrap::rcfg>;
template class BackendLLVMDominate<false, false, 2, &FunctionWrap:: cfg>;
template class BackendLLVMDominate<false, false, 3, &FunctionWrap::rcfg>;

template class BackendLLVMDominate<false, true,  4, &FunctionWrap:: pdg>;
template class BackendLLVMDominate<false, true,  5, &FunctionWrap::rpdg>;
template class BackendLLVMDominate<false, false, 4, &FunctionWrap:: pdg>;
template class BackendLLVMDominate<false, false, 5, &FunctionWrap::rpdg>;

template class BackendLLVMDominate<true, true,  0, &FunctionWrap:: dfg>;
template class BackendLLVMDominate<true, true,  1, &FunctionWrap::rdfg>;
template class BackendLLVMDominate<true, false, 0, &FunctionWrap:: dfg>;
template class BackendLLVMDominate<true, false, 1, &FunctionWrap::rdfg>;

template class BackendLLVMDominate<true, true,  2, &FunctionWrap:: cfg>;
template class BackendLLVMDominate<true, true,  3, &FunctionWrap::rcfg>;
template class BackendLLVMDominate<true, false, 2, &FunctionWrap:: cfg>;
template class BackendLLVMDominate<true, false, 3, &FunctionWrap::rcfg>;

template class BackendLLVMDominate<true, true,  4, &FunctionWrap:: pdg>;
template class BackendLLVMDominate<true, true,  5, &FunctionWrap::rpdg>;
template class BackendLLVMDominate<true, false, 4, &FunctionWrap:: pdg>;
template class BackendLLVMDominate<true, false, 5, &FunctionWrap::rpdg>;

template class BackendLLVMDominate<false, false, UINT_MAX, &FunctionWrap::cfg>;
template class BackendLLVMDominate<false, false, UINT_MAX, &FunctionWrap::dfg>;
template class BackendLLVMDominate<false, false, UINT_MAX, &FunctionWrap::pdg>;

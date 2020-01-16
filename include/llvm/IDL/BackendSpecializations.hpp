#ifndef _BACKEND_SPECIALIZATIONS_HPP_
#define _BACKEND_SPECIALIZATIONS_HPP_
#include "BackendClasses.hpp"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Timer.h"
#include <vector>
#include <iostream>

class BackendConstantValue : public BackendSingle
{
public:
    BackendConstantValue() = default;
    BackendConstantValue(const FunctionWrap& wrap, unsigned val=UINT_MAX-1);
};

using BackendUnused = BackendConstantValue;

class BackendLLVMSingleBase : public BackendSingle
{
public:
    BackendLLVMSingleBase() = default;
    BackendLLVMSingleBase(const FunctionWrap& wrap, std::function<bool(llvm::Value&)> pred);

private:
    static std::vector<unsigned> compute_hits(const FunctionWrap& wrap, std::function<bool(llvm::Value&)> pred);
};

template<typename Type>
class BackendLLVMSingle : public BackendLLVMSingleBase
{
public:
    BackendLLVMSingle() = default;
    BackendLLVMSingle(const FunctionWrap& wrap, std::function<bool(Type&)> pred) :
        BackendLLVMSingleBase(wrap, [pred](llvm::Value& val)->bool
        {
            if(auto cast_val = llvm::dyn_cast<Type>(&val))
                return pred == nullptr || pred(*cast_val);
            else return false;
        }) { }
};

class BackendNotNumericConstant : public BackendLLVMSingle<llvm::Value>
{
public:
    BackendNotNumericConstant() = default;
    BackendNotNumericConstant(const FunctionWrap& wrap);
};

class BackendConstant : public BackendLLVMSingle<llvm::Constant>
{
public:
    BackendConstant() = default;
    BackendConstant(const FunctionWrap& wrap);
};


class BackendPreexecution : public BackendLLVMSingle<llvm::Value>
{
public:
    BackendPreexecution() = default;
    BackendPreexecution(const FunctionWrap& wrap);
};

class BackendArgument : public BackendLLVMSingle<llvm::Argument>
{
public:
    BackendArgument() = default;
    BackendArgument(const FunctionWrap& wrap);
};

class BackendFloatZero : public BackendLLVMSingle<llvm::ConstantFP>
{
public:
    BackendFloatZero() = default;
    BackendFloatZero(const FunctionWrap& wrap);
};

class BackendIntZero : public BackendLLVMSingle<llvm::ConstantInt>
{
public:
    BackendIntZero() = default;
    BackendIntZero(const FunctionWrap& wrap);
};

class BackendOpcode : public BackendLLVMSingleBase
{
public:
    BackendOpcode() = default;
    BackendOpcode(const FunctionWrap& wrap);
    BackendOpcode(const FunctionWrap& wrap, unsigned op);
};

using BackendInstruction = BackendOpcode;

template<bool(llvm::Type::*predicate)() const>
class BackendLLVMType: public BackendLLVMSingle<llvm::Value>
{
public:
    BackendLLVMType() = default;
    BackendLLVMType(const FunctionWrap& wrap);
};

class BackendFunctionAttribute : public BackendLLVMSingle<llvm::Function>
{
public:
    BackendFunctionAttribute() = default;
    BackendFunctionAttribute(const FunctionWrap& wrap);
};

template<std::vector<std::vector<unsigned>> FunctionWrap::* forw_graph,
         std::vector<std::vector<unsigned>> FunctionWrap::* back_graph>
class BackendLLVMEdge : public BackendEdge
{
public:
    BackendLLVMEdge(const FunctionWrap& wrap);
};

template<unsigned i>
class BackendLLVMOperand : public BackendEdge
{
public:
    BackendLLVMOperand(const FunctionWrap& wrap);
};

template<unsigned i>
class BackendLLVMSuccessor : public BackendEdge
{
public:
    BackendLLVMSuccessor(const FunctionWrap& wrap);
};

template<bool lt, bool eq, bool gt>
class BackendOrderWrap : public BackendOrdering<lt,eq,gt>
{
public:
    BackendOrderWrap(const FunctionWrap&);
};

template<bool inverted, bool unstrict, unsigned origin_calc,
         std::vector<std::vector<unsigned>> FunctionWrap::* forw_graph>
class BackendLLVMDominate : public BackendDominate<inverted,unstrict>
{
public:
    BackendLLVMDominate(std::array<unsigned,3> sizes, const FunctionWrap& wrap);

    template<unsigned idx1> SkipResult skip_invalid(unsigned idx2, unsigned &c)
        { return BackendDominate<inverted,unstrict>::template skip_invalid<idx1>(idx2+(idx1?0:number_origins), c); }

    template<unsigned idx1> void begin(unsigned idx2)
        { BackendDominate<inverted,unstrict>::template begin<idx1>(idx2+(idx1?0:number_origins)); }

    template<unsigned idx1> void fixate(unsigned idx2, unsigned c)
        { BackendDominate<inverted,unstrict>::template fixate<idx1>(idx2+(idx1?0:number_origins), c); }

    template<unsigned idx1> void resume(unsigned idx2)
        { BackendDominate<inverted,unstrict>::template resume<idx1>(idx2+(idx1?0:number_origins)); }

private:
    static std::vector<unsigned> get_origins(const FunctionWrap& wrap);

private:
    unsigned number_origins;
};

using BackendIntegerType = BackendLLVMType<&llvm::Type::isIntegerTy>;
using BackendFloatType   = BackendLLVMType<&llvm::Type::isFloatTy>;
using BackendVectorType  = BackendLLVMType<&llvm::Type::isVectorTy>;
using BackendPointerType = BackendLLVMType<&llvm::Type::isPointerTy>;

using BackendSame     = BackendOrderWrap<false,true,false>;
using BackendDistinct = BackendOrderWrap<true,false,true>;
using BackendOrder    = BackendOrderWrap<true,false,false>;

using BackendBlock    = BackendLLVMEdge<&FunctionWrap::blocks, &FunctionWrap::rblocks>;
using BackendDFGEdge  = BackendLLVMEdge<&FunctionWrap::dfg,    &FunctionWrap::rdfg>;
using BackendCFGEdge  = BackendLLVMEdge<&FunctionWrap::cfg,    &FunctionWrap::rcfg>;
using BackendCDGEdge  = BackendLLVMEdge<&FunctionWrap::cdg,    &FunctionWrap::rcdg>;
using BackendPDGEdge  = BackendLLVMEdge<&FunctionWrap::pdg,    &FunctionWrap::rpdg>;

using BackendFirstOperand  = BackendLLVMOperand<0>;
using BackendSecondOperand = BackendLLVMOperand<1>;
using BackendThirdOperand  = BackendLLVMOperand<2>;
using BackendFourthOperand = BackendLLVMOperand<3>;

using BackendFirstSuccessor  = BackendLLVMSuccessor<0>;
using BackendSecondSuccessor = BackendLLVMSuccessor<1>;
using BackendThirdSuccessor  = BackendLLVMSuccessor<2>;
using BackendFourthSuccessor = BackendLLVMSuccessor<3>;

using BackendDFGDominate       = BackendLLVMDominate<false, true,  0, &FunctionWrap:: dfg>;
using BackendDFGPostdom        = BackendLLVMDominate<false, true,  1, &FunctionWrap::rdfg>;
using BackendDFGDominateStrict = BackendLLVMDominate<false, false, 0, &FunctionWrap:: dfg>;
using BackendDFGPostdomStrict  = BackendLLVMDominate<false, false, 1, &FunctionWrap::rdfg>;

using BackendCFGDominate       = BackendLLVMDominate<false, true,  2, &FunctionWrap:: cfg>;
using BackendCFGPostdom        = BackendLLVMDominate<false, true,  3, &FunctionWrap::rcfg>;
using BackendCFGDominateStrict = BackendLLVMDominate<false, false, 2, &FunctionWrap:: cfg>;
using BackendCFGPostdomStrict  = BackendLLVMDominate<false, false, 3, &FunctionWrap::rcfg>;

using BackendPDGDominate       = BackendLLVMDominate<false, true,  4, &FunctionWrap:: pdg>;
using BackendPDGPostdom        = BackendLLVMDominate<false, true,  5, &FunctionWrap::rpdg>;
using BackendPDGDominateStrict = BackendLLVMDominate<false, false, 4, &FunctionWrap:: pdg>;
using BackendPDGPostdomStrict  = BackendLLVMDominate<false, false, 5, &FunctionWrap::rpdg>;

using BackendDFGNotDominate       = BackendLLVMDominate<true, true,  0, &FunctionWrap:: dfg>;
using BackendDFGNotPostdom        = BackendLLVMDominate<true, true,  1, &FunctionWrap::rdfg>;
using BackendDFGNotDominateStrict = BackendLLVMDominate<true, false, 0, &FunctionWrap:: dfg>;
using BackendDFGNotPostdomStrict  = BackendLLVMDominate<true, false, 1, &FunctionWrap::rdfg>;

using BackendCFGNotDominate       = BackendLLVMDominate<true, true,  2, &FunctionWrap:: cfg>;
using BackendCFGNotPostdom        = BackendLLVMDominate<true, true,  3, &FunctionWrap::rcfg>;
using BackendCFGNotDominateStrict = BackendLLVMDominate<true, false, 2, &FunctionWrap:: cfg>;
using BackendCFGNotPostdomStrict  = BackendLLVMDominate<true, false, 3, &FunctionWrap::rcfg>;

using BackendPDGNotDominate       = BackendLLVMDominate<true, true,  4, &FunctionWrap:: pdg>;
using BackendPDGNotPostdom        = BackendLLVMDominate<true, true,  5, &FunctionWrap::rpdg>;
using BackendPDGNotDominateStrict = BackendLLVMDominate<true, false, 4, &FunctionWrap:: pdg>;
using BackendPDGNotPostdomStrict  = BackendLLVMDominate<true, false, 5, &FunctionWrap::rpdg>;

using BackendCFGBlocked = BackendLLVMDominate<false, false, UINT_MAX, &FunctionWrap::cfg>;
using BackendDFGBlocked = BackendLLVMDominate<false, false, UINT_MAX, &FunctionWrap::dfg>;
using BackendPDGBlocked = BackendLLVMDominate<false, false, UINT_MAX, &FunctionWrap::pdg>;

#endif

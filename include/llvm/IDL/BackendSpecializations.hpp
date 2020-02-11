#ifndef _BACKEND_SPECIALIZATIONS_HPP_
#define _BACKEND_SPECIALIZATIONS_HPP_
#include "BackendClasses.hpp"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Timer.h"
#include <vector>

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

class BackendNotNumericConstant : public BackendLLVMSingleBase
{
public:
    BackendNotNumericConstant() = default;
    BackendNotNumericConstant(const FunctionWrap& wrap);
};

class BackendConstant : public BackendLLVMSingleBase
{
public:
    BackendConstant() = default;
    BackendConstant(const FunctionWrap& wrap);
};

class BackendPreexecution : public BackendLLVMSingleBase
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

class BackendFunctionAttribute : public BackendLLVMSingle<llvm::Function>
{
public:
    BackendFunctionAttribute() = default;
    BackendFunctionAttribute(const FunctionWrap& wrap);
};

class BackendIntegerType : public BackendLLVMSingleBase
{
public:
    BackendIntegerType() = default;
    BackendIntegerType(FunctionWrap& wrap)
       : BackendLLVMSingleBase(wrap, [](llvm::Value& v) { return v.getType()->isIntegerTy(); }) { }
};

class BackendFloatType : public BackendLLVMSingleBase
{
public:
    BackendFloatType() = default;
    BackendFloatType(FunctionWrap& wrap)
       : BackendLLVMSingleBase(wrap, [](llvm::Value& v) { return v.getType()->isFloatTy(); }) { }
};

class BackendVectorType : public BackendLLVMSingleBase
{
public:
    BackendVectorType() = default;
    BackendVectorType(FunctionWrap& wrap)
       : BackendLLVMSingleBase(wrap, [](llvm::Value& v) { return v.getType()->isVectorTy(); }) { }
};

class BackendPointerType : public BackendLLVMSingleBase
{
public:
    BackendPointerType() = default;
    BackendPointerType(FunctionWrap& wrap)
       : BackendLLVMSingleBase(wrap, [](llvm::Value& v) { return v.getType()->isPointerTy(); }) { }
};

class BackendDFGEdge : public BackendEdge
    { public: BackendDFGEdge(FunctionWrap& wrap) : BackendEdge(wrap.dfg, wrap.rdfg) { } };

class BackendCFGEdge : public BackendEdge
    { public: BackendCFGEdge(FunctionWrap& wrap) : BackendEdge(wrap.cfg, wrap.rcfg) { } };

class BackendCDGEdge : public BackendEdge
    { public: BackendCDGEdge(FunctionWrap& wrap) : BackendEdge(wrap.cdg, wrap.rcdg) { } };

class BackendPDGEdge : public BackendEdge
    { public: BackendPDGEdge(FunctionWrap& wrap) : BackendEdge(wrap.pdg, wrap.rpdg) { } };

class BackendBlock : public BackendEdge
    { public: BackendBlock(FunctionWrap& wrap) : BackendEdge(wrap.blocks, wrap.rblocks) { } };

class BackendFirstOperand : public BackendEdge
    { public: BackendFirstOperand(FunctionWrap& wrap) : BackendEdge(wrap.odfg[0], wrap.rodfg[0]) { } };

class BackendSecondOperand : public BackendEdge
    { public: BackendSecondOperand(FunctionWrap& wrap) : BackendEdge(wrap.odfg[1], wrap.rodfg[1]) { } };

class BackendThirdOperand : public BackendEdge
    { public: BackendThirdOperand(FunctionWrap& wrap) : BackendEdge(wrap.odfg[2], wrap.rodfg[2]) { } };

class BackendFourthOperand : public BackendEdge
    { public: BackendFourthOperand(FunctionWrap& wrap) : BackendEdge(wrap.odfg[3], wrap.rodfg[3]) { } };

class BackendFirstSuccessor : public BackendEdge
    { public: BackendFirstSuccessor(FunctionWrap& wrap) : BackendEdge(wrap.rocfg[0], wrap.ocfg[0]) { } };

class BackendSecondSuccessor : public BackendEdge
    { public: BackendSecondSuccessor(FunctionWrap& wrap) : BackendEdge(wrap.rocfg[1], wrap.ocfg[1]) { } };

class BackendThirdSuccessor : public BackendEdge
    { public: BackendThirdSuccessor(FunctionWrap& wrap) : BackendEdge(wrap.rocfg[2], wrap.ocfg[2]) { } };

class BackendFourthSuccessor : public BackendEdge
    { public: BackendFourthSuccessor(FunctionWrap& wrap) : BackendEdge(wrap.rocfg[3], wrap.ocfg[3]) { } };

template<bool lt, bool eq, bool gt>
class BackendOrderWrap : public BackendOrdering<lt,eq,gt>
{
public:
    BackendOrderWrap(const FunctionWrap&);
};

using BackendSame     = BackendOrderWrap<false,true,false>;
using BackendDistinct = BackendOrderWrap<true,false,true>;

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

using BackendCFGDominate       = BackendLLVMDominate<false, true,  2, &FunctionWrap:: cfg>;
using BackendCFGPostdom        = BackendLLVMDominate<false, true,  3, &FunctionWrap::rcfg>;
using BackendCFGDominateStrict = BackendLLVMDominate<false, false, 2, &FunctionWrap:: cfg>;
using BackendCFGPostdomStrict  = BackendLLVMDominate<false, false, 3, &FunctionWrap::rcfg>;

using BackendPDGDominate       = BackendLLVMDominate<false, true,  4, &FunctionWrap:: pdg>;
using BackendPDGPostdom        = BackendLLVMDominate<false, true,  5, &FunctionWrap::rpdg>;
using BackendPDGDominateStrict = BackendLLVMDominate<false, false, 4, &FunctionWrap:: pdg>;
using BackendPDGPostdomStrict  = BackendLLVMDominate<false, false, 5, &FunctionWrap::rpdg>;

using BackendCFGBlocked = BackendLLVMDominate<false, false, UINT_MAX, &FunctionWrap::cfg>;
using BackendDFGBlocked = BackendLLVMDominate<false, false, UINT_MAX, &FunctionWrap::dfg>;
using BackendPDGBlocked = BackendLLVMDominate<false, false, UINT_MAX, &FunctionWrap::pdg>;

#endif

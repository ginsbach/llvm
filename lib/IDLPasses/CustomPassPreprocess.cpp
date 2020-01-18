#include "llvm/IDL/CustomPasses.hpp"
#include "llvm/IDL/Solution.hpp"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include <unordered_set>
#include <string>
#include <vector>

using namespace llvm;

class ResearchPreprocessor : public FunctionPass
{
public:
    static char ID;

    ResearchPreprocessor() : FunctionPass(ID) { }

    bool runOnFunction(Function& function) override;
};

class SmartIRBuilder : public IRBuilder<>
{
public:
    using IRBuilder<>::IRBuilder;

    Value* CreateAdd(Value* a, int64_t bv)
    {
        if(bv == 0) return a;

        ConstantInt*    b = dyn_cast<ConstantInt>(ConstantInt::get(a->getType(), bv));
        BinaryOperator* binop;
        BinaryOperator* binop2;
        ConstantInt*    c;
        ConstantInt*    d;

        if((binop = dyn_cast<BinaryOperator>(a)) &&
           binop->getOpcode() == Instruction::Add &&
           (c = dyn_cast<ConstantInt>(binop->getOperand(1))))
        {
            return IRBuilder<>::CreateAdd(binop->getOperand(0),
                                          IRBuilder<>::CreateAdd(c, b));
        }
        if((binop = dyn_cast<BinaryOperator>(a)) &&
           binop->getOpcode() == Instruction::Mul &&
           (c = dyn_cast<ConstantInt>(binop->getOperand(1))) &&
           (binop2 = dyn_cast<BinaryOperator>(binop->getOperand(0))) &&
           binop2->getOpcode() == Instruction::Add &&
           (d = dyn_cast<ConstantInt>(binop2->getOperand(1))) &&
           b->getSExtValue() > 0 && c->getSExtValue() > 0 &&
           b->getSExtValue() % c->getSExtValue() == 0)
        {
            return IRBuilder<>::CreateMul(
                       IRBuilder<>::CreateAdd(binop2->getOperand(0),
                                              IRBuilder<>::CreateAdd(d, IRBuilder<>::CreateUDiv(b, c))), c);
        }
        return IRBuilder<>::CreateAdd(a, b);
    }

    Value* CreateAdd(Value* a, Value* b)
    {
        ConstantInt* c;
        if((c = dyn_cast<ConstantInt>(a))) return CreateAdd(b, c->getSExtValue());
        if((c = dyn_cast<ConstantInt>(b))) return CreateAdd(a, c->getSExtValue());

        BinaryOperator* binop1;
        BinaryOperator* binop2;
        ConstantInt*    fac1;
        ConstantInt*    fac2;

        if((binop1 = dyn_cast<BinaryOperator>(a)) &&
           binop1->getOpcode() == Instruction::Mul &&
           (fac1 = dyn_cast<ConstantInt>(binop1->getOperand(1))) &&
           (binop2 = dyn_cast<BinaryOperator>(b)) &&
           binop2->getOpcode() == Instruction::Mul &&
           (fac2 = dyn_cast<ConstantInt>(binop2->getOperand(1))))
        {
            if(fac1->getSExtValue() % fac2->getSExtValue() == 0)
            {
                return IRBuilder<>::CreateMul(
                           IRBuilder<>::CreateAdd(IRBuilder<>::CreateMul(binop1->getOperand(0),
                                                                         IRBuilder<>::CreateUDiv(fac1, fac2)),
                                                  binop2->getOperand(0)), fac2);
            }
            if(fac2->getSExtValue() % fac1->getSExtValue() == 0)
            {
                return IRBuilder<>::CreateMul(
                           IRBuilder<>::CreateAdd(binop1->getOperand(0),
                                                  IRBuilder<>::CreateMul(binop2->getOperand(0),
                                                                         IRBuilder<>::CreateUDiv(fac2, fac1))), fac1);
            }
        }

        return IRBuilder<>::CreateAdd(a, b);
    }

    Value* CreateMul(Value* a, int64_t bv)
    {
        if(bv == 1) return a;

        ConstantInt*    b = dyn_cast<ConstantInt>(ConstantInt::get(a->getType(), bv));
        BinaryOperator* binop;
        BinaryOperator* binop2;
        ConstantInt*    c;
        ConstantInt*    d;

        if((binop = dyn_cast<BinaryOperator>(a)) &&
           binop->getOpcode() == Instruction::Mul &&
           (c = dyn_cast<ConstantInt>(binop->getOperand(1))))
        {
            return IRBuilder<>::CreateMul(binop->getOperand(0),
                                          IRBuilder<>::CreateMul(c, b));
        }
        if((binop = dyn_cast<BinaryOperator>(a)) &&
           binop->getOpcode() == Instruction::Add &&
           (c = dyn_cast<ConstantInt>(binop->getOperand(1))) &&
           (binop2 = dyn_cast<BinaryOperator>(binop->getOperand(0))) &&
           binop2->getOpcode() == Instruction::Mul &&
           (d = dyn_cast<ConstantInt>(binop2->getOperand(1))))
        {
            return IRBuilder<>::CreateAdd(IRBuilder<>::CreateMul(binop2->getOperand(0),
                                                                 IRBuilder<>::CreateMul(d, b)),
                                          IRBuilder<>::CreateMul(c, b));
        }
        return IRBuilder<>::CreateMul(a, b);
    }

    Value* CreateMul(Value* a, Value* b)
    {
        ConstantInt* c;
        if((c = dyn_cast<ConstantInt>(a))) return CreateMul(b, c->getSExtValue());
        if((c = dyn_cast<ConstantInt>(b))) return CreateMul(a, c->getSExtValue());
        return IRBuilder<>::CreateMul(a, b);
    }
};

bool ResearchPreprocessor::runOnFunction(Function& function)
{
    DataLayout data_layout(function.getParent());

/*  I DON'T REMEMBER WHAT THIS WAS FOR ANYMORE!
    // Eliminate useless PHI nodes.
    for(BasicBlock& block : function.getBasicBlockList())
    {
        for(Instruction& instruction : block.getInstList())
        {
            if(auto* phi = dyn_cast<PHINode>(&instruction))
            {
                auto previous = dyn_cast<Instruction>(phi->getOperand(0));

                for(int i = 1; i < phi->getNumOperands() && previous; i++)
                {
                    auto next = dyn_cast<Instruction>(phi->getOperand(i));
                    if(next && previous->isIdenticalTo(next))
                        previous = next;
                    else
                        previous = nullptr;
                }

                if(previous)
                {
                    auto clone = previous->clone();
                    clone->insertBefore(phi->getParent()->getFirstNonPHI());

                    phi->replaceAllUsesWith(clone);
                }
            }
        }
    }
*/
    for(BasicBlock& block : function.getBasicBlockList())
    {
        for(Instruction& instruction : block.getInstList())
        {
            BasicBlock::iterator iter(&instruction);
            SmartIRBuilder builder(&block, iter);

            Instruction* mul_inst;
            ConstantInt* intconst;
            int64_t      or_const;
            int64_t      mul_const;

            if(instruction.getOpcode() == Instruction::Shl &&
               (intconst = dyn_cast<ConstantInt>(instruction.getOperand(1))) &&
               intconst->getZExtValue() < 32)
            {
                instruction.replaceAllUsesWith(
                    builder.CreateMul(
                        instruction.getOperand(0),
                        ConstantInt::get(intconst->getType(), 1<<intconst->getZExtValue())));
            }
            else if(instruction.getOpcode() == Instruction::Or &&
               (intconst = dyn_cast<ConstantInt>(instruction.getOperand(1))) &&
               (or_const = intconst->getSExtValue()) &&
               (mul_inst = dyn_cast<Instruction>(instruction.getOperand(0))) &&
               (intconst = dyn_cast<ConstantInt>(mul_inst->getOperand(1))) &&
               ((mul_inst->getOpcode() == Instruction::Mul && (mul_const = intconst->getSExtValue())) ||
                (mul_inst->getOpcode() == Instruction::Shl && intconst->getZExtValue() < 32 &&
                                                              (mul_const = 1 << intconst->getZExtValue()))))
            {
                while(mul_const % 2 == 0)
                {
                    mul_const /= 2;
                    or_const  /= 2;
                }

                if(or_const != 0) continue;

                instruction.replaceAllUsesWith(
                    builder.CreateAdd(instruction.getOperand(0), instruction.getOperand(1)));
            }
            else if(auto gep_inst = dyn_cast<GetElementPtrInst>(&instruction))
            {
                std::vector<Value*> reverse_index_vector;
                for(unsigned i = 1; i < gep_inst->getNumOperands(); i++)
                {
                    reverse_index_vector.push_back(gep_inst->getOperand(gep_inst->getNumOperands()-i));
                }

                auto innermost_gep = gep_inst;
                while(auto next_gep = dyn_cast<GetElementPtrInst>(innermost_gep->getPointerOperand()))
                {
                    reverse_index_vector.back() = builder.CreateAdd(reverse_index_vector.back(),
                                                                    next_gep->getOperand(next_gep->getNumOperands()-1));

                    for(unsigned i = 2; i < next_gep->getNumOperands(); i++)
                    {
                        reverse_index_vector.push_back(next_gep->getOperand(next_gep->getNumOperands()-i));
                    }
                    
                    innermost_gep = next_gep;
                }

                std::reverse(reverse_index_vector.begin(), reverse_index_vector.end());

                auto gepinst = dyn_cast<GetElementPtrInst>(&instruction);
                Type*  type  = innermost_gep->getPointerOperandType();
                Value* index = ConstantInt::get(reverse_index_vector[0]->getType(), 0);

                std::vector<Value*> indizes;
                std::vector<size_t> factors;

                {
                    std::string string_value;
                    llvm::raw_string_ostream out_stream(string_value);
                    out_stream<<*gepinst;
                }

                for(auto operand : reverse_index_vector)
                {
                    PointerType* pointer_type;
                    VectorType*  vec_type;
                    ArrayType*   arr_type;
                    StructType*  struct_type;
                    ConstantInt* struct_member;

                    if((pointer_type = dyn_cast<PointerType>(type)))
                    {
                        index = builder.CreateAdd(
                                    index,
                                    builder.CreateMul(
                                        operand,
                                        data_layout.getTypeAllocSize(pointer_type->getElementType())));
                        type = pointer_type->getElementType();
                    }
                    else if((vec_type = dyn_cast<VectorType>(type)) &&
                            (pointer_type = dyn_cast<PointerType>(vec_type->getElementType())))
                    {
                        index = builder.CreateAdd(
                                    index,
                                    builder.CreateMul(
                                        operand,
                                        ConstantInt::get(operand->getType(), data_layout.getTypeAllocSize(pointer_type->getElementType()))));
                        type = vec_type->getElementType();
                    }
                    else if((arr_type = dyn_cast<ArrayType>(type)))
                    {
                        index = builder.CreateAdd(
                                    index,
                                    builder.CreateMul(
                                        operand,
                                        data_layout.getTypeAllocSize(arr_type->getElementType())));
                        type = arr_type->getElementType();
                    }
                    else if((struct_type = dyn_cast<StructType>(type)) &&
                            (struct_member = dyn_cast<ConstantInt>(operand)))
                    {
                        unsigned member_nr = struct_member->getZExtValue();

                        auto offset = data_layout.getStructLayout(struct_type)->getElementOffset(member_nr);

                        index = builder.CreateAdd(index, offset);
                        type = struct_type->getElementType(member_nr);
                    }
                }

                auto gep_origin = innermost_gep->getOperand(0);

                while(auto bitcast_inst = dyn_cast<BitCastInst>(gep_origin))
                {
                    gep_origin = bitcast_inst->getOperand(0);
                }

                auto new_gep = builder.CreateBitCast(
                                   builder.CreateGEP(
                                       builder.CreateBitCast(
                                           gep_origin,
                                           Type::getInt8PtrTy(gepinst->getContext(),
                                                              (dyn_cast<PointerType>(type)?dyn_cast<PointerType>(type)->getAddressSpace():0))),
                                       index),
                                   gepinst->getType());

                std::vector<std::pair<Instruction*,unsigned>> uses;
                for(auto& use : instruction.uses())
                {
                    if(auto instruction2 = dyn_cast<Instruction>(use.getUser()))
                        if(instruction2->getOpcode() == Instruction::Load ||
                           instruction2->getOpcode() == Instruction::Store)
                        {
                            uses.emplace_back(instruction2, use.getOperandNo());
                        }
                }

                for(auto use : uses)
                {
                    use.first->setOperand(use.second, new_gep);
                }
            }
        }
    }

    for(auto solution : GenerateAnalysis("ForWithIteratorTest")(function, UINT_MAX))
    {
        auto comparison = dyn_cast<Instruction>((Value*)solution["comparison"]);
        auto increment  = dyn_cast<Instruction>((Value*)solution["increment"]);
        auto iter_step  = dyn_cast<ConstantInt>((Value*)solution["iter_step"]);

        if(!comparison || !increment || !iter_step || iter_step->getSExtValue() < 0) continue;

        comparison->setOperand(0, increment);
        comparison->setOperand(1, BinaryOperator::Create(Instruction::Add,
                                      comparison->getOperand(1),
                                      increment->getOperand(1), "", comparison));
    }

    std::unordered_set<Value*> removed_instructions;

    for(auto solution : GenerateAnalysis("HoistSelect")(function, UINT_MAX))
    {
        SelectInst*        select_inst  = dyn_cast<SelectInst>       ((Value*)solution["select"]);
        GetElementPtrInst* gep_inst1    = dyn_cast<GetElementPtrInst>((Value*)solution["input1"]);
        GetElementPtrInst* gep_inst2    = dyn_cast<GetElementPtrInst>((Value*)solution["input2"]);

        if(removed_instructions.find(select_inst) == removed_instructions.end() &&
           removed_instructions.find(gep_inst1)   == removed_instructions.end() &&
           removed_instructions.find(gep_inst2)   == removed_instructions.end() &&
           gep_inst1->getNumOperands() == 2 && gep_inst2->getNumOperands() == 2 &&
           gep_inst1->getOperand(0)->getType() == gep_inst2->getOperand(0)->getType() &&
           gep_inst1->getOperand(1)->getType() == gep_inst2->getOperand(1)->getType())
        {
            BasicBlock::iterator insert_point(select_inst);
            IRBuilder<> builder(select_inst->getParent(), insert_point);

            ReplaceInstWithValue(select_inst->getParent()->getInstList(), insert_point,
                builder.CreateBitCast(
                    builder.CreateGEP(
                        gep_inst1->getOperand(0),
                        builder.CreateSelect(select_inst->getCondition(),
                                             gep_inst1->getOperand(1),
                                             gep_inst2->getOperand(1))),
                    select_inst->getType()));

            removed_instructions.insert(select_inst);
        }
    }

/*  THIS IS ONLY FOR STENCILS AND TENDS TO MESS THINGS UP
    for(auto solution : GenerateAnalysis("Distributive")(function, UINT_MAX))
    {
        Instruction* sum = dyn_cast<Instruction>((Value*)solution["value"]);

        std::vector<Value*> prod1_factors;
        std::vector<Value*> prod2_factors;
        std::vector<Value*> sum1_factors;
        std::vector<Value*> sum2_factors;
        std::vector<Value*> sum1_results;
        std::vector<Value*> sum2_results;

        bool any_factor_removed = false;
        for(unsigned i = 0; i < 4 ; i++)
        {
            Value* find_prod1_it   = solution["product1"]["factors"][i];
            Value* find_prod2_it   = solution["product2"]["factors"][i];
            Value* find_sum1_it    = solution["sum1"]["factors"][i];
            Value* find_sum2_it    = solution["sum2"]["factors"][i];
            Value* find_sumres1_it = solution["sum1"]["results"][i];
            Value* find_sumres2_it = solution["sum2"]["results"][i];

            if(find_prod1_it   != nullptr) prod1_factors.push_back(find_prod1_it);
            if(find_prod2_it   != nullptr) prod2_factors.push_back(find_prod2_it);
            if(find_sum1_it    != nullptr) sum1_factors.push_back(find_sum1_it);
            if(find_sum2_it    != nullptr) sum2_factors.push_back(find_sum2_it);
            if(find_sumres1_it != nullptr) sum1_results.push_back(find_sumres1_it);
            if(find_sumres2_it != nullptr) sum2_results.push_back(find_sumres2_it);

            if(removed_instructions.find(find_prod1_it)   != removed_instructions.end() ||
               removed_instructions.find(find_prod2_it)   != removed_instructions.end() ||
               removed_instructions.find(find_sum1_it)    != removed_instructions.end() ||
               removed_instructions.find(find_sum2_it)    != removed_instructions.end() ||
               removed_instructions.find(find_sumres1_it) != removed_instructions.end() ||
               removed_instructions.find(find_sumres2_it) != removed_instructions.end()) any_factor_removed = true;
        }

        std::reverse(prod1_factors.begin(), prod1_factors.end());
        std::reverse(prod2_factors.begin(), prod2_factors.end());
        std::reverse(sum1_factors.begin(),  sum1_factors.end());
        std::reverse(sum2_factors.begin(),  sum2_factors.end());
        std::reverse(sum1_results.begin(),  sum1_results.end());
        std::reverse(sum2_results.begin(),  sum2_results.end());

        if(removed_instructions.find(sum) == removed_instructions.end() &&! any_factor_removed &&
           prod1_factors.size() > 0 && prod2_factors.size() > 0 &&
           sum1_factors.size() > 0 && sum2_factors.size() > 0 &&
           sum1_factors.size() == sum1_results.size() && sum2_factors.size() == sum2_results.size())
        {
            Type* int_type = sum->getType();

            BasicBlock::iterator insert_point(sum);
            IRBuilder<> builder(sum->getParent(), insert_point);

            Value* first_sum = ConstantInt::get(int_type, 0);
            for(unsigned j = 1; j <= sum1_factors.size()-1; j++)
                if(j==1 || dyn_cast<Instruction>(sum1_results[j])->getOpcode() == Instruction::Add)
                    first_sum = builder.CreateAdd(builder.CreateSExtOrTrunc(sum1_factors[j], int_type), first_sum);
                else
                    first_sum = builder.CreateSub(builder.CreateSExtOrTrunc(sum1_factors[j], int_type), first_sum);

            Value* second_sum = ConstantInt::get(int_type, 0);
            for(unsigned j = 1; j <= sum2_factors.size()-1; j++)
                if(j==1 || dyn_cast<Instruction>(sum2_results[j])->getOpcode() == Instruction::Add)
                    first_sum = builder.CreateAdd(builder.CreateSExtOrTrunc(sum2_factors[j], int_type), second_sum);
                else
                    first_sum = builder.CreateSub(builder.CreateSExtOrTrunc(sum2_factors[j], int_type), second_sum);

            int factor1 = 1;
            for(unsigned j = 1; j <= sum1_factors.size()-1; j++)
                if(dyn_cast<Instruction>(sum1_results[j])->getOpcode() == Instruction::Sub)
                    factor1 *= -1;

            int factor2 = 1;
            for(unsigned j = 1; j <= sum2_factors.size()-1; j++)
                if(dyn_cast<Instruction>(sum2_results[j])->getOpcode() == Instruction::Sub)
                    factor2 *= -1;

            Value* first_prod  = ConstantInt::get(int_type, factor1);
            Value* second_prod = ConstantInt::get(int_type, factor2);

            for(unsigned j = 1; j <= prod1_factors.size()-1; j++)
                first_prod = builder.CreateMul(builder.CreateSExtOrTrunc(prod1_factors[j], int_type), first_prod);

            for(unsigned j = 1; j <= prod2_factors.size()-1; j++)
                second_prod = builder.CreateMul(builder.CreateSExtOrTrunc(prod2_factors[j], int_type), second_prod);

            ReplaceInstWithValue(sum->getParent()->getInstList(), insert_point,
                builder.CreateAdd(
                    builder.CreateAdd(first_sum, second_sum),
                    builder.CreateMul(
                        builder.CreateAdd(first_prod, second_prod),
                        builder.CreateSExtOrTrunc(prod1_factors[0], int_type))));

            removed_instructions.insert(sum);
        }
    }*/

    return false;
}

char ResearchPreprocessor::ID = 0;

static RegisterPass<ResearchPreprocessor> X("research-preprocessor", "Research preprocessor", false, false);

FunctionPass *llvm::createResearchPreprocessorPass() {
  return new ResearchPreprocessor();
}

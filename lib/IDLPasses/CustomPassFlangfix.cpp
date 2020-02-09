#include "llvm/IDL/CustomPasses.hpp"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

using namespace llvm;

class ResearchFlangfix : public FunctionPass
{
public:
    static char ID;

    ResearchFlangfix() : FunctionPass(ID) {}

    bool runOnFunction(Function& function) override;
};

bool ResearchFlangfix::runOnFunction(Function& function)
{
    return false;
    // Remove secondary induction variables.
    for(BasicBlock& block : function.getBasicBlockList())
    {
        std::vector<std::pair<PHINode*,int>> induction_variables;
    
        for(Instruction& inst : block.getInstList())
        {
            if(auto phinode = dyn_cast<PHINode>(&inst))
            {
                if(auto addinst = dyn_cast<BinaryOperator>(phinode->getOperand(1)))
                {
                    if(auto constint = dyn_cast<ConstantInt>(addinst->getOperand(1)))
                    {
                        if(phinode->getNumOperands() == 2 &&   //check same target block as well
                           addinst->getOpcode() == Instruction::Add &&
                           addinst->getOperand(0) == phinode)
                        {
                            induction_variables.emplace_back(phinode, constint->getSExtValue());
                        }
                    }
                }
                if(auto addinst = dyn_cast<BinaryOperator>(phinode->getOperand(0)))
                {
                    if(auto constint = dyn_cast<ConstantInt>(addinst->getOperand(1)))
                    {
                        if(phinode->getNumOperands() == 2 &&   //check same target block as well
                           addinst->getOpcode() == Instruction::Add &&
                           addinst->getOperand(0) == phinode)
                        {
                            Value* op0 = phinode->getIncomingValue(0);
                            Value* op1 = phinode->getIncomingValue(1);
                            phinode->setIncomingValue(0, op1);
                            phinode->setIncomingValue(1, op0);

                            BasicBlock* bl0 = phinode->getIncomingBlock(0);
                            BasicBlock* bl1 = phinode->getIncomingBlock(1);
                            phinode->setIncomingBlock(0, bl1);
                            phinode->setIncomingBlock(1, bl0);

                            induction_variables.emplace_back(phinode, constint->getSExtValue());
                        }
                    }
                }
            }
        }

        if(induction_variables.size() > 1 && induction_variables[0].second == 1)
        {
            for(unsigned i = 1; i < induction_variables.size(); i++)
            {
                Instruction* instruction = dyn_cast<Instruction>(block.getFirstNonPHI());
                BasicBlock::iterator insert_point(instruction);
                IRBuilder<> builder(instruction->getParent(), insert_point);

                BasicBlock::iterator insert_point2(induction_variables[i].first);

                auto current_iteration =
                    builder.CreateSExtOrTrunc(
                        builder.CreateSub(
                            builder.CreateAdd(
                                induction_variables[0].first,
                                ConstantInt::get(induction_variables[0].first->getType(), 1), "", true, true),
                            induction_variables[0].first->getOperand(0), "", true, true),
                        induction_variables[i].first->getType());


                if(induction_variables[i].second != 1 && induction_variables[i].second != -1)
                {
                    current_iteration =
                        builder.CreateMul(
                            current_iteration,
                            ConstantInt::get(induction_variables[i].first->getType(),
                                             abs(induction_variables[i].second)), "", true, true);
                }


                Value* recomputed_next_iterator;

                if(induction_variables[i].second >= 0)
                {
                    recomputed_next_iterator =
                        builder.CreateAdd(
                            induction_variables[i].first->getOperand(0),
                            current_iteration, "", true, true);
                }
                else
                {
                    recomputed_next_iterator =
                        builder.CreateSub(
                            induction_variables[i].first->getOperand(0),
                            current_iteration, "", true, true);
                }

                auto recomputed_current_iterator =
                    builder.CreateAdd(
                        recomputed_next_iterator,
                        ConstantInt::get(induction_variables[i].first->getType(),
                                         -induction_variables[i].second), "", true, true);

                ReplaceInstWithValue(block.getInstList(), insert_point2, recomputed_current_iterator);
            }
        }
    }

    // Simplify comparisons slightly.
    for(BasicBlock& block : function.getBasicBlockList())
    {
        for(Instruction& inst : block.getInstList())
        {
            BasicBlock::iterator iter(&inst);
            IRBuilder<> builder(&block, iter);

            if(auto compare = dyn_cast<CmpInst>(&inst))
            {
                if(auto addition = dyn_cast<BinaryOperator>(compare->getOperand(0)))
                {
                    if(addition->getOpcode() == Instruction::Add &&
                       addition->getOperand(1) == compare->getOperand(1))
                    {
                        compare->setOperand(0, addition->getOperand(0));
                        compare->setOperand(1, ConstantInt::get(addition->getOperand(0)->getType(), 0));
                    }
                }
                if(auto subtraction = dyn_cast<BinaryOperator>(compare->getOperand(0)))
                {
                    if(auto constant = dyn_cast<Constant>(compare->getOperand(1)))
                    {
                        if(subtraction->getOpcode() == Instruction::Sub && constant->isZeroValue())
                        {
                            compare->setOperand(0, subtraction->getOperand(0));
                            compare->setOperand(1, subtraction->getOperand(1));
                        }
                    }
                }
                if(auto truncation = dyn_cast<TruncInst>(compare->getOperand(1)))
                {
                    inst.replaceAllUsesWith(
                        builder.CreateICmp(compare->getPredicate(),
                            builder.CreateSExt(compare->getOperand(0), truncation->getOperand(0)->getType()),
                            truncation->getOperand(0)));
                }
            }
        }
    }

    // Raise BinaryOperations if they are extended later.
    for(BasicBlock& block : function.getBasicBlockList())
    {
        for(Instruction& inst : block.getInstList())
        {
            BasicBlock::iterator iter(&inst);
            IRBuilder<> builder(&block, iter);

            if(auto extension = dyn_cast<SExtInst>(&inst))
            {
                if(auto binaryop = dyn_cast<BinaryOperator>(extension->getOperand(0)))
                {
                    inst.replaceAllUsesWith(
                        builder.CreateBinOp(binaryop->getOpcode(),
                            builder.CreateSExt(binaryop->getOperand(0), extension->getType()),
                            builder.CreateSExt(binaryop->getOperand(1), extension->getType())));
                }
            }
        }
    }

    // Make one based loops zero based.
    for(BasicBlock& block : function.getBasicBlockList())
    {
        std::vector<std::pair<PHINode*,BinaryOperator*>> worklist;
    
        for(Instruction& inst : block.getInstList())
        {
            if(auto phinode = dyn_cast<PHINode>(&inst))
            {
                if(auto constint1 = dyn_cast<ConstantInt>(phinode->getOperand(0)))
                {
                    if(auto addinst = dyn_cast<BinaryOperator>(phinode->getOperand(1)))
                    {
                        if(auto constint2 = dyn_cast<ConstantInt>(addinst->getOperand(1)))
                        {
                            if(phinode->getNumOperands() == 2 &&
                               addinst->getOpcode() == Instruction::Add &&
                               addinst->getOperand(0) == phinode &&
                               constint1->isOneValue() && constint2->isOneValue())
                            {
                                phinode->setOperand(0, ConstantInt::get(phinode->getType(), 0));
                                worklist.emplace_back(phinode, addinst);
                            }
                        }
                    }
                }
            }
        }

        for(auto& entry : worklist)
        {
            BasicBlock::iterator insert_point(block.getFirstNonPHI());
            IRBuilder<> builder(&block, insert_point);

            auto onebased = builder.CreateAdd(entry.first, ConstantInt::get(entry.first->getType(), 1));

            std::vector<std::pair<Instruction*, int>> newworklist;

            for(auto& use : entry.first->uses())
            {
                if(auto instruction = dyn_cast<Instruction>(use.getUser()))
                {
                    if(instruction != entry.second && instruction != onebased)
                    {
                        newworklist.emplace_back(instruction, use.getOperandNo());
                    }
                }
            }

            for(auto& entry2 : newworklist)
            {
                entry2.first->setOperand(entry2.second, onebased);
            }
        }
    }

    // Put constant offset on typed arrays.
    for(BasicBlock& block : function.getBasicBlockList())
    {
        std::vector<std::tuple<BasicBlock::iterator,BitCastInst*,Value*,Constant*>> worklist;
    
        DataLayout data_layout(block.getParent()->getParent());

        for(Instruction& inst : block.getInstList())
        {
            if(auto bitcastinst = dyn_cast<BitCastInst>(&inst))
            {
                if(auto pointertype = dyn_cast<PointerType>(bitcastinst->getType()))
                {
                    if(auto gepinst = dyn_cast<GetElementPtrInst>(bitcastinst->getOperand(0)))
                    {
                        if(auto constant = dyn_cast<ConstantInt>(gepinst->getOperand(1)))
                        {
                            if(gepinst->getNumOperands() == 2)
                            {
                                int size1 = data_layout.getTypeAllocSize(gepinst->getSourceElementType());
                                int size2 = data_layout.getTypeAllocSize(pointertype->getElementType());

                                worklist.emplace_back(bitcastinst, bitcastinst, gepinst->getOperand(0),
                                                      ConstantInt::get(constant->getType(),
                                                                       constant->getSExtValue()*size1/size2));
                            }
                        }
                    }
                }
            }
        }

        for(auto& entry : worklist)
        {
            IRBuilder<> builder(&block, std::get<0>(entry));

            ReplaceInstWithValue(block.getInstList(), std::get<0>(entry),
                builder.CreateGEP(
                    builder.CreateBitCast(std::get<2>(entry), std::get<1>(entry)->getType()),
                    std::get<3>(entry)));
        }
    }

    return false;
}

char ResearchFlangfix::ID = 0;

static RegisterPass<ResearchFlangfix> X("research-flangfix", "Research flangfix", false, false);

FunctionPass *llvm::createResearchFlangfixPass() {
  return new ResearchFlangfix();
}

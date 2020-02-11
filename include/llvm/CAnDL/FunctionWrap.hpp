#ifndef _FUNCTION_WRAP_HH_
#define _FUNCTION_WRAP_HH_
#include <vector>

namespace llvm {
class Function;
class Value;
}

using Struct2D = std::vector<std::vector<unsigned>>;
using Struct3D = std::vector<std::vector<std::vector<unsigned>>>;

/* This class is used as a wrapper around llvm::Function. */
class FunctionWrap : public std::vector<llvm::Value*>
{
public:
    FunctionWrap(llvm::Function& llvm_function);

    llvm::Function& function;

    Struct2D cfg;
    Struct2D rcfg;
    Struct3D ocfg;
    Struct3D rocfg;
    Struct2D dfg;
    Struct2D rdfg;
    Struct3D odfg;
    Struct3D rodfg;
    Struct2D cdg;
    Struct2D rcdg;
    Struct2D pdg;
    Struct2D rpdg;
    Struct2D blocks;
    Struct2D rblocks;
};

#endif

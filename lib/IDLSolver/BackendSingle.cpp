#include "llvm/IDL/BackendClasses.hpp"

BackendSingle::BackendSingle(std::vector<SolverAtom::Value> h) : hits(std::move(h)), hit_start(hits.begin()) { }

SkipResult BackendSingle::skip_invalid(SolverAtom::Value& c) const
{
    auto ptr = hit_start;
    while(ptr != hits.end() && c > *ptr) ++ptr;
    if(ptr == hits.end()) return SkipResult::FAIL;
    if(*ptr == c) return SkipResult::PASS;
    c = *ptr;
    return SkipResult::CHANGEPASS;
}

void BackendSingle::begin() { hit_start = hits.begin(); }

void BackendSingle::fixate(SolverAtom::Value c)
{
    for(auto ptr = hit_start; ptr != hits.end(); ptr++)
    {
        if(*ptr >= c)
        {
            hit_start = ptr;
            return;
        }
    }
}

void BackendSingle::resume() { }

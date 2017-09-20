#include "llvm/Constraints/Solution.hpp"
#include "llvm/Constraints/Solver.hpp"
#include "llvm/Constraints/Constraint.hpp"
#include "llvm/Constraints/FunctionWrap.hpp"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Function.h"
#include <algorithm>
#include <sstream>
#include <string>
#include <map>

std::vector<Solution> Solution::Find(const Constraint& constraint, llvm::Function& function,
                                     unsigned max_solutions)
{
    FunctionWrap wrap(function);
    std::vector<std::unique_ptr<SolverAtom>> specials;
    constraint.get_specials(wrap, specials);
    Solver solver(std::move(specials));

    std::vector<Solution> result;
    while(result.size() < max_solutions)
    {
        Solver::max_iterations = UINT_MAX;

        auto solution = solver.next_solution();

        if(solution.empty()) break;

        std::vector<llvm::Value*> llvm_solution;

        std::transform(solution.begin(), solution.end(), std::back_inserter(llvm_solution),
                       [&](unsigned v) { return v < wrap.size() ? wrap[v] : nullptr; });

        result.push_back(Solution(constraint, llvm_solution));
    }

    return result;
}

Solution::Solution(std::vector<std::string> labels, std::vector<llvm::Value*> values)
              : single_value(nullptr)
{
    if(labels.size() == 1 && values.size() == 1 && labels.front().empty())
    {
        single_value = values.front();
        return;
    }

    struct IdiomPart
    {
        std::vector<std::string>  labels;
        std::vector<llvm::Value*> values;
    };

    std::map<std::string,IdiomPart> map_parts;
    std::vector<IdiomPart>          vector_parts;

    for(unsigned i = 0; i < labels.size() && i < values.size(); i++)
    {
        if(labels[i].empty())
            return;

        if(labels[i][0] == '[')
        {
            unsigned index = 0;

            std::string::const_iterator it = labels[i].begin();

            while(++it != labels[i].end())
            {
                if(*it >= '0' && *it <= '9')
                    index = index * 10 + (*it - '0');
                else break;
            }

            if(index >= vector_parts.size())
                vector_parts.resize(index+1);

            if(it != labels[i].end() && *it == ']')
            {
                if(it+1 == labels[i].end())
                {
                    vector_parts[index].labels.emplace_back("");
                    vector_parts[index].values.emplace_back(values[i]);
                }
                else if(*(it+1) == '[')
                {
                    vector_parts[index].labels.emplace_back(std::string(it+1, labels[i].cend()));
                    vector_parts[index].values.emplace_back(values[i]);
                }
                else if(*(it+1) == '.' && it+2 != labels[i].end() && *(it+2) != '[')
                {
                    vector_parts[index].labels.emplace_back(std::string(it+2, labels[i].cend()));
                    vector_parts[index].values.emplace_back(values[i]);
                }
                else return;
            }
        }
        else if(labels[i][0] != '.')
        {
            std::string::const_iterator it = labels[i].begin();

            while(it != labels[i].end())
            {
                if(*it == '[' || *it == '.')
                    break;
                else ++it;
            }

            if(it == labels[i].end())
            {
                map_parts[labels[i]].labels.emplace_back();
                map_parts[labels[i]].values.emplace_back(values[i]);
            }
            else if(*it == '[')
            {
                map_parts[std::string(labels[i].cbegin(), it)].labels.emplace_back(it, labels[i].cend());
                map_parts[std::string(labels[i].cbegin(), it)].values.emplace_back(values[i]);
            }
            else if(*it == '.' && it+1 != labels[i].end() && *(it+1) != '[')
            {
                map_parts[std::string(labels[i].cbegin(), it)].labels.emplace_back(it+1, labels[i].cend());
                map_parts[std::string(labels[i].cbegin(), it)].values.emplace_back(values[i]);
            }
            else return;
        }
        else return;
    }

    if(map_parts.empty())
    {
        for(auto& part : vector_parts)
            vector_value.emplace_back(Solution(part.labels, part.values));
    }
    if(vector_parts.empty())
    {
        for(auto& part : map_parts)
            map_value.emplace(part.first, Solution(part.second.labels, part.second.values));
    }
}

Solution::operator llvm::Value*() const
{
    return single_value;
}

Solution Solution::operator[](std::string str) const
{
    auto find_it = map_value.find(str);
    return find_it != map_value.end() ? find_it->second : Solution();
}

Solution Solution::operator[](unsigned idx) const
{
    return idx < vector_value.size() ? vector_value[idx] : Solution();
}


Solution::operator std::map<std::string,llvm::Value*>() const
{
    if(single_value)
        return std::map<std::string,llvm::Value*>{{"",single_value}};

    if(map_value.empty())
    {
        std::map<std::string,llvm::Value*> result;

        for(unsigned i = 0; i < vector_value.size(); i++)
        {
            for(auto& part2 : (std::map<std::string,llvm::Value*>)vector_value[i])
            {
                std::stringstream sstr;
                sstr<<"["<<i<<"]";
                if(!part2.first.empty())
                    sstr<<".";
                sstr<<part2.first;

                result.emplace(sstr.str(), part2.second);
            }
        }

        return result;
    }

    if(vector_value.empty())
    {
        std::map<std::string,llvm::Value*> result;

        for(auto& part : map_value)
        {
            for(auto& part2 : (std::map<std::string,llvm::Value*>)part.second)
            {
                std::stringstream sstr;
                sstr<<part.first;
                if(!part2.first.empty() && part2.first.front() != '[')
                    sstr<<".";
                sstr<<part2.first;

                result.emplace(sstr.str(), part2.second);
            }
        }

        return result;
    }

    return {};
}

Solution Solution::prune() const
{
    if(!map_value.empty() && vector_value.empty())
    {
        Solution result;

        for(auto& pair : map_value)
        {
            auto pruned = pair.second.prune();

            if(!pruned.map_value.empty() ||
               !pruned.vector_value.empty() ||
               pruned.single_value)
            {
                result.map_value.emplace(pair.first, pruned);
            }
        }

        return result;
    }

    if(!vector_value.empty() && map_value.empty())
    {
        Solution result;

        for(auto& value : vector_value)
        {
            auto pruned = value.prune();

            if(!pruned.map_value.empty() ||
               !pruned.vector_value.empty() ||
               pruned.single_value)
            {
                result.vector_value.emplace_back(pruned);
            }
        }

        return result;
    }

    return *this;
}

std::string Solution::print_json(llvm::ModuleSlotTracker& slot_tracker) const
{
    if(map_value.empty() && !vector_value.empty())
    {
        std::string result = {'['};

        for(const auto& part : vector_value)
        {
            if(result.size() > 1)
                result += ",\n ";

            std::string temp = part.print_json(slot_tracker);
            for(char c : temp)
            {
                result.push_back(c);
                if(c == '\n')
                    result.push_back(' ');
            }
        }

        result.push_back(']');

        return result;
    }

    if(vector_value.empty() && !map_value.empty())
    {
        std::string result = {'{'};

        for(const auto& part : map_value)
        {
            if(result.size() > 1)
                result += ",\n ";

            std::string temp = part.second.print_json(slot_tracker);

            if(temp.empty() || temp.front() != '[')
            {
                result += "\"" + part.first + "\":";

                for(char c : temp)
                {
                    result.push_back(c);
                    if(c == '\n')
                        result += std::string(part.first.size() + 4, ' ');
                }
            }
            else
            {
                result += "\"" + part.first + "\":\n ";

                for(char c : temp)
                {
                    result.push_back(c);
                    if(c == '\n')
                        result.push_back(' ');
                }
            }
        }

        result.push_back('}');

        return result;
    }

    if(single_value)
    {
        std::string string_value;

        llvm::raw_string_ostream out_stream(string_value);

        if(llvm::Function* func_value = llvm::dyn_cast<llvm::Function>(single_value))
        {
            func_value->printAsOperand(out_stream, true, slot_tracker);
        }
        else if(llvm::Instruction* instr_value = llvm::dyn_cast<llvm::Instruction>(single_value))
        {
            instr_value->print(out_stream, slot_tracker);
        }
        else
        {
            out_stream<<"  ";
            single_value->print(out_stream, slot_tracker);
        }

        std::string escaped_string;

        for(char c : out_stream.str())
        {
            if(c == '\n')      escaped_string += "\\n";
            else if(c == '\"') escaped_string += "\\\"";
            else if(c == '\'') escaped_string += "\\\'";
            else if(c == '\\') escaped_string += "\\\\";
            else               escaped_string += std::string(1, c);
        }

        while(escaped_string.size() > 0 && escaped_string[0] == ' ')
            escaped_string = std::string(escaped_string.begin() + 1, escaped_string.end());

        escaped_string = "\"" + escaped_string + "\"";

        return escaped_string;
    }

    return "null";
}

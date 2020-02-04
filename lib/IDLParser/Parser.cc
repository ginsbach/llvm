// This program receives two filenames as command line arguments.
// The first argument is the filename of a grammar specification in Backus-Naur
// form. The second argument is the filename of a program adhering to that
// specification.
// The output of this program is a rendering of the parsed input from the second
// input file. 

#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <vector>
#include <map>

using namespace std;

// This class implements a language-independent syntax tree. As a conscious
// design decision, the "leaf_value" field is _not_ reused to store the
// value returned by get_type(), although this might seem convenient at first.
class SyntaxTree
{
public:
    SyntaxTree(string l) : leaf_value(l) { }
    SyntaxTree(string l, vector<SyntaxTree> n);

    bool operator!=(const SyntaxTree& t) const;

    bool               isleaf()                const { return node_value.empty() && !leaf_value.empty(); }
    bool               isleaf(const string& s) const { return node_value.empty() && leaf_value == s; }
    bool               isnode()                const { return !node_value.empty(); }
    bool               isnode(const string& s) const { return !node_value.empty() && get_type() == s; }
    const string&      get_leaf()              const { return leaf_value; }
    const string&      get_type()              const { return node_value[0].get_leaf(); }
    size_t             size    ()              const { return node_value.empty()?0:(node_value.size()-1); }
    vector<SyntaxTree> children()              &&;
    const SyntaxTree&  operator[](size_t i)    const { return node_value[i+1]; }

private:
    string             leaf_value;
    vector<SyntaxTree> node_value;

    friend ostream& operator<<(ostream& ostr, const vector<SyntaxTree>& x);
};

// These four functions are called by main to perform all relevant steps.
// Firstly, the Backus-Naur form language specification is parsed.
// Secondly, this is used to inform a second parser for the actual input.
// Thirdly, the generic parsed syntax is analysed with IDL-specific routines.
// Finally, the IDL structures are written to the standard output stream in a
// format that is convenient for the succeeding code generation pass.
vector<pair<string,SyntaxTree>> ParseBNF       (string input);
vector<SyntaxTree>              Parse          (vector<pair<string,SyntaxTree>> bnf_spec, string input);
vector<SyntaxTree>              ProcessIDLSpecs(const vector<SyntaxTree>& in_vector);
ostream&                        operator<<     (ostream& ostr, const vector<SyntaxTree>& x);

// The main function basically just chains the previously declared functions.
int main(int argc, char** argv)
{
    try
    {
        if(argc != 3)
            throw string("This program requires two filenames (BNF + code) as command line arguments.");

        auto read_whole_file = [](char* filename) {
            ifstream ifs(filename);
            return string(istreambuf_iterator<char>(ifs), {});
        };

        auto parsed_bnf     = ParseBNF(read_whole_file(argv[1]));
        auto parse_result   = Parse(parsed_bnf, read_whole_file(argv[2]));
        auto exported_specs = ProcessIDLSpecs(parse_result);
        cout<<exported_specs;
    }
    catch(string error)
    {
        cerr<<"Error: "<<error<<"\n";
        return 1;
    }
    return 0;
}

// The syntax trees are printed with parentheses to signify the hierarchy and
// commas to separate the siblings. Backspace is used as an escape character.
ostream& operator<<(ostream& ostr, const vector<SyntaxTree>& x)
{
    for(size_t i = 0; i < x.size(); i++)
    {
        if(!x[i].node_value.empty()) ostr<<(i?" ":"(")<<x[i].node_value<<",";
        else if(x[i].leaf_value.empty() || !isdigit(x[i].leaf_value[0]))
        {
            ostr<<(i?" \"":"(\"");
            for(char c : x[i].leaf_value)
                if(isprint(c)) ostr<<((c == '('||c == ')'||c == ','||c == ' '||c == '\\')?"\\":"")<<c;
            ostr<<"\",";
        }
        else ostr<<(i?" ":"(")<<x[i].leaf_value<<",";
    }
    return ostr<<(x.size()?")":"()");
}

// This function parses the Backus-Naur form language specification. Different to
// the actual parser that the resulting grammar is informing, this is a hardcoded
// deterministic LALR(2) parser.
// The input has to be fairly well structured. In particular, all tokens have to
// be separated by whitespace, as the lexer is very primitive.
vector<pair<string,SyntaxTree>> ParseBNF(string input)
{
    vector<string> tokens{""};
    for(auto c : input + " ) )")
    {
        if(isspace(c) && !tokens.back().empty()) tokens.push_back("");
        if(!isspace(c)) tokens.back().push_back(c);
    }
    if(tokens.back().empty()) tokens.pop_back();

    vector<SyntaxTree>              stack;
    vector<pair<string,SyntaxTree>> result;
    for(const auto& t : tokens)
    {
        if(t == "::=" || t == "|" || t == "[" || t == "]" || t == "{" || t == "}" || t == "(" || t == ")" || t == "!")
            stack.push_back(SyntaxTree(t));
        else if(t == "<s>")
            stack.push_back(SyntaxTree("string", {}));
        else if(t == "<n>")
            stack.push_back(SyntaxTree("number", {}));
        else if(t.front() == '<' && t.back() == '>')
            stack.push_back(SyntaxTree("reference", {SyntaxTree(string(t.begin()+1, t.end()-1))}));
        else if(t.front() == '\'' && t.back() == '\'' && t.size() >= 2)
            stack.push_back(SyntaxTree("literal", {SyntaxTree(string(t.begin()+1, t.end()-1))}));
        else stack.push_back(SyntaxTree("literal", {SyntaxTree(t)}));

        while(true)
        {
            auto len = [&s=stack](size_t n)->bool             { return s.size() >= n; };
            auto get = [&s=stack](size_t n)->const SyntaxTree&{ return s.rbegin()[n]; };
            auto mov = [&s=stack](size_t n)->SyntaxTree       { return move(s.rbegin()[n]); };
            auto set = [&s=stack](size_t n, SyntaxTree t)     { s.rbegin()[n] = t; s.erase(s.end()-n, s.end()-2); };
            auto add = [get,mov] (size_t n1, size_t n2)->auto {
                auto name = get(n1).get_type();
                auto vect = mov(n1).children();
                vect.push_back(mov(n2));
                return SyntaxTree(name, vect);
            };

            if(len(4) && get(3).isnode() && get(2).isnode() && !get(1).isleaf("::="))
            {
                set(3, get(3).isnode("sequence")?add(3, 2):SyntaxTree("sequence", {mov(3), mov(2)}));
            }
            else if(len(5) && get(4).isleaf("[") && get(3).isnode() && get(2).isleaf("]"))
            {
                set(4, SyntaxTree("optional", get(3).isnode("sequence")?mov(3).children():vector<SyntaxTree>{mov(3)}));
            }
            else if(len(5) && get(4).isleaf("{") && get(3).isnode() && get(2).isleaf("}"))
            {
                set(4, SyntaxTree("repeat", get(3).isnode("sequence")?mov(3).children():vector<SyntaxTree>{mov(3)}));
            }
            else if(len(5) && get(4).isnode() && get(3).isleaf("|") && get(2).isnode() && (get(0).isleaf("::=") ||
                    get(1).isleaf("|") || get(1).isleaf(")") || get(1).isleaf("]") || get(1).isleaf("}")))
            {
                set(4, get(4).isnode("choice")?add(4, 2):SyntaxTree("choice", {mov(4), mov(2)}));
            }
            else if(len(5) && get(4).isnode("literal") && get(3).isleaf("::=") && get(2).isnode() &&
                   ((get(1).isleaf(")") && get(0).isleaf(")")) || (get(1).isnode("literal") && get(0).isleaf("::="))))
            {
                result.push_back({get(4)[0].get_leaf(), mov(2)});
                stack.erase(stack.end()-5, stack.end()-2);
            }
            else if(len(5) && get(4).isleaf("(") && get(3).isnode() && get(2).isleaf(")")) set(4, mov(3));
            else break;
        }
    }

    if(stack.size() > 2)
    {
        stringstream sstr;
        sstr<<"Failed to parse BNF file, as not all syntax collapsed to transformation rules.\n"
            <<"    "<<vector<SyntaxTree>(stack.begin(), stack.end()-2)<<"\n";
        throw sstr.str();
    }

    return result;
}

// This constructor places the type string for the node in the first child and
// simplifies the tree by "inlining" all nodes with type strings containing '@'.
SyntaxTree::SyntaxTree(string l, vector<SyntaxTree> n) : node_value(n)
{
    for(size_t i = 0; i < node_value.size(); )
    {
        SyntaxTree child = move(node_value[i]);

        if(child.isnode() && child.get_type().find('@') != string::npos)
        {
            if(child.node_value.size() > 1)
            {
                node_value[i] = move(child.node_value[1]);
                node_value.insert(node_value.begin()+i+1, make_move_iterator(child.node_value.begin()+2),
                                                          make_move_iterator(child.node_value.end()));
            }
            else node_value.erase(node_value.begin()+i);
            i += child.size();
        }
        else
        {
            node_value[i] = move(child);
            i++;
        }
    }

    node_value.insert(node_value.begin(), SyntaxTree(l));
}

// This gives a way to move out the children without copies.
vector<SyntaxTree> SyntaxTree::children() &&
{
    if(!node_value.empty())
        node_value.erase(node_value.begin());
    return move(node_value);
}

// This class stores one possible state for the non-deterministic parser below.
class ParseBranch : public SyntaxTree
{
public:
    ParseBranch(SyntaxTree h, shared_ptr<ParseBranch> t) : SyntaxTree(h), tail(t) { }

    bool operator==(const ParseBranch& o) const;

    operator vector<SyntaxTree>() &&;

    bool tailtest(const unordered_set<string>&, bool allow_null=false) const;

    ParseBranch fork(bool from_tail, bool keep_head, string name) const;

private:
    shared_ptr<ParseBranch> tail;
};

// These two functors are used by the parser to apply grammar rules.
struct ParserRules
{
    function<void(const ParseBranch&,vector<ParseBranch>&)>             fork_rule;
    function<void(const ParseBranch&,vector<shared_ptr<ParseBranch>>&)> keep_rule;
};

// This function decomposes the parsed Backus-Naur form syntax description
// into simple transition rules that are easily applied by the Parse function.
ParserRules DecomposeBNF(vector<pair<string,SyntaxTree>> bnf);

// This is the actual parser. However, much of the heavy lifting has been
// offloaded to the GrammarRules and ParseBranch classes.
vector<SyntaxTree> Parse(vector<pair<string,SyntaxTree>> bnf_spec, string input)
{
    bool in_comment = false;
    vector<string> tokens{{}};
    for(auto c : input)
    {
        if(c == '#')  in_comment = true;
        if(c == '\n') in_comment = false;
        if(in_comment) continue;
        
        if(isspace(c) && !tokens.back().empty()) tokens.push_back("");
        else if(ispunct(c) && c != '_')
        {
            if(c == '.' && tokens.size() > 2 && tokens.back().empty() && tokens.rbegin()[1]==".")
            {
                tokens.rbegin()[1] = "..";
                continue;
            }
            if(c == '>' && tokens.size() > 2 && tokens.back().empty() && tokens.rbegin()[1]=="-")
            {
                tokens.rbegin()[1] = "->";
                continue;
            }
            if(tokens.back().empty()) tokens.back().push_back(c);
            else tokens.push_back({c});
            tokens.push_back({});
        }
        else if(!isspace(c)) tokens.back().push_back(c);
    }
    if(tokens.back().empty()) tokens.pop_back();    

    auto grammar_rules = DecomposeBNF(bnf_spec);

    vector<shared_ptr<ParseBranch>> syntax_branches{nullptr};
    for(auto& token : tokens)
    {
        vector<ParseBranch> branch_workset;
        for(auto& branch : syntax_branches)
            branch_workset.push_back(ParseBranch(SyntaxTree(token), move(branch)));

        vector<shared_ptr<ParseBranch>> next_syntax_branches;
        while(!branch_workset.empty())
        {
            vector<ParseBranch> next_branch_workset;
            for(auto& branch : branch_workset)
            {
                grammar_rules.fork_rule(branch, next_branch_workset);
                grammar_rules.keep_rule(branch, next_syntax_branches);
            }

            branch_workset = move(next_branch_workset);
        }

        syntax_branches = move(next_syntax_branches);
        if(syntax_branches.empty()) break;
    }

    if(syntax_branches.size() == 1 && syntax_branches.front())
        return move(*syntax_branches.front());
    else throw string("Parsing was ambivalent.");
}

// This is the obvious deep comparison operator using recursive calls.
bool ParseBranch::operator==(const ParseBranch& o) const
{
    if((const SyntaxTree&)*this != (const SyntaxTree&)o) return false;
    if(tail == o.tail) return true;
    if(tail == nullptr || o.tail == nullptr) return false;
    return *tail == *o.tail;
}

// This is the obvious deep comparison operator using recursive calls.
bool SyntaxTree::operator!=(const SyntaxTree& t) const
{
    if(isleaf())  return !t.isleaf(get_leaf());
    if(!isnode()) return t.isleaf() || t.isnode();
    if(!t.isnode(get_type()) || size() != t.size()) return true;
    for(size_t i = 0; i < size(); i++)
        if((*this)[i] != t[i]) return true;
    return false;
}

// By convention, all top-level grammar constructs have to be contained in a
// syntax node of type "#". Otherwise, the parser throws a syntax error here.
ParseBranch::operator vector<SyntaxTree>() &&
{
    auto it = move(tail);
    vector<SyntaxTree> trees{move(*this)};

    while(it != nullptr)
    {
        auto next_it = move(it->tail);
        trees.push_back(move(*it));
        it = next_it;
    }

    for(size_t i = 0; i < trees.size(); i++)
    {
        if(trees[i].isnode("#"))
            trees[i] = trees[i][0];
        else throw string("There was a syntax error.");
    }

    return trees;
}

// This is a workaround to allow building without enabling C++20 in the compiler.
template<class SType, class EType> bool contains(const SType& S, const EType& E) { return S.find(E) != S.end(); }

// This function takes the parsed language specification and decomposes the
// Backus-Naur form syntax rules into small, incremental transformation rules.
ParserRules DecomposeBNF(vector<pair<string,SyntaxTree>> stack)
{
    vector<tuple<string,unordered_set<string>,unordered_set<string>>> ref_rule;
    vector<tuple<string,unordered_set<string>,string>>                lit_rule;
    vector<tuple<string,unordered_set<string>>>                       str_rule;
    vector<tuple<string,unordered_set<string>>>                       nbr_rule;
    while(!stack.empty())
    {
        auto l = move(stack.back().first);
        auto r = move(stack.back().second);
        stack.pop_back();

        using make_set = unordered_set<string>;

        auto str_to_refsyntax = [](unordered_set<string> vec) {
            vector<SyntaxTree> result;
            for(const auto& x : vec) result.push_back(SyntaxTree("reference", {SyntaxTree(x)}));
            return result;
        };
        auto extract_refsyntax = [](const vector<SyntaxTree>& vec) {
            unordered_set<string> result;
            for(const auto& x : vec) if(x.isnode("reference")) result.insert(x[0].get_leaf());
            return result;
        };
        auto extract_non_refsyntax = [](vector<SyntaxTree> vec) {
            vector<SyntaxTree> result;
            for(auto& x : vec) if(!x.isnode("reference")) result.push_back(move(x));
            return result;
        };
        auto sequence_cont = [str_to_refsyntax](unordered_set<string> prev, vector<SyntaxTree> s) {
            s.insert(s.begin(), SyntaxTree("choice", str_to_refsyntax(prev)));
            return SyntaxTree("sequence", s);
        };

        if     (r.isnode("literal"))   lit_rule.emplace_back(l, make_set{}, r[0].get_leaf());
        else if(r.isnode("reference")) ref_rule.emplace_back(l, make_set{}, make_set{r[0].get_leaf()});
        else if(r.isnode("string"))    str_rule.emplace_back(l, make_set{});
        else if(r.isnode("number"))    nbr_rule.emplace_back(l, make_set{});
        else if(r.isnode("choice"))
        {
            auto children = move(r).children();
                
            auto refs = extract_refsyntax(children);
            if(!refs.empty()) ref_rule.emplace_back(l, make_set{}, refs);

            for(auto& choice : extract_non_refsyntax(children))
                stack.push_back({l, move(choice)});
        }
        else if(r.isnode("sequence"))
        {
            size_t counter = 0;
            unordered_set<string> old_tmps;
            unordered_set<string> new_tmps;
            for(auto& part : move(r).children())
            {
                new_tmps.clear();

                stringstream sstr;
                sstr<<l<<"@"<<(counter++);
                string name = sstr.str();

                if(part.isnode("choice") && old_tmps.empty())
                {
                    auto children = move(part).children();

                    new_tmps.merge(extract_refsyntax(children));

                    for(auto& choice : extract_non_refsyntax(children))
                        stack.push_back({name, move(choice)});
                }
                else if(part.isnode("optional") && !old_tmps.empty())
                {
                    stack.push_back({name, sequence_cont(old_tmps, move(part).children())});

                    new_tmps = move(old_tmps);
                    new_tmps.insert(name);
                }

                else if(part.isnode("repeat") && !old_tmps.empty())
                {
                    old_tmps.insert(name);
        
                    stack.push_back({name, sequence_cont(old_tmps, move(part).children())});

                    new_tmps = move(old_tmps);
                }
                else if(part.isnode("literal"))
                {
                    auto find_it = find_if(lit_rule.begin(), lit_rule.end(),
                                           [&](const auto& x) { return (get<1>(x) == old_tmps &&
                                                                        get<2>(x) == part[0].get_leaf() &&
                                                                        get<0>(x).find('@') != string::npos); });
                    if(find_it != lit_rule.end()) name = get<0>(*find_it);
                    else                          lit_rule.emplace_back(name, old_tmps, part[0].get_leaf());
                }
                else if(part.isnode("reference"))
                {
                    auto find_it = find_if(ref_rule.begin(), ref_rule.end(),
                                           [&](const auto& x) { return (get<1>(x) == old_tmps &&
                                                                        get<2>(x) == make_set{part[0].get_leaf()} &&
                                                                        get<0>(x).find('@') != string::npos); });
                    if(find_it != ref_rule.end()) name = get<0>(*find_it);
                    else if(old_tmps.empty())     name = part[0].get_leaf();
                    else                          ref_rule.emplace_back(name, old_tmps, make_set{part[0].get_leaf()});
                }
                else if(part.isnode("string")) str_rule.emplace_back(name, old_tmps);
                else if(part.isnode("number")) nbr_rule.emplace_back(name, old_tmps);
                else throw string("Failed to process BNF file, some sequence rule cannot be dissected fully.");

                new_tmps.insert(name);
                swap(old_tmps, new_tmps);
            }
            for(auto old_tmp : old_tmps)
                ref_rule.push_back({l, make_set{}, make_set{old_tmp}});
        }
        else throw string("Syntax Error in BNF file: No decomposition to break down complex rule.");
    }

    unordered_set<string> types_with_ref_cont{"#"};
    unordered_set<string> types_with_any_cont{"#"};
    for(const auto& rule : lit_rule) types_with_any_cont.insert(get<1>(rule).begin(), get<1>(rule).end());
    for(const auto& rule : ref_rule) types_with_ref_cont.insert(get<1>(rule).begin(), get<1>(rule).end());
    for(const auto& rule : ref_rule) types_with_any_cont.insert(get<1>(rule).begin(), get<1>(rule).end());
    for(const auto& rule : str_rule) types_with_any_cont.insert(get<1>(rule).begin(), get<1>(rule).end());
    for(const auto& rule : nbr_rule) types_with_any_cont.insert(get<1>(rule).begin(), get<1>(rule).end());

    return {[=](const ParseBranch& b, vector<ParseBranch>& out) {
        for(auto& rule : lit_rule)
            if((get<1>(rule).empty() || b.tailtest(get<1>(rule))) && b.isleaf(get<2>(rule)))
                out.emplace_back(b.fork(!get<1>(rule).empty(), false, get<0>(rule)));
        for(auto& rule : ref_rule)
            if((get<1>(rule).empty() || b.tailtest(get<1>(rule))) && b.isnode() && contains(get<2>(rule), b.get_type()))
                out.emplace_back(b.fork(!get<1>(rule).empty(), true, get<0>(rule)));
        for(auto& rule : str_rule)
            if((get<1>(rule).empty() || b.tailtest(get<1>(rule))) && b.isleaf() && !isdigit(b.get_leaf()[0]))
                out.emplace_back(b.fork(!get<1>(rule).empty(), true, get<0>(rule)));
        for(auto& rule : nbr_rule)
            if((get<1>(rule).empty() || b.tailtest(get<1>(rule))) && b.isleaf() && isdigit(b.get_leaf()[0]))
                out.emplace_back(b.fork(!get<1>(rule).empty(), true, get<0>(rule)));
    },
    [=](const ParseBranch& b, vector<shared_ptr<ParseBranch>>& out) {
        if(b.tailtest(types_with_ref_cont, true) && b.isnode() && contains(types_with_any_cont, b.get_type()))
            out.push_back(shared_ptr<ParseBranch>(new ParseBranch(move(b))));
    }};
}

// This function pattern matches on the fail of the branch.
bool ParseBranch::tailtest(const unordered_set<string>& p, bool allow_null) const
{
    return (allow_null && tail == nullptr) || (tail && tail->isnode() && contains(p, tail->get_type()));
}

// This function forks the branch in order to apply a syntax rule.
ParseBranch ParseBranch::fork(bool from_tail, bool keep_head, string n) const
{
    if(from_tail && tail && keep_head) return ParseBranch(SyntaxTree(n, {*tail, *this}), tail->tail);
    else if(from_tail && tail)         return ParseBranch(SyntaxTree(n, {*tail}),        tail->tail);
    else if                (keep_head) return ParseBranch(SyntaxTree(n,        {*this}), tail);
    else                               return ParseBranch(SyntaxTree(n,      {}),        tail);
}

// This is the first function that is specific to IDL. It eliminates certain
// complex langauge features by unrolling loop constructs etc.
SyntaxTree SimplifyIDLConstraint(SyntaxTree in, const unordered_map<string,SyntaxTree>& lookup,
                                 unordered_map<string,int> indices={}, vector<string> collidx = {},
                                 function<SyntaxTree(const SyntaxTree&)> renaming = nullptr);

SyntaxTree FlattenIDLJunctions(SyntaxTree in);

vector<SyntaxTree> ProcessIDLSpecs(const vector<SyntaxTree>& in_vector)
{
    unordered_map<string,SyntaxTree> lookup;
    for(const SyntaxTree& in : in_vector)
        if(in.isnode("specification") && in.size() == 2)
            lookup.insert({in[0].get_leaf(), in[1]});

    vector<SyntaxTree> result;
    for(size_t i = 0; i+1 < in_vector.size(); i++)
        if(in_vector[i+1].isnode("export") && in_vector[i].isnode("specification"))
        {
            auto constraint = in_vector[i][1];
            constraint      = SimplifyIDLConstraint(constraint, lookup);
            constraint      = FlattenIDLJunctions(constraint);
            result.push_back({"specification", {in_vector[i][0], constraint}});
        }

    return result;
}

int CollapseIDLIndex(SyntaxTree in, const unordered_map<string,int>& vars);

vector<SyntaxTree> NormaliseIDLVar(SyntaxTree in, const unordered_map<string,int>& indices,
                                                  const vector<string>&            collidx);

SyntaxTree SimplifyIDLConstraint(SyntaxTree in, const unordered_map<string,SyntaxTree>& lookup,
                                unordered_map<string,int> indices, vector<string> collidx,
                                function<SyntaxTree(const SyntaxTree&)> renaming)
{
    if(in.isnode("atom") && in.size() == 1)
    {
        string             type     = in[0].get_type();
        vector<SyntaxTree> children = move(move(in).children()[0]).children();

        for(auto& child : children)
        {
            if(child.isleaf()) continue;

            vector<SyntaxTree> vars;
            for(auto tmp : NormaliseIDLVar(child, indices, collidx))
                vars.push_back(renaming?renaming(tmp):tmp);

            child = (vars.size()==1)?vars.front():SyntaxTree("slottuple", vars);
        }

        return SyntaxTree("atom", {SyntaxTree(type, children)});
    }
    else if(in.isnode("conRange") || in.isnode("disRange"))
    {
        auto begin = CollapseIDLIndex(in[2], indices);
        auto end   = in.isnode("for")?(begin+1):CollapseIDLIndex(in[3], indices);

        string type = string(in.get_type().begin(), in.get_type().begin()+3)+"junction";

        vector<SyntaxTree> elements;
        for(int index = begin; index < end; index++)
        {
            indices[in[1].get_leaf()] = index;
            elements.push_back(SimplifyIDLConstraint(in[0], lookup, indices, collidx, renaming));
        }

        return SyntaxTree(type, elements);
    }
    else if(in.isnode("conjunction") || in.isnode("disjunction"))
    {
        auto type = in.get_type();

        vector<SyntaxTree> elements;
        for(auto& child : move(in).children())
            elements.push_back(SimplifyIDLConstraint(child, lookup, indices, collidx, renaming));

        return SyntaxTree(type, elements);
    }
    else if(in.isnode("for"))
    {
        indices[in[1].get_leaf()] = CollapseIDLIndex(in[2], indices);
        return SimplifyIDLConstraint(in[0], lookup, indices, collidx, renaming);
    }
    else if(in.isnode("default" ))
    {
        if(!contains(indices, in[1].get_leaf()))
            indices[in[1].get_leaf()] = CollapseIDLIndex(in[2], indices);

        return SimplifyIDLConstraint(in[0], lookup, indices, collidx, renaming);
    }
    else if(in.isnode("rename"))
    {
        auto comp = [](const SyntaxTree& a, const SyntaxTree& b)->bool {
            const SyntaxTree* it1 = &a;
            const SyntaxTree* it2 = &b;
            while(true) {
                if(it1->get_type() < it2->get_type()) return true;
                if(it1->get_type() > it2->get_type()) return false;

                if(it1->isnode("slotbase"))
                    return (*it1)[0].get_leaf() < (*it2)[0].get_leaf();
                else if(it1->isnode("slotmember"))
                {
                    if((*it1)[1].get_leaf() < (*it2)[1].get_leaf()) return true;
                    if((*it1)[1].get_leaf() > (*it2)[1].get_leaf()) return false;
                }
                else if(it1->isnode("slotindex") && ((*it1)[1].isnode("baseconst") || (*it1)[1].isnode("basevar"))
                                                 && ((*it2)[1].isnode("baseconst") || (*it2)[1].isnode("basevar")))
                {
                    if((*it1)[1][0].get_leaf() < (*it2)[1][0].get_leaf()) return true;
                    if((*it1)[1][0].get_leaf() > (*it2)[1][0].get_leaf()) return false;
                }
                else throw string("This shouldn't happen.");
                it1 = &(*it1)[0];
                it2 = &(*it2)[0];
            }
        };

        map<SyntaxTree,SyntaxTree,decltype(comp)> rename(comp);
        SyntaxTree                                prefix("");

        for(size_t i = 1; i+1 < in.size(); i+=2)
        {
            auto tmp1 = NormaliseIDLVar(in[i+1], indices, collidx);
            auto tmp2 = NormaliseIDLVar(in[i],   indices, collidx);
            if(tmp1.size() == tmp2.size())
            {
                for(size_t j = 0; j < tmp1.size(); j++)
                    rename.insert({tmp1[j], tmp2[j]});
            }
            else throw string("Rename of touples with unequal sizes is invalid.");
        }
        if(0 == (in.size() % 2))
        {
            auto tmp1 = NormaliseIDLVar(in[in.size()-1], indices, collidx);
            if(tmp1.size() == 1)
                prefix = tmp1.front();
            else throw string("Cannot prefix a tuple.");
        }

        return SimplifyIDLConstraint(in[0], lookup, indices, collidx, [renaming,&rename,&prefix](const SyntaxTree& var)
        {
            auto apply = renaming?renaming:[](const SyntaxTree& x) { return x; };

            const auto* it = &var;
            while(true)
            {
                auto find_it = rename.find(*it);
                if(find_it != rename.end())
                    return apply(find_it->second);

                if(it->isnode("slotbase"))
                {
                    if(prefix.isnode()) return apply(SyntaxTree("slotmember", {prefix, (*it)[0]}));
                    else                return apply(*it);
                }
                else if(it->isnode("slotmember"))
                    apply = [old=apply,member=(*it)[1]](const SyntaxTree& syntax)
                        { return old(SyntaxTree("slotmember", {syntax, member})); };
                else if(it->isnode("slotindex"))
                    apply = [old=apply,index=(*it)[1]](const SyntaxTree& syntax)
                        { return old(SyntaxTree("slotindex", {syntax, index})); };
                else throw string("This shouldn't happen.");

                it = &(*it)[0];
            }
        });
    }
    else if(in.isnode("collect"))
    {
        collidx.push_back(in[0].get_leaf());
        return SyntaxTree("collect", {in[0], in[1], SimplifyIDLConstraint(in[2], lookup, indices, collidx, renaming)});
    }
    else if(in.isnode("if"))
    {
        auto index1 = CollapseIDLIndex(in[0], indices);
        auto index2 = CollapseIDLIndex(in[1], indices);
        return SimplifyIDLConstraint(in[(index1 == index2)?2:3], lookup, indices, collidx, renaming);
    }
    else if(in.isnode("include"))
    {
        unordered_map<string,int> new_indices;
        for(size_t i = 1; i+1 < in.size(); i += 2)
            new_indices[in[i].get_leaf()] = CollapseIDLIndex(in[i+1], indices);

        if(new_indices.empty()) new_indices = indices;

        auto find_it = lookup.find(in[0].get_leaf());
        if(find_it == lookup.end())
            throw string("Included specification \""+in[0].get_leaf()+"\" does not exist.\n");
        return SimplifyIDLConstraint(find_it->second, lookup, new_indices, collidx, renaming);
    }
    else throw string("Constraint formula constraints invalid node type \""+in.get_type()+"\".");
}

SyntaxTree FlattenIDLJunctions(SyntaxTree in)
{
    if(in.isnode("conjunction") || in.isnode("disjunction"))
    {
        vector<SyntaxTree> elements;
        for(size_t i = 0; i < in.size(); i++)
        {
            SyntaxTree el = in[i];

            if(el.isnode(in.get_type()))
            {
                for(size_t j = 0; j < el.size(); j++)
                    elements.push_back(el[j]);
            }
            else elements.push_back(el);
        }

        return (elements.size()==1)?elements.front():SyntaxTree(in.get_type(), elements);
    }
    else if(in.isnode("collect"))
    {
        return SyntaxTree("collect", {in[0], in[1], FlattenIDLJunctions(in[2])});
    }
    else return in;
}

int CollapseIDLIndex(SyntaxTree in, const unordered_map<string,int>& vars)
{
    auto recurs = [&in,&vars](size_t i) { return CollapseIDLIndex(in[i], vars); };
    auto as_int = [&in      ](size_t i) { return atoi(in[i].get_leaf().c_str()); };
    auto lookup = [&in,&vars](size_t i) {
        auto find_it = vars.find(in[i].get_leaf());
        if(find_it == vars.end())
            throw string("Undefined variable \""+in[1].get_leaf()+"\" in index calculations.");
        return find_it->second;
    };
    if(in.isnode("basevar")   && in.size() == 1) return lookup(0);
    if(in.isnode("baseconst") && in.size() == 1) return as_int(0);
    if(in.isnode("addvar")    && in.size() == 2) return recurs(0)+lookup(1);
    if(in.isnode("addconst")  && in.size() == 2) return recurs(0)+as_int(1);
    if(in.isnode("subvar")    && in.size() == 2) return recurs(0)-lookup(1);
    if(in.isnode("subconst")  && in.size() == 2) return recurs(0)-as_int(1);
    throw string("Index calculation constraints invalid node type \""+in.get_type()+"\".");
}

vector<SyntaxTree> NormaliseIDLVar(SyntaxTree in, const unordered_map<string,int>& indices,
                                                  const vector<string>&            collidx)
{
    auto const_syntax = [](int i)->SyntaxTree {
        stringstream sstr; sstr<<i;
        return SyntaxTree("baseconst", {SyntaxTree(sstr.str())});
    };

    vector<SyntaxTree> result;
    if(in.isnode("slotbase")) result.push_back(in);
    else if(in.isnode("slottuple"))
    {
        for(size_t i = 0; i < in.size(); i++)
            for(auto tmp : NormaliseIDLVar(in[i], indices, collidx))
                result.push_back(tmp);
    }
    else if(in.isnode("slotmember"))
    {
        for(auto tmp : NormaliseIDLVar(in[0], indices, collidx))
            result.push_back({"slotmember", {tmp, in[1]}});
    }
    else if(in.isnode("slotindex"))
    {
        bool iscollidx = in[1].isnode("basevar") && !collidx.empty() && collidx.back() == in[1][0].get_leaf();
        for(auto tmp : NormaliseIDLVar(in[0], indices, {collidx.begin(), collidx.end()-(iscollidx?1:0)}))
            result.push_back({"slotindex", {tmp, iscollidx?in[1]:const_syntax(CollapseIDLIndex(in[1], indices))}});
    }
    else if(in.isnode("slotrange"))
    {
        int begin = CollapseIDLIndex(in[1], indices);
        int end   = CollapseIDLIndex(in[2], indices);
        for(int index = begin; index < end; index++)
            for(auto tmp : NormaliseIDLVar(in[0], indices, collidx))
                result.push_back({"slotindex", {tmp, const_syntax(index)}});
    }
    else throw string("Variable slot constraints invalid node type \""+in.get_type()+"\".");
    return result;
}

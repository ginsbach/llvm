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
#include <deque>

using namespace std;

/* This program receives two arguments.
   The first argument is the filename of a grammar specification in Backus-Naur
   form. The second argument is the filename of a program adhering to that
   specification.
   The output of this program is then a rendering of the parsed input from the
   second input file.
*/

// This class implements a language-independent syntax tree. As a conscious
// design decision, the "leaf_value" field is _not_ reused to store the
// value returned by get_type(), although this might seem convenient at first.
class SyntaxTree
{
public:
    SyntaxTree(string l) : leaf_value(l) { }
    SyntaxTree(string l, vector<SyntaxTree> n);

    bool operator!=(const SyntaxTree& t) const;

    bool isleaf(const string& s={}) const { return !leaf_value.empty() && (s.empty() || leaf_value == s); }
    bool isnode(const string& s={}) const { return !node_value.empty() && (s.empty() || get_type() == s); }
    const string&     get_leaf  ()                   const { return leaf_value; }
    const string&     get_type  ()                   const { return node_value[0].get_leaf(); }
    size_t            size      ()                   const { return node_value.empty()?0:(node_value.size()-1); }
    const SyntaxTree& operator[](size_t i)           const { return node_value[i+1]; }

private:
    string             leaf_value;
    vector<SyntaxTree> node_value;

    friend ostream& operator<<(ostream& ostr, const vector<SyntaxTree>& x);
};

// This function implements a generic parser using a Backus-Naur Form language
// specification that is provided as a string.
static vector<SyntaxTree> Parse(string bnf_specification, string program_code);

// This function extract IDL-specific constructs from the generic parsed code
// that is provided as SyntaxTree instances.
vector<SyntaxTree> ProcessIDLSpecs(const vector<SyntaxTree>& in_vector);

// This formats the IDL specifications for further processing and codegen.
ostream& operator<<(ostream& ostr, const vector<SyntaxTree>& x);

double stopped_time;
// The program is straightforward: The program is language specification and
// program code are read, the IDL is parsed, and specifications are extracted
// and output to cout.
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

        stopped_time = 0.0;
        auto parse_result   = Parse(read_whole_file(argv[1]), read_whole_file(argv[2]));
        auto exported_specs = ProcessIDLSpecs(parse_result);

        cerr<<"Stopped seconds: "<<stopped_time<<"\n";

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

// This class stores one possible state for the non-deterministic parser below.
class ParseBranch : public SyntaxTree
{
public:
    using PotentialBranch = shared_ptr<ParseBranch>;
    using TransitionRule  = function<PotentialBranch(const ParseBranch&)>;
    using RetentionRule   = function<bool(const ParseBranch&)>;

    ParseBranch(SyntaxTree h, shared_ptr<ParseBranch> t) : SyntaxTree(h), tail(t) { }

    PotentialBranch literal_rule      (const string& n, const string& match) const;
    PotentialBranch reference_rule    (const string& n, const string& match) const;
    PotentialBranch string_rule       (const string& n) const;
    PotentialBranch number_rule       (const string& n) const;
    PotentialBranch add_literal_rule  (const string& n, const unordered_set<string>& prev, const string& match) const;
    PotentialBranch add_reference_rule(const string& n, const unordered_set<string>& prev, const string& match) const;
    PotentialBranch add_string_rule   (const string& n, const unordered_set<string>& prev) const;
    PotentialBranch add_number_rule   (const string& n, const unordered_set<string>& prev) const;

    bool operator==(const ParseBranch& o) const;

    operator vector<SyntaxTree>() &&;

    shared_ptr<ParseBranch> tail;
};

// This class stores the non-deterministic transition rules that are extracted
// from the Backus-Naur Form langauge specification in the single constructor.
class GrammarRules
{
public:
    GrammarRules(string bnf_specification);

    vector<ParseBranch::TransitionRule> transition_rules;
    ParseBranch::RetentionRule          retention_rule;
};

// This is the actual parser. However, much of the heavy lifting has been
// offloaded to the GrammarRules and ParseBranch classes.
vector<SyntaxTree> Parse(string bnf_specification, string program_code)
{
    bool in_comment = false;
    vector<string> tokens{{}};
    for(auto c : program_code)
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
            if(tokens.back().empty()) tokens.back().push_back(c);
            else tokens.push_back({c});
            tokens.push_back({});
        }
        else if(!isspace(c)) tokens.back().push_back(c);
    }
    if(tokens.back().empty()) tokens.pop_back();    

    auto grammar_rules = GrammarRules(bnf_specification);

    vector<shared_ptr<ParseBranch>> syntax_branches{nullptr};
    for(auto& token : tokens)
    {
        for(size_t i = 0; i < syntax_branches.size(); i++)
            syntax_branches[i] = make_shared<ParseBranch>(ParseBranch{SyntaxTree(token), syntax_branches[i]});

        vector<shared_ptr<ParseBranch>> next_branch_workset;
        vector<shared_ptr<ParseBranch>> branch_workset = move(syntax_branches);
        syntax_branches.clear();

        while(!branch_workset.empty())
        {
            for(auto& branch : branch_workset)
            {
                auto tmp1 = clock();

                for(const auto& rule : grammar_rules.transition_rules)
                    if(auto result = rule(*branch))
                        next_branch_workset.push_back(result);

                auto tmp2 = clock();
                stopped_time += (double)(tmp2-tmp1)/(double)CLOCKS_PER_SEC;

                if(grammar_rules.retention_rule(*branch))
                {
                    bool previous = false;
                    for(const auto& prev_branch : syntax_branches)
                    {
                        if(*branch == *prev_branch){ previous = true; break; }
                    }
                    if(!previous) syntax_branches.push_back(branch);
                }
            }

            branch_workset = move(next_branch_workset);
            next_branch_workset.clear();
        }
        if(syntax_branches.empty()) break;
    }

    if(syntax_branches.size() == 1) return move(*syntax_branches.front());
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
bool SyntaxTree::operator!=(const SyntaxTree& t) const {
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

struct BNFSyntax
{
    static vector<pair<string,BNFSyntax>> Parse(string input);

    static BNFSyntax Special  (string s)                         { return BNFSyntax{ s, {}, {}, {}, {}, {}, {}, {}}; }
    static BNFSyntax Literal  (string l)                         { return BNFSyntax{{},  l, {}, {}, {}, {}, {}, {}}; }
    static BNFSyntax Reference(string r)                         { return BNFSyntax{{}, {},  r, {}, {}, {}, {}, {}}; }
    static BNFSyntax Sequence (vector<BNFSyntax> s)              { return BNFSyntax{{}, {}, {},  s, {}, {}, {}, {}}; }
    static BNFSyntax Choice   (vector<BNFSyntax> c)              { return BNFSyntax{{}, {}, {}, {},  c, {}, {}, {}}; }
    static BNFSyntax Optional (vector<BNFSyntax> o)              { return BNFSyntax{{}, {}, {}, {}, {},  o, {}, {}}; }
    static BNFSyntax Repeat   (vector<BNFSyntax> r)              { return BNFSyntax{{}, {}, {}, {}, {}, {},  r, {}}; }
    static BNFSyntax Bound    (vector<pair<string,BNFSyntax>> v) { return BNFSyntax{{}, {}, {}, {}, {}, {}, {},  v}; }

    static BNFSyntax RefChoice(vector<string> v)
    {
        BNFSyntax res;
        for(auto s : v) res.choice.push_back(Reference(s));
        if(res.choice.size() == 1) return res.choice.front();
        else                       return res;
    }

    bool operator==(const string& s) const { return s == special || s == "\'"+literal+"\'" || s == "<"+reference+">"; }
    bool isnormal()                  const { return special.empty(); }
    bool isliteral()                 const { return !literal.empty(); }
    bool isreference()               const { return !reference.empty(); }
    bool issequence()                const { return !sequence.empty(); }
    bool ischoice()                  const { return !choice.empty(); }
    bool isoptional()                const { return !optional.empty(); }
    bool isrepeat()                  const { return !repeat.empty(); }
    bool isbound()                   const { return !bound.empty(); }

    string                         special;
    string                         literal;
    string                         reference;
    vector<BNFSyntax>              sequence;
    vector<BNFSyntax>              choice;
    vector<BNFSyntax>              optional;
    vector<BNFSyntax>              repeat;
    vector<pair<string,BNFSyntax>> bound;
};

// This is a workaround to allow building without enabling C++20 in the compiler.
template<typename T> bool contains(const unordered_set<T>& S, const T& E) { return S.find(E) != S.end(); }

GrammarRules::GrammarRules(string bnf_specification)
{
    auto stack = BNFSyntax::Parse(bnf_specification);

    auto get_temp = [](string in, int i)->string{ stringstream sstr; sstr<<in<<"@"<<i; return sstr.str(); };

    unordered_set<string> types_with_ref_continue;
    unordered_set<string> types_with_unref_continue;
    vector<function<shared_ptr<ParseBranch>(const ParseBranch&)>> rules;

    for(size_t j = 0; j < stack.size(); j++) {
        if(!stack[j].first.empty())
        {
            auto l = move(stack[j].first);
            auto r = move(stack[j].second);

            if(r.isliteral())
                rules.push_back([=](const ParseBranch& branch) { return branch.literal_rule(l, r.literal); });
            else if(r == "<s>")
                rules.push_back([=](const ParseBranch& branch) { return branch.string_rule (l); });
            else if(r == "<n>")
                rules.push_back([=](const ParseBranch& branch) { return branch.number_rule (l); });
            else if(r.isreference())
                rules.push_back([=](const ParseBranch& branch) { return branch.reference_rule(l, r.reference); });
            else if(r.ischoice())    for(auto& choice : r.choice) stack.push_back({l, choice});
            else if(r.issequence())
            {
                vector<string> old_tmps{get_temp(l, 0)};

                if(r.sequence[0].isliteral())
                    rules.push_back([=](const ParseBranch& branch) { return branch.literal_rule(old_tmps.front(), r.sequence[0].literal); });
                else if(r.sequence[0].isreference())
                    old_tmps.front() = r.sequence[0].reference;
                else if(r.sequence[0].ischoice())
                {
                    for(auto& choice : r.sequence[0].choice)
                        if(choice.isreference()) old_tmps.push_back(choice.reference);
                        else                     stack.push_back({old_tmps.front(), choice});
                }
                else throw string("Syntax Error in BNF file: Sequence rule has invalid initial part.");

                for(size_t i = 1; i < r.sequence.size(); i++)
                {
                    vector<string> new_tmps;

                    if(r.sequence[i].ischoice())
                    {
                        string tmp = (i+1 < r.sequence.size())?get_temp(l, i):l;
                        new_tmps.assign({tmp});

                        for(auto& choice : r.choice)
                        {
                            if(!choice.issequence()) choice = BNFSyntax::Sequence({choice});
                            choice.sequence.insert(choice.sequence.begin(), BNFSyntax::RefChoice(old_tmps));
                            stack.push_back({tmp, move(choice)});
                        }
                    }
                    else if(r.sequence[i].isoptional() || r.sequence[i].isrepeat())
                    {
                        string tmp = (i+1 < r.sequence.size())?get_temp(l, i):l;
                        new_tmps.assign({tmp});

                        if(r.sequence[i].isrepeat()) {
                            old_tmps.push_back(tmp);
                        }

                        auto news2 = BNFSyntax::Sequence({BNFSyntax::RefChoice(old_tmps)});

                        if(r.sequence[i].isoptional())
                            news2.sequence.insert(news2.sequence.end(), r.sequence[i].optional.begin(), r.sequence[i].optional.end());

                        if(r.sequence[i].isrepeat())
                            news2.sequence.insert(news2.sequence.end(), r.sequence[i].repeat.begin(), r.sequence[i].repeat.end());

                        stack.push_back({tmp, news2});

                        if(r.sequence[i].isoptional())
                            new_tmps.insert(new_tmps.end(), old_tmps.begin(), old_tmps.end());
                        else new_tmps = old_tmps;
                    }
                    else
                    {
                        string tmp = (i+1 < r.sequence.size())?get_temp(l, i):l;
                        new_tmps.assign({tmp});

                        if(r.sequence[i].isliteral()) {
                            rules.push_back([=](const ParseBranch& branch) { return branch.add_literal_rule(tmp, {old_tmps.begin(), old_tmps.end()}, r.sequence[i].literal); });
                            types_with_unref_continue.insert(old_tmps.begin(), old_tmps.end());
                        }
                        else if(r.sequence[i] == "<s>") {
                            rules.push_back([=](const ParseBranch& branch) { return branch.add_string_rule(tmp, {old_tmps.begin(), old_tmps.end()}); });
                            types_with_unref_continue.insert(old_tmps.begin(), old_tmps.end());
                        }
                        else if(r.sequence[i] == "<n>") {
                            rules.push_back([=](const ParseBranch& branch) { return branch.add_number_rule(tmp, {old_tmps.begin(), old_tmps.end()}); });
                            types_with_unref_continue.insert(old_tmps.begin(), old_tmps.end());
                        }
                        else if(r.sequence[i].isreference()) {
                            rules.push_back([=](const ParseBranch& branch) { return branch.add_reference_rule(tmp, {old_tmps.begin(), old_tmps.end()}, r.sequence[i].reference); });
                            types_with_ref_continue.insert(old_tmps.begin(), old_tmps.end());
                        }
                        else throw string("Failed to process BNF file, some sequence rule cannot be dissected fully.");
                    }

                    old_tmps = new_tmps;
                }
                for(auto old_tmp : old_tmps)
                    if(old_tmp != l)
                        rules.push_back([=](const ParseBranch& branch) { return branch.reference_rule(l, old_tmp); });
            }
            else throw string("Syntax Error in BNF file: No decomposition to break down complex rule.");
        }
    }

    swap(transition_rules, rules);

    retention_rule = [types_with_ref_continue,types_with_unref_continue](const ParseBranch& branch) {
        if(branch.tail != nullptr && !branch.tail->isnode("#") && (!branch.tail->isnode() ||
           !contains(types_with_ref_continue, branch.tail->get_type()))) return false;
        if(branch.isnode("#")) return true;
        if(!branch.isnode())   return false;

        return contains(types_with_ref_continue, branch.get_type()) ||
               contains(types_with_unref_continue, branch.get_type());
    };
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

shared_ptr<ParseBranch> ParseBranch::literal_rule(const string& n, const string& match) const
{
    if(isleaf(match))
        return make_shared<ParseBranch>(SyntaxTree(n, {}), tail);
    else return nullptr;
}

shared_ptr<ParseBranch> ParseBranch::reference_rule(const string& n, const string& match) const
{
    if(isnode(match))
        return make_shared<ParseBranch>(SyntaxTree(n, {*this}), tail);
    else return nullptr;
}

shared_ptr<ParseBranch> ParseBranch::string_rule(const string& n) const
{
    if(isleaf() && !isdigit(get_leaf()[0]))
        return make_shared<ParseBranch>(SyntaxTree(n, {*this}), tail);
    else return nullptr;
}

shared_ptr<ParseBranch> ParseBranch::number_rule(const string& n) const
{
    if(isleaf() && isdigit(get_leaf()[0]))
        return make_shared<ParseBranch>(SyntaxTree(n, {*this}), tail);
    else return nullptr;
}

shared_ptr<ParseBranch> ParseBranch::add_literal_rule(const string& n, const unordered_set<string>& prev, const string& match) const
{
    if(tail && tail->isnode() && contains(prev, tail->get_type()) && isleaf(match))
        return make_shared<ParseBranch>(SyntaxTree(n, {*tail}), tail->tail);
    else return nullptr;
}

shared_ptr<ParseBranch> ParseBranch::add_reference_rule(const string& n, const unordered_set<string>& prev, const string& match) const
{
    if(tail && tail->isnode() && contains(prev, tail->get_type()) && isnode(match))
        return make_shared<ParseBranch>(SyntaxTree(n, {*tail, *this}), tail->tail);
    else return nullptr;
}

shared_ptr<ParseBranch> ParseBranch::add_string_rule(const string& n, const unordered_set<string>& prev) const
{
    if(tail && tail->isnode() && contains(prev, tail->get_type()) && isleaf() && !isdigit(get_leaf()[0]))
        return make_shared<ParseBranch>(SyntaxTree(n, {*tail, *this}), tail->tail);
    else return nullptr;
}

shared_ptr<ParseBranch> ParseBranch::add_number_rule(const string& n, const unordered_set<string>& prev) const
{
    if(tail && tail->isnode() && contains(prev, tail->get_type()) && isleaf() && isdigit(get_leaf()[0]))
        return make_shared<ParseBranch>(SyntaxTree(n, {*tail, *this}), tail->tail);
    else return nullptr;
}

// This function parses the Backus-Naur Form language specification. As opposed
// the actual parser that the resulting grammar is informing, this is hardcoded.
vector<pair<string,BNFSyntax>> BNFSyntax::Parse(string input)
{
    vector<string> tokens{""};
    for(auto c : input)
    {
        if(isspace(c) && !tokens.back().empty()) tokens.push_back("");
        if(!isspace(c)) tokens.back().push_back(c);
    }
    if(tokens.back().empty()) tokens.pop_back();
    tokens.push_back(")");
    tokens.push_back(")");

    vector<BNFSyntax> stack;
    for(const auto& t : tokens)
    {
        if(t == "::=" || t == "|" || t == "[" || t == "]" || t == "{" || t == "}" || t == "(" || t == ")" || t == "!")
            stack.push_back(BNFSyntax::Special(t));
        else if(t.front() == '<' && t.back() == '>')
            stack.push_back(BNFSyntax::Reference(string(t.begin()+1, t.end()-1)));
        else if(t.front() == '\'' && t.back() == '\'' && t.size() >= 2)
            stack.push_back(BNFSyntax::Literal(string(t.begin()+1, t.end()-1)));
        else stack.push_back(BNFSyntax::Literal(t));

        while(true)
        {
            auto get   = [&stack](size_t n)->BNFSyntax&{ return stack.rbegin()[n]; };
            auto merge = [&stack](size_t n)            { stack.erase(stack.end()-n-1, stack.end()-2); };
            if(stack.size() >= 4 && get(3).isnormal() && get(2).isnormal() && !(get(1) == "::="))
            {
                if(get(3).issequence()) get(3).sequence.push_back(get(2));
                else                    get(3) = BNFSyntax::Sequence({move(get(3)), move(get(2))});
                merge(2);
            }
            else if(stack.size() >= 5 && get(4).isnormal() && get(3) == "|" && get(2).isnormal() &&
                   (get(1) == "|" || get(1) == ")" || get(1) == "]" || get(1) == "}" || get(0) == "::="))
            {
                if(get(4).ischoice()) get(4).choice.push_back(get(2));
                else                  get(4) = BNFSyntax::Choice({get(4), get(2)});
                merge(3);
            }
            else if(stack.size() >= 5 && get(4) == "(" && get(3).isnormal() && get(2) == ")")
            {
                get(4) = move(get(3));
                merge(3);
            }
            else if(stack.size() >= 5 && get(4) == "[" && get(3).isnormal() && get(2) == "]")
            {
                if(get(3).issequence()) get(4) = BNFSyntax::Optional(move(get(3).sequence));
                else                    get(4) = BNFSyntax::Optional({move(get(3))});
                merge(3);
            }
            else if(stack.size() >= 5 && get(4) == "{" && get(3).isnormal() && get(2) == "}")
            {
                if(get(3).issequence()) get(4) = BNFSyntax::Repeat(move(get(3).sequence));
                else                    get(4) = BNFSyntax::Repeat({move(get(3))});
                merge(3);
            }
            else if(stack.size() >= 6 && get(4).isliteral() && get(3) == "::=" && get(2).isnormal() &&
                   ((get(1) == ")" && get(0) == ")") || (get(1).isliteral() && get(0) == "::=")) && get(5).isbound())
            {
                get(5).bound.push_back({move(get(4).literal), move(get(2))});
                merge(4);

            }
            else if(stack.size() >= 5 && get(4).isliteral() && get(3) == "::=" && get(2).isnormal() &&
                   ((get(1) == ")" && get(0) == ")") || (get(1).isliteral() && get(0) == "::=")))
            {
                get(4) = BNFSyntax::Bound({{move(get(4).literal), move(get(2))}});
                merge(3);
            }
            else if(stack.size() == 3 && get(2).isbound() && get(1) == ")" && get(0) == ")")
                return get(2).bound;
            else break;
        }
    }

    string error_msg = "Failed to parse BNF file, as not all syntax collapsed to transformation rules.\n    [";
    for(const auto& s: stack)
    {
        if(!s.isnormal() || s.isliteral() || s.isreference())
            error_msg += "\""+s.special+s.literal+s.reference+"\", ";
        else if(s.ischoice())   error_msg += "<choice>, ";
        else if(s.isoptional()) error_msg += "<optional>, ";
        else if(s.isrepeat())   error_msg += "<repeat>, ";
        else                    error_msg += "<"+string(s.bound.size(), '.')+">";
    }
    error_msg.resize(error_msg.size()-2);
    throw string(error_msg+"]");
}

/* This part is IDL specific */


SyntaxTree extract_constraint(SyntaxTree in, const unordered_map<string,SyntaxTree>& lookup,
                              unordered_map<string,int> varlookup={}, unordered_set<string> collectvars = {},
                              function<SyntaxTree(const SyntaxTree&)> renaming = nullptr);

vector<SyntaxTree> ProcessIDLSpecs(const vector<SyntaxTree>& in_vector)
{
    unordered_map<string,SyntaxTree> lookup;
    for(const SyntaxTree& in : in_vector)
        if(in.isnode("specification") && in.size() == 2)
            lookup.insert({in[0].get_leaf(), in[1]});

    vector<SyntaxTree> result;
    for(size_t i = 0; i+1 < in_vector.size(); i++) {
        const SyntaxTree& in = in_vector[i];
        const SyntaxTree& next = in_vector[i+1];
        if(next.isnode("export") && in.isnode("specification"))
        {
            SyntaxTree tree(SyntaxTree{"specification", {in[0], extract_constraint(in[1], lookup)}});
            result.push_back(tree);
        }
    }
    return result;
}

int extract_index(SyntaxTree in, const unordered_map<string,int>& vars)
{
    auto recurs = [&in,&vars](size_t i) { return extract_index(in[i], vars); };
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

vector<SyntaxTree> extract_var(SyntaxTree in, const unordered_map<string,int>& vars,
                                                     const unordered_set<string>& collectvars)
{
    auto int_to_syntax = [](int i)->SyntaxTree {
        stringstream sstr; sstr<<i;
        return SyntaxTree("baseconst", {SyntaxTree(sstr.str())});
    };

    vector<SyntaxTree> result;
    if(in.isnode("slotbase")) result.push_back(in);
    else if(in.isnode("slottuple"))
    {
        for(size_t i = 0; i < in.size(); i++)
            for(auto tmp : extract_var(in[i], vars, collectvars))
                result.push_back(tmp);
    }
    else if(in.isnode("slotmember"))
    {
        for(auto tmp : extract_var(in[0], vars, collectvars))
            result.push_back(SyntaxTree("slotmember", {tmp, in[1]}));
    }
    else if(in.isnode("slotindex"))
    {
        if(in[1].isnode("basevar"))
        {
            auto find_it = collectvars.find(in[1][0].get_leaf());
            if(find_it != collectvars.end())
            {
                for(auto tmp : extract_var(in[0], vars, collectvars))
                    result.push_back(SyntaxTree("slotindex", {tmp, in[1]}));
            }
            else
            {
                for(auto tmp : extract_var(in[0], vars, collectvars))
                    result.push_back(SyntaxTree("slotindex", {tmp, int_to_syntax(extract_index(in[1], vars))}));
            }
        }
        else
        {
            for(auto tmp : extract_var(in[0], vars, collectvars))
                result.push_back(SyntaxTree("slotindex", {tmp, int_to_syntax(extract_index(in[1], vars))}));
        }
    }
    else if(in.isnode("slotrange"))
    {
        int begin = extract_index(in[1], vars);
        int end   = extract_index(in[2], vars);
        for(int index = begin; index < end; index++)
            for(auto tmp : extract_var(in[0], vars, collectvars))
                result.push_back(SyntaxTree("slotindex", {tmp, int_to_syntax(index)}));
    }
    else throw string("Variable slot constraints invalid node type \""+in.get_type()+"\".");
    return result;
}

SyntaxTree extract_constraint(SyntaxTree in, const unordered_map<string,SyntaxTree>& lookup,
                              unordered_map<string,int> varlookup, unordered_set<string> collectvars,
                              function<SyntaxTree(const SyntaxTree&)> renaming)
{
    if(in.isnode("atom"))
    {
        vector<SyntaxTree> processed_vars;
        auto constraint = in[0];
        for(size_t i = 0; i < constraint.size(); i++)
        {
            if(SyntaxTree(constraint[i]).isleaf())
            {
                processed_vars.push_back(SyntaxTree(SyntaxTree(constraint[i]).get_leaf()));
                continue;
            }

            vector<SyntaxTree> vars;
            for(auto tmp : extract_var(constraint[i], varlookup, collectvars))
                vars.push_back(renaming?renaming(tmp):tmp);

            if(vars.size() == 1)
                 processed_vars.push_back(vars.front());
            else
                processed_vars.push_back(SyntaxTree("slottuple", vars));
        }
        in = SyntaxTree("atom", {SyntaxTree(in[0].get_type(), processed_vars)});
    }
    else if(in.isnode("conjunction") || in.isnode("disjunction"))
    {
        vector<SyntaxTree> elements;
        for(size_t i = 0; i < in.size(); i++)
        {
            SyntaxTree el = extract_constraint(in[i], lookup, varlookup, collectvars, renaming);

            if(el.isnode(in.get_type()))
            {
                for(size_t j = 0; j < el.size(); j++)
                    elements.push_back(el[j]);
            }
            else elements.push_back(el);
        }

        in = (elements.size()==1)?elements.front():SyntaxTree(in.get_type(), elements);
    }
    else if(in.isnode("conRange") || in.isnode("disRange"))
    {
        auto begin = extract_index(in[2], varlookup);
        auto end   = in.isnode("for")?(begin+1):extract_index(in[3], varlookup);

        string type = in.get_type();
        type.resize(3);
        type += "junction";

        vector<SyntaxTree> elements;
        for(int index = begin; index < end; index++)
        {
            varlookup[in[1].get_leaf()] = index;
            SyntaxTree el = extract_constraint(in[0], lookup, varlookup, collectvars, renaming);

            if(el.isnode(type))
            {
                for(size_t j = 0; j < el.size(); j++)
                    elements.push_back(el[j]);
            }
            else elements.push_back(el);
        }

        in = (elements.size()==1)?elements.front():SyntaxTree(type, elements);
    }
    else if(in.isnode("default" )|| in.isnode("for"))
    {
        auto index = extract_index(in[2], varlookup);

        auto find_it = in.isnode("for")?varlookup.end():varlookup.find(in[1].get_leaf());
        if(find_it == varlookup.end())
        {
            varlookup[in[1].get_leaf()] = index;
        }

        in = extract_constraint(in[0], lookup, varlookup, collectvars, renaming);
    }
    else if(in.isnode("rename"))
    {
        auto print_var_recurs = [](const SyntaxTree& s, const auto& rec)->string
        {
            if(s.isnode("slotbase"))                                   return s[0].get_leaf();
            else if(s.isnode("slotmember"))                            return rec(s[0], rec)+"."+s[1].get_leaf();
            else if(s.isnode("slotindex") && s[1].isnode("basevar"))   return rec(s[0], rec)+"["+s[1][0].get_leaf()+"]";
            else if(s.isnode("slotindex") && s[1].isnode("baseconst")) return rec(s[0], rec)+"["+s[1][0].get_leaf()+"]";
            else throw string("Attempting to rename a variable that is not normalised \""+s.get_type()+"\".");
        };

        auto print_var = [=](const SyntaxTree& s)->string{ return print_var_recurs(s, print_var_recurs); };

        unordered_map<string,SyntaxTree> rename;
        SyntaxTree                       prefix("");
        for(size_t i = 1; i+1 < in.size(); i+=2)
        {
            auto tmp1 = extract_var(in[i+1], varlookup, collectvars);
            auto tmp2 = extract_var(in[i],   varlookup, collectvars);
            if(tmp1.size() == tmp2.size())
            {
                for(size_t j = 0; j < tmp1.size(); j++)
                    rename.insert({print_var(tmp1[j]), tmp2[j]});
            }
            else throw string("Rename of touples with unequal sizes is invalid.");
        }
        if(0 == (in.size() % 2))
        {
            auto tmp1 = extract_var(in[in.size()-1], varlookup, collectvars);
            if(tmp1.size() == 1)
                prefix = tmp1.front();
            else throw string("Cannot prefix a tuple.");
        }

        in = extract_constraint(in[0], lookup, varlookup, collectvars,
                                [renaming,&rename,&prefix,print_var](const SyntaxTree& var)
        {
            string printed = "."+print_var(var);
            size_t len     =  printed.size();

            SyntaxTree new_var = prefix;

            while(len > 0)
            {
                auto find_it = rename.find(string(printed.begin()+1, printed.begin()+len));
                if(find_it != rename.end())
                {
                    new_var = find_it->second;
                    break;
                }

                do len--;
                while(printed[len] != '[' && printed[len] != '.');
            }

            if(!new_var.isnode())
                return renaming?renaming(var):SyntaxTree(var);

            while(len < printed.size())
            {
                size_t next_len = len+1;
                if(printed[len] == '[')
                {
                    while(next_len < printed.size() && printed[next_len] != ']') next_len++;
                    string new_str(printed.begin()+len+1, printed.begin()+next_len);

                    if(new_str.empty() || !isdigit(new_str[0]))
                        new_var = SyntaxTree("slotindex", {new_var, SyntaxTree("basevar", {SyntaxTree(new_str)})});
                    else
                        new_var = SyntaxTree("slotindex", {new_var, SyntaxTree("baseconst", {SyntaxTree(new_str)})});

                    len = (next_len < printed.size())?(next_len+1):next_len;
                }
                else if(printed[len] == '.')
                {
                    while(next_len < printed.size() && printed[next_len] != '[' && printed[next_len] != '.') next_len++;
                    string new_str(printed.begin()+len+1, printed.begin()+next_len);

                    new_var = SyntaxTree("slotmember", {new_var, SyntaxTree(new_str)});

                    len = next_len;
                }
                else throw string("Impossible variable.");
            }

            return renaming?renaming(new_var):new_var;
        });
    }
    else if(in.isnode("collect"))
    {
        collectvars.insert(in[0].get_leaf());
        in = SyntaxTree("collect", {in[0], in[1],
                                           extract_constraint(in[2], lookup, varlookup, collectvars, renaming)});
    }
    else if(in.isnode("if"))
    {
        auto index1 = extract_index(in[0], varlookup);
        auto index2 = extract_index(in[1], varlookup);
        in = extract_constraint(in[(index1 == index2)?2:3], lookup, varlookup, collectvars, renaming);
    }
    else if(in.isnode("include"))
    {
        unordered_map<string,int> new_varlookup;
        for(size_t i = 1; i+1 < in.size(); i+=2)
            new_varlookup[in[i].get_leaf()] = extract_index(in[i+1], varlookup);

        if(new_varlookup.empty()) new_varlookup = varlookup;

        auto find_it = lookup.find(in[0].get_leaf());
        if(find_it == lookup.end())
            throw string("Included specification \""+in[0].get_leaf()+"\" does not exist.\n");
        in = extract_constraint(find_it->second, lookup, new_varlookup, collectvars, renaming);
    }
    else throw string("Constraint formula constraints invalid node type \""+in.get_type()+"\".");
    return in;
}

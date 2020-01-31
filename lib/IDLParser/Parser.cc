// This program receives two filenames as command line arguments.
// The first argument is the filename of a grammar specification in Backus-Naur
// form. The second argument is the filename of a program adhering to that
// specification.
// The output of this program is a rendering of the parsed input from the second
// input file. 

double stopped_time;

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

// This class implements a language-independent syntax tree. As a conscious
// design decision, the "leaf_value" field is _not_ reused to store the
// value returned by get_type(), although this might seem convenient at first.
class SyntaxTree
{
public:
    SyntaxTree(string l) : leaf_value(l) { }
    SyntaxTree(string l, vector<SyntaxTree> n);

    bool operator!=(const SyntaxTree& t) const;

    bool               isleaf()                const { return node_value.empty(); }
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

// This function implements a generic parser using a Backus-Naur form language
// specification that is provided as a string.
static vector<SyntaxTree> Parse(string bnf_specification, string program_code);

// This function extract IDL-specific constructs from the generic parsed code
// that is provided as SyntaxTree instances.
vector<SyntaxTree> ProcessIDLSpecs(const vector<SyntaxTree>& in_vector);

// This formats the IDL specifications for further processing and codegen.
ostream& operator<<(ostream& ostr, const vector<SyntaxTree>& x);

// The main function is straightforward:
// First, the program is language specification and program code are read.
// Second the code is parsed according to the langauge specification.
// Finally, the iDL specifications are extracted from the parsed syntax and
// output to the standard output stream.
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
// from the Backus-Naur form langauge specification in the single constructor.
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
    vector<ParseBranch>             branch_workset;
    vector<ParseBranch>             next_branch_workset;

    for(auto& token : tokens)
    {
        branch_workset.clear();
        for(auto& branch : syntax_branches)
            branch_workset.push_back(ParseBranch(SyntaxTree(token), move(branch)));

        syntax_branches.clear();
        while(!branch_workset.empty())
        {
            next_branch_workset.clear();
            for(auto& branch : branch_workset)
            {
                auto tmp1 = clock();

                for(const auto& rule : grammar_rules.transition_rules)
                    if(auto result = rule(branch))
                        next_branch_workset.push_back(*result);

                auto tmp2 = clock();
                stopped_time += (double)(tmp2-tmp1)/(double)CLOCKS_PER_SEC;

                if(grammar_rules.retention_rule(branch) && !any_of(syntax_branches.begin(), syntax_branches.end(),
                                                                   [&branch](const auto& b) { return *b == branch; }))
                    syntax_branches.push_back(shared_ptr<ParseBranch>(new ParseBranch(move(branch))));
            }

            swap(branch_workset, next_branch_workset);
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

// This function parses the Backus-Naur form language specification.
vector<pair<string,SyntaxTree>> ParseBNF(string bnf_specification);

// This is a workaround to allow building without enabling C++20 in the compiler.
template<typename T> bool contains(const unordered_set<T>& S, const T& E) { return S.find(E) != S.end(); }

GrammarRules::GrammarRules(string bnf_specification)
{
    auto stack = ParseBNF(bnf_specification);

    auto get_temp = [](string in, int i)->string{ stringstream sstr; sstr<<in<<"@"<<i; return sstr.str(); };

    unordered_set<string> types_with_ref_continue;
    unordered_set<string> types_with_unref_continue;
    vector<function<shared_ptr<ParseBranch>(const ParseBranch&)>> rules;

    while(!stack.empty())
    {
        auto l = move(stack.back().first);
        auto r = move(stack.back().second);
        stack.pop_back();

        if(r.isnode("literal"))
            rules.push_back([l,s=r[0].get_leaf()](const ParseBranch& b) { return b.literal_rule(l, s); });
        else if(r.isnode("reference") && r[0].isleaf("s"))
            rules.push_back([l](const ParseBranch& b) { return b.string_rule (l); });
        else if(r.isnode("reference") && r[0].isleaf("n"))
            rules.push_back([l](const ParseBranch& b) { return b.number_rule (l); });
        else if(r.isnode("reference"))
            rules.push_back([l,s=r[0].get_leaf()](const ParseBranch& b) { return b.reference_rule(l, s); });
        else if(r.isnode("choice"))
            for(auto& choice : move(r).children()) stack.push_back({l, choice});
        else if(r.isnode("sequence"))
        {
            unordered_set<string> old_tmps;
            string                tmp = get_temp(l, 0);

            auto tail = move(r).children();
            auto head = move(tail.front());
            tail.erase(tail.begin());

            if(head.isnode("literal"))
                rules.push_back([l=tmp,s=head[0].get_leaf()](const ParseBranch& b) { return b.literal_rule(l, s); });
            else if(head.isnode("reference"))
                tmp = head[0].get_leaf();
            else if(head.isnode("choice"))
            {
                for(auto& choice : move(head).children())
                    if(choice.isnode("reference")) old_tmps.insert(choice[0].get_leaf());
                    else                           stack.push_back({tmp, move(choice)});
            }
            else throw string("Syntax Error in BNF file: Sequence rule has invalid initial part.");

            old_tmps.insert(tmp);
            for(size_t i = 0; i < tail.size(); i++)
            {
                unordered_set<string> new_tmps;

                if(tail[i].isnode("choice"))
                {
                    string tmp = (i+1 < tail.size())?get_temp(l, i+1):l;
                    new_tmps.insert(tmp);

                    for(size_t k = 0; k < tail[i].size(); k++)
                    {
                        vector<SyntaxTree> refchoice_contents;
                        for(const auto& old_tmp : old_tmps)
                            refchoice_contents.push_back(SyntaxTree("reference", {SyntaxTree(old_tmp)}));

                        vector<SyntaxTree> sequence_contents{SyntaxTree("choice", refchoice_contents)};
    
                        if(tail[i][k].isnode("sequence"))
                            for(size_t l = 0; l < tail[i][k].size(); l++)
                                sequence_contents.push_back(tail[i][k][l]);

                        stack.push_back({tmp,SyntaxTree("sequence", sequence_contents)});
                    }
                }
                else if(tail[i].isnode("optional") || tail[i].isnode("repeat"))
                {
                    string tmp = (i+1 < tail.size())?get_temp(l, i+1):l;
                    new_tmps.insert(tmp);

                    if(tail[i].isnode("repeat")) {
                        old_tmps.insert(tmp);
                    }

                    vector<SyntaxTree> refchoice_contents;
                    for(const auto& old_tmp : old_tmps)
                        refchoice_contents.push_back(SyntaxTree("reference", {SyntaxTree(old_tmp)}));

                    vector<SyntaxTree> news2_content{SyntaxTree("choice", refchoice_contents)};

                    for(size_t k = 0; k < tail[i].size(); k++)
                        news2_content.push_back(tail[i][k]);

                    stack.push_back({tmp, SyntaxTree("sequence", news2_content)});

                    if(tail[i].isnode("optional"))
                        new_tmps.insert(old_tmps.begin(), old_tmps.end());
                    else new_tmps = old_tmps;
                }
                else
                {
                    string tmp = (i+1 < tail.size())?get_temp(l, i+1):l;
                    new_tmps.insert(tmp);

                    if(tail[i].isnode("literal")) {
                        rules.push_back([=](const ParseBranch& branch) { return branch.add_literal_rule(tmp, old_tmps, tail[i][0].get_leaf()); });
                        types_with_unref_continue.insert(old_tmps.begin(), old_tmps.end());
                    }
                    else if(tail[i].isnode("reference") && tail[i][0].isleaf("s")) {
                        rules.push_back([=](const ParseBranch& branch) { return branch.add_string_rule(tmp, old_tmps); });
                        types_with_unref_continue.insert(old_tmps.begin(), old_tmps.end());
                    }
                    else if(tail[i].isnode("reference") && tail[i][0].isleaf("n")) {
                        rules.push_back([=](const ParseBranch& branch) { return branch.add_number_rule(tmp, old_tmps); });
                        types_with_unref_continue.insert(old_tmps.begin(), old_tmps.end());
                    }
                    else if(tail[i].isnode("reference")) {
                        rules.push_back([=](const ParseBranch& branch) { return branch.add_reference_rule(tmp, old_tmps, tail[i][0].get_leaf()); });
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

// This gives a way to move out the children without copies.
vector<SyntaxTree> SyntaxTree::children() &&
{
    if(!node_value.empty())
        node_value.erase(node_value.begin());
    return move(node_value);
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

// This function parses the Backus-Naur form language specification. Different to
// the actual parser that the resulting grammar is informing, this is a hardcoded
// deterministic LALR(2) parser.
// The input has to be fairly well structured. In particular, all tokens have to
// be separated by whitespace, as the lexer is very primitive.
vector<pair<string,SyntaxTree>> ParseBNF(string bnf_specification)
{
    vector<string> tokens{""};
    for(auto c : bnf_specification + " ) )")
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
                set(3, get(3).isnode("sequence")?add(3, 2):SyntaxTree("sequence", {move(get(3)), move(get(2))}));
            }
            else if(len(5) && get(4).isnode() && get(3).isleaf("|") && get(2).isnode() && (get(0).isleaf("::=") ||
                    get(1).isleaf("|") || get(1).isleaf(")") || get(1).isleaf("]") || get(1).isleaf("}")))
            {
                set(4, get(4).isnode("choice")?add(4, 2):SyntaxTree("choice", {mov(4), mov(2)}));
            }
            else if(len(5) && get(4).isleaf("(") && get(3).isnode() && get(2).isleaf(")"))
            {
                set(4, mov(3));
            }
            else if(len(5) && get(4).isleaf("[") && get(3).isnode() && get(2).isleaf("]"))
            {
                set(4, SyntaxTree("optional", get(3).isnode("sequence")?mov(3).children():vector<SyntaxTree>{mov(3)}));
            }
            else if(len(5) && get(4).isleaf("{") && get(3).isnode() && get(2).isleaf("}"))
            {
                set(4, SyntaxTree("repeat", get(3).isnode("sequence")?mov(3).children():vector<SyntaxTree>{mov(3)}));
            }
            else if(len(5) && get(4).isnode("literal") && get(3).isleaf("::=") && get(2).isnode() &&
                   ((get(1).isleaf(")") && get(0).isleaf(")")) || (get(1).isnode("literal") && get(0).isleaf("::="))))
            {
                result.push_back({get(4)[0].get_leaf(), mov(2)});
                stack.erase(stack.end()-5, stack.end()-2);
            }
            else if(stack.size() == 2 && get(1).isleaf(")") && get(0).isleaf(")"))
                return result;
            else break;
        }
    }

    stringstream sstr;
    sstr<<"Failed to parse BNF file, as not all syntax collapsed to transformation rules.\n    "<<stack<<"\n";
    throw sstr.str();
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

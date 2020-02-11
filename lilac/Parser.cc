#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>

using namespace std;

struct ProgramPart
{
    vector<string>     header;
    string             main_content;
    map<string,string> extra_content;
};

vector<ProgramPart> SplitCodeParts(string input);

void               GenerateIDLSpecs          (vector<ProgramPart> parts);
void               GenerateHarnessInterfaces (vector<ProgramPart> parts);
void               GenerateNaiveHarnesses    (vector<ProgramPart> parts);
map<string,string> GenerateMarshallingCpp    (vector<ProgramPart> parts);
void               GenerateSpecifiedHarnesses(vector<ProgramPart> parts);

void parse_program(vector<ProgramPart> input);

int main()
{
    try {
        auto iter  = istreambuf_iterator<char>(cin);
        auto parts = SplitCodeParts(string(iter, {}));
        GenerateIDLSpecs           (parts);
        GenerateHarnessInterfaces  (parts);
        GenerateNaiveHarnesses     (parts);
        GenerateSpecifiedHarnesses (parts);
    }
    catch(string error) {
        std::cerr<<"Error: "<<error<<std::endl;
        return 1;
    }
    return 0;
}

vector<string> split_by(string input, char s);
string         indent  (string input, size_t n);

struct LiLACWhat
{
    LiLACWhat(string t, string c): type(t), content{c} {}
    LiLACWhat(string t, vector<LiLACWhat> c): type(t), child{c} {}

    bool operator==(const LiLACWhat& other) const
        { return type == other.type && content == other.content && child == other.child; }

    bool isnode() const { return !child.empty(); }
    bool isleaf() const { return !content.empty(); }
    bool isleaf(const string& c) const { return content == c; }
    bool isnode(const string& t) const { return type == t; }
    const string& get_type() const { return type; }
    const string& get_leaf() const { return content; }
    const LiLACWhat& operator[](size_t i) const { return child[i]; }
    bool isloop() const { return isnode("loop") || isnode("map") || isnode("dot"); }
    const string& get_loop_iter() const { return child[1].get_leaf(); }

private:
    string type;
    string content;
    vector<LiLACWhat> child;
};

LiLACWhat parse_lilacwhat(string str);

vector<pair<string,string>> capture_arguments(const LiLACWhat& what);

string generate_naive_impl(const LiLACWhat& what, string pad="");

string print_harness(string name, vector<pair<string,string>> args, string body,
                     string global_body="", string namespace_body="", string functor_body="");


string generate_idl(const LiLACWhat& what);

struct CapturedRange
{
    string type;
    string name;
    string kind;
    string array;
    string range_size;
};

CapturedRange parse_marshalling(string input);

void GenerateIDLSpecs(vector<ProgramPart> parts)
{
    for(auto& part : parts)
    {
        if(part.header.size() == 2 && part.header[0] == "COMPUTATION" && part.extra_content.empty())
        {
            auto what = parse_lilacwhat(part.main_content);
            auto args = capture_arguments(what);

            auto string_toupper = [](string s) ->string{
                transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return toupper(c); });
                return s;
            };

            ofstream ofs(part.header[1]+".idl");
            ofs<<"Export\nConstraint "+string_toupper(part.header[1])+"\n"+generate_idl(what)+"\nEnd\n";
        }
    }
}

void GenerateHarnessInterfaces(vector<ProgramPart> parts)
{
    for(auto& part : parts)
    {
        if(part.header.size() == 2 && part.header[0] == "COMPUTATION" && part.extra_content.empty())
        {
            auto what = parse_lilacwhat(part.main_content);
            auto args = capture_arguments(what);

            auto string_toupper = [](string s) ->string{
                transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return toupper(c); });
                return s;
            };

            ofstream ofs(part.header[1]+"_replacer.h");
            ofs<<"{\""+string_toupper(part.header[1])+"\", [](const Solution& s)->Value*{ return s[\"outer_loop\"][\"comparison\"]; },\n"
                 "[](Function& function, Solution solution) {\n"
                 "    replace_idiom(function, solution, \""+part.header[1]+"_harness\", solution[\"outer_loop\"][\"precursor\"],\n"
                 "                  {";
            bool first = true;
            for(auto& arg : args)
            {
                if(!first) ofs<<",\n                   ";
                if(arg.first == "double*" || arg.first == "int*")
                    ofs<<"solution[\""<<arg.second<<"\"][\"base_pointer\"]";
                else if((what.isnode("loop") || what.isnode("map") || what.isnode("dot")) && what[2].isleaf(arg.second))
                    ofs<<"solution[\"outer_loop\"][\"iter_end\"]";
                else if(what.isnode("loop") && what[3].isnode("map") && what[3][2].isleaf(arg.second))
                    ofs<<"solution[\"map_loop\"][\"iter_end\"]";
                else if(what.isnode("map") && what[4].isnode("dot") && what[4][2].isleaf(arg.second))
                    ofs<<"solution[\"dot_loop\"][\"iter_end\"]";
                else if(what.isnode("loop") && what[3].isnode("map") && what[3][4].isnode("dot") && what[3][4][2].isleaf(arg.second))
                    ofs<<"solution[\"dot_loop\"][\"iter_end\"]";
                else
                    ofs<<"solution[\""<<arg.second<<"\"]";
                first = false;
            }
            ofs<<"},\n"
                 "                  {solution[\"output\"][\"store\"]}); }},\n";
        }
    }
}

void GenerateNaiveHarnesses(vector<ProgramPart> parts)
{
    for(auto& part : parts)
    {
        if(part.header.size() == 2 && part.header[0] == "COMPUTATION" && part.extra_content.empty())
        {
            auto what = parse_lilacwhat(part.main_content);
            auto args = capture_arguments(what);

            ofstream ofs(part.header[1]+"_naive.cc");
            ofs<<print_harness(part.header[1], args, generate_naive_impl(what));
        }
    }
}

map<string,string> GenerateMarshallingCpp(vector<ProgramPart> parts)
{
    map<string,string> result;
    for(auto& part : parts)
    {
        if(part.header.size() == 2 && (part.header[0] == "READABLE" || part.header[0] == "WRITEABLE"))
        {
            string code = "template<typename type_in, typename type_out>\n"
                          "void "+part.header[1]+"_update(type_in* in, int size, type_out& out) {\n"
                          +indent(part.main_content, 4)+
                          "}\n\n";

            if(!part.extra_content["BeforeFirstExecution"].empty())
            {
                code += "template<typename type_in, typename type_out>\n"
                        "void "+part.header[1]+"_construct(int size, type_out& out) {\n"
                        +indent(part.extra_content["BeforeFirstExecution"], 4)+
                        "}\n\n";
            }

            if(!part.extra_content["AfterLastExecution"].empty())
            {
                code += "template<typename type_in, typename type_out>\n"
                        "void "+part.header[1]+"_destruct(int size, type_out& out) {\n"
                        +indent(part.extra_content["AfterLastExecution"], 4)+
                        "}\n\n";
            } 

            string base_class = (part.header[0] == "READABLE")?"ReadObject":"WriteObject";

            code += "template<typename type_in, typename type_out>\n"
                    "using "+part.header[1]+" = "+base_class+"<type_in, type_out,\n"
                    "    "+part.header[1]+"_update<type_in,type_out>,\n";

            if(part.extra_content["BeforeFirstExecution"].empty())
                 code += "    nullptr,\n";
            else code += "    "+part.header[1]+"_construct<type_in,type_out>,\n";

            if(part.extra_content["AfterLastExecution"].empty())
                 code += "    nullptr>;\n\n";
            else code += "    "+part.header[1]+"_destruct<type_in,type_out>>;\n\n";

            result[part.header[1]] = code;
        }
    }
    return result;
}

void GenerateSpecifiedHarnesses(vector<ProgramPart> parts)
{
    map<string,vector<pair<string,string>>> interface_dict;
    map<string,string>                      marshalling_dict = GenerateMarshallingCpp(parts);

    for(auto& part : parts)
    {
        if(part.header.size() == 2 && part.header[0] == "COMPUTATION" && part.extra_content.empty())
        {
            auto what = parse_lilacwhat(part.main_content);
            auto args = capture_arguments(what);

            interface_dict[part.header[1]] = args;
        }
    }

    for(auto& part : parts)
    {
        if(part.header.size() == 4 && part.header[0] == "HARNESS")
        {
            ofstream ofs(part.header[3]+"_"+part.header[1]+".cc");

            auto arguments = interface_dict[part.header[3]];
            auto elemtype = [&arguments](const string s)->string{
                for(const auto& arg : arguments)
                    if(arg.second == s && !arg.first.empty() && arg.first.back() == '*')
                        return {arg.first.begin(), arg.first.end()-1};
                return {};
            };

            string functor_body, inner_code;

            if(!part.extra_content["BeforeFirstExecution"].empty())
            {
                functor_body += "Functor() {\n"
                                +indent(part.extra_content["BeforeFirstExecution"], 4)+
                                "}\n\n";
            }
            if(!part.extra_content["AfterLastExecution"].empty())
            {
                functor_body += "~Functor() {\n"
                                +indent(part.extra_content["AfterLastExecution"], 4)+
                                "}\n\n";
            }

            for(const auto& line : split_by(part.extra_content["PersistentVariables"], '\n'))
                functor_body += line+";\n";

            vector<string> used_marsh;
            for(const auto& line : split_by(part.extra_content["OnDemandEvaluated"], '\n'))
            {
                auto marsh = parse_marshalling(line);
                inner_code   += "auto "+marsh.name+" = shadow_"+marsh.name+"("+marsh.array+", "+marsh.range_size+");\n";
                functor_body += marsh.kind+"<"+elemtype(marsh.array)+","+marsh.type+"> shadow_"+marsh.name+";\n";

                if(find(used_marsh.begin(), used_marsh.end(), marsh.kind) == used_marsh.end())
                    used_marsh.push_back(marsh.kind);
            }

            string namespace_body;
            for(const auto& marsh : used_marsh)
                namespace_body += marshalling_dict[marsh];

            ofs<<print_harness(part.header[3], arguments, inner_code+part.main_content,
                               part.extra_content["CppHeaderFiles"], namespace_body, functor_body);
            ofs.close();
        }
    }
}

vector<string> split_by(string input, char s)
{
    vector<string> list;
    bool separated = true;
    for(char c : input)
    {
        if(c != s && separated) list.push_back({c}); 
        else if (c != s) list.back().push_back(c);
        separated = (c == s);
    }

    for(auto& line : list)
        while(!line.empty() && isspace(line.back()))
            line.pop_back();
    return list;
}

string indent(string input, size_t n)
{
    string result = string(n, ' ');
    for(size_t i = 0; i <= input.size(); i++)
    {
        if(i == input.size() || input[i] == '\n')
            while(!result.empty() && isspace(result.back()) && result.back() != '\n')
                result.pop_back();
        if(i < input.size()) result.push_back(input[i]);
        if(i < input.size() && input[i] == '\n') result += string(n, ' ');
    }
    return result;
}

struct ProgramSegment
{
    vector<string> header;
    vector<string> content;
};

vector<ProgramPart> SplitCodeParts(string input)
{
    auto line_list = split_by(input, '\n');

    vector<pair<vector<string>,vector<string>>> segment_list;
    for(size_t i = 0; i < line_list.size(); i++)
    {
        if(line_list[i].empty()) continue;

        vector<string> header = split_by(line_list[i], ' ');

        vector<string> content;
        for(; i+1 < line_list.size() && (line_list[i+1].empty() ||
              (line_list[i+1].size() > 2 && line_list[i+1][0] == ' ' && line_list[i+1][1] == ' ')); i++)
        {
            content.push_back(string(line_list[i+1].begin()+2, line_list[i+1].end()));
        }

        segment_list.push_back({move(header), move(content)});
    }

    vector<ProgramPart> result;
    for(size_t i = 0; i < segment_list.size(); i++)
    {
        ProgramPart part;
        part.header = move(segment_list[i].first);
        for(auto& line : segment_list[i].second)
            part.main_content += move(line)+"\n";

        for(; i+1 < segment_list.size() && segment_list[i+1].first.size() == 1; i++)
        {
            for(auto& line : segment_list[i+1].second)
                 part.extra_content[segment_list[i+1].first[0]] += move(line)+"\n";
        }
        result.push_back(move(part));
    }

    return result;
}

LiLACWhat parse_lilacwhat(string str)
{
    vector<LiLACWhat> tokens;
    string            new_tok;

    set<char> specials{'(', ')', '[', ']', '+', ',', '*', '=', '.', ':', '{', '}', '<', ';'};
    for(char c : str)
    {
        if(isspace(c)) continue;
        if(specials.find(c) != specials.end())
        {
            if(!new_tok.empty()) tokens.emplace_back("s", move(new_tok));
            new_tok.clear();
            tokens.emplace_back(string{c},  vector<LiLACWhat>{});
        }
        else new_tok.push_back(c);
    }
    if(!new_tok.empty()) tokens.emplace_back("s", move(new_tok));

    vector<LiLACWhat> stack;
    for(auto& token : tokens) {
        stack.push_back(move(token));

        auto try_rule = [&stack](string type, vector<string> rules, size_t keep=0) {
            auto test_atom = [](const LiLACWhat& token, string pattern) -> bool {
                pattern.push_back('|');
                size_t it, it2;
                for(it = 0; (it2 = pattern.find('|', it)) < pattern.size(); it = it2+1) {
                    if(pattern[it] == '=' && token.isleaf({pattern.begin()+it+1, pattern.begin()+it2})) return true;
                    if(pattern[it] != '=' && token.isnode({pattern.begin()+it, pattern.begin()+it2}))   return true;
                }
                return false;
            };

            if(stack.size() < rules.size()) return false;
            for(size_t i = 0; i < rules.size(); i++)
                if(!test_atom(stack[stack.size() - rules.size() + i], rules[i]))
                    return false;

            vector<LiLACWhat> gather_children;
            for(size_t i = 0; i < rules.size()-keep; i++)
                if((rules[i][0] != '=' || rules[i].find('|') < rules[i].size()) && (rules[i].size() > 1 || rules[i] == "s"))
                        gather_children.push_back(move(stack[stack.size()-rules.size()+i]));

            LiLACWhat what(type, gather_children);
            stack[stack.size()-rules.size()] = move(what);
            stack.erase(stack.end()-rules.size()+1, stack.end()-keep);
            return true;
        };

        while(try_rule("index", {"s", "[", "binop|index|s", "]"})
           || try_rule("binop", {"binop|index|s", "+|-|*", "binop|index|s", ")|]|,|+|-|*"}, 1)
           || try_rule("dot",   {"=sum", "(", "binop|index|s", "<", "=", "s", "<",
                                 "binop|index|s", ")", "binop|index|s", "*", "binop|index|s", ";"})
           || try_rule("map",   {"=forall", "(", "binop|index|s", "<", "=", "s", "<",
                                 "binop|index|s", ")", "{", "index", "=", "dot", "}"})
           || try_rule("loop",  {"=forall", "(", "binop|index|s", "<", "=", "s", "<",
                                 "binop|index|s", ")", "{", "map", "}"}));
    }

    if(stack.size() == 1)
        return stack[0];

    string error_string = "  The partially merged AST looks as follows:\n";
    for(auto entry : stack) error_string += "  "+entry.get_type()+"\n";
    throw "Syntax error in LiLAC-What.\n"+error_string;
}

CapturedRange parse_marshalling(string input)
{
    if(!input.empty() && input.back() == ']')
    {
        auto iter1 = min(input.find("="),             input.size());
        auto iter2 = min(input.find(" of ", iter1+1), input.size());
        auto iter3 = min(input.find("[0..", iter2+4), input.size());

        if(iter3 < input.size())
        {
            auto crop = [&input](size_t a, size_t b)->string {
                while(a < b && isspace(input[a])) a++;
                while(a < b && isspace(input[b-1])) b--;
                return string(input.begin()+a, input.begin()+b);
            };

            string type = crop(0, iter1);
            size_t iter0 = iter1;
            while(iter0 > 0 && !(isalnum(input[iter0-1]) || input[iter0-1]=='_')) iter0--;
            while(iter0 > 0 &&  (isalnum(input[iter0-1]) || input[iter0-1]=='_')) iter0--;


            return {crop(0, iter0), crop(iter0, iter1), crop(iter1+1, iter2),
                    crop(iter2+4, iter3), crop(iter3+4, input.size()-1)};
        }
    }

    throw string("Syntax error on marshaling line \""+input+"\".");
}

string generate_naive_impl(const LiLACWhat& what, string pad)
{
    if(what.isloop())
    {
        string loopheader = "for(int "+what.get_loop_iter()+" = "+generate_naive_impl(what[0])+"; "
                                      +what.get_loop_iter()+" < "+generate_naive_impl(what[2])+"; "
                                      +what.get_loop_iter()+"++)";
        if(what.isnode("map"))
            return pad+loopheader+" {\n"
                  +pad+"  double value = 0.0;\n"
                      +generate_naive_impl(what[4], pad+"  ")
                  +pad+"  "+generate_naive_impl(what[3])+" = value;\n"
                  +pad+"}\n";
        else if(what.isnode("dot"))
            return pad+loopheader+"\n"
                  +pad+"  value += "+generate_naive_impl(what[3])+" * "+generate_naive_impl(what[4])+";\n";
        else
            return pad+loopheader+" {\n"
                      +generate_naive_impl(what[3], pad+"  ")
                  +pad+"}\n";
    }
    else if(what.isnode("index"))
        return what[0].get_leaf()+"["+generate_naive_impl(what[1])+"]";
    else if(what.isnode("binop"))
        return generate_naive_impl(what[0])+what[1].get_type()+generate_naive_impl(what[2]);
    else if(what.isnode("s")) return what.get_leaf();
    else throw string("Invalid computation \""+what.get_type()+"\", cannot generate naive implementation.");
}

vector<pair<string,string>> capture_arguments(const LiLACWhat& what)
{
    vector<pair<string,string>> arguments;
    set<string> arg_set;

    auto capture = [&arg_set,&arguments](string type, string name) {
        if(arg_set.insert(name).second) arguments.push_back({type, name});
    };

    vector<LiLACWhat> stack{what};
    while(!stack.empty())
    {
        auto front = stack.back();
        stack.pop_back();
        if(front.isnode("s") && !isdigit(front.get_leaf().front()))
        {
            capture("int", front.get_leaf());
        }
        else if(front.isnode("index"))
        {
            capture("int*", front[0].get_leaf());
            stack.push_back(front[1]);
        }
        else if(front.isnode("binop"))
        {
            stack.push_back(front[2]);
            stack.push_back(front[0]);
        }
        else if(front.isloop())
        {
            arg_set.insert(front.get_loop_iter());
        
            if(!front.isnode("loop")) capture("double*", front[3][0].get_leaf());
            if(front.isnode("dot"))   capture("double*", front[4][0].get_leaf());
            if(front.isnode("map"))   stack.push_back(front[4]);
            if(front.isnode("dot"))   stack.push_back(front[4][1]);
            if(front.isnode("loop"))  stack.push_back(front[3]);
            else                      stack.push_back(front[3][1]);
            stack.push_back(front[2]);
            stack.push_back(front[0]);
        }
    }

    vector<pair<string,string>> reordered;
    for(auto& arg : arguments) if(arg.first == "double*") reordered.push_back(move(arg));
    for(auto& arg : arguments) if(arg.first == "int*")    reordered.push_back(move(arg));
    for(auto& arg : arguments) if(arg.first == "int")     reordered.push_back(move(arg));
    return reordered;
}

string generate_idl(const LiLACWhat& what)
{
    vector<pair<LiLACWhat,int>> stack{{what, 0}};

    string                         code;
    size_t                         expr_name_counter = 1;
    vector<string>                 expr_name_stack;
    vector<pair<LiLACWhat,string>> what_names;
    string                         last_loop;

    auto nested_comp = [&expr_name_counter,&expr_name_stack,&what_names,&stack](const LiLACWhat& what)->string {
        for(const auto& prev_what : what_names)
            if(prev_what.first == what)
                return prev_what.second;

        string name;
        if(what.isnode("s"))
        {

        }
        else if(what.isnode("index"))
        {
            stack.push_back({what,0});
            name = what[0].get_leaf()+".value";
        }
        else if(what.isnode("binop"))
        {
            stack.push_back({what,0});
            name = "tmp"+string(1, '0'+(expr_name_counter++));
            expr_name_stack.push_back(name);
            name = name+".value";
        }

        what_names.push_back({what, name});
        return name;
    };

    while(!stack.empty()) {
        auto front = stack.back().first;
        auto flags = stack.back().second;
        stack.pop_back();

        if(what.isnode("loop"))
        {
            return "( inherits ForNest(N=3)\n"
                   "      with {outer_loop} as {for[0]}\n"
                   "       and {map_loop}   as {for[1]}\n"
                   "       and {dot_loop}   as {for[2]} and\n"
                   "  inherits MatrixStore\n"
                   "      with {outer_loop.iterator} as {col}\n"
                   "       and {map_loop.iterator}   as {row}\n"
                   "       and {begin} as {begin}    at {output} and\n"
                   "  inherits MatrixRead\n" 
                   "      with {outer_loop.iterator} as {col}\n"
                   "       and {dot_loop.iterator}   as {row}\n"
                   "       and {begin} as {begin}    at {left} and\n"
                   "  inherits MatrixRead\n"
                   "      with {map_loop.iterator} as {col}\n"
                   "       and {dot_loop.iterator} as {row}\n"
                   "       and {begin} as {begin}  at {right} and\n"
                   "  inherits DotProductLoopAlphaBeta\n"
                   "      with {dot_loop}       as {loop}\n"
                   "       and {left.value}     as {src1}\n"
                   "       and {right.value}    as {src2}\n"
                   "       and {output.address} as {update_address})";
        }
        else if(front.isnode("map"))
        {
            string new_loop = last_loop.empty()?"outer_loop":"map_loop";
            code = code+(code.empty()?"( ":" and\n  ")+"inherits For2 at {"+new_loop+"}";
            if(!last_loop.empty())
                code += " and\n  {"+last_loop+".begin} strictly\n"
                              "      control flow dominates {"+new_loop+".begin} and\n"
                              "  {"+last_loop+".end} strictly\n"
                              "      control flow post dominates {"+new_loop+".end}";
            last_loop = new_loop;

            what_names.push_back({{"s", front.get_loop_iter()}, new_loop+".iterator"});



            stack.push_back({front[4],0});
            stack.push_back({front[3],1});
        }
        else if(front.isnode("dot"))
        {
            string new_loop = last_loop.empty()?"outer_loop":"dot_loop";
            code = code+(code.empty()?"( ":" and\n  ")+"inherits DotProductFor at {"+new_loop+"}";
            if(!last_loop.empty())
                code += " and\n  {"+last_loop+".begin} strictly\n"
                              "      control flow dominates {"+new_loop+".begin} and\n"
                              "  {"+last_loop+".end} strictly\n"
                              "      control flow post dominates {"+new_loop+".end}";
            last_loop = new_loop;
            what_names.push_back({{"s", front.get_loop_iter()}, new_loop+".iterator"});
            stack.push_back({front[4],3});
            stack.push_back({front[3],2});
            
        }
        else if(front.isnode("index")) {
            code              += (code.empty()?"( ":" and\n  ");
            code =       code +    "inherits Vector"+((flags==1)?"Store":"Read")+"\n"
                                 "      with {outer_loop}      as {scope}\n";
            if(flags==2) code += "       and {"+last_loop+".src1}   as {value}\n";
            if(flags==3) code += "       and {"+last_loop+".src2}   as {value}\n";
            code +=              "       and {"+nested_comp(front[1])+"} as {input_index}\n"
                                 "                             at {"+front[0].get_leaf()+"}";
        }
        else if(front.isnode("binop") && front[1].isnode("+")) {
            code              += (code.empty()?"( ":" and\n  ");
            code = code+"inherits Addition\n"
                      "      with {"+nested_comp(front[0])+"} as {input}\n"
                      "       and {"+nested_comp(front[2])+"} as {addend}\n"
                      "                             at {"+expr_name_stack.back()+"}";
            expr_name_stack.pop_back();
        }
    }

    code += (code.empty()?"( ":" and\n  ");
    if(what[4][0].isleaf("0"))
        code += "inherits ReadZeroRanges\n"
              "      with {outer_loop}          as {scope}\n"
              "       and {"+last_loop+".iter_begin} as {range_begin}\n"
              "       and {"+last_loop+".iter_end}   as {range_end}\n"
              "       and {outer_loop.iterator} as {input_index}\n"
              "                                 at {"+what[4][2][0].get_leaf()+"}";
    else
        code += "inherits ReadRanges\n"
              "      with {outer_loop}          as {scope}\n"
              "       and {"+last_loop+".iter_begin} as {range_begin}\n"
              "       and {"+last_loop+".iter_end}   as {range_end}\n"
              "       and {outer_loop.iterator} as {input_index}\n"
              "                                 at {"+what[4][2][0].get_leaf()+"}";
    return code+")";
}

string print_harness(string name, vector<pair<string,string>> args, string body,
                     string global_body, string namespace_body, string functor_body)
{
    string type_interface, notp_interface;
    for(const auto& arg : args)
    {
        type_interface += (type_interface.empty()?"":", ")+arg.first+" "+arg.second;
        notp_interface += (notp_interface.empty()?"":", ")+arg.second;
    }

    return "#include \"llvm/IDL/harness.hpp\"\n"
           "#include <cstdio>\n"
           +global_body+"\n"
           "namespace {\n"
           +namespace_body+
           "struct Functor\n{\n"
           +indent(functor_body, 4)+(functor_body.empty()?"":"\n")+
           "    void operator()("+type_interface+") {\n"
           "        printf(\"Entering harness '"+name+"'\\n\");\n"
           +indent(body, 8)+
           "        printf(\"Leaving harness '"+name+"'\\n\");\n"
           "    }\n};\n}\n\n"
           "extern \"C\"\n"
           "void "+name+"_harness("+type_interface+") {\n"
           "    static Functor functor;\n"
           "    functor("+notp_interface+");\n"
           "}\n";
}

#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>

using namespace std;

vector<string> split_by(string input, char s)
{
    vector<string> list;
    bool separated = true;
    for(char c : input)
    {
        if(c != s && separated) list.push_back({c}); 
        else if (c != s)        list.back().push_back(c);
        separated = (c == s);
    }
    return list;
}

void strip_right(string& input)
{
    while(!input.empty() && isspace(input.back()))
        input.pop_back();
}

struct ProgramSegment
{
    vector<string> header;
    vector<string> content;
};

vector<ProgramSegment> parse_segments(string input)
{
    auto line_list = split_by(input, '\n');

    for(auto& line : line_list) strip_right(line);

    vector<ProgramSegment> result;
    for(int i = 0; i < line_list.size(); i++)
    {
        if(line_list[i].empty()) continue;

        ProgramSegment segment;
        segment.header = split_by(line_list[i], ' ');

        for(; i+1 < line_list.size() && (line_list[i+1].empty() ||
              (line_list[i+1].size() > 2 && line_list[i+1][0] == ' ' && line_list[i+1][1] == ' ')); i++)
        {
            segment.content.push_back(string(line_list[i+1].begin()+2, line_list[i+1].end()));
        }

        result.push_back(move(segment));
    }

    return result;
}

struct ProgramPart
{
    vector<string> header;
    string main_content;
    map<string,vector<string>> extra_content;
};

vector<ProgramPart> parse_parts(string input)
{
    auto segment_list = parse_segments(input);

    vector<ProgramPart> result;
    for(int i = 0; i < segment_list.size(); i++)
    {
        ProgramPart part;
        part.header = move(segment_list[i].header);
        for(auto& line : segment_list[i].content)
            part.main_content += move(line)+"\n";

        for(; i+1 < segment_list.size() && segment_list[i+1].header.size() == 1; i++)
        {
            part.extra_content[segment_list[i+1].header[0]] = segment_list[i+1].content;
        }
        result.push_back(move(part));
    }
    return result;
}

struct LiLACWhat
{
    LiLACWhat() {}
    LiLACWhat(string t) : type(t) {}
    LiLACWhat(string t, string c): type(t), content{c} {}
    LiLACWhat(string t, vector<LiLACWhat> c): type(t), child{c} {}

    string type;
    string content;
    vector<LiLACWhat> child;
};

LiLACWhat parse_lilacwhat(string str)
{
    vector<LiLACWhat> tokens;

    set<char> specials{'(', ')', '[', ']', '+', ',', '*', '=', '.', ':', '{', '}', '<', ';'};
    for(char c : str)
    {
        if(isspace(c)) continue;
        if(specials.find(c) != specials.end()) tokens.emplace_back(string{c});
        else if(!tokens.empty() && tokens.back().type == "s") tokens.back().content.push_back(c);
        else tokens.emplace_back("s", string{c});
    }

    vector<LiLACWhat> stack;
    auto test_rule = [&stack](vector<string> rules) {

        auto test_atom = [](const LiLACWhat& token, string pattern) -> bool {
            if(pattern.front() == '=') return token.content == string(pattern.begin()+1, pattern.end());
            size_t it = 0, it2;
            while((it2 = pattern.find('|', it)) < pattern.size()) {
                if(token.type == string(pattern.begin() + it, pattern.begin()+it2))
                    return true;
                it = it2+1;
            }
            return token.type == string(pattern.begin() + it, pattern.end());
        };

        if(stack.size() < rules.size()) return false;
        for(int i = 0; i < rules.size(); i++)
            if(!test_atom(stack[stack.size() - rules.size() + i], rules[i]))
                return false;
        return true;
    };

    for(auto& token : tokens) {
        stack.push_back(move(token));

        while(true)
        {
            if(test_rule({"binary|index|s|const", "+|-|*", "binary|index|s|const", ")|]|,|+|-|*"}))
            {
                stack.rbegin()[3] = LiLACWhat("binary", {stack.rbegin()[3], stack.rbegin()[2], stack.rbegin()[1]});
                stack.rbegin()[2] = stack.rbegin()[0];
                stack.resize(stack.size()-2);
            }
            if(test_rule({"=sum", "(", "binary|index|s|const", "<", "=", "s", "<", "binary|index|s|const", ")",
                          "binary|index|s|const", "*", "binary|index|s|const", ";"}))
            {
                stack.rbegin()[12] = LiLACWhat("dot", {stack.rbegin()[10],stack.rbegin()[5], stack.rbegin()[7],
                                                       stack.rbegin()[3], stack.rbegin()[1]});
                stack.resize(stack.size()-12);
            }
            if(test_rule({"=forall", "(", "binary|index|s|const", "<", "=", "s", "<", "binary|index|s|const", ")",
                          "{", "index", "=", "dot", "}"}))
            {
                stack.rbegin()[13] = LiLACWhat("map", {stack.rbegin()[11],stack.rbegin()[6], stack.rbegin()[8],
                                                       stack.rbegin()[3], stack.rbegin()[1]});
                stack.resize(stack.size()-13);
            }
            if(test_rule({"=forall", "(", "binary|index|s|const", "<", "=", "s", "<", "binary|index|s|const", ")",
                          "{", "map", "}"}))
            {
                stack.rbegin()[11] = LiLACWhat("loop", {stack.rbegin()[9], stack.rbegin()[4], stack.rbegin()[6],
                                                        stack.rbegin()[1]});
                stack.resize(stack.size()-11);
            }
            if(test_rule({"s", "[", "binary|index|s|const", "]"}))
            {
                stack.rbegin()[3] = LiLACWhat("index", {stack.rbegin()[3], stack.rbegin()[1]});
                stack.resize(stack.size()-3);
            }
            else break;
        }
    }

    if(stack.size() == 1)
        return stack[0];

    string error_string = "  The final stack looks as follows:\n";
    for(auto entry : stack) error_string += "  "+entry.type+"\n";
    throw "Syntax error in LiLAC-What.\n"+error_string;
}

struct CapturedRange
{
    string variable;
    string marshalling;
    string memory;
    string range_left;
    string range_right;
};

CapturedRange parse_marshalling(string input)
{
    while(!input.empty() && isspace(input.back())) input.pop_back();

    if(!input.empty() && input.back() == ']')
    {
        auto iter1 = min(input.find("="),             input.size());
        auto iter2 = min(input.find(" of ", iter1+1), input.size());
        auto iter3 = min(input.find("[",    iter2+4), input.size());
        auto iter4 = min(input.find("..",   iter3+1), input.size());

        if(iter4 < input.size())
        {
            auto crop = [&input](size_t a, size_t b)->string {
                while(a < b && isspace(input[a])) a++;
                while(a < b && isspace(input[b-1])) b--;
                return string(input.begin()+a, input.begin()+b);
            };

            return {crop(0, iter1), crop(iter1+1, iter2), crop(iter2+4, iter3),
                    crop(iter3+1, iter4), crop(iter4+2, input.size()-1)};
        }
    }

    throw string("Syntax error on marshalling line.");
}

string generate_naive_impl(const LiLACWhat& what, string pad = "  ")
{
    if(what.type == "s") return what.content;
    else if(what.type == "index" && what.child.size() == 2)
        return what.child[0].content+"["+generate_naive_impl(what.child[1])+"]";
    else if(what.type == "binary")
        return generate_naive_impl(what.child[0])+what.child[1].type+generate_naive_impl(what.child[2]);
    else if(what.type == "loop")
        return pad+"int "+what.child[2].content+", "+what.child[3].child[2].content+", "+what.child[3].child[4].child[2].content+";\n"
              +pad+"for("+what.child[2].content+" = "+generate_naive_impl(what.child[0])+"; "
                         +what.child[2].content+" < "+generate_naive_impl(what.child[1])+"; "
                         +what.child[2].content+"++) {\n"
              +pad+"  for("+what.child[3].child[2].content+" = "+generate_naive_impl(what.child[3].child[0])+"; "
                           +what.child[3].child[2].content+" < "+generate_naive_impl(what.child[3].child[1])+"; "
                           +what.child[3].child[2].content+"++) {\n"
              +pad+"    double value = 0.0;\n"
                  +generate_naive_impl(what.child[3].child[4], pad+"    ")
              +pad+"    "+generate_naive_impl(what.child[3].child[3])+" = value;\n"
              +pad+"  }\n"
              +pad+"}\n";
    else if(what.type == "map")
        return pad+"int "+what.child[2].content+", "+what.child[4].child[2].content+";\n"
              +pad+"for("+what.child[2].content+" = "+generate_naive_impl(what.child[0])+"; "
                         +what.child[2].content+" < "+generate_naive_impl(what.child[1])+"; "
                         +what.child[2].content+"++) {\n"
              +pad+"  double value = 0.0;\n"
                  +generate_naive_impl(what.child[4], pad+"  ")
              +pad+"  "+generate_naive_impl(what.child[3])+" = value;\n"
              +pad+"}\n";
    else if(what.type == "dot")
        return pad+"for("+what.child[2].content+" = "+generate_naive_impl(what.child[0])+"; "
                         +what.child[2].content+" < "+generate_naive_impl(what.child[1])+"; "
                         +what.child[2].content+"++)\n"
              +pad+"  value += "+generate_naive_impl(what.child[3])+" * "+generate_naive_impl(what.child[4])+";\n";

    throw string("Invalid computation \""+what.type+"\", cannot generate naive implementation.");
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
        if(front.type == "s")
        {
            if(isalpha(front.content.front()))
                capture("int", front.content);
        }
        else if(front.type == "index")
        {
            capture("int*", front.child[0].content);
            stack.push_back(front.child[1]);
        }
        else if(front.type == "binary")
        {
            stack.push_back(front.child[2]);
            stack.push_back(front.child[0]);
        }
        else if(front.type == "loop")
        {
            arg_set.insert(front.child[2].content);
            stack.push_back(front.child[1]);
            stack.push_back(front.child[0]);
            stack.push_back(front.child[3]);
        }
        else if(front.type == "map")
        {
            arg_set.insert(front.child[2].content);
            capture("double*", front.child[3].child[0].content);
            stack.push_back(front.child[1]);
            stack.push_back(front.child[0]);
            stack.push_back(front.child[4]);
            stack.push_back(front.child[3].child[1]);
        }
        else if(front.type == "dot")
        {
            arg_set.insert(front.child[2].content);
            capture("double*", front.child[3].child[0].content);
            capture("double*", front.child[4].child[0].content);
            stack.push_back(front.child[1]);
            stack.push_back(front.child[0]);
            stack.push_back(front.child[4].child[1]);
            stack.push_back(front.child[3].child[1]);
        }
    }
    return arguments;
}

string generate_idl(const LiLACWhat& what)
{
    if(what.type == "loop")
        return "( inherits ForNest(N=3) and\n"
               "  inherits MatrixStore\n"
               "      with {iterator[0]} as {col}\n"
               "       and {iterator[1]} as {row}\n"
               "       and {begin} as {begin} at {output} and\n"
               "  inherits MatrixRead\n"
               "      with {iterator[0]} as {col}\n"
               "       and {iterator[2]} as {row}\n"
               "       and {begin} as {begin} at {input1} and\n"
               "  inherits MatrixRead\n"
               "      with {iterator[1]} as {col}\n"
               "       and {iterator[2]} as {row}\n"
               "       and {begin} as {begin} at {input2} and\n"
               "  inherits DotProductLoop\n"
               "      with {for[2]}         as {loop}\n"
               "       and {input1.value}   as {src1}\n"
               "       and {input2.value}   as {src2}\n"
               "       and {output.address} as {update_address})\n";
    else if(what.type == "map")
        return "( inherits SPMV_BASE and\n"
               "  {matrix_read.idx} is the same as {inner.iterator} and\n"
               "  {vector_read.idx} is the same as {index_read.value} and\n"
               "  {index_read.idx}  is the same as {inner.iterator} and\n"
               "  {output.idx}      is the same as {iterator} and\n"
               "  inherits ReadRange\n"
               "      with {iterator} as {idx}\n"
               "       and {inner.iter_begin} as {range_begin}\n"
               "       and {inner.iter_end}   as {range_end})\n";
    else return "";
}


void parse_program(string input)
{
    auto parts = parse_parts(input);

    for(int i = 0; i < parts.size(); i++)
    {
        if(parts[i].header.size() == 2 && parts[i].header[0] == "COMPUTATION" && parts[i].extra_content.empty())
        {
            auto what = parse_lilacwhat(parts[i].main_content);
            auto args = capture_arguments(what);

            string type_interface, notp_interface;
            for(const auto& arg : args)
            {
                type_interface += (type_interface.empty()?"":", ")+arg.first+" "+arg.second;
                notp_interface += (notp_interface.empty()?"":", ")+arg.second;
            }

            ofstream ofs;
            ofs.open(parts[i].header[1]+"_naive.cc");
            ofs<<"#include \"llvm/IDL/harness.hpp\"\n"
                 "#include <cstdio>\n\n"
                 "namespace {\nstruct Functor\n{\n"
                 "    void operator()("+type_interface+") {\n"
                 "        printf(\"Entering harness 'spmv_csr'\\n\");\n"
               <<generate_naive_impl(what, "        ")
               <<"        printf(\"Leaving harness 'spmv_csr'\\n\");\n"
                 "    }\n};\n}\n\n"
                 "extern \"C\"\n"
                 "void spmv_csr_harness("+type_interface+") {\n"
                 "    static Functor functor;\n"
                 "    functor("+notp_interface+");\n"
                 "}\n";
            ofs.close();

            auto string_toupper = [](string s) ->string{
                transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return toupper(c); });
                return s;
            };

            ofs.open(parts[i].header[1]+".idl");
            ofs<<"Constraint "+string_toupper(parts[i].header[1])+"\n"
               <<generate_idl(what)
               <<"End\n";
            ofs.close();
        }
    }
}

int main()
{
    try {
        parse_program(string(istreambuf_iterator<char>(cin), {}));
    }
    catch(string error) {
        std::cerr<<"Error: "<<error<<std::endl;
        return 1;
    }
    return 0;
}
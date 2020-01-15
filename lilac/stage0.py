#!/usr/bin/python
import sys
import os

computations = {}
interfaces   = {}
harnesses    = {}
readables    = {}
writeables   = {}

def read_input(stream):
    currentmode    = None
    currentsubmode = None
    currentobject  = None

    for n,line in enumerate(stream):
        if line.lstrip() == line:
            splitline = line.split()
            if len(splitline) == 2 and splitline[0] == "COMPUTATION":
                currentmode    = "INTERFACE"
                currentsubmode = "BODY"
                currentobject  = {"BODY": []}
                computations[splitline[1]] = currentobject

            elif len(splitline) == 2 and splitline[0] == "READABLE":
                currentmode    = "READABLE"
                currentsubmode = "BODY"
                currentobject  = {"BODY": [], "BeforeFirstExecution" : [], "AfterLastExecution" : []}
                readables[splitline[1]] = currentobject

            elif len(splitline) == 2 and splitline[0] == "WRITEABLE":
                currentmode    = "WRITEABLE"
                currentsubmode = "BODY"
                currentobject  = {"BODY": [], "BeforeFirstExecution" : [], "AfterLastExecution" : []}
                writeables[splitline[1]] = currentobject

            elif len(splitline) == 4 and splitline[0] == "HARNESS" and splitline[2] == "IMPLEMENTS":
                currentmode    = "HARNESS"
                currentsubmode = "BODY"
                currentobject  = {"INTERFACE" : splitline[3],
                                  "BODY": [], "OnDemandEvaluated" : [],
                                  "CppHeaderFiles" : [], "PersistentVariables" : [],
                                  "BeforeFirstExecution" : [], "AfterLastExecution" : []}
                harnesses[splitline[1],splitline[3]] = currentobject

            elif len(splitline) == 1:
                if ((currentmode in ["WRITEABLE", "READABLE", "HARNESS"]
                     and splitline[0] in ["BeforeFirstExecution", "AfterLastExecution"]) or
                     currentmode in ["HARNESS"]
                     and splitline[0] in ["OnDemandEvaluated", "CppHeaderFiles", "PersistentVariables"]):
                    currentsubmode = splitline[0]
                else:
                    raise Exception("Syntax error in line "+str(n+1)+"\n")

            else:
                raise Exception("Syntax error in line "+str(n+1)+"\n")

        elif line.startswith("  "):

            if currentsubmode in ["BODY", "BeforeFirstExecution", "AfterLastExecution", "CppHeaderFiles", "COMPUTATION"]:
                currentobject[currentsubmode].append(line[2:].rstrip())

            if currentsubmode in ["ARGUMENTS", "PersistentVariables"]:
                typename, name = line.split()
                currentobject[currentsubmode].append((typename,name))

            if currentsubmode == "OnDemandEvaluated":
                left,right = line.split("=")
                typename,name = left.split()
                classname,rangeexpr = right.split(" of ")
                classname, = classname.split()
                base, indexexpr = rangeexpr.split("[", 1)

                if indexexpr.endswith("]\n"):
                    indexexpr = indexexpr[:-2]
                else:
                    raise

                rangeleft, rangeright = indexexpr.split("..")

                rangeleft, = rangeleft.split()
                rangeright, = rangeright.split()

                if rangeleft != "0":
                    raise

                currentobject[currentsubmode].append((typename,name,classname,base,rangeright))

        elif line.strip():
            raise

def print_to_stream(stream, harness):
    stream.write("#include \"llvm/IDL/harness.hpp\"\n")
    stream.write("#include <cstdio>\n")

    for line in harness["CppHeaderFiles"]:
        stream.write("{}\n".format(line))

    stream.write("\n")
    stream.write("namespace {\n")

    used_readwriteables = set()

    for typename,name,classname,base,rangeright in harness["OnDemandEvaluated"]:
        if classname in used_readwriteables:
            continue
        used_readwriteables.add(classname)

        if classname in readables:
            readwriteable = readables[classname]
            writes        = "ReadObject"
        elif classname in writeables:
            readwriteable = writeables[classname]
            writes        = "WriteObject"
        else:
            raise

        if readwriteable["BODY"]:
            stream.write("template<typename type_in, typename type_out>\n")
            stream.write("void {}_update(type_in* in, int size, type_out& out) {{\n".format(classname))
            for line in readwriteable["BODY"]:
                stream.write("    {}\n".format(line))
            stream.write("}\n")
            stream.write("\n")

        if readwriteable["BeforeFirstExecution"]:
            stream.write("template<typename type_in, typename type_out>\n")
            stream.write("void {}_construct(int size, type_out& out) {{\n".format(classname))
            for line in readwriteable["BeforeFirstExecution"]:
                stream.write("    {}\n".format(line))
            stream.write("}\n")
            stream.write("\n")

        if readwriteable["AfterLastExecution"]:
            stream.write("template<typename type_in, typename type_out>\n")
            stream.write("void {}_destruct(int size, type_out& out) {{\n".format(classname))
            for line in readwriteable["AfterLastExecution"]:
                stream.write("    {}\n".format(line))
            stream.write("}\n")
            stream.write("\n")

        stream.write("template<typename type_in, typename type_out>\n")
        stream.write("using {} = {}<type_in, type_out,\n".format(classname, writes))

        if readwriteable["BODY"]:
            stream.write("    {}_update<type_in,type_out>,\n".format(classname))
        else:
            stream.write("    nullptr,\n".format(classname))

        if readwriteable["BeforeFirstExecution"]:
            stream.write("    {}_construct<type_in,type_out>,\n".format(classname))
        else:
            stream.write("    nullptr,\n".format(classname))

        if readwriteable["AfterLastExecution"]:
            stream.write("    {}_destruct<type_in,type_out>>;\n".format(classname))
        else:
            stream.write("    nullptr>;\n".format(classname))
        stream.write("\n")

    stream.write("struct Functor\n")
    stream.write("{\n")

    if harness["BeforeFirstExecution"]:
        stream.write("    Functor() {\n")
        for line in harness["BeforeFirstExecution"]:
            stream.write("        {}\n".format(line))
        stream.write("    }\n")
        stream.write("\n")

    if harness["AfterLastExecution"]:
        stream.write("    ~Functor() {\n")
        for line in harness["AfterLastExecution"]:
            stream.write("        {}\n".format(line))
        stream.write("    }\n")
        stream.write("\n")

    for typename,name in harness["PersistentVariables"]:
        stream.write("    {} {};\n".format(typename, name))

    if harness["OnDemandEvaluated"]:
        for typename,name,classname,base,rangeright in harness["OnDemandEvaluated"]:
            base_type = None
            for typename2,name2 in interfaces[harness["INTERFACE"]]["ARGUMENTS"]:
                if name2 == base:
                    base_type = typename2
            stream.write("    {}<{},{}> shadow_{};\n".format(classname,base_type[:-1], typename, name))

        stream.write("\n")

    stream.write("    void operator()({}) {{\n".format(", ".join(t+" "+n for (t,n) in interfaces[harness["INTERFACE"]]["ARGUMENTS"])))
    stream.write("        printf(\"Entering harness '{}'\\n\");\n".format(harness["INTERFACE"]))

    for typename,name,classname,base,rangeright in harness["OnDemandEvaluated"]:
        stream.write("        auto {} = shadow_{}({}, {});\n".format(name, name, base, rangeright))

    for line in harness["BODY"]:
        stream.write("        {}\n".format(line))

    stream.write("        printf(\"Leaving harness '{}'\\n\");\n".format(harness["INTERFACE"]))
    stream.write("    }\n")
    stream.write("};\n")
    stream.write("}\n")
    stream.write("\n")
    stream.write("extern \"C\"\n")
    stream.write("void {}_harness({}) {{\n".format(harness["INTERFACE"], ", ".join(t+" "+n for (t,n) in interfaces[harness["INTERFACE"]]["ARGUMENTS"])))
    stream.write("    static Functor functor;\n")
    stream.write("    functor({});\n".format(", ".join(n for (t,n) in interfaces[harness["INTERFACE"]]["ARGUMENTS"])))
    stream.write("}\n")

def parse_computation(computation):
    ast = []

    for c in computation:
        if c in "()[]+,*=.:{}<;":
            ast.append((None, c))
        else:
            ast.append(("s", c))

        while True:
            if len(ast) >= 2 and ast[-2][0] == ast[-1][0] == "s":
                ast = ast[:-2] + [("s", ast[-2][1] + ast[-1][1])]

            elif (len(ast) >= 4 and ast[-4][0] in ["binary", "index", "s", "const"]
                                and ast[-3][1] in ["+", "-", "*"]
                                and ast[-2][0] in ["binary", "index", "s", "const"]
                                and ast[-1][1] in [")", "]", ",", "+", "-", "*"]):
                ast = ast[:-4] + [("binary", ast[-3][1], ast[-4], ast[-2]), ast[-1]]

            elif (len(ast) >= 13 and ast[-1][1] == ";"
                                 and ast[-2][0] in ["binary", "index", "s", "const"]
                                 and ast[-3][1] == "*"
                                 and ast[-4][0] in ["binary", "index", "s", "const"]
                                 and ast[-5][1] == ")"
                                 and ast[-6][0] in ["binary", "index", "s", "const"]
                                 and ast[-7][1] == "<"
                                 and ast[-8][0] =="s"
                                 and ast[-9][1] == "="
                                 and ast[-10][1] == "<"
                                 and ast[-11][0] in ["binary", "index", "s", "const"]
                                 and ast[-12][1] == "("
                                 and ast[-13][1] == "sum"):
                ast = ast[:-13] + [("dot", [ast[-11], ast[-6], ast[-8], ast[-4], ast[-2]])]

            elif (len(ast) >= 14 and ast[-1][1] == "}"
                                 and ast[-2][0] == "dot"
                                 and ast[-3][1] == "="
                                 and ast[-4][0] == "index"
                                 and ast[-5][1] == "{"
                                 and ast[-6][1] == ")"
                                 and ast[-7][0] in ["binary", "index", "s", "const"]
                                 and ast[-8][1] == "<"
                                 and ast[-9][0] == "s"
                                 and ast[-10][1] == "="
                                 and ast[-11][1] == "<"
                                 and ast[-12][0] in ["binary", "index", "s", "const"]
                                 and ast[-13][1] == "("
                                 and ast[-14][1] == "forall"):
                ast = ast[:-14] + [("map", [ast[-12], ast[-7], ast[-9], ast[-4], ast[-2]])]

            elif (len(ast) >= 12 and ast[-1][1] == "}"
                                 and ast[-2][0] == "map"
                                 and ast[-3][1] == "{"
                                 and ast[-4][1] == ")"
                                 and ast[-5][0] in ["binary", "index", "s", "const"]
                                 and ast[-6][1] == "<"
                                 and ast[-7][0] == "s"
                                 and ast[-8][1] == "="
                                 and ast[-9][1] == "<"
                                 and ast[-10][0] in ["binary", "index", "s", "const"]
                                 and ast[-11][1] == "("
                                 and ast[-12][1] == "forall"):
                ast = ast[:-12] + [("loop", [ast[-10], ast[-5], ast[-7], ast[-2]])]

            elif (len(ast) >= 4 and ast[-1][1] == "]"
                                and ast[-2][0] in ["binary", "index", "s", "const"]
                                and ast[-3][1] == "["
                                and ast[-4][0] == "s"):
                ast = ast[:-4] + [("index", ast[-4][1], ast[-2])]

            else:
                break
    return ast

def generate_naive_recursive(parsed):
    if parsed[0] == "s":
        return parsed[1]
    elif parsed[0] == "index":
        return "{}[{}]".format(parsed[1], generate_naive_recursive(parsed[2]))
    elif parsed[0] == "binary":
        return "{}{}{}".format(generate_naive_recursive(parsed[2]),
                                 parsed[1],
                                 generate_naive_recursive(parsed[3]))
    elif parsed[0] == "loop":
        return (["int i, j, k;",
                 "for({} = {}; {} < {}; {}++) {{".format(
                    parsed[1][2][1], generate_naive_recursive(parsed[1][0]),
                    parsed[1][2][1], generate_naive_recursive(parsed[1][1]),
                    parsed[1][2][1]),

                 "  for({} = {}; {} < {}; {}++) {{".format(
                    parsed[1][3][1][2][1], generate_naive_recursive(parsed[1][3][1][0]),
                    parsed[1][3][1][2][1], generate_naive_recursive(parsed[1][3][1][1]),
                    parsed[1][3][1][2][1]),

                 "    double value = 0.0;"] +
                ["    " + line for line in generate_naive_recursive(parsed[1][3][1][4])] +
                ["    {} = value;".format(generate_naive_recursive(parsed[1][3][1][3])),
                 "  }",
                 "}"])

    elif parsed[0] == "map":
        return (["int i, j;",
                 "for({} = {}; {} < {}; {}++) {{".format(
                    parsed[1][2][1], generate_naive_recursive(parsed[1][0]),
                    parsed[1][2][1], generate_naive_recursive(parsed[1][1]),
                    parsed[1][2][1]),
                 "  double value = 0.0;"] +
                ["  " + line for line in generate_naive_recursive(parsed[1][4])] +
                ["  {} = value;".format(generate_naive_recursive(parsed[1][3])), "}"])

    elif parsed[0] == "dot":
        return ["for({} = {}; {} < {}; {}++)".format(
                    parsed[1][2][1], generate_naive_recursive(parsed[1][0]),
                    parsed[1][2][1], generate_naive_recursive(parsed[1][1]),
                    parsed[1][2][1]),
                "  value += {} * {};".format(
                    generate_naive_recursive(parsed[1][3]),
                    generate_naive_recursive(parsed[1][4]))]

def generate_naive(parsed):
    return generate_naive_recursive(parsed[0])

# float pointers, integer pointers, floats, integers
def generate_interface_recursive(parsed, iterators=[], typed=None):
    if parsed[0] == "s":
        if parsed[1] not in iterators and not parsed[1].isdigit():
            if typed == "float":
                return ([], [], [parsed[1]], [])
            elif typed == "int":
                return ([], [], [], [parsed[1]])
        return ([], [], [], [])
    elif parsed[0] == "index":
        parts = generate_interface_recursive(parsed[2], iterators, "int")
        if typed == "float":
            parts[0].append(parsed[1])
        elif typed == "int":
            parts[1].append(parsed[1])
        return parts
    elif parsed[0] == "binary":
        parts1 = generate_interface_recursive(parsed[2], iterators, typed)
        parts2 = generate_interface_recursive(parsed[3], iterators, typed)
        return (parts1[0]+parts2[0], parts1[1]+parts2[1],
                parts1[2]+parts2[2], parts1[3]+parts2[3])
    elif parsed[0] == "loop":
        new_iter = iterators[:]
        new_iter.append(parsed[1][2][1])
        parts1 = generate_interface_recursive(parsed[1][0], new_iter, "int")
        parts2 = generate_interface_recursive(parsed[1][1], new_iter, "int")
        parts3 = generate_interface_recursive(parsed[1][3], new_iter, "float")
        return (parts1[0]+parts2[0]+parts3[0],
                parts1[1]+parts2[1]+parts3[1],
                parts1[2]+parts2[2]+parts3[2],
                parts1[3]+parts2[3]+parts3[3])
    elif parsed[0] == "map":
        new_iter = iterators[:]
        new_iter.append(parsed[1][2][1])
        parts1 = generate_interface_recursive(parsed[1][0], new_iter, "int")
        parts2 = generate_interface_recursive(parsed[1][1], new_iter, "int")
        parts3 = generate_interface_recursive(parsed[1][3], new_iter, "float")
        parts4 = generate_interface_recursive(parsed[1][4], new_iter, "float")
        return (parts1[0]+parts2[0]+parts3[0]+parts4[0],
                parts1[1]+parts2[1]+parts3[1]+parts4[1],
                parts1[2]+parts2[2]+parts3[2]+parts4[2],
                parts1[3]+parts2[3]+parts3[3]+parts4[3])
    elif parsed[0] == "dot":
        new_iter = iterators[:]
        new_iter.append(parsed[1][2][1])
        parts1 = generate_interface_recursive(parsed[1][0], new_iter, "int")
        parts2 = generate_interface_recursive(parsed[1][1], new_iter, "int")
        parts3 = generate_interface_recursive(parsed[1][3], new_iter, "float")
        parts4 = generate_interface_recursive(parsed[1][4], new_iter, "float")
        return (parts1[0]+parts2[0]+parts3[0]+parts4[0],
                parts1[1]+parts2[1]+parts3[1]+parts4[1],
                parts1[2]+parts2[2]+parts3[2]+parts4[2],
                parts1[3]+parts2[3]+parts3[3]+parts4[3])
    else:
        print("Can't parse ", parsed)
    
def generate_interface(parsed):
    parts = generate_interface_recursive(parsed[0])
    parts = ([p for n,p in enumerate(parts[0]) if p not in parts[0][:n]],
             [p for n,p in enumerate(parts[1]) if p not in parts[1][:n]],
             [p for n,p in enumerate(parts[2]) if p not in parts[2][:n]],
             [p for n,p in enumerate(parts[3]) if p not in parts[3][:n]])
    return ([("double*", s) for s in parts[0]]
           +[("int*", s) for s in parts[1]]
           +[("double", s) for s in parts[2]]
           +[("int", s) for s in parts[3]])


def generateIDL(parsed):
    if parsed[0][0] == "loop" and parsed[0][1][3][0] == "map" and parsed[0][1][3][1][4][0] == "dot":
        return """( inherits ForNest(N=3) and
  inherits MatrixStore
      with {iterator[0]} as {col}
       and {iterator[1]} as {row}
       and {begin} as {begin} at {output} and
  inherits MatrixRead
      with {iterator[0]} as {col}
       and {iterator[2]} as {row}
       and {begin} as {begin} at {input1} and
  inherits MatrixRead
      with {iterator[1]} as {col}
       and {iterator[2]} as {row}
       and {begin} as {begin} at {input2} and
  inherits DotProductLoop
      with {for[2]}         as {loop}
       and {input1.value}   as {src1}
       and {input2.value}   as {src2}
       and {output.address} as {update_address})
"""
    elif parsed[0][0] == "map" and parsed[0][1][4][0] == "dot":
        return """( inherits SPMV_BASE and
  {matrix_read.idx} is the same as {inner.iterator} and
  {vector_read.idx} is the same as {index_read.value} and
  {index_read.idx}  is the same as {inner.iterator} and
  {output.idx}      is the same as {iterator} and
  inherits ReadRange
      with {iterator} as {idx}
       and {inner.iter_begin} as {range_begin}
       and {inner.iter_end}   as {range_end})
"""
    else:
        return ""

if len(sys.argv) == 2:
    read_input(open(sys.argv[1]))

    dirname = os.path.dirname(sys.argv[1])

    for computation in computations:
        parsed = parse_computation(" ".join(computations[computation]["BODY"]).replace(" ", ""))
        program = generate_naive(parsed)
        harnesses["naive",computation] = {"INTERFACE" : computation,
                                          "BODY": program, "OnDemandEvaluated" : [],
                                          "CppHeaderFiles" : [], "PersistentVariables" : [],
                                          "BeforeFirstExecution" : [], "AfterLastExecution" : []}
        interfaces[computation]={"ARGUMENTS":generate_interface(parsed)}

        idloutpath = filename  = "{}.idl".format(computation)
        idlstream  = open(idloutpath, "w")
        idlstream.write("Constraint "+computation.upper()+"\n")
        idlstream.write(generateIDL(parsed))
        idlstream.write("End\n")

        for harnessname,comp in harnesses:
            if computation == comp:
                idiomname = harnesses[harnessname,comp]["INTERFACE"]
                filename  = "{}_{}.cc".format(idiomname, harnessname)
                outpath   = filename
                stream    = open(outpath, "w")
                print_to_stream(stream, harnesses[harnessname,comp])
else:
    print("Usage: generator.py inputfile")



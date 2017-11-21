#!/usr/bin/pypy

grammar = """
# ::= <specification>

specification ::= Constraint <s> <constraint> End

constraint ::= <grouping> | <collect> | <rename> | <rebase> | <atom>
             | <GeneralizedDominance> | <GeneralizedSame> | '(' <constraint> ')'

grouping    ::= <conjunction> | <disjunction> | <inheritance> | <forall> | <forsome> | <forone> | <if> | <default>
inheritance ::= <rawinherit> | <arginherit>
rawinherit  ::= include <s>
arginherit  ::= <rawinherit> '(' <s> = <calculation> { , <s> = <calculation> } ')'
conjunction ::= '(' <constraint> and <constraint> { and <constraint> } ')'
disjunction ::= '(' <constraint> or <constraint> { or <constraint> } ')'
forall      ::= <constraint> for all <s> = <calculation> .. <calculation>
forsome     ::= <constraint> for some <s> = <calculation> .. <calculation>
forone      ::= <constraint> for <s> = <calculation>
default     ::= <constraint> for <s> = <calculation> if not otherwise specified
if          ::= if <calculation> = <calculation> then <constraint> else <constraint> endif
collect     ::= collect <s> <n> <constraint>
rename      ::= <grouping> with <slot> as <slot> { and <slot> as <slot> }
rebase      ::= <grouping> at <slot> | <rename> at <slot>

slot       ::= '{' <openslot> '}' | <slottuple>
openslot   ::= <slotbase> | <slotindex> | <slotrange> | <slotmember> | <slottuple>
slotbase   ::= <s>
slotindex  ::= <openslot> '[' <calculation> ']'
slotrange  ::= <openslot> '[' <calculation> .. <calculation> ']'
slotmember ::= <openslot> . <s>
slottuple  ::= '{' <openslot> , { <openslot> , } <openslot> '}'

calculation ::= <basevar> | <baseconst> | <addvar> | <addconst> | <subvar> | <subconst>
basevar     ::= <s>
baseconst   ::= <n>
addvar      ::= <calculation> + <s>
addconst    ::= <calculation> + <n>
subvar      ::= <calculation> - <s>
subconst    ::= <calculation> - <n>

atom ::= <ConstraintIntegerType> | <ConstraintFloatType> | <ConstraintVectorType> | <ConstraintPointerType>
         | <ConstraintIntZero> | <ConstraintFloatZero>
         | <ConstraintUnused>       | <ConstraintNotNumericConstant> | <ConstraintConstant>
         | <ConstraintPreexecution> | <ConstraintArgument>           | <ConstraintInstruction>
         | <ConstraintStoreInst>  | <ConstraintLoadInst>   | <ConstraintReturnInst> | <ConstraintBranchInst>
         | <ConstraintAddInst>    | <ConstraintSubInst>    | <ConstraintMulInst>    | <ConstraintFAddInst>
         | <ConstraintFSubInst>   | <ConstraintFMulInst>   | <ConstraintFDivInst>   | <ConstraintBitOrInst>
         | <ConstraintBitAndInst>
         | <ConstraintLShiftInst> | <ConstraintSelectInst> | <ConstraintSExtInst>   | <ConstraintZExtInst>
         | <ConstraintGEPInst>    | <ConstraintICmpInst>   | <ConstraintCallInst>
         | <ConstraintShufflevectorInst> | <ConstraintInsertelementInst>
         | <ConstraintSame> | <ConstraintDistinct>
         | <ConstraintDFGEdge> | <ConstraintCFGEdge> | <ConstraintCDGEdge> | <ConstraintPDGEdge>
         | <ConstraintFirstOperand> | <ConstraintSecondOperand>
         | <ConstraintThirdOperand> | <ConstraintFourthOperand>
         | <ConstraintDFGDominate>          | <ConstraintDFGPostdom>
         | <ConstraintDFGDominateStrict>    | <ConstraintDFGPostdomStrict>
         | <ConstraintCFGDominate>          | <ConstraintCFGPostdom>
         | <ConstraintCFGDominateStrict>    | <ConstraintCFGPostdomStrict>
         | <ConstraintPDGDominate>          | <ConstraintPDGPostdom>
         | <ConstraintPDGDominateStrict>    | <ConstraintPDGPostdomStrict>
         | <ConstraintDFGNotDominate>       | <ConstraintDFGNotPostdom>
         | <ConstraintDFGNotDominateStrict> | <ConstraintDFGNotPostdomStrict>
         | <ConstraintCFGNotDominate>       | <ConstraintCFGNotPostdom>
         | <ConstraintCFGNotDominateStrict> | <ConstraintCFGNotPostdomStrict>
         | <ConstraintPDGNotDominate>       | <ConstraintPDGNotPostdom>
         | <ConstraintPDGNotDominateStrict> | <ConstraintPDGNotPostdomStrict>
         | <ConstraintIncomingValue>
         | <ConstraintDFGBlocked> | <ConstraintCFGBlocked> | <ConstraintPDGBlocked>

ConstraintIntegerType ::= <slot> is an integer
ConstraintFloatType   ::= <slot> is a float
ConstraintVectorType  ::= <slot> is a vector
ConstraintPointerType ::= <slot> is a pointer

ConstraintIntZero   ::= <slot> is integer zero
ConstraintFloatZero ::= <slot> is floating point zero

ConstraintUnused             ::= <slot> is unused
ConstraintNotNumericConstant ::= <slot> is not a numeric constant
ConstraintConstant           ::= <slot> is a constant
ConstraintPreexecution       ::= <slot> is preexecution
ConstraintArgument           ::= <slot> is an argument
ConstraintInstruction        ::= <slot> is an instruction

ConstraintStoreInst  ::= <slot> is store instruction
ConstraintLoadInst   ::= <slot> is load instruction
ConstraintReturnInst ::= <slot> is return instruction
ConstraintBranchInst ::= <slot> is branch instruction
ConstraintAddInst    ::= <slot> is add instruction
ConstraintSubInst    ::= <slot> is sub instruction
ConstraintMulInst    ::= <slot> is mul instruction
ConstraintFAddInst   ::= <slot> is fadd instruction
ConstraintFSubInst   ::= <slot> is fsub instruction
ConstraintFMulInst   ::= <slot> is fmul instruction
ConstraintFDivInst   ::= <slot> is fdiv instruction
ConstraintBitOrInst  ::= <slot> is bitor instruction
ConstraintBitAndInst ::= <slot> is bitand instruction
ConstraintLShiftInst ::= <slot> is lshift instruction
ConstraintSelectInst ::= <slot> is select instruction
ConstraintSExtInst   ::= <slot> is sext instruction
ConstraintZExtInst   ::= <slot> is zext instruction
ConstraintGEPInst    ::= <slot> is gep instruction
ConstraintICmpInst   ::= <slot> is icmp instruction
ConstraintCallInst   ::= <slot> is call instruction
ConstraintShufflevectorInst ::= <slot> is shufflevector instruction
ConstraintInsertelementInst ::= <slot> is insertelement instruction

ConstraintSame     ::= <slot> is the same as <slot>
ConstraintDistinct ::= <slot> is not the same as <slot>

ConstraintDFGEdge ::= <slot> has data flow to <slot>
ConstraintCFGEdge ::= <slot> has control flow to <slot>
ConstraintCDGEdge ::= <slot> has control dominance to <slot>
ConstraintPDGEdge ::= <slot> has dependence edge to <slot>

ConstraintFirstOperand  ::= <slot> is first argument of <slot>
ConstraintSecondOperand ::= <slot> is second argument of <slot>
ConstraintThirdOperand  ::= <slot> is third argument of <slot>
ConstraintFourthOperand ::= <slot> is fourth argument of <slot>

ConstraintDFGDominate       ::= <slot> data flow dominates <slot>
ConstraintDFGPostdom        ::= <slot> data flow post dominates <slot>
ConstraintDFGDominateStrict ::= <slot> strictly data flow dominates <slot>
ConstraintDFGPostdomStrict  ::= <slot> strictly data flow post dominates <slot>

ConstraintCFGDominate       ::= <slot> control flow dominates <slot>
ConstraintCFGPostdom        ::= <slot> control flow post dominates <slot>
ConstraintCFGDominateStrict ::= <slot> strictly control flow dominates <slot>
ConstraintCFGPostdomStrict  ::= <slot> strictly control flow post dominates <slot>

ConstraintPDGDominate       ::= <slot> dominates <slot>
ConstraintPDGPostdom        ::= <slot> post dominates <slot>
ConstraintPDGDominateStrict ::= <slot> strictly dominates <slot>
ConstraintPDGPostdomStrict  ::= <slot> strictly post dominates <slot>

ConstraintDFGNotDominate       ::= <slot> does not data flow dominate <slot>
ConstraintDFGNotPostdom        ::= <slot> does not data flow post dominate <slot>
ConstraintDFGNotDominateStrict ::= <slot> does not strictly data flow dominate <slot>
ConstraintDFGNotPostdomStrict  ::= <slot> does not strictly data flow post dominate <slot>

ConstraintCFGNotDominate       ::= <slot> does not control flow dominate <slot>
ConstraintCFGNotPostdom        ::= <slot> does not control flow post dominate <slot>
ConstraintCFGNotDominateStrict ::= <slot> does not strictly control flow dominate <slot>
ConstraintCFGNotPostdomStrict  ::= <slot> does not strictly control flow post dominate <slot>

ConstraintPDGNotDominate       ::= <slot> does not dominate <slot>
ConstraintPDGNotPostdom        ::= <slot> does not post dominate <slot>
ConstraintPDGNotDominateStrict ::= <slot> does not strictly dominate <slot>
ConstraintPDGNotPostdomStrict  ::= <slot> does not strictly post dominate <slot>

ConstraintIncomingValue ::= <slot> reaches phi node <slot> from <slot>

ConstraintDFGBlocked ::= all data flow from <slot> to <slot> passes through <slot>
ConstraintCFGBlocked ::= all control flow from <slot> to <slot> passes through <slot>
ConstraintPDGBlocked ::= all flow from <slot> to <slot> passes through <slot>

GeneralizedSame  ::= <slot> is the same set as <slot>

GeneralizedDominance ::= all flow from <slot> or any origin to any of <slot> passes through at least one of <slot>
"""

def token_match(token, rule):
    if rule.startswith("<") and rule.endswith(">"):
        if rule == "<s>":   return type(token) is str
        elif rule == "<n>": return type(token) is int
        else:               return type(token) is tuple and token[0] == rule[1:-1]
    else:                   return type(token) is str and token == rule

def full_match(stack, rules, lookahead):
    if lookahead and (len(stack) < len(rules) + 1 or not any(token_match(stack[-len(rules)-1], l) for l in lookahead)):
        return False
    elif len(stack) < len(rules) or not all(token_match(stack[-1-i], rules[-1-i]) for i in range(len(rules))):
        return False
    else:
        return True

def list_possibles(stack, grammar, lookahead_rules):
    possibles    = []
    not_dead_end = all(type(s) == tuple and s[0]=="#" for s in stack)

    for name, rule in grammar:
        for branch in rule:
            if full_match(stack, branch, lookahead_rules[name]):
                new_element = (name,)+ tuple(a for a,b in zip(stack[-len(branch):],branch) if b[:1]+b[-1:] == "<>")
                possibles += list_possibles(stack[:-len(branch)] + [new_element], grammar, lookahead_rules)
            if not not_dead_end and any(full_match(stack, branch[:part], lookahead_rules[name]) for part in range(1, len(branch))):
                not_dead_end = True

    return possibles + [stack] if not_dead_end else possibles

def postprocess(syntax):
    invisibles = ["#", "conjunctionprefix", "constraint", "disjunctionprefix", "renameprefix", "slot", "openslot",
                  "slottupleprefix", "calculation", "arginheritprefix", "rawinherit", "arginherit", "grouping"]

    if type(syntax) == tuple:
        result = tuple(s for a in map(postprocess, syntax) for s in a)
        return result[1:] if result[0] in invisibles else (result,)
    else:
        return (syntax,)

def split_list(lst, delimeter):
    return [list(value) for flag, value in itertools.groupby(lst, (lambda x: x == delimeter)) if flag == False]

def iterate_to_fixpoint(function, start, *extra):
    start, result = None, start
    while result != start:
        start, result = result, function(result, *extra)
    return result

def collect_aliases(aliases, grammar):
    return aliases | {"<{}>".format(name) for name,rule in grammar if any(branch[0] in aliases for branch in rule)}

def parse(code, grammartext):
    grammar = split_list(grammartext.split()+[""], "::=")
    grammar = [(a[-1], b[:-1]) for a,b in zip(grammar[:-1], grammar[1:])]

    for i,(name,rule) in enumerate(grammar[::1]):
        if "{" in rule and "}" in rule:
            prefix     = rule[:rule.index("{")]
            core       = rule[rule.index("{")+1:rule.index("}")]
            suffix     = rule[rule.index("}")+1:]
            grammar[i] = (name, ["<{}prefix>".format(name)] + suffix)

            grammar.append(("{}prefix".format(name), prefix + ["|", "<{}prefix>".format(name)] + core))

    grammar = [(name, [[t.strip("'") for t in value] for value in split_list(rule, "|")]) for name, rule in grammar]
    aliases = {name:iterate_to_fixpoint(collect_aliases, {"<{}>".format(name)}, grammar) for name,rules in grammar}

    lookahead_rules = {name:set() for name,rule in grammar}
    for name,_ in grammar:
        precursors = [branch[i] for _,rule in grammar for branch in rule for i,b in enumerate(branch[1:]) if b in aliases[name]]
        lookahead_rules[name] = set() if "<s>" in precursors or "<n>" in precursors else set(precursors)

    code = " ".join(line.split("#")[0] for line in code.split("\n"))
    code = "".join(" "+c+" " if c in "(){}[]=.+-," else c for c in code).replace(".  .", "..")

    possibles = [[]]

    debug_tokens = []
    for token in [int(token) if token.isdigit() else token for token in code.split() if token]:
        possibles = sum([list_possibles(pos+[token], grammar, lookahead_rules) for pos in possibles], [])

        debug_tokens = debug_tokens[-7:]+[str(token)]

        if not possibles:
            raise Exception("Syntax error at \""+" ".join(debug_tokens)+"\"")

    possibles = [pos for pos in possibles if all(type(s) == tuple and s[0] == "#" for s in pos)]

    if len(possibles) == 1:
        return tuple(s for a in map(postprocess, possibles[0]) for s in a)

def partial_evaluator(syntax, handler, *extras):
    handler_result = handler(syntax, *extras)
    if handler_result:
        return handler_result
    return tuple(partial_evaluator(s, handler, *extras) if type(s) is tuple else s for s in syntax)

def rebase(prefix, suffix):
    if not prefix:
        return suffix
    elif suffix[0] == "slotrange":
        return (suffix[0], rebase(prefix, suffix[1]), suffix[2], suffix[3])
    elif suffix[0] in ["slotmember", "slotindex"]:
        return (suffix[0], rebase(prefix, suffix[1]), suffix[2])
    elif suffix[0] == "slotbase":
        return ("slotmember", prefix, suffix[1])
    raise Exception("Error, \"" + suffix[0] + "\" is not allowed in suffix.")

def evaluate_remove_rename_rebase(syntax, renames={}, prefix=None):
    if syntax[0] in ["rebase", "rename"]:
        child      = syntax[1]  if syntax[0] == "rename" else syntax[1][1] if syntax[1][0] == "rename" else syntax[1]
        newprefix  = None       if syntax[0] == "rename" else syntax[-1]
        renamevars = syntax[2:] if syntax[0] == "rename" else syntax[1][2:] if syntax[1][0] == "rename" else []
        newrenames = dict(zip(renamevars[1::2], renamevars[0::2]))

        return partial_evaluator(partial_evaluator(child, evaluate_remove_rename_rebase, newrenames, newprefix),
                                                          evaluate_remove_rename_rebase, renames, prefix)

    elif syntax[0] in ["slotmember", "slotindex"]:
        if syntax in renames:
            return renames[syntax]
        else:
            return (syntax[0], partial_evaluator(syntax[1], evaluate_remove_rename_rebase, renames, prefix), syntax[2])

    elif syntax[0] == "slotbase":
        if syntax in renames:
            return renames[syntax]
        else:
            return rebase(prefix, syntax)

def evaluate_remove_for_with(syntax, specs, vardict={}, collectvars=[]):
    if syntax[0] == "inheritance":
        new_vardict = {}
        for i in range(2,len(syntax),2):
            rangevalue = partial_evaluator(syntax[i+1], evaluate_remove_for_with, specs, vardict, collectvars)

            if rangevalue[0] == "baseconst":
                new_vardict[syntax[i]] = rangevalue[1]
            else:
                raise Exception("Free variables remain in for assignment.")

        if not new_vardict:
            new_vardict = vardict

        return partial_evaluator(specs[syntax[1]], evaluate_remove_for_with, specs, new_vardict, collectvars)

    elif syntax[0] in ["forall", "forsome"]:
        rangestart = partial_evaluator(syntax[3], evaluate_remove_for_with, specs, vardict, collectvars)
        rangestop  = partial_evaluator(syntax[4], evaluate_remove_for_with, specs, vardict, collectvars)

        if rangestart[0] == rangestop[0] == "baseconst":

            branches = []

            for i in range(rangestart[1],rangestop[1]):

                new_vardict = dict(vardict)
                new_vardict[syntax[2]] = i
                branches.append(partial_evaluator(syntax[1], evaluate_remove_for_with, specs, new_vardict, collectvars))

            return ("conjunction" if syntax[0] == "forall" else "disjunction",)+tuple(branches)
        raise Exception("Free variables remain in for loop range start.")

    elif syntax[0] == "forone":
        rangevalue = partial_evaluator(syntax[3], evaluate_remove_for_with, specs, vardict, collectvars)

        if rangevalue[0] == "baseconst":

            new_vardict = dict(vardict)
            new_vardict[syntax[2]] = rangevalue[1]
            return partial_evaluator(syntax[1], evaluate_remove_for_with, specs, new_vardict, collectvars)
        raise Exception("Free variables remain in for assignment.")

    elif syntax[0] == "if":
        leftvalue  = partial_evaluator(syntax[1], evaluate_remove_for_with, specs, vardict, collectvars)
        rightvalue = partial_evaluator(syntax[2], evaluate_remove_for_with, specs, vardict, collectvars)

        if leftvalue[0] == "baseconst" and rightvalue[0] == "baseconst":
            if leftvalue[1] == rightvalue[1]:
                return partial_evaluator(syntax[3], evaluate_remove_for_with, specs, vardict, collectvars)
            else:
                return partial_evaluator(syntax[4], evaluate_remove_for_with, specs,vardict, collectvars)
        raise Exception("Free variables remain in conditional.")

    elif syntax[0] == "default":
        defaultvalue = partial_evaluator(syntax[3], evaluate_remove_for_with, specs, vardict, collectvars)

        if syntax[2] in vardict or syntax[2] in collectvars:
            return partial_evaluator(syntax[1], evaluate_remove_for_with, specs, vardict, collectvars)
        elif defaultvalue[0] == "baseconst":
            new_vardict = dict(vardict)
            new_vardict[syntax[2]] = defaultvalue[1]
            return partial_evaluator(syntax[1], evaluate_remove_for_with, specs, new_vardict, collectvars)
        raise Exception("Free variables remain in default value.")

    elif syntax[0] == "collect":
        new_collectvars = collectvars[:] + [syntax[1]]
        return syntax[:3] + (partial_evaluator(syntax[3], evaluate_remove_for_with, specs, vardict, new_collectvars),)

    elif syntax[0] == "basevar":
        if syntax[1] in vardict:
            return ("baseconst", vardict[syntax[1]])
        elif syntax[1] in collectvars:
            return syntax
        raise Exception("Free variable \""+syntax[1]+"\" remain in static calculation.")

    elif syntax[0] in ["addvar", "subvar"]:
        if syntax[2] in vardict:
            return evaluate_remove_for_with((syntax[0][:3]+"const", syntax[1], vardict[syntax[2]]), specs, vardict)
        raise Exception("Free variables remain in static calculation of "+syntax[0]+".")

    elif syntax[0] in ["addconst", "subconst"]:
        leftvalue = evaluate_remove_for_with(syntax[1], specs, vardict)

        if leftvalue[0] == "baseconst":
            return (leftvalue[0], leftvalue[1] + syntax[2] * {"add":+1,"sub":-1}[syntax[0][:-5]])
        else:
            raise Exception("Free variables remain in static calculation.")

def evaluate_remove_trivials(syntax):
    if syntax[0] in ["conjunction", "disjunction"]:
        is_trivial = False
        result     = syntax[:1]
        for s in [partial_evaluator(s, evaluate_remove_trivials) for s in syntax[1:]]:
            if s[0] == {"con":"false","dis":"true"}[syntax[0][:3]]:
                is_trivial = True
            elif s[0] != {"con":"true","dis":"false"}[syntax[0][:3]]:
                result = result + (s,)
        if is_trivial:
            return ({"con":"false","dis":"true"}[syntax[0][:3]],)
        elif len(result) == 1:
            return ({"con":"true","dis":"false"}[syntax[0][:3]],)
        else:
            return result if len(result) > 2 else result[1]

def evaluate_flatten_connectives(syntax):
    if syntax[0] in ["conjunction", "disjunction"]:
        result = syntax[:1]

        for child in (partial_evaluator(s, evaluate_flatten_connectives) for s in syntax[1:]):
            if child[0] == syntax[0]:
                result = result + child[1:]
            else:
                result = result + (child,)
        return result

def replace_variables(syntax, replaces):
    if syntax[0] in ["slotbase", "slotmember", "slotindex"]:
        return replaces[syntax] if syntax in replaces else syntax

def optimize_delay_aliases(syntax, slotlist):
    if syntax[0] == "conjunction":
        replaces = {}
        for a,b in (s[1][1:] for s in syntax[1:] if s[0] == "atom" and s[1][0] == "ConstraintSame"):
            aflat, bflat = generate_cpp_slot(a), generate_cpp_slot(b)
            if aflat in slotlist and bflat in slotlist:
                if slotlist.index(aflat) < slotlist.index(bflat):
                    replaces[b] = a
                if slotlist.index(bflat) < slotlist.index(aflat):
                    replaces[a] = b

        return syntax[:1] + tuple(s if s[0] == "atom" and s[1][0] == "ConstraintSame" else
                                  partial_evaluator(
                                  partial_evaluator(s, replace_variables,      replaces),
                                                       optimize_delay_aliases, slotlist) for s in syntax[1:])

def indent_code(prefix, code):
    current_indent = 0
    while code[:1] == " " and current_indent < len(prefix):
        code = code[1:]
        current_indent += 1

    return prefix + code.replace("\n", "\n"+" "*(len(prefix) - current_indent))

def generate_cpp_type(syntax):
    if syntax[0] in ["conjunction", "disjunction"]:
        return {"conjunction":"ConstraintAnd", "disjunction":"ConstraintOr"}[syntax[0]]
    elif syntax[0] == "atom":
        return syntax[1][0]
    raise Exception("Error, \"" + syntax[0] + "\" is not allowed in type generator.")

def generate_cpp_slot(syntax):
    if syntax[0] == "slotbase":
        return syntax[1]
    elif syntax[0] == "slotmember":
        return generate_cpp_slot(syntax[1]) + "." + syntax[2]
    elif syntax[0] == "slotindex" and syntax[2][0] in ["baseconst", "basevar"]:
        return generate_cpp_slot(syntax[1]) + "[" + str(syntax[2][1]) + "]"
    raise Exception("Error, \"" + syntax[0] + "\" is not allowed in single slot.")

def generate_cpp_slotlist(syntax):
    if syntax[0] == "slotbase":
        return (syntax[1],)
    elif syntax[0] == "slotmember":
        return tuple(prefix+"."+syntax[2] for prefix in generate_cpp_slotlist(syntax[1]))
    elif syntax[0] == "slotindex" and syntax[2][0] in ["baseconst", "basevar"]:
        return tuple(prefix+"["+str(syntax[2][1])+"]" for prefix in generate_cpp_slotlist(syntax[1]))
    elif syntax[0] == "slotrange" and syntax[2][0] == "baseconst" and syntax[3][0] == "baseconst":
        return tuple(prefix+"["+str(i)+"]" for prefix in generate_cpp_slotlist(syntax[1]) for i in range(syntax[2][1], syntax[3][1]))
    elif syntax[0] == "slottuple":
        return sum((generate_cpp_slotlist(s) for s in syntax[1:]), ())
    raise Exception("Error, \"" + syntax[0] + "\" is not allowed in slot list.")

def getatom(counter, typename):
    if typename in counter:
        result = "atom{}_[{}]".format(counter[typename][0], counter[typename][1])
        counter[typename] = (counter[typename][0], counter[typename][1]+1)
    else:
        result = "atom{}_[{}]".format(len(counter), 0)
        counter[typename] = (len(counter), 1)
    return result;

def code_generation_core(syntax, counter):
    if syntax[0] == "atom":
        classname = "Backend{}".format(syntax[1][0][10:])
        if syntax[1][0][10:13] in ["DFG","CFG","PDG"] and syntax[1][0][13:20] in ["Dominat","Postdom"]:
            atom = getatom(counter, "my_shared_ptr<{}>".format(classname))
            code = "{} = {{{{0,1,1}}, wrap}};\n".format(atom)
        elif syntax[1][0][10:13] in ["DFG","CFG","PDG"] and syntax[1][0][13:20] == "Blocked":
            atom = getatom(counter, "my_shared_ptr<{}>".format(classname))
            code = "{} = {{{{1,1,1}}, wrap}};\n".format(atom)
        elif len(syntax[1]) > 2:
            atom = getatom(counter, "my_shared_ptr<{}>".format(classname))
            code = "{} = {{wrap}};\n".format(atom)
        else:
            if classname not in counter:
                atom = getatom(counter, classname)
                code = "{} = {{wrap}};\n".format(atom)
            else:
                atom = "atom{}_[0]".format(counter[classname][0])
                code = ""
#            atom = getatom(counter, "Backend{}".format(syntax[1][0][10:]))
#            code = "{} = {{wrap}};\n".format(atom)

        slots = [generate_cpp_slot(s) for s in syntax[1][1:2]+syntax[1][3:1:-1]]

        if syntax[1][0][10:13] in ["DFG","CFG","PDG"] and syntax[1][0][13:20] in ["Dominat","Postdom"]:
            result = {slot:("MultiVectorSelector<Backend{},{}>".format(syntax[1][0][10:], i+1), "{}, <[0]>".format(atom)) for i,slot in enumerate(slots)}
        elif syntax[1][0][10:13] in ["DFG","CFG","PDG"] and syntax[1][0][13:20] == "Blocked":
            result = {slot:("MultiVectorSelector<Backend{},{}>".format(syntax[1][0][10:], i), "{}, <[0]>".format(atom)) for i,slot in enumerate(slots)}
        elif len(syntax[1]) == 2:
            result = {slot:("Backend{}".format(syntax[1][0][10:]), atom) for i,slot in enumerate(slots)}
        else:
            result = {slot:("ScalarSelector<Backend{},{}>".format(syntax[1][0][10:], i), atom) for i,slot in enumerate(slots)}

        return slots,result,code

    elif syntax[0] == "GeneralizedDominance":
        atom        = getatom(counter, "my_shared_ptr<BackendPDGDominate>")
        slotlists   = [generate_cpp_slotlist(s) for s in syntax[1:2]+syntax[3:1:-1]]
        code        = "{} = {{{{{},{},{}}}, wrap}};\n".format(atom, len(slotlists[0]), len(slotlists[1]), len(slotlists[2]))
        slots       = [slot for slotlist in slotlists for slot in slotlist]
        slots       = [slot for n,slot in enumerate(slots) if slot not in slots[:n]]
        result      = {slot:("MultiVectorSelector<BackendPDGDominate,{}>".format(j), "{}, <[{}]>".format(atom, i))
                       for j,slotlist in enumerate(slotlists) for i,slot in enumerate(slotlist)}
        return slots,result,code

    elif syntax[0] == "GeneralizedSame":
        atom        = getatom(counter, "my_shared_ptr<BackendSameSets>")
        slotlists   = [generate_cpp_slotlist(s) for s in syntax[1:2]+syntax[3:1:-1]]
        code        = "{} = {{{{{}}}}};\n".format(atom, len(slotlists[0]))
        slots       = [slot for slotlist in slotlists for slot in slotlist]
        slots       = [slot for n,slot in enumerate(slots) if slot not in slots[:n]]
        result      = {slot:("MultiVectorSelector<BackendSameSets,{}>".format(j), "{}, <[{}]>".format(atom, i))
                       for j,slotlist in enumerate(slotlists) for i,slot in enumerate(slotlist)}
        return slots,result,code

    elif syntax[0] in ["conjunction", "disjunction"]:
        part_results = [code_generation_core(s, counter) for s in syntax[1:]]
        code         = "".join([part[2] for part in part_results])
        slots        = [slot for part in part_results for slot in part[0]]
        slots        = [slot for n,slot in enumerate(slots) if slot not in slots[:n]]
        result       = {slot:[part[1][slot] for part in part_results if slot in part[1]] for slot in slots}

        if syntax[0] == "conjunction":
            for slot in slots:
                if len(result[slot]) == 1:
                    result[slot] = result[slot][0]
                else:
                    classname    = indent_code("BackendAnd<", ",\n".join(a for a,b in result[slot])+">")
                    atom         = getatom(counter, classname)
                    code        += "{} = {{{}}};\n".format(atom, ", ".join("{"+b+"}" for a,b in result[slot]))
                    classname    = "remove_reference<decltype({}[0])>::type".format(atom[:atom.index("[")])
                    result[slot] = (classname, atom)

        """
        if syntax[0] == "disjunction":
            choices        = max([0]+[len(result[slot]) for slot in slots])
            templateparams = ",\n".join("tuple<"+",".join(a for a,b in result[slot])+">" for slot in slots)
            classname      = indent_code("BackendOr<{},".format(choices), "{}>".format(templateparams))
            constructargs  = ", ".join("{"+", ".join("{"+b+"}" for a,b in result[slot])+"}" for slot in slots)
            atom           = getatom(counter, indent_code("my_shared_ptr<", "{}>".format(classname)))
            code          += "{} = {{{}}};\n".format(atom, constructargs)
            classname      = "remove_reference<decltype(*{}[0])>::type".format(atom[:atom.index("[")])
            result         = {slot:("ScalarSelector<{},{}>".format(classname, n), atom) for n,slot in enumerate(slots)}
"""
        if syntax[0] == "disjunction":
            choices        = max([0]+[len(result[slot]) for slot in slots])
            templateparams = ",\n".join("tuple<"+",".join("IndirSolverAtom" for a,b in result[slot])+">" for slot in slots)
            classname      = indent_code("BackendOr<{},".format(choices), "{}>".format(templateparams))
            constructargs  = ", ".join("{"+", ".join(a+"{"+b+"}" for a,b in result[slot])+"}" for slot in slots)
            atom           = getatom(counter, indent_code("my_shared_ptr<", "{}>".format(classname)))
            code          += "{} = {{{}}};\n".format(atom, constructargs)
            classname      = "remove_reference<decltype(*{}[0])>::type".format(atom[:atom.index("[")])
            result         = {slot:("ScalarSelector<{},{}>".format(classname, n), atom) for n,slot in enumerate(slots)}

        return slots,result,code

    elif syntax[0] == "collect":
        atom = getatom(counter, "my_shared_ptr<BackendCollect>")

        part_slots, part_result, code = code_generation_core(syntax[3], counter)

        local_slots_idx  = [i for i,slot in enumerate(part_slots) if "["+syntax[1]+"]"     in slot]
        global_slots_idx = [i for i,slot in enumerate(part_slots) if "["+syntax[1]+"]" not in slot]

        local_parts  = [part_result[part_slots[idx]] for idx in local_slots_idx]
        global_parts = [part_result[part_slots[idx]] for idx in global_slots_idx]

        mergedatom = atom[4:atom.index("[")] + atom[atom.index("[")+1:atom.index("]")] + "_"

        code += "\n".join(["vector<unique_ptr<SolverAtom>> globals{};".format(mergedatom)]
                         +["globals{}.emplace_back(unique_ptr<SolverAtom>(new {}({})));".format(mergedatom, part[0], part[1]) for part in global_parts]
                         +["vector<unique_ptr<SolverAtom>> locals{};".format(mergedatom)]
                         +["locals{}.emplace_back(unique_ptr<SolverAtom>(new {}({})));".format(mergedatom, part[0], part[1]) for part in local_parts]
                         +["{} = {{{{{{{},{}}}}}, move(globals{}), move(locals{})}};".format(atom, len(global_parts), len(local_parts), mergedatom, mergedatom)])+"\n"

        local_slots  = [part_slots[idx].replace("["+syntax[1]+"]", "["+str(n)+"]") for n in range(syntax[2]) for idx in local_slots_idx]
        global_slots = [part_slots[idx] for idx in global_slots_idx]

        result      = {slot:("MultiVectorSelector<BackendCollect,0>", "{}, <[{}]>".format(atom, i)) for i,slot in enumerate(global_slots)}
        result.update({slot:("MultiVectorSelector<BackendCollect,1>", "{}, <[{}]>".format(atom, i)) for i,slot in enumerate(local_slots)})

        return global_slots+local_slots, result, code

    raise Exception("Error, \"" + syntax[0] + "\" is not allowed in atoms collection.")

def postprocess_copyconstructions(code):
    atomic_def_lines = [line for line in code.split("\n") if line.startswith("atom")]
    code_atomic_defs = [(line,line.split("[")[0]+line.split(" = ")[1]) for line in atomic_def_lines]
    duplication_dict = {}

    for n,line in enumerate(code_atomic_defs):
        for oldline in code_atomic_defs[:n]:
            if oldline[1:] == line[1:]:
                duplication_dict[line[0]] = line[0].split(" = ")[0] + " = " + oldline[0].split(" = ")[0] + ";"
                break

    return "\n".join([duplication_dict[line] if line in duplication_dict else line for line in code.split("\n")])

def check_differences(first, second, third, fourth):
    diff1 = [y-x for line1,line2 in zip(first,second) for x,y in zip(line1,line2)]
    diff2 = [y-x for line1,line2 in zip(third,fourth) for x,y in zip(line1,line2)]

    return all([x==y for x,y in zip(diff1,diff2)])

# This does not actually check that indizes are a linear progression for more than two repetitions!
def loop_block(block, depth=0):
    if depth > 4: return block
    stripindizes = [line[::2] for line in block]
    onlyindizes  = [[int(n) for n in line[1::2]] for line in block]

    repeatstart  = 0
    repeatsize   = 1
    repeatamount = 1
    codesaved    = 0

    for n in range(len(stripindizes)):
        for m in range(1,1000):
            if stripindizes[n:n+m] == stripindizes[n+m:n+2*m]:
                newrepeatamount = 2
                for k in range(2,100):
                    if (stripindizes[n+(k-1)*m:n+k*m] == stripindizes[n+k*m:n+(k+1)*m] and
                        check_differences(onlyindizes[n+(k-2)*m:n+(k-1)*m], onlyindizes[n+(k-1)*m:n+(k+0)*m],
                                          onlyindizes[n+(k-1)*m:n+(k+0)*m], onlyindizes[n+(k+0)*m:n+(k+1)*m])):
                        newrepeatamount += 1
                    else:
                        break
                if (newrepeatamount-1)*m > codesaved:
                    repeatstart  = n
                    repeatsize   = m
                    repeatamount = newrepeatamount
                    codesaved    = (newrepeatamount-1)*m

    if codesaved:
        block_before = block[:repeatstart]
        block_after  = block[repeatstart+repeatamount*repeatsize:]

        modified_block = block[repeatstart:repeatstart+repeatsize]

        for k,line in enumerate(modified_block):
            for i in range(2,len(line),2):
                difference = int(block[repeatstart+repeatsize+k][i-1])-int(block[repeatstart+k][i-1])
                if difference > 1:
                    line[i] = "+{}*{}{}".format("ijklm"[depth], difference, line[i])
                elif difference < -1:
                    line[i] = "-{}*{}{}".format("ijklm"[depth], -difference, line[i])
                elif difference == 1:
                    line[i] = "+{}{}".format("ijklm"[depth], line[i])
                elif difference == -1:
                    line[i] = "-{}{}".format("ijklm"[depth], line[i])

        return (loop_block(block_before, depth)
               +[["for(unsigned {0} = 0; {0} < {1}; {0}++) {{".format("ijklm"[depth], repeatamount)]]
               +[["    "+line[0]]+line[1:] for line in loop_block(modified_block, depth+1)]
               +[["}"]]
               +loop_block(block_after, depth))

    return block

def postprocess_add_loops_one_block(block):
    block = [[part for word in line.split(']') for part in word.split('[')] for line in block]
    block = [line if len(line) <= 1 else [line[0]+"["]+line[1:-1]+["]"+line[-1]] for line in block]
    for line in block:
        if len(line) > 1:
            line[2:-2:2] = ["]"+e+"[" for e in line[2:-2:2]]
    block = loop_block(block)
    return ["".join(line) for line in block]

def postprocess_add_loops(code):
    grouped = itertools.groupby(code.split('\n'), (lambda x: x.startswith("atom")))
    grouped = [postprocess_add_loops_one_block(list(b)) if a else list(b) for a,b in grouped]
    return "\n".join(line for group in grouped for line in group)

def generate_fast_cpp_specification(syntax, specs):
    constr = partial_evaluator(syntax[2], evaluate_remove_for_with, specs)
    constr = partial_evaluator(constr,    evaluate_remove_rename_rebase)
    constr = partial_evaluator(constr,    evaluate_remove_trivials)
    constr = partial_evaluator(constr,    evaluate_flatten_connectives)

    atom_counter = {}
    slots, result, code = code_generation_core(constr, atom_counter)

    constr = partial_evaluator(constr, optimize_delay_aliases, slots)

    atom_counter = {}
    slots2, result, code = code_generation_core(constr, atom_counter)

    code = postprocess_copyconstructions(code)
    code = postprocess_add_loops(code)

    code = "\n".join(["{} atom{}_[{}];".format(typename, atom_counter[typename][0], atom_counter[typename][1]) for typename in atom_counter]
                    +["solver_timer2.startTimer();"]
                    +[code.rstrip()]
                    +["vector<pair<string,unique_ptr<SolverAtom>>> constraint({});".format(len(slots))]
                    +["solver_timer2.stopTimer();"]
                    +["solver_timer8.startTimer();"]
                    +["constraint[{}] = make_pair(\"{}\", unique_ptr<SolverAtom>(new {}({})));".format(n, slot, result[slot][0], result[slot][1]) for n,slot in enumerate(slots)]
                    +["solver_timer8.stopTimer();"])

    code = code.replace("<[", "").replace("]>", "")

    return "\n".join(["vector<Solution> Detect{}(llvm::Function& function, unsigned max_solutions)".format(syntax[1])]
                    +["{"]
                    +["    solver_timer1.startTimer();"]
                    +["    FunctionWrap wrap(function);"]
                    +["    solver_timer1.stopTimer();"]
                    +[indent_code("    ", code)]
                    +["    solver_timer3.startTimer();"]
                    +["    auto result = Solution::Find(move(constraint), function, max_solutions);"]
                    +["    solver_timer3.stopTimer();"]
                    +["    return result;"]
                    +["}"])

def generate_cpp_code(syntax_list):
    includes  = ["IdiomSpecifications","BackendSpecializations", "BackendDirectClasses", "BackendSelectors", "Solution"]
    specs     = {spec[1] : spec[2] for spec in syntax_list}
    whitelist = ["Distributive", "HoistSelect", "AXPYn", "GEMM", "GEMV", "AXPY",
                 "DOT", "SPMV", "Reduction", "Histo", "Stencil", "StencilPlus", "Experiment"]

    return "\n".join(["#include \"llvm/Constraints/{}.hpp\"".format(s) for s in includes]
                    +["using namespace std;"]
                    +["#pragma GCC optimize (\"O0\")"]
                    +["#pragma clang optimize off"]
                    +["llvm::Timer solver_timer1 = llvm::Timer();"]
                    +["llvm::Timer solver_timer2 = llvm::Timer();"]
                    +["llvm::Timer solver_timer3 = llvm::Timer();"]
                    +["llvm::Timer solver_timer8 = llvm::Timer();"]
                    +["template<typename T>"]
                    +["class my_shared_ptr : public shared_ptr<T>"]
                    +["{"]
                    +["public:"]
                    +["    my_shared_ptr() = default;"]
                    +["    my_shared_ptr<T>& operator=(T t) { shared_ptr<T>::operator=(make_shared<T>(move(t))); return *this; }"]
                    +["    my_shared_ptr<T>& operator=(const my_shared_ptr<T>& t) { return *this = *t; }"]
                    +["};"]
                    +[generate_fast_cpp_specification(syntax, specs) for syntax in syntax_list if syntax[1] in whitelist])

def print_syntax_tree(syntax, indent=0):
    if type(syntax) is str or type(syntax) is int:
        print("| "*indent+str(syntax))
    elif type(syntax) is tuple and type(syntax[0]) is str:
        print("| "*indent+str(syntax[0]))
        for s in syntax[1:]:
            print_syntax_tree(s, indent+1)
    elif type(syntax) is tuple:
        for s in syntax:
            print_syntax_tree(s, indent+1)

import sys
import itertools

print(generate_cpp_code(parse(sys.stdin.read(), grammar)))

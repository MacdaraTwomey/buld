
struct cmd_variable {
    string Name;
    string Value;
};

typedef array(cmd_variable) cmd_variable_array;

static cmd_variable_array GlobalVariables = {};

void DefineVariable(string Name, string Value) {
    cmd_variable Var = {};
    Var.Name = Name;
    Var.Value = Value;
    ArrayAdd(&GlobalVariables, Var);
}

struct get_variable_result {
    string Value;
    bool Found;
};

get_variable_result GetVariable(cmd_variable *Variables, u64 VariableCount, string Name) {
    get_variable_result Result = {};
    for (u64 i = 0; i < VariableCount; i += 1) {
        cmd_variable Var = Variables[i];
        if (StringMatch(Var.Name, Name)) {
            Result.Value = Var.Value;
            Result.Found = true;
            break;
        }
    }

    if (!Result.Found) {
        for (u64 i = 0; i < GlobalVariables.Count; i += 1) {
            cmd_variable Var = GlobalVariables.Data[i];
            if (StringMatch(Var.Name, Name)) {
                Result.Value = Var.Value;
                Result.Found = true;
                break;
            }
        }
    }

    return Result;
}

string ReplaceVars(string String, cmd_variable *Variables, u64 VariableCount) {
    u64 VariablesLength = 0;
    for (u64 i = 0; i < VariableCount; i += 1) {
        VariablesLength += Variables[i].Value.Length;
    }

    // TODO: Find a better way
    for (u64 i = 0; i < GlobalVariables.Count; i += 1) {
        VariablesLength += GlobalVariables.Data[i].Value.Length;
    }

    u8 *Buffer = (u8 *)calloc(VariablesLength + String.Length, 1);
    u64 Length = 0;
    for (u64 i = 0; i < String.Length; i += 1) {
        u8 c = String.Str[i];
        if (c == '{') {
            u64 CloseBraceIndex = StringFindChar(String, '}', i);
            if (CloseBraceIndex < String.Length) {
                string VariableName = SubstrRange(String, i + 1, CloseBraceIndex);
                get_variable_result Variable = GetVariable(Variables, VariableCount, VariableName);
                if (Variable.Found) {
                    memcpy(Buffer + Length, Variable.Value.Str, Variable.Value.Length);
                    Length += Variable.Value.Length;
                    i = CloseBraceIndex;
                    continue;
                }
                else {
                    fprintf(stdout, "Error: Undefined variable %.*s\n", StrArg(VariableName));
                }
            }
        }
        Buffer[Length++] = c;
    }

    return CreateString(Buffer, Length);
}

struct arg_parser {
    u64 At;
    string String;
    bool Error;
    bool Done;
};

string ParseArg(arg_parser *Parser) {

    if (Parser->At == Parser->String.Length) {
        Parser->Done = true;
    }

    if (Parser->Done) {
        return {};
    }

    u8 QuoteChar = 0;
    u64 Start = Parser->String.Length;
    u64 End = Parser->String.Length;
    for (; Parser->At < Parser->String.Length; Parser->At += 1) {
        u8 C = Parser->String.Str[Parser->At];
        if (C != ' ') {
            Start = Parser->At;
            if (C == '\'' || C == '"') {
                QuoteChar = C;
                Parser->At += 1;
            }
            break;
        }
    }

    bool QuotesClosed = (QuoteChar == 0);
    for (; Parser->At < Parser->String.Length; Parser->At += 1) {
        u8 C = Parser->String.Str[Parser->At];
        if (QuoteChar) {
            // TODO: Handle escaping
            if (C == QuoteChar) {
                Parser->At += 1;
                End = Parser->At;
                QuotesClosed = true;
                break;
            }
        }
        else if (C == ' ') {
            End = Parser->At;
            break;
        }
    }

    string Arg = {};

    if (QuotesClosed) {
        Arg = SubstrRange(Parser->String, Start, End);
    }
    else {
        Parser->Error = true;
        Parser->Done = true;
    }

    return Arg;
}

list<string> ParseArgString(string String) {
    list<string> ArgList = {};

    arg_parser Parser = {};
    for (string Arg = ParseArg(&Parser); !Parser.Done; Arg = ParseArg(&Parser)) {
        ListAdd(&ArgList, Arg);
    }
    
    return ArgList;
}
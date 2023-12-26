#define MAZ_REP_CTX "Savefile report"
#include <mz_rep.h>
#include <save.h>
#include <fstream>

static MZMesg r(MAZ_REP_CTX);
#define ERR(...) { MZErr_(__VA_ARGS__).p(); return 0; } 
#define ERRF(...) { MZErrF_(__VA_ARGS__).p(); return 0; } 

bool deserialize(std::string file, std::function<void(std::string &name, std::string &key, std::string &value)> cb){
#define LINE(fmt, ...) ERRF("%mWhile parsing config file %G<\n%cIn line %B[: %Y'\n%r" fmt, file, line_num, line, __VA_ARGS__)
    std::ifstream save(file);
    std::string line, identifier, key, value;
    int line_num = 0;

    enum KEY {
        Identifier,
        LBrace,
        RBrace,
    };
    KEY exp_key = Identifier;

    auto skip_spaces = [](std::string &str) -> const char *{
        int idx = 0;
        for(; idx != str.size(); idx++){ if(!isspace(str[idx]))break; }
        return str.c_str() + idx;
    };

    if(!save.is_open())return 0;

    while(1){
        line_num++;
        if(std::getline(save, line).eof()){
            if(exp_key == Identifier)break;
            LINE("Expecting a %G'. Got to end of file", "brace");
        }
        if(line.empty())continue;

        switch(exp_key){
            case Identifier:
                if(isspace(line[0]))break;
                identifier = line;
                exp_key = LBrace;
                break;
            case LBrace:
                if(line[0] != '{')LINE("Expecting a brace %G(", "'{'");
                exp_key = RBrace;
                break;
            case RBrace: {
                if(line[0] == '}'){
                    exp_key = Identifier;
                    break;
                }
                const char *_key = skip_spaces(line);
                int idx = 0;
                while(_key[idx]){
                    if(isspace(_key[idx]))break;
                    idx++;
                }
                key = std::string(_key, idx);
                value = std::string(_key + idx);
                cb(identifier, key, value);
                break;
             }
        }
    }
    return 1;
}

#undef ERR
#undef ERRF
#define ERR(...) { MZErr_(__VA_ARGS__).p(); return;  } 
#define ERRF(...) { MZErrF_(__VA_ARGS__).p(); return; } 
void serialize(std::string file, std::function<std::string(int)> cb){
    std::ofstream save(file);
    if(!save.is_open())return;

    for(int i = 0; i != 16; i++){
        save << "Channel " << (i + 1) << NL;
        save << "{\n";
        save << cb(i);
        save << "}\n";
    }
}

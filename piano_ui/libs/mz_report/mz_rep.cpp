#include <mz_rep.h>
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>

static HANDLE hTerm;
static uint16_t clear_color;

static void init_color_term(){
    static bool setuped = 0;
    if(setuped)return;

    hTerm = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO consoleinfo;
    
    /* save current attributes */
    GetConsoleScreenBufferInfo(hTerm, &consoleinfo);
    clear_color = consoleinfo.wAttributes;

    setuped = 1;
}

static void set_term_color(MZ::MZColor color){
    #define b FOREGROUND_INTENSITY
    #define t(color) SetConsoleTextAttribute(hTerm, color)

    switch(color){
        case MZ::Red:     t(b | FOREGROUND_RED); break;
        case MZ::Blue:    t(b | FOREGROUND_BLUE); break;
        case MZ::Green:   t(b | FOREGROUND_GREEN); break;
        case MZ::Yellow:  t(b | FOREGROUND_GREEN | FOREGROUND_RED); break;
        case MZ::Cyan:    t(b | FOREGROUND_BLUE | FOREGROUND_GREEN); break;
        case MZ::Mag:     t(b | FOREGROUND_RED | FOREGROUND_BLUE); break;
        case MZ::White:   t(b | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); break;
        case MZ::Grey:    t(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); break;
        case MZ::Purp:    t(FOREGROUND_RED | FOREGROUND_BLUE); break;
        case MZ::Clear:   t(clear_color); break;
    }
}
#else
static uint16_t clear_color;

static void init_color_term(){
    static bool setuped = 0;
    if(setuped)return;
    setuped = 1;
}

static void set_term_color(MZ::MZColor color){
    #define N(c) std::cout << "\e[0;" #c "m"
    #define B(c) std::cout << "\e[1;" #c "m"

    //std::cout << "\e[";
    switch(color){
        case MZ::Red:    N(31); break; 
        case MZ::Blue:   N(34); break; 
        case MZ::Green:  N(32); break; 
        case MZ::Yellow: N(33); break; 
        case MZ::Cyan:   N(36); break; 
        case MZ::Purp:   N(35); break; 
        case MZ::White:  N(37); break; 
        case MZ::Grey:   N(37); break; 
        case MZ::Clear:  N(0); break; 

        case MZ::Mag:    B(95); break; 
    }
    //std::cout << "m";
}

#endif

void MZStream::term_begin(){
    init_color_term();
}

MZStream &MZStream::operator<<(MZ::MZColor &&arg){
    color_changes.push_back({
        .color = arg,
        .pos   = (uint32_t)str_msg.str().size()
    });
    return *this;
}

MZStream &MZStream::operator<<(MZ::MZColor &arg){
    color_changes.push_back({
        .color = arg,
        .pos   = (uint32_t)str_msg.str().size()
    });
    return *this;
}

void MZStream::p(void){
    str_msg << NL;

    std::string str_obj = str_msg.str();
    const char *start = str_obj.c_str();
    const char *str = start;

    set_term_color(MZ::Clear);
    for(auto &cursor : color_changes){
        const char *new_str = start + cursor.pos;
        uint32_t size = new_str - str;
        std::cout << std::string(str, size);
        set_term_color(cursor.color);
        str = new_str;
    }

    std::cout << str;
    str_msg.str("");
    str_msg.seekg(0, std::ios::beg);
    color_changes.clear();

    set_term_color(MZ::Clear);
}

void _MZErr::p(void){
    for(auto &cursor : color_changes){
        if(cursor.color == MZ::Clear)cursor.color = MZ::Red;
    }

    MZStream::p();
}

void _MZWarn::p(void){
    for(auto &cursor : color_changes){
        if(cursor.color == MZ::Clear)cursor.color = MZ::Red;
    }

    MZStream::p();
}

void MZMesg::p(void){
    str_msg << NL;

    set_term_color(MZ::Mag);
    std::cout << ctx << ' ';
    set_term_color(MZ::Blue);
    std::cout << "[Message]:\n";

    MZStream::p();
}

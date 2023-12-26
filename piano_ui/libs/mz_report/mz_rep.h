/*
  David Rubio
 
  Error/Msg/Warning reporting made simpler (with color!!!)
  17/05/23
*/

#ifndef MAZ_REP_H
#define MAZ_REP_H

#include <string>
#include <vector>
#include <sstream>
#include <cstdint>

#ifndef MAZ_REP_CTX
#define MAZ_REP_CTX ""
#endif

#define NL '\n'

namespace MZ {
    enum MZColor : uint8_t {
        Red,
        White,   //RED WHITE, COMO, COMO *hace pitchuun*
        Blue,
        Green,
        Yellow,
        Cyan,
        Mag,
        Grey,
        Purp,
        Clear,
    };
}

enum class MZPrintf { Is_printf };

struct maz_color_pos {
    MZ::MZColor color;
    uint32_t pos;
};


class MZStream {
public:
    template<typename ...Args>
    MZStream(Args&& ...args){
        _default_color = MZ::Clear;
        term_begin();
        (_put(args), ...);
    }

    MZStream(void){
        _default_color = MZ::Clear;
       term_begin(); 
    }

    virtual void p(void);

    template<typename ...Args>
    void print(Args&& ...args){
        (_put(args), ...);
        p();
    }

    MZStream &operator<< (MZ::MZColor &&arg);
    MZStream &operator<< (MZ::MZColor &arg);

//good old c hacks
#ifdef MAZ_CUSTOM_TYPES
    MAZ_CUSTOM_TYPES
#endif

    template <typename T>
    MZStream &operator<< (T &arg){
        str_msg << arg;
        return *this;
    }

    template <typename T>
    MZStream &operator<< (T &&arg){
        str_msg << arg;
        return *this;
    }

    template <typename ...Args>
    void put(Args&& ...args){ (_put(args), ...); }

    template <typename ...T>
    void printf(std::string fmt, T... args){
        const char *rest = fmt.c_str();
        const char *end = rest + fmt.size();
        int dummy;

        _prev_color = _default_color;
        (_printf_put(args, &rest, end), ...);
        if(rest == end)return;
        _printf_put(dummy, &rest, end);
    }

protected:
    template<typename T>
    void _put(T v){ (*this) << v; }

    template <typename T>
    void _printf_put(T &value, const char **rest, const char *end){
        const char *next_pos = *rest;

        auto P = [this, &value, &rest, &next_pos](MZ::MZColor c){
            (*this) << std::string(*rest, next_pos - *rest - 1) << c;

            next_pos++;
            char surround = *next_pos;
            switch(surround) {
                case '\'': (*this) << '"' << value << '"'; break;
                case '(': (*this) << '(' << value << ')'; break;
                case '{': (*this) << '{' << value << '}'; break;
                case '[': (*this) << '[' << value << ']'; break;
                case '<': (*this) << '<' << value << '>'; break;
                default: (*this) << value; next_pos--; break;
            }

            (*this) << _prev_color;
            *rest = next_pos + 1;
        };

        auto C = [this, &rest, &next_pos](MZ::MZColor c){
            (*this) << std::string(*rest, next_pos - *rest - 1) << (_prev_color = c);
            *rest = next_pos + 1;
        };

        for(; next_pos != end - 1; next_pos++){
            bool quoted = false;
            if(*next_pos != '%')continue;
            next_pos++;

            switch(*next_pos){
                case 'R': P(MZ::Red); return;
                case 'G': P(MZ::Green); return;
                case 'B': P(MZ::Blue); return;
                case 'C': P(MZ::Cyan); return;
                case 'M': P(MZ::Mag); return;
                case 'W': P(MZ::White); return;
                case 'Y': P(MZ::Yellow); return;

                case 'r': C(MZ::Red); break;
                case 'g': C(MZ::Green); break;
                case 'b': C(MZ::Blue); break;
                case 'c': C(MZ::Cyan); break;
                case 'm': C(MZ::Mag); break;
                case 'w': C(MZ::White); break;
                case 'y': C(MZ::Yellow); break;
            }
        }
        (*this) << *rest;
    }


    void term_begin();
    std::stringstream str_msg;
    std::vector<maz_color_pos> color_changes;
    MZ::MZColor _prev_color;
    MZ::MZColor _default_color;
};

class MZMesg : public MZStream {
public:
    template<typename ...Args>
    MZMesg(const char *_ctx) : MZStream() { ctx = _ctx; }
    void p(void) override;

protected:
    const char *ctx;
};

class _MZErr : public MZStream {
public:
    template<typename ...Args>
    _MZErr(const char *ctx, const char *func, Args&& ...args) {
        MZStream::_default_color = MZ::Red;
        (*this) << MZ::Mag << ctx << ' ' << MZ::Red << "[ERROR]:" << NL;
        (*this) << "In Function: " << MZ::Green << func << MZ::Red << NL;
        
        term_begin();
        (_put(args), ...);

        (*this) << NL;
    }

    template<typename ...Args>
    _MZErr(MZPrintf &&format_flag, const char *ctx, const char *func, std::string fmt, Args&& ...args) {
        MZStream::_default_color = MZ::Red;
        (*this) << MZ::Mag << ctx << ' ' << MZ::Red << "[ERROR]:" << NL;
        (*this) << "In Function: " << MZ::Green << func << MZ::Red << NL;
        
        term_begin();
        MZStream::printf(fmt, args...);

        (*this) << NL;
    }

    void p(void) override;
};

class _MZWarn : public MZStream {
public:
    template<typename ...Args>
    _MZWarn(const char *ctx, const char *func, Args&& ...args){
        (*this) << MZ::Mag << ctx << ' ' << MZ::Yellow << "[Warning]:" << NL;
        (*this) << "In Function: " << MZ::Green << func << MZ::Yellow << NL;
        
        term_begin();
        (_put(args), ...);
        (*this) << NL;
    }

    void p(void) override;
    ~_MZWarn(){ p(); }
};

#define MZ_STR(x) MZ_STR2(x)
#define MZ_STR2(x) #x

#define MZ_HEADER MAZ_REP_CTX " (" __FILE_NAME__ ":" MZ_STR(__LINE__) ")"
#define MZErr(...) throw _MZErr(MZ_HEADER, __PRETTY_FUNCTION__, __VA_ARGS__)
#define MZErrF(...) throw _MZErr(MZPrintf::Is_printf, MZ_HEADER, __PRETTY_FUNCTION__, __VA_ARGS__)
#define MZErr_(...) _MZErr(MZ_HEADER, __PRETTY_FUNCTION__, __VA_ARGS__)
#define MZErrF_(...) _MZErr(MZPrintf::Is_printf, MZ_HEADER, __PRETTY_FUNCTION__, __VA_ARGS__)
#define MZWarn(...) _MZWarn(MZ_HEADER, __PRETTY_FUNCTION__, __VA_ARGS__)
#define Quote(col, exp) col, '"', exp, '"', MZ::Clear
#define _Quote(col, exp) col << '"' << exp << '"' << MZ::Clear
#define MZC(data, color) color, data, MZ::Clear
#define _MZC(data, color) color << data << MZ::Clear

//Useful hex formatting
#if __cplusplus >= 202002L
#include <format>
#define HEX(v) std::format("0x{:02x}", (uint32_t)v)
#define HEX8(v) std::format("0x{:016x}", (uint64_t)v)
#define HEX4(v) std::format("0x{:08x}", (uint32_t)v)
#define HEX2(v) std::format("0x{:04x}", (uint32_t)v)

#endif

#endif

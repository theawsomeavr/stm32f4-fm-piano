#define MAZ_REP_CTX "MIDI Parsee Report"
#include <mz_rep.h>
#include <colors.h>
#include <midi_parsee.h>
#include <cstring>
#include <fstream>

#define Err(...) { abort(); MZErr("In MIDI file: ", Quote(I1, _file), NL, Info, "Offset: ", _prev_cursor, MZ::Clear, "\n\n", __VA_ARGS__); }
#define TrErr(...) { _MZErr err_msg(MAZ_REP_CTX, __PRETTY_FUNCTION__, "In MIDI file: ", Quote(I1, _file), NL, Info, "Offset: ", _prev_cursor, ", Track: ", _track_num, MZ::Clear, "\n\n", __VA_ARGS__); abort(); throw err_msg; }
#define TrMsg(...) { r.print("In MIDI file: ", Quote(I1, _file), NL, Info, "Offset: ", _prev_cursor, ", Track: ", _track_num, MZ::Clear, "\n\n", __VA_ARGS__); }

static MZMesg r(MAZ_REP_CTX);

static const uint32_t MThd = *(uint32_t *)"dhTM";
static const uint32_t MTrk = *(uint32_t *)"krTM";

MIDIParsee::~MIDIParsee(){ abort(); }

void MIDIParsee::open(std::string file){
    std::ifstream f(file, std::ios::binary | std::ios::ate);

    if(!f.is_open())MZErr("Failed to open file ", Quote(I1, file)); 

    _song_len = 0;
    _song_pos = 0;
    _size = f.tellg();
    _data = (uint8_t *)malloc(_size);

    f.seekg(0);
    f.read((char *)_data, _size);
    f.close();
    _file = file;
    _is_ours = 1;
    mem_parse();
}

template <typename T>
T MIDIParsee::N(){
    T temp;
    _prev_cursor = _cursor;
    if(_cursor + sizeof(T) > _size)Err("Unexpected EOF");

    //MIDI files store data in Most Significant Byte first order. Why
    for(uint32_t i = 0; i != sizeof(T); i++) *((uint8_t *)&temp + (sizeof(T) - 1 - i)) = *(uint8_t *)(_data + _cursor + i);
    _cursor += sizeof(T);
    return temp;
}

void MIDIParsee::skip(uint32_t bytes){
    if(_cursor + bytes > _size)Err("Unexpected EOF");
    _cursor += bytes;
}

uint32_t MIDIParsee::midi_var(){
    uint32_t value = 0;

    for(uint8_t i = 0; i != 4; i++){
        uint8_t b = N<uint8_t>();
        value <<= 7;
        value |= b & 0x7f;
        if(!(b & 0x80))break;
    }
    return value;
}

void MIDIParsee::abort(){
    if(_is_ours){
        free(_data);
        _data = nullptr;
    }

    delete[] _MIDI_tracks;
    _MIDI_tracks = nullptr;
    _event_list.clear();
}

bool MIDIParsee::get_event(MIDIEvent &ev){
    if(_event_list.empty())return 0;
    if(_cur_event == (MIDIEvent *)&*_event_list.end())return 0;
    ev = *_cur_event;
    if(ev.is_delay())_song_pos += ev.ticks;
    _cur_event++;
    return 1;
}

void MIDIParsee::mem_parse(){
    uint16_t format;
    _us_per_1_4_note = 500000;
    _cursor = 0;
    _MIDI_tracks = nullptr;

    if(N<uint32_t>() != MThd)Err("Invalid MIDI file: ", I1, "No MThd at BOF");
    if(N<uint32_t>() != 6)Err("Invalid MIDI file: ", I1, "MThd header is not 6 bytes in length");

    format = N<uint16_t>();
    if(format == 2)Err("Parser does not support MIDI files of format 2");
    if(format > 2)Err("Invalid MIDI file format: ", Quote(I1, format));

    _tracks = N<uint16_t>();
    _division = N<uint16_t>();
    if(_division & 0x8000)Err("Parser does not support SMPTE format");
    _us_per_tick = (_us_per_1_4_note / (double)_division) / 1000.0;

    //Init tracks
    _MIDI_tracks = new MIDITrack[_tracks]; //Use new in order to call the string constructor
    for(uint16_t i = 0; i != _tracks; i++){
        if(N<uint32_t>() != MTrk)Err("Invalid MIDI track: ", I1, "No MTrk at chunk beggining");
        _MIDI_tracks[i].size = N<uint32_t>();
        _MIDI_tracks[i].ticks = 0;
        _MIDI_tracks[i].done = 0;
        _MIDI_tracks[i].waiting = 0;
        _MIDI_tracks[i].cursor = _cursor;
        _cursor += _MIDI_tracks[i].size;
    }
    
    _acc_delay = 0;

    r << "Format: " << format << NL;
    r << "Tracks: " << _tracks << NL;
    r << "Division: " << _division;
    r.p();

    while(!parse_tracks());
    if(_is_ours){
        free(_data);
        _data = nullptr;
    }

    delete[] _MIDI_tracks;
    _MIDI_tracks = nullptr;

    _cur_event = (MIDIEvent *)&_event_list[0];
}

void MIDIParsee::rewind(){
    _cur_event = (MIDIEvent *)&_event_list[0];
    _song_pos = 0;
}

bool MIDIParsee::parse_tracks(){
    uint32_t min_delay = -1;

    //Exec Phase
    for(_track_num = 0; _track_num != _tracks; _track_num++){
        MIDITrack &track = _MIDI_tracks[_track_num];
        if(track.done)continue;
        if(track.ticks){
            min_delay = std::min(min_delay, track.ticks);
            continue;
        }

        _cursor = track.cursor;
        while(1){
            if(!track.waiting){
                track.ticks = midi_var();
                if(track.ticks){
                    min_delay = std::min(min_delay, track.ticks);
                    track.waiting = 1;
                    break;
                }
            }

            track.waiting = 0;
            parse_event(track);
            if(track.done)break;
        }

        track.cursor = _cursor;
    }
    if(min_delay == (uint32_t)-1)return 1;

    //Delay Phase
    _acc_delay += min_delay * _us_per_tick;
    for(_track_num = 0; _track_num != _tracks; _track_num++){
        MIDITrack &track = _MIDI_tracks[_track_num];
        if(track.done)continue;
        track.ticks -= min_delay;
    }

    return 0;
}

void MIDIParsee::parse_event(MIDITrack &tr){
    uint8_t ev = N<uint8_t>();

    if(!(ev & 0x80)){
        _cursor--;
        parse_MIDI_event(tr); //Running Status
        return;
    }
    switch(ev){
        //Ignore SysEX cmds
        case 0xF0:
        case 0xF7:
            skip(midi_var());
            break;
        case 0xFF:
            parse_meta_event(tr);
            break;
        default:
            tr.running_status = ev;
            parse_MIDI_event(tr);
    }
}

void MIDIParsee::parse_meta_event(MIDITrack &tr){
    uint8_t type = N<uint8_t>();
    uint32_t len = midi_var();
    uint8_t *data = _data + _cursor;
    skip(len);

    switch(type){
        case 0x03:
            tr.name = std::string((char *)data, len);
            r.print("Track #", _track_num, " name: ", Quote(I1, tr.name));
            break;
        case 0x51: //Tempo
            _us_per_1_4_note = data[2] | (data[1] << 8) | (data[0] << 16);
            _us_per_tick = (_us_per_1_4_note / (double)_division) / 1000.0;
            break;
        case 0x58: //Time signature
            break;
        case 0x2F:
            tr.done = 1;
            break;
        default: break;
    }
}

void MIDIParsee::parse_MIDI_event(MIDITrack &tr){
    uint8_t data[3] = { tr.running_status };
    uint8_t cmd = data[0] & 0xf0;
    uint8_t chan = data[0] & 0x0f;

    auto append = [this](MIDIEventType type, const void *data){
        MIDIEvent ev;
        ev.type = type;

        switch(type){
            case MIDIEventType::Delay:
                if(!((uint32_t)_acc_delay))return;
                ev.ticks = _acc_delay;
                _acc_delay -= (uint32_t)_acc_delay;
                _event_list.push_back(ev);
                _song_len += ev.ticks;
                break;
            case MIDIEventType::Cmd:
                ev.cmd = ((uint8_t *)data)[0];
                ev.data1 = ((uint8_t *)data)[1];
                ev.data2 = 0x00;
                if((ev.cmd & 0xf0) != 0xD0 && (ev.cmd & 0xf0) != 0xC0)ev.data2 = ((uint8_t *)data)[2];
                _event_list.push_back(ev);
                break;
        }
    };

    data[1] = N<uint8_t>();
    if(cmd != 0xC0 && cmd != 0xD0)data[2] = N<uint8_t>();

    switch(cmd){
        case 0x90:
            if(!data[2])data[0] = 0x80 | chan;
        case 0xC0:
        case 0x80:
        case 0xE0:
            append(MIDIEventType::Delay, nullptr);
            append(MIDIEventType::Cmd, data);
            break;
        default: break;
    }
}

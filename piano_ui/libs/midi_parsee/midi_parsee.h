#ifndef MIDI_PARSEE_H
#define MIDI_PARSEE_H

#include <string>
#include <vector>
#include <stdint.h>

enum class MIDIEventType : uint8_t {
    Delay,
    Cmd,
};

struct MIDIEvent {
    MIDIEventType type;
    union {
        struct {
            uint8_t cmd, data1, data2;
        };
        uint32_t ticks;
    };

    bool is_delay(){ return type == MIDIEventType::Delay; }

};

class MIDIParsee {
    struct MIDITrack {
        std::string name;
        uint32_t cursor, size, ticks;
        uint8_t running_status;
        bool done, waiting;
    };

public:
    ~MIDIParsee();
    void open(std::string file);
    void close(){ abort(); }
    void rewind();
    bool get_event(MIDIEvent &ev);
    bool is_open(){ return !_event_list.empty(); }
    uint32_t song_len(){ return _song_len; }
    uint32_t song_pos(){ return _song_pos; }
    float song_pos_f(){ return _song_pos / (float)_song_len; }

private:
    uint32_t midi_var();
    void mem_parse();
    bool parse_tracks();
    void parse_event(MIDITrack &tr);
    void parse_meta_event(MIDITrack &tr);
    void parse_MIDI_event(MIDITrack &tr);
    void abort();

    template <typename T>
    T N();
    void skip(uint32_t bytes);

    std::string _file;
    bool _is_ours;
    double _acc_delay, _us_per_tick;
    uint8_t *_data = nullptr;
    uint32_t _prev_cursor, _cursor, _size;
    uint32_t _us_per_1_4_note;
    uint32_t _song_len, _song_pos;
    uint16_t _tracks, _track_num, _division;

    MIDIEvent *_cur_event;
    std::vector<MIDIEvent> _event_list;
    MIDITrack *_MIDI_tracks = nullptr;
};

#endif

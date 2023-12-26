#ifndef GOYCO_MIDI_H
#define GOYCO_MIDI_H

#include <cstdint>
#include <functional>
#include <vector>
#include <string>

struct MIDICmd {
enum Type {
    MIDI,
    File
};

    Type type;
    std::vector<uint8_t> data;

    static MIDICmd request_file(std::string name);
    static MIDICmd msg(std::vector<uint8_t> _msg);
};

void goyco_MIDI_mute_solo(uint8_t channel, int button, bool state);
int goyco_MIDI_get_notes(uint8_t channel);
void goyco_MIDI_send(MIDICmd msg);
void goyco_MIDI_seek(float pos);
void goyco_MIDI_rseek(int millis);
void goyco_MIDI_pause();
bool goyco_MIDI_fpath(std::string &name);
float goyco_MIDI_get_pos();
void goyco_MIDI_init(std::function<void()> callback);
void goyco_MIDI_stop();

#endif

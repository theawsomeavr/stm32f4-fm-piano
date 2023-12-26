#ifndef SAVE_H
#define SAVE_H

#include <functional>
#include <string>

bool deserialize(std::string file, std::function<void(std::string &name, std::string &key, std::string &value)> cb);
void serialize(std::string file, std::function<std::string(int)> cb);

#endif

#include "dfa_match.hh"

#include <sstream>
#include <string>

int main()
{
    DFA_Matcher original;
    original.AddMatch("*.cpp", false, 17);
    original.AddMatch("README*", true, 42);
    original.Compile();

    std::stringstream saved(std::ios::in | std::ios::out | std::ios::binary);
    original.Save(saved);
    const std::string bytes = saved.str();
    if(bytes.size() < 32) return 1;

    DFA_Matcher loaded;
    loaded.AddMatch("*.cpp", false, 17);
    loaded.AddMatch("README*", true, 42);
    saved.seekg(0);
    if(!loaded.Load(saved) || !loaded.Valid()) return 2;
    if(loaded.Test("file.cpp", -1) != 17) return 3;
    if(loaded.Test("readme.txt", -1) != 42) return 4;
    if(loaded.Test("other.txt", -1) != -1) return 5;

    // An overlong/invalid varint header must be rejected without changing
    // the already-loaded machine.
    const std::string corrupt(32, static_cast<char>(0xFF));
    std::stringstream malformed(corrupt, std::ios::in | std::ios::binary);
    if(loaded.Load(malformed)) return 6;
    if(loaded.Test("file.cpp", -1) != 17) return 7;

    // Truncated data must also be rejected without reading past the stream.
    std::stringstream truncated(bytes.substr(0, 12), std::ios::in | std::ios::binary);
    if(loaded.Load(truncated)) return 8;
    return 0;
}

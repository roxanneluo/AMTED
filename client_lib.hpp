#ifndef __CLIENT__
#define __CLIENT__

#include <unordered_map>

constexpr int MAX_BUFFER_SIZE = 2097152 + 20; // 2MB + 20 for safe

using cfd_t = int; // client file descriptor 

struct ClientState {
  char buffer[MAX_BUFFER_SIZE];
  int write_offset = 0;
};

using ClientMap = std::unordered_map<cfd_t, ClientState>;

#endif

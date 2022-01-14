#ifndef BREAKPOINT_H
#define BREAKPOINT_H

#include <unistd.h>
#include <cstdint>

class Breakpoint {
private:
  pid_t pid;
  std::intptr_t address;
  bool enabled;
  // data which used to be at the break point address
  uint8_t savedData;
public:
  Breakpoint() = default;
  Breakpoint(pid_t p, std::intptr_t addr);
  // Inject INT3 for enabling breakpoint
  void enable();
  // Restore the acommand for disabling breakpoint
  void disable();
  bool isEnabled() const {return enabled;}
  std::intptr_t getAddress() const {return address;}
};

#endif // BREAKPOINT_H

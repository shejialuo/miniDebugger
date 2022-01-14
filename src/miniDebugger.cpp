#include <iostream>
#include <unistd.h>
#include <sys/ptrace.h>
#include <spdlog/spdlog.h>
#include <debugger.h>

int main(int argc, char* argv[]) {
  if(argc < 2) {
    spdlog::error("Program name out specified");
    return -1;
  }
  auto programName = argv[1];

  auto pid = fork();

  if(pid == 0) {
  /*
    * In child, start ptrace and start the child process for
    * the debugged program.
  */
    ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
    execl(programName, programName, nullptr);
  } else if (pid > 0) {
    spdlog::info("Start debugging process {}", pid);
    Debugger debugger{programName, pid};
    debugger.run();
  } else {
    spdlog::error("Fork Error");
  }
}

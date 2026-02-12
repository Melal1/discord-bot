#ifndef UTILS_H
#define UTILS_H
#include <dpp/dpp.h>
#include <string>
#include <fmt/core.h>

using DL = dpp::loglevel;

inline bool GetBotToken(std::string &Buffer)
{
  const char *res = getenv("DisBotTok");
  if (!res)
    return 0;
  Buffer = res;
  return 1;
}

constexpr const char *SeverityName(dpp::loglevel lvl)
{
  switch (lvl)
  {
  case dpp::ll_trace:
    return "\x1b[90mtrace"; // gray
  case dpp::ll_debug:
    return "\x1b[36mdebug"; // cyan
  case dpp::ll_info:
    return "\x1b[32minfo"; // green
  case dpp::ll_warning:
    return "\x1b[33mwarn"; // yellow
  case dpp::ll_error:
    return "\x1b[31merror"; // red
  case dpp::ll_critical:
    return "\x1b[91mcritical"; // bright red
  default:
    return "\x1b[0munknown";
  }
}



#endif


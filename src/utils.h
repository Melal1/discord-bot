#ifndef UTILS_H
#define UTILS_H
#include "session_manager.h"
#include <dpp/dpp.h>
#include <dpp/guild.h>
#include <dpp/voicestate.h>
#include <fmt/core.h>
#include <string>
#define msg_fl(content, flags) dpp::message(content).set_flags(flags)
#define mFlagCmp(flags, flag_to_cmp) (flags &(uint8_t)SessionManager::Session::Flag::flag_to_cmp)

using DL = dpp::loglevel;

namespace utl
{
bool GetBotToken(std::string &Buffer);
dpp::voicestate *get_voice_state(dpp::snowflake guild_id, dpp::snowflake user_id);
dpp::voicestate *get_voice_state(dpp::guild *guild, dpp::snowflake user_id);

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

inline bool FlagCmp(uint8_t flags, SessionManager::Session::Flag flag_to_cmp)
{
  return (flags & (uint8_t)flag_to_cmp);
}

} // namespace utl

#endif

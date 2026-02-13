#include "utils.h"

bool utl::GetBotToken(std::string &Buffer)
{
  const char *res = getenv("DisBotTok");
  if (!res)
    return 0;
  Buffer = res;
  return 1;
}

dpp::voicestate *utl::get_voice_state(dpp::snowflake guild_id, dpp::snowflake user_id)
{
  dpp::guild *g = dpp::find_guild(guild_id);
  if (!g)
    return nullptr;

  auto it = g->voice_members.find(user_id);
  if (it == g->voice_members.end())
    return nullptr;

  if (it->second.channel_id.empty())
    return nullptr;

  return &it->second;
}

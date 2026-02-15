#ifndef VOICE_H
#define VOICE_H
#include <dpp/cluster.h>
#include <dpp/guild.h>
#include <dpp/snowflake.h>
#include <oggz/oggz.h>

void PlayAudio(dpp::cluster &bot, dpp::snowflake guild_id, dpp::snowflake channel_id, const char *path_to_file);
void PlayAudio(
    dpp::cluster &bot, dpp::snowflake guild_id, dpp::snowflake channel_id, const char *path_to_file, uint32_t duration);

#endif

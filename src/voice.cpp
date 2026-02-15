#include "voice.h"
#include "utils.h"
#include <dpp/cache.h>
#include <dpp/cluster.h>
#include <dpp/discordclient.h>
#include <dpp/misc-enum.h>
#include <dpp/snowflake.h>
#include <dpp/timer.h>
#include <fmt/base.h>
#include <fmt/format.h>
#include <opus/opusfile.h>

double ogg_opus_duration_seconds(const char *path)
{
  int err = 0;
  OggOpusFile *of = op_open_file(path, &err);
  if (!of)
  {
    return -1.0;
  }

  ogg_int64_t total_samples = op_pcm_total(of, -1); // -1 = is for whole fle
  op_free(of);

  if (total_samples < 0)
    return -1.0;

  return static_cast<double>(total_samples) / 48000.0;
}

inline dpp::voiceconn *HandleVoiceConnectionReady(
    dpp::cluster &bot, dpp::discord_client *shard, dpp::timer timer_id, dpp::snowflake guild_id, uint32_t &tries)
{
  if (tries == 0)
  {
    bot.stop_timer(timer_id);
    bot.log(DL::ll_warning, "Tried for 10 times stopping now");
    shard->disconnect_voice(guild_id);
    return nullptr;
  }
  tries--;

  dpp::voiceconn *V = shard->get_voice(guild_id);
  if (!V || !V->voiceclient || !V->voiceclient->is_ready())
  {
    bot.log(DL::ll_warning, fmt::format("From play audio function: Bot voice connection not ready try no. {}", tries));
    return nullptr;
  }
  if (tries != 0)
    bot.stop_timer(timer_id);
  return V;
}

inline dpp::discord_client *FindAndJoinVoice(dpp::cluster &bot, dpp::snowflake guild_id, dpp::snowflake channel_id)
{
  dpp::guild *guild = dpp::find_guild(guild_id);
  if (!guild)
  {
    bot.log(DL::ll_warning, "Error in playing audio function : guild not valid");
    return nullptr;
  }

  auto *shard = bot.get_shard(guild->shard_id);
  if (shard)
  {
    shard->connect_voice(guild_id, channel_id);
    return shard;
  }
  else
  {
    bot.log(DL::ll_warning, "Error in playing audio function : Couldn't connect to voice");
    return nullptr;
  }
}

void PlayAudio(dpp::cluster &bot, dpp::snowflake guild_id, dpp::snowflake channel_id, const char *path_to_file)
{
  auto shard = FindAndJoinVoice(bot, guild_id, channel_id);

  if (!shard)
    return;

  double duration = ogg_opus_duration_seconds(path_to_file);
  OGGZ *track_og = oggz_open(path_to_file, OGGZ_READ);

  if (!track_og)
  {
    bot.log(DL::ll_warning, fmt::format("Error in playing audio function : Couldn't open the file {}", path_to_file));
    return;
  }
  uint32_t tries = 10;
  bot.start_timer(
      [=, &bot](dpp::timer t) mutable
      {
        auto V = HandleVoiceConnectionReady(bot, shard, t, guild_id, tries);
        if (!V)
          return;

        oggz_set_read_callback(
            track_og,
            -1,
            [](OGGZ *, oggz_packet *packet, long, void *user_data)
            {
              auto *vc = static_cast<dpp::voiceconn *>(user_data);
              vc->voiceclient->send_audio_opus(packet->op.packet, packet->op.bytes);
              return 0;
            },
            (void *)V);

        while (V && V->voiceclient && !V->voiceclient->terminating)
        {
          static constexpr long CHUNK_READ = BUFSIZ * 2;
          long read_bytes = oggz_read(track_og, CHUNK_READ);
          if (read_bytes <= 0)
            break;
        }

        oggz_close(track_og);

        bot.start_timer(
            [=, &bot](dpp::timer t2)
            {
              shard->disconnect_voice(guild_id);
              bot.stop_timer(t2);
            },
            duration + 2);
      },
      2);

  return;
}

void PlayAudio(
    dpp::cluster &bot, dpp::snowflake guild_id, dpp::snowflake channel_id, const char *path_to_file, uint32_t duration)

{
  auto shard = FindAndJoinVoice(bot, guild_id, channel_id);

  if (!shard)
    return;

  OGGZ *track_og = oggz_open(path_to_file, OGGZ_READ);

  if (!track_og)
  {
    bot.log(DL::ll_warning, fmt::format("Error in playing audio function : Couldn't open the file {}", path_to_file));
    return;
  }
  uint32_t tries = 10;
  bot.start_timer(
      [=, &bot](dpp::timer t) mutable
      {
        auto V = HandleVoiceConnectionReady(bot, shard, t, guild_id, tries);

        if (!V)
          return;

        oggz_set_read_callback(
            track_og,
            -1,
            [](OGGZ *, oggz_packet *packet, long, void *user_data)
            {
              auto *vc = static_cast<dpp::voiceconn *>(user_data);
              vc->voiceclient->send_audio_opus(packet->op.packet, packet->op.bytes);
              return 0;
            },
            (void *)V);

        while (V && V->voiceclient && !V->voiceclient->terminating)
        {
          static constexpr long CHUNK_READ = BUFSIZ * 2;
          long read_bytes = oggz_read(track_og, CHUNK_READ);
          if (read_bytes <= 0)
            break;
        }

        oggz_close(track_og);

        bot.start_timer(
            [=, &bot](dpp::timer t2)
            {
              shard->disconnect_voice(guild_id);
              bot.stop_timer(t2);
            },
            duration + 2);
      },
      2);

  return;
}

#include "session_manager.h"
#include <dpp/cluster.h>
#include <dpp/guild.h>
#include <dpp/message.h>
#include <dpp/misc-enum.h>
#include <dpp/timer.h>
#include <fmt/format.h>
#include <functional>
#include <string>
using SMS = SessionManager::Session;

// Constructors
SessionManager::SessionManager(dpp::cluster &bot) : Bot(bot)
{
  Bot.log(dpp::loglevel::ll_info, "Session manager init");
}

SMS::Session(
    snflake usr_id,
    snflake channel_id,
    snflake guild_id,
    std::vector<snflake> &&members_ids,
    unsigned work_period,
    unsigned break_period,
    unsigned repeat)
    : OwnerId(usr_id), ChannelId(channel_id), GuildId(guild_id), MembersId(std::move(members_ids)),
      WorkPeriod(work_period), BreakPeriod(break_period), Repeat(repeat + 1)
{
  StartTime = std::chrono::steady_clock::now();
  CurrentSession = 1;
  CurrentPhase = Phases::Break;
}

//-------------

// Getters

SMS *SessionManager::GetSession(snflake owner_id)
{
  auto it = ActiveSessions.find(owner_id);
  return it == ActiveSessions.end() ? nullptr : &it->second;
}

SMS const *SessionManager::GetSession(snflake owner_id) const
{
  auto it = ActiveSessions.find(owner_id);
  return it == ActiveSessions.end() ? nullptr : &it->second;
}

//------------

void SMS::SchedulePhase(SessionManager &Manager)
{
  auto &bot = Manager.Bot;
  auto ChangeMembersStatus = [&](bool mute)
  {
    dpp::guild_member GuildMember;
    GuildMember.set_mute(mute);
    for (auto const &id : MembersId)
    {
      GuildMember.guild_id = GuildId;
      GuildMember.user_id = id;
      bot.guild_edit_member(GuildMember);
    }
  };
  if (CurrentSession >= Repeat)
  {
    bot.message_create(dpp::message(ChannelId, "Pomodoro session finished!"));
    ChangeMembersStatus(0);
    Manager.CancelTimer(OwnerId);
    return;
  }

  auto ScheduleNext = [&](SMS::Phases phase, unsigned period) // helper lambda
  {
    CurrentPhase = phase;
    TimerId = bot.start_timer(
        [&](dpp::timer t)
        {
          bot.stop_timer(t);
          SchedulePhase(Manager);
        },
        WorkPeriod);
  };

  switch (CurrentPhase)
  {
  case Phases::Break: // Starting work session
    bot.message_create(dpp::message(ChannelId, fmt::format("Work session {} started !", CurrentSession)));
    ChangeMembersStatus(0);
    ScheduleNext(Phases::Work, WorkPeriod);
    CurrentSession++;
    break;
  case Phases::Work: // Starting break session
    bot.message_create(dpp::message(ChannelId, fmt::format("Break session {} started !", CurrentSession - 1)));
    ChangeMembersStatus(0);
    ScheduleNext(Phases::Break, BreakPeriod);
    break;
  }
}

bool SessionManager::StartTimer(
    snflake usr_id,
    snflake channel_id,
    dpp::guild *guild,
    unsigned work_period_in_min,
    unsigned break_period_in_min,
    unsigned repeat,
    std::function<void(Session const &session)> call_back)
{

  std::vector<snflake> MembersIds;
  MembersIds.reserve(5);
  for (auto const &[usr, vc] : guild->voice_members)
  {
    if (vc.channel_id == channel_id)
      MembersIds.push_back(usr);
  }

  auto res = ActiveSessions.emplace(
      usr_id,
      Session(usr_id, channel_id, guild->id, std::move(MembersIds), work_period_in_min, break_period_in_min, repeat));
  if (call_back)
    call_back(res.first->second);
  res.first->second.SchedulePhase(*this);

  return 1;
}

bool SessionManager::CancelTimer(
    snflake owner_id, std::function<void(SessionManager::Session const &session)> call_before_remove)
{
  auto it = ActiveSessions.find(owner_id);
  if (it == ActiveSessions.end())
    return 0;

  Bot.stop_timer(it->second.TimerId);

  if (call_before_remove)
    call_before_remove(it->second);

  ActiveSessions.erase(it);

  return 1;
}

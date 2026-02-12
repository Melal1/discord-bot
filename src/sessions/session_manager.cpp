#include "session_manager.h"
#include <dpp/cluster.h>
#include <dpp/message.h>
#include <dpp/misc-enum.h>
#include <fmt/format.h>
#include <string>

SessionManager::SessionManager(dpp::cluster &bot) : Bot(bot)
{
  Bot.log(dpp::loglevel::ll_info, "Session manager init");
}

void SessionManager::Session::SchedulePhase(SessionManager &Manager)
{
  auto &bot = Manager.Bot;
  if (CurrentSession >= Repeat)
  {
    bot.message_create(dpp::message(ChannelId, "Pomodoro session finished!"));
    Manager.CancelTimer(OwnerId);
    return;
  }

  switch (CurrentPhase)
  {
  case Phases::Work:
    CurrentPhase = Phases::Break; // Starting break session
    bot.message_create(dpp::message(ChannelId, fmt::format("Break session {} started !", CurrentSession - 1)));
    TimerId = bot.start_timer(
        [&](dpp::timer t)
        {
          bot.stop_timer(t);
          SchedulePhase(Manager);
        },
        BreakPeriod);
    break;
  case Phases::Break:
    CurrentPhase = Phases::Work; // Starting work session
    bot.message_create(dpp::message(ChannelId, fmt::format("Work session {} started !", CurrentSession)));
    TimerId = bot.start_timer(
        [&](dpp::timer t)
        {
          bot.stop_timer(t);
          CurrentSession++;
          SchedulePhase(Manager);
        },
        WorkPeriod);

    break;
  }
}

SessionManager::Session::Session(snflake usr_id, snflake channel_id, unsigned work_period, unsigned break_period, unsigned repeat)
    : OwnerId(usr_id), ChannelId(channel_id), WorkPeriod(work_period), BreakPeriod(break_period), Repeat(repeat + 1)
{
  StartTime = std::chrono::steady_clock::now();
  CurrentSession = 1;
  CurrentPhase = Phases::Break;
}

bool SessionManager::StartTimer(snflake usr_id, snflake channel_id, unsigned work_period_in_sec, unsigned break_period_in_sec, unsigned repeat)
{
  if (ActiveSessions.find(usr_id) != ActiveSessions.end())
  {
    return 0;
  }
  auto res = ActiveSessions.emplace(usr_id, Session(usr_id, channel_id, work_period_in_sec, break_period_in_sec, repeat));
  res.first->second.SchedulePhase(*this);

  return 1;
}

bool SessionManager::CancelTimer(snflake owner_id)
{
  auto it = ActiveSessions.find(owner_id);
  if (it == ActiveSessions.end())
    return 0;

  Bot.message_create(dpp::message(it->second.ChannelId, "Timer ended"));
  Bot.stop_timer(it->second.TimerId);
  ActiveSessions.erase(it);

  return 1;
}

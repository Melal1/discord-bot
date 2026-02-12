#include "SessionManager.h"
#include <dpp/cluster.h>
#include <dpp/message.h>
#include <dpp/misc-enum.h>
#include <string>

SessionManager::SessionManager(dpp::cluster &bot) : Bot(bot)
{
  Bot.log(dpp::loglevel::ll_info, "Session manager init");
}

void SessionManager::Session::SchedulePhase(SessionManager &Manager)
{
  auto &bot = Manager.Bot;
  if (WorkSessionDone >= Repeat)
  {
    Manager.CancelTimer(OwnerId);
    return;
  }
  bot.stop_timer(TimerId);

  switch (CurrentPhase)
  {
  case Phases::Work:
    WorkSessionDone++;
    CurrentPhase = Phases::Break;
    bot.message_create(dpp::message(ChannelId, "Work Session Started" + std::to_string(WorkSessionDone)));
    TimerId = bot.start_timer(
        [&](dpp::timer t)
        {
          SchedulePhase(Manager);
        },
        BreakPeriod);
    break;
  case Phases::Break:
    CurrentPhase = Phases::Work;
    bot.message_create(dpp::message(ChannelId, "Break Session started" + std::to_string(WorkSessionDone)));
    TimerId = bot.start_timer(
        [&](dpp::timer t)
        {
          SchedulePhase(Manager);
        },
        WorkPeriod);
    break;
  }
}

SessionManager::Session::Session(dpp::cluster &bot, snflake usr_id, snflake channel_id, unsigned work_period, unsigned break_period, unsigned repeat) : OwnerId(usr_id), ChannelId(channel_id), WorkPeriod(work_period), BreakPeriod(break_period), Repeat(repeat)
{
  StartTime = std::chrono::steady_clock::now();
  WorkSessionDone = 0;
  CurrentPhase = Phases::Work;
}

bool SessionManager::StartTimer(snflake usr_id, snflake channel_id, unsigned work_period_in_sec, unsigned break_period_in_sec, unsigned repeat)
{
  if (ActiveSessions.find(usr_id) != ActiveSessions.end())
  {
    return 0;
  }
  auto res = ActiveSessions.emplace(usr_id, Session(Bot, usr_id, channel_id, work_period_in_sec, break_period_in_sec, repeat));
  res.first->second.SchedulePhase(*this);

  return 1;
}

bool SessionManager::CancelTimer(snflake owner_id)
{
  auto it = ActiveSessions.find(owner_id);

  Bot.message_create(dpp::message(it->second.ChannelId, "Timer ended"));
  Bot.stop_timer(it->second.TimerId);
  ActiveSessions.erase(it);


  return 1;
}

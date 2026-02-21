#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H
#include <chrono>
#include <dpp/channel.h>
#include <dpp/cluster.h>
#include <dpp/snowflake.h>
#include <dpp/timer.h>
#include <functional>
#include <unordered_map>
#include <vector>
using flag_t = uint8_t;

struct Audio
{
  const char *path;
  const uint32_t &&duration;
};

constexpr const Audio BreakToWorkAudio{"assests/audio/BreakToWork.opus", 6};
constexpr const Audio WorkToBreakAudio{"assests/audio/WorkToBreak.opus", 4};

// constexpr const Audio QadTasama { };

class SessionManager
{
  using snflake = dpp::snowflake;

public:
  struct Session
  {
    enum class Flag : flag_t
    {
      Break = 1u << 0, // So if bit-0 was 1 in the flags then it's a break session
      Mute = 1u << 1,  // So if the bit-1 was 1 in the flags then mute is on ( 0-off )
      Voice = 1u << 2
    };
    snflake OwnerId;
    snflake ChannelId;
    snflake GuildId;

    std::vector<snflake> MembersId;
    dpp::timer TimerId;
    std::chrono::steady_clock::time_point PhaseStartTime;

    unsigned WorkPeriod;
    unsigned BreakPeriod;
    unsigned Repeat;
    unsigned CurrentSessionNumber;

    std::string VoiceChannelName;
    // 1-byte
    flag_t Flags; // bit-0 for current phase , bit-1 for mute flag
    Session(
        snflake usr_id,
        snflake channel_id,
        snflake guild_id,
        std::vector<snflake> &&members_ids,
        unsigned work_period,
        unsigned break_period,
        unsigned repeat,
        std::string_view vc_channel_name,
        flag_t flags = 1u << 0 //
    );
    void SchedulePhase(SessionManager &manager);
    long GetRemainingTime();

    void ChangeMembersStatus(SessionManager &manager, bool mute);
  };

  explicit SessionManager(dpp::cluster &bot);
  Session *GetSessionByOwnerId(snflake owner_id);
  Session const *GetSessionByOwnerId(snflake owner_id) const;
  Session *GetSessionByUserId(snflake usr_id);

  Session const *GetSessionByUserId(snflake usr_id) const;
  void StartTimer(
      snflake usr_id,
      dpp::channel *channel,
      unsigned work_period_in_min,
      unsigned break_period_in_min,
      unsigned repeat,
      flag_t flags = 1u << 0,
      std::function<void(Session const &session)> call_back = nullptr);
  bool CancelTimer(
      snflake owner_id, std::function<void(SessionManager::Session const &session)> call_before_remove = nullptr);
  void CancelTimer(
      Session *session, std::function<void(SessionManager::Session const &session)> call_before_remove = nullptr);
  dpp::cluster &Bot;

  // Static methods

  static inline void SetFlag(flag_t &flags, Session::Flag type, bool mode)
  {
    if (mode)
      flags |= static_cast<flag_t>(type); // set bit
    else
      flags &= ~static_cast<flag_t>(type); // clear bit
  }

  static bool HasFlag(flag_t flags, Session::Flag type)
  {
    return flags & static_cast<flag_t>(type);
  }

private:
  std::unordered_map<snflake, Session> _active_sessions;
};
#endif

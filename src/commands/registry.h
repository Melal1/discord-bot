#ifndef REGISTRY_H
#define REGISTRY_H
#include "utils.h"
#include <dpp/cluster.h>
#include <dpp/dispatcher.h>
#include <functional>
#include <unordered_map>
using Handler = std::function<void(dpp::slashcommand_t const &)>;

class Registry
{
public:
  void Add(std::string_view command_name, Handler);
  bool Dispatch(std::string_view command_name, dpp::slashcommand_t const &event);

  Registry(dpp::cluster &bot) : Bot(bot)
  {
    Bot.log(DL::ll_info, "Registry init");
  };

  dpp::cluster &Bot;

private:
  std::unordered_map<std::string_view, Handler> Handlers;
};

#endif

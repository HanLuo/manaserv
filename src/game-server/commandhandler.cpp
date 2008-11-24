/*
 *  The Mana World
 *  Copyright 2008 The Mana World Development Team
 *
 *  This file is part of The Mana World.
 *
 *  The Mana World is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  The Mana World is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with The Mana World; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "defines.h"
#include "commandhandler.hpp"
#include "accountconnection.hpp"
#include "character.hpp"
#include "gamehandler.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "itemmanager.hpp"
#include "mapmanager.hpp"
#include "monster.hpp"
#include "monstermanager.hpp"
#include "state.hpp"

#include "../utils/string.hpp"

static void say(const std::string error, Character *player)
{
    GameState::sayTo(player, NULL, error);
}

static bool handlePermissions(Character *player, unsigned int permissions)
{
    if (player->getAccountLevel() & permissions)
    {
        return true;
    }

    say("Invalid permissions", player);

    return false;
}

static std::string getArgument(std::string &args)
{
    std::string argument = "";
    std::string::size_type pos = args.find(' ');
    if (pos != std::string::npos)
    {
        argument = args.substr(0, pos);
        args = args.substr(pos+1);
    }
    else
    {
        argument = args.substr(0);
        args = "";
    }

    return argument;
}

static Character* getPlayer(const std::string &player)
{
    // get character, via the client, as they may be
    // on a different game server
    GameClient *client = gameHandler->getClientByNameSlow(player);
    if (!client)
    {
        return NULL;
    }

    if (client->status != CLIENT_CONNECTED)
    {
        return NULL;
    }

    return client->character;
}

static void handleHelp(Character *player, std::string &args)
{
    if (args == "")
    {
        if (player->getAccountLevel() & AL_PLAYER)
        {
            say("Game Master Commands:", player);
            say("@help [command]", player);
            say("@report <bug>", player);
        }

        if (player->getAccountLevel() & AL_TESTER)
        {
            say("@warp <character> <map> <x> <y>", player);
            say("@goto <character>", player);
        }

        if (player->getAccountLevel() & AL_GM)
        {
            say("@recall <character>", player);
            say("@ban <character> <length of time>", player);
        }

        if (player->getAccountLevel() & AL_DEV)
        {
            say("@item <character> <item id> <amount>", player);
            say("@drop <item id> <amount>", player);
            say("@money <character> <amount>", player);
            say("@spawn <monster id> <number>", player);
            say("@attribute <character> <attribute> <value>", player);
        }

        if (player->getAccountLevel() & AL_ADMIN)
        {
            say("Administrator Commands", player);
            say("@reload", player);
            say("@setgroup <character> <AL level>", player);
        }
    }
    else
    {

    }
}

static void handleWarp(Character *player, std::string &args)
{
    int x, y;
    MapComposite *map;
    Character *other;

    // get the arguments
    std::string character = getArgument(args);
    std::string mapstr = getArgument(args);
    std::string xstr = getArgument(args);
    std::string ystr = getArgument(args);

    // if any of them are empty strings, no argument was given
    if (character == "" || mapstr == "" || xstr == "" || ystr == "")
    {
        say("Invalid number of arguments given.", player);
        return;
    }

    // if it contains # then it means the player
    if (character == "#")
    {
        other = player;
    }
    else
    {
        // check for valid player
        other = getPlayer(character);
        if (!other)
        {
            say("Invalid character, or they are offline", player);
            return;
        }
    }

    // if it contains # then it means the player's map
    if (mapstr == "#")
    {
        map = player->getMap();
    }
    else
    {
        // check for valid map id
        int id;
        if (!utils::isNumeric(mapstr))
        {
            say("Invalid map", player);
            return;
        }

        id = utils::stringToInt(mapstr);

        // get the map
        map = MapManager::getMap(id);
        if (!map)
        {
            say("Invalid map", player);
            return;
        }
    }

    if (!utils::isNumeric(xstr))
    {
        say("Invalid x", player);
        return;
    }

    if (!utils::isNumeric(ystr))
    {
        say("Invalid y", player);
        return;
    }

    // change the x and y to integers
    x = utils::stringToInt(xstr);
    y = utils::stringToInt(ystr);

    // now warp the player
    GameState::warp(other, map, x, y);
}

static void handleItem(Character *player, std::string &args)
{
    Character *other;
    ItemClass *ic;
    int value;
    int id;

    // get arguments
    std::string character = getArgument(args);
    std::string itemclass = getArgument(args);
    std::string valuestr = getArgument(args);

    // check all arguments are there
    if (character == "" || itemclass == "" || valuestr == "")
    {
        say("Invalid number of arguments given.", player);
        return;
    }

    // if it contains # that means the player
    if (character == "#")
    {
        other = player;
    }
    else
    {
        // check for valid player
        other = getPlayer(character);
        if (!other)
        {
            say("Invalid character or they are offline", player);
            return;
        }
    }

    // check we have a valid item
    if (!utils::isNumeric(itemclass))
    {
        say("Invalid item", player);
        return;
    }

    // put the itemclass id into an integer
    id = utils::stringToInt(itemclass);

    // check for valid item class
    ic = ItemManager::getItem(id);

    if (!ic)
    {
        say("Invalid item", player);
        return;
    }

    if (!utils::isNumeric(valuestr))
    {
        say("Invalid value", player);
        return;
    }

    value = utils::stringToInt(valuestr);

    if (value < 0)
    {
        say("Invalid amount", player);
        return;
    }

    // insert the item into the inventory
    Inventory(other).insert(ic->getDatabaseID(), value);
}

static void handleDrop(Character *player, std::string &args)
{
    ItemClass *ic;
    int value, id;

    // get arguments
    std::string itemclass = getArgument(args);
    std::string valuestr = getArgument(args);

    // check all arguments are there
    if (itemclass == "" || valuestr == "")
    {
        say("Invalid number of arguments given.", player);
        return;
    }

    // check that itemclass id and value are really integers
    if (!utils::isNumeric(itemclass) || !utils::isNumeric(valuestr))
    {
        say("Invalid arguments passed.", player);
        return;
    }

    // put the item class id into an integer
    id = utils::stringToInt(itemclass);

    // check for valid item
    ic = ItemManager::getItem(id);
    if (!ic)
    {
        say("Invalid item", player);
        return;
    }

    // put the value into an integer
    value = utils::stringToInt(valuestr);

    if (value < 0)
    {
        say("Invalid amount", player);
        return;
    }

    // create the integer and put it on the map
    Item *item = new Item(ic, value);
    item->setMap(player->getMap());
    item->setPosition(player->getPosition());
    GameState::insertSafe(item);
}

static void handleMoney(Character *player, std::string &args)
{
    Character *other;
    int value;

    // get arguments
    std::string character = getArgument(args);
    std::string valuestr = getArgument(args);

    // check all arguments are there
    if (character == "" || valuestr == "")
    {
        say("Invalid number of arguments given", player);
        return;
    }

    // check if its the player itself
    if (character == "#")
    {
        other = player;
    }
    else
    {
        // check for valid player
        other = getPlayer(character);
        if (!other)
        {
            say("Invalid character or they are offline", player);
            return;
        }
    }

    // check value is an integer
    if (!utils::isNumeric(valuestr))
    {
        say("Invalid argument", player);
        return;
    }

    // change value into an integer
    value = utils::stringToInt(valuestr);

    // change how much money the player has
    Inventory(other).changeMoney(value);
}

static void handleSpawn(Character *player, std::string &args)
{
    MonsterClass *mc;
    MapComposite *map = player->getMap();
    Point const &pos = player->getPosition();
    int value, id;

    // get the arguments
    std::string monsterclass = getArgument(args);
    std::string valuestr = getArgument(args);

    // check all arguments are there
    if (monsterclass == "" || valuestr == "")
    {
        say("Invalid amount of arguments given.", player);
        return;
    }

    // check they are really numbers
    if (!utils::isNumeric(monsterclass) || !utils::isNumeric(valuestr))
    {
        say("Invalid arguments", player);
        return;
    }

    // put the monster class id into an integer
    id = utils::stringToInt(monsterclass);

    // check for valid monster
    mc = MonsterManager::getMonster(id);
    if (!mc)
    {
        say("Invalid monster", player);
        return;
    }

    // put the amount into an integer
    value = utils::stringToInt(valuestr);

    if (value < 0)
    {
        say("Invalid amount", player);
        return;
    }

    // create the monsters and put them on the map
    for (int i = 0; i < value; ++i)
    {
        Being *monster = new Monster(mc);
        monster->setMap(map);
        monster->setPosition(pos);
        monster->clearDestination();
        if (!GameState::insertSafe(monster))
        {
            // The map is full. Break out.
            break;
        }
    }
}

static void handleGoto(Character *player, std::string &args)
{
    Character *other;

    // get the arguments
    std::string character = getArgument(args);

    // check all arguments are there
    if (character == "")
    {
        say("Invalid amount of arguments given.", player);
        return;
    }

    // check for valid player
    other = getPlayer(character);
    if (!other)
    {
        say("Invalid character, or they are offline.", player);
        return;
    }

    // move the player to where the other player is
    MapComposite *map = other->getMap();
    Point const &pos = other->getPosition();
    GameState::warp(player, map, pos.x, pos.y);
}

static void handleRecall(Character *player, std::string &args)
{
    Character *other;

    // get the arguments
    std::string character = getArgument(args);

    // check all arguments are there
    if (character == "")
    {
        say("Invalid amount of arguments given.", player);
        return;
    }

    // check for valid player
    other = getPlayer(character);
    if (!other)
    {
        say("Invalid character, or they are offline.", player);
        return;
    }

    // move the other player to where the player is
    MapComposite *map = player->getMap();
    Point const &pos = player->getPosition();
    GameState::warp(other, map, pos.x, pos.y);
}

static void handleReload(Character *player)
{
    // reload the items and monsters
    ItemManager::reload();
    MonsterManager::reload();
}

static void handleBan(Character *player, std::string &args)
{
    Character *other;
    int length;

    // get arguments
    std::string character = getArgument(args);
    std::string valuestr = getArgument(args);

    // check all arguments are there
    if (character == "" || valuestr == "")
    {
        say("Invalid number of arguments given.", player);
        return;
    }

    // check for valid player
    other = getPlayer(character);
    if (!other)
    {
        say("Invalid character", player);
        return;
    }

    // check the length is really an integer
    if (!utils::isNumeric(valuestr))
    {
        say("Invalid argument", player);
        return;
    }

    // change the length to an integer
    length = utils::stringToInt(valuestr);

    if (length < 0)
    {
        say("Invalid length", player);
        return;
    }

    // ban the player
    accountHandler->banCharacter(other, length);
}

static void handleSetGroup(Character *player, std::string &args)
{
    Character *other;
    int level = 0;

    // get the arguments
    std::string character = getArgument(args);
    std::string levelstr = getArgument(args);

    // check all arguments are there
    if (character == "" || levelstr == "")
    {
        say("Invalid number of arguments given.", player);
        return;
    }

    // check if its to effect the player
    if (character == "#")
    {
        other = player;
    }
    else
    {
        // check for valid player
        other = getPlayer(character);
        if (!other)
        {
            say("Invalid character", player);
            return;
        }
    }

    // check which level they should be
    // refer to defines.h for level info
    if (levelstr == "AL_PLAYER")
    {
        level = AL_PLAYER;
    }
    else if (levelstr == "AL_TESTER")
    {
        level = AL_PLAYER | AL_TESTER;
    }
    else if (levelstr == "AL_GM")
    {
        level = AL_PLAYER | AL_TESTER | AL_GM;
    }
    else if (levelstr == "AL_DEV")
    {
        level = AL_PLAYER | AL_TESTER | AL_DEV;
    }
    else if (levelstr == "AL_ADMIN")
    {
        level = 255;
    }

    if (level == 0)
    {
        say("Invalid group", player);
        return;
    }

    // change the player's account level
    accountHandler->changeAccountLevel(other, level);
}

static void handleAttribute(Character *player, std::string &args)
{
    Character *other;
    int attr, value;

    // get arguments
    std::string character = getArgument(args);
    std::string attrstr = getArgument(args);
    std::string valuestr = getArgument(args);

    // check all arguments are there
    if (character == "" || valuestr == "" || attrstr == "")
    {
        say("Invalid number of arguments given.", player);
        return;
    }

    // check if its the player or another player
    if (character == "#")
    {
        other = player;
    }
    else
    {
        // check for valid player
        other = getPlayer(character);
        if (!other)
        {
            say("Invalid character", player);
            return;
        }
    }

    // check they are really integers
    if (!utils::isNumeric(valuestr) || !utils::isNumeric(attrstr))
    {
        say("Invalid argument", player);
        return;
    }

    // put the attribute into an integer
    attr = utils::stringToInt(attrstr);

    if (attr < 0)
    {
        say("Invalid Attribute", player);
        return;
    }

    // put the value into an integer
    value = utils::stringToInt(valuestr);

    if (value < 0)
    {
        say("Invalid amount", player);
        return;
    }

    // change the player's attribute
    other->setAttribute(attr, value);
}

static void handleReport(Character *player, std::string &args)
{
    std::string bugReport = getArgument(args);

    if (bugReport == "")
    {
        say("Invalid number of arguments given.", player);
        return;
    }

    // TODO: Send the report to a developer or something
}

void CommandHandler::handleCommand(Character *player,
                                   const std::string &command)
{
    // get command type, and arguments
    // remove first character (the @)
    std::string::size_type pos = command.find(' ');
    std::string type(command, 1, pos == std::string::npos ? pos : pos - 1);
    std::string args(command, pos == std::string::npos ? command.size() : pos + 1);

    // handle the command
    if (type == "help")
    {
        if (handlePermissions(player, AL_PLAYER))
            handleHelp(player, args);
    }
    else if (type == "warp")
    {
        if (handlePermissions(player, AL_TESTER))
            handleWarp(player, args);
    }
    else if (type == "item")
    {
        if (handlePermissions(player, AL_DEV))
            handleItem(player, args);
    }
    else if (type == "drop")
    {
        if (handlePermissions(player, AL_DEV))
            handleDrop(player, args);
    }
    else if (type == "money")
    {
        if (handlePermissions(player, AL_DEV))
            handleMoney(player, args);
    }
    else if (type == "spawn")
    {
        if (handlePermissions(player, AL_DEV))
            handleSpawn(player, args);
    }
    else if (type == "goto")
    {
        if (handlePermissions(player, AL_TESTER))
            handleGoto(player, args);
    }
    else if (type == "recall")
    {
        if (handlePermissions(player, AL_GM))
            handleRecall(player, args);
    }
    else if (type == "reload")
    {
        if (handlePermissions(player, AL_ADMIN))
            handleReload(player);
    }
    else if (type == "ban")
    {
        if (handlePermissions(player, AL_GM))
            handleBan(player, args);
    }
    else if (type == "setgroup")
    {
        if (handlePermissions(player, AL_ADMIN))
            handleSetGroup(player, args);
    }
    else if (type == "attribute")
    {
        if (handlePermissions(player, AL_DEV))
            handleAttribute(player, args);
    }
    else if (type == "report")
    {
        if (handlePermissions(player, AL_PLAYER))
            handleReport(player, args);
    }
    else
    {
        say("command not found", player);
    }
}
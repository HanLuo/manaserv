/*
 *  The Mana World Server
 *  Copyright 2007 The Mana World Development Team
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
 *
 *  $Id$
 */

#include <algorithm>
#include <cassert>

#include "game-server/trade.hpp"

#include "defines.h"
#include "game-server/character.hpp"
#include "game-server/gamehandler.hpp"
#include "game-server/inventory.hpp"
#include "net/messageout.hpp"

Trade::Trade(Character *c1, Character *c2):
    mChar1(c1), mChar2(c2), mState(TRADE_INIT)
{
    MessageOut msg(GPMSG_TRADE_REQUEST);
    msg.writeShort(c1->getPublicID());
    c2->getClient()->send(msg);
    c1->setTrading(this);
    c2->setTrading(this);
}

Trade::~Trade()
{
    mChar1->setTrading(NULL);
    mChar2->setTrading(NULL);
}

void Trade::cancel(Character *c)
{
    MessageOut msg(GPMSG_TRADE_CANCEL);
    if (c != mChar1) mChar1->getClient()->send(msg);
    if (c != mChar2) mChar2->getClient()->send(msg);
    delete this;
}

bool Trade::request(Character *c, int id)
{
    if (mState != TRADE_INIT || c != mChar2 || mChar1->getPublicID() != id)
    {
        /* This is not an ack for the current transaction. So assume
           a new one is about to start and cancel the current one. */
        cancel(c);
        return false;
    }

    // Starts trading.
    mState = TRADE_RUN;
    MessageOut msg(GPMSG_TRADE_START);
    mChar1->getClient()->send(msg);
    mChar2->getClient()->send(msg);
    return true;
}

static bool performTrade(TradedItems items, Inventory &inv1, Inventory &inv2)
{
    for (TradedItems::const_iterator i = items.begin(),
         i_end = items.end(); i != i_end; ++i)
    {
        if (i->id != inv1.getItem(i->slot) ||
            inv1.removeFromSlot(i->slot, i->amount) != 0 ||
            inv2.insert(i->id, i->amount) != 0)
        {
            return false;
        }
    }
    return true;
}

void Trade::accept(Character *c)
{
    if (mState == TRADE_RUN)
    {
        if (c == mChar2)
        {
            std::swap(mChar1, mChar2);
            std::swap(mItems1, mItems2);
        }
        assert(c == mChar1);
        // First player agrees.
        mState = TRADE_EXIT;
        MessageOut msg(GPMSG_TRADE_ACCEPT);
        mChar2->getClient()->send(msg);
        return;
    }

    if (mState != TRADE_EXIT || c != mChar2)
    {
        // First player has already agreed. We only care about the second one.
        return;
    }

    Inventory v1(mChar1, true), v2(mChar2, true);
    if (!performTrade(mItems1, v1, v2) || !performTrade(mItems2, v2, v1))
    {
        v1.cancel();
        v2.cancel();
        cancel(NULL);
        return;
    }

    MessageOut msg(GPMSG_TRADE_COMPLETE);
    mChar1->getClient()->send(msg);
    mChar2->getClient()->send(msg);
    delete this;
}

void Trade::addItem(Character *c, int slot, int amount)
{
    if (mState == TRADE_INIT) return;

    Character *other;
    TradedItems *items;
    if (c == mChar1)
    {
        other = mChar2;
        items = &mItems1;
    }
    else
    {
        assert(c == mChar2);
        other = mChar1;
        items = &mItems2;
    }

    // Arbitrary limit to prevent a client from DOSing the server.
    if (items->size() >= 50) return;

    Inventory inv(c, true);
    int id = inv.getItem(slot);
    if (id == 0) return;

    /* Checking now if there is enough items is useless as it can change
       later on. At worst, the transaction will be cancelled at the end if
       the client lied. */

    TradedItem ti = { id, slot, amount };
    items->push_back(ti);

    MessageOut msg(GPMSG_TRADE_ADD_ITEM);
    msg.writeShort(id);
    msg.writeByte(amount);
    other->getClient()->send(msg);

    // Go back to normal run.
    mState = TRADE_RUN;
}
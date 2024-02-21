#include "Client.hpp"
#include "Channel.hpp"
#include <queue>

static bool is_channel(string channel)
{
    if (channel.size() == 1 && channel[0] == '0')
        return (true);
    if (channel.size() < 2 || channel.size() > 51 || channel[0] != '#')
        return (false);
    channel.erase(0, 1);
    for (size_t i = 0; i < channel.size(); i++)
    {
        if (channel[i] == '\0' || channel[i] == '\a' || channel[i] == '\r' || channel[i] == '\n' || channel[i] == ' ' || channel[i] == ',' || channel[i] == ':')
            return (false);
    }
    return (true);
}

static void    joinParser(vector<string> join, queue<string> &channels, queue<string> &passwords, string &msg)
{
    join.erase(join.begin());

    stringstream chnls(join[0]);
    string channel;
    while (getline(chnls, channel, ','))
    {
        if (!is_channel(channel))
        {
            msg += "476  JOIN :Bad channel name " + channel;
        }
        else
        {
            channels.push(channel);
        }
    }
    
    stringstream pswds(join[1]);
    string password;
    while (getline(pswds, password, ','))
    {
        passwords.push(password);
    }
}

bool    Client::joinCommand(vector<string> join)
{
    queue<string>   channels;
    queue<string>   passwords;
    string          msg;

    if (!this->Regestred)
    {
        msg = ":ircserv 451 " + ((!this->NckName.empty()) ? this->NckName : "* ") + " " + join[0] + " :You have not registered\r\n";
        send(this->ClntFd, msg.c_str(), msg.size(), 0);
        return (false);
    }

    joinParser(join, channels, passwords, msg);

    while (!channels.empty())
    {
        map<string, Channel*>                &Channels =  Server::getInstance()->getChannels();
        map<string, Channel*>::iterator      search   =  Channels.find(channels.front());


        if (channels.front().size() == 1 && channels.front()[0] == '0')
        {
            for (size_t i = 0; i < this->Chnls.size(); i++)
            {
                msg = ":" + this->NckName+ "!" + this->UsrName + "@" + this->HstName + " PART " + this->Chnls[i] + "\r\n";
                for (size_t j = 0; j < Channels.find(this->Chnls[i])->second->getMembers().size(); j++)
                    send(Channels.find(this->Chnls[i])->second->getMembers()[j]->getClntFd(), msg.c_str(), msg.size(), 0);
                Channels[this->Chnls[i]]->removeMember(this);
                Channels[this->Chnls[i]]->removeOperator(this);
                Channels[this->Chnls[i]]->removeInvited(this);
            }
            this->Chnls.clear();
            channels.pop();
            msg = "";
            continue;
        }
        if (!Channels.count(channels.front()))
        {
            Channel *chnl = new Channel(this, channels.front());

            Channels[channels.front()] = chnl;

            cout << "channel created" << Channels[channels.front()]->getName() << " : " << channels.front() << endl;
            msg += "creation:" + this->NckName+ "!" + this->UsrName + "@" + this->HstName + " JOIN " + channels.front() + "\r\n";
            msg += ":IRC_SERVER 353 " + this->NckName + " = " + channels.front() + " :@"+this->NckName+"\r\n";
            msg += ":IRC_SERVER 366 " + this->NckName + " " + channels.front() + " ::End of /NAMES list\r\n";
            this->Chnls.push_back(channels.front());
        }
        else
        {

            if (search->second->isLocked())
            {
                if (passwords.empty() || (search->second->getPassword() != passwords.front()))
                {
                    msg += "475  JOIN :Wrong password";
                    channels.pop();
                    continue;
                }
                else {
                    passwords.pop();
                }
            }

            if (search->second->isLimited())
            {
                if (search->second->getLimit() >= search->second->getMembers().size())
                {
                    msg += "471  JOIN :Cannot join channel (+l)";
                    channels.pop();
                    continue;
                }
            }

            if (search->second->isMember(this))
            {
                msg += "477  JOIN :Already a member of channel";
                channels.pop();
                continue;
            }

            if (search->second->isInviteOnly())
            {
                if (!search->second->isInvited(this))
                {
                    msg += "473  JOIN :Cannot join channel (+i)";
                    channels.pop();
                    continue;
                }
                else
                {
                    search->second->removeInvited(this);
                }
            }

            search->second->addMember(this);
            this->Chnls.push_back(channels.front());

            for (size_t i = 0; i < search->second->getMembers().size(); i++)
            {
                msg = ":" + this->NckName+ "!" + this->UsrName + "@" + this->HstName + " JOIN " + channels.front() + "\r\n";
                send(search->second->getMembers()[i]->getClntFd(), msg.c_str(), msg.size(), 0);
            }

            msg = ":IRC_SERVER 353 " + this->NckName + " = " + channels.front() + " :@"+this->NckName+"\r\n";
            msg += ":IRC_SERVER 366 " + this->NckName + " " + channels.front() + " ::End of /NAMES list\r\n";
        }
        channels.pop();
    }

    send(this->ClntFd, msg.c_str(), msg.size(), 0);
    return (true);
}

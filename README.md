RetroShareChatBot
=================

C++ chat bot for RetroShare using the RPC interface.  
The bot uses an options.conf file for configuration and you can change settings from a control lobby in real-time (this changes are **not** saved)  
_Have a look at the options.conf file for an example configuration._

What is Retroshare?
-------------------
RetroShare is a secure decentralised communication platform:  
http://retroshare.sourceforge.net/

For more information on the RetroShare RPC system check out the git repository of its developer: 
https://github.com/drbob/pyrs#readme

Features:
---------
- written in C++ to run as a daemon
- automatically join specific lobbies / all lobbies available
- automatically create lobbies if they are not available
- automatic response system
- bridge to IRC (multiple channels and servers (TODO server))
- control lobby - control your bot through a (suggested private) chat lobby

The bot is also able to leave lobbies when someone writes a special phrase (e.g. "!kill")  
If desired the lobby can then be added to a blacklist and the bot will stop auto-joining it.  
_This options are all configurable!_

A few words to the code:
------------------------
This is my first C++ project ... so be gentle. Also i'm not familiar with IRC.
If you find odd things just look away (or tell me how to to it better) :P

Most things are tested (at least once) and the code should be quite stable - but i can't promise that it's 100% bug free.

There are several parts that are a horrible mess (like the IRC code) and i'd like to rewrite them in the future. But since they work i'll keep them for now.

Things you need:
----------------
- Protocol Buffers - Google's data interchange format (http://code.google.com/p/protobuf/)
- libircclient (http://www.ulduzsoft.com/libircclient/)
- c++11 capable compiler

The project is written in Code::Blocks and the Makefile was created with cbp2make (http://sourceforge.net/projects/cbp2make/)

How to build/run:
-----------------
__build:__ use Code::Blocks or just "make"  
__run:__ just run the executable in _bin/Debug_ / _bin/Release_ - there are no parameters (yet). The bot expects the options.conf to be in the same folder as the binary (for now)

Other things you might want to know:
------------------------------------
- For now the bot only supports __one__ IRC server (multiple channels are fine as long as they are on the same server)
- I've only tested "normal" public IRC lobby and i expect everything else to fail
- The options.conf file parser is not very robust - if you mess around with the file the bot will mostly crash

Control lobby commands:
-----------------------
_Every command starts with a special character (you can pick one in the options.conf file). I'll use '!' here_
- __!lobbies__ lists all available lobbies with their topic and an indicator showing if the bot has joined them
- __!options__ shows current settings
- __!commands__ shows available commands
- __!blacklist__ shows lobbies ion the lobby blacklist
- __!clearblacklist__ clears the lobby blacklist
- __!reload__/ __!restart__ restarts the bot (reloading of options.conf)
- __!off__ shuts down the bot

__Change settings__  
_all settings here requere a '1' for on or a '0' for off (e.g. "!autojoin 0")_  
_i use CamelCase here for better reading_
- __!autoResponse__ enable/disable auto reponse 
- __!autoCreate__ enable/disable lobby creation
- __!autoJoin__ enable/disable automatically joining of lobbies
- __!joinAllLobbies__ enable/disable automatically joining of __all__ lobbies (needs !autojoin being on )
- __!leaveLobbies__ enable/disable leaving (all) lobbies when (the bot) is shutdown
- __!leaveLobbyOnMsg__ enable/disable leaving on special phrase (like "!kill")
- __!useBlacklist__ enable/disable using blacklist when leaving lobby an command

#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

enum ReturnCode
{
    rc_NONE,
    rc_OK,
    rc_RESTART,
    rc_EXIT,
    rc_ERROR
};
enum cmdAction // not needed atm
{
    cmd_NONE,
    cmd_START,
    cmd_STOP
};

void startDaemon();
void stopDaemon();
void runChatBot();


#endif // MAIN_H_INCLUDED

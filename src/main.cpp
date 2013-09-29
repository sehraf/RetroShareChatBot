#include "main.h"

#include "ChatBot.h"

#include <iostream>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>   /* umask */
#include <errno.h>      /* errno */

#define DEBUG
#define NO_DAEMON

using namespace std;

int main(int argc, char* argv[])
{
    // nothing to do here for now ...
    startDaemon();

//    int c;
//    cmdAction action = cmd_START;
//
//    while((c = getopt(argc, argv,"a:p:u:")) != -1)
//    {
//        switch(c)
//        {
//        case 'u':
//            userName = optarg;
//            break;
//        case 'p':
//            password = optarg;
//            break;
//        case 'a':
//            if (strcmp(optarg, "start") == 0)
//                action = cmd_START;
//            else if (strcmp(optarg, "stop") == 0)
//                action = cmd_STOP;
//            break;
//        default:
//            fprintf (stdout, "Unknown option `-%c'.\n", c+53);
//        }
//    }

//    switch (action)
//    {
//    case cmd_NONE:
//        std::cout << "no action choosen -> shuting down" << std::endl;
//        break;
//    case cmd_START:
//        startDaemon();
//        break;
//    case cmd_STOP:
//        stopDaemon();
//        break;
//    default:
//        std::cerr << "HOW COULD DIS HAPPE (" << action << " should not exist)" << std::endl;
//        exit(EXIT_FAILURE);
//    }

    exit(EXIT_SUCCESS);
}

void startDaemon()
{

#ifdef NO_DAEMON
    runChatBot();
#else
    pid_t pid = -1, sid = -1;
    pid = fork();

    if (pid == 0)
    {
        // child
        umask(0);

        sid = setsid();
        if (sid < 0)
        {
            std::cerr << "startDaemon() error sid <  0" << std::endl;
            exit(EXIT_FAILURE);
        }

        chdir("/");

#ifndef DEBUG
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
#endif // DEBUG
        // start bot
        runChatBot();
    }
    else if (pid > 0)
    {
        // parrent
        std::cout << "startDaemon() created child with pid: " << pid << std::endl;
        exit(EXIT_SUCCESS);
    }
    else
    {
        // error
        std::cerr << "startDaemon() error while forking!" << std::endl;
        std::cerr << " -- errno: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }
#endif // NO_DAEMON
}

void stopDaemon()
{

}

void runChatBot()
{
    bool exit = false;
    ReturnCode rc;

    do
    {
        ChatBot* cb = new ChatBot();
        rc = cb->Run();

        switch(rc)
        {
        default:
        case ReturnCode::rc_NONE:
        case ReturnCode::rc_EXIT:
        case ReturnCode::rc_OK:
        case ReturnCode::rc_ERROR:
            // we are done clean up and bye
            delete cb;
            exit = true;
            break;
        case ReturnCode::rc_RESTART:
            // delete ChatBot but leave exit false
            delete cb;
            exit = false;
            sleep(5); // give the rpc server some time
            break;
        }

    }
    while(!exit);
}

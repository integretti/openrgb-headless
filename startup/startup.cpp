/*---------------------------------------------------------*\
| startup.cpp                                               |
|                                                           |
|   Startup for the OpenRGB headless server.                |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <chrono>
#include <thread>

#include "cli.h"
#include "ResourceManager.h"
#include "NetworkServer.h"
#include "startup.h"

#ifdef __linux__
#include <csignal>
#include <cstdlib>
#endif

/******************************************************************************************\
*                                                                                          *
*   Linux signal handler                                                                   *
*                                                                                          *
\******************************************************************************************/
#ifdef __linux__
void sigHandler(int s)
{
    std::signal(s, SIG_DFL);
    std::exit(0);
}
#endif

/******************************************************************************************\
*                                                                                          *
*   startup                                                                                *
*                                                                                          *
*       Boots the SDK server and blocks until it shuts down. There is no GUI in this       *
*       fork; see README.md for the rationale.                                             *
*                                                                                          *
\******************************************************************************************/
int startup(int /*argc*/, char* /*argv*/[], unsigned int ret_flags)
{
    /*-----------------------------------------------------*\
    | Wait for device detection to finish before serving    |
    | clients.                                              |
    \*-----------------------------------------------------*/
    ResourceManager::get()->WaitForInitialization();

    /*-----------------------------------------------------*\
    | Always run the SDK server in headless mode, even if   |
    | the CLI parser didn't explicitly request it (it sets  |
    | RET_FLAG_START_GUI when no args were given).          |
    \*-----------------------------------------------------*/
    ret_flags |= RET_FLAG_START_SERVER;

    NetworkServer* server = ResourceManager::get()->GetServer();
    if(server == nullptr)
    {
        return EXIT_FAILURE;
    }

    /*-----------------------------------------------------*\
    | Block forever — the SDK server runs on a background   |
    | thread, so we'd otherwise return immediately. Sleep   |
    | with a coarse poll instead of a tight loop.           |
    \*-----------------------------------------------------*/
    while(server->GetOnline())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return EXIT_SUCCESS;
}

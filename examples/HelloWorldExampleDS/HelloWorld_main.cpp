// Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file HelloWorld_main.cpp
 *
 */

#include "HelloWorldPublisher.h"
#include "HelloWorldSubscriber.h"
#include "HelloWorldServer.h"

#include <fastrtps/Domain.h>

#include <fastrtps/log/Log.h>

#include "optionparser.h"

struct Arg : public option::Arg
{
    static void print_error(const char* msg1, const option::Option& opt, const char* msg2)
    {
        fprintf(stderr, "%s", msg1);
        fwrite(opt.name, opt.namelen, 1, stderr);
        fprintf(stderr, "%s", msg2);
    }

    static option::ArgStatus Unknown(const option::Option& option, bool msg)
    {
        if (msg) print_error("Unknown option '", option, "'\n");
        return option::ARG_ILLEGAL;
    }

    static option::ArgStatus Required(const option::Option& option, bool msg)
    {
        if (option.arg != 0 && option.arg[0] != 0)
            return option::ARG_OK;

        if (msg) print_error("Option '", option, "' requires an argument\n");
        return option::ARG_ILLEGAL;
    }

    static option::ArgStatus Numeric(const option::Option& option, bool msg)
    {
        char* endptr = 0;
        if (option.arg != 0 && strtol(option.arg, &endptr, 10))
        {
        }
        if (endptr != option.arg && *endptr == 0)
        {
            return option::ARG_OK;
        }

        if (msg)
        {
            print_error("Option '", option, "' requires a numeric argument\n");
        }
        return option::ARG_ILLEGAL;
    }

    static option::ArgStatus String(const option::Option& option, bool msg)
    {
        if (option.arg != 0)
        {
            return option::ARG_OK;
        }
        if (msg)
        {
            print_error("Option '", option, "' requires a numeric argument\n");
        }
        return option::ARG_ILLEGAL;
    }
};

enum  optionIndex {
    UNKNOWN_OPT,
    HELP,
    SAMPLES,
    INTERVAL,
    TCP
};

const option::Descriptor usage[] = {
    { UNKNOWN_OPT, 0,"", "",                Arg::None,
        "Usage: HelloWorldExampleDS <publisher|subscriber|server>\n\nGeneral options:" },
    { HELP,    0,"h", "help",               Arg::None,      "  -h \t--help  \tProduce help message." },
    { TCP,0,"t","tcp",                   Arg::None,
        "  -t \t--tcp \tUse tcp transport instead of the default UDP one." },
    { SAMPLES,0,"c","count",              Arg::Numeric,
        "  -c <num>, \t--count=<num>  \tNumber of datagrams to send (0 = infinite) defaults to 10." },
    { INTERVAL,0,"i","interval",            Arg::Numeric,
        "  -i <num>, \t--interval=<num>  \tTime between samples in milliseconds (Default: 100)." },
    { 0, 0, 0, 0, 0, 0 }
};

using namespace eprosima;
using namespace fastrtps;
using namespace rtps;
int main(int argc, char** argv)
{
    int columns;

    #if defined(_WIN32)
        char* buf = nullptr;
        size_t sz = 0;
        if (_dupenv_s(&buf, &sz, "COLUMNS") == 0 && buf != nullptr)
        {
            columns = strtol(buf, nullptr, 10);
            free(buf);
        }
        else
        {
            columns = 80;
        }
    #else
        columns = getenv("COLUMNS") ? atoi(getenv("COLUMNS")) : 80;
    #endif

    std::cout << "Starting "<< std::endl;
    int type = 1;
    int count = 20;
    long sleep = 100;
    bool use_tpc = false;

    if(argc > 1)
    {
        if (strcmp(argv[1], "publisher") == 0)
        {
            type = 1;
        }
        else if (strcmp(argv[1], "subscriber") == 0)
        {
            type = 2;
        }
        else if (strcmp(argv[1], "server") == 0)
        {
            type = 3;
        }
        else
        {
            option::printUsage(fwrite, stdout, usage, columns);
            return 0;
        }

        argc -= (argc > 0);
        argv += (argc > 0); // skip program name argv[0] if present
        --argc; ++argv; // skip pub/sub argument
        option::Stats stats(usage, argc, argv);
        std::vector<option::Option> options(stats.options_max);
        std::vector<option::Option> buffer(stats.buffer_max);
        option::Parser parse(usage, argc, argv, &options[0], &buffer[0]);

        if (parse.error())
        {
            return 1;
        }

        for (int i = 0; i < parse.optionsCount(); ++i)
        {
            option::Option& opt = buffer[i];
            switch (opt.index())
            {
            case HELP:
                option::printUsage(fwrite, stdout, usage, columns);
                return 0;

            case SAMPLES:
                count = strtol(opt.arg, nullptr, 10);
                break;

            case INTERVAL:
                sleep = strtol(opt.arg, nullptr, 10);
                break;

            case TCP:
                use_tpc = true;
                break;

            case UNKNOWN_OPT:
                option::printUsage(fwrite, stdout, usage, columns);
                return 0;
                break;
           
            }
        }

    }
    else
    {
        std::cout << "publisher, subscriber OR server argument needed" << std::endl;
        Log::Reset();
        return 0;
    }

    switch(type)
    {
        case 1:
            {
                HelloWorldPublisher mypub;
                if(mypub.init(use_tpc))
                {
                    mypub.run(count, sleep);
                }
                break;
            }
        case 2:
            {
                HelloWorldSubscriber mysub;
                if(mysub.init(use_tpc))
                {
                    mysub.run();
                }
                break;
            }
        case 3:
        {
            HelloWorldServer myserver;
            if (myserver.init(use_tpc))
            {
                myserver.run();
            }
            break;
        }
    }
    Log::Flush();
    Domain::stopAll();
    return 0;
}

#include "calk.hpp"
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <print>
#include <readline/history.h>
#include <readline/readline.h>

struct AltScreen
{
    AltScreen()
    {
        std::print("\033[?1049h");
        std::print("\033[2J\033[H");
    }
    ~AltScreen()
    {
        std::print("\033[?1049l");
    }
};

using namespace calk;

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        str argument = argv[1];
        run(argument);
    }
    else
    {
        AltScreen alt;
        while (true)
        {
            char *line = readline("> ");
            if (!line)
                return 0;

            str input = line;
            std::free(line);

            if (input.empty())
                continue;
            add_history(input.c_str());

            if (input[0] == ':')
            {
                if (input[1] == 'q')
                    return 0;
                else if (input[1] == 'v')
                {
                    for (const auto &[varName, varValue] : currentContext.variables)
                        std::println("{} = {}", varName, varValue);

                    continue;
                }
                else if (input[1] == 'f')
                {
                    for (const auto &[funcName, funcObjs] : currentContext.functions)
                    {
                        std::print("{}( ", funcName);
                        for (const auto &arg : funcObjs.args)
                            std::print("{} ", arg);
                        std::println(") ={}", funcObjs.expression);
                    }
                    continue;
                }
                else if (input[1] == 'c')
                {
                    std::print("\033[2J\033[H");
                    continue;
                }
                else
                {
                    std::println(stderr, "unrecognized command");
                    continue;
                }
            }
            else if (input[0] == '!')
            {
                i32 histTemp = 1;
                std::from_chars(input.data() + 1, input.data() + input.size(), histTemp);
                u32 histIndex = currentContext.history.size() - histTemp;
                if (histIndex < currentContext.history.size())
                {
                    std::println("{} = {}", currentContext.history[histIndex].first,
                                 currentContext.history[histIndex].second);
                }
                else
                    std::println(stderr, "invalid history index");
                continue;
            }
            run(input);
        }
    }
    return 0;
}

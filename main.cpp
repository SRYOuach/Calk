#include "calk.hpp"
#include <charconv>
#include <iostream>
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
        while (true)
        {
            str input;
            std::cout << "> ";
            if (!std::getline(std::cin, input))
                return 0;
            if (input.empty())
                continue;
            if (input[0] == ':')
            {
                if (input[1] == 'q')
                    return 0;
                else if (input[1] == 'v')
                {
                    for (const auto &[varName, varValue] : currentContext.variables)
                        std::cout << varName << " = " << varValue << std::endl;

                    continue;
                }
                else if (input[1] == 'f')
                {
                    for (const auto &[funcName, funcObjs] : currentContext.functions)
                    {
                        std::cout << funcName << "( ";
                        for (const auto &arg : funcObjs.args)
                            std::cout << arg << " ";
                        std::cout << ") =" << funcObjs.expression << std::endl;
                    }
                    continue;
                }
                else
                {
                    std::cerr << "unrecognized command" << std::endl;
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
                    std::cout << currentContext.history[histIndex].first << " = "
                              << currentContext.history[histIndex].second << std::endl;
                }
                else
                    std::cerr << "invalid history index" << std::endl;
                continue;
            }
            run(input);
        }
    }
    return 0;
}

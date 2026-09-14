#include "calk.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <print>
#include <stdexcept>
#include <sys/syscall.h>
#include <unordered_map>
#include <variant>

using namespace std::string_literals;

namespace calk
{
std::unordered_map<str, f64> builtVariables = {{"PI", 3.141592653589793238}, {"e", 2.71828182845904}, {"ans", 0}};
std::unordered_map<str, f64 (*)(f64)> builtFunctions = {
    {"sin", sin}, {"cos", cos}, {"tan", tan}, {"ln", log}, {"sqrt", sqrt}};
Context currentContext;

f64 Operator::calculate(f64 operand1, f64 operand2)
{
    if (binary)
    {
        switch (operation)
        {
        case '+':
            return operand1 + operand2;
            break;
        case '-':
            return operand1 - operand2;
            break;
        case '*':
            return operand1 * operand2;
            break;
        case '/':
            return operand1 / operand2;
            break;
        case '%':
            return (i32)operand1 % (i32)operand2;
            break;
        case '^':
            return pow(operand1, operand2);
            break;
        default:
            throw std::runtime_error("unrecognized operator "s + operation);
            break;
        }
    }
    else
    {
        switch (operation)
        {
        case '-':
            return -operand1;
            break;
        case '+':
            return operand1;
            break;
        default:
            throw std::runtime_error("unrecognized operator "s + operation);
            break;
        }
    }
}

f64 NumToken::getValue() const
{
    return value;
}
f64 calk::VarToken::getValue() const
{
    if (builtVariables.contains(name))
    {
        return builtVariables[name];
    }
    else if (!currentContext.inputVars.empty() && currentContext.inputVars.top().contains(name))
    {
        return currentContext.inputVars.top()[name];
    }
    else if (currentContext.variables.contains(name))
    {
        return currentContext.variables[name];
    }
    else
    {
        throw std::runtime_error("unrecognized variable");
    }
}
f64 OpToken::getValue()
{
    if (!op.binary)
    {
        operand2 = 0;
    }
    return op.calculate(operand1, operand2);
}
f64 FuncToken::getValue()
{
    if (alreadyCalclated)
        return result;

    f64 temp = evaluate(parse(func->tokenizedExpression));
    alreadyCalclated = true;
    return temp;
}
f64 BuiltFuncToken::getValue()
{
    return func(operand);
}
f64 MultiValueToken::getValue(u32 index) const
{
    return evaluate(valueNodes[index]);
}

vec<Token> Tokenize(const str &expr, vec<str> *forgiven)
{
    u32 i = 0;
    vec<Token> tokens;

    while (i < expr.size())
    {
        if (std::isspace(expr[i]))
        {
            i++;
            continue;
        }

        if (std::isdigit(expr[i]) || expr[i] == '.')
        {
            u32 start = i;
            bool seenDot = false;
            while (i < expr.size() && (std::isdigit(expr[i]) || expr[i] == '.'))
            {
                if (!seenDot && expr[i] == '.')
                {
                    seenDot = true;
                }
                else if (seenDot && expr[i] == '.')
                {
                    throw std::runtime_error("syntax error");
                }
                i++;
            }
            str numStr = expr.substr(start, i - start);
            if (expr[start] == '.')
                numStr = '0' + numStr;

            f64 number;
            std::from_chars(numStr.data(), numStr.data() + numStr.size(), number);

            // check if the number is a scalar to a scope or a variable
            auto previousVar = (tokens.empty()) ? nullptr : std::get_if<VarToken>(&tokens.back());
            auto previousSym = (tokens.empty()) ? nullptr : std::get_if<SymbToken>(&tokens.back());

            if ((previousSym && previousSym->symbol == ')') || previousVar)
                tokens.push_back(OpToken(Operator{.binary = true, .priority = 1, .operation = '*'}));

            tokens.push_back(NumToken(number));

            continue;
        }

        if (std::isalpha(expr[i]))
        {
            auto previousSym = (tokens.empty()) ? nullptr : std::get_if<SymbToken>(&tokens.back());
            auto previousVar = (tokens.empty()) ? nullptr : std::get_if<VarToken>(&tokens.back());
            auto previousNum = (tokens.empty()) ? nullptr : std::get_if<NumToken>(&tokens.back());

            u32 start = i;
            while (i < expr.size() && std::isalpha(expr[i]))
            {
                i++;
            }
            str temp = expr.substr(start, i - start);
            if (forgiven && std::ranges::contains(*forgiven, temp))
            {
                // check if the function is preceeded by a scalar
                if (previousVar || previousNum || (previousSym && previousSym->symbol == ')'))
                    tokens.push_back(OpToken(Operator{.binary = true, .priority = 1, .operation = '*'}));
                tokens.push_back(VarToken(temp));
                continue;
            }
            if (builtFunctions.contains(temp))
            {
                // check if the function is preceeded by a scalar
                if (previousVar || previousNum || (previousSym && previousSym->symbol == ')'))
                    tokens.push_back(OpToken(Operator{.binary = true, .priority = 1, .operation = '*'}));
                tokens.push_back(BuiltFuncToken(builtFunctions[temp]));
                continue;
            }
            else if (builtVariables.contains(temp))
            {
                // check if the variable is a scalar to a scope
                if (previousNum || previousVar || (previousSym && previousSym->symbol == ')'))
                    tokens.push_back(OpToken(Operator{.binary = true, .priority = 1, .operation = '*'}));
                tokens.push_back(VarToken(temp));
                continue;
            }
            else if (currentContext.functions.contains(temp))
            {
                // check if the function is preceeded by a scalar
                if (previousVar || previousNum || (previousSym && previousSym->symbol == ')'))
                    tokens.push_back(OpToken(Operator{.binary = true, .priority = 1, .operation = '*'}));

                tokens.push_back(FuncToken(currentContext.functions[temp]));
                continue;
            }
            else if (!currentContext.inputVars.empty() && currentContext.inputVars.top().contains(temp))
            {
                // check if the variable is a scalar to a scope
                if (previousVar || previousNum || (previousSym && previousSym->symbol == ')'))
                    tokens.push_back(OpToken(Operator{.binary = true, .priority = 1, .operation = '*'}));
                tokens.push_back(VarToken(temp));
                continue;
            }
            else if (currentContext.variables.contains(temp))
            {
                // check if the variable is a scalar to a scope
                if (previousVar || previousNum || (previousSym && previousSym->symbol == ')'))
                    tokens.push_back(OpToken(Operator{.binary = true, .priority = 1, .operation = '*'}));
                tokens.push_back(VarToken(temp));
                continue;
            }
            else
            {
                throw std::runtime_error("unrecognized identifier: " + temp);
            }
        }

        auto previousOP = (tokens.empty()) ? nullptr : std::get_if<OpToken>(&tokens.back());
        auto previousSYM = (tokens.empty()) ? nullptr : std::get_if<SymbToken>(&tokens.back());

        switch (expr[i])
        {
        case '+': {
            if (i == 0 || previousOP || (previousSYM && previousSYM->symbol == '('))
                tokens.push_back(OpToken(Operator({.binary = false, .priority = 3, .operation = '+'})));
            else
                tokens.push_back(OpToken(Operator{.binary = true, .priority = 0, .operation = '+'}));
            break;
        }
        case '-': {
            if (i == 0 || previousOP || (previousSYM && previousSYM->symbol == '('))
                tokens.push_back(OpToken(Operator({.binary = false, .priority = 3, .operation = '-'})));
            else
                tokens.push_back(OpToken(Operator{.binary = true, .priority = 0, .operation = '-'}));
            break;
        }
        case '*':
            tokens.push_back(OpToken(Operator({.binary = true, .priority = 1, .operation = '*'})));
            break;
        case '/':
            tokens.push_back(OpToken(Operator({.binary = true, .priority = 1, .operation = '/'})));
            break;
        case '%':
            tokens.push_back(OpToken(Operator({.binary = true, .priority = 1, .operation = '%'})));
            break;
        case '^':
            tokens.push_back(OpToken(Operator({.binary = true, .priority = 2, .operation = '^'})));
            break;
        case '(': {
            // check if the previous token is a scalar
            auto prevNum = (tokens.empty()) ? nullptr : std::get_if<NumToken>(&tokens.back());
            auto prevVar = (tokens.empty()) ? nullptr : std::get_if<VarToken>(&tokens.back());
            auto prevSym = (tokens.empty()) ? nullptr : std::get_if<SymbToken>(&tokens.back());
            if (prevNum || prevVar || (prevSym && prevSym->symbol == ')'))
                tokens.push_back(OpToken(Operator{.binary = true, .priority = 1, .operation = '*'}));

            tokens.push_back(SymbToken('('));
            break;
        }
        case ')':
            tokens.push_back(SymbToken(')'));
            break;
        case ',':
            tokens.push_back(SymbToken(','));
            break;
        default:
            throw std::runtime_error("unrecognized symbol/operator "s + expr[i]);
            break;
        }
        i++;
    }
    return tokens;
}

u32 peek(vec<Token> &tokens, u32 index, u32 end, u32 priority)
{
    i32 scopeIndex = 0;
    for (u32 i = index; i <= end; i++)
    {
        auto current = std::get_if<OpToken>(&tokens[i]);
        auto scope = std::get_if<SymbToken>(&tokens[i]);
        if (scope)
        {
            if (scope->symbol == '(')
                scopeIndex++;
            else if (scope->symbol == ')')
                scopeIndex--;
        }
        else if (current)
        {
            if (current->op.priority == priority && scopeIndex == 0)
                return i;
        }
    }
    return -1;
}

u32 peek(vec<Token> &tokens, u32 index, u32 end, char symbol)
{
    i32 scopeIndex = 0;
    for (u32 i = index; i <= end; i++)
    {
        auto current = std::get_if<SymbToken>(&tokens[i]);
        if (current)
        {
            if (current->symbol == '(')
                scopeIndex++;
            else if (current->symbol == ')')
                scopeIndex--;
            else if (current->symbol == symbol && scopeIndex == 0)
                return i;
        }
    }
    return -1;
}

uptr<Node> parse(vec<Token> &tokens, u32 start, u32 end, u32 priority)
{
    // preconditions and guard clauses
    end = (end == -1) ? tokens.size() - 1 : end;
    if (start > end)
    {
        throw std::runtime_error("empty expression error");
    }
    auto tempstart = std::get_if<SymbToken>(&tokens[start]);
    auto tempend = std::get_if<SymbToken>(&tokens[end]);

    if ((tempstart && tempstart->symbol != '(') || (tempend && tempend->symbol != ')'))
        throw std::runtime_error("syntax error: misplaced symbol");

    // check for enclosed parenthesis and reset priority
    // check for incomplete parenthesis pairs and throw
    bool isScoped = false;
    vec<bool> scopes;
    i32 scopesTracker = 0;
    for (u32 i = start; i <= end; i++)
    {
        auto tempcurrent = std::get_if<SymbToken>(&tokens[i]);
        if ((tempstart && tempstart->symbol == '(') && (tempend && tempend->symbol == ')') &&
            (i == end && (scopes.size() > 0 && scopes[0] == false)))
        {
            scopesTracker++;
            if ((i32)scopes.size() - scopesTracker < 0)
                throw std::runtime_error("syntax error: missing parenthesis");

            scopes[scopes.size() - scopesTracker] = true;
            isScoped = true;
            priority = 0;
            start++;
            end--;
        }
        else if (tempcurrent && tempcurrent->symbol == '(')
        {
            scopesTracker = 0;
            scopes.push_back(false);
        }
        else if (tempcurrent && tempcurrent->symbol == ')')
        {
            scopesTracker++;
            if ((i32)scopes.size() - scopesTracker < 0)
                throw std::runtime_error("syntax error: missing parenthesis");

            scopes[scopes.size() - scopesTracker] = true;
        }
    }
    if (scopes.size() > 0 && !scopes.back())
        throw std::runtime_error("syntax error: missing parenthesis");

    // function logic begins
    uptr<Node> current = nullptr;

    // check and handle the case where the given expression is the argument list for a multivariable function
    u32 currentColonIndex = start;
    u32 nextColonIndex = peek(tokens, currentColonIndex, end, ',');
    if (nextColonIndex != (u32)-1 && !isScoped)
        throw std::runtime_error("syntax error: colon outside parenthesis");

    if (nextColonIndex != (u32)-1)
    {
        current = std::make_unique<Node>(std::make_unique<MultiValueToken>());
        while (nextColonIndex != (u32)-1)
        {
            nextColonIndex = peek(tokens, currentColonIndex, end, ',');
            current->multiValue->valueNodes.push_back(
                parse(tokens, currentColonIndex, (nextColonIndex == (u32)-1) ? end : nextColonIndex - 1, 0));
            currentColonIndex = nextColonIndex + 1;
        }
        return current;
    }

    // parse based on mathematical operation priority level
    if (priority < 3)
    {
        u32 index = start;
        while (index <= end)
        {
            u32 nextIndex = peek(tokens, index, end, priority);

            if (nextIndex == end)
                throw std::runtime_error("syntax error: missing right operand");

            if (!current)
            {
                if (nextIndex == (u32)-1)
                {
                    current = parse(tokens, start, end, priority + 1);
                    break;
                }
                else
                {
                    current = std::make_unique<Node>(tokens[nextIndex], parse(tokens, start, nextIndex - 1));
                    index = nextIndex + 1;
                }
            }
            else
            {
                if (nextIndex == (u32)-1)
                {
                    current->right = parse(tokens, index, end, priority + 1);
                    break;
                }
                else
                {
                    current->right = parse(tokens, index, nextIndex - 1, priority + 1);
                    uptr<Node> next = std::make_unique<Node>(tokens[nextIndex], std::move(current));
                    current = std::move(next);
                    index = nextIndex + 1;
                }
            }
        }
        if (!current)
            current = parse(tokens, start, end, priority + 1);
        return current;
    }
    else if (priority == 3)
    {
        auto unaryOp = std::get_if<OpToken>(&tokens[start]);

        if (unaryOp && !unaryOp->op.binary)
        {
            current = std::make_unique<Node>(tokens[start], nullptr);
            current->right = parse(tokens, start + 1, end, priority);
            return current;
        }
        else
        {
            current = parse(tokens, start, end, priority + 1);
            return current;
        }
    }
    else if (priority == 4)
    {
        auto funcUser = std::get_if<FuncToken>(&tokens[start]);
        auto funcBuilt = std::get_if<BuiltFuncToken>(&tokens[start]);
        if (!funcUser && !funcBuilt)
            return parse(tokens, start, end, priority + 1);
        else
        {
            current = std::make_unique<Node>(tokens[start], nullptr, nullptr);
            current->right = parse(tokens, start + 1, end, priority + 1);
            return current;
        }
    }
    else if (priority == 5)
    {
        if (end - start > 1)
            throw std::runtime_error("syntax error: expected a single variable/number");

        auto isVar = std::get_if<VarToken>(&tokens[start]);
        auto isNum = std::get_if<NumToken>(&tokens[start]);

        if (isVar || isNum)
        {
            current = std::make_unique<Node>(tokens[start]);
            return current;
        }
        else
            throw std::runtime_error("syntax error: missing expected variable/number");
    }
    else
        throw std::runtime_error("unrecognized priority level");
}

f64 evaluate(const uptr<Node> &parent)
{
    auto isNum = std::get_if<NumToken>(&parent->token);
    auto isVar = std::get_if<VarToken>(&parent->token);
    auto isOp = std::get_if<OpToken>(&parent->token);
    auto isBuiltFunc = std::get_if<BuiltFuncToken>(&parent->token);
    auto isFunc = std::get_if<FuncToken>(&parent->token);

    if (isNum)
    {
        return isNum->getValue();
    }
    else if (isVar)
    {
        return isVar->getValue();
    }
    else if (isOp)
    {
        if (isOp->op.binary)
        {
            isOp->operand1 = evaluate(parent->left);
            isOp->operand2 = evaluate(parent->right);
        }
        else
            isOp->operand1 = evaluate(parent->right);
        return isOp->getValue();
    }
    else if (isBuiltFunc)
    {
        isBuiltFunc->operand = evaluate(parent->right);
        return isBuiltFunc->getValue();
    }
    else if (isFunc)
    {
        vec<f64> tempValues;
        u32 argsCount = isFunc->func->args.size();

        if (argsCount == 0)
            throw std::runtime_error("logical error: user defined function with no arguments");
        else if (argsCount == 1)
        {
            tempValues.push_back(evaluate(parent->right));
            currentContext.inputVars.push({});
            currentContext.inputVars.top()[isFunc->func->args[0]] = tempValues[0];
        }
        else
        {
            if (!parent->right->multiValue || parent->right->multiValue->valueNodes.size() < argsCount)
                throw std::runtime_error("missing function argument error");

            for (u32 u = 0; u < argsCount; u++)
            {
                tempValues.push_back(evaluate(parent->right->multiValue->valueNodes[u]));
            }
            currentContext.inputVars.push({});
            for (u32 u = 0; u < argsCount; u++)
                currentContext.inputVars.top()[isFunc->func->args[u]] = tempValues[u];
        }
        f64 result = isFunc->getValue();
        currentContext.inputVars.pop();
        return result;
    }
    else
        throw std::runtime_error("evaluation error : unrecognized token");
}

void assign(str LHS, str RHS)
{
    // format and syntax check the LHS (either a function declaration or a variable declaration)
    // remove all whitespaces from the left hand side
    auto [first, last] = std::ranges::remove_if(LHS, [](unsigned char c) { return std::isspace(c); });
    LHS.erase(first, last);

    // if the left hand side is a function declaration
    if (LHS.contains(')') || LHS.contains('('))
    {
        u32 i = 0;
        // confirm the LHS ends with a left parenthesis
        if (LHS.back() != ')')
            throw std::runtime_error("left hand side syntax error");

        // get the index where the function name ends, and confirm it is immediately followed by a right parenthesis
        while (std::isalpha(LHS[i]))
        {
            i++;
        }
        u32 funcEndIndex = i;
        if (funcEndIndex == 0 || LHS[i] != '(')
            throw std::runtime_error("left hand side syntax error");

        str funcName = LHS.substr(0, funcEndIndex);
        if (builtVariables.contains(funcName) || builtFunctions.contains(funcName))
            throw std::runtime_error("you can't override built in variables/functions");

        vec<str> funcArgs;

        // slice the content of the parenthesis and push the input variable names into the arguments vector
        u32 prevArgColon = funcEndIndex + 1;
        for (i = prevArgColon; i < LHS.size() - 1; i++)
        {
            if (LHS[i] != ',' && !std::isalpha(LHS[i]))
                throw std::runtime_error("left hand sign syntax error");

            if (LHS[i] == ',')
            {
                if (i == prevArgColon || i == LHS.size() - 2)
                    throw std::runtime_error("left hand side syntax error");

                funcArgs.push_back(LHS.substr(prevArgColon, i - prevArgColon));
                prevArgColon = i + 1;
            }
            if (i == LHS.size() - 2)
                funcArgs.push_back(LHS.substr(prevArgColon, i - prevArgColon + 1));
        }
        vec<Token> funcExpr = Tokenize(RHS, &funcArgs);
        currentContext.functions[funcName] = Function{funcArgs, RHS, funcExpr};
    }
    else
    {
        u32 i = 0;
        while (i < LHS.size())
        {
            if (!std::isalpha(LHS[i]))
                throw std::runtime_error("left hand size syntax error");

            i++;
        }
        str varName = LHS.substr(0, 9);
        if (builtVariables.contains(varName) || builtFunctions.contains(varName))
            throw std::runtime_error("you can't override built in variables/functions");

        vec<Token> varExpr = Tokenize(RHS);
        currentContext.variables[varName] = evaluate(parse(varExpr));
    }
}

void run(const str &expr)
{
    u32 equalIndex = -1;
    for (u32 i = 0; i < expr.size(); i++)
    {
        if (expr[i] == '=' && equalIndex != (u32)-1)
            throw std::runtime_error("syntax error: multiple equal signs");
        else if (expr[i] == '=' && equalIndex == (u32)-1)
            equalIndex = i;
    }
    try
    {
        if (equalIndex == (u32)-1)
        {
            vec<Token> parsed_expression = Tokenize(expr);
            f64 result = evaluate(parse(parsed_expression));
            builtVariables["ans"] = result;
            currentContext.history.push_back({expr, result});
            std::println("{}", result);
        }
        else
        {
            assign(expr.substr(0, equalIndex - 1), expr.substr(equalIndex + 1));
        }
    }
    catch (const std::runtime_error &e)
    {
        std::print(stderr, "{}", e.what());
        return;
    }
}
} // namespace calk

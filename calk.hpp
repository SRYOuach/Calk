// --- priority system ---
// priority 0 : addition + subtracion
// priority 1 : multiplication + division + modulo
// priority 2 : exponents
// priority 3 : exponents
// priority 4 : functions (user defined + built in)
// priority 5 : variables + numbers

#pragma once

#include <cassert>
#include <cctype>
#include <cmath>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using f32 = float;
using f64 = double;
using str = std::string;
using u32 = unsigned int;
using u64 = unsigned long int;
using i32 = int;
using i64 = long int;
template <typename T> using uptr = std::unique_ptr<T>;
template <typename T> using shptr = std::shared_ptr<T>;
template <typename T> using vec = std::vector<T>;

namespace calk
{
struct Node;

struct Operator;
struct Function;

struct NumToken;
struct VarToken;
struct SymbToken;
struct OpToken;
struct FuncToken;
struct BuiltFuncToken;
struct MultiValueToken;

using Token = std::variant<NumToken, VarToken, SymbToken, OpToken, FuncToken, BuiltFuncToken>;

struct Operator
{
    bool binary;
    u32 priority;

    char operation;
    f64 calculate(f64 operand1, f64 operand2 = 0);
};
struct Function
{
    vec<str> args;
    str expression;
    vec<Token> tokenizedExpression;
};

struct NumToken
{
    f64 value;
    NumToken(f64 v = 0.0) : value(v)
    {
    }
    f64 getValue() const;
};
struct VarToken
{
    str name;
    VarToken(str name) : name(name)
    {
    }
    f64 getValue() const;
};
struct SymbToken
{
    char symbol;
    SymbToken(char sym) : symbol(sym)
    {
    }
};
struct OpToken
{
    Operator op;
    f64 operand1;
    f64 operand2;
    OpToken(Operator oper) : op(oper)
    {
    }
    f64 getValue();
};
struct FuncToken
{
    shptr<Function> func;
    bool alreadyCalclated = false;
    f64 result;
    FuncToken(Function f) : func(std::make_shared<Function>(std::move(f)))
    {
    }
    f64 getValue();
};
struct BuiltFuncToken
{
    f64 operand;
    f64 (*func)(f64);
    f64 getValue();
    BuiltFuncToken(f64 (*f)(f64))
    {
        func = f;
    }
};

struct MultiValueToken
{
    vec<uptr<Node>> valueNodes;
    f64 getValue(u32 index) const;
};

struct Node
{
    Token token;
    uptr<MultiValueToken> multiValue = nullptr;

    uptr<Node> left = nullptr;
    uptr<Node> right = nullptr;

    Node(Token t, uptr<Node> l = nullptr, uptr<Node> r = nullptr)
        : token(std::move(t)), left(std::move(l)), right(std::move(r))
    {
    }
    Node(uptr<MultiValueToken> mv) : multiValue(std::move(mv))
    {
    }
};

struct Context
{
    std::unordered_map<str, Function> functions;
    std::unordered_map<str, f64> variables;
    std::stack<std::unordered_map<str, f64>> inputVars;
    vec<std::pair<str, f64>> history;
};

extern std::unordered_map<str, f64> builtVariables;
extern std::unordered_map<str, Operator> operators;
extern std::unordered_map<str, f64 (*)(f64)> builtFunctions;
extern Context currentContext;

vec<Token> Tokenize(const str &expr, vec<str> *forgiven = nullptr);

u32 peek(vec<Token> &tokens, u32 index, u32 end, u32 priority);
u32 peek(vec<Token> &tokens, u32 index, u32 end, char symbol);

uptr<Node> parse(vec<Token> &tokens, u32 start = 0, u32 end = -1, u32 priority = 0);
f64 evaluate(const uptr<Node> &parent);
void assign(str LHS, str RHS);

void run(const str &expr);
}; // namespace calk

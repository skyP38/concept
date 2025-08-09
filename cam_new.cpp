#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <stack>
#include <variant>

using namespace std;

// ==================== AST ====================
struct Term;
using TermPtr = shared_ptr<Term>;

struct Term {
    virtual ~Term() = default;
    virtual string toString() const = 0;
    virtual TermPtr toDeBruijn(int level = 0, const unordered_map<string, int>& env = {}) const = 0;
    virtual TermPtr reduce() const = 0;
    virtual bool isValue() const { return false; }
};

struct Variable : Term {
    string name;
    int deBruijnIdx = -1;
    
    Variable(string n) : name(n) {}
    
    string toString() const override {
        if (deBruijnIdx >= 0) return "#" + to_string(deBruijnIdx);
        return name;
    }
    
    TermPtr toDeBruijn(int level, const unordered_map<string, int>& env) const override {
        auto newVar = make_shared<Variable>(*this);
        if (env.count(name)) {
            newVar->deBruijnIdx = level - env.at(name) - 1;
        } else {
            throw runtime_error("Unbound variable: " + name);
        }
        return newVar;
    }
    
    TermPtr reduce() const override { return make_shared<Variable>(*this); }
    bool isValue() const override { return true; }
};

struct Number : Term {
    int value;
    Number(int v) : value(v) {}
    
    string toString() const override { return to_string(value); }
    
    TermPtr toDeBruijn(int level, const unordered_map<string, int>& env) const override {
        return make_shared<Number>(*this);
    }
    
    TermPtr reduce() const override { return make_shared<Number>(*this); }
    bool isValue() const override { return true; }
};

struct Lambda : Term {
    string param;
    TermPtr body;
    
    Lambda(string p, TermPtr b) : param(p), body(b) {}
    
    string toString() const override { 
        return "(λ" + param + "." + body->toString() + ")"; 
    }
    
    TermPtr toDeBruijn(int level, const unordered_map<string, int>& env) const override {
        unordered_map<string, int> new_env = env;
        new_env[param] = level;
        return make_shared<Lambda>(param, body->toDeBruijn(level + 1, new_env));
    }
    
    TermPtr reduce() const override {
        return make_shared<Lambda>(param, body->reduce());
    }
    
    bool isValue() const override { return true; }
};

struct Application : Term {
    TermPtr func;
    TermPtr arg;
    
    Application(TermPtr f, TermPtr a) : func(f), arg(a) {}
    
    string toString() const override { 
        return "(" + func->toString() + " " + arg->toString() + ")"; 
    }
    
    TermPtr toDeBruijn(int level, const unordered_map<string, int>& env) const override {
        return make_shared<Application>(
            func->toDeBruijn(level, env), 
            arg->toDeBruijn(level, env));
    }
    
    TermPtr reduce() const override {
        if (auto lam = dynamic_pointer_cast<Lambda>(func)) {
            return substitute(lam->body, lam->param, arg)->reduce();
        }
        if (!func->isValue()) {
            return make_shared<Application>(func->reduce(), arg);
        }
        if (!arg->isValue()) {
            return make_shared<Application>(func, arg->reduce());
        }
        return make_shared<Application>(func, arg);
    }
    
    static TermPtr substitute(TermPtr term, const string& var, TermPtr replacement) {
        if (auto v = dynamic_pointer_cast<Variable>(term)) {
            if (v->name == var) return replacement;
            return term;
        }
        if (auto num = dynamic_pointer_cast<Number>(term)) {
            return term;
        }
        if (auto lam = dynamic_pointer_cast<Lambda>(term)) {
            if (lam->param == var) return term;
            auto newBody = substitute(lam->body, var, replacement);
            return make_shared<Lambda>(lam->param, newBody);
        }
        if (auto app = dynamic_pointer_cast<Application>(term)) {
            auto newFunc = substitute(app->func, var, replacement);
            auto newArg = substitute(app->arg, var, replacement);
            return make_shared<Application>(newFunc, newArg);
        }
        return term;
    }
};

struct BinaryOp : Term {
    char op;
    TermPtr left;
    TermPtr right;
    
    BinaryOp(char o, TermPtr l, TermPtr r) : op(o), left(l), right(r) {}
    
    string toString() const override {
        return "(" + left->toString() + " " + op + " " + right->toString() + ")";
    }
    
    TermPtr toDeBruijn(int level, const unordered_map<string, int>& env) const override {
        return make_shared<BinaryOp>(op, 
            left->toDeBruijn(level, env), 
            right->toDeBruijn(level, env));
    }
    
    TermPtr reduce() const override {
        if (!left->isValue()) {
            return make_shared<BinaryOp>(op, left->reduce(), right);
        }
        if (!right->isValue()) {
            return make_shared<BinaryOp>(op, left, right->reduce());
        }
        if (auto lnum = dynamic_pointer_cast<Number>(left)) {
            if (auto rnum = dynamic_pointer_cast<Number>(right)) {
                if (op == '+') return make_shared<Number>(lnum->value + rnum->value);
                if (op == '*') return make_shared<Number>(lnum->value * rnum->value);
            }
        }
        return make_shared<BinaryOp>(op, left, right);
    }
};

// ==================== CAM Machine ====================
enum class CAMCommand {
    PUSH,
    GRAB,
    ACCESS,
    APPLY,
    ADD,
    MUL,
    RETURN,
    JUMP,
    JUMP_IF,
    LOOP,
    HALT
};

struct CAMState {
    vector<pair<CAMCommand, int>> code;
    vector<int> env;
    vector<int> dump;
};

class CAMMachine {
    vector<pair<CAMCommand, int>> code_stack;
    vector<int> env_stack;
    vector<vector<int>> dump_stack;  
    stack<int> eval_stack;

    void executeCommand(const pair<CAMCommand, int>& instruction) {
        auto [cmd, arg] = instruction;
        
        switch (cmd) {
            case CAMCommand::PUSH:
                eval_stack.push(arg);
                break;
                
           case CAMCommand::GRAB: {
                if (eval_stack.empty()) throw runtime_error("Stack underflow");
                int arg_val = eval_stack.top();
                eval_stack.pop();
                
                // текущее состояние
                vector<int> dump_item;
                dump_item.push_back(code_stack.size());  // Позиция в коде
                dump_item.push_back(env_stack.size());  // Размер окружения
                dump_item.insert(dump_item.end(), env_stack.begin(), env_stack.end());
                dump_stack.push_back(dump_item);
                
                env_stack.insert(env_stack.begin(), arg_val);
                break;
            }

            case CAMCommand::ACCESS: {
                if (arg < 0 || arg >= env_stack.size()) {
                    throw runtime_error("Variable access out of bounds: index " + 
                                     to_string(arg) + " in env of size " + 
                                     to_string(env_stack.size()));
                }
                eval_stack.push(env_stack[arg]);
                break;
            }
                
            case CAMCommand::APPLY: {
                // передача управления следующей команде
                break;
            }
                
            case CAMCommand::RETURN: {
                if (eval_stack.empty()) throw runtime_error("Stack underflow");
                int result = eval_stack.top();
                eval_stack.pop();
                
                if (dump_stack.empty()) {
                    eval_stack.push(result);
                    code_stack.clear();
                    return;
                }
                
                vector<int> dump_item = dump_stack.back();
                dump_stack.pop_back();
                
                auto it = dump_item.begin();
                int code_pos = *it++;
                int env_size = *it++;
                vector<int> saved_env(it, it + env_size);
                
                env_stack = saved_env;
                
                if (code_pos < code_stack.size()) {
                    code_stack.erase(code_stack.begin(), code_stack.begin() + code_pos);
                }
                
                eval_stack.push(result);
                break;
            }
                
            case CAMCommand::ADD: {
                if (eval_stack.size() < 2) throw runtime_error("Stack underflow");
                int b = eval_stack.top(); eval_stack.pop();
                int a = eval_stack.top(); eval_stack.pop();
                eval_stack.push(a + b);
                break;
            }
                
            case CAMCommand::MUL: {
                if (eval_stack.size() < 2) throw runtime_error("Stack underflow");
                int b = eval_stack.top(); eval_stack.pop();
                int a = eval_stack.top(); eval_stack.pop();
                eval_stack.push(a * b);
                break;
            }
                
            default:
                throw runtime_error("Unknown command");
        }
    }

public:
    int execute(const vector<pair<CAMCommand, int>>& program) {
        code_stack = program;
        env_stack.clear();
        dump_stack.clear();
        eval_stack = stack<int>();
        
        while (!code_stack.empty()) {
            auto instruction = code_stack.front();
            code_stack.erase(code_stack.begin());
            executeCommand(instruction);
        }
        
        if (eval_stack.size() != 1) {
            throw runtime_error("Invalid final stack state");
        }
        return eval_stack.top();
    }
};

// =================== CAM Compiler ====================
class CAMCompiler {
    vector<pair<CAMCommand, int>> compileTerm(TermPtr term, int env_size = 0) {
        vector<pair<CAMCommand, int>> code;
        
        if (auto var = dynamic_pointer_cast<Variable>(term)) {
            // Проверяем, что индекс переменной в пределах окружения
            if (var->deBruijnIdx >= env_size) {
                throw runtime_error("Variable access out of bounds during compilation");
            }
            code.emplace_back(CAMCommand::ACCESS, var->deBruijnIdx);
        }
        else if (auto num = dynamic_pointer_cast<Number>(term)) {
            code.emplace_back(CAMCommand::PUSH, num->value);
        }
        else if (auto lam = dynamic_pointer_cast<Lambda>(term)) {
            code.emplace_back(CAMCommand::GRAB, 0);
            auto body_code = compileTerm(lam->body, env_size + 1);
            code.insert(code.end(), body_code.begin(), body_code.end());
            code.emplace_back(CAMCommand::RETURN, 0);
        }
        else if (auto app = dynamic_pointer_cast<Application>(term)) {
            // Сначала компилируем аргумент, затем функцию
            auto arg_code = compileTerm(app->arg, env_size);
            auto func_code = compileTerm(app->func, env_size);
            code.insert(code.end(), arg_code.begin(), arg_code.end());
            code.insert(code.end(), func_code.begin(), func_code.end());
            code.emplace_back(CAMCommand::APPLY, 0);
        }
        else if (auto binop = dynamic_pointer_cast<BinaryOp>(term)) {
            auto left_code = compileTerm(binop->left, env_size);
            auto right_code = compileTerm(binop->right, env_size);
            code.insert(code.end(), left_code.begin(), left_code.end());
            code.insert(code.end(), right_code.begin(), right_code.end());
            code.emplace_back(binop->op == '+' ? CAMCommand::ADD : CAMCommand::MUL, 0);
        }
        
        return code;
    }

    public:
    vector<pair<CAMCommand, int>> compile(TermPtr term) {
        auto dbTerm = term->toDeBruijn();
        return compileTerm(dbTerm);
    }
};

// ==================== Parser ====================
class Parser {
    string input;
    size_t pos = 0;
    
    void skipWhitespace() {
        while (pos < input.size() && isspace(input[pos])) pos++;
    }
    
    char peek() {
        skipWhitespace();
        if (pos >= input.size()) return '\0';
        return input[pos];
    }
    
    char next() {
        skipWhitespace();
        if (pos >= input.size()) throw runtime_error("Unexpected end of input");
        return input[pos++];
    }
    
    TermPtr parseTerm() {
        if (peek() != '(') {
            if (isdigit(peek())) {
                return parseNumber();
            } else if (isalpha(peek())) {
                return parseVariable();
            }
            throw runtime_error("Unexpected character");
        }
        
        next(); // consume '('
        if (peek() == 'l' && input.substr(pos, 6) == "lambda") {
            pos += 6;
            return parseLambda();
        }
        
        auto left = parseTerm();
        char c = peek();
        
        if (c == '+' || c == '*') {
            next();
            auto right = parseTerm();
            if (next() != ')') throw runtime_error("Expected ')'");
            return make_shared<BinaryOp>(c, left, right);
        } else {
            auto arg = parseTerm();
            if (next() != ')') throw runtime_error("Expected ')'");
            return make_shared<Application>(left, arg);
        }
    }
    
    TermPtr parseLambda() {
        string param;
        while (isalpha(peek())) param += next();
        
        if (param.empty()) throw runtime_error("Expected parameter name after lambda");
        if (next() != '.') throw runtime_error("Expected '.' after lambda parameter");
        
        auto body = parseTerm();
        if (next() != ')') throw runtime_error("Expected ')' after lambda body");
        return make_shared<Lambda>(param, body);
    }
    
    TermPtr parseNumber() {
        string num;
        while (isdigit(peek())) num += next();
        return make_shared<Number>(stoi(num));
    }
    
    TermPtr parseVariable() {
        string var;
        while (isalpha(peek())) var += next();
        return make_shared<Variable>(var);
    }

public:
    TermPtr parse(const string& str) {
        input = str;
        pos = 0;
        auto term = parseTerm();
        if (pos != input.size()) throw runtime_error("Extra input at end");
        return term;
    }
};

// ==================== Main ====================
int main() {
    Parser parser;
    CAMCompiler compiler;
    CAMMachine machine;
    
    vector<string> tests = {
        "42",
        "(1 + 2)",
        "((lambda x. x) 42)",
        "((lambda x. (x + 1)) 42)",
        "((lambda x. (x * 2)) 11)",
        "((lambda x. ((lambda y. (x + y)) 10)) 32)"
    };
    
    for (const auto& test : tests) {
        cout << "Testing: " << test << endl;
        try {
            TermPtr term = parser.parse(test);
            cout << "Parsed: " << term->toString() << endl;
            
            auto dbTerm = term->toDeBruijn();
            cout << "DeBruijn: " << dbTerm->toString() << endl;
            
            auto reduced = term->reduce();
            cout << "Reduced: " << reduced->toString() << endl;
            
            auto program = compiler.compile(term);
            
            cout << "CAM program: ";
            for (auto [cmd, arg] : program) {
                switch (cmd) {
                    case CAMCommand::PUSH: cout << "PUSH(" << arg << ") "; break;
                    case CAMCommand::GRAB: cout << "GRAB "; break;
                    case CAMCommand::ACCESS: cout << "ACCESS(" << arg << ") "; break;
                    case CAMCommand::APPLY: cout << "APPLY "; break;
                    case CAMCommand::ADD: cout << "ADD "; break;
                    case CAMCommand::MUL: cout << "MUL "; break;
                    case CAMCommand::RETURN: cout << "RETURN "; break;
                    case CAMCommand::JUMP: cout << "JUMP(" << arg << ") "; break;
                    case CAMCommand::JUMP_IF: cout << "JUMP_IF(" << arg << ") "; break;
                    case CAMCommand::LOOP: cout << "LOOP(" << arg << ") "; break;
                    case CAMCommand::HALT: cout << "HALT "; break;
                }
            }
            cout << endl;
            
            int result = machine.execute(program);
            cout << "Result: " << result << endl;
        } catch (const exception& e) {
            cerr << "Error: " << e.what() << endl;
        }
        cout << "-------------------" << endl;
    }
    
    return 0;
}
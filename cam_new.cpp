#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <variant>
#include <stdexcept>
#include <sstream>
#include <cctype>
#include <unordered_map>

using namespace std;

// AST Node Declarations
struct Term;
struct Variable;
struct Number;
struct Lambda;
struct Application;
struct BinaryOp;

using TermPtr = shared_ptr<Term>;

struct Term {
    virtual ~Term() = default;
    virtual string toString() const = 0;
};

struct Variable : Term {
    string name;
    Variable(string n) : name(n) {}
    string toString() const override { return name; }
};

struct Number : Term {
    int value;
    Number(int v) : value(v) {}
    string toString() const override { return to_string(value); }
};

struct Lambda : Term {
    string param;
    TermPtr body;
    Lambda(string p, TermPtr b) : param(p), body(b) {}
    string toString() const override { 
        return "(lambda." + param + " " + body->toString() + ")"; 
    }
};

struct Application : Term {
    TermPtr func;
    TermPtr arg;
    Application(TermPtr f, TermPtr a) : func(f), arg(a) {}
    string toString() const override { 
        return "(" + func->toString() + " " + arg->toString() + ")"; 
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
};

// Parser
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
    
    bool match(const string& s) {
        size_t old_pos = pos;
        try {
            for (char c : s) {
                if (next() != c) {
                    pos = old_pos;
                    return false;
                }
            }
            return true;
        } catch (...) {
            pos = old_pos;
            return false;
        }
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
        if (match("lambda.")) {
            return parseLambda();
        }
        
        // Application or BinaryOp
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
        
        if (param.empty()) {
            throw runtime_error("Expected parameter name after lambda.");
        }
        
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

// Runtime Value
struct Closure {
    string param;
    TermPtr body;
    unordered_map<string, int> env;
};

using RuntimeValue = variant<int, Closure>;

// Interpreter
class Interpreter {
    unordered_map<string, int> env;
    
    RuntimeValue evaluate(TermPtr term) {
        if (auto num = dynamic_pointer_cast<Number>(term)) {
            return num->value;
        } else if (auto var = dynamic_pointer_cast<Variable>(term)) {
            auto it = env.find(var->name);
            if (it == env.end()) throw runtime_error("Unbound variable: " + var->name);
            return it->second;
        } else if (auto lam = dynamic_pointer_cast<Lambda>(term)) {
            return Closure{lam->param, lam->body, env};
        } else if (auto app = dynamic_pointer_cast<Application>(term)) {
            auto func_val = evaluate(app->func);
            auto arg_val = evaluate(app->arg);
            
            if (holds_alternative<Closure>(func_val)) {
                auto closure = get<Closure>(func_val);
                if (!holds_alternative<int>(arg_val)) {
                    throw runtime_error("Function argument must be an integer");
                }
                
                // Create new environment with the argument bound
                unordered_map<string, int> new_env = closure.env;
                new_env[closure.param] = get<int>(arg_val);
                
                // Evaluate the body with the new environment
                Interpreter new_interp;
                new_interp.env = new_env;
                return new_interp.evaluate(closure.body);
            }
            throw runtime_error("Application of non-function");
        } else if (auto binop = dynamic_pointer_cast<BinaryOp>(term)) {
            auto left_val = evaluate(binop->left);
            auto right_val = evaluate(binop->right);
            
            if (!holds_alternative<int>(left_val) || !holds_alternative<int>(right_val)) {
                throw runtime_error("Binary operation on non-integers");
            }
            
            int left = get<int>(left_val);
            int right = get<int>(right_val);
            
            switch (binop->op) {
                case '+': return left + right;
                case '*': return left * right;
                default: throw runtime_error("Unknown operator");
            }
        }
        throw runtime_error("Unknown term type");
    }

public:
    int execute(TermPtr term) {
        auto result = evaluate(term);
        if (holds_alternative<int>(result)) {
            return get<int>(result);
        }
        throw runtime_error("Expected integer result");
    }
};

int main() {
    Parser parser;
    Interpreter interpreter;
    
    vector<string> tests = {
        "((lambda.x (x + 1)) 43)",
        "((lambda.x (x * 2)) 21)", 
        "((lambda.x ((lambda.y (x + y)) 10)) 32)"
    };
    
    for (const auto& test : tests) {
        cout << "Testing: " << test << endl;
        try {
            TermPtr term = parser.parse(test);
            int result = interpreter.execute(term);
            cout << "Result: " << result << endl;
        } catch (const exception& e) {
            cerr << "Error: " << e.what() << endl;
        }
    }
    
    return 0;
}
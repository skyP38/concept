#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <sstream>
#include <iomanip>
#include <variant>
#include <functional>
#include <map>

// === Базовые структуры данных ===
struct Term;
using TermPtr = std::shared_ptr<Term>;

struct Term {
    enum Type { EMPTY, ATOM, PAIR, CLOSURE, NUMBER, QUOTE };
    Type type;
    
    // Значения
    std::string atom_value;
    int number_value;
    TermPtr first;
    TermPtr second;
    std::vector<std::string> closure_code;
    TermPtr closure_env;
    TermPtr quoted_term;
    
    // Конструкторы
    Term(Type t) : type(t) {}
    Term(const std::string& value) : type(ATOM), atom_value(value) {}
    Term(int value) : type(NUMBER), number_value(value) {}
    Term(TermPtr f, TermPtr s) : type(PAIR), first(f), second(s) {}
    Term(const std::vector<std::string>& code, TermPtr env) 
        : type(CLOSURE), closure_code(code), closure_env(env) {}
    Term(TermPtr term) : type(QUOTE), quoted_term(term) {}
    
    std::string toString() const {
        switch (type) {
            case EMPTY: return "()";
            case ATOM: return atom_value;
            case NUMBER: return std::to_string(number_value);
            case PAIR: return "[" + first->toString() + ", " + second->toString() + "]";
            case CLOSURE: return "Λ(" + std::to_string(closure_code.size()) + ")";
            case QUOTE: return "'" + quoted_term->toString();
            default: return "unknown";
        }
    }
};

// Фабричные функции
TermPtr make_empty() { return std::make_shared<Term>(Term::EMPTY); }
TermPtr make_atom(const std::string& name) { return std::make_shared<Term>(name); }
TermPtr make_number(int value) { return std::make_shared<Term>(value); }
TermPtr make_pair(TermPtr first, TermPtr second) { return std::make_shared<Term>(first, second); }
TermPtr make_closure(const std::vector<std::string>& code, TermPtr env) { 
    return std::make_shared<Term>(code, env); 
}
TermPtr make_quote(TermPtr term) { return std::make_shared<Term>(term); }

// === Инструкции КАМ ===
struct Instruction {
    enum Type { 
        PUSH, SWAP, CONS, 
        CAR, CDR, 
        QUOTE, 
        CUR, APP,
        ACCESS
    };
    
    Type type;
    TermPtr term;
    std::vector<std::string> code;
    int index;
    
    Instruction(Type t) : type(t), term(nullptr), index(-1) {}
    Instruction(Type t, TermPtr term) : type(t), term(term), index(-1) {}
    Instruction(Type t, const std::vector<std::string>& code) : type(t), code(code), index(-1) {}
    Instruction(Type t, int index) : type(t), term(nullptr), index(index) {}
    
    std::string toString() const {
        switch (type) {
            case PUSH: return "push" + (term ? " " + term->toString() : "");
            case SWAP: return "swap";
            case CONS: return "cons";
            case CAR: return "car";
            case CDR: return "cdr";
            case QUOTE: return "quote" + (term ? " " + term->toString() : "");
            case CUR: return "cur";
            case APP: return "app";
            case ACCESS: return "access[" + std::to_string(index) + "]";
            default: return "unknown";
        }
    }
};

using Code = std::vector<Instruction>;

// === Состояние КАМ ===
struct State {
    TermPtr term;
    Code code;
    std::vector<TermPtr> stack;
    
    void print(int step) const {
        std::cout << std::setw(2) << step << " | "
                  << std::setw(20) << (term ? term->toString() : "()") << " | "
                  << std::setw(30);
        
        std::string codeStr;
        for (const auto& instr : code) {
            codeStr += instr.toString() + " ";
        }
        if (codeStr.empty()) codeStr = "ε";
        std::cout << codeStr << " | [";
        
        for (size_t i = 0; i < stack.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << (stack[i] ? stack[i]->toString() : "()");
        }
        std::cout << "]" << std::endl;
    }
};

// === Реализация КАМ ===
class CAMMachine {
private:
    State state;
    int stepCount;
    
    TermPtr car(TermPtr term) {
        if (term && term->type == Term::PAIR) {
            return term->first;
        }
        return make_empty();
    }
    
    TermPtr cdr(TermPtr term) {
        if (term && term->type == Term::PAIR) {
            return term->second;
        }
        return make_empty();
    }
    
    void execute_instruction(const std::string& instr_str) {
        if (instr_str == "car") {
            state.code.insert(state.code.begin(), Instruction(Instruction::CAR));
        } else if (instr_str == "cdr") {
            state.code.insert(state.code.begin(), Instruction(Instruction::CDR));
        } else if (instr_str == "access[0]") {
            state.code.insert(state.code.begin(), Instruction(Instruction::ACCESS, 0));
        } else if (instr_str == "access[1]") {
            state.code.insert(state.code.begin(), Instruction(Instruction::ACCESS, 1));
        } else if (instr_str.rfind("push ", 0) == 0) {
            // Обработка push инструкций
            std::string value_str = instr_str.substr(5);
            try {
                int value = std::stoi(value_str);
                state.code.insert(state.code.begin(), Instruction(Instruction::PUSH, make_number(value)));
            } catch (...) {
                state.code.insert(state.code.begin(), Instruction(Instruction::PUSH, make_atom(value_str)));
            }
        }
    }
    
public:
    CAMMachine(TermPtr initial_term, Code initial_code = {}) {
        state.term = initial_term;
        state.code = initial_code;
        state.stack = {};
        stepCount = 0;
    }
    
    bool step() {
        if (state.code.empty()) return false;
        
        printState();
        
        Instruction current = state.code.front();
        state.code.erase(state.code.begin());
        
        switch (current.type) {
            case Instruction::PUSH:
                state.stack.push_back(state.term);
                state.term = current.term;
                break;
                
            case Instruction::SWAP:
                if (!state.stack.empty()) {
                    TermPtr top = state.stack.back();
                    state.stack.pop_back();
                    state.stack.push_back(state.term);
                    state.term = top;
                }
                break;
                
            case Instruction::CONS:
                if (!state.stack.empty()) {
                    TermPtr first = state.stack.back();
                    state.stack.pop_back();
                    state.term = make_pair(first, state.term);
                }
                break;
                
            case Instruction::CAR:
                state.term = car(state.term);
                break;
                
            case Instruction::CDR:
                state.term = cdr(state.term);
                break;
                
            case Instruction::QUOTE:
                state.term = make_quote(current.term);
                break;
                
            case Instruction::CUR:
                state.term = make_closure(current.code, state.term);
                break;
                
            case Instruction::APP:
                if (state.term && state.term->type == Term::PAIR) {
                    TermPtr func = car(state.term);
                    TermPtr arg = cdr(state.term);
                    
                    if (func && func->type == Term::CLOSURE) {
                        // Сохраняем текущее состояние
                        state.stack.push_back(state.term);
                        
                        // Устанавливаем окружение замыкания
                        state.term = func->closure_env;
                        
                        // Добавляем код замыкания в обратном порядке
                        for (auto it = func->closure_code.rbegin(); it != func->closure_code.rend(); ++it) {
                            execute_instruction(*it);
                        }
                        
                        // Добавляем аргумент в стек
                        state.stack.push_back(arg);
                    }
                }
                break;
                
            case Instruction::ACCESS:
                if (current.index >= 0 && current.index < state.stack.size()) {
                    state.term = state.stack[state.stack.size() - 1 - current.index];
                }
                break;
        }
        
        stepCount++;
        return !state.code.empty();
    }
    
    void printState() const { state.print(stepCount); }
    
    void run() {
        std::cout << "Step |        Term         |             Code              | Stack" << std::endl;
        std::cout << "-----|---------------------|-------------------------------|------" << std::endl;
        while (step()) {}
        printState();
    }
    
    TermPtr getResult() const { return state.term; }
};

// === Тесты ===
void test_kam_basic() {
    std::cout << "=== Базовый тест КАМ: 2 + 3 ===" << std::endl;
    
    Code code = {
        Instruction(Instruction::PUSH, make_number(2)),
        Instruction(Instruction::PUSH, make_number(3)),
    };
    
    CAMMachine machine(make_empty(), code);
    machine.run();
    std::cout << "Результат: " << machine.getResult()->toString() << std::endl;
}

void test_kam_simple_closure() {
    std::cout << "=== Простое замыкание: const 5 ===" << std::endl;
    
    // Замыкание, которое всегда возвращает 5
    std::vector<std::string> const_code = {"push 5"};
    
    Code code = {
        Instruction(Instruction::CUR, const_code),
        Instruction(Instruction::PUSH, make_number(999)), // Любой аргумент
        Instruction(Instruction::CONS),
        Instruction(Instruction::APP)
    };
    
    CAMMachine machine(make_empty(), code);
    machine.run();
    std::cout << "Результат: " << machine.getResult()->toString() << std::endl;
}

void test_kam_identity() {
    std::cout << "=== Тождественное замыкание ===" << std::endl;
    
    // Замыкание, которое возвращает свой аргумент
    std::vector<std::string> identity_code = {"access[0]"};
    
    Code code = {
        Instruction(Instruction::CUR, identity_code),
        Instruction(Instruction::PUSH, make_number(42)),
        Instruction(Instruction::CONS),
        Instruction(Instruction::APP)
    };
    
    CAMMachine machine(make_empty(), code);
    machine.run();
    std::cout << "Результат: " << machine.getResult()->toString() << std::endl;
}

int main() {
    test_kam_basic();
    std::cout << std::endl;
    
    test_kam_simple_closure();
    std::cout << std::endl;
    
    test_kam_identity();
    std::cout << std::endl;
    
    return 0;
}
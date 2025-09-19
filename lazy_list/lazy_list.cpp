#include <iostream>
#include <functional>
#include <memory>
#include <vector>
#include <stdexcept>

template <typename T>
class LazyList {
private:
    struct Node {
        T head;
        std::function<std::shared_ptr<Node>()> tail_func;
        mutable std::shared_ptr<Node> cached_tail;
        mutable bool evaluated;
        
        Node(T head, std::function<std::shared_ptr<Node>()> tail_func)
            : head(head), tail_func(tail_func), cached_tail(nullptr), evaluated(false) {}
        
        std::shared_ptr<Node> get_tail() const {
            if (!evaluated) {
                cached_tail = tail_func();
                evaluated = true;
            }
            return cached_tail;
        }
    };
    
    std::shared_ptr<Node> node;
    
    // Приватный конструктор для внутреннего использования
    LazyList(std::shared_ptr<Node> node) : node(node) {}
    
public:
    LazyList() : node(nullptr) {}
    
    LazyList(T head, std::function<LazyList<T>()> tail_func) 
        : node(std::make_shared<Node>(head, [tail_func]() { 
            auto tail_list = tail_func();
            return tail_list.node; 
        })) {}
    
    bool empty() const { return !node; }
    
    T head() const {
        if (empty()) throw std::runtime_error("Empty list");
        return node->head;
    }
    
    LazyList<T> tail() const {
        if (empty()) throw std::runtime_error("Empty list");
        return LazyList<T>(node->get_tail());
    }
    
    // Взять первые n элементов
    LazyList<T> take(int n) const {
        if (n <= 0 || empty()) return LazyList<T>();
        return LazyList<T>(head(), [*this, n]() { 
            return this->tail().take(n - 1); 
        });
    }
    
    // Собрать первые n элементов в вектор
    std::vector<T> collect(int n) const {
        std::vector<T> result;
        LazyList<T> current = *this;
        
        for (int i = 0; i < n && !current.empty(); ++i) {
            result.push_back(current.head());
            current = current.tail();
        }
        
        return result;
    }
    
    // Вывести первые n элементов
    void print(int n) const {
        auto elements = collect(n);
        for (size_t i = 0; i < elements.size(); ++i) {
            std::cout << elements[i];
            if (i < elements.size() - 1) {
                std::cout << " ";
            }
        }
        std::cout << std::endl;
    }
};

// Генератор натуральных чисел
LazyList<int> natural_numbers(int start = 1) {
    return LazyList<int>(start, [start]() {
        return natural_numbers(start + 1);
    });
}

// Генератор чисел с шагом
LazyList<int> range(int start, int step = 1) {
    return LazyList<int>(start, [start, step]() {
        return range(start + step, step);
    });
}

// Генератор Фибоначчи
LazyList<int> fibonacci(int a = 0, int b = 1) {
    return LazyList<int>(a, [a, b]() {
        return fibonacci(b, a + b);
    });
}

int main() {
    try {
        std::cout << "First 10 natural numbers: ";
        natural_numbers().print(10);
        
        std::cout << "Even numbers: ";
        range(0, 2).print(10);
        
        std::cout << "Fibonacci numbers: ";
        fibonacci().print(10);
        
        std::cout << "First 5 squares: ";
        auto squares = natural_numbers().take(5).collect(5);
        for (int num : squares) {
            std::cout << num * num << " ";
        }
        std::cout << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
    
    return 0;
}
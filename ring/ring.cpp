#include <concepts>
#include <iostream>

// аддитивная полугруппа (замкнутость относительно сложения)
template <typename T>
concept AdditiveSemigroup = requires(T a, T b) {
    { a + b } -> std::same_as<T>;
};

// аддитивная группа (обратный элемент и нейтральный)
template <typename T>
concept AdditiveGroup = AdditiveSemigroup<T> && requires(T a) {
    { -a } -> std::same_as<T>;    // Обратный элемент
    { T(0) } -> std::same_as<T>;  // Нейтральный элемент
};

// мультипликативная полугруппа (замкнутость относительно умножения)
template <typename T>
concept MultiplicativeSemigroup = requires(T a, T b) {
    { a * b } -> std::same_as<T>;
};

// кольцо (аддитивная группа + мультипликативная полугруппа + дистрибутивность)
template <typename T>
concept Ring = AdditiveGroup<T> && MultiplicativeSemigroup<T> && requires(T a, T b, T c) {
    { a * (b + c) } -> std::same_as<T>;  // Дистрибутивность
    { (a + b) * c } -> std::same_as<T>;  // Дистрибутивность
};

// Пример класса, удовлетворяющего концепту кольца
class Integer {
    int value;
public:
    Integer(int v = 0) : value(v) {}
    
    // Операция сложения
    Integer operator+(const Integer& other) const {
        return Integer(value + other.value);
    }
    
    // Обратный элемент по сложению
    Integer operator-() const {
        return Integer(-value);
    }
    
    // Операция умножения
    Integer operator*(const Integer& other) const {
        return Integer(value * other.value);
    }
    
    // Нейтральный элемент по сложению (строго говоря не требуется для концепта)
    static Integer zero() {
        return Integer(0);
    }
    
    friend std::ostream& operator<<(std::ostream& os, const Integer& i) {
        return os << i.value;
    }
};

// Функция, работающая только с типами, удовлетворяющими концепту кольца
template <Ring R>
R ring_example(R a, R b, R c) {
    return a * (b + c) + (a + b) * c;
}

int main() {
    Integer a(3), b(4), c(5);
    
    std::cout << "Ring example with Integers:\n";
    std::cout << "a = " << a << ", b = " << b << ", c = " << c << "\n";
    std::cout << "a*(b+c) + (a+b)*c = " << ring_example(a, b, c) << "\n";
    
    std::cout << "\nRing example with built-in int:\n";
    int x = 2, y = 3, z = 4;
    std::cout << "x = " << x << ", y = " << y << ", z = " << z << "\n";
    std::cout << "x*(y+z) + (x+y)*z = " << ring_example(x, y, z) << "\n";
    
    return 0;
}
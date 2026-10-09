#include <gtest/gtest.h>

// Пример функции, которую мы хотим протестировать
int Add(int a, int b) {
    return a + b;
}

// Тест 1: Проверка базового сложения
TEST(MathFunctions, AdditionTrue) {
    // EXPECT_EQ проверяет равенство двух значений
    EXPECT_EQ(Add(2, 3), 5); 
}

// Тест 2: Проверка некорректного равенства
TEST(MathFunctions, AdditionFalse) {
    EXPECT_NE(Add(2, 2), 5); // EXPECT_NE ожидает, что значения НЕ равны
}

// Точка входа для запуска всех тестов
int test(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
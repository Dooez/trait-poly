#include "overloads/arguments.hpp"
#include "overloads/cv.hpp"
#include "overloads/ref.hpp"
#include "overloads/special_cases.hpp"

int main() {
    arguments::run();
    cv::run();
    ref::run();
    special_cases::run();
}

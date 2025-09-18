#include <iostream>
#include <ltfec/version.h>

int main() {
    std::cout << "fec_receiver " << ltfec::version() << "\n";
    return 0;
}

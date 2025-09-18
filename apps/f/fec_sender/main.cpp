#include <iostream>
#include <ltfec/version.h>

int main() {
    std::cout << "fec_sender " << ltfec::version() << "\n";
    return 0;
}
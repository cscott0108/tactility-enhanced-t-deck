#include "UartRelay.h"
#include <TactilityCpp/App.h>

extern "C" {

int main(int argc, char* argv[]) {
    registerApp<UartRelay>();
    return 0;
}

}

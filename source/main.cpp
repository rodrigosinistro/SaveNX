#include "SaveNX.hpp"

#include <switch.h>

int main()
{
    SaveNX savenx{};
    while (savenx.is_running())
    {
        savenx.update();
        savenx.render();
    }
    return 0;
}

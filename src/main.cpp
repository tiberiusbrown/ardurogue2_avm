#include <avm.h>

extern "C" int main()
{
    avm_set_frame_rate(60);

    for(;;)
    {
        if(avm_next_frame())
            avm_display(AVM_CLEAR_BUFFER);
    }
}

#include "mdr1986ve92qi_touch_ps2_mouse.h"

static mdr_touch_ps2_mouse_t g_touch_ps2;

int main(void)
{
    mdr_touch_ps2_mouse_init(&g_touch_ps2);

    while (1) {
        mdr_touch_ps2_mouse_poll(&g_touch_ps2);
    }
}

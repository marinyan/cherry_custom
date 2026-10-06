#include <assert.h>
#include <stdio.h>
#include "wheel-plan.h"
int main(void)
{
    WheelPlan p = PlanWheel(0, -120, 3);
    assert(p.count == 3 && p.command == 1 && p.remainder == 0);
    p = PlanWheel(0, 240, 3);
    assert(p.count == 6 && p.command == 0);
    p = PlanWheel(0, -40, 3);
    assert(p.count == 0 && p.remainder == -40);
    p = PlanWheel(p.remainder, -40, 3);
    assert(p.count == 0 && p.remainder == -80);
    p = PlanWheel(p.remainder, -40, 3);
    assert(p.count == 3 && p.remainder == 0);
    p = PlanWheel(80, -40, 3);
    assert(p.count == 0 && p.remainder == 40);
    p = PlanWheel(0, -120, 0);
    assert(p.count == 0);
    p = PlanWheel(0, -120, 0xffffffffu);
    assert(p.count == 1 && p.command == 3);
    p = PlanWheel(0, 240, 0xffffffffu);
    assert(p.count == 2 && p.command == 2);
    p = PlanWheel(0, 0, 3);
    assert(p.count == 0);
    p = PlanWheel(0, -32768, 0xfffffffeu);
    assert(p.count == 27300 && p.remainder == -8);
    puts("PASS: direction, multiple notches, partial deltas, reversal, zero, page and bounded settings");
    return 0;
}

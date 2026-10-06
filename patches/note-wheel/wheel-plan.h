/* Pure wheel arithmetic, also exercised by wheel-test.c. */
typedef struct WheelPlan { int remainder; unsigned int count; unsigned int command; } WheelPlan;
static WheelPlan PlanWheel(int remainder, int delta, unsigned int lines)
{
    WheelPlan plan;
    int total = remainder + delta;
    int steps = total / 120;
    plan.remainder = total % 120;
    plan.command = steps > 0 ? 0 : 1; /* SB_LINEUP / SB_LINEDOWN */
    plan.count = (unsigned int)(steps < 0 ? -steps : steps);
    if (lines == 0xffffffffu) {
        plan.command += 2; /* SB_PAGEUP / SB_PAGEDOWN */
    } else {
        /* Limit malformed settings without turning page-scroll into a huge loop. */
        if (lines > 100) lines = 100;
        plan.count *= lines;
    }
    return plan;
}

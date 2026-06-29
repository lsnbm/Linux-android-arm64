#pragma once

static inline int v_gyro_init(void)
{
    return 0;
}

static inline int v_gyro_report(int gyro_x, int gyro_y, int gyro_z)
{
    (void)gyro_x;
    (void)gyro_y;
    (void)gyro_z;
    return 0;
}

static inline void v_gyro_destroy(void)
{
}

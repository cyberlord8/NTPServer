#pragma once

void temp_init();
float read_temp_c();
float temp_ema_update_throttled(float sample_c);


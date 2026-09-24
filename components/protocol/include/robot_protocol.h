#pragma once

typedef enum {
    ROBOT_STATE_IDLE = 0,
    ROBOT_STATE_LISTENING,
    ROBOT_STATE_THINKING,
    ROBOT_STATE_SPEAKING,
    ROBOT_STATE_ERROR
} robot_state_t;

#include "mbot_controller.h"

rc_filter_t left_wheel_pid;
rc_filter_t right_wheel_pid;
rc_filter_t back_wheel_pid;
rc_filter_t mbot_vx_pid;
rc_filter_t mbot_vy_pid;
rc_filter_t mbot_wz_pid;

rc_filter_t vx_cmd_lpf;
rc_filter_t wz_cmd_lpf;

// Initializes the RC filters with the given configuration struct
// Only run this once
int mbot_init_ctlr(mbot_ctlr_cfg_t ctlr_cfg) {
    left_wheel_pid = rc_filter_empty();
    right_wheel_pid = rc_filter_empty();
    mbot_vx_pid = rc_filter_empty();
    mbot_wz_pid = rc_filter_empty();

    rc_filter_pid(&left_wheel_pid, ctlr_cfg.left.kp, ctlr_cfg.left.ki, ctlr_cfg.left.kd, ctlr_cfg.left.Tf, MAIN_LOOP_PERIOD);
    rc_filter_pid(&right_wheel_pid, ctlr_cfg.right.kp, ctlr_cfg.right.ki, ctlr_cfg.right.kd, ctlr_cfg.right.Tf, MAIN_LOOP_PERIOD);
    rc_filter_pid(&mbot_vx_pid, ctlr_cfg.vx.kp, ctlr_cfg.vx.ki, ctlr_cfg.vx.kd, ctlr_cfg.vx.Tf, MAIN_LOOP_PERIOD);
    rc_filter_pid(&mbot_wz_pid, ctlr_cfg.wz.kp, ctlr_cfg.wz.ki, ctlr_cfg.wz.kd, ctlr_cfg.wz.Tf, MAIN_LOOP_PERIOD);

    // Initialize vx and wz LPFs, yui
    vx_cmd_lpf = rc_filter_empty();
    wz_cmd_lpf = rc_filter_empty();
    rc_filter_first_order_lowpass(&vx_cmd_lpf, MAIN_LOOP_PERIOD, 0.25);
    rc_filter_first_order_lowpass(&wz_cmd_lpf, MAIN_LOOP_PERIOD, 0.25);

    #ifdef MBOT_TYPE_OMNI
    back_wheel_pid = rc_filter_empty();
    mbot_vy_pid = rc_filter_empty();
    rc_filter_pid(&mbot_vy_pid, ctlr_cfg.vy.kp, ctlr_cfg.vy.ki, ctlr_cfg.vy.kd, ctlr_cfg.vy.Tf, MAIN_LOOP_PERIOD);
    rc_filter_pid(&back_wheel_pid, ctlr_cfg.back.kp, ctlr_cfg.back.ki, ctlr_cfg.back.kd, ctlr_cfg.back.Tf, MAIN_LOOP_PERIOD);
    #endif

    return 0;
}

// Performs PID control on the motor velocities, given a desired wheel velocity and actual wheel velocity
int mbot_motor_vel_ctlr(serial_mbot_motor_vel_t vel_cmd, serial_mbot_motor_vel_t vel, serial_mbot_motor_pwm_t *mbot_motor_pwm) {
    // Find the errors
    float right_error = vel_cmd.velocity[MOT_R] - vel.velocity[MOT_R];
    float left_error = vel_cmd.velocity[MOT_L] - vel.velocity[MOT_L];

    // Find the PWM to correct for the errors using PID
    float right_cmd = rc_filter_march(&right_wheel_pid, right_error);
    float left_cmd = rc_filter_march(&left_wheel_pid, left_error);

    // Overwrite the sent PWM with the commanded PWM
    // As the values are overwritten, if you want to modify these values in some other way, it must be done after this function is called
    mbot_motor_pwm->pwm[MOT_R] = right_cmd;
    mbot_motor_pwm->pwm[MOT_L] = left_cmd;

    // Omni-specific code
    #ifdef MBOT_TYPE_OMNI
    float back_error = vel_cmd.velocity[MOT_B] - vel.velocity[MOT_B];
    float back_cmd = rc_filter_march(&back_wheel_pid, back_error);
    mbot_motor_pwm->pwm[MOT_B] = back_cmd;
    #endif

    return 0;
}

// Implements a controller for the MBot's body velocity, outputting the desired angular velocities of the motors
int mbot_ctlr(serial_twist2D_t vel_cmd, serial_twist2D_t vel, serial_mbot_motor_vel_t *mbot_motor_vel) {
    // TODO implement a controller for the MBot's body velocity
    float vx_lpf = rc_filter_march(&vx_cmd_lpf, vel_cmd.vx);
    float wz_lpf = rc_filter_march(&wz_cmd_lpf, vel_cmd.wz);

    // Find the errors
    // float vx_error = vel_cmd.vx - vel.vx;
    // float wz_error = vel_cmd.wz - vel.wz;
    float wz_error = wz_lpf - vel.wz;


    // Find the PWM to correct for the errors using PID
    // float vx_cmd = vel_cmd.vx + rc_filter_march(&mbot_vx_pid, vx_error);
    float wz_cmd = vel_cmd.wz + rc_filter_march(&mbot_wz_pid, wz_error);

    // mbot_motor_vel->velocity[MOT_L] = (vel_cmd.vx - DIFF_BASE_RADIUS * wz_cmd) / DIFF_WHEEL_RADIUS;
    // mbot_motor_vel->velocity[MOT_R] = (-vel_cmd.vx - DIFF_BASE_RADIUS * wz_cmd) / DIFF_WHEEL_RADIUS;

    mbot_motor_vel->velocity[MOT_L] = (vx_lpf - DIFF_BASE_RADIUS * wz_cmd) / DIFF_WHEEL_RADIUS;
    mbot_motor_vel->velocity[MOT_R] = (-vx_lpf - DIFF_BASE_RADIUS * wz_cmd) / DIFF_WHEEL_RADIUS;

    return 0;
}

"""Webots physics endpoint for the production XSFC host executable.

Current sensor boundary uses ideal attitude + gyro, NOT firmware fusion.
"""
import ctypes
import csv
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'sitl'))
sys.path.insert(0, str(ROOT / 'sitl/.venv/Lib/site-packages'))
os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = '1'
import pygame
from controller import Supervisor
from controller.wb import wb
from radio import ArmGate, binding_active, normalize, validate_profile


def main():
    robot = Supervisor()
    body = robot.getSelf()
    gyro = robot.getDevice('gyro')
    attitude = robot.getDevice('attitude_reference')
    gyro.enable(1)
    attitude.enable(1)
    keyboard = robot.getKeyboard()
    keyboard.enable(16)
    camera = robot.getDevice('fpv')
    camera.enable(32)
    pygame.display.init()
    pygame.joystick.init()
    profile = validate_profile(json.loads((ROOT / 'sitl/profiles/tango2-usb.json').read_text()))
    exe = subprocess.check_output(['wsl.exe', '--exec', 'wslpath', '-a', str(ROOT / 'sitl/build/host/xsfc-host')], text=True).strip()
    process = subprocess.Popen(['wsl.exe', '--exec', exe, '--bridge'], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, text=True, bufsize=1)
    gate = ArmGate()
    device = None
    motors = [0.] * 4
    last_arm_key = -math.inf
    angle_mode = True
    previous_mode_button = True
    buttons_ready = False
    bindings = profile.get('buttons', {})
    test = os.environ.get('XSFC_WEBOTS_TEST') == '1'
    print('XSFC endpoint started; scripted test:', test, flush=True)
    peak = 0.
    max_tilt = 0.
    max_torque = 0.
    samples = []
    responses = dict(roll=0., pitch=0., yaw=0.)
    recovery_tilt = 0.
    timing_start = time.perf_counter()
    timing_sim_start = robot.getTime()
    bridge_times = []
    simulation_speed = 1.
    telemetry_path = ROOT / 'sitl/build/webots-telemetry.csv'
    telemetry = telemetry_path.open('w', newline='')
    telemetry_writer = csv.writer(telemetry)
    telemetry_writer.writerow(['wall_s', 'sim_s', 'armed', 'angle', 'throttle', 'height_m',
                               'vertical_speed_m_s', 'thrust_N', 'pwm_RR', 'pwm_FR', 'pwm_RL', 'pwm_FL'])
    telemetry_start = time.monotonic()
    next_telemetry = 0.
    start_time = robot.getTime()
    try:
        while robot.step(1) != -1:
            pygame.event.pump()
            events = pygame.event.get()
            for event in events:
                if event.type == pygame.JOYDEVICEREMOVED:
                    device = None
                    gate.armed = False
                    buttons_ready = False
                    angle_mode = True
            if device is None:
                for index in range(pygame.joystick.get_count()):
                    candidate = pygame.joystick.Joystick(index)
                    if candidate.get_guid() == profile['guid']:
                        device = candidate
                        device.init()
                        break
            controls = None
            if device and device.get_init():
                try:
                    controls = normalize(profile, [device.get_axis(i) for i in range(device.get_numaxes())])
                except (ValueError, IndexError, pygame.error):
                    device = None
            key = keyboard.getKey()
            arm_button = mode_button = False
            if controls is not None and device and bindings:
                try:
                    axes = [device.get_axis(i) for i in range(device.get_numaxes())]
                    buttons = [device.get_button(i) for i in range(device.get_numbuttons())]
                    arm_button = binding_active(bindings['arm'], axes, buttons)
                    mode_button = binding_active(bindings['mode'], axes, buttons)
                    if not arm_button and not mode_button:
                        buttons_ready = True
                except (KeyError, IndexError, ValueError, pygame.error):
                    controls = None
                    buttons_ready = False
            if controls is None:
                buttons_ready = False
                angle_mode = True
            mode_binding = bindings.get('mode')
            mode_is_switch = isinstance(mode_binding, dict) and mode_binding.get('kind') == 'axis'
            if buttons_ready and mode_is_switch:
                angle_mode = not mode_button
            elif buttons_ready and mode_button and not previous_mode_button:
                angle_mode = not angle_mode
            previous_mode_button = mode_button
            now = time.monotonic()
            request = (buttons_ready and arm_button) or (key == ord(' ') and now - last_arm_key > .6)
            if key == ord(' '):
                last_arm_key = now
            armed = gate.update(now, controls is not None, controls, request)
            arm_binding = bindings.get('arm')
            arm_is_switch = isinstance(arm_binding, dict) and arm_binding.get('kind') == 'axis'
            if arm_is_switch and not arm_button:
                gate.armed = armed = False
            if key == 27:
                gate.armed = armed = False
            if key == ord('R') and not armed:
                body.getField('translation').setSFVec3f([0, 0, .1])
                body.getField('rotation').setSFRotation([0, 0, 1, 0])
                body.resetPhysics()
                motors = [0.] * 4
                process.stdin.close()
                process.wait(timeout=3)
                process = subprocess.Popen(['wsl.exe', '--exec', exe, '--bridge'], stdin=subprocess.PIPE,
                                           stdout=subprocess.PIPE, text=True, bufsize=1)
            if controls is None:
                controls = dict(roll=0., pitch=0., yaw=0., throttle=0.)
            t = robot.getTime() - start_time
            if test:
                armed = .3 < t < 5.5
                controls = dict(roll=.08 if 2 < t < 2.3 else 0.,
                                pitch=.08 if 3 < t < 3.3 else 0.,
                                yaw=.08 if 4 < t < 4.3 else 0.,
                                throttle=.34 if t < 1 else .278)
            g = gyro.getValues()
            e = attitude.getRollPitchYaw()
            # Remove the extra roll inversion reported during the USB flight check.
            # Keep the calibrated radio profile unchanged; conversion belongs here.
            values = [*g, *e, controls['roll'], -controls['pitch'], controls['yaw'], 2 * controls['throttle'] - 1, int(armed), int(angle_mode)]
            if not all(math.isfinite(v) for v in values):
                raise RuntimeError('Invalid sensor data')
            bridge_start = time.perf_counter()
            process.stdin.write(' '.join(map(str, values)) + '\n')
            process.stdin.flush()
            line = process.stdout.readline()
            bridge_times.append((time.perf_counter() - bridge_start) * 1000)
            pulses = list(map(float, line.split()))
            if len(pulses) != 4 or not all(900 <= p <= 2100 for p in pulses):
                raise RuntimeError('Bad XSFC motor reply: ' + line)
            # Synthetic linear command/thrust and 25ms lag, NOT measured 51466 data.
            for i in range(4):
                target = max(0., min(1., (pulses[i] - 1000) / 1000)) if armed else 0.
                motors[i] += (target - motors[i]) * (1 - math.exp(-.001 / .025))
            thrust = [m * 5 for m in motors]
            a = .225 / (2 * math.sqrt(2))
            # Production QUADX order: RR, FR, RL, FL. Webots FLU body.
            positions = [(-a, -a), (a, -a), (-a, a), (a, a)]
            torque = [sum(y * f for (x, y), f in zip(positions, thrust)),
                      sum(-x * f for (x, y), f in zip(positions, thrust)),
                      # Mixer.cpp negates yaw by default (yawReverse=false).
                      sum(s * f for s, f in zip([1, -1, -1, 1], thrust)) * .012]
            max_torque = max(max_torque, *(abs(v) for v in torque))
            body.addForce([0, 0, sum(thrust)], True)
            # R2025a node.py addTorque incorrectly calls wb_supervisor_node_add_force.
            # Use the actual C API; do not modify the installed simulator bindings.
            wb.wb_supervisor_node_add_torque(body._ref, (ctypes.c_double * 3)(*torque), 1)
            if int(t * 1000) % 100 == 0:
                robot.setLabel(0, f'XSFC Controller/Mixer | {"ARMED" if armed else "DISARMED"} | {"ANGLE" if angle_mode else "ACRO"} | USB {"OK" if controls is not None and device else "LOST"}\n'
                               f'Gas {controls["throttle"]:.0%} | SPACE arm/disarm | ESC disarm | R reset (disarmed)\n'
                               f'Height {body.getPosition()[2]:.2f}m | Vz {body.getVelocity()[2]:+.2f}m/s | Thrust {sum(thrust):.2f}N | Speed {simulation_speed:.2f}x\n'
                               'Ideal attitude; provisional 650g/225mm/thrust. Not full firmware SITL.',
                               .02, .02, .045, 0xffffff, 0, 'Arial')
            peak = max(peak, body.getPosition()[2])
            max_tilt = max(max_tilt, abs(e[0]), abs(e[1]))
            if t >= next_telemetry:
                telemetry_writer.writerow([time.monotonic() - telemetry_start, t, int(armed), int(angle_mode),
                                           controls['throttle'], body.getPosition()[2], body.getVelocity()[2],
                                           sum(thrust), *pulses])
                telemetry.flush()
                next_telemetry = t + .05
            if test:
                if 2 < t < 2.5:
                    responses['roll'] = max(responses['roll'], e[0])
                if 3 < t < 3.5:
                    responses['pitch'] = max(responses['pitch'], -e[1])
                if 4 < t < 4.5:
                    responses['yaw'] = max(responses['yaw'], -g[2])
                if 5 < t < 5.4:
                    recovery_tilt = max(recovery_tilt, abs(e[0]), abs(e[1]))
                if int(t * 1000) % 100 == 0:
                    samples.append(dict(time=t, position=body.getPosition(), attitude=e,
                                        gyro=g, pulses=pulses, thrust=sum(thrust)))
            if time.perf_counter() - timing_start >= 5:
                wall_elapsed = time.perf_counter() - timing_start
                ordered = sorted(bridge_times)
                timing = dict(simulation_speed=(robot.getTime() - timing_sim_start) / wall_elapsed,
                              bridge_median_ms=ordered[len(ordered) // 2],
                              bridge_p99_ms=ordered[min(len(ordered) - 1, int(len(ordered) * .99))],
                              bridge_max_ms=max(ordered), samples=len(ordered))
                simulation_speed = timing['simulation_speed']
                (ROOT / 'sitl/build/webots-timing.json').write_text(json.dumps(timing, indent=2))
                print('XSFC timing: ' + json.dumps(timing), flush=True)
                bridge_times.clear()
                timing_start = time.perf_counter()
                timing_sim_start = robot.getTime()
            if test and t >= 6:
                passed = (.3 < peak < 8 and .02 < max_tilt < .3
                          and responses['roll'] > .02 and responses['pitch'] > .02
                          and responses['yaw'] > .02 and recovery_tilt < .02
                          and pulses == [1000.] * 4)
                print(json.dumps(dict(test='flight', peak=peak, max_tilt=max_tilt, passed=passed)), flush=True)
                (ROOT / 'sitl/build/webots-flight-test.json').write_text(json.dumps(dict(time=t, peak=peak, max_tilt=max_tilt, max_torque=max_torque, responses=responses, recovery_tilt=recovery_tilt, rotation=body.getField('rotation').getSFRotation(), gyro=g, attitude=e, passed=passed, samples=samples), indent=2))
                robot.simulationQuit(0 if passed else 2)
                break
    finally:
        telemetry.close()
        process.stdin.close()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
        pygame.quit()


if __name__ == '__main__':
    main()

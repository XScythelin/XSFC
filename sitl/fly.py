"""Interactive Windows MuJoCo + production XSFC algorithm bridge (WSL g++)."""
import argparse
import json
from pathlib import Path
import subprocess
import time
import math
import numpy as np
import mujoco
import pygame

ROOT = Path(__file__).resolve().parent.parent


def build():
    def wp(p):
        return subprocess.check_output(['wsl.exe', '--exec', 'wslpath', '-a', str(p)], text=True).strip()
    exe = ROOT / 'sitl/build/xsfc-flight-bridge'
    exe.parent.mkdir(exist_ok=True)
    sources = ['sitl/bridge.cpp', 'lib/AHRS/src/Mahony.cpp', 'lib/Xsfc/src/Control/Pid.cpp', 'lib/Xsfc/src/Utils/Filter.cpp']
    subprocess.run(['wsl.exe', '--exec', 'g++', '-O2', '-std=c++17',
                    *['-I' + wp(ROOT / p) for p in ['lib/AHRS/src', 'lib/Xsfc/src', 'test/imu/stubs']],
                    *[wp(ROOT / p) for p in sources], '-o', wp(exe)], check=True)
    return wp(exe)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--headless', action='store_true')
    ap.add_argument('--seconds', type=float, default=0)
    ap.add_argument('--rate', type=int, choices=[2000, 4000, 8000], default=4000)
    args = ap.parse_args()
    exe = build()
    model = mujoco.MjModel.from_xml_path(str(ROOT / 'sitl/world.xml'))
    data = mujoco.MjData(model)
    mujoco.mj_forward(model, data)
    proc = subprocess.Popen(['wsl.exe', '--exec', exe, str(args.rate)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
    renderer = None
    screen = None
    camera = None
    pygame.init()
    joystick = None
    if not args.headless:
        screen = pygame.display.set_mode((1280, 720))
        pygame.display.set_caption('XSFC input: SPACE arm | arrows tilt | W/S throttle | A/D yaw | R reset | ESC quit')
        if pygame.joystick.get_count():
            joystick = pygame.joystick.Joystick(0); joystick.init()
            print('Joystick detected (not enabled until mapping confirmed):', joystick.get_name(), flush=True)
        renderer = mujoco.Renderer(model, height=720, width=1280)
        camera = mujoco.MjvCamera()
        camera.distance = 1.6
        camera.azimuth = 135
        camera.elevation = -25
        font = pygame.font.Font(None, 26)
    throttle = 0
    armed = False
    motor = np.zeros(4)
    peak = 0
    start = time.perf_counter()
    steps = 0
    controls = dict(roll=0, pitch=0, yaw=0)
    try:
        while not args.seconds or data.time < args.seconds:
            tick = time.perf_counter()
            if args.headless:
                armed = True
                throttle = .36 if data.time < 1 else .65 * 9.81 / (4 * 5)
                controls['pitch'] = .15 if 2 < data.time < 3 else 0
            else:
                for event in pygame.event.get():
                    if event.type == pygame.QUIT: return
                    if event.type == pygame.KEYDOWN:
                        if event.key == pygame.K_ESCAPE: return
                        if event.key == pygame.K_SPACE:
                            armed = not armed
                            if not armed: throttle = 0
                        if event.key == pygame.K_r:
                            armed = False; throttle = 0; motor[:] = 0
                            mujoco.mj_resetData(model, data); mujoco.mj_forward(model, data)
                            # Reset estimator/controller too, not just physics.
                            proc.stdin.close(); proc.wait(timeout=3)
                            proc = subprocess.Popen(['wsl.exe', '--exec', exe, str(args.rate)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
                keys = pygame.key.get_pressed()
                throttle = np.clip(throttle + (keys[pygame.K_w] - keys[pygame.K_s]) * .0006, 0, 1)
                controls = dict(roll=(keys[pygame.K_RIGHT]-keys[pygame.K_LEFT])*.5,
                                pitch=(keys[pygame.K_DOWN]-keys[pygame.K_UP])*.5,
                                yaw=(keys[pygame.K_a]-keys[pygame.K_d])*.5)
            sensor = data.sensordata
            values = [*sensor[:3], *sensor[3:6], controls['roll'], controls['pitch'], controls['yaw'], throttle, int(armed)]
            proc.stdin.write(' '.join(map(str, values)) + '\n'); proc.stdin.flush()
            reply = proc.stdout.readline()
            if not reply: raise RuntimeError('Controller bridge stopped')
            output = np.array(list(map(float, reply.split())))
            if len(output) != 6 or not np.isfinite(output).all(): raise RuntimeError('Bad controller response')
            motor += (output[:4] - motor) * (1 - math.exp(-.002/.025))
            thrust = motor * 5  # N per motor at full command; synthetic linear curve.
            rotation = data.xmat[1].reshape(3, 3)
            force = rotation @ np.array([0, 0, thrust.sum()])
            positions = np.array([[.11,.11,0],[.11,-.11,0],[-.11,.11,0],[-.11,-.11,0]])
            torque = np.cross(positions, np.column_stack((np.zeros(4), np.zeros(4), thrust))).sum(axis=0)
            torque[2] += np.dot(thrust, [1,-1,-1,1]) * .012
            torque -= sensor[:3] * .003
            data.xfrc_applied[1,:3] = force - data.qvel[:3] * .15
            data.xfrc_applied[1,3:] = rotation @ torque
            mujoco.mj_step(model, data)
            if not np.isfinite(data.qpos).all(): raise RuntimeError('Nonfinite physics')
            peak = max(peak, float(data.qpos[2]))
            steps += 1
            if renderer and steps % 8 == 0:
                camera.lookat[:] = data.qpos[:3]
                renderer.update_scene(data, camera=camera)
                image = renderer.render()
                screen.blit(pygame.image.frombuffer(image.tobytes(), (1280,720), 'RGB'), (0,0))
                pygame.draw.rect(screen, (18,25,35), (0,0,1280,100))
                lines = [f'XSFC P4 algorithm prototype | {args.rate} Hz | {"ARMED" if armed else "DISARMED"} | Throttle {throttle:.0%} | Height {data.qpos[2]:.2f} m',
                         'SPACE arm/disarm | W/S throttle (hover ~32%) | Arrows tilt | A/D yaw | R reset | ESC exit',
                         'Synthetic quad, simplified sensor timing — NOT validated flight tuning']
                for row, text in enumerate(lines):
                    screen.blit(font.render(text, True, (225,235,245)), (15,10+row*28))
                pygame.display.flip()
            if not args.headless:
                time.sleep(max(0, .002 - (time.perf_counter()-tick)))
        print(json.dumps(dict(sim_seconds=data.time, steps=steps, peak_height=peak, final_position=data.qpos[:3].tolist(), wall_seconds=time.perf_counter()-start)), flush=True)
    finally:
        proc.stdin.close()
        try: proc.wait(timeout=3)
        except subprocess.TimeoutExpired: proc.terminate()
        if renderer: renderer.close()
        pygame.quit()


if __name__ == '__main__':
    main()

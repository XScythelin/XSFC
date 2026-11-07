"""Read-only USB radio diagnostics. Never opens serial ports or arms anything."""
import argparse
import json
import os
import time

os.environ.setdefault('PYGAME_HIDE_SUPPORT_PROMPT', '1')
import pygame


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=float, default=0, help='0: watch until Ctrl+C')
    parser.add_argument('--list', action='store_true', help='List devices and exit')
    args = parser.parse_args()
    pygame.display.init()
    pygame.joystick.init()
    devices = {}

    def attach(index):
        device = pygame.joystick.Joystick(index)
        device.init()
        devices[device.get_instance_id()] = device
        print(json.dumps(dict(event='connected', name=device.get_name(), guid=device.get_guid(),
                              axes=device.get_numaxes(), buttons=device.get_numbuttons()), ensure_ascii=False), flush=True)

    try:
        for index in range(pygame.joystick.get_count()):
            attach(index)
        if not devices:
            print('No USB joystick detected. Connect Tango II and choose USB Joystick (HID), not USB Storage.', flush=True)
        if args.list:
            return
        print('Read-only axis monitor. Move ONE stick at a time; Ctrl+C to stop. No flight output.', flush=True)
        start = time.monotonic()
        last = {}
        while not args.seconds or time.monotonic() - start < args.seconds:
            for event in pygame.event.get():
                if event.type == pygame.JOYDEVICEADDED:
                    candidate = pygame.joystick.Joystick(event.device_index)
                    if candidate.get_instance_id() not in devices:
                        attach(event.device_index)
                elif event.type == pygame.JOYDEVICEREMOVED:
                    devices.pop(event.instance_id, None)
                    last.pop(event.instance_id, None)
                    print('Disconnected:', event.instance_id, flush=True)
            for instance, device in devices.items():
                state = dict(axes=[round(device.get_axis(i), 3) for i in range(device.get_numaxes())],
                             buttons=[device.get_button(i) for i in range(device.get_numbuttons())])
                if state != last.get(instance):
                    print(json.dumps(dict(instance=instance, **state)), flush=True)
                    last[instance] = state
            time.sleep(.05)
    except KeyboardInterrupt:
        pass
    finally:
        pygame.quit()


if __name__ == '__main__':
    main()

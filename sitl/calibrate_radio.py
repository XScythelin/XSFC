"""Single-window USB radio calibration. Does not start or arm a simulator."""
import json
import os
from pathlib import Path

os.environ.setdefault('PYGAME_HIDE_SUPPORT_PROMPT', '1')
import pygame
from radio import normalize, validate_profile

TARGET = Path(__file__).resolve().parent / 'profiles/tango2-usb.json'
STEPS = [
    ('roll', 'Крен: удерживай правый стик ВПРАВО'),
    ('pitch', 'Тангаж: удерживай правый стик НА СЕБЯ'),
    ('yaw', 'Рыскание: удерживай левый стик ВПРАВО'),
    ('throttle', 'Газ: удерживай левый стик ВВЕРХ (полный газ)'),
]


def main():
    pygame.init()
    screen = pygame.display.set_mode((1040, 650))
    pygame.display.set_caption('XSFC — калибровка Tango II USB — без моторов')
    font = pygame.font.SysFont('segoeui', 23)
    small = pygame.font.SysFont('segoeui', 18)
    clock = pygame.time.Clock()
    device = None
    stage = 'connect'
    message = ''
    channels = {}
    center = []
    lows, highs = [], []
    step = 0
    profile = None
    try:
        while True:
            pressed = False
            for event in pygame.event.get():
                if event.type == pygame.QUIT or event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                    return
                if event.type == pygame.KEYDOWN and event.key == pygame.K_RETURN:
                    pressed = True
                if event.type == pygame.KEYDOWN and event.key == pygame.K_r:
                    stage, channels, message, profile = 'connect', {}, '', None
                if event.type == pygame.JOYDEVICEREMOVED and device and event.instance_id == device.get_instance_id():
                    device = None
                    stage, channels, message, profile = 'connect', {}, 'USB отключён. Калибровка сброшена.', None
            if device is None:
                for index in range(pygame.joystick.get_count()):
                    candidate = pygame.joystick.Joystick(index)
                    if 'TBS' in candidate.get_name().upper() or 'TANGO' in candidate.get_name().upper():
                        candidate.init()
                        device = candidate
                        break
            axes = [device.get_axis(i) for i in range(device.get_numaxes())] if device else []
            if stage == 'connect' and device:
                stage = 'center'
            if stage == 'range':
                lows = [min(a, b) for a, b in zip(lows, axes)]
                highs = [max(a, b) for a, b in zip(highs, axes)]
            if pressed and device:
                try:
                    if stage == 'center':
                        center = axes[:]
                        lows, highs = axes[:], axes[:]
                        stage, message = 'range', ''
                    elif stage == 'range':
                        if sum(hi - lo >= 1.0 for lo, hi in zip(lows, highs)) < 4:
                            raise ValueError('Пройди полный диапазон всех четырёх стиков.')
                        stage, step, message = 'direction', 0, ''
                    elif stage == 'direction':
                        used = {cfg['axis'] for cfg in channels.values()}
                        candidates = [(abs(value - center[i]), i) for i, value in enumerate(axes)
                                      if i not in used and highs[i] - lows[i] >= 1.0]
                        amount, axis = max(candidates, default=(0, -1))
                        if amount < .55:
                            raise ValueError('Удерживай указанный стик у края; остальные верни в центр.')
                        if sum(amount2 > .4 for amount2, _ in candidates) > 1:
                            raise ValueError('Отклонено несколько осей. Двигай только указанную.')
                        name = STEPS[step][0]
                        if min(abs(axes[axis] - lows[axis]), abs(axes[axis] - highs[axis])) > .15:
                            raise ValueError('Доведи стик до крайнего положения.')
                        channels[name] = dict(axis=axis, min=lows[axis], max=highs[axis],
                                              center=center[axis], sign=1 if axes[axis] > center[axis] else -1)
                        step += 1
                        message = ''
                        if step == len(STEPS):
                            profile = validate_profile(dict(version=1, name=device.get_name(),
                                                            guid=device.get_guid(), channels=channels))
                            stage = 'verify'
                    elif stage == 'verify':
                        if normalize(profile, axes)['throttle'] > .05:
                            raise ValueError('Перед сохранением опусти газ до минимума.')
                        TARGET.parent.mkdir(exist_ok=True)
                        temporary = TARGET.with_suffix('.json.tmp')
                        temporary.write_text(json.dumps(profile, indent=2), encoding='utf-8')
                        temporary.replace(TARGET)
                        stage, message = 'saved', 'Профиль сохранён. Симулятор НЕ запущен и НЕ вооружён.'
                        print('Saved radio calibration:', TARGET, flush=True)
                except (ValueError, KeyError, IndexError) as error:
                    message = str(error)
            screen.fill((19, 25, 35))
            lines = ['XSFC / Tango II — USB-калибровка',
                     'Только чтение пульта. Реальные моторы и COM-порты не используются.',
                     device.get_name() if device else 'Подключи Tango II: USB Joystick / HID, не USB Storage.', '']
            instructions = {
                'connect': 'Ожидание пульта…',
                'center': 'Поставь ОБА стика по центру, включая газ. Нажми Enter.',
                'range': 'Пройди стиками все края и углы, затем верни в центр. Enter.',
                'direction': STEPS[min(step, 3)][1] + '. Enter, затем верни в центр.',
                'verify': 'Проверь каналы ниже. Опусти газ и нажми Enter для сохранения.',
                'saved': 'Сохранено. Esc — закрыть; R — повторить калибровку.',
            }
            lines.append(instructions[stage])
            lines.append('Esc — выход без запуска полёта. R — начать заново.')
            for row, text in enumerate(lines):
                screen.blit(font.render(text, True, (225, 234, 245)), (24, 20 + row * 35))
            if profile:
                try:
                    values = normalize(profile, axes)
                    for row, (name, value) in enumerate(values.items()):
                        screen.blit(font.render(f'{name}: {value:+.3f}  (ось {channels[name]["axis"]})', True, (120, 230, 180)), (24, 290 + row * 40))
                except (ValueError, IndexError):
                    pass
            else:
                for row, value in enumerate(axes[:8]):
                    y = 285 + row * 32
                    screen.blit(small.render(f'Ось {row}: {value:+.3f}', True, (180, 195, 210)), (24, y))
                    pygame.draw.rect(screen, (45, 57, 74), (220, y + 5, 600, 15))
                    pygame.draw.rect(screen, (85, 170, 235), (220, y + 5, int((value + 1) * 300), 15))
            screen.blit(small.render(message, True, (255, 185, 110)), (24, 575))
            screen.blit(small.render(str(TARGET), True, (150, 165, 180)), (24, 615))
            pygame.display.flip()
            clock.tick(30)
    finally:
        pygame.quit()


if __name__ == '__main__':
    main()

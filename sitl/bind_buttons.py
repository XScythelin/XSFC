"""Explicit USB control binding; no simulator or hardware commands."""
import json
import os
from pathlib import Path
import tkinter as tk
from tkinter import messagebox

os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = '1'
import pygame

PATH = Path(__file__).resolve().parent / 'profiles/tango2-usb.json'


def main():
    profile = json.loads(PATH.read_text())
    pygame.display.init()
    pygame.joystick.init()
    window = tk.Tk()
    window.title('XSFC — привязка USB, моторы не включаются')
    window.geometry('820x520')
    status = tk.StringVar(value='Подключение…')
    live = tk.StringVar()
    chosen = tk.StringVar(value='ARM: не назначен\nРежим: не назначен')
    tk.Label(window, textvariable=status, wraplength=780, font=('Segoe UI', 12)).pack(pady=12)
    tk.Label(window, textvariable=live, justify='left', font=('Consolas', 11)).pack(pady=10)
    tk.Label(window, text='Нажмите «Назначить», затем измените только нужную кнопку/переключатель.\n'
             'Другие включённые переключатели не мешают. Стики не трогайте.').pack(pady=10)
    tk.Label(window, textvariable=chosen, justify='left').pack(pady=10)
    device = None
    snapshot = None
    pending = None
    baseline = None
    bindings = {}

    def describe(binding):
        if binding['kind'] == 'button':
            return f'кнопка {binding["index"]}, активное значение {binding["active"]}'
        return f'ось {binding["index"]}, активное значение {binding["active"]:+.2f}'

    def assign(name):
        nonlocal pending, baseline
        if snapshot is None:
            status.set('Нет данных USB — проверьте подключение пульта.')
            return
        pending = name
        baseline = snapshot
        status.set(f'Ожидаю изменение для {name.upper()}. Нажмите нужную кнопку или переключите тумблер.')

    def save():
        if set(bindings) != {'arm', 'mode'}:
            messagebox.showerror('Не сохранено', 'Сначала назначьте ARM и режим.')
            return
        if (bindings['arm']['kind'], bindings['arm']['index']) == (bindings['mode']['kind'], bindings['mode']['index']):
            messagebox.showerror('Не сохранено', 'Для ARM и режима нужны разные органы управления.')
            return
        try:
            profile['buttons'] = bindings
            temporary = PATH.with_suffix('.json.tmp')
            temporary.write_text(json.dumps(profile, indent=2) + '\n')
            temporary.replace(PATH)
        except OSError as error:
            messagebox.showerror('Ошибка сохранения', str(error))
            return
        status.set('СОХРАНЕНО. Можно закрыть окно.')
        messagebox.showinfo('Сохранено', f'Привязка записана в\n{PATH}')

    tk.Button(window, text='Назначить ARM / DISARM', command=lambda: assign('arm')).pack(pady=4)
    tk.Button(window, text='Назначить ANGLE / ACRO', command=lambda: assign('mode')).pack(pady=4)
    tk.Button(window, text='Сохранить привязку', command=save).pack(pady=12)

    def poll():
        nonlocal device, snapshot, pending, baseline
        try:
            pygame.event.get()
            if device is None:
                for i in range(pygame.joystick.get_count()):
                    candidate = pygame.joystick.Joystick(i)
                    candidate.init()
                    if candidate.get_guid() == profile['guid']:
                        device = candidate
                        status.set('Tango II найден. Выберите назначение ниже.')
                        break
            if device is None:
                status.set('Tango II не найден. Подключите USB-джойстик.')
            else:
                axes = [device.get_axis(i) for i in range(device.get_numaxes())]
                buttons = [device.get_button(i) for i in range(device.get_numbuttons())]
                snapshot = (axes, buttons)
                live.set('Оси: ' + ' '.join(f'{i}:{v:+.2f}' for i, v in enumerate(axes)) + '\n'
                         + 'Кнопки ON: ' + (', '.join(str(i) for i, v in enumerate(buttons) if v) or 'нет'))
                if pending:
                    changes = [dict(kind='button', index=i, active=v) for i, v in enumerate(buttons)
                               if v != baseline[1][i]]
                    stick_axes = {c['axis'] for c in profile['channels'].values()}
                    changes += [dict(kind='axis', index=i, active=v, inactive=baseline[0][i])
                                for i, v in enumerate(axes) if i not in stick_axes and abs(v - baseline[0][i]) > .6]
                    if len(changes) == 1:
                        bindings[pending] = changes[0]
                        pending = None
                        status.set('Изменение распознано. Назначьте вторую кнопку, затем нажмите «Сохранить».')
                        chosen.set('\n'.join(f'{name}: {describe(bindings[name]) if name in bindings else "не назначен"}'
                                             for name in ('arm', 'mode')))
                    elif len(changes) > 1:
                        status.set('Изменилось несколько каналов. Верните переключатель назад и измените только один.')
        except (pygame.error, IndexError) as error:
            device = snapshot = pending = None
            status.set(f'Ошибка USB: {error}')
        window.after(20, poll)

    window.after(100, poll)
    try:
        window.mainloop()
    finally:
        pygame.quit()


if __name__ == '__main__':
    main()

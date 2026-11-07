"""Prepare an isolated Webots development world; never modify installed demos."""
import argparse
from pathlib import Path
import json

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--webots-home', type=Path, default=Path.home() / 'AppData/Local/Programs/Webots')
    parser.add_argument('--mass-kg', type=float, required=True, help='Measured or explicitly provisional flying mass')
    parser.add_argument('--wheelbase-mm', type=float, required=True, help='Measured or explicitly provisional diagonal motor spacing')
    args = parser.parse_args()
    if not .1 < args.mass_kg < 5 or not 150 < args.wheelbase_mm < 400:
        parser.error('Mass or diagonal spacing outside plausible 5-inch development range')
    source = args.webots_home / 'projects/robots/dji/mavic/worlds/mavic_2_pro.wbt'
    world = source.read_text(encoding='utf-8')
    # Preserve the installed rural environment, replacing only the demo robot.
    start = world.index('Mavic2Pro {')
    depth = 0
    end = None
    for index in range(start, len(world)):
        if world[index] == '{':
            depth += 1
        elif world[index] == '}':
            depth -= 1
            if depth == 0:
                end = index + 1
                break
    robot = '''DEF XSFC_QUAD Robot {
  translation 0 0 0.1
  name "XSFC Mark4 V2 development"
  controller "xsfc_webots"
  supervisor TRUE
  children [
    Gyro { name "gyro" }
    Accelerometer { name "accelerometer" }
    InertialUnit { name "attitude_reference" }
    GPS { name "position_reference" }
    Shape {
      appearance PBRAppearance { baseColor 0.08 0.08 0.09 roughness 0.8 metalness 0 }
      geometry Box { size 0.15 0.045 0.035 }
    }
    Camera { translation 0.08 0 0 name "fpv" width 640 height 360 }
    %s
  ]
  boundingObject Box { size 0.15 0.045 0.035 }
  physics Physics { density -1 mass %s centerOfMass [ 0 0 0 ] inertiaMatrix [ 0.003 0.003 0.005 0 0 0 ] }
}
'''
    # Geometry only. Motor force interface is deliberately not faked here.
    coordinate = args.wheelbase_mm / 1000 / (2 * 2 ** .5)
    motors = []
    for x, y in [(coordinate, coordinate), (coordinate, -coordinate), (-coordinate, coordinate), (-coordinate, -coordinate)]:
        motors.append('''Pose { translation %f %f 0 children [
          Shape { appearance PBRAppearance { baseColor 0.15 0.15 0.17 } geometry Cylinder { radius 0.014 height 0.025 } }
          Pose { translation 0 0 0.016 children [ Shape {
            appearance PBRAppearance { baseColor 0.8 0.2 0.1 transparency 0.65 }
            geometry Cylinder { radius 0.06477 height 0.002 }
          } ] }
        ] }''' % (x, y))
    world = world[:start] + robot % ('\n'.join(motors), args.mass_kg) + world[end:]
    world = world.replace('follow "Mavic 2 PRO"', 'follow "XSFC Mark4 V2 development"')
    world = world.replace('basicTimeStep 8', 'basicTimeStep 1')
    world = world.replace('linear 0.5', 'linear 0.02').replace('angular 0.5', 'angular 0.001')
    world = world.replace('title "DJI Mavic 2 PRO demo"', 'title "XSFC Controller/Mixer — provisional Mark4 model"')
    worlds = HERE / 'worlds'
    worlds.mkdir(exist_ok=True)
    target = worlds / 'mark4_development.wbt'
    target.write_text(world, encoding='utf-8')
    (worlds / 'development-assumptions.json').write_text(json.dumps(dict(
        mass_kg=args.mass_kg, wheelbase_mm=args.wheelbase_mm,
        warning='Explicit development assumptions, not measured user hardware. No motor/XSFC adapter attached.'), indent=2), encoding='utf-8')
    print(target)


if __name__ == '__main__':
    main()

"""PyCraft - a small Minecraft-style survival game.

Run:  python main.py            (random world)
      python main.py --seed 42  (same world every time)
"""
import argparse
import sys


def main():
    parser = argparse.ArgumentParser(description='PyCraft, a Minecraft-style voxel game')
    parser.add_argument('--seed', type=int, default=None, help='world seed (random if omitted)')
    parser.add_argument('--render-distance', type=int, default=6, help='view distance in chunks (default 6)')
    parser.add_argument('--fullscreen', action='store_true')
    parser.add_argument('--autotest', metavar='DIR', help=argparse.SUPPRESS)  # scripted smoke test
    args = parser.parse_args()

    import pyglet
    from pycraft.game import Game

    autotest = None
    if args.autotest:
        from pycraft.autotest import AutoTest
        autotest = AutoTest(args.autotest)
    size = {} if args.fullscreen else {'width': 1280, 'height': 720}
    Game(seed=args.seed, render_distance=max(2, min(12, args.render_distance)), autotest=autotest,
         fullscreen=args.fullscreen, **size)
    pyglet.app.run(1 / 60)


if __name__ == '__main__':
    sys.exit(main())

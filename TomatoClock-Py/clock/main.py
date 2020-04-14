# coding:utf-8

import argparse
import asyncio

import gbulb
from gi.repository import Notify

from .clock import Clock


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-s', '--silent', action='store',
                        help="don't print time logs")
    args = parser.parse_args()

    Notify.init('你的Personal Timer')
    gbulb.install()
    loop = asyncio.get_event_loop()
    clock = Clock(silent=args.silent)
    try:
        loop.run_until_complete(clock.run())
    except KeyboardInterrupt:
        pass
    finally:
        clock.close()


if __name__=="__main__":
    main()
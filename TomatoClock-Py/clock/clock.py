import asyncio
from datetime import datetime

import attr
from gi.repository import Notify

from .config import DELAYS, ICON


@attr.s
class Clock:
    """Notify API: https://lazka.github.io/pgi-docs/Notify-0.7/functions.html"""
    notification = attr.ib(default=None)
    state = attr.ib(default='work')
    silent = attr.ib(default=False)
    count = attr.ib(default=0)

    @asyncio.coroutine
    def run(self):
        notifications = {
            'work': self.break_notice,
            'long_work': self.long_break_notice,
            'break': self.work_notice,
            'long_break': self.work_notice,
            'not_break': self.break_notice,
            'not_work': self.work_notice}

        while True:
            if self.state == 'cancel':
                break
            if self.state == 'break' and self.count >= 3:
                self.state = 'long_work'
                self.count = 0
            if self.state == 'long_break':
                self.count = 0
            future, callback = self.future_callback()
            # 向callback传入不同notice function的参数
            self.notification = notifications[self.state](callback)
            # 窗口关闭时触发callback function。None为此处传递给callback的参数
            # default_action再没有点击notice窗口中的选项，而窗口关闭时的默认action
            self.notification.connect('closed', callback, self.notification.default_action)

            # 等待
            delay = DELAYS[self.state]
            yield from asyncio.sleep(delay)

            # 显示通知
            self.notification.show()
            self.log()  # 终端显示log

            # future中获取action，处理逻辑在future_callback函数
            action = yield from future
            if action.action != 'not_break' and action != 'not_work':
                self.count += 1
                action.counter = self.count

            self.state = action.action

    @staticmethod
    def break_notice(callback):
        """返回一个Notification对象"""
        notification = Notify.Notification.new('路漫漫', '休息一下，喝杯水', ICON)
        notification.set_timeout(Notify.EXPIRES_NEVER)
        notification.default_action = 'break'
        notification.add_action('break', '休息5分钟', callback)  # 中止当前异步事件，并传入事件输出结果
        notification.add_action('not_break', '5分钟后再休息', callback)
        return notification

    @staticmethod
    def long_break_notice(callback):
        notification = Notify.Notification.new('一蓑烟雨', '平生，一路，歇脚处自有天地', ICON)
        notification.set_timeout(Notify.EXPIRES_NEVER)
        notification.default_action = 'long_break'
        notification.add_action('long_break', '休息15分钟吧', callback)
        notification.add_action('not_break', '5分钟后再休息', callback)
        return notification

    @staticmethod
    def work_notice(callback):
        notification = Notify.Notification.new('总有事情值得去做', '启程了', ICON)
        notification.set_timeout(Notify.EXPIRES_NEVER)
        notification.default_action = 'work'
        notification.add_action('work',  '开始工作', callback)
        notification.add_action('not_break', '5分钟后再工作', callback)
        return notification

    @staticmethod
    def future_callback():
        """返回异步事件future，同时callback定义了事件的中止返回结果"""
        future = asyncio.Future()
        def callback(notification, action, counter=0):
            # 在一系列的notice function中，通过add_function获取
            action = Action(notification, action, counter)
            if not future.done():
                future.set_result(action)
        return future, callback

    def log(self):
        if self.silent: return
        print('{:%Y-%m-%d %H:%M} - {}'.format(datetime.now(), self.notification.props.body))

    def close(self):
        if self.notification is not None:
            self.notification.close()


@attr.s
class Action:
    notification = attr.ib(type=str)
    action = attr.ib(type=str)
    counter = attr.ib(default=0, type=int)
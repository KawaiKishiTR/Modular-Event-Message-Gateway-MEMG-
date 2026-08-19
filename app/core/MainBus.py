import functools
from typing import Callable
from collections import defaultdict

from .AsyncTaskManager import AsyncTaskManager
from .Mixins import SingletonMixin
from .MainBusEnums import MainBusEnums


class MainBus(SingletonMixin):
    """Event bus class for handling events"""
    Events = MainBusEnums

    def __init__(self) -> None:
        super().__init__()
        if self._is_initialized:
            return
        self.subscribers:dict[str, list[Callable]] = defaultdict(list)

    def subscribe(self, event_type:MainBusEnums | str, callback:Callable):
        key = self.normalize_event_type(event_type)
        if callback not in self.subscribers[key]:
            self.subscribers[key].append(callback)

    def unsubscribe(self, event_type:MainBusEnums | str, callback:Callable):
        key = self.normalize_event_type(event_type)
        if callback in self.subscribers[key]:
            self.subscribers[key].remove(callback)

    async def emit(self, event_type:MainBusEnums | str, *args, **kwargs):
        key = self.normalize_event_type(event_type)

        TaskManager = AsyncTaskManager()
        self._log(key, *args, **kwargs)
        for callback in self.subscribers.get(key, []):
            if args or kwargs:
                wrapped_cb = functools.partial(callback, *args, **kwargs)
            else:
                wrapped_cb = callback
            
            TaskManager.add(wrapped_cb, name=f"[MAIN BUS] callback of event {key}")

    @staticmethod
    def normalize_event_type(event_type:MainBusEnums | str) -> str:
        return event_type.name if isinstance(event_type, MainBusEnums) else str(event_type)

    @staticmethod
    def _log(event_type:str, *args, **kwargs):
        msg = f"[MainBus] Sending signal: {event_type}"
        with_ = False
        if args:
            msg += f" with args: {args}"
            with_ = True
        if kwargs:
            if with_:
                msg += ","
            else:
                msg += " with"
            msg += f" kwargs: {kwargs}"
            with_ = True
        if not with_:
            msg += " with no arguments"
        print(msg)

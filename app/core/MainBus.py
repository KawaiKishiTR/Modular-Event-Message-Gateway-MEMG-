import asyncio
from typing import Callable
from collections import defaultdict

from app.core.Mixins import SingletonMixin



class MainBus(SingletonMixin):
    """Event bus class for handling events"""

    def __init__(self) -> None:
        super().__init__()
        if self._is_initialized:
            return
        self.subscribers:dict[str, list[Callable]] = defaultdict(list)

    def subscribe(self, event_type:str, callback:Callable):
        if callback not in self.subscribers[event_type]:
            self.subscribers[event_type].append(callback)

    def unsubscribe(self, event_type:str, callback:Callable):
        if callback in self.subscribers[event_type]:
            self.subscribers[event_type].remove(callback)

    def emit(self, event_type:str, *args, **kwargs):
        self._log(event_type, *args, **kwargs)
        for callback in self.subscribers.get(event_type, []):
            callback(*args, **kwargs)

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

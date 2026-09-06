from .Mixins import SingletonMixin
from typing import TypeVar


T = TypeVar("T")

class Container(SingletonMixin):
    def __init__(self):
        if self._is_initialized:
            return

        self._container:dict[T:type[T]] = {}

    def set(self, key:type[T], value:T) -> None:
        self._container[key] = value

    def get(self, key:type[T]) -> T:
        return self._container[key]


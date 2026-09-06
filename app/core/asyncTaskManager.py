import inspect
import typing
import asyncio
import functools
from typing import Any, Callable

from .Mixins import SingletonMixin


class AsyncTaskManager(SingletonMixin):
    def __init__(self):
        if self._is_initialized:
            return

        self._taskpool = set()

    def _discard(self, task:asyncio.Task):
        self._taskpool.discard(task)

        try:
            r = task.result()
        except asyncio.CancelledError as e:
            print(f"[ASYNC RUNNER][TASK CANCELLED] task: {task.get_name()} <> {e}")
        except asyncio.InvalidStateError as e:
            print(f"[ASYNC RUNNER][TASK ISNT DONE] task: {task.get_name()} <> {e}")
        except Exception as e:
            print(f"[ASYNC RUNNER][TASK ERROR] on task: {task.get_name()} <> {e}")

    @staticmethod
    def _is_async_callable(target: Any) -> bool:
        """partial veya custom callable olsa dahi async olup olmadığını kontrol eder."""
        while isinstance(target, functools.partial):
            target = target.func
        return inspect.iscoroutinefunction(target) or inspect.iscoroutinefunction(getattr(target, "__call__", None))

    def add(self, target: Any,
            done_callback:list[Callable] | None = None,
            name:str | None = None
            ) -> asyncio.Task:

        if isinstance(target, asyncio.Task):
            return self.add_task(target, done_callback=done_callback, name=name)
        
        if inspect.iscoroutine(target):
            return self.add_coroutine(target, done_callback=done_callback, name=name)
        
        if inspect.iscoroutinefunction(target) or self._is_async_callable(target):
            return self.add_coroutine(target(), done_callback=done_callback, name=name)
        
        if callable(target):
            return self.add_regular_function(target, done_callback=done_callback, name=name)
        
        raise TypeError(f"Unsupported target type for AsyncTaskManager: {type(target)}")

    def add_regular_function(self, func:Callable,
                            done_callback:list[Callable] | None = None,
                            name:str | None = None
                            ) -> asyncio.Task:
        return self.add_coroutine(asyncio.to_thread(func), done_callback=done_callback, name=name)

    def add_coroutine(self, coroutine:typing.Coroutine,
                    done_callback:list[Callable] | None = None,
                    name:str | None = None
                    ) -> asyncio.Task:
        return self.add_task(asyncio.create_task(coroutine, name=name), done_callback=done_callback)

    def add_task(self, task:asyncio.Task,
                done_callback:list[Callable] | None = None,
                name:str | None = None
                ) -> asyncio.Task:
        self._taskpool.add(task)
        if name is not None: task.set_name(name)

        task.add_done_callback(self._discard)
        if done_callback is None: return task
        for callback in done_callback:
            task.add_done_callback(callback)

        return task


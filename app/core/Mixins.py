import types
from typing import Any, Callable, Union, get_args, get_origin
from dataclasses import fields
import threading
import json

class SingletonMixin:
    """Tüm alt sınıfları Singleton yapan Mixin sınıfı.

    Thread-safe (İş parçacığı güvenli) yapıdadır.
    """
    _instances = {}
    _lock = threading.Lock()

    def __new__(cls, *args, **kwargs):
        if cls not in cls._instances:
            with cls._lock:
                # Çift kontrollü kilitleme (Double-Checked Locking) deseni
                if cls not in cls._instances:
                    instance = super().__new__(cls)
                    cls._instances[cls] = instance
        ins = cls._instances[cls]
        if hasattr(ins, "_is_initialized"):
            ins._is_initialized = True
        else:
            ins._is_initialized = False
        return ins

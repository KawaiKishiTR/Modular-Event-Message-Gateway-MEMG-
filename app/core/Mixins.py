import threading



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
        return cls._instances[cls]

    def __init__(self) -> None:
        if hasattr(self, "_is_initialized"):
            self._is_initialized = True
            return
        self._is_initialized = False

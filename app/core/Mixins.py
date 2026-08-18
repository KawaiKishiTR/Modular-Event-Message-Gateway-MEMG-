



class SingletonMixin:
    """Tüm alt sınıfları Singleton yapan Mixin sınıfı.

    Thread-safe (İş parçacığı güvenli) yapıdadır.
    """
    __instance = None

    def __new__(cls, *args, **kwargs):
        if cls.__instance is None:
            cls.__instance = super().__new__(cls)
        return cls.__instance

    def __init__(self) -> None:
        if getattr(self, "_is_initialized", False):
            return
        self._is_initialized = True
